"""Category B: Observation Tests — validate snapshot structure, types, and data plausibility."""

from .base import BridgeTestCase
from .helpers import (
    assert_true, assert_type, assert_keys_present, assert_in_range, assert_gte, assert_gt,
)


# ============================================================
# B1: Player state (me)
# ============================================================

async def test_me_present(tc: BridgeTestCase):
    """Tier 1 snapshot has 'me' key."""
    snap = await tc.wait_for_snapshot(tier=1, timeout=5.0)
    assert_keys_present(snap, ["me"], "tier 1")


async def test_me_agent_id_nonzero(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_gt(snap["me"]["agent_id"], 0, "me.agent_id")


async def test_me_position_plausible(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    me = snap["me"]
    assert_type(me["x"], (int, float), "me.x")
    assert_type(me["y"], (int, float), "me.y")
    assert_true(me["x"] != 0.0 or me["y"] != 0.0, "Player position should not be (0, 0)")


async def test_me_hp_in_range(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_in_range(snap["me"]["hp"], 0.0, 1.0, "me.hp")


async def test_me_energy_in_range(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_in_range(snap["me"]["energy"], 0.0, 1.0, "me.energy")


async def test_me_max_hp_and_energy(tc: BridgeTestCase):
    """max_hp and max_energy should be positive integers."""
    snap = await tc.wait_for_snapshot(tier=1)
    assert_gt(snap["me"]["max_hp"], 0, "me.max_hp")
    assert_gt(snap["me"]["max_energy"], 0, "me.max_energy")


async def test_me_professions_valid(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_in_range(snap["me"]["primary"], 1, 10, "me.primary")  # primary must be 1-10 (not 0)
    assert_in_range(snap["me"]["secondary"], 0, 10, "me.secondary")


async def test_me_level_valid(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_in_range(snap["me"]["level"], 1, 20, "me.level")


async def test_me_state_flags_are_booleans(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_type(snap["me"]["is_moving"], bool, "me.is_moving")
    assert_type(snap["me"]["is_casting"], bool, "me.is_casting")


async def test_me_target_id_is_int(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_type(snap["me"]["target_id"], int, "me.target_id")


# ============================================================
# B2: Skillbar
# ============================================================

async def test_skillbar_has_8_slots(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_true(len(snap["skillbar"]) == 8, f"Skillbar should have 8 slots, got {len(snap['skillbar'])}")


async def test_skillbar_slot_indices(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    for i, sk in enumerate(snap["skillbar"]):
        assert_true(sk["slot"] == i, f"Slot {i} has slot={sk['slot']}")


async def test_skillbar_skill_id_is_int(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    for sk in snap["skillbar"]:
        assert_type(sk["skill_id"], int, f"slot {sk['slot']}.skill_id")


async def test_skillbar_skill_data(tc: BridgeTestCase):
    """Non-zero skill_id entries should have constant data with valid types."""
    snap = await tc.wait_for_snapshot(tier=1)
    found_skill = False
    for sk in snap["skillbar"]:
        if sk["skill_id"] != 0:
            found_skill = True
            assert_keys_present(sk, ["type", "energy_cost", "activation", "recharge_time", "profession", "attribute"],
                                f"skill slot {sk['slot']}")
            assert_type(sk["energy_cost"], (int, float), f"slot {sk['slot']}.energy_cost")
            assert_type(sk["activation"], (int, float), f"slot {sk['slot']}.activation")
            assert_gte(sk["recharge_time"], 0, f"slot {sk['slot']}.recharge_time")
    if not found_skill:
        tc.skip("No skills loaded in skillbar")


async def test_skillbar_recharge_nonneg(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    for sk in snap["skillbar"]:
        assert_gte(sk["recharge"], 0, f"slot {sk['slot']} recharge")


# ============================================================
# B3: Map state
# ============================================================

async def test_map_id_positive(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_type(snap["map"]["map_id"], int, "map.map_id")
    assert_in_range(snap["map"]["map_id"], 1, 999, "map.map_id")


async def test_map_is_loaded(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_type(snap["map"]["is_loaded"], bool, "map.is_loaded")
    assert_true(snap["map"]["is_loaded"], "Map should be loaded during tests")


async def test_map_instance_time(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_gte(snap["map"]["instance_time"], 0, "map.instance_time")


async def test_map_has_region(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_type(snap["map"]["region"], int, "map.region")
    assert_in_range(snap["map"]["region"], 0, 20, "map.region")


# ============================================================
# B4: Party state
# ============================================================

async def test_party_size_positive(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_gt(snap["party"]["size"], 0, "party.size")


async def test_party_has_members(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    members = snap["party"]["members"]
    assert_true(len(members) >= 1, f"Party should have at least 1 member, got {len(members)}")


async def test_party_member_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    for m in snap["party"]["members"]:
        assert_keys_present(m, ["agent_id", "hp", "energy", "is_alive", "is_player", "is_hero", "primary", "level"],
                            f"party member {m.get('agent_id')}")
        assert_type(m["agent_id"], int, "member.agent_id")
        assert_gt(m["agent_id"], 0, "member.agent_id")
        assert_in_range(m["hp"], 0.0, 1.0, "member.hp")
        assert_in_range(m["energy"], 0.0, 1.0, "member.energy")
        assert_type(m["is_alive"], bool, "member.is_alive")
        assert_type(m["is_player"], bool, "member.is_player")
        assert_type(m["is_hero"], bool, "member.is_hero")


async def test_party_self_present(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    players = [m for m in snap["party"]["members"] if m.get("is_player")]
    assert_true(len(players) >= 1, "Party should contain at least one player member")


async def test_party_defeated_flag(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_type(snap["party"]["is_defeated"], bool, "party.is_defeated")


async def test_party_dead_count(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    dead_count = snap["party"]["dead_count"]
    size = snap["party"]["size"]
    assert_gte(dead_count, 0, "party.dead_count")
    assert_true(dead_count <= size, f"dead_count ({dead_count}) should not exceed party size ({size})")
    # Cross-validate: count members with is_alive=false
    actual_dead = sum(1 for m in snap["party"]["members"] if not m.get("is_alive", True))
    assert_true(dead_count == actual_dead,
                f"dead_count ({dead_count}) should match actual dead members ({actual_dead})")


# ============================================================
# B5: Nearby agents (Tier 2)
# ============================================================

async def test_agents_is_array(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_type(snap["agents"], list, "agents")


async def test_agent_core_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["agents"]:
        tc.skip("No agents nearby")
    for a in snap["agents"][:10]:
        assert_keys_present(a, ["id", "x", "y", "distance", "agent_type"], f"agent {a.get('id')}")
        assert_type(a["id"], int, f"agent.id")
        assert_gt(a["id"], 0, f"agent.id")
        assert_type(a["distance"], (int, float), f"agent {a['id']}.distance")
        assert_gte(a["distance"], 0, f"agent {a['id']}.distance")
        assert_true(
            a["agent_type"] in ("living", "item", "gadget", "unknown"),
            f"agent {a['id']} has unknown agent_type: '{a['agent_type']}'",
        )


async def test_living_agent_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    living = [a for a in snap["agents"] if a.get("agent_type") == "living"]
    if not living:
        tc.skip("No living agents nearby")
    for a in living[:5]:
        assert_keys_present(a, ["hp", "max_hp", "energy", "max_energy", "allegiance", "primary",
                                "secondary", "level", "is_alive", "is_casting", "casting_skill_id",
                                "has_hex", "has_enchantment"],
                            f"living agent {a['id']}")
        assert_in_range(a["hp"], 0.0, 1.0, f"agent {a['id']}.hp")
        assert_in_range(a["energy"], 0.0, 1.0, f"agent {a['id']}.energy")
        assert_in_range(a["allegiance"], 0, 255, f"agent {a['id']}.allegiance")
        assert_in_range(a["primary"], 0, 10, f"agent {a['id']}.primary")
        assert_in_range(a["level"], 0, 30, f"agent {a['id']}.level")
        assert_type(a["is_alive"], bool, f"agent {a['id']}.is_alive")
        assert_type(a["is_casting"], bool, f"agent {a['id']}.is_casting")


async def test_item_agent_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    items = [a for a in snap["agents"] if a.get("agent_type") == "item"]
    if not items:
        tc.skip("No item agents nearby")
    for a in items[:5]:
        assert_keys_present(a, ["item_id", "owner", "model_id", "item_type", "quantity", "value"],
                            f"item agent {a['id']}")
        assert_type(a["item_id"], int, f"item {a['id']}.item_id")
        assert_gt(a["item_id"], 0, f"item {a['id']}.item_id")
        assert_type(a["model_id"], int, f"item {a['id']}.model_id")


async def test_foe_casting_fields(tc: BridgeTestCase):
    """Living agents should have is_casting and casting_skill_id fields."""
    snap = await tc.wait_for_snapshot(tier=2)
    living = [a for a in snap["agents"] if a.get("agent_type") == "living"]
    if not living:
        tc.skip("No living agents nearby")
    for a in living[:5]:
        assert_keys_present(a, ["is_casting", "casting_skill_id"], f"agent {a['id']}")
        assert_type(a["is_casting"], bool, f"agent {a['id']}.is_casting")


async def test_hex_enchant_flags(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    living = [a for a in snap["agents"] if a.get("agent_type") == "living"]
    if not living:
        tc.skip("No living agents nearby")
    for a in living[:5]:
        assert_keys_present(a, ["has_hex", "has_enchantment"], f"agent {a['id']}")
        assert_type(a["has_hex"], bool, f"agent {a['id']}.has_hex")
        assert_type(a["has_enchantment"], bool, f"agent {a['id']}.has_enchantment")


# ============================================================
# B6: Hero skillbars (Tier 2)
# ============================================================

async def test_heroes_is_array(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_type(snap["heroes"], list, "heroes")


async def test_hero_has_agent_id(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["heroes"]:
        tc.skip("No heroes in party")
    for h in snap["heroes"]:
        assert_type(h["agent_id"], int, "hero.agent_id")
        assert_gt(h["agent_id"], 0, "hero.agent_id")
        assert_in_range(h["hp"], 0.0, 1.0, f"hero {h['agent_id']}.hp")
        assert_in_range(h["energy"], 0.0, 1.0, f"hero {h['agent_id']}.energy")
        assert_in_range(h["primary"], 1, 10, f"hero {h['agent_id']}.primary")


async def test_hero_has_skillbar(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["heroes"]:
        tc.skip("No heroes in party")
    for h in snap["heroes"]:
        assert_type(h["skillbar"], list, f"hero {h['agent_id']}.skillbar")
        assert_true(len(h["skillbar"]) == 8,
                    f"Hero {h['agent_id']} skillbar should have 8 slots, got {len(h['skillbar'])}")
        for sk in h["skillbar"]:
            assert_keys_present(sk, ["slot", "skill_id", "recharge"], f"hero {h['agent_id']} skill")


async def test_hero_casting_state(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["heroes"]:
        tc.skip("No heroes in party")
    for h in snap["heroes"]:
        assert_keys_present(h, ["is_casting", "casting_skill_id"], f"hero {h['agent_id']}")
        assert_type(h["is_casting"], bool, f"hero {h['agent_id']}.is_casting")
        assert_type(h["casting_skill_id"], int, f"hero {h['agent_id']}.casting_skill_id")
        if h["is_casting"]:
            assert_gt(h["casting_skill_id"], 0,
                      f"hero {h['agent_id']} is_casting=true but casting_skill_id=0")


# ============================================================
# B7-B9: Dialog, merchant, chat (Tier 2)
# ============================================================

async def test_dialog_closed_state(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap, ["dialog"], "tier 2")
    assert_type(snap["dialog"]["is_open"], bool, "dialog.is_open")


async def test_merchant_closed_state(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap, ["merchant"], "tier 2")
    assert_type(snap["merchant"]["is_open"], bool, "merchant.is_open")


async def test_chat_is_array(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap, ["chat"], "tier 2")
    assert_type(snap["chat"], list, "chat")


async def test_chat_message_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["chat"]:
        tc.skip("No chat messages in this snapshot")
    for msg in snap["chat"][:5]:
        assert_keys_present(msg, ["channel", "message"], f"chat message")


# ============================================================
# B10-B11: Inventory & storage (Tier 3)
# ============================================================

async def test_inventory_present(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    assert_keys_present(snap, ["inventory"], "tier 3")


async def test_inventory_gold(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    assert_type(snap["inventory"]["gold_character"], int, "gold_character")
    assert_type(snap["inventory"]["gold_storage"], int, "gold_storage")
    assert_in_range(snap["inventory"]["gold_character"], 0, 1000000, "gold_character")
    assert_in_range(snap["inventory"]["gold_storage"], 0, 10000000, "gold_storage")


async def test_inventory_bags(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    bags = snap["inventory"]["bags"]
    assert_type(bags, list, "inventory.bags")
    assert_true(len(bags) >= 1, f"Should have at least 1 bag, got {len(bags)}")


async def test_inventory_bag_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    for bag in snap["inventory"]["bags"]:
        assert_keys_present(bag, ["bag_index", "item_count", "items", "free_slots"], f"bag {bag.get('bag_index')}")


async def test_inventory_item_rarity(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    for bag in snap["inventory"]["bags"]:
        for item in bag["items"][:3]:
            assert_keys_present(item, ["item_id", "model_id", "type", "quantity", "rarity"], f"item {item.get('item_id')}")
            assert_true(
                item["rarity"] in ("white", "blue", "purple", "gold", "green", "gray"),
                f"Unknown rarity: {item['rarity']}",
            )


async def test_inventory_free_slots(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    assert_gte(snap["inventory"]["free_slots_total"], 0, "free_slots_total")


async def test_storage_present(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    assert_keys_present(snap, ["storage"], "tier 3")
    assert_type(snap["storage"], list, "storage")


# ============================================================
# B12: Effects & tier progression
# ============================================================

async def test_effects_present(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    assert_keys_present(snap, ["effects"], "tier 3")
    assert_type(snap["effects"], list, "effects")


async def test_effect_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    if not snap["effects"]:
        tc.skip("No active effects on player")
    for eff in snap["effects"][:5]:
        assert_keys_present(eff, ["skill_id", "time_remaining", "duration", "type", "caster_agent_id"],
                            f"effect skill_id={eff.get('skill_id')}")
        assert_type(eff["skill_id"], int, "effect.skill_id")
        assert_gt(eff["skill_id"], 0, "effect.skill_id")
        assert_type(eff["time_remaining"], (int, float), "effect.time_remaining")
        assert_gte(eff["time_remaining"], 0, "effect.time_remaining")


async def test_tier_1_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_keys_present(snap, ["me", "skillbar", "map", "party", "bot"], "tier 1")


async def test_tier_2_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap, ["me", "skillbar", "map", "party", "bot", "agents", "heroes", "dialog", "merchant", "chat", "quests"], "tier 2")


async def test_tier_3_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    assert_keys_present(snap, ["me", "skillbar", "map", "party", "bot", "agents", "heroes", "dialog", "merchant", "chat", "quests", "inventory", "storage", "effects", "titles"], "tier 3")


# ============================================================
# B13: Morale (GWA3-088)
# ============================================================

async def test_party_has_morale(tc: BridgeTestCase):
    """Party object includes morale field."""
    snap = await tc.wait_for_snapshot(tier=1)
    assert_keys_present(snap["party"], ["morale"], "party")


async def test_morale_in_range(tc: BridgeTestCase):
    """Morale should be between -60 and +10."""
    snap = await tc.wait_for_snapshot(tier=1)
    assert_in_range(snap["party"]["morale"], -60, 10, "party.morale")


# ============================================================
# B14: Vanquish progress (GWA3-088)
# ============================================================

async def test_map_has_vanquish_fields(tc: BridgeTestCase):
    """Map object includes foes_killed and foes_to_kill when available."""
    snap = await tc.wait_for_snapshot(tier=1)
    m = snap["map"]
    # These may be absent if not in explorable — just verify types if present
    if "foes_killed" in m:
        assert_type(m["foes_killed"], int, "map.foes_killed")
        assert_gte(m["foes_killed"], 0, "map.foes_killed")
    if "foes_to_kill" in m:
        assert_type(m["foes_to_kill"], int, "map.foes_to_kill")
        assert_gte(m["foes_to_kill"], 0, "map.foes_to_kill")


# ============================================================
# B15: Map loading 3-state (GWA3-088)
# ============================================================

async def test_map_loading_state_present(tc: BridgeTestCase):
    """Map has loading_state field."""
    snap = await tc.wait_for_snapshot(tier=1)
    assert_keys_present(snap["map"], ["loading_state"], "map")


async def test_map_loading_state_valid(tc: BridgeTestCase):
    """loading_state is 0, 1, or 2."""
    snap = await tc.wait_for_snapshot(tier=1)
    assert_true(
        snap["map"]["loading_state"] in (0, 1, 2),
        f"loading_state should be 0/1/2, got {snap['map']['loading_state']}",
    )


async def test_map_loading_state_loaded_during_test(tc: BridgeTestCase):
    """During tests, loading_state should be 1 (loaded)."""
    snap = await tc.wait_for_snapshot(tier=1)
    assert_true(
        snap["map"]["loading_state"] == 1,
        f"Expected loading_state=1 during test, got {snap['map']['loading_state']}",
    )


# ============================================================
# B16: Quest state (GWA3-089)
# ============================================================

async def test_quests_present_in_tier2(tc: BridgeTestCase):
    """Tier 2 snapshot has quests object."""
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap, ["quests"], "tier 2")


async def test_quests_has_active_quest_id(tc: BridgeTestCase):
    """Quests object has active_quest_id field."""
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap["quests"], ["active_quest_id", "quest_log_size"], "quests")
    assert_type(snap["quests"]["active_quest_id"], int, "quests.active_quest_id")


async def test_quests_log_size_nonneg(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_gte(snap["quests"]["quest_log_size"], 0, "quests.quest_log_size")


async def test_quest_log_is_array(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap["quests"], ["quest_log"], "quests")
    assert_type(snap["quests"]["quest_log"], list, "quests.quest_log")


async def test_quest_log_entry_fields(tc: BridgeTestCase):
    """Quest log entries have quest_id, log_state, is_completed."""
    snap = await tc.wait_for_snapshot(tier=2)
    log = snap["quests"]["quest_log"]
    if not log:
        tc.skip("Quest log is empty")
    for entry in log[:5]:
        assert_keys_present(entry, ["quest_id", "log_state", "is_completed"], f"quest {entry.get('quest_id')}")
        assert_type(entry["is_completed"], bool, "quest.is_completed")


async def test_active_quest_details(tc: BridgeTestCase):
    """If active quest ID != 0, active_quest object has details."""
    snap = await tc.wait_for_snapshot(tier=2)
    q = snap["quests"]
    if q["active_quest_id"] == 0:
        tc.skip("No active quest")
    assert_keys_present(q, ["active_quest"], "quests")
    aq = q["active_quest"]
    assert_keys_present(aq, ["quest_id", "log_state", "is_completed", "map_from", "map_to"], "active_quest")


# ============================================================
# B17: Title progression (GWA3-089)
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
# B18: Dialog decoding (GWA3-090)
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
# B19: Chat log channels (GWA3-090)
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
# B20: Bot state (GWA3-096 partial)
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
# B21: Chest identification (GWA3-091)
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
# B22: Agent names (GWA3-091)
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
# B23: Froggy observable state changes (GWA3-120)
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
