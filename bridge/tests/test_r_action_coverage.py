import asyncio
import os
import re
import time
import unittest
from pathlib import Path

from bridge.ipc_client import IpcClient
from bridge.tool_schema import ALL_TOOLS


ROOT = Path(__file__).resolve().parents[2]
ACTION_EXECUTOR_SOURCES = tuple((ROOT / "src/gwa3/llm").glob("ActionExecutor*.cpp"))
DEFAULT_PIPE = r"\\.\pipe\gwa3_llm"

LOCAL_ONLY_TOOLS = {
    "search_trade_prices",
    "get_recipe",
    "get_outpost_info",
    "get_material_info",
    "get_dungeon_info",
    "get_blessing_info",
    "get_hero_build",
    "get_quest_info",
}

LIVE_PROBE_EXCLUDED_TOOLS = {
    # Static coverage proves these are registered. The live probe avoids
    # no-parameter actions that can move, zone, trade, or change bot state.
    "accept_trade",
    "cancel_action",
    "cancel_trade",
    "change_trade_offer",
    "enter_mission",
    "froggy_prepare_tekks_dungeon_entry",
    "froggy_refresh_combat_skillbar",
    "froggy_run_dungeon_loop",
    "froggy_run_full_maintenance",
    "froggy_run_maintenance_cycle",
    "froggy_run_sparkfly_route_to_tekks",
    "froggy_run_town_setup",
    "froggy_travel_to_gadds",
    "froggy_travel_to_sparkfly",
    "open_quest_log",
    "resign",
    "return_to_outpost",
    "salvage_done",
    "salvage_materials",
    "skip_cinematic",
    "submit_trade_offer",
    "unflag_all",
}


def all_tool_names() -> set[str]:
    return {tool["function"]["name"] for tool in ALL_TOOLS}


def cxx_dispatch_names() -> set[str]:
    names: set[str] = set()
    for source_path in ACTION_EXECUTOR_SOURCES:
        source = source_path.read_text(encoding="utf-8")
        names.update(re.findall(r'(?:g_dispatch|dispatch)\["([^"]+)"\]', source))
    return names


class ActionCoverageContractTests(unittest.TestCase):
    def test_all_tools_have_cxx_or_local_dispatch(self):
        missing = all_tool_names() - cxx_dispatch_names() - LOCAL_ONLY_TOOLS

        self.assertEqual(sorted(missing), [])

    def test_local_only_tools_are_in_schema(self):
        missing = LOCAL_ONLY_TOOLS - all_tool_names()

        self.assertEqual(sorted(missing), [])


class LiveActionCoverageTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        if os.environ.get("GWA3_LIVE_ACTION_COVERAGE") != "1":
            self.skipTest("set GWA3_LIVE_ACTION_COVERAGE=1 to run against an injected DLL")

        self.ipc = IpcClient(os.environ.get("GWA3_PIPE_NAME", DEFAULT_PIPE))
        if not await self.ipc.connect(timeout=20.0):
            self.fail("could not connect to the configured gwa3 pipe")

    async def asyncTearDown(self):
        if hasattr(self, "ipc"):
            self.ipc.disconnect()

    async def test_cxx_schema_tools_do_not_return_unknown_action(self):
        probe_names = sorted(all_tool_names() - LOCAL_ONLY_TOOLS - LIVE_PROBE_EXCLUDED_TOOLS)
        failures: list[str] = []

        for idx, name in enumerate(probe_names):
            request_id = f"coverage-{idx}-{name}"
            await self.ipc.send_action(name, {}, request_id)
            result = await self._wait_for_action_result(request_id, timeout=8.0)
            if result is None:
                failures.append(f"{name}: no action_result")
            elif result.get("error") == "unknown_action":
                failures.append(f"{name}: unknown_action")
            await asyncio.sleep(0.05)

        self.assertEqual(failures, [])

    async def _wait_for_action_result(self, request_id: str, timeout: float) -> dict | None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining = max(0.1, deadline - time.monotonic())
            msg = await asyncio.wait_for(self.ipc.read_message(), timeout=remaining)
            if msg is None:
                return None
            if msg.get("type") == "action_result" and msg.get("request_id") == request_id:
                return msg
        return None


if __name__ == "__main__":
    unittest.main()
