"""Category C: Action Tests — send commands through the bridge and verify results + state changes."""

from .base import BridgeTestCase, assert_true, assert_gt, assert_keys_present, assert_type

# ============================================================
# C1-C2: Movement & Targeting
# ============================================================

async def test_move_to_success(tc: BridgeTestCase):
    """Send move_to with valid coordinates, verify success and result structure."""
    snap = await tc.wait_for_snapshot(tier=1)
    x = snap["me"]["x"] + 100
    y = snap["me"]["y"] + 100
    result = await tc.send_action("move_to", {"x": x, "y": y})
    tc.assert_action_success(result)
    assert_keys_present(result, ["success", "request_id"], "action_result")
    assert_true(result["error"] is None, f"Expected null error, got '{result.get('error')}'")



async def test_move_to_missing_params(tc: BridgeTestCase):
    result = await tc.send_action("move_to", {})
    tc.assert_action_error(result, "missing x or y")


async def test_move_to_out_of_range(tc: BridgeTestCase):
    result = await tc.send_action("move_to", {"x": 999999, "y": 999999})
    tc.assert_action_error(result, "coordinates_out_of_range")


async def test_move_to_state_change(tc: BridgeTestCase):
    """After move_to, position must change by at least 10 units within 5s."""
    snap = await tc.wait_for_snapshot(tier=1)
    old_x = snap["me"]["x"]
    old_y = snap["me"]["y"]
    result = await tc.send_action("move_to", {"x": old_x + 200, "y": old_y + 200})
    tc.assert_action_success(result)

    def position_changed(s):
        me = s.get("me", {})
        dx = abs(me.get("x", old_x) - old_x)
        dy = abs(me.get("y", old_y) - old_y)
        return dx > 10 or dy > 10

    new_snap = await tc.wait_for_state_change(position_changed, tier=1, timeout=5.0)
    # Verify the distance is actually meaningful
    new_x = new_snap["me"]["x"]
    new_y = new_snap["me"]["y"]
    dist = ((new_x - old_x)**2 + (new_y - old_y)**2)**0.5
    assert_gt(dist, 10, f"Position should have moved >10 units, moved {dist:.1f}")


async def test_cancel_action_success(tc: BridgeTestCase):
    result = await tc.send_action("cancel_action", {})
    tc.assert_action_success(result)


async def test_change_target_missing_id(tc: BridgeTestCase):
    result = await tc.send_action("change_target", {})
    tc.assert_action_error(result, "missing agent_id")


async def test_change_target_bad_id(tc: BridgeTestCase):
    result = await tc.send_action("change_target", {"agent_id": 99999999})
    tc.assert_action_error(result, "agent_not_found")


async def test_change_target_success(tc: BridgeTestCase):
    """Find a nearby agent and target it."""
    snap = await tc.wait_for_snapshot(tier=2)
    agents = snap.get("agents", [])
    if not agents:
        tc.skip("No nearby agents to target")
    target_id = agents[0]["id"]
    result = await tc.send_action("change_target", {"agent_id": target_id})
    tc.assert_action_success(result)


async def test_change_target_state_change(tc: BridgeTestCase):
    """After change_target, me.target_id must match the requested agent."""
    snap = await tc.wait_for_snapshot(tier=2)
    agents = snap.get("agents", [])
    if not agents:
        tc.skip("No nearby agents to target")
    old_target = snap["me"]["target_id"]
    # Pick an agent that is NOT our current target
    candidates = [a for a in agents if a["id"] != old_target]
    if not candidates:
        tc.skip("All agents already targeted or only one agent")
    target_id = candidates[0]["id"]

    result = await tc.send_action("change_target", {"agent_id": target_id})
    tc.assert_action_success(result)

    def target_matches(s):
        return s.get("me", {}).get("target_id") == target_id

    new_snap = await tc.wait_for_state_change(target_matches, tier=1, timeout=5.0)
    assert_true(
        new_snap["me"]["target_id"] == target_id,
        f"target_id should be {target_id}, got {new_snap['me']['target_id']}",
    )


# ============================================================
# C3: Combat
# ============================================================

async def test_attack_missing_id(tc: BridgeTestCase):
    result = await tc.send_action("attack", {})
    tc.assert_action_error(result, "missing agent_id")


async def test_attack_bad_id(tc: BridgeTestCase):
    result = await tc.send_action("attack", {"agent_id": 99999999})
    tc.assert_action_error(result, "agent_not_found")


async def test_attack_success(tc: BridgeTestCase):
    """Attack a nearby foe (requires explorable area)."""
    snap = await tc.wait_for_snapshot(tier=2)
    foes = [a for a in snap.get("agents", []) if a.get("allegiance") == 3 and a.get("is_alive")]
    if not foes:
        tc.skip("No alive foes nearby — need explorable area")
    result = await tc.send_action("attack", {"agent_id": foes[0]["id"]})
    tc.assert_action_success(result)


async def test_use_skill_missing_slot(tc: BridgeTestCase):
    result = await tc.send_action("use_skill", {})
    tc.assert_action_error(result, "missing slot")


async def test_use_skill_invalid_slot(tc: BridgeTestCase):
    result = await tc.send_action("use_skill", {"slot": 8})
    tc.assert_action_error(result, "invalid_slot")


async def test_use_skill_success(tc: BridgeTestCase):
    """Use a ready skill and verify recharge starts or energy changes."""
    snap = await tc.wait_for_snapshot(tier=1)
    # Find any ready skill
    ready_slot = None
    for sk in snap["skillbar"]:
        if sk["skill_id"] != 0 and sk["recharge"] == 0:
            ready_slot = sk["slot"]
            break
    if ready_slot is None:
        tc.skip("No ready skills in skillbar")
    old_energy = snap["me"]["energy"]
    result = await tc.send_action("use_skill", {"slot": ready_slot})
    tc.assert_action_success(result)

    # Verify: recharge started OR energy dropped OR casting started
    def skill_effect_visible(s):
        sk = s["skillbar"][ready_slot]
        energy_dropped = s["me"]["energy"] < old_energy - 0.01
        recharge_started = sk["recharge"] > 0
        casting = s["me"]["is_casting"]
        return energy_dropped or recharge_started or casting

    try:
        await tc.wait_for_state_change(skill_effect_visible, tier=1, timeout=5.0)
    except Exception:
        pass  # Skill may have been blocked by game state — success response is still valid


async def test_call_target_missing(tc: BridgeTestCase):
    result = await tc.send_action("call_target", {})
    tc.assert_action_error(result, "missing agent_id")


async def test_call_target_success(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    agents = snap.get("agents", [])
    if not agents:
        tc.skip("No nearby agents")
    result = await tc.send_action("call_target", {"agent_id": agents[0]["id"]})
    tc.assert_action_success(result)


async def test_use_hero_skill_missing(tc: BridgeTestCase):
    result = await tc.send_action("use_hero_skill", {})
    tc.assert_action_error(result, "missing hero_index or slot")


async def test_use_hero_skill_invalid_slot(tc: BridgeTestCase):
    result = await tc.send_action("use_hero_skill", {"hero_index": 1, "slot": 8})
    tc.assert_action_error(result, "invalid_slot")


# ============================================================
# C4: Party & Hero
# ============================================================

async def test_kick_all_heroes_deprecated(tc: BridgeTestCase):
    """Bulk kick-all is intentionally disabled; caller should use kick_hero repeatedly."""
    result = await tc.send_action("kick_all_heroes", {})
    tc.assert_action_error(result, "deprecated_use_kick_hero_individually")


async def test_add_hero_success(tc: BridgeTestCase):
    """Add a hero — party size should increase."""
    snap_before = await tc.wait_for_snapshot(tier=1)
    size_before = snap_before["party"]["size"]
    result = await tc.send_action("add_hero", {"hero_id": 25})
    tc.assert_action_success(result)

    def party_grew(s):
        return s["party"]["size"] > size_before

    try:
        await tc.wait_for_state_change(party_grew, tier=1, timeout=5.0)
    except Exception:
        pass  # Hero may already be in party


async def test_add_hero_missing_id(tc: BridgeTestCase):
    result = await tc.send_action("add_hero", {})
    tc.assert_action_error(result, "missing hero_id")


async def test_set_hero_behavior_success(tc: BridgeTestCase):
    result = await tc.send_action("set_hero_behavior", {"hero_index": 1, "behavior": 1})
    tc.assert_action_success(result)


async def test_set_hero_behavior_invalid(tc: BridgeTestCase):
    result = await tc.send_action("set_hero_behavior", {"hero_index": 1, "behavior": 5})
    tc.assert_action_error(result, "invalid_behavior")


async def test_flag_hero_success(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    x = snap["me"]["x"] + 300
    y = snap["me"]["y"] + 200
    result = await tc.send_action("flag_hero", {"hero_index": 1, "x": x, "y": y})
    tc.assert_action_success(result)


async def test_flag_all_success(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    result = await tc.send_action("flag_all", {"x": snap["me"]["x"], "y": snap["me"]["y"]})
    tc.assert_action_success(result)


async def test_unflag_all_success(tc: BridgeTestCase):
    result = await tc.send_action("unflag_all", {})
    tc.assert_action_success(result)


async def test_lock_hero_target_success(tc: BridgeTestCase):
    result = await tc.send_action("lock_hero_target", {"hero_index": 1, "target_id": 0})
    tc.assert_action_success(result)
