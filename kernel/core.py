"""
SwanOS — LLM Kernel
The cognitive core: sends user intents to the LLM,
handles tool-call loops, and returns final answers.
Supports Groq (OpenAI-compatible) and Gemini providers.
"""

import time
import requests

from config import (
    API_KEYS, API_BASE_URL, SYSTEM_PROMPT,
    MAX_TOOL_ROUNDS, MODEL_NAME, LLM_PROVIDER,
)
from kernel.scheduler import Scheduler
from drivers.filesystem import FilesystemDriver
from drivers.code_executor import CodeExecutorDriver
from drivers.web_search import WebSearchDriver

MAX_RETRIES = 5
INITIAL_BACKOFF = 2
MAX_HISTORY = 20
MAX_RESULT_LEN = 4000


# ══════════════════════════════════════════════════════════
#  Tool schema builders — one per provider format
# ══════════════════════════════════════════════════════════

def _build_openai_tools(tool_defs: list) -> list:
    """Build OpenAI/Groq format tool schemas (already in this format)."""
    return tool_defs


def _build_gemini_tools(tool_defs: list) -> list:
    """Convert OpenAI-style tool schemas to Gemini REST API format."""
    declarations = []
    for tool in tool_defs:
        func = tool["function"]
        params = func.get("parameters", {})
        declarations.append({
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
        })
    return [{"functionDeclarations": declarations}]


def _truncate(text: str, max_len: int = MAX_RESULT_LEN) -> str:
    """Truncate long text to save API tokens."""
    if len(text) <= max_len:
        return text
    half = max_len // 2
    return text[:half] + "\n…[truncated]…\n" + text[-half:]


# ══════════════════════════════════════════════════════════
#  LLM Kernel
# ══════════════════════════════════════════════════════════

class LLMKernel:
    """The brain of SwanOS — supports Groq and Gemini providers."""

    def __init__(self):
        if not API_KEYS:
            raise RuntimeError(
                "No API key set! Add GROQ_API_KEY or GEMINI_API_KEY to .env"
            )

        self.api_keys = API_KEYS
        self._key_index = 0
        self.model = MODEL_NAME
        self.provider = LLM_PROVIDER

        # Persistent HTTP session
        self.session = requests.Session()
        self.session.headers.update({"Content-Type": "application/json"})

        # Build scheduler and register drivers
        self.scheduler = Scheduler()
        self.scheduler.register_driver(FilesystemDriver())
        self.scheduler.register_driver(CodeExecutorDriver())
        self.scheduler.register_driver(WebSearchDriver())

        # Build provider-specific tool schemas
        openai_tools = self.scheduler.get_all_tool_definitions()
        if self.provider == "groq":
            self._tools = _build_openai_tools(openai_tools)
        else:
            self._tools = _build_gemini_tools(openai_tools)

        # Conversation memory
        self.history = []

    def _next_key(self) -> str:
        """Get the next API key via round-robin."""
        key = self.api_keys[self._key_index % len(self.api_keys)]
        self._key_index += 1
        return key

    # ── Groq / OpenAI-compatible API ───────────────────────

    def _call_groq(self, messages: list) -> dict:
        """Call the Groq API (OpenAI-compatible format)."""
        backoff = INITIAL_BACKOFF
        total_keys = len(self.api_keys)

        for attempt in range(1, MAX_RETRIES + 1):
            api_key = self._next_key()

            payload = {
                "model": self.model,
                "messages": messages,
                "tools": self._tools,
                "tool_choice": "auto",
                "max_tokens": 2048,
                "temperature": 0.7,
            }

            headers = {
                "Content-Type": "application/json",
                "Authorization": "Bearer {}".format(api_key),
            }

            resp = self.session.post(
                API_BASE_URL, json=payload, headers=headers, timeout=60
            )

            if resp.status_code == 429:
                if attempt == MAX_RETRIES:
                    resp.raise_for_status()
                if total_keys > 1:
                    print("  🔄 Key rate-limited. Rotating…")
                    time.sleep(0.5)
                else:
                    print("  ⏳ Rate-limited. Retrying in {}s…".format(backoff))
                    time.sleep(backoff)
                    backoff *= 2
                continue

            if resp.status_code >= 400:
                try:
                    err_msg = resp.json().get("error", {}).get("message", resp.text[:300])
                except Exception:
                    err_msg = resp.text[:300]
                raise RuntimeError("Groq API error {}: {}".format(resp.status_code, err_msg))

            return resp.json()

        raise RuntimeError("Max retries exhausted")

    def _run_groq(self, user_intent: str) -> str:
        """Groq/OpenAI cognitive loop with tool calling."""
        self.history.append({"role": "user", "content": user_intent})

        messages = [{"role": "system", "content": SYSTEM_PROMPT}] + list(self.history)

        for _ in range(MAX_TOOL_ROUNDS):
            data = self._call_groq(messages)

            choice = data.get("choices", [{}])[0]
            message = choice.get("message", {})

            tool_calls = message.get("tool_calls")

            if not tool_calls:
                # Final text answer
                answer = message.get("content", "") or "[No response from kernel]"
                self.history.append({"role": "assistant", "content": answer})
                self._trim_history()
                return answer

            # Append assistant message with tool calls to history
            messages.append(message)

            # Dispatch each tool call
            for tc in tool_calls:
                func = tc.get("function", {})
                tool_name = func.get("name", "")
                import json
                try:
                    tool_args = json.loads(func.get("arguments", "{}"))
                except json.JSONDecodeError:
                    tool_args = {}

                print("  ⚙  [{}] {}".format(tool_name, tool_args))
                result = _truncate(str(self.scheduler.dispatch(tool_name, tool_args)))
                print("  ↳  {}".format(result))

                # Append tool result
                messages.append({
                    "role": "tool",
                    "tool_call_id": tc.get("id", ""),
                    "name": tool_name,
                    "content": result,
                })

        return "⚠ Reached maximum tool rounds — stopping."

    # ── Gemini API ─────────────────────────────────────────

    def _call_gemini(self, contents: list) -> dict:
        """Call the Gemini REST API."""
        backoff = INITIAL_BACKOFF
        total_keys = len(self.api_keys)

        for attempt in range(1, MAX_RETRIES + 1):
            api_key = self._next_key()
            url = "{}/{}:generateContent?key={}".format(
                API_BASE_URL, self.model, api_key
            )

            payload = {
                "contents": contents,
                "tools": self._tools,
                "systemInstruction": {"parts": [{"text": SYSTEM_PROMPT}]},
                "generationConfig": {
                    "temperature": 0.7,
                    "maxOutputTokens": 2048,
                },
            }

            resp = self.session.post(url, json=payload, timeout=60)

            if resp.status_code == 429:
                if attempt == MAX_RETRIES:
                    resp.raise_for_status()
                if total_keys > 1:
                    print("  🔄 Key rate-limited. Rotating…")
                    time.sleep(0.5)
                else:
                    print("  ⏳ Rate-limited. Retrying in {}s…".format(backoff))
                    time.sleep(backoff)
                    backoff *= 2
                continue

            resp.raise_for_status()
            return resp.json()

        resp.raise_for_status()
        return resp.json()

    def _run_gemini(self, user_intent: str) -> str:
        """Gemini cognitive loop with tool calling."""
        self.history.append(
            {"role": "user", "parts": [{"text": user_intent}]}
        )
        contents = list(self.history)

        for _ in range(MAX_TOOL_ROUNDS):
            data = self._call_gemini(contents)
            candidates = data.get("candidates", [])
            if not candidates:
                return "[No response from kernel]"

            parts = candidates[0].get("content", {}).get("parts", [])
            function_calls = [p for p in parts if "functionCall" in p]

            if not function_calls:
                text_parts = [p.get("text", "") for p in parts if "text" in p]
                answer = "\n".join(text_parts) or "[No response from kernel]"
                self.history.append({"role": "model", "parts": parts})
                self._trim_history()
                return answer

            contents.append({"role": "model", "parts": parts})

            fn_response_parts = []
            for fc_part in function_calls:
                fc = fc_part["functionCall"]
                tool_name = fc["name"]
                tool_args = fc.get("args", {})

                print("  ⚙  [{}] {}".format(tool_name, tool_args))
                result = _truncate(str(self.scheduler.dispatch(tool_name, tool_args)))
                print("  ↳  {}".format(result))

                fn_response_parts.append({
                    "functionResponse": {
                        "name": tool_name,
                        "response": {"result": result},
                    }
                })

            contents.append({"role": "user", "parts": fn_response_parts})

        return "⚠ Reached maximum tool rounds — stopping."

    # ── Public interface ───────────────────────────────────

    def run(self, user_intent: str) -> str:
        """Route to the correct provider's cognitive loop."""
        if self.provider == "groq":
            return self._run_groq(user_intent)
        else:
            return self._run_gemini(user_intent)

    def _trim_history(self):
        """Keep conversation history within bounds."""
        if len(self.history) > MAX_HISTORY:
            self.history = self.history[-MAX_HISTORY:]

    @property
    def key_count(self) -> int:
        return len(self.api_keys)

    def clear_history(self):
        """Clear conversation memory."""
        self.history.clear()