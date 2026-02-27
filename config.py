"""
SwanOS — Centralized Configuration
Loads environment variables and defines system-wide settings.
"""

import os
from dotenv import load_dotenv

load_dotenv()

# ── LLM Model ──────────────────────────────────────────────
MODEL_NAME = os.getenv("SWANOS_MODEL", "gemini-2.5-flash")

# ── API Keys ───────────────────────────────────────────────
GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "")

# ── Paths ──────────────────────────────────────────────────
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
WORKSPACE_DIR = os.path.join(BASE_DIR, "workspace")

# Ensure workspace exists
os.makedirs(WORKSPACE_DIR, exist_ok=True)

# ── Kernel Settings ────────────────────────────────────────
MAX_TOOL_ROUNDS = 10          # Safety guard: max LLM↔tool loops per query
CODE_EXEC_TIMEOUT = 10        # Seconds before code execution is killed
REQUEST_DELAY = 2             # Seconds between API calls (free-tier rate limit)

# ── System Prompt ──────────────────────────────────────────
SYSTEM_PROMPT = """You are SwanOS, an AI-powered operating system.

You have access to tools that let you interact with the local filesystem
and execute code. Use them when the user's request requires action — do not
guess file contents or execution results.

Rules:
- Think step-by-step before acting.
- Always use the tools instead of making up information.
- File operations are sandboxed to the workspace directory.
- Report tool results clearly to the user.
"""
