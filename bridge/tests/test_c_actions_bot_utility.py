"""Category C: Action Tests — send commands through the bridge and verify results + state changes."""

from .base import BridgeTestCase, assert_true, assert_gt, assert_keys_present, assert_type

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


async def test_send_whisper_missing(tc: BridgeTestCase):
    result = await tc.send_action("send_whisper", {})
    tc.assert_action_error(result, "missing recipient or message")


async def test_send_whisper_empty_recipient(tc: BridgeTestCase):
    result = await tc.send_action("send_whisper", {"recipient": "", "message": "bridge_test"})
    tc.assert_action_error(result, "empty_recipient")


async def test_send_whisper_empty_message(tc: BridgeTestCase):
    result = await tc.send_action("send_whisper", {"recipient": "Test Recipient", "message": ""})
    tc.assert_action_error(result, "empty_message")


async def test_send_whisper_success(tc: BridgeTestCase):
    result = await tc.send_action("send_whisper", {"recipient": "Test Recipient", "message": "bridge_test"})
    tc.assert_action_success(result)


async def test_wait_success(tc: BridgeTestCase):
    result = await tc.send_action("wait", {"milliseconds": 100})
    tc.assert_action_success(result)


# ============================================================
# C13: Craft item ()
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
# C14: Resign action ()
# ============================================================

async def test_resign_success(tc: BridgeTestCase):
    """Resign action should succeed (sends /resign chat)."""
    result = await tc.send_action("resign", {})
    tc.assert_action_success(result)


# ============================================================
# C15: Set bot state (, )
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


# ============================================================
# C16: Combat mode toggle ()
# ============================================================

async def test_set_combat_mode_missing(tc: BridgeTestCase):
    result = await tc.send_action("set_combat_mode", {})
    tc.assert_action_error(result, "missing mode")


async def test_set_combat_mode_unknown(tc: BridgeTestCase):
    result = await tc.send_action("set_combat_mode", {"mode": "invalid"})
    tc.assert_action_error(result, "unknown_mode")


async def test_set_combat_mode_llm(tc: BridgeTestCase):
    result = await tc.send_action("set_combat_mode", {"mode": "llm"})
    tc.assert_action_success(result)
    # Restore
    await tc.send_action("set_combat_mode", {"mode": "builtin"})


async def test_set_combat_mode_builtin(tc: BridgeTestCase):
    result = await tc.send_action("set_combat_mode", {"mode": "builtin"})
    tc.assert_action_success(result)


async def test_combat_mode_reflects_in_snapshot(tc: BridgeTestCase):
    """After set_combat_mode, bot.combat_mode should update in snapshot."""
    snap_before = await tc.wait_for_snapshot(tier=1)
    original = snap_before.get("bot", {}).get("combat_mode", "builtin")

    target = "llm" if original != "llm" else "builtin"
    result = await tc.send_action("set_combat_mode", {"mode": target})
    tc.assert_action_success(result)

    def check(snap):
        return snap.get("bot", {}).get("combat_mode") == target

    try:
        new_snap = await tc.wait_for_state_change(check, tier=1, timeout=5.0)
        assert_true(
            new_snap["bot"]["combat_mode"] == target,
            f"Expected combat_mode '{target}', got '{new_snap['bot']['combat_mode']}'",
        )
    finally:
        await tc.send_action("set_combat_mode", {"mode": original})
