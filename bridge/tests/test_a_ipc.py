"""Category A: IPC Protocol Tests — pipe connection, framing, heartbeat, rate limiting."""

from .base import BridgeTestCase
from .helpers import assert_true, assert_keys_present, assert_type


async def test_pipe_connect(tc: BridgeTestCase):
    """Verify pipe connection succeeds."""
    assert_true(tc.ipc.connected, "IPC client should be connected after setUp")


async def test_snapshot_reception(tc: BridgeTestCase):
    """Verify at least one snapshot arrives within 3 seconds."""
    snap = await tc.wait_for_snapshot(timeout=3.0)
    assert_true(snap is not None, "Should receive a snapshot")
    assert_keys_present(snap, ["type", "tier", "tick"], "snapshot")
    assert_true(snap["type"] == "snapshot", f"Expected type 'snapshot', got '{snap['type']}'")


async def test_snapshot_has_tier(tc: BridgeTestCase):
    """Verify snapshots have a tier field in {1, 2, 3}."""
    snap = await tc.wait_for_snapshot(timeout=3.0)
    assert_true(snap["tier"] in (1, 2, 3), f"Tier should be 1, 2, or 3, got {snap['tier']}")


async def test_snapshot_tick_positive(tc: BridgeTestCase):
    """Verify snapshot tick counter is positive."""
    snap = await tc.wait_for_snapshot(timeout=3.0)
    assert_true(snap["tick"] > 0, f"Tick should be > 0, got {snap['tick']}")


async def test_heartbeat_reception(tc: BridgeTestCase):
    """Verify heartbeat message arrives within 10 seconds."""
    msg = await tc.wait_for_message_type("heartbeat", timeout=10.0)
    assert_true(msg["type"] == "heartbeat", "Should receive a heartbeat")


async def test_rapid_send_rate_limiter(tc: BridgeTestCase):
    """Send 15 actions in <1s. At least one should return 'rate_limited'."""
    results = []
    for i in range(15):
        result = await tc.send_action("cancel_action", {}, timeout=2.0)
        results.append(result)

    rate_limited = [r for r in results if r.get("error") == "rate_limited"]
    assert_true(
        len(rate_limited) > 0,
        f"Expected at least one rate_limited error from 15 rapid actions, got {len(rate_limited)}",
    )
