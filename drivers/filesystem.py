"""
SwanOS — Filesystem Driver
Provides sandboxed file I/O operations within the workspace directory.
"""

import os

from config import WORKSPACE_DIR


class FilesystemDriver:
    """Peripheral driver for local storage access, sandboxed to workspace/."""

    def _safe_path(self, relative_path: str) -> str:
        """Resolve a user-supplied path into a safe absolute path inside workspace."""
        resolved = os.path.normpath(os.path.join(WORKSPACE_DIR, relative_path))
        if not resolved.startswith(WORKSPACE_DIR):
            raise PermissionError(f"Access denied — path escapes workspace: {relative_path}")
        return resolved

    # ── Tool Definitions ────────────────────────────────────

    def get_tool_definitions(self):
        """Returns OpenAI-style tool schemas for the LLM."""
        return [
            {
                "type": "function",
                "function": {
                    "name": "list_directory",
                    "description": "Lists all files and folders at a path inside the workspace.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "path": {
                                "type": "string",
                                "description": "Relative path inside workspace. Defaults to root.",
                                "default": "."
                            }
                        }
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "read_file",
                    "description": "Reads and returns the content of a file in the workspace.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "file_path": {
                                "type": "string",
                                "description": "Relative path to the file inside workspace."
                            }
                        },
                        "required": ["file_path"]
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "write_file",
                    "description": "Writes content to a file in the workspace. Creates it if it doesn't exist.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "file_path": {
                                "type": "string",
                                "description": "Relative path to the file inside workspace."
                            },
                            "content": {
                                "type": "string",
                                "description": "The text content to write."
                            }
                        },
                        "required": ["file_path", "content"]
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "delete_file",
                    "description": "Deletes a file from the workspace.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "file_path": {
                                "type": "string",
                                "description": "Relative path to the file inside workspace."
                            }
                        },
                        "required": ["file_path"]
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "create_directory",
                    "description": "Creates a new directory inside the workspace.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "path": {
                                "type": "string",
                                "description": "Relative path for the new directory."
                            }
                        },
                        "required": ["path"]
                    }
                }
            }
        ]

    # ── Execution ───────────────────────────────────────────

    def execute(self, tool_name: str, args: dict) -> str:
        """Route a tool call to the correct handler."""
        try:
            if tool_name == "list_directory":
                path = self._safe_path(args.get("path", "."))
                entries = os.listdir(path)
                if not entries:
                    return "Directory is empty."
                return "\n".join(entries)

            elif tool_name == "read_file":
                path = self._safe_path(args["file_path"])
                with open(path, "r", encoding="utf-8") as f:
                    return f.read()

            elif tool_name == "write_file":
                path = self._safe_path(args["file_path"])
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(args["content"])
                return f"✓ Written to {args['file_path']}"

            elif tool_name == "delete_file":
                path = self._safe_path(args["file_path"])
                os.remove(path)
                return f"✓ Deleted {args['file_path']}"

            elif tool_name == "create_directory":
                path = self._safe_path(args["path"])
                os.makedirs(path, exist_ok=True)
                return f"✓ Created directory {args['path']}"

        except FileNotFoundError:
            return f"✗ File not found: {args.get('file_path', args.get('path', ''))}"
        except PermissionError as e:
            return f"✗ Permission denied: {e}"
        except Exception as e:
            return f"✗ Filesystem error: {e}"

        return f"✗ Unknown filesystem tool: {tool_name}"