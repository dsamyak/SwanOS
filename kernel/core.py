"""
SwanOS — LLM Kernel
The cognitive core: sends user intents to Gemini via the official SDK,
handles tool-call loops, and returns final answers.
"""

from google import genai
from google.genai import types

from config import GEMINI_API_KEY, SYSTEM_PROMPT, MAX_TOOL_ROUNDS, MODEL_NAME
from kernel.scheduler import Scheduler
from drivers.filesystem import FilesystemDriver
from drivers.code_executor import CodeExecutorDriver
from drivers.web_search import WebSearchDriver


def _build_tool_declarations(tool_defs: list) -> list:
    """Convert our OpenAI-style tool schemas into Gemini SDK FunctionDeclarations."""
    declarations = []
    for tool in tool_defs:
        func = tool["function"]
        params = func.get("parameters", {})
        declarations.append(types.FunctionDeclaration(
            name=func["name"],
            description=func["description"],
            parameters={
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
        ))
    return declarations


class LLMKernel:
    """The brain of SwanOS — calls Gemini SDK with a tool-call loop."""

    def __init__(self):
        if not GEMINI_API_KEY:
            raise RuntimeError("GEMINI_API_KEY not set in .env")

        # Initialize the Gemini client
        self.client = genai.Client(api_key=GEMINI_API_KEY)
        self.model = MODEL_NAME

        # Build the scheduler and register every driver
        self.scheduler = Scheduler()
        self.scheduler.register_driver(FilesystemDriver())
        self.scheduler.register_driver(CodeExecutorDriver())
        self.scheduler.register_driver(WebSearchDriver())

        # Prepare Gemini tool declarations
        openai_tools = self.scheduler.get_all_tool_definitions()
        self.tool_declarations = _build_tool_declarations(openai_tools)

        # Build reusable generation config
        self.gen_config = types.GenerateContentConfig(
            system_instruction=SYSTEM_PROMPT,
            tools=[types.Tool(function_declarations=self.tool_declarations)],
        )

    def run(self, user_intent: str) -> str:
        """
        Cognitive loop:
        1. Send intent + tools to Gemini.
        2. If Gemini asks for tool calls → dispatch via Scheduler → feed results back.
        3. Repeat until Gemini returns a plain text answer (or max rounds hit).
        """
        contents = [
            types.Content(
                role="user",
                parts=[types.Part.from_text(text=user_intent)],
            )
        ]

        for _ in range(MAX_TOOL_ROUNDS):
            response = self.client.models.generate_content(
                model=self.model,
                contents=contents,
                config=self.gen_config,
            )

            # Check for function calls in the response
            function_calls = [
                part for part in response.candidates[0].content.parts
                if part.function_call is not None
            ]

            if not function_calls:
                # No tool calls — return the text answer
                text_parts = [
                    part.text for part in response.candidates[0].content.parts
                    if part.text is not None
                ]
                return "\n".join(text_parts) or "[No response from kernel]"

            # Append the model's response to history
            contents.append(response.candidates[0].content)

            # Process each function call and build responses
            fn_response_parts = []
            for part in function_calls:
                fc = part.function_call
                tool_name = fc.name
                tool_args = dict(fc.args) if fc.args else {}

                print(f"  ⚙  [{tool_name}] {tool_args}")
                result = self.scheduler.dispatch(tool_name, tool_args)
                print(f"  ↳  {result}")

                fn_response_parts.append(
                    types.Part.from_function_response(
                        name=tool_name,
                        response={"result": str(result)},
                    )
                )

            # Feed tool results back to Gemini
            contents.append(
                types.Content(role="user", parts=fn_response_parts)
            )

        return "⚠ Reached maximum tool rounds — stopping."