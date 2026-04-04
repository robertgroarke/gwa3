"""Category D: Validation & Error Handling Tests — rate limiting, unknown actions, bad params."""

import asyncio
from .base import BridgeTestCase
from .helpers import assert_true


async def test_rate_limiter(tc: BridgeTestCase):
    """Send 15 actions in rapid succession — at least one should be rate-limited."""
    results = []
    for _ in range(15):
        result = await tc.send_action("cancel_action", {}, timeout=2.0)
        results.append(result)
    rate_limited = [r for r in results if r.get("error") == "rate_limited"]
    assert_true(
        len(rate_limited) > 0,
        f"Expected at least one rate_limited error from 15 rapid actions, got 0",
    )


async def test_unknown_action(tc: BridgeTestCase):
    """Unknown action name should return 'unknown_action' error."""
    result = await tc.send_action("nonexistent_action_xyz", {})
    tc.assert_action_error(result, "unknown_action")


async def test_empty_action_name(tc: BridgeTestCase):
    """Empty action name should return 'empty_action_name' error."""
    # Send raw message with empty name to bypass send_action's name param
    msg = {"type": "action", "name": "", "params": {}, "request_id": "test_empty"}
    await tc.ipc.send_message(msg)
    # Read until we get the result
    result = await tc.wait_for_message_type("action_result", timeout=3.0)
    tc.assert_action_error(result, "empty_action_name")


async def test_agent_not_found(tc: BridgeTestCase):
    """Targeting a nonexistent agent should return 'agent_not_found'."""
    result = await tc.send_action("change_target", {"agent_id": 99999999})
    tc.assert_action_error(result, "agent_not_found")


async def test_invalid_slot(tc: BridgeTestCase):
    """Skill slot 8 (out of range 0-7) should return 'invalid_slot'."""
    result = await tc.send_action("use_skill", {"slot": 8})
    tc.assert_action_error(result, "invalid_slot")


async def test_coordinates_out_of_range(tc: BridgeTestCase):
    """Extreme coordinates should return 'coordinates_out_of_range'."""
    result = await tc.send_action("move_to", {"x": 999999, "y": 999999})
    tc.assert_action_error(result, "coordinates_out_of_range")


async def test_invalid_behavior(tc: BridgeTestCase):
    """Hero behavior 5 (out of range 0-2) should return 'invalid_behavior'."""
    result = await tc.send_action("set_hero_behavior", {"hero_index": 1, "behavior": 5})
    tc.assert_action_error(result, "invalid_behavior")
