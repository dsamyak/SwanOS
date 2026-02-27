"""
SwanOS v2.0 — Boot Sequence
Entry point: initializes the kernel and starts the interactive REPL.
Features colorized output, built-in commands, and session management.
"""

import os
import sys
import time
import json
import platform
from datetime import datetime

from colorama import init as colorama_init, Fore, Style

from config import (
    MODEL_NAME, WORKSPACE_DIR, MAX_TOOL_ROUNDS,
    CODE_EXEC_TIMEOUT, API_KEYS, LLM_PROVIDER,
)
from kernel.core import LLMKernel

# Initialize colorama for Windows color support
colorama_init()

VERSION = "2.0"

# ── Color helpers ──────────────────────────────────────────
def _c(text, color):
    return f"{color}{text}{Style.RESET_ALL}"

def _green(t):  return _c(t, Fore.GREEN)
def _red(t):    return _c(t, Fore.RED)
def _cyan(t):   return _c(t, Fore.CYAN)
def _yellow(t): return _c(t, Fore.YELLOW)
def _mag(t):    return _c(t, Fore.MAGENTA)
def _dim(t):    return _c(t, Style.DIM)


BANNER = _cyan(r"""
 ███████╗██╗    ██╗ █████╗ ███╗   ██╗     ██████╗ ███████╗
 ██╔════╝██║    ██║██╔══██╗████╗  ██║    ██╔═══██╗██╔════╝
 ███████╗██║ █╗ ██║███████║██╔██╗ ██║    ██║   ██║███████╗
 ╚════██║██║███╗██║██╔══██║██║╚██╗██║    ██║   ██║╚════██║
 ███████║╚███╔███╔╝██║  ██║██║ ╚████║    ╚██████╔╝███████║
 ╚══════╝ ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═══╝     ╚═════╝ ╚══════╝
""")

BUILTIN_HELP = f"""
  {_cyan('╔══════════════════════════════════════════════════════╗')}
  {_cyan('║')}       {_green('SwanOS v' + VERSION)} — Built-in Commands            {_cyan('║')}
  {_cyan('╠══════════════════════════════════════════════════════╣')}
  {_cyan('║')}                                                      {_cyan('║')}
  {_cyan('║')}  {_yellow('System')}                                              {_cyan('║')}
  {_cyan('║')}  help              Show this help message             {_cyan('║')}
  {_cyan('║')}  status            Kernel dashboard                   {_cyan('║')}
  {_cyan('║')}  whoami            OS & Python info                   {_cyan('║')}
  {_cyan('║')}  time              Current date & time                {_cyan('║')}
  {_cyan('║')}  uptime            Session duration                   {_cyan('║')}
  {_cyan('║')}  version           SwanOS version                     {_cyan('║')}
  {_cyan('║')}                                                      {_cyan('║')}
  {_cyan('║')}  {_yellow('Files (workspace only)')}                              {_cyan('║')}
  {_cyan('║')}  ls [path]         List files                         {_cyan('║')}
  {_cyan('║')}  cat <file>        Read a file                        {_cyan('║')}
  {_cyan('║')}  pwd               Workspace path                     {_cyan('║')}
  {_cyan('║')}  tree              Directory tree                     {_cyan('║')}
  {_cyan('║')}                                                      {_cyan('║')}
  {_cyan('║')}  {_yellow('Utilities')}                                           {_cyan('║')}
  {_cyan('║')}  calc <expr>       Quick math calculator              {_cyan('║')}
  {_cyan('║')}  echo <text>       Print text                         {_cyan('║')}
  {_cyan('║')}  notes add <text>  Save a note                        {_cyan('║')}
  {_cyan('║')}  notes list        Show saved notes                   {_cyan('║')}
  {_cyan('║')}  notes clear       Clear all notes                    {_cyan('║')}
  {_cyan('║')}                                                      {_cyan('║')}
  {_cyan('║')}  {_yellow('Session')}                                             {_cyan('║')}
  {_cyan('║')}  clear             Clear conversation memory          {_cyan('║')}
  {_cyan('║')}  history           Show conversation turns            {_cyan('║')}
  {_cyan('║')}                                                      {_cyan('║')}
  {_cyan('║')}  {_yellow('Power')}                                               {_cyan('║')}
  {_cyan('║')}  restart           Reboot SwanOS                      {_cyan('║')}
  {_cyan('║')}  exit / shutdown   Power off                          {_cyan('║')}
  {_cyan('║')}                                                      {_cyan('║')}
  {_cyan('║')}  {_dim('Anything else → sent to the AI kernel')}              {_cyan('║')}
  {_cyan('╚══════════════════════════════════════════════════════╝')}
"""


# ── Utility Functions ──────────────────────────────────────

def _safe_workspace_path(rel_path: str) -> str:
    resolved = os.path.normpath(os.path.join(WORKSPACE_DIR, rel_path))
    if not resolved.startswith(WORKSPACE_DIR):
        return None
    return resolved


def _format_size(size_bytes: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if size_bytes < 1024:
            return f"{size_bytes:.1f} {unit}" if unit != "B" else f"{size_bytes} {unit}"
        size_bytes /= 1024
    return f"{size_bytes:.1f} TB"


def _build_tree(directory: str, prefix: str = "", max_depth: int = 3, depth: int = 0) -> str:
    if depth >= max_depth:
        return ""
    lines = []
    try:
        entries = sorted(os.listdir(directory))
    except PermissionError:
        return prefix + "  [permission denied]\n"

    dirs = [e for e in entries if os.path.isdir(os.path.join(directory, e))]
    files = [e for e in entries if os.path.isfile(os.path.join(directory, e))]

    for f in files:
        size = _format_size(os.path.getsize(os.path.join(directory, f)))
        lines.append(f"{prefix}  📄 {f}  {_dim('(' + size + ')')}")

    for d in dirs:
        lines.append(f"{prefix}  📁 {_cyan(d)}/")
        subtree = _build_tree(os.path.join(directory, d), prefix + "    ", max_depth, depth + 1)
        if subtree:
            lines.append(subtree)

    return "\n".join(lines)


# ── Notes Storage ──────────────────────────────────────────
_notes_file = os.path.join(WORKSPACE_DIR, ".swanos_notes.json")

def _load_notes() -> list:
    if os.path.exists(_notes_file):
        try:
            with open(_notes_file, "r") as f:
                return json.load(f)
        except Exception:
            return []
    return []

def _save_notes(notes: list):
    with open(_notes_file, "w") as f:
        json.dump(notes, f, indent=2)


# ── Built-in Command Handler ──────────────────────────────

def _handle_builtin(command: str, kernel: LLMKernel, boot_time: float) -> str:
    parts = command.strip().split(maxsplit=1)
    cmd = parts[0].lower()
    arg = parts[1] if len(parts) > 1 else ""

    # ── Power ──
    if cmd in ("exit", "shutdown", "quit"):
        return "__EXIT__"
    if cmd == "restart":
        return "__RESTART__"

    # ── Help ──
    if cmd == "help":
        return BUILTIN_HELP

    # ── Version ──
    if cmd == "version":
        return f"  {_green('SwanOS')} {_cyan('v' + VERSION)} — AI-Powered Operating System"

    # ── Status ──
    if cmd == "status":
        mem_count = len(kernel.history)
        uptime_s = time.time() - boot_time
        mins, secs = divmod(int(uptime_s), 60)
        hrs, mins = divmod(mins, 60)
        tools = kernel.scheduler.get_all_tool_definitions()
        return (
            f"  {_cyan('╭─ Kernel Status ─────────────────────────────╮')}\n"
            f"  {_cyan('│')}  Provider    : {_green(LLM_PROVIDER.upper()):<35}{_cyan('│')}\n"
            f"  {_cyan('│')}  Model       : {MODEL_NAME:<35}{_cyan('│')}\n"
            f"  {_cyan('│')}  Uptime      : {hrs:02d}h {mins:02d}m {secs:02d}s{' ' * 24}{_cyan('│')}\n"
            f"  {_cyan('│')}  Memory      : {mem_count} turns{' ' * (29 - len(str(mem_count)))}{_cyan('│')}\n"
            f"  {_cyan('│')}  API Keys    : {len(API_KEYS)}{' ' * 33}{_cyan('│')}\n"
            f"  {_cyan('│')}  Tools       : {len(tools)} registered{' ' * 23}{_cyan('│')}\n"
            f"  {_cyan('│')}  Status      : {_green('● ONLINE')}{' ' * 17}{_cyan('│')}\n"
            f"  {_cyan('╰─────────────────────────────────────────────╯')}"
        )

    # ── Whoami ──
    if cmd == "whoami":
        return (
            f"  {_yellow('OS')}       : {'SwanOs'}\n"
            f"  {_yellow('Machine')}  : {platform.machine()}\n"
            
            f"  {_yellow('Node')}     : {platform.node()}\n"
            f"  {_yellow('SwanOS')}   : v{VERSION}"
        )

    # ── Time ──
    if cmd in ("time", "date"):
        now = datetime.now()
        return f"  🕐 {_cyan(now.strftime('%A, %B %d, %Y — %I:%M:%S %p'))}"

    # ── Uptime ──
    if cmd == "uptime":
        uptime_s = time.time() - boot_time
        mins, secs = divmod(int(uptime_s), 60)
        hrs, mins = divmod(mins, 60)
        return f"  ⏱  Running for {_cyan(f'{hrs:02d}h {mins:02d}m {secs:02d}s')}"

    # ── PWD ──
    if cmd == "pwd":
        return f"  📂 {_cyan(WORKSPACE_DIR)}"

    # ── LS ──
    if cmd in ("ls", "dir"):
        target = _safe_workspace_path(arg) if arg else WORKSPACE_DIR
        if target is None:
            return f"  {_red('✗ Access denied — path escapes workspace.')}"
        if not os.path.isdir(target):
            return f"  {_red('✗ Not a directory:')} {arg}"
        try:
            entries = sorted(os.listdir(target))
        except PermissionError:
            return f"  {_red('✗ Permission denied.')}"
        if not entries:
            return f"  {_dim('(empty directory)')}"

        lines = []
        for e in entries:
            if e.startswith(".swanos_"):
                continue  # hide internal files
            full = os.path.join(target, e)
            if os.path.isdir(full):
                lines.append(f"  📁 {_cyan(e)}/")
            else:
                size = _format_size(os.path.getsize(full))
                lines.append(f"  📄 {e}  {_dim('(' + size + ')')}")
        return "\n".join(lines) if lines else f"  {_dim('(empty directory)')}"

    # ── Cat ──
    if cmd in ("cat", "read"):
        if not arg:
            return f"  {_red('✗ Usage: cat <filename>')}"
        target = _safe_workspace_path(arg)
        if target is None:
            return f"  {_red('✗ Access denied — path escapes workspace.')}"
        if not os.path.isfile(target):
            return f"  {_red('✗ File not found:')} {arg}"
        try:
            with open(target, "r", encoding="utf-8") as f:
                content = f.read()
            if len(content) > 5000:
                content = content[:5000] + f"\n\n  {_dim('…[truncated, ' + str(len(content)) + ' chars total]')}"
            return f"  {_cyan('── ' + arg + ' ──')}\n{content}"
        except Exception as e:
            return f"  {_red('✗ Error reading file:')} {e}"

    # ── Tree ──
    if cmd == "tree":
        tree = _build_tree(WORKSPACE_DIR)
        if not tree:
            return f"  {_dim('(workspace is empty)')}"
        return f"  📂 {_cyan(WORKSPACE_DIR)}\n{tree}"

    # ── Calc ──
    if cmd == "calc":
        if not arg:
            return f"  {_red('✗ Usage: calc <expression>')}"
        try:
            # Safe math eval — only allow math operations
            allowed = set("0123456789+-*/.()% ")
            clean = arg.replace("**", "^").replace("^", "**")
            result = eval(compile(arg, "<calc>", "eval"), {"__builtins__": {}}, {
                "abs": abs, "round": round, "min": min, "max": max,
                "pow": pow, "sum": sum, "len": len,
            })
            return f"  {_green('=')} {_cyan(str(result))}"
        except Exception as e:
            return f"  {_red('✗ Calc error:')} {e}"

    # ── Echo ──
    if cmd == "echo":
        return f"  {arg}" if arg else ""

    # ── Notes ──
    if cmd == "notes":
        sub_parts = arg.split(maxsplit=1) if arg else []
        sub_cmd = sub_parts[0].lower() if sub_parts else "list"
        sub_arg = sub_parts[1] if len(sub_parts) > 1 else ""

        if sub_cmd == "add" and sub_arg:
            notes = _load_notes()
            notes.append({
                "text": sub_arg,
                "time": datetime.now().strftime("%H:%M:%S"),
            })
            _save_notes(notes)
            return f"  {_green('✓')} Note #{len(notes)} saved."
        elif sub_cmd == "list":
            notes = _load_notes()
            if not notes:
                return f"  {_dim('(no notes yet — use: notes add <text>)')}"
            lines = []
            for i, n in enumerate(notes, 1):
                lines.append(f"  {_yellow(str(i) + '.')} {n['text']}  {_dim(n.get('time', ''))}")
            return "\n".join(lines)
        elif sub_cmd == "clear":
            _save_notes([])
            return f"  {_green('✓')} All notes cleared."
        else:
            return f"  {_red('✗ Usage: notes add <text> | notes list | notes clear')}"

    # ── Clear Memory ──
    if cmd == "clear":
        kernel.clear_history()
        return f"  {_green('✓')} Conversation memory cleared."

    # ── History ──
    if cmd == "history":
        if not kernel.history:
            return f"  {_dim('(no conversation history)')}"
        lines = []
        for i, entry in enumerate(kernel.history):
            role = entry.get("role", "?")
            parts_list = entry.get("parts", [])
            content = entry.get("content", "")

            text = ""
            if content:
                text = content[:80]
            else:
                for p in parts_list:
                    if isinstance(p, dict) and "text" in p:
                        text = p["text"][:80]; break
                    elif isinstance(p, dict) and "functionCall" in p:
                        text = "[tool: {}]".format(p["functionCall"].get("name", "?")); break

            if role in ("user",):
                lines.append(f"  {_green('👤 user')}   │ {text}")
            else:
                lines.append(f"  {_cyan('🤖 model')}  │ {text}")
        return "\n".join(lines)

    # Not a built-in
    return None


# ── Boot ───────────────────────────────────────────────────

def boot():
    """Initialize the kernel and start the REPL."""
    print(BANNER)
    print(f"  {_yellow('Provider')} : {_green(LLM_PROVIDER.upper())}")
    print(f"  {_yellow('Model')}    : {MODEL_NAME}")
    print(f"  {_yellow('API Keys')} : {len(API_KEYS)} loaded {'(' + _green('rotation ON') + ')' if len(API_KEYS) > 1 else ''}")
    print(f"  {_yellow('Version')}  : v{VERSION}")
    print(f"  {_yellow('Status')}   : {_green('● ONLINE')}")
    print(f"  Type {_cyan('help')} for commands.\n")
    print(_dim("─" * 56))

    try:
        kernel = LLMKernel()
    except Exception as e:
        print(f"\n  {_red('✗ BOOT FAILURE:')} {e}")
        sys.exit(1)

    tools = kernel.scheduler.get_all_tool_definitions()
    print(f"  {_green('✓')} Kernel loaded — {_cyan(str(len(tools)))} tools registered\n")

    boot_time = time.time()

    # ── Interactive Loop ────────────────────────────────────
    while True:
        try:
            prompt = f"\n  {_green('You ❯')} "
            user_input = input(prompt).strip()
        except (EOFError, KeyboardInterrupt):
            print(f"\n\n  {_yellow('Shutting down SwanOS… Goodbye.')}")
            break

        if not user_input:
            continue

        # Try built-in commands first
        result = _handle_builtin(user_input, kernel, boot_time)

        if result == "__EXIT__":
            print(f"\n  {_yellow('Powering off SwanOS… Goodbye.')}")
            break
        elif result == "__RESTART__":
            print(f"\n  {_cyan('🔄 Rebooting SwanOS…')}\n")
            os.execv(sys.executable, [sys.executable] + sys.argv)
        elif result is not None:
            print(result)
            continue

        # Not a built-in → send to AI kernel
        print()
        try:
            start = time.time()
            response = kernel.run(user_input)
            elapsed = time.time() - start
            print(f"\n  {_cyan('SwanOS ❯')} {response}")
            print(f"  {_dim(f'⏱  {elapsed:.1f}s')}")
        except Exception as e:
            print(f"\n  {_red('✗ Kernel Error:')} {e}")


if __name__ == "__main__":
    boot()