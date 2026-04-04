"""Category A: IPC Protocol Tests — pipe connection, framing, heartbeat, rate limiting."""

from .base import BridgeTestCase
from .helpers import assert_true, assert_keys_present, assert_type, assert_gt


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
    assert_gt(len(rate_limited), 0,
              f"Expected at least one rate_limited from 15 rapid actions, got 0 "
              f"({len(successes)} successes)")
    assert_gt(len(successes), 0,
              f"Expected at least one success from 15 actions, got 0 "
              f"({len(rate_limited)} rate_limited)")
    # Total should account for all results
    assert_true(
        len(successes) + len(rate_limited) == len(results),
        f"All results should be success or rate_limited, got "
        f"{len(successes)} success + {len(rate_limited)} rate_limited out of {len(results)}",
    )
