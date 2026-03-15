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
"""

import sys
import os
import time
import argparse
import logging
from groq import Groq

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
    log_handlers = [logging.StreamHandler(sys.stdout)]
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
    conversation_history = []
    
    try:
        with open(pipe_in_path, 'rb', buffering=0) as pipe_in, \
             open(pipe_out_path, 'wb', buffering=0) as pipe_out:
                 
            logger.info("Bridge connected. Waiting for commands from SwanOS...")
            
            buffer = b""
            while True:
                chunk = pipe_in.read(1)
                if not chunk:
                    time.sleep(0.01)
                    continue
                    
                buffer += chunk
                
                # Check for EOT marker (\x04)
                if b'\x04' in buffer:
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
                        
                    cmd_type = chr(msg_content[0])
                    payload = msg_content[1:].decode('utf-8', errors='ignore').strip()
                    
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
                        
                        # Query Groq
                        try:
                            completion = client.chat.completions.create(
                                model=current_model,
                                messages=messages,
                                temperature=current_temperature,
                                max_completion_tokens=256,
                            )
                            response = completion.choices[0].message.content.strip()
                            logger.info(f"Response (len {len(response)})")
                            
                            # Update conversation history
                            conversation_history.append({"role": "user", "content": payload})
                            conversation_history.append({"role": "assistant", "content": response})
                            
                            # Prune history to avoid context limits
                            if len(conversation_history) > MAX_HISTORY * 2:
                                conversation_history = conversation_history[-(MAX_HISTORY * 2):]
                                
                        except Exception as e:
                            logger.error(f"Error querying Groq: {e}")
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
