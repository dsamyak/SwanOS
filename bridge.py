"""
SwanOS — LLM Bridge
Reads queries from the VM's serial port, calls the Groq API,
and sends responses back.

Usage:
  For QEMU:    python bridge.py                (reads from stdin/stdout)
  For VirtualBox: python bridge.py --pipe COM3  (reads from named pipe)

The VM sends queries ending with \\x04 (EOT).
The bridge sends responses ending with \\x04 (EOT).
"""

import sys
import os
import json
import argparse
import requests

# ── Config ─────────────────────────────────────────────────

API_URL = "https://api.groq.com/openai/v1/chat/completions"
MODEL   = "llama-3.3-70b-versatile"

# Load API key from environment or .env file
API_KEY = os.getenv("GROQ_API_KEY", "")
if not API_KEY:
    env_path = os.path.join(os.path.dirname(__file__), ".env")
    if os.path.exists(env_path):
        for line in open(env_path):
            if line.startswith("GROQ_API_KEY="):
                API_KEY = line.split("=", 1)[1].strip()

SYSTEM_PROMPT = (
    "You are SwanOS AI, the intelligence inside a bare-metal operating system. "
    "Be concise and helpful. Keep responses under 200 words. "
    "You are running directly on x86 hardware with no other OS underneath."
)

# ── LLM Call ───────────────────────────────────────────────

def call_llm(query):
    """Call the Groq API and return the response text."""
    if not API_KEY:
        return "Error: GROQ_API_KEY not set. Set it in .env or environment."

    try:
        resp = requests.post(
            API_URL,
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {API_KEY}",
            },
            json={
                "model": MODEL,
                "messages": [
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": query},
                ],
                "max_tokens": 512,
                "temperature": 0.7,
            },
            timeout=30,
        )

        if resp.status_code == 200:
            data = resp.json()
            return data["choices"][0]["message"]["content"]
        else:
            return f"API error {resp.status_code}: {resp.text[:200]}"

    except requests.Timeout:
        return "Error: API request timed out."
    except Exception as e:
        return f"Error: {e}"


# ── Serial Bridge (stdin/stdout for QEMU) ──────────────────

def bridge_stdio():
    """Read from stdin, call LLM, write to stdout. For QEMU -serial stdio."""
    print("[SwanOS Bridge] Listening on stdin/stdout...", file=sys.stderr)
    print(f"[SwanOS Bridge] Model: {MODEL}", file=sys.stderr)
    print(f"[SwanOS Bridge] API Key: {'set' if API_KEY else 'NOT SET'}", file=sys.stderr)

    buffer = ""
    while True:
        try:
            ch = sys.stdin.read(1)
            if not ch:
                break

            if ch == '\x04':  # EOT — end of query
                query = buffer.strip()
                buffer = ""

                if query:
                    print(f"[SwanOS Bridge] Query: {query}", file=sys.stderr)
                    response = call_llm(query)
                    print(f"[SwanOS Bridge] Response: {response[:100]}...", file=sys.stderr)

                    # Send response + EOT
                    sys.stdout.write(response)
                    sys.stdout.write('\x04')
                    sys.stdout.flush()
            else:
                buffer += ch

        except KeyboardInterrupt:
            print("\n[SwanOS Bridge] Shutting down.", file=sys.stderr)
            break


# ── Serial Bridge (COM port / named pipe for VirtualBox) ───

def bridge_pipe(pipe_path):
    """Read from a named pipe (VirtualBox serial port). Windows only."""
    print(f"[SwanOS Bridge] Opening pipe: {pipe_path}", file=sys.stderr)

    try:
        import serial  # pyserial
        ser = serial.Serial(pipe_path, 115200, timeout=1)
        print(f"[SwanOS Bridge] Connected to {pipe_path}", file=sys.stderr)

        buffer = ""
        while True:
            data = ser.read(1)
            if not data:
                continue

            ch = data.decode('ascii', errors='ignore')

            if ch == '\x04':
                query = buffer.strip()
                buffer = ""

                if query:
                    print(f"[SwanOS Bridge] Query: {query}", file=sys.stderr)
                    response = call_llm(query)
                    print(f"[SwanOS Bridge] Response: {response[:100]}...", file=sys.stderr)

                    ser.write(response.encode('ascii', errors='ignore'))
                    ser.write(b'\x04')
                    ser.flush()
            else:
                buffer += ch

    except ImportError:
        print("Error: pyserial required for pipe mode. Install: pip install pyserial", file=sys.stderr)
    except KeyboardInterrupt:
        print("\n[SwanOS Bridge] Shutting down.", file=sys.stderr)
    except Exception as e:
        print(f"[SwanOS Bridge] Error: {e}", file=sys.stderr)


# ── Main ───────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="SwanOS LLM Bridge")
    parser.add_argument("--pipe", help="COM port or named pipe (e.g., COM3, /dev/ttyS0)")
    args = parser.parse_args()

    if args.pipe:
        bridge_pipe(args.pipe)
    else:
        bridge_stdio()
