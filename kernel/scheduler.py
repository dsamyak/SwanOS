"""
SwanOS — Scheduler (Tool Router)
Maps tool names to driver instances and dispatches calls.
"""


class Scheduler:
    """Routes LLM tool calls to the correct driver."""

    def __init__(self):
        self._tool_map: dict = {}      # tool_name → driver instance
        self._drivers: list = []

    def register_driver(self, driver) -> None:
        """Register a driver and index all of its tools."""
        self._drivers.append(driver)
        for tool_def in driver.get_tool_definitions():
            name = tool_def["function"]["name"]
            self._tool_map[name] = driver

    def get_all_tool_definitions(self) -> list:
        """Collect tool schemas from every registered driver."""
        tools = []
        for driver in self._drivers:
            tools.extend(driver.get_tool_definitions())
        return tools

    def dispatch(self, tool_name: str, args: dict) -> str:
        """Route a tool call to the correct driver and return the result."""
        driver = self._tool_map.get(tool_name)
        if driver is None:
            return f"✗ Unknown tool: {tool_name}"
        try:
            return driver.execute(tool_name, args)
        except Exception as e:
            return f"✗ Driver error in '{tool_name}': {e}"
