"""
SwanOS — Code Executor Driver
Runs Python code in a subprocess sandbox with timeout protection.
"""

import subprocess
import sys

from config import CODE_EXEC_TIMEOUT


class CodeExecutorDriver:
    """Executes Python code strings in an isolated subprocess."""

    def get_tool_definitions(self):
        """Returns OpenAI-style tool schemas for the LLM."""
        return [
            {
                "type": "function",
                "function": {
                    "name": "execute_python",
                    "description": "Executes a Python code snippet and returns its stdout/stderr output.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "code": {
                                "type": "string",
                                "description": "The Python source code to execute."
                            }
                        },
                        "required": ["code"]
                    }
                }
            }
        ]

    def execute(self, tool_name: str, args: dict) -> str:
        """Run the requested tool."""
        if tool_name == "execute_python":
            return self._run_code(args.get("code", ""))
        return f"✗ Unknown code executor tool: {tool_name}"

    def _run_code(self, code: str) -> str:
        """Execute Python code in a subprocess with timeout."""
        if not code.strip():
            return "✗ No code provided."

        try:
            result = subprocess.run(
                [sys.executable, "-c", code],
                capture_output=True,
                text=True,
                timeout=CODE_EXEC_TIMEOUT,
            )

            output_parts = []
            if result.stdout.strip():
                output_parts.append(f"[stdout]\n{result.stdout.strip()}")
            if result.stderr.strip():
                output_parts.append(f"[stderr]\n{result.stderr.strip()}")

            if not output_parts:
                return "✓ Code executed successfully (no output)."

            status = "✓" if result.returncode == 0 else f"✗ Exit code {result.returncode}"
            return f"{status}\n" + "\n".join(output_parts)

        except subprocess.TimeoutExpired:
            return f"✗ Execution timed out after {CODE_EXEC_TIMEOUT}s."
        except Exception as e:
            return f"✗ Execution error: {e}"
