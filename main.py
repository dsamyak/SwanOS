"""
SwanOS — Boot Sequence
Entry point: initializes the kernel and starts the interactive REPL.
"""

import sys
import time

from config import MODEL_NAME, REQUEST_DELAY
from kernel.core import LLMKernel

BANNER = r"""
 ███████╗██╗    ██╗ █████╗ ███╗   ██╗     ██████╗ ███████╗
 ██╔════╝██║    ██║██╔══██╗████╗  ██║    ██╔═══██╗██╔════╝
 ███████╗██║ █╗ ██║███████║██╔██╗ ██║    ██║   ██║███████╗
 ╚════██║██║███╗██║██╔══██║██║╚██╗██║    ██║   ██║╚════██║
 ███████║╚███╔███╔╝██║  ██║██║ ╚████║    ╚██████╔╝███████║
 ╚══════╝ ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═══╝     ╚═════╝ ╚══════╝
"""


def boot():
    """Initialize the kernel and start the REPL."""
    print(BANNER)
    print(f"  Model   : {MODEL_NAME}")
    print(f"  Status  : ONLINE")
    print(f"  Type 'exit' or 'shutdown' to power off.\n")
    print("─" * 56)

    try:
        kernel = LLMKernel()
    except Exception as e:
        print(f"\n  ✗ BOOT FAILURE: {e}")
        sys.exit(1)

    # ── Interactive Loop ────────────────────────────────────
    while True:
        try:
            user_input = input("\n  You ❯ ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n\n  Shutting down SwanOS… Goodbye.")
            break

        if not user_input:
            continue
        if user_input.lower() in ("exit", "shutdown", "quit"):
            print("\n  Powering off SwanOS… Goodbye.")
            break

        print()
        try:
            response = kernel.run(user_input)
            print(f"\n  SwanOS ❯ {response}")
        except Exception as e:
            print(f"\n  ✗ Kernel Error: {e}")

        # Rate-limit to respect free-tier quotas
        time.sleep(REQUEST_DELAY)


if __name__ == "__main__":
    boot()