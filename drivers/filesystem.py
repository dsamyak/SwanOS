"""
SwanOS — Filesystem Driver
Provides sandboxed file I/O operations within the workspace directory.
"""

import os
import shutil

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
                                "description": "Relative path inside workspace. Use '.' for root."
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
                    "name": "append_file",
                    "description": "Appends content to the end of an existing file.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "file_path": {
                                "type": "string",
                                "description": "Relative path to the file inside workspace."
                            },
                            "content": {
                                "type": "string",
                                "description": "The text content to append."
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
                    "description": "Deletes a file or directory from the workspace.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "file_path": {
                                "type": "string",
                                "description": "Relative path to the file or directory."
                            }
                        },
                        "required": ["file_path"]
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "move_file",
                    "description": "Moves or renames a file/directory within the workspace.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "source": {
                                "type": "string",
                                "description": "Relative path of the source file/directory."
                            },
                            "destination": {
                                "type": "string",
                                "description": "Relative path of the destination."
                            }
                        },
                        "required": ["source", "destination"]
                    }
                }
            },
            {
                "type": "function",
                "function": {
                    "name": "copy_file",
                    "description": "Copies a file or directory within the workspace.",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "source": {
                                "type": "string",
                                "description": "Relative path of the source file/directory."
                            },
                            "destination": {
                                "type": "string",
                                "description": "Relative path of the destination."
                            }
                        },
                        "required": ["source", "destination"]
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
                if not os.path.isdir(path):
                    return f"✗ Not a directory: {args.get('path', '.')}"
                entries = os.listdir(path)
                if not entries:
                    return "Directory is empty."
                lines = []
                for e in sorted(entries):
                    full = os.path.join(path, e)
                    if os.path.isdir(full):
                        lines.append(f"📁 {e}/")
                    else:
                        size = os.path.getsize(full)
                        lines.append(f"📄 {e} ({size} bytes)")
                return "\n".join(lines)

            elif tool_name == "read_file":
                path = self._safe_path(args["file_path"])
                with open(path, "r", encoding="utf-8") as f:
                    return f.read()

            elif tool_name == "write_file":
                path = self._safe_path(args["file_path"])
                os.makedirs(os.path.dirname(path) or WORKSPACE_DIR, exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(args["content"])
                return f"✓ Written to {args['file_path']}"

            elif tool_name == "append_file":
                path = self._safe_path(args["file_path"])
                with open(path, "a", encoding="utf-8") as f:
                    f.write(args["content"])
                return f"✓ Appended to {args['file_path']}"

            elif tool_name == "delete_file":
                path = self._safe_path(args["file_path"])
                if os.path.isdir(path):
                    shutil.rmtree(path)
                    return f"✓ Deleted directory {args['file_path']}"
                elif os.path.exists(path):
                    os.remove(path)
                    return f"✓ Deleted {args['file_path']}"
                else:
                    return f"✗ Not found: {args['file_path']}"

            elif tool_name == "move_file":
                src = self._safe_path(args["source"])
                dst = self._safe_path(args["destination"])
                os.makedirs(os.path.dirname(dst) or WORKSPACE_DIR, exist_ok=True)
                shutil.move(src, dst)
                return f"✓ Moved {args['source']} → {args['destination']}"

            elif tool_name == "copy_file":
                src = self._safe_path(args["source"])
                dst = self._safe_path(args["destination"])
                os.makedirs(os.path.dirname(dst) or WORKSPACE_DIR, exist_ok=True)
                if os.path.isdir(src):
                    shutil.copytree(src, dst)
                else:
                    shutil.copy2(src, dst)
                return f"✓ Copied {args['source']} → {args['destination']}"

            elif tool_name == "create_directory":
                path = self._safe_path(args["path"])
                os.makedirs(path, exist_ok=True)
                return f"✓ Created directory {args['path']}"

        except FileNotFoundError:
            return f"✗ Not found: {args.get('file_path', args.get('path', args.get('source', '')))}"
        except PermissionError as e:
            return f"✗ Permission denied: {e}"
        except Exception as e:
            return f"✗ Filesystem error: {e}"

        return f"✗ Unknown filesystem tool: {tool_name}"