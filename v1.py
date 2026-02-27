import json
from openai import OpenAI

# ==========================================
# 1. INITIALIZE THE COMMUNITY API GATEWAY
# ==========================================
# Ollama runs locally and provides an OpenAI-compatible endpoint.
# No paid API keys or internet connection required.
client = OpenAI(
    base_url="http://localhost:11434/v1",
    api_key="ollama", # API key is ignored by local Ollama, but required by the client package
)

# Use a community model that you have pulled locally
MODEL_NAME = "llama3.2" 

# ==========================================
# 2. DEFINE YOUR TOOLS (The I/O Layer)
# ==========================================
def read_file(filepath):
    return f"Content of {filepath}: [Mock File Data]"

def execute_code(code_string):
    print(f"\n>>> Running in Local Sandbox: \n{code_string}\n>>>")
    return "Execution success: 0 errors. Output: Hello, Local Swan OS!"

def search_google(query):
    # Note: If you are entirely offline, this tool would fail. 
    # For a truly offline OS, this would be replaced by a local RAG vector search.
    return f"Search results for '{query}': [Mock Search Results]"

available_tools = {
    "read_file": read_file,
    "execute_code": execute_code,
    "search_google": search_google
}

llm_tools = [
    {
        "type": "function",
        "function": {
            "name": "execute_code",
            "description": "Executes Python code in a sandboxed environment.",
            "parameters": {
                "type": "object",
                "properties": {
                    "code_string": {
                        "type": "string",
                        "description": "The Python code to execute."
                    }
                },
                "required": ["code_string"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "search_google",
            "description": "Searches the web for current information.",
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "The search query."
                    }
                },
                "required": ["query"]
            }
        }
    }
]

# ==========================================
# 3. THE COGNITIVE LOOP (The Scheduler)
# ==========================================
system_prompt = """You are an OS. You have access to tools. Think step-by-step. Do not guess information."""

def cognitive_kernel_loop(user_intent):
    print(f"[Swan OS Booting] User Intent: {user_intent}\n")
    
    messages = [
        {"role": "system", "content": system_prompt},
        {"role": "user", "content": user_intent}
    ]

    while True:
        print(f"[{MODEL_NAME} Kernel Thinking...]")
        
        # Call the local Ollama model
        response = client.chat.completions.create(
            model=MODEL_NAME,
            messages=messages,
            tools=llm_tools,
            tool_choice="auto"
        )
        
        response_message = response.choices[0].message
        messages.append(response_message)
        
        tool_calls = response_message.tool_calls
        
        if tool_calls:
            for tool_call in tool_calls:
                tool_name = tool_call.function.name
                
                # Ollama's tool arguments sometimes need extra handling depending on the model
                try:
                    tool_args = json.loads(tool_call.function.arguments)
                except json.JSONDecodeError:
                    print("[Error]: Model hallucinated invalid JSON arguments.")
                    break
                
                print(f"[Orchestrator]: Routing to '{tool_name}' with args: {tool_args}...")
                
                if tool_name in available_tools:
                    if tool_name == "execute_code":
                        result = available_tools[tool_name](tool_args.get("code_string", ""))
                    elif tool_name == "search_google":
                        result = available_tools[tool_name](tool_args.get("query", ""))
                        
                    print(f"[System]: Tool returned -> {result}\n")
                    
                    messages.append({
                        "tool_call_id": tool_call.id,
                        "role": "tool",
                        "name": tool_name,
                        "content": result
                    })
                else:
                    print(f"[Error]: Tool {tool_name} not recognized.")
        else:
            print(f"\n[Final Output]: {response_message.content}")
            break

# Run the local OS
cognitive_kernel_loop("Write a python script to print hello world and execute it.")