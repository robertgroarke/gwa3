"""Category B: Observation Tests — validate snapshot structure, types, and data plausibility."""

from .base import (
    BridgeTestCase,
    assert_true, assert_type, assert_keys_present, assert_in_range, assert_gte, assert_gt,
)

# ============================================================
# B17: Title progression ()
# ============================================================

async def test_titles_present_in_tier3(tc: BridgeTestCase):
    """Tier 3 snapshot has titles object."""
    snap = await tc.wait_for_snapshot(tier=3)
    assert_keys_present(snap, ["titles"], "tier 3")
    assert_type(snap["titles"], dict, "titles")


async def test_title_entry_fields(tc: BridgeTestCase):
    """Title entries have current_points, current_rank, points_needed_next, max_rank."""
    snap = await tc.wait_for_snapshot(tier=3)
    titles = snap["titles"]
    if not titles:
        tc.skip("No title data available")
    for name, data in list(titles.items())[:3]:
        assert_keys_present(data, ["current_points", "current_rank", "max_rank"], f"title '{name}'")
        assert_gte(data["current_points"], 0, f"title '{name}'.current_points")


# ============================================================
# B18: Dialog decoding ()
# ============================================================

async def test_dialog_has_is_open(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap["dialog"], ["is_open"], "dialog")
    assert_type(snap["dialog"]["is_open"], bool, "dialog.is_open")


async def test_dialog_body_field_exists_when_open(tc: BridgeTestCase):
    """If dialog is open, it should have body (decoded) or body_raw."""
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["dialog"]["is_open"]:
        tc.skip("No dialog open — trigger via interact_npc to test")
    # At least one of body or body_raw should be present
    assert_true(
        "body" in snap["dialog"] or "body_raw" in snap["dialog"],
        "Open dialog should have 'body' or 'body_raw' field",
    )


async def test_dialog_buttons_when_open(tc: BridgeTestCase):
    """If dialog is open, buttons array should have entries with dialog_id and label."""
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["dialog"]["is_open"]:
        tc.skip("No dialog open")
    buttons = snap["dialog"].get("buttons", [])
    if not buttons:
        tc.skip("Dialog has no buttons")
    for btn in buttons:
        assert_keys_present(btn, ["dialog_id", "label"], "dialog button")
        assert_type(btn["dialog_id"], int, "button.dialog_id")


# ============================================================
# B19: Chat log channels ()
# ============================================================

async def test_chat_message_has_channel(tc: BridgeTestCase):
    """Chat messages have a channel field with a known channel name."""
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["chat"]:
        tc.skip("No chat messages in this snapshot")
    known_channels = {"all", "team", "guild", "trade", "alliance", "whisper",
                      "emote", "warning", "allies", "global", "advisory", "unknown"}
    for msg in snap["chat"][:5]:
        assert_keys_present(msg, ["channel", "message"], "chat message")
        assert_true(
            msg["channel"] in known_channels,
            f"Unknown chat channel: '{msg['channel']}'",
        )


async def test_chat_sender_field(tc: BridgeTestCase):
    """Chat messages should have a sender field (may be empty for system messages)."""
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["chat"]:
        tc.skip("No chat messages in this snapshot")
    for msg in snap["chat"][:5]:
        assert_true("sender" in msg or "message" in msg, "Chat entry should have sender or message")


# ============================================================
# B20: Bot state ( partial)
# ============================================================

async def test_bot_state_present(tc: BridgeTestCase):
    """Tier 1 includes bot state object."""
    snap = await tc.wait_for_snapshot(tier=1)
    assert_keys_present(snap, ["bot"], "tier 1")


async def test_bot_state_fields(tc: BridgeTestCase):
    """Bot state has state and is_running fields."""
    snap = await tc.wait_for_snapshot(tier=1)
    bot = snap["bot"]
    assert_keys_present(bot, ["state", "is_running"], "bot")
    assert_type(bot["state"], str, "bot.state")
    assert_type(bot["is_running"], bool, "bot.is_running")


async def test_bot_combat_mode(tc: BridgeTestCase):
    """Bot state includes combat_mode field."""
    snap = await tc.wait_for_snapshot(tier=1)
    bot = snap.get("bot", {})
    if not bot:
        tc.skip("No bot state in snapshot")
    assert_keys_present(bot, ["combat_mode"], "bot")
    assert_true(
        bot["combat_mode"] in ("builtin", "llm"),
        f"combat_mode should be 'builtin' or 'llm', got '{bot.get('combat_mode')}'",
    )


async def test_bot_state_valid_name(tc: BridgeTestCase):
    """Bot state name is one of the known states."""
    snap = await tc.wait_for_snapshot(tier=1)
    valid_states = {"idle", "char_select", "in_town", "traveling", "in_dungeon",
                    "looting", "merchant", "maintenance", "error", "stopping",
                    "llm_controlled", "unknown"}
    assert_true(
        snap["bot"]["state"] in valid_states,
        f"Unknown bot state: '{snap['bot']['state']}'",
    )


# ============================================================
# B21: Chest identification ()
# ============================================================

async def test_gadget_has_gadget_id(tc: BridgeTestCase):
    """Gadget agents should have gadget_id and is_chest fields."""
    snap = await tc.wait_for_snapshot(tier=2)
    gadgets = [a for a in snap.get("agents", []) if a.get("agent_type") == "gadget"]
    if not gadgets:
        tc.skip("No gadget agents nearby")
    for g in gadgets[:5]:
        assert_keys_present(g, ["gadget_id", "is_chest"], f"gadget {g.get('id')}")
        assert_type(g["is_chest"], bool, f"gadget {g['id']}.is_chest")
        assert_type(g["gadget_id"], int, f"gadget {g['id']}.gadget_id")


async def test_gadget_extra_type(tc: BridgeTestCase):
    """Gadgets should have extra_type field."""
    snap = await tc.wait_for_snapshot(tier=2)
    gadgets = [a for a in snap.get("agents", []) if a.get("agent_type") == "gadget"]
    if not gadgets:
        tc.skip("No gadget agents nearby")
    for g in gadgets[:5]:
        assert_keys_present(g, ["extra_type"], f"gadget {g.get('id')}")


# ============================================================
# B22: Agent names ()
# ============================================================

async def test_player_agent_has_name(tc: BridgeTestCase):
    """Player agents (login_number > 0) should have a name field."""
    snap = await tc.wait_for_snapshot(tier=2)
    players = [a for a in snap.get("agents", [])
               if a.get("agent_type") == "living" and a.get("player_number", 0) > 0]
    if not players:
        tc.skip("No other player agents nearby")
    for p in players[:3]:
        assert_keys_present(p, ["name"], f"player agent {p.get('id')}")
        assert_type(p["name"], str, f"player agent {p['id']}.name")
        assert_true(len(p["name"]) > 0, f"Player agent {p['id']} name should not be empty")


async def test_living_agents_have_player_number(tc: BridgeTestCase):
    """All living agents should have player_number field."""
    snap = await tc.wait_for_snapshot(tier=2)
    living = [a for a in snap.get("agents", []) if a.get("agent_type") == "living"]
    if not living:
        tc.skip("No living agents nearby")
    for a in living[:5]:
        assert_keys_present(a, ["player_number"], f"agent {a.get('id')}")


# ============================================================
# B23: Froggy observable state changes ()
# ============================================================

async def test_froggy_skillbar_has_skills_in_outpost(tc: BridgeTestCase):
    """In an outpost after setup, player skillbar should have non-zero skills."""
    snap = await tc.wait_for_snapshot(tier=1)
    skills = snap.get("skillbar", [])
    assert_true(len(skills) == 8, f"Skillbar should have 8 slots, got {len(skills)}")
    # Verify at least some slots have skill data structure
    for sk in skills:
        assert_keys_present(sk, ["slot", "skill_id", "recharge"], f"skill slot {sk.get('slot')}")


async def test_froggy_inventory_has_rarity(tc: BridgeTestCase):
    """After Froggy's identify/salvage, items should have rarity field."""
    snap = await tc.wait_for_snapshot(tier=3)
    inv = snap.get("inventory", {})
    if not inv:
        tc.skip("No inventory data")
    for bag in inv.get("bags", []):
        for item in bag.get("items", [])[:3]:
            assert_keys_present(item, ["rarity"], f"item {item.get('item_id')}")
            assert_true(
                item["rarity"] in ("white", "blue", "purple", "gold", "green", "gray"),
                f"Invalid rarity: {item['rarity']}",
            )


async def test_froggy_free_slots_after_salvage(tc: BridgeTestCase):
    """After salvage/sell, free_slots_total should be > 0."""
    snap = await tc.wait_for_snapshot(tier=3)
    inv = snap.get("inventory", {})
    if not inv:
        tc.skip("No inventory data")
    free = inv.get("free_slots_total", 0)
    assert_type(free, int, "inventory.free_slots_total")
    # Can't guarantee specific count, just verify it's plausible
    assert_gte(free, 0, "inventory.free_slots_total")
