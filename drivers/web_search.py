"""
SwanOS — Web Search Driver
Provides real web search using DuckDuckGo (free, no API key required).
"""

from duckduckgo_search import DDGS


class WebSearchDriver:
    """Searches the web via DuckDuckGo and returns summarised results."""

    def __init__(self):
        self.max_results = 5

    def get_tool_definitions(self):
        """Returns OpenAI-style tool schemas for the LLM."""
        return [
            {
                "type": "function",
                "function": {
                    "name": "web_search",
                    "description": "Searches the web for current information and returns top results with titles, URLs, and snippets.",
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

    def execute(self, tool_name: str, args: dict) -> str:
        """Run the requested tool."""
        if tool_name == "web_search":
            return self._search(args.get("query", ""))
        return f"✗ Unknown search tool: {tool_name}"

    def _search(self, query: str) -> str:
        """Perform a DuckDuckGo search and format the results."""
        if not query.strip():
            return "✗ No search query provided."

        try:
            with DDGS() as ddgs:
                results = list(ddgs.text(query, max_results=self.max_results))

            if not results:
                return f"No results found for '{query}'."

            formatted = []
            for i, r in enumerate(results, 1):
                title = r.get("title", "No title")
                url = r.get("href", "")
                snippet = r.get("body", "No description")
                formatted.append(f"{i}. {title}\n   {url}\n   {snippet}")

            return f"Search results for '{query}':\n\n" + "\n\n".join(formatted)

        except Exception as e:
            return f"✗ Search failed: {e}"
