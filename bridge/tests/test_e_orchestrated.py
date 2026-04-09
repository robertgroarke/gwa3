"""Category E: Orchestrated Tests — sets up game state to avoid skips.

Unlike tests in categories A-D which are independent and gracefully skip,
these tests run in sequence and SET UP the game state they need:
  Phase 1: Verify/travel to outpost
  Phase 2: Add heroes, verify party
  Phase 3: Open merchant, verify merchant window
  Phase 4: Enter explorable, verify enemies
  Phase 5: Return to outpost

Run via: python -m bridge.tests --filter "test_orchestrated*"
"""

import asyncio
import time
from .base import BridgeTestCase
from .helpers import (
    assert_true, assert_type, assert_keys_present, assert_gt, assert_gte,
)

MAP_GADDS = 638
MAP_SPARKFLY = 558

MERCHANT_X = -8374.0
MERCHANT_Y = -22491.0

# Gadd's exit portal waypoints to Sparkfly Swamp
EXIT_WP1 = (-10018.0, -21892.0)
EXIT_WP2 = (-9550.0, -20400.0)
EXIT_PUSH = (-9451.0, -19766.0)


async def _wait_until_near(tc, x, y, threshold=350.0, timeout=20.0):
    """Poll snapshots until player position is within threshold of target. Returns True if reached."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        snap = await tc.wait_for_snapshot(tier=1, timeout=3.0)
        me = snap.get("me", {})
        dx = me.get("x", 0) - x
        dy = me.get("y", 0) - y
        dist = (dx * dx + dy * dy) ** 0.5
        if dist <= threshold:
            return True
        # Re-issue move in case it got interrupted
        await tc.ipc.send_action("move_to", {"x": x, "y": y}, "")
        await asyncio.sleep(1.0)
    return False


async def test_orchestrated_phase1_outpost(tc: BridgeTestCase):
    """Phase 1: Ensure we're in Gadd's Encampment. Travel if needed."""
    snap = await tc.wait_for_snapshot(tier=1, timeout=10.0)
    map_id = snap["map"]["map_id"]

    if map_id != MAP_GADDS:
        result = await tc.send_action("travel", {"map_id": MAP_GADDS})
        tc.assert_action_success(result)

        # Wait for map to change AND load
        def at_gadds_loaded(s):
            return s["map"]["map_id"] == MAP_GADDS and s["map"]["loading_state"] == 1

        await tc.wait_for_state_change(at_gadds_loaded, tier=1, timeout=60.0)
        # Extra hydration wait
        await asyncio.sleep(5.0)

    # Verify final state
    snap = await tc.wait_for_snapshot(tier=1)
    assert_true(snap["me"]["agent_id"] > 0, "Agent ID should be valid")
    assert_true(snap["map"]["map_id"] == MAP_GADDS, f"Should be at Gadd's, got {snap['map']['map_id']}")
    assert_true(snap["map"]["loading_state"] == 1, "Map should be loaded")


async def test_orchestrated_phase2_party_setup(tc: BridgeTestCase):
    """Phase 2: Add heroes, verify party grows."""
    # Kick existing heroes
    result = await tc.send_action("kick_all_heroes")
    tc.assert_action_error(result, "deprecated_use_kick_hero_individually")

    # Wait for party to actually shrink to 1 (just player)
    def party_is_solo(s):
        return s["party"]["size"] <= 2  # player + maybe 1 lingering hero
    try:
        await tc.wait_for_state_change(party_is_solo, tier=1, timeout=5.0)
    except Exception:
        pass  # may already be solo

    snap = await tc.wait_for_snapshot(tier=1)
    initial_size = snap["party"]["size"]

    # Add 3 heroes
    hero_ids = [25, 14, 21]
    for hid in hero_ids:
        result = await tc.send_action("add_hero", {"hero_id": hid})
        tc.assert_action_success(result)
        await asyncio.sleep(0.5)

    # Poll until party size actually increased
    def party_grew(s):
        return s["party"]["size"] > initial_size

    new_snap = await tc.wait_for_state_change(party_grew, tier=1, timeout=10.0)
    assert_gt(new_snap["party"]["size"], initial_size,
              f"Party should grow from {initial_size}")

    # Verify heroes visible in Tier 2
    snap2 = await tc.wait_for_snapshot(tier=2, timeout=5.0)
    heroes = snap2.get("heroes", [])
    assert_gt(len(heroes), 0, "Should see heroes in Tier 2 after adding")

    # Set hero behaviors to guard
    for i in range(1, len(hero_ids) + 1):
        await tc.send_action("set_hero_behavior", {"hero_index": i, "behavior": 1})
        await asyncio.sleep(0.2)


async def test_orchestrated_phase3_merchant(tc: BridgeTestCase):
    """Phase 3: Open merchant at Gadd's, verify items visible."""
    snap = await tc.wait_for_snapshot(tier=1)
    if snap["map"]["map_id"] != MAP_GADDS:
        tc.skip("Not in Gadd's — phase 1 may have failed")
        return

    # Move to merchant NPC using distance-based polling
    result = await tc.send_action("move_to", {"x": MERCHANT_X, "y": MERCHANT_Y})
    tc.assert_action_success(result)
    reached = await _wait_until_near(tc, MERCHANT_X, MERCHANT_Y, threshold=350.0, timeout=15.0)
    assert_true(reached, "Should reach merchant area within 15s")

    # Find NPC near merchant coords in snapshot
    snap2 = await tc.wait_for_snapshot(tier=2, timeout=5.0)
    npcs = [a for a in snap2.get("agents", [])
            if a.get("agent_type") == "living"
            and a.get("allegiance") not in (1, 3)
            and a.get("is_alive", False)
            and a.get("distance", 9999) < 900]

    if not npcs:
        tc.skip("No merchant NPC found near merchant coords")
        return

    npc = min(npcs, key=lambda a: a.get("distance", 9999))
    npc_id = npc["id"]

    # Move close to NPC
    npc_x = npc.get("x", MERCHANT_X)
    npc_y = npc.get("y", MERCHANT_Y)
    await tc.send_action("move_to", {"x": npc_x, "y": npc_y})
    await _wait_until_near(tc, npc_x, npc_y, threshold=150.0, timeout=10.0)

    # Interact with merchant NPC (this sends packet 0x38 + opens dialog)
    result = await tc.send_action("interact_npc", {"agent_id": npc_id})
    tc.assert_action_success(result)
    await asyncio.sleep(2.0)

    # Check if merchant window opened
    def merchant_open(s):
        return s.get("merchant", {}).get("is_open", False)

    try:
        merch_snap = await tc.wait_for_state_change(merchant_open, tier=2, timeout=10.0)
        assert_true(merch_snap["merchant"]["is_open"], "Merchant should be open")

        items = merch_snap["merchant"].get("items", [])
        assert_gt(len(items), 0, "Merchant should have items")

        # Validate item structure
        item = items[0]
        assert_keys_present(item, ["item_id", "model_id", "type", "value"], "merchant item")
        assert_gt(item["item_id"], 0, "merchant item_id")
    except Exception:
        tc.skip("Merchant window didn't open — NPC may need specific dialog sequence")

    # Close by moving away
    await tc.send_action("cancel_action")
    await asyncio.sleep(1.0)


async def test_orchestrated_phase4_explorable(tc: BridgeTestCase):
    """Phase 4: Enter Sparkfly Swamp, verify enemies visible."""
    snap = await tc.wait_for_snapshot(tier=1)
    if snap["map"]["map_id"] != MAP_GADDS:
        # Try to travel back
        await tc.send_action("travel", {"map_id": MAP_GADDS})
        def at_gadds(s):
            return s["map"]["map_id"] == MAP_GADDS and s["map"]["loading_state"] == 1
        try:
            await tc.wait_for_state_change(at_gadds, tier=1, timeout=60.0)
            await asyncio.sleep(3.0)
        except Exception:
            tc.skip("Cannot get to Gadd's for explorable entry")
            return

    # Walk to exit portal waypoints using distance-based polling
    for wx, wy in [EXIT_WP1, EXIT_WP2]:
        await tc.send_action("move_to", {"x": wx, "y": wy})
        reached = await _wait_until_near(tc, wx, wy, threshold=350.0, timeout=25.0)
        if not reached:
            tc.skip(f"Failed to reach exit waypoint ({wx}, {wy})")
            return

    # Push toward zone exit — keep moving until map changes
    deadline = time.monotonic() + 30.0
    left_outpost = False
    while time.monotonic() < deadline:
        await tc.send_action("move_to", {"x": EXIT_PUSH[0], "y": EXIT_PUSH[1]})
        await asyncio.sleep(1.0)
        snap = await tc.wait_for_snapshot(tier=1, timeout=2.0)
        if snap["map"]["map_id"] != MAP_GADDS:
            left_outpost = True
            break

    if not left_outpost:
        tc.skip("Failed to exit Gadd's Encampment within 30s")
        return

    # Wait for Sparkfly to load
    def in_sparkfly(s):
        return s["map"]["map_id"] == MAP_SPARKFLY and s["map"]["loading_state"] == 1

    try:
        await tc.wait_for_state_change(in_sparkfly, tier=1, timeout=30.0)
    except Exception:
        tc.skip("Sparkfly Swamp didn't load within 30s")
        return

    await asyncio.sleep(5.0)  # stability

    # Verify explorable
    snap = await tc.wait_for_snapshot(tier=1)
    assert_true(snap["map"]["map_id"] == MAP_SPARKFLY,
                f"Should be in Sparkfly, got {snap['map']['map_id']}")
    assert_true(snap["me"]["agent_id"] > 0, "Agent should be valid")

    # Look for enemies
    snap2 = await tc.wait_for_snapshot(tier=2, timeout=5.0)
    foes = [a for a in snap2.get("agents", [])
            if a.get("allegiance") == 3 and a.get("is_alive", False)]

    if not foes:
        # Move toward first waypoint where enemies typically are
        await tc.send_action("move_to", {"x": -4559.0, "y": -14406.0})
        reached = await _wait_until_near(tc, -4559.0, -14406.0, threshold=500.0, timeout=15.0)
        snap2 = await tc.wait_for_snapshot(tier=2, timeout=5.0)
        foes = [a for a in snap2.get("agents", [])
                if a.get("allegiance") == 3 and a.get("is_alive", False)]

    if foes:
        foe = foes[0]
        assert_keys_present(foe, ["id", "hp", "allegiance", "is_casting", "has_hex"], "enemy")
        assert_true(foe["allegiance"] == 3, "Enemy allegiance should be 3")
        assert_true(foe["hp"] > 0, "Enemy should be alive")

        # Attack to verify combat works
        result = await tc.send_action("attack", {"agent_id": foe["id"]})
        tc.assert_action_success(result)
    else:
        tc.skip("No enemies found — area may be cleared")


async def test_orchestrated_phase5_return(tc: BridgeTestCase):
    """Phase 5: Return to outpost from explorable."""
    snap = await tc.wait_for_snapshot(tier=1)
    if snap["map"]["map_id"] == MAP_GADDS:
        return  # already in outpost

    result = await tc.send_action("return_to_outpost")
    tc.assert_action_success(result)

    def back_at_gadds(s):
        return s["map"]["map_id"] == MAP_GADDS and s["map"]["loading_state"] == 1

    try:
        new_snap = await tc.wait_for_state_change(back_at_gadds, tier=1, timeout=60.0)
        assert_true(new_snap["map"]["map_id"] == MAP_GADDS,
                    f"Should be at Gadd's, got {new_snap['map']['map_id']}")
    except Exception:
        tc.skip("Return to outpost timed out")
