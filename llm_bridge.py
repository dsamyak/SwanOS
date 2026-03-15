#!/usr/bin/env python3
"""
SwanOS - Groq API Serial Bridge
Reads commands from the QEMU/VirtualBox serial pipe and forwards them to the Groq API.

Setup:
    pip install groq
    
Usage:
    ./llm_bridge.py /tmp/swanos_serial.in /tmp/swanos_serial.out
    
Protocol:
    \x01K<api_key>\x04  - Sets the Groq API key
    \x01Q<query>\x04    - Sends a query to the LLM (responds with text + \x04)
"""

import sys
import os
import time
import threading
from groq import Groq

# Model configurations
GROQ_MODEL = "llama-3.3-70b-versatile"

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <serial_in_pipe> <serial_out_pipe>")
        sys.exit(1)
        
    pipe_in_path = sys.argv[1]
    pipe_out_path = sys.argv[2]
    
    print(f"[*] Starting SwanOS Groq Bridge")
    print(f"[*] Listening on {pipe_in_path} (IN) / {pipe_out_path} (OUT)")
    
    # Wait for pipes to exist (created by QEMU/VBox)
    while not os.path.exists(pipe_in_path) or not os.path.exists(pipe_out_path):
        print(f"[*] Waiting for pipes to be created by emulator...")
        time.sleep(2)
        
    api_key = None
    client = None
    
    try:
        with open(pipe_in_path, 'rb', buffering=0) as pipe_in, \
             open(pipe_out_path, 'wb', buffering=0) as pipe_out:
                 
            print("[*] Bridge connected. Waiting for commands from SwanOS...")
            
            buffer = b""
            while True:
                chunk = pipe_in.read(1)
                if not chunk:
                    time.sleep(0.01)
                    continue
                    
                buffer += chunk
                
                # Check for EOT marker
                if b'\x04' in buffer:
                    parts = buffer.split(b'\x04', 1)
                    msg = parts[0]
                    buffer = parts[1]
                    
                    if len(msg) < 2 or msg[0] != 0x01:
                        continue # Ignore invalid framing
                        
                    cmd_type = chr(msg[1])
                    payload = msg[2:].decode('utf-8', errors='ignore').strip()
                    
                    if cmd_type == 'K': # Set API Key
                        print(f"[*] Received new API key configuration from OS")
                        api_key = payload
                        client = Groq(api_key=api_key)
                        
                    elif cmd_type == 'Q': # LLM Query
                        print(f"[>] Query: {payload}")
                        
                        if not client:
                            response = "Error: Groq API key not set or invalid."
                            print(f"[!] {response}")
                            pipe_out.write(response.encode('utf-8') + b'\x04')
                            pipe_out.flush()
                            continue
                            
                        # Query Groq
                        try:
                            completion = client.chat.completions.create(
                                model=GROQ_MODEL,
                                messages=[
                                    {"role": "system", "content": "You are SwanOS AI, an intelligent assistant built directly into the core of a bare-metal operating system. Keep your answers concise, helpful, and under 50 words when possible as the OS terminal is small."},
                                    {"role": "user", "content": payload}
                                ],
                                temperature=0.7,
                                max_completion_tokens=256,
                            )
                            response = completion.choices[0].message.content.strip()
                            print(f"[<] Response (len {len(response)})")
                            
                        except Exception as e:
                            print(f"[!] Error querying Groq: {e}")
                            response = f"API Error: {str(e)}"
                            
                        # Send back to OS
                        pipe_out.write(response.encode('utf-8') + b'\x04')
                        pipe_out.flush()
                        
    except KeyboardInterrupt:
        print("\n[*] Bridge shutting down.")
    except Exception as e:
        print(f"[!] Fatal bridge error: {e}")

if __name__ == "__main__":
    main()
