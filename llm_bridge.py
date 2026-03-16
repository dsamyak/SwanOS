#!/usr/bin/env python3
"""
SwanOS - Groq API Serial Bridge
Reads commands from the QEMU/VirtualBox serial pipe and forwards them to the Groq API.
Supports conversation history, model switching, and system prompts.

Setup:
    pip install groq
    
Usage:
    ./llm_bridge.py <serial_in_pipe> <serial_out_pipe> [--log <logfile>]
    
Protocol:
    \x01K<api_key>\x04       - Sets the Groq API key
    \x01Q<query>\x04         - Sends a query to the LLM (responds with text + \x04)
    \x01M<model_name>\x04    - Sets the LLM model to use
    \x01S<system_prompt>\x04 - Sets the system prompt
    \x01C\x04                - Clears the conversation history
    \x01T<temperature>\x04   - Sets the temperature (0.0 to 2.0)
    \x01O<max_tokens>\x04    - Sets the max tokens for response
    \x01P<top_p>\x04         - Sets the top-p sampling value (0.0 to 1.0)
    \x01I\x04                - Returns current bridge info/status
"""

import sys
import os
import time
import argparse
import logging
import typing
from groq import Groq  # type: ignore

# Default configurations
DEFAULT_MODEL = "llama-3.3-70b-versatile"
DEFAULT_SYSTEM_PROMPT = "You are SwanOS AI, an intelligent assistant built directly into the core of a bare-metal operating system. Keep your answers concise, helpful, and under 50 words when possible as the OS terminal is small."
DEFAULT_TEMPERATURE = 0.7
MAX_HISTORY = 10  # Maximum number of messages to keep in history to avoid token limits

def main():
    parser = argparse.ArgumentParser(description="SwanOS Groq API Serial Bridge")
    parser.add_argument("pipe_in", help="Path to the serial input pipe (from emulator)")
    parser.add_argument("pipe_out", help="Path to the serial output pipe (to emulator)")
    parser.add_argument("--log", help="Optional log file path", default=None)
    
    args = parser.parse_args()
    
    # Configure logging
    log_handlers: list[logging.Handler] = [logging.StreamHandler(sys.stdout)]
    if args.log:
        log_handlers.append(logging.FileHandler(args.log))
        
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s [%(levelname)s] %(message)s',
        handlers=log_handlers
    )
    
    logger = logging.getLogger("SwanOS-Bridge")
    
    pipe_in_path = args.pipe_in
    pipe_out_path = args.pipe_out
    
    # Create host data directory for SwanOS persistent storage
    HOST_DATA_DIR = "host_data"
    os.makedirs(HOST_DATA_DIR, exist_ok=True)
    
    logger.info("Starting SwanOS Groq Bridge")
    logger.info(f"Listening on {pipe_in_path} (IN) / {pipe_out_path} (OUT)")
    
    # Wait for pipes to exist (created by QEMU/VBox)
    while not os.path.exists(pipe_in_path) or not os.path.exists(pipe_out_path):
        logger.info("Waiting for pipes to be created by emulator...")
        time.sleep(2)
        
    api_key = None
    client = None
    
    # State
    current_model = DEFAULT_MODEL
    current_system_prompt = DEFAULT_SYSTEM_PROMPT
    current_temperature = DEFAULT_TEMPERATURE
    current_max_tokens = 256
    current_top_p = 1.0
    conversation_history: list[dict[str, str]] = []
    
    try:
        with open(pipe_in_path, 'rb', buffering=0) as pipe_in, \
             open(pipe_out_path, 'wb', buffering=0) as pipe_out:
                 
            logger.info("Bridge connected. Waiting for commands from SwanOS...")
            
            buffer: bytes = b""
            while True:
                chunk = pipe_in.read(1024)
                if not chunk:
                    time.sleep(0.01)
                    continue
                    
                buffer += chunk
                
                # Process all complete messages in the buffer (\x04)
                while b'\x04' in buffer:
                    parts = buffer.split(b'\x04', 1)
                    raw_msg = parts[0]
                    buffer = parts[1]
                    
                    # Ensure it has the start of heading (\x01)
                    if b'\x01' not in raw_msg:
                        continue
                        
                    # Extract message from the last \x01
                    msg_content = raw_msg[raw_msg.rfind(b'\x01') + 1:]
                    
                    if len(msg_content) < 1:
                        continue
                        
                    cmd_type = chr(typing.cast(int, msg_content[0]))
                    payload = typing.cast(bytes, msg_content[1:]).decode('utf-8', errors='ignore').strip()
                    
                    if cmd_type == 'K': # Set API Key
                        logger.info("Received new API key configuration from OS")
                        api_key = payload
                        client = Groq(api_key=api_key)
                        
                    elif cmd_type == 'M': # Set Model
                        logger.info(f"Changing model to: {payload}")
                        current_model = payload
                        
                    elif cmd_type == 'S': # Set System Prompt
                        logger.info(f"Changing system prompt to: {payload}")
                        current_system_prompt = payload
                        conversation_history = [] # Reset history when system prompt changes
                        
                    elif cmd_type == 'C': # Clear History
                        logger.info("Clearing conversation history")
                        conversation_history = []
                        
                    elif cmd_type == 'T': # Set Temperature
                        try:
                            temp_val = float(payload)
                            current_temperature = max(0.0, min(2.0, temp_val))
                            logger.info(f"Temperature set to {current_temperature}")
                        except ValueError:
                            logger.error(f"Invalid temperature received: {payload}")
                            
                    elif cmd_type == 'O': # Set Max Tokens
                        try:
                            tokens = int(payload)
                            current_max_tokens = max(1, tokens)
                            logger.info(f"Max tokens set to {current_max_tokens}")
                        except ValueError:
                            logger.error(f"Invalid max tokens received: {payload}")
                            
                    elif cmd_type == 'P': # Set Top P
                        try:
                            p_val = float(payload)
                            current_top_p = max(0.0, min(1.0, p_val))
                            logger.info(f"Top-P set to {current_top_p}")
                        except ValueError:
                            logger.error(f"Invalid Top-P received: {payload}")
                            
                    elif cmd_type == 'I': # Get Bridge Info
                        logger.info("Responding to info/ping request")
                        status_str = f"Bridge Active | Model: {current_model} | Temp: {current_temperature} | Top-P: {current_top_p} | MaxTokens: {current_max_tokens}"
                        pipe_out.write(status_str.encode('utf-8') + b'\x04')
                        pipe_out.flush()
                        continue
                        
                    elif cmd_type == 'V':  # Host Save (V for "Volume Save")
                        # Format: Vfilename|content
                        parts = payload.split('|', 1)
                        if len(parts) == 2:
                            filename, content = parts
                            # Prevent directory traversal
                            safe_name = os.path.basename(filename)
                            file_path = os.path.join(HOST_DATA_DIR, safe_name)
                            try:
                                with open(file_path, 'w', encoding='utf-8') as f:
                                    f.write(content)
                                logger.info(f"Host Save: {safe_name}")
                            except Exception as e:
                                logger.error(f"Host Save Error: {e}")
                                
                    elif cmd_type == 'L':  # Host Load (L for "Load")
                        # Format: Lfilename (responds with content + \x04)
                        safe_name = os.path.basename(payload)
                        file_path = os.path.join(HOST_DATA_DIR, safe_name)
                        try:
                            if os.path.exists(file_path):
                                with open(file_path, 'r', encoding='utf-8') as f:
                                    content = f.read()
                                pipe_out.write(content.encode('utf-8') + b'\x04')
                            else:
                                pipe_out.write(b'\x04')  # Empty response for not found
                            logger.info(f"Host Load: {safe_name}")
                        except Exception as e:
                            logger.error(f"Host Load Error: {e}")
                            pipe_out.write(b'\x04')
                        pipe_out.flush()
                        continue

                    elif cmd_type == 'A':  # Host Audit (A for "Audit Append")
                        # Format: Acontent
                        audit_path = os.path.join(HOST_DATA_DIR, "audit.log")
                        try:
                            with open(audit_path, 'a', encoding='utf-8') as f:
                                timestamp = time.strftime("[%Y-%m-%d %H:%M:%S]")
                                f.write(f"{timestamp} {payload}\n")
                            logger.info(f"Host Audit: {payload}")
                        except Exception as e:
                            logger.error(f"Host Audit Error: {e}")
                            
                    elif cmd_type == 'Q': # LLM Query
                        logger.info(f"Query: {payload}")
                        
                        if not client:
                            response = "Error: Groq API key not set or invalid."
                            logger.warning(response)
                            pipe_out.write(response.encode('utf-8') + b'\x04')
                            pipe_out.flush()
                            continue
                            
                        # Build messages array
                        messages = [{"role": "system", "content": current_system_prompt}]
                        messages.extend(conversation_history)
                        messages.append({"role": "user", "content": payload})
                        
                        # Query Groq with Retry Logic
                        max_retries = 3
                        for attempt in range(max_retries):
                            try:
                                completion = client.chat.completions.create(  # type: ignore
                                    model=current_model,
                                    messages=messages,
                                    temperature=current_temperature,
                                    max_completion_tokens=current_max_tokens,
                                    top_p=current_top_p,
                                )
                                response = completion.choices[0].message.content.strip()
                                logger.info(f"Response (len {len(response)})")
                                
                                # Update conversation history
                                conversation_history.append({"role": "user", "content": payload})  # pyre-ignore
                                conversation_history.append({"role": "assistant", "content": response})  # pyre-ignore
                                
                                # Prune history to avoid context limits
                                if len(conversation_history) > MAX_HISTORY * 2:
                                    conversation_history = conversation_history[-(MAX_HISTORY * 2):]  # pyre-ignore
                                break # Successful query, exit retry loop
                                    
                            except Exception as e:
                                logger.error(f"Error querying Groq (Attempt {attempt + 1}/{max_retries}): {e}")
                                if attempt < max_retries - 1:
                                    time.sleep(2 ** attempt) # Exponential backoff
                                else:
                                    response = f"API Error: {str(e)}"
                            
                        # Send back to OS
                        pipe_out.write(response.encode('utf-8') + b'\x04')
                        pipe_out.flush()
                        
    except KeyboardInterrupt:
        logger.info("\nBridge shutting down.")
    except Exception as e:
        logger.error(f"Fatal bridge error: {e}")

if __name__ == "__main__":
    main()
