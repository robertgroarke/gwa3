"""Hand-curated farming knowledge for the Gemma-driven bot.

Unlike `gamedata.py` (auto-generated from AutoIt enums), this module contains
*gameplay* knowledge that the bot can't derive from snapshots alone — which NPC
crafts which consumable, which materials a recipe takes, where the material
trader and Xunlai chest are in each outpost, etc.

Exposed via the `get_recipe` and `get_outpost_info` tools so the LLM can
look things up on demand instead of having them all baked into the system
prompt.

Sources:
  - Consumable recipes: crafter NPC dialog in game + observed in
    `bridge/tests/test_conset_bridge.py`
  - Material trader NPC IDs per map: `GWA Censured/lib/Utils-Maintenance.au3`
    `GetMaterialTrader()` table
  - Xunlai chest coords (guild halls): `Chest()` function in the same AutoIt lib
  - Outpost NPC coords: probed via the conset bridge test harness
"""

# =============================================================================
# Consumable crafting recipes — model_id → recipe metadata
# =============================================================================
# Materials per single craft (not per batch). For a batch of N, multiply
# `quantity` by N and pass `gold_cost * N` as the total gold.

CONSUMABLE_RECIPES: dict[int, dict] = {
    24861: {  # Grail of Might
        "name": "Grail of Might",
        "effect_id": 2521,
        "crafter_name": "Eyja",
        "crafter_outpost_map_id": 857,
        "crafter_outpost_name": "Embark Beach",
        "crafter_x": 3336.0,
        "crafter_y": 627.0,
        "gold_cost": 250,
        "materials": [
            {"model_id": 948, "name": "Iron Ingot", "quantity": 50},
            {"model_id": 929, "name": "Pile of Glittering Dust", "quantity": 50},
        ],
    },
    24859: {  # Essence of Celerity
        "name": "Essence of Celerity",
        "effect_id": 2522,
        "crafter_name": "Kwat",
        "crafter_outpost_map_id": 857,
        "crafter_outpost_name": "Embark Beach",
        "crafter_x": 3596.0,
        "crafter_y": 107.0,
        "gold_cost": 250,
        "materials": [
            {"model_id": 933, "name": "Feather", "quantity": 50},
            {"model_id": 929, "name": "Pile of Glittering Dust", "quantity": 50},
        ],
    },
    24860: {  # Armor of Salvation
        "name": "Armor of Salvation",
        "effect_id": 2520,
        "crafter_name": "Alcus",
        "crafter_outpost_map_id": 857,
        "crafter_outpost_name": "Embark Beach",
        "crafter_x": 3704.0,
        "crafter_y": -163.0,
        "gold_cost": 250,
        "materials": [
            {"model_id": 948, "name": "Iron Ingot", "quantity": 50},
            {"model_id": 921, "name": "Bone", "quantity": 50},
        ],
    },
}

# =============================================================================
# Outpost NPCs — map_id → {material trader, xunlai chest, crafters, ...}
# =============================================================================
# Coordinates confirmed for Embark Beach from conset_bridge harness runs.
# Other outposts: material trader NPC model IDs from AutoIt scripts, but
# coordinates require probing in-game to populate.

OUTPOST_NPCS: dict[int, dict] = {
    857: {  # Embark Beach — one-stop shop for Nightfall/EotN conset runs
        "name": "Embark Beach",
        "material_trader": {
            "name": "Argus",
            "x": 2933.0,
            "y": -2236.0,
            "npc_model_id": 3285,
        },
        "rare_material_trader": {
            "name": "Argus [Rare Material Trader]",
            "x": 2933.0,
            "y": -2236.0,
        },
        "xunlai_chest": {"x": 2283.0, "y": -2134.0},
        "merchant": {"name": "Ozem", "x": 2650.0, "y": -2100.0},
        "crafters": [
            {"name": "Eyja", "x": 3336.0, "y": 627.0,
             "crafts": ["Grail of Might"]},
            {"name": "Kwat", "x": 3596.0, "y": 107.0,
             "crafts": ["Essence of Celerity"]},
            {"name": "Alcus", "x": 3704.0, "y": -163.0,
             "crafts": ["Armor of Salvation"]},
        ],
    },
}

# =============================================================================
# Material trader NPC model IDs per map
# =============================================================================
# Source: GWA Censured GetMaterialTrader() AutoIt lookup. The COORDS of the
# NPC in each map aren't hardcoded there — the bot has to find the NPC by
# model_id. These IDs match agent->agent_model_type in the AgentLiving struct.

MATERIAL_TRADER_NPC_MODEL_BY_MAP: dict[int, int] = {
    4: 204, 5: 204, 6: 204, 52: 204, 176: 204, 177: 204, 178: 204, 179: 204,
    275: 191, 276: 191, 359: 191, 360: 191, 529: 191, 530: 191, 537: 191, 538: 191,
    49: 2017, 81: 2017, 109: 2017,
    193: 3624,
    194: 3285, 242: 3285, 857: 3285,
    376: 5391,
    638: 6763,
    640: 6764,
    641: 6065,
    645: 6050,
}

# =============================================================================
# Material model_id → readable name
# =============================================================================

MATERIAL_NAMES: dict[int, str] = {
    921: "Bone",
    922: "Charcoal",
    923: "Monstrous Claw",
    925: "Bolt of Cloth",
    926: "Bolt of Linen",
    927: "Bolt of Damask",
    928: "Bolt of Silk",
    929: "Pile of Glittering Dust",
    930: "Glob of Ectoplasm",
    931: "Monstrous Eye",
    932: "Monstrous Fang",
    933: "Feather",
    934: "Plant Fiber",
    935: "Diamond",
    936: "Onyx Gemstone",
    937: "Ruby",
    938: "Sapphire",
    939: "Vial of Ink",
    940: "Tanned Hide Square",
    948: "Iron Ingot",
}

# =============================================================================
# Outpost map names — subset that matters for farming routes
# =============================================================================

MAP_NAMES: dict[int, str] = {
    85: "Ascalon Arena",
    109: "Amnoon Oasis",
    193: "Cavalon",
    194: "Kaineng Center",
    553: "Varajar Fells",
    558: "Sparkfly Swamp",
    569: "Magus Stones",
    570: "Catacombs of Kathandrax L1",
    571: "Catacombs of Kathandrax L2",
    573: "Rragar's Menagerie L1",
    584: "Arachni's Haunt L1",
    585: "Arachni's Haunt L2",
    615: "Bogroot Growths L1",
    616: "Bogroot Growths L2",
    617: "Raven's Point L1",
    618: "Raven's Point L2",
    619: "Raven's Point L3",
    630: "Frostmaw's Burrows L1",
    638: "Gadd's Encampment",
    640: "Rata Sum",
    641: "Sunspear Great Hall",
    645: "Olafstead",
    676: "Catacombs of Kathandrax (outpost)",
    857: "Embark Beach",
}

# =============================================================================
# Dungeon metadata — which outpost to travel to + the level map IDs
# =============================================================================

DUNGEONS: dict[str, dict] = {
    "Bogroot Growths": {
        "entry_outpost_map_id": 638,
        "entry_outpost_name": "Gadd's Encampment",
        "level_map_ids": [615, 616],
    },
    "Arachni's Haunt": {
        "entry_outpost_map_id": 640,
        "entry_outpost_name": "Rata Sum",
        "entry_explorable_map_id": 569,
        "level_map_ids": [584, 585],
    },
    "Raven's Point": {
        "entry_outpost_map_id": 645,
        "entry_outpost_name": "Olafstead",
        "entry_explorable_map_id": 553,
        "level_map_ids": [617, 618, 619],
    },
    "Catacombs of Kathandrax": {
        "entry_outpost_map_id": 676,
        "entry_explorable_map_id": 569,
        "level_map_ids": [570, 571],
    },
    "Rragar's Menagerie": {
        "level_map_ids": [573],
    },
    "Frostmaw's Burrows": {
        "level_map_ids": [630, 631, 632, 633, 634],
    },
}


# =============================================================================
# Lookup helpers (exported as LLM tools)
# =============================================================================

def get_recipe(consumable_model_id: int) -> dict:
    """Return recipe details for a consumable, or an error dict if unknown."""
    recipe = CONSUMABLE_RECIPES.get(consumable_model_id)
    if recipe is None:
        return {
            "error": "unknown_consumable",
            "model_id": consumable_model_id,
            "known_consumable_model_ids": sorted(CONSUMABLE_RECIPES.keys()),
        }
    return {"success": True, "recipe": recipe}


def get_outpost_info(map_id: int) -> dict:
    """Return NPC locations (trader, chest, crafters, merchant) for an outpost."""
    info = OUTPOST_NPCS.get(map_id)
    if info is None:
        # Fall back to partial info derivable from the other tables
        partial: dict = {}
        if map_id in MAP_NAMES:
            partial["name"] = MAP_NAMES[map_id]
        if map_id in MATERIAL_TRADER_NPC_MODEL_BY_MAP:
            partial["material_trader_npc_model_id"] = MATERIAL_TRADER_NPC_MODEL_BY_MAP[map_id]
            partial["note"] = (
                "Coords not hardcoded for this outpost — locate the NPC by "
                "scanning agents with matching model and allegiance=6"
            )
        if partial:
            return {"success": True, "partial": True, "info": partial}
        return {
            "error": "unknown_outpost",
            "map_id": map_id,
            "known_outpost_map_ids": sorted(OUTPOST_NPCS.keys()),
        }
    return {"success": True, "info": info}


def get_material_info(model_id: int) -> dict:
    """Return the human name of a material model_id."""
    name = MATERIAL_NAMES.get(model_id)
    if name is None:
        return {
            "error": "unknown_material",
            "model_id": model_id,
        }
    return {"success": True, "model_id": model_id, "name": name}


def get_dungeon_info(name: str) -> dict:
    """Return entry outpost + level map IDs for a dungeon by name."""
    entry = DUNGEONS.get(name)
    if entry is None:
        return {
            "error": "unknown_dungeon",
            "known_dungeons": sorted(DUNGEONS.keys()),
        }
    out = {"success": True, "name": name, **entry}
    if "entry_outpost_map_id" in entry:
        out["entry_outpost_name"] = MAP_NAMES.get(
            entry["entry_outpost_map_id"], entry.get("entry_outpost_name", "?"))
    return out
