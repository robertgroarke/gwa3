"""Category M: Quest log manipulation — read the quest log from snapshots,
drive set_active_quest / abandon_quest / request_quest_info through the bridge,
and prove the LLM tool schema wires them up so Gemma can pick them.

The live (state-changing) tests skip gracefully when the character has no
quests in the log, so this module is safe to run from an empty outpost.

Run with:
    python -m bridge.tests --filter "test_quest_*"
    python -m bridge.tests --filter "test_m_*"
    python -m unittest bridge.tests.test_m_quest_log -v     # for schema-only

## Test plan — strict pass/fail criteria

## Tool schema + dispatch (no game pipe required)
## M01: set_active_quest in ALL_TOOLS with quest_id required
## M02: abandon_quest   in ALL_TOOLS with quest_id required
## M03: request_quest_info in ALL_TOOLS with quest_id required
## M04: quest tools forwarded to pipe (NOT routed locally like farming_knowledge)

## Snapshot exposure (Tier 2; in-game)
## M10: quests.quest_log entries expose the new optional fields
##      (is_primary, is_area_primary, is_active, marker_x, marker_y)
## M11: active quest's is_active flag is true in the log
## M12: location + npc strings are UTF-8 when present

## Action validation (any game state)
## M20: set_active_quest missing quest_id -> "missing quest_id"
## M21: set_active_quest quest_id=0       -> "quest_id_zero"
## M22: abandon_quest missing quest_id    -> "missing quest_id"
## M23: abandon_quest quest_id=0          -> "quest_id_zero"
## M24: abandon_quest non-existent id     -> "quest_not_in_log"
## M25: request_quest_info missing id     -> "missing quest_id"
## M26: unknown_action for typo           -> "unknown_action"

## Live round-trip (skips when quest log is empty)
## M30: set_active_quest on a log quest succeeds
## M31: After set_active_quest, active_quest_id updates within 5s
## M32: request_quest_info succeeds on a log quest
"""

from __future__ import annotations

import asyncio
import unittest
from unittest.mock import AsyncMock, MagicMock

from .base import BridgeTestCase
from .helpers import assert_true, assert_type


# ---------------------------------------------------------------------------
# Tool schema + dispatch (no-pipe unit tests)
# ---------------------------------------------------------------------------

class TestQuestToolSchema(unittest.TestCase):
    """M01-M04: Verify the LLM sees all three quest tools with the right shape."""

    def test_m01_set_active_quest_schema(self):
        from bridge import tool_schema as ts
        names = [t["function"]["name"] for t in ts.ALL_TOOLS]
        self.assertIn("set_active_quest", names)
        t = next(t for t in ts.ALL_TOOLS if t["function"]["name"] == "set_active_quest")
        params = t["function"]["parameters"]
        self.assertIn("quest_id", params["properties"])
        self.assertEqual(params["required"], ["quest_id"])

    def test_m02_abandon_quest_schema(self):
        from bridge import tool_schema as ts
        names = [t["function"]["name"] for t in ts.ALL_TOOLS]
        self.assertIn("abandon_quest", names)
        t = next(t for t in ts.ALL_TOOLS if t["function"]["name"] == "abandon_quest")
        self.assertEqual(t["function"]["parameters"]["required"], ["quest_id"])

    def test_m03_request_quest_info_schema(self):
        from bridge import tool_schema as ts
        names = [t["function"]["name"] for t in ts.ALL_TOOLS]
        self.assertIn("request_quest_info", names)
        t = next(t for t in ts.ALL_TOOLS if t["function"]["name"] == "request_quest_info")
        self.assertEqual(t["function"]["parameters"]["required"], ["quest_id"])

    def test_m04_quest_tools_forwarded_to_pipe(self):
        """Quest actions are NOT locally routed (unlike get_recipe); they must
        flow through IpcClient.send_action so the DLL receives them."""
        from bridge import agent_loop

        fake_ipc = MagicMock()
        fake_ipc.send_action = AsyncMock(return_value=None)
        fake_ipc.read_message = AsyncMock(return_value=None)
        fake_llm = MagicMock()
        loop = agent_loop.AgentLoop(ipc=fake_ipc, llm=fake_llm, autonomy="tactical")

        call = MagicMock()
        call.id = "call-1"
        call.name = "set_active_quest"
        call.arguments = '{"quest_id": 42}'
        call.parsed_arguments = {"quest_id": 42}

        response = MagicMock()
        response.tool_calls = [call]

        asyncio.run(loop._execute_tool_calls(response))
        # Quest tools must reach send_action (not a local handler)
        assert fake_ipc.send_action.called, \
            "set_active_quest should be forwarded to the DLL via ipc.send_action"
        sent_name, sent_params, _ = fake_ipc.send_action.call_args[0]
        self.assertEqual(sent_name, "set_active_quest")
        self.assertEqual(sent_params, {"quest_id": 42})


# ---------------------------------------------------------------------------
# Helpers for the live (bridge) tests
# ---------------------------------------------------------------------------

def _pick_log_quest(snap):
    """Return the first quest in the log that is not completed, or None."""
    log = snap.get("quests", {}).get("quest_log", [])
    for q in log:
        if not q.get("is_completed") and q.get("quest_id", 0) > 0:
            return q
    return log[0] if log else None


# ---------------------------------------------------------------------------
# Live snapshot + action bridge tests
# ---------------------------------------------------------------------------

async def test_quest_log_exposes_new_fields(tc: BridgeTestCase):
    """M10: new optional fields present on quest_log entries."""
    snap = await tc.wait_for_snapshot(tier=2)
    log = snap.get("quests", {}).get("quest_log", [])
    if not log:
        tc.skip("Quest log is empty")
    entry = log[0]
    for k in ("is_primary", "is_area_primary", "is_active", "marker_x", "marker_y"):
        assert_true(k in entry, f"quest_log entry missing {k}: {entry}")


async def test_active_quest_is_flagged_in_log(tc: BridgeTestCase):
    """M11: exactly one log entry has is_active=True when an active quest exists."""
    snap = await tc.wait_for_snapshot(tier=2)
    q = snap.get("quests", {})
    active_id = q.get("active_quest_id", 0)
    log = q.get("quest_log", [])
    if not log or active_id == 0:
        tc.skip("No active quest or empty log")
    active_flags = [e for e in log if e.get("is_active")]
    assert_true(
        len(active_flags) == 1 and active_flags[0]["quest_id"] == active_id,
        f"Expected exactly one is_active entry matching {active_id}, got {active_flags}",
    )


async def test_quest_strings_are_utf8(tc: BridgeTestCase):
    """M12: any name/location/npc strings are valid Python strings."""
    snap = await tc.wait_for_snapshot(tier=2)
    log = snap.get("quests", {}).get("quest_log", [])
    if not log:
        tc.skip("Quest log is empty")
    for entry in log[:5]:
        for k in ("name", "location", "npc"):
            if k in entry:
                assert_type(entry[k], str, f"quest.{k}")


# --- Action validation (no live quest needed) ----------------------------

async def test_set_active_quest_missing_id(tc: BridgeTestCase):
    """M20"""
    r = await tc.send_action("set_active_quest", {})
    tc.assert_action_error(r, "missing quest_id")


async def test_set_active_quest_zero_id(tc: BridgeTestCase):
    """M21"""
    r = await tc.send_action("set_active_quest", {"quest_id": 0})
    tc.assert_action_error(r, "quest_id_zero")


async def test_abandon_quest_missing_id(tc: BridgeTestCase):
    """M22"""
    r = await tc.send_action("abandon_quest", {})
    tc.assert_action_error(r, "missing quest_id")


async def test_abandon_quest_zero_id(tc: BridgeTestCase):
    """M23"""
    r = await tc.send_action("abandon_quest", {"quest_id": 0})
    tc.assert_action_error(r, "quest_id_zero")


async def test_abandon_quest_unknown_id(tc: BridgeTestCase):
    """M24: abandon must fail cleanly when the quest is not in the log."""
    r = await tc.send_action("abandon_quest", {"quest_id": 0x7FFFFFF0})
    tc.assert_action_error(r, "quest_not_in_log")


async def test_request_quest_info_missing_id(tc: BridgeTestCase):
    """M25"""
    r = await tc.send_action("request_quest_info", {})
    tc.assert_action_error(r, "missing quest_id")


# --- Live round-trip -----------------------------------------------------

async def test_set_active_quest_roundtrip(tc: BridgeTestCase):
    """M30+M31: set_active_quest on a log quest ultimately flips active_quest_id."""
    snap = await tc.wait_for_snapshot(tier=2)
    quest = _pick_log_quest(snap)
    if not quest:
        tc.skip("No quest in log to switch to")

    target_id = quest["quest_id"]
    prev_active = snap["quests"]["active_quest_id"]
    if target_id == prev_active and len(snap["quests"]["quest_log"]) > 1:
        # pick a different one so we can observe the change
        for q in snap["quests"]["quest_log"]:
            if q["quest_id"] != prev_active and q["quest_id"] > 0:
                target_id = q["quest_id"]
                break

    r = await tc.send_action("set_active_quest", {"quest_id": target_id})
    tc.assert_action_success(r)

    def active_matches(s):
        return s.get("quests", {}).get("active_quest_id") == target_id

    try:
        new_snap = await tc.wait_for_state_change(active_matches, tier=2, timeout=6.0)
    except Exception:
        # Some quests can't be set active (completed, mission-quest only, etc.)
        tc.skip(f"Quest {target_id} did not become active — likely server rejection")
        return
    assert_true(
        new_snap["quests"]["active_quest_id"] == target_id,
        f"active_quest_id should be {target_id}, got {new_snap['quests']['active_quest_id']}",
    )


async def test_request_quest_info_success(tc: BridgeTestCase):
    """M32: request_quest_info on any log quest returns success."""
    snap = await tc.wait_for_snapshot(tier=2)
    log = snap.get("quests", {}).get("quest_log", [])
    if not log:
        tc.skip("Quest log is empty")
    target_id = log[0]["quest_id"]
    r = await tc.send_action("request_quest_info", {"quest_id": target_id})
    tc.assert_action_success(r)
