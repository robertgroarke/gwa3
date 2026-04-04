"""Category B: Observation Tests — validate snapshot structure, types, and data plausibility."""

from .base import BridgeTestCase
from .helpers import (
    assert_true, assert_type, assert_keys_present, assert_in_range, assert_gte, assert_gt,
    snapshot_get,
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
    assert_true(me["x"] != 0.0 or me["y"] != 0.0, "Player position should not be (0, 0)")


async def test_me_hp_in_range(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_in_range(snap["me"]["hp"], 0.0, 1.0, "me.hp")


async def test_me_energy_in_range(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_in_range(snap["me"]["energy"], 0.0, 1.0, "me.energy")


async def test_me_professions_valid(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_in_range(snap["me"]["primary"], 0, 10, "me.primary")
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


async def test_skillbar_skill_data(tc: BridgeTestCase):
    """Non-zero skill_id entries should have constant data (type, energy_cost)."""
    snap = await tc.wait_for_snapshot(tier=1)
    for sk in snap["skillbar"]:
        if sk["skill_id"] != 0:
            assert_keys_present(sk, ["type", "energy_cost", "activation", "recharge_time"],
                                f"skill slot {sk['slot']}")


async def test_skillbar_recharge_nonneg(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    for sk in snap["skillbar"]:
        assert_gte(sk["recharge"], 0, f"slot {sk['slot']} recharge")


# ============================================================
# B3: Map state
# ============================================================

async def test_map_id_positive(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_gt(snap["map"]["map_id"], 0, "map.map_id")


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
        assert_keys_present(m, ["agent_id", "hp", "is_alive", "is_player"], "party member")


async def test_party_self_present(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    players = [m for m in snap["party"]["members"] if m.get("is_player")]
    assert_true(len(players) >= 1, "Party should contain at least one player member")


async def test_party_defeated_flag(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_type(snap["party"]["is_defeated"], bool, "party.is_defeated")


async def test_party_dead_count(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_gte(snap["party"]["dead_count"], 0, "party.dead_count")


# ============================================================
# B5: Nearby agents (Tier 2)
# ============================================================

async def test_agents_is_array(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_type(snap["agents"], list, "agents")


async def test_agent_core_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    for a in snap["agents"][:10]:  # check first 10
        assert_keys_present(a, ["id", "x", "y", "distance", "agent_type"], f"agent {a.get('id')}")


async def test_living_agent_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    living = [a for a in snap["agents"] if a.get("agent_type") == "living"]
    if not living:
        tc.skip("No living agents nearby")
    for a in living[:5]:
        assert_keys_present(a, ["hp", "allegiance", "primary", "level", "is_alive"], f"living agent {a['id']}")


async def test_item_agent_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    items = [a for a in snap["agents"] if a.get("agent_type") == "item"]
    if not items:
        tc.skip("No item agents nearby")
    for a in items[:5]:
        assert_keys_present(a, ["item_id", "model_id"], f"item agent {a['id']}")


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
        assert_gt(h["agent_id"], 0, "hero.agent_id")


async def test_hero_has_skillbar(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["heroes"]:
        tc.skip("No heroes in party")
    for h in snap["heroes"]:
        assert_true(len(h["skillbar"]) == 8, f"Hero {h['agent_id']} skillbar should have 8 slots")


async def test_hero_casting_state(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    if not snap["heroes"]:
        tc.skip("No heroes in party")
    for h in snap["heroes"]:
        assert_keys_present(h, ["is_casting", "casting_skill_id"], f"hero {h['agent_id']}")


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
    assert_gte(snap["inventory"]["gold_character"], 0, "gold_character")
    assert_gte(snap["inventory"]["gold_storage"], 0, "gold_storage")


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
        assert_keys_present(eff, ["skill_id", "time_remaining"], f"effect")


async def test_tier_1_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=1)
    assert_keys_present(snap, ["me", "skillbar", "map", "party"], "tier 1")


async def test_tier_2_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=2)
    assert_keys_present(snap, ["me", "skillbar", "map", "party", "agents", "heroes", "dialog", "merchant", "chat"], "tier 2")


async def test_tier_3_fields(tc: BridgeTestCase):
    snap = await tc.wait_for_snapshot(tier=3)
    assert_keys_present(snap, ["me", "skillbar", "map", "party", "agents", "heroes", "dialog", "merchant", "chat", "inventory", "storage", "effects"], "tier 3")
