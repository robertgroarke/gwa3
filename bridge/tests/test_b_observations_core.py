"""Category B: Observation Tests — validate snapshot structure, types, and data plausibility."""

from .base import (
    BridgeTestCase,
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
