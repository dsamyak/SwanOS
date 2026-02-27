"""
SwanOS — Centralized Configuration
Loads environment variables and defines system-wide settings.
Supports multiple LLM providers (Groq, Gemini).
"""

import os
from dotenv import load_dotenv

load_dotenv()

# ── LLM Provider ──────────────────────────────────────────
LLM_PROVIDER = os.getenv("LLM_PROVIDER", "groq").lower()   # "groq" or "gemini"

# ── Provider-specific settings ─────────────────────────────
if LLM_PROVIDER == "groq":
    MODEL_NAME = os.getenv("SWANOS_MODEL", "llama-3.3-70b-versatile")
    API_BASE_URL = "https://api.groq.com/openai/v1/chat/completions"
    _raw_keys = os.getenv("GROQ_API_KEY", "")
else:
    MODEL_NAME = os.getenv("SWANOS_MODEL", "gemini-2.0-flash")
    API_BASE_URL = "https://generativelanguage.googleapis.com/v1beta/models"
    _raw_keys = os.getenv("GEMINI_API_KEY", "")

# Support multiple comma-separated keys for rotation
API_KEYS = [k.strip() for k in _raw_keys.split(",") if k.strip()]

# ── Paths ──────────────────────────────────────────────────
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
WORKSPACE_DIR = os.path.join(BASE_DIR, "workspace")

# Ensure workspace exists
os.makedirs(WORKSPACE_DIR, exist_ok=True)

# ── Kernel Settings ────────────────────────────────────────
MAX_TOOL_ROUNDS = 10          # Safety guard: max LLM↔tool loops per query
CODE_EXEC_TIMEOUT = 10        # Seconds before code execution is killed
REQUEST_DELAY = 0             # No delay — key rotation handles rate limits

# ── System Prompt ──────────────────────────────────────────
SYSTEM_PROMPT = """You are SwanOS, an AI-powered operating system.

You have tools to manage files in a sandboxed workspace, execute Python code, and search the web.

Efficiency rules (CRITICAL):
- Call MULTIPLE tools at once in a single response when possible.
- For file creation: just call write_file directly. Do NOT list or read first.
- For deletion: just call delete_file directly. Do NOT check if it exists first.
- Never do unnecessary read/list calls before a write or delete.
- Minimize the number of tool-call rounds. Batch related operations together.
- Be concise. Report results in 1-2 sentences.
- Use tools instead of fabricating information.
"""
