"""
SwanOS — LLM Kernel
The cognitive core: sends user intents to Gemini via REST API,
handles tool-call loops, and returns final answers.
Uses raw HTTP requests for maximum Python version compatibility.
"""

import json
import time as _time
import requests

from config import GEMINI_API_KEY, SYSTEM_PROMPT, MAX_TOOL_ROUNDS, MODEL_NAME
from kernel.scheduler import Scheduler
from drivers.filesystem import FilesystemDriver
from drivers.code_executor import CodeExecutorDriver
from drivers.web_search import WebSearchDriver

API_BASE = "https://generativelanguage.googleapis.com/v1beta/models"
MAX_RETRIES = 5          # How many times to retry on 429
INITIAL_BACKOFF = 5      # First wait (seconds); doubles each retry


def _build_gemini_tools(tool_defs: list) -> list:
    """Convert OpenAI-style tool schemas to Gemini REST API format."""
    declarations = []
    for tool in tool_defs:
        func = tool["function"]
        params = func.get("parameters", {})
        decl = {
            "name": func["name"],
            "description": func["description"],
            "parameters": {
                "type": "OBJECT",
                "properties": {
                    k: {
                        "type": "STRING",
                        "description": v.get("description", ""),
                    }
                    for k, v in params.get("properties", {}).items()
                },
                "required": params.get("required", []),
            },
        }
        declarations.append(decl)
    return [{"functionDeclarations": declarations}]


class LLMKernel:
    """The brain of SwanOS — calls Gemini REST API with a tool-call loop."""

    def __init__(self):
        if not GEMINI_API_KEY:
            raise RuntimeError("GEMINI_API_KEY not set in .env")

        self.api_key = GEMINI_API_KEY
        self.model = MODEL_NAME

        # Build the scheduler and register every driver
        self.scheduler = Scheduler()
        self.scheduler.register_driver(FilesystemDriver())
        self.scheduler.register_driver(CodeExecutorDriver())
        self.scheduler.register_driver(WebSearchDriver())

        # Prepare Gemini-format tool definitions
        openai_tools = self.scheduler.get_all_tool_definitions()
        self.gemini_tools = _build_gemini_tools(openai_tools)

    def _call_gemini(self, contents: list) -> dict:
        """Make a single call to Gemini with automatic retry on 429 rate-limit."""
        url = f"{API_BASE}/{self.model}:generateContent?key={self.api_key}"

        payload = {
            "contents": contents,
            "tools": self.gemini_tools,
            "systemInstruction": {
                "parts": [{"text": SYSTEM_PROMPT}]
            },
        }

        backoff = INITIAL_BACKOFF
        for attempt in range(1, MAX_RETRIES + 1):
            resp = requests.post(url, json=payload, timeout=60)

            if resp.status_code == 429:
                if attempt == MAX_RETRIES:
                    resp.raise_for_status()          # give up after final attempt
                print(f"  ⏳ Rate-limited (429). Retrying in {backoff}s… (attempt {attempt}/{MAX_RETRIES})")
                _time.sleep(backoff)
                backoff *= 2                          # exponential back-off
                continue

            resp.raise_for_status()
            return resp.json()

        # Should never reach here, but just in case
        resp.raise_for_status()
        return resp.json()

    def run(self, user_intent: str) -> str:
        """
        Cognitive loop:
        1. Send intent + tools to Gemini.
        2. If Gemini asks for tool calls → dispatch via Scheduler → feed results back.
        3. Repeat until Gemini returns a plain text answer (or max rounds hit).
        """
        contents = [
            {"role": "user", "parts": [{"text": user_intent}]}
        ]

        for _ in range(MAX_TOOL_ROUNDS):
            data = self._call_gemini(contents)

            # Parse the response
            candidates = data.get("candidates", [])
            if not candidates:
                return "[No response from kernel]"

            parts = candidates[0].get("content", {}).get("parts", [])

            # Check for function calls
            function_calls = [p for p in parts if "functionCall" in p]

            if not function_calls:
                # Extract text response
                text_parts = [p.get("text", "") for p in parts if "text" in p]
                return "\n".join(text_parts) or "[No response from kernel]"

            # Append the model's response to history
            contents.append({
                "role": "model",
                "parts": parts,
            })

            # Process each function call and build responses
            fn_response_parts = []
            for fc_part in function_calls:
                fc = fc_part["functionCall"]
                tool_name = fc["name"]
                tool_args = fc.get("args", {})

                print(f"  ⚙  [{tool_name}] {tool_args}")
                result = self.scheduler.dispatch(tool_name, tool_args)
                print(f"  ↳  {result}")

                fn_response_parts.append({
                    "functionResponse": {
                        "name": tool_name,
                        "response": {"result": str(result)},
                    }
                })

            # Feed tool results back to Gemini
            contents.append({
                "role": "user",
                "parts": fn_response_parts,
            })

        return "⚠ Reached maximum tool rounds — stopping."