"""Category C: Action Tests — send commands through the bridge and verify results + state changes."""

from .base import BridgeTestCase, assert_true, assert_gt, assert_keys_present, assert_type

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
