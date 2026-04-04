"""Category C: Action Tests — send commands through the bridge and verify results + state changes."""

import asyncio
from .base import BridgeTestCase
from .helpers import assert_true, assert_gt, assert_keys_present, snapshot_get


# ============================================================
# C1-C2: Movement & Targeting
# ============================================================

async def test_move_to_success(tc: BridgeTestCase):
    """Send move_to with valid coordinates, expect success."""
    snap = await tc.wait_for_snapshot(tier=1)
    x = snap["me"]["x"] + 100
    y = snap["me"]["y"] + 100
    result = await tc.send_action("move_to", {"x": x, "y": y})
    tc.assert_action_success(result)


async def test_move_to_missing_params(tc: BridgeTestCase):
    result = await tc.send_action("move_to", {})
    tc.assert_action_error(result, "missing x or y")


async def test_move_to_out_of_range(tc: BridgeTestCase):
    result = await tc.send_action("move_to", {"x": 999999, "y": 999999})
    tc.assert_action_error(result, "coordinates_out_of_range")


async def test_move_to_state_change(tc: BridgeTestCase):
    """After move_to, player should be moving or position should change."""
    snap = await tc.wait_for_snapshot(tier=1)
    old_x = snap["me"]["x"]
    old_y = snap["me"]["y"]
    result = await tc.send_action("move_to", {"x": old_x + 200, "y": old_y + 200})
    tc.assert_action_success(result)

    # Wait for position to change or is_moving to be true
    def moved(s):
        me = s.get("me", {})
        dx = abs(me.get("x", old_x) - old_x)
        dy = abs(me.get("y", old_y) - old_y)
        return me.get("is_moving", False) or (dx > 10 or dy > 10)

    await tc.wait_for_state_change(moved, tier=1, timeout=5.0)


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
    """After change_target, me.target_id should update."""
    snap = await tc.wait_for_snapshot(tier=2)
    agents = snap.get("agents", [])
    if not agents:
        tc.skip("No nearby agents to target")
    target_id = agents[0]["id"]
    result = await tc.send_action("change_target", {"agent_id": target_id})
    tc.assert_action_success(result)

    def target_changed(s):
        return s.get("me", {}).get("target_id") == target_id

    await tc.wait_for_state_change(target_changed, tier=1, timeout=5.0)


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
    """Use skill in slot 0 if it's ready."""
    snap = await tc.wait_for_snapshot(tier=1)
    sk = snap["skillbar"][0]
    if sk["skill_id"] == 0:
        tc.skip("No skill in slot 0")
    if sk["recharge"] > 0:
        tc.skip("Skill in slot 0 is on recharge")
    result = await tc.send_action("use_skill", {"slot": 0})
    tc.assert_action_success(result)


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
    result = await tc.send_action("kick_all_heroes", {})
    tc.assert_action_success(result)
    await asyncio.sleep(1.0)  # let party update


async def test_add_hero_success(tc: BridgeTestCase):
    result = await tc.send_action("add_hero", {"hero_id": 25})
    tc.assert_action_success(result)
    await asyncio.sleep(0.5)


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
