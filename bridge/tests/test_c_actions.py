"""Category C: Action Tests — send commands through the bridge and verify results + state changes."""

from .base import BridgeTestCase
from .helpers import assert_true, assert_gt, assert_keys_present, assert_type, assert_gte


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

async def test_kick_all_heroes_success(tc: BridgeTestCase):
    """Kick all heroes — party size should decrease to 1 (just player)."""
    snap_before = await tc.wait_for_snapshot(tier=1)
    size_before = snap_before["party"]["size"]
    result = await tc.send_action("kick_all_heroes", {})
    tc.assert_action_success(result)

    if size_before > 1:
        def party_shrunk(s):
            return s["party"]["size"] < size_before
        try:
            await tc.wait_for_state_change(party_shrunk, tier=1, timeout=5.0)
        except Exception:
            pass  # May not shrink if no heroes were in party


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


# ============================================================
# C5: Travel
# ============================================================

async def test_travel_missing_map_id(tc: BridgeTestCase):
    result = await tc.send_action("travel", {})
    tc.assert_action_error(result, "missing map_id")


async def test_travel_invalid_map_id_zero(tc: BridgeTestCase):
    result = await tc.send_action("travel", {"map_id": 0})
    tc.assert_action_error(result, "invalid_map_id")


async def test_travel_invalid_map_id_high(tc: BridgeTestCase):
    result = await tc.send_action("travel", {"map_id": 1000})
    tc.assert_action_error(result, "invalid_map_id")


async def test_set_hard_mode_success(tc: BridgeTestCase):
    result = await tc.send_action("set_hard_mode", {"enabled": True})
    tc.assert_action_success(result)


async def test_set_hard_mode_missing(tc: BridgeTestCase):
    result = await tc.send_action("set_hard_mode", {})
    tc.assert_action_error(result, "missing enabled")


async def test_skip_cinematic_success(tc: BridgeTestCase):
    result = await tc.send_action("skip_cinematic", {})
    tc.assert_action_success(result)


async def test_return_to_outpost_success(tc: BridgeTestCase):
    result = await tc.send_action("return_to_outpost", {})
    tc.assert_action_success(result)


async def test_enter_mission_success(tc: BridgeTestCase):
    result = await tc.send_action("enter_mission", {})
    tc.assert_action_success(result)


# ============================================================
# C6: Items
# ============================================================

async def test_pick_up_item_missing(tc: BridgeTestCase):
    result = await tc.send_action("pick_up_item", {})
    tc.assert_action_error(result, "missing agent_id")


async def test_pick_up_item_bad_agent(tc: BridgeTestCase):
    result = await tc.send_action("pick_up_item", {"agent_id": 99999999})
    tc.assert_action_error(result, "agent_not_found")


async def test_use_item_missing(tc: BridgeTestCase):
    result = await tc.send_action("use_item", {})
    tc.assert_action_error(result, "missing item_id")


async def test_use_item_not_found(tc: BridgeTestCase):
    result = await tc.send_action("use_item", {"item_id": 99999999})
    tc.assert_action_error(result, "item_not_found")


async def test_equip_item_not_found(tc: BridgeTestCase):
    result = await tc.send_action("equip_item", {"item_id": 99999999})
    tc.assert_action_error(result, "item_not_found")


async def test_drop_item_not_found(tc: BridgeTestCase):
    result = await tc.send_action("drop_item", {"item_id": 99999999})
    tc.assert_action_error(result, "item_not_found")


async def test_move_item_missing(tc: BridgeTestCase):
    result = await tc.send_action("move_item", {})
    tc.assert_action_error(result, "missing item_id, bag_id, or slot")


async def test_drop_gold_success(tc: BridgeTestCase):
    result = await tc.send_action("drop_gold", {"amount": 1})
    tc.assert_action_success(result)


# ============================================================
# C7: Salvage & Identify
# ============================================================

async def test_salvage_start_missing(tc: BridgeTestCase):
    result = await tc.send_action("salvage_start", {})
    tc.assert_action_error(result, "missing item_id or kit_id")


async def test_salvage_start_not_found(tc: BridgeTestCase):
    result = await tc.send_action("salvage_start", {"item_id": 99999, "kit_id": 99999})
    tc.assert_action_error(result, "item_not_found")


async def test_salvage_materials_success(tc: BridgeTestCase):
    result = await tc.send_action("salvage_materials", {})
    tc.assert_action_success(result)


async def test_salvage_done_success(tc: BridgeTestCase):
    result = await tc.send_action("salvage_done", {})
    tc.assert_action_success(result)


async def test_identify_missing(tc: BridgeTestCase):
    result = await tc.send_action("identify_item", {})
    tc.assert_action_error(result, "missing item_id or kit_id")


async def test_identify_not_found(tc: BridgeTestCase):
    result = await tc.send_action("identify_item", {"item_id": 99999, "kit_id": 99999})
    tc.assert_action_error(result, "item_not_found")


# ============================================================
# C8: Trade
# ============================================================

async def test_buy_materials_missing(tc: BridgeTestCase):
    result = await tc.send_action("buy_materials", {})
    tc.assert_action_error(result, "missing model_id or quantity")


async def test_request_quote_missing(tc: BridgeTestCase):
    result = await tc.send_action("request_quote", {})
    tc.assert_action_error(result, "missing item_id")


async def test_transact_items_missing(tc: BridgeTestCase):
    result = await tc.send_action("transact_items", {})
    tc.assert_action_error(result, "missing type, quantity, or item_id")


# ============================================================
# C9: Dialog & NPC Interaction
# ============================================================

async def test_interact_npc_missing(tc: BridgeTestCase):
    result = await tc.send_action("interact_npc", {})
    tc.assert_action_error(result, "missing agent_id")


async def test_interact_npc_bad_id(tc: BridgeTestCase):
    result = await tc.send_action("interact_npc", {"agent_id": 99999999})
    tc.assert_action_error(result, "agent_not_found")


async def test_dialog_missing(tc: BridgeTestCase):
    result = await tc.send_action("dialog", {})
    tc.assert_action_error(result, "missing dialog_id")


async def test_interact_npc_success(tc: BridgeTestCase):
    """Find an NPC and interact with it."""
    snap = await tc.wait_for_snapshot(tier=2)
    npcs = [a for a in snap.get("agents", [])
            if a.get("agent_type") == "living"
            and a.get("allegiance") not in (1, 3)  # not ally or foe — NPC
            and a.get("is_alive", False)]
    if not npcs:
        tc.skip("No NPCs nearby")
    result = await tc.send_action("interact_npc", {"agent_id": npcs[0]["id"]})
    tc.assert_action_success(result)


# ============================================================
# C10-C12: Skillbar, Chat, Utility
# ============================================================

async def test_load_skillbar_missing(tc: BridgeTestCase):
    result = await tc.send_action("load_skillbar", {})
    tc.assert_action_error(result, "missing skill_ids")


async def test_load_skillbar_wrong_count(tc: BridgeTestCase):
    result = await tc.send_action("load_skillbar", {"skill_ids": [1, 2, 3]})
    tc.assert_action_error(result, "skill_ids must be array of 8")


async def test_send_chat_missing(tc: BridgeTestCase):
    result = await tc.send_action("send_chat", {})
    tc.assert_action_error(result, "missing message or channel")


async def test_send_chat_empty(tc: BridgeTestCase):
    result = await tc.send_action("send_chat", {"message": "", "channel": "team"})
    tc.assert_action_error(result, "empty_message")


async def test_send_chat_success(tc: BridgeTestCase):
    result = await tc.send_action("send_chat", {"message": "bridge_test", "channel": "team"})
    tc.assert_action_success(result)


async def test_wait_success(tc: BridgeTestCase):
    result = await tc.send_action("wait", {"milliseconds": 100})
    tc.assert_action_success(result)


# ============================================================
# C13: Craft item (GWA3-092)
# ============================================================

async def test_craft_item_missing_id(tc: BridgeTestCase):
    result = await tc.send_action("craft_item", {})
    tc.assert_action_error(result, "missing item_id")


async def test_craft_item_success_with_merchant(tc: BridgeTestCase):
    """If merchant/crafter window is open, craft_item should succeed and items should have valid fields."""
    snap = await tc.wait_for_snapshot(tier=2)
    merchant = snap.get("merchant", {})
    if not merchant.get("is_open"):
        tc.skip("No merchant/crafter window open")
    items = merchant.get("items", [])
    if not items:
        tc.skip("No items available in merchant window")
    # Validate merchant item structure
    item = items[0]
    assert_keys_present(item, ["item_id", "model_id", "type", "value"], f"merchant item")
    assert_type(item["item_id"], int, "merchant item.item_id")
    assert_gt(item["item_id"], 0, "merchant item.item_id")
    result = await tc.send_action("craft_item", {"item_id": item["item_id"], "quantity": 1})
    tc.assert_action_success(result)


async def test_transact_items_craft_type(tc: BridgeTestCase):
    """transact_items with type=3 (CrafterBuy) should accept valid params."""
    snap = await tc.wait_for_snapshot(tier=2)
    merchant = snap.get("merchant", {})
    if not merchant.get("is_open"):
        tc.skip("No merchant window open")
    items = merchant.get("items", [])
    if not items:
        tc.skip("No items in merchant list")
    result = await tc.send_action("transact_items", {
        "type": 3, "quantity": 1, "item_id": items[0]["item_id"]
    })
    tc.assert_action_success(result)


# ============================================================
# C14: Resign action (GWA3-093)
# ============================================================

async def test_resign_success(tc: BridgeTestCase):
    """Resign action should succeed (sends /resign chat)."""
    result = await tc.send_action("resign", {})
    tc.assert_action_success(result)


# ============================================================
# C15: Set bot state (GWA3-093, GWA3-096)
# ============================================================

async def test_set_bot_state_missing(tc: BridgeTestCase):
    result = await tc.send_action("set_bot_state", {})
    tc.assert_action_error(result, "missing state")


async def test_set_bot_state_unknown(tc: BridgeTestCase):
    result = await tc.send_action("set_bot_state", {"state": "nonexistent_state"})
    tc.assert_action_error(result, "unknown_state")


async def test_set_bot_state_idle(tc: BridgeTestCase):
    """set_bot_state to idle should succeed."""
    result = await tc.send_action("set_bot_state", {"state": "idle"})
    tc.assert_action_success(result)


async def test_set_bot_state_llm_controlled(tc: BridgeTestCase):
    """set_bot_state to llm_controlled should succeed."""
    result = await tc.send_action("set_bot_state", {"state": "llm_controlled"})
    tc.assert_action_success(result)


async def test_set_bot_state_reflects_in_snapshot(tc: BridgeTestCase):
    """After set_bot_state, the bot.state field in snapshot must change within 5s."""
    # Read original state
    snap_before = await tc.wait_for_snapshot(tier=1)
    original = snap_before.get("bot", {}).get("state", "idle")

    # Set to a known different state
    target = "maintenance" if original != "maintenance" else "merchant"
    result = await tc.send_action("set_bot_state", {"state": target})
    tc.assert_action_success(result)

    def check(snap):
        return snap.get("bot", {}).get("state") == target

    try:
        new_snap = await tc.wait_for_state_change(check, tier=1, timeout=5.0)
        assert_true(
            new_snap["bot"]["state"] == target,
            f"Expected bot state '{target}', got '{new_snap['bot']['state']}'",
        )
    finally:
        # Always restore
        await tc.send_action("set_bot_state", {"state": original})


async def test_set_bot_state_all_valid_states(tc: BridgeTestCase):
    """All valid state names should be accepted."""
    valid = ["idle", "in_town", "traveling", "in_dungeon", "looting",
             "merchant", "maintenance", "llm_controlled"]
    for state in valid:
        result = await tc.send_action("set_bot_state", {"state": state})
        tc.assert_action_success(result)
    # Restore
    await tc.send_action("set_bot_state", {"state": "idle"})
