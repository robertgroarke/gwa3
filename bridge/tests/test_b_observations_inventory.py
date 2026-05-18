"""Category B: Observation Tests — validate snapshot structure, types, and data plausibility."""

from .base import (
    BridgeTestCase,
    assert_true, assert_type, assert_keys_present, assert_in_range, assert_gte, assert_gt,
)

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
# B13: Morale ()
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
# B14: Vanquish progress ()
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
# B15: Map loading 3-state ()
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
# B16: Quest state ()
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
