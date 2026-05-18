"""Category B: Observation Tests — validate snapshot structure, types, and data plausibility."""

from .base import (
    BridgeTestCase,
    assert_true, assert_type, assert_keys_present, assert_in_range, assert_gte, assert_gt,
)

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
