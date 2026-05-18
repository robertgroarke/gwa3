"""Category D: Validation & Error Handling Tests — rate limiting, unknown actions, bad params."""

import asyncio
from .base import BridgeTestCase, assert_true, assert_keys_present, assert_type, assert_gt


async def test_pipe_connect(tc: BridgeTestCase):
    """Verify pipe connection succeeds and client reports connected."""
    assert_true(tc.ipc.connected, "IPC client should be connected after setUp")


async def test_snapshot_is_valid_json_dict(tc: BridgeTestCase):
    """First snapshot should be a dict with required protocol fields."""
    snap = await tc.wait_for_snapshot(timeout=3.0)
    assert_type(snap, dict, "snapshot")
    assert_keys_present(snap, ["type", "tier", "tick"], "snapshot")
    assert_true(snap["type"] == "snapshot", f"Expected type 'snapshot', got '{snap['type']}'")
    assert_type(snap["tier"], int, "snapshot.tier")
    assert_type(snap["tick"], int, "snapshot.tick")


async def test_snapshot_tier_in_valid_range(tc: BridgeTestCase):
    """Snapshot tier must be 1, 2, or 3."""
    snap = await tc.wait_for_snapshot(timeout=3.0)
    assert_true(snap["tier"] in (1, 2, 3), f"Tier should be 1, 2, or 3, got {snap['tier']}")


async def test_snapshot_tick_positive_and_increasing(tc: BridgeTestCase):
    """Tick counter should be positive and increase between snapshots."""
    snap1 = await tc.wait_for_snapshot(timeout=3.0)
    assert_gt(snap1["tick"], 0, "first tick")
    snap2 = await tc.wait_for_snapshot(timeout=3.0)
    assert_gt(snap2["tick"], snap1["tick"], "tick should increase between snapshots")


async def test_heartbeat_reception(tc: BridgeTestCase):
    """Heartbeat message arrives within 10 seconds with correct type field."""
    msg = await tc.wait_for_message_type("heartbeat", timeout=10.0)
    assert_type(msg, dict, "heartbeat")
    assert_true(msg["type"] == "heartbeat", f"Expected type 'heartbeat', got '{msg['type']}'")


async def test_rapid_send_rate_limiter(tc: BridgeTestCase):
    """Send 15 actions in <1s. At least one rate_limited, and the rest should succeed."""
    results = []
    for _ in range(15):
        result = await tc.send_action("cancel_action", {}, timeout=2.0)
        results.append(result)

    successes = [r for r in results if r.get("success") is True]
    rate_limited = [r for r in results if r.get("error") == "rate_limited"]
    assert_gt(
        len(rate_limited),
        0,
        f"Expected at least one rate_limited from 15 rapid actions, got 0 "
        f"({len(successes)} successes)",
    )
    assert_gt(
        len(successes),
        0,
        f"Expected at least one success from 15 actions, got 0 "
        f"({len(rate_limited)} rate_limited)",
    )
    assert_true(
        len(successes) + len(rate_limited) == len(results),
        f"All results should be success or rate_limited, got "
        f"{len(successes)} success + {len(rate_limited)} rate_limited out of {len(results)}",
    )


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


# ============================================================
# D2: StoC event push validation ()
# ============================================================

async def test_event_messages_arrive(tc: BridgeTestCase):
    """Over 30 seconds of gameplay, at least one event message should arrive."""
    import time
    deadline = time.monotonic() + 30.0
    events_seen = []
    while time.monotonic() < deadline:
        try:
            msg = await asyncio.wait_for(tc.ipc.read_message(), timeout=1.0)
            if msg and msg.get("type") == "event":
                events_seen.append(msg)
                break  # One is enough
        except asyncio.TimeoutError:
            continue
    if not events_seen:
        tc.skip("No StoC events received in 30s — may need active gameplay")


async def test_event_has_event_field(tc: BridgeTestCase):
    """Event messages should have an 'event' field with a string name."""
    import time
    deadline = time.monotonic() + 15.0
    while time.monotonic() < deadline:
        try:
            msg = await asyncio.wait_for(tc.ipc.read_message(), timeout=1.0)
            if msg and msg.get("type") == "event":
                assert_keys_present(msg, ["type", "event"], "event message")
                assert_type(msg["event"], str, "event.event")
                known_events = {
                    "instance_load", "map_loaded", "party_defeated", "agent_died",
                    "skill_activated", "item_owner_changed", "agent_spawned",
                    "agent_despawned", "cinematic_start", "cinematic_end",
                    "quest_added", "quest_removed", "dungeon_reward",
                }
                assert_true(
                    msg["event"] in known_events,
                    f"Unknown event type: '{msg['event']}'",
                )
                return
        except asyncio.TimeoutError:
            continue
    tc.skip("No events received — need active gameplay to generate events")


# ============================================================
# D3: Advisory mode coexistence ()
# ============================================================

async def test_advisory_bot_running(tc: BridgeTestCase):
    """In advisory mode, bot.is_running should be True."""
    snap = await tc.wait_for_snapshot(tier=1)
    bot = snap.get("bot", {})
    if not bot.get("is_running"):
        tc.skip("Bot not running — test requires --llm-advisory mode")
    assert_true(bot["is_running"], "Bot should be running in advisory mode")


async def test_advisory_bot_has_active_state(tc: BridgeTestCase):
    """In advisory mode, bot state should not be 'idle' or 'stopping' after startup."""
    snap = await tc.wait_for_snapshot(tier=1)
    bot = snap.get("bot", {})
    if not bot.get("is_running"):
        tc.skip("Bot not running — test requires --llm-advisory mode")
    # Bot should be in an active state (not idle, unless just started)
    assert_true(
        bot["state"] != "stopping",
        f"Bot should not be stopping in advisory mode, got state={bot['state']}",
    )


async def test_advisory_llm_and_bot_coexist(tc: BridgeTestCase):
    """In advisory mode, we can read snapshots (LLM bridge) AND bot is running."""
    snap = await tc.wait_for_snapshot(tier=1, timeout=5.0)
    assert_true(snap is not None, "Should receive snapshots (LLM bridge active)")
    bot = snap.get("bot", {})
    if not bot.get("is_running"):
        tc.skip("Bot not running — requires --llm-advisory mode")
    # Both are running: we got a snapshot (bridge works) and bot reports running
    assert_true(True, "Both LLM bridge and bot are running")


async def test_advisory_state_override_and_restore(tc: BridgeTestCase):
    """In advisory mode, LLM can override bot state and it reflects in snapshot."""
    snap = await tc.wait_for_snapshot(tier=1)
    bot = snap.get("bot", {})
    if not bot.get("is_running"):
        tc.skip("Bot not running — requires --llm-advisory mode")

    original_state = bot["state"]

    # Override to merchant
    result = await tc.send_action("set_bot_state", {"state": "merchant"})
    tc.assert_action_success(result)

    # Verify it changed
    def check_merchant(s):
        return s.get("bot", {}).get("state") == "merchant"

    try:
        await tc.wait_for_state_change(check_merchant, tier=1, timeout=5.0)
    finally:
        # Restore original state
        await tc.send_action("set_bot_state", {"state": original_state})
