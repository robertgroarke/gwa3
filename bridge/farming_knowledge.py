"""Hand-curated farming knowledge for the Gemma-driven bot.

Unlike `gamedata.py` (auto-generated from AutoIt enums), this module contains
*gameplay* knowledge that the bot can't derive from snapshots alone — which NPC
crafts which consumable, which materials a recipe takes, where the material
trader and Xunlai chest are in each outpost, etc.

Exposed via the `get_recipe` and `get_outpost_info` tools so the LLM can
look things up on demand instead of having them all baked into the system
prompt.

Sources:
  - Consumable recipes: crafter NPC dialog in game
  - Material trader NPC IDs per map: `GWA Censured/lib/Utils-Maintenance.au3`
    `GetMaterialTrader()` table
  - Xunlai chest coords (guild halls): `Chest()` function in the same AutoIt lib
  - Outpost NPC coords: observed in game
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
        "crafter_name": "Alcus Nailbiter",
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
            "x": 2865.0,
            "y": -2406.0,
        },
        "xunlai_chest": {"x": 2283.0, "y": -2134.0},
        "merchant": {"name": "Ozem", "x": 2233.0, "y": -2009.0},
        "crafters": [
            {"name": "Eyja", "x": 3336.0, "y": 627.0,
             "crafts": ["Grail of Might"]},
            {"name": "Kwat", "x": 3596.0, "y": 107.0,
             "crafts": ["Essence of Celerity"]},
            {"name": "Alcus Nailbiter", "x": 3704.0, "y": -163.0,
             "crafts": ["Armor of Salvation"]},
            {"name": "Edwin", "x": 3515.0, "y": 369.0,
             "crafts": ["Powerstone of Courage", "Scroll of Resurrection"]},
        ],
    },
    638: {  # Gadd's Encampment — Bogroot Growths entry + material stop
        "name": "Gadd's Encampment",
        "material_trader": {
            "x": -9097.0, "y": -23353.0,
            "npc_model_id": 6763,
        },
        "rare_material_trader": {"x": -9136.0, "y": -23153.0},
        "xunlai_chest": {"x": -10481.0, "y": -22787.0},
        "merchant": {"x": -8374.0, "y": -22491.0},
    },
    642: {  # Eye of the North
        "name": "Eye of the North",
        "merchant": {"x": -2700.0, "y": 1075.0},
    },
    640: {"name": "Rata Sum",
          "material_trader": {"npc_model_id": 6764}},
    645: {"name": "Olafstead",
          "material_trader": {"npc_model_id": 6050}},
    641: {"name": "Sunspear Great Hall",
          "material_trader": {"npc_model_id": 6065}},
    # Sifhalla + Doomlore Shrine don't have hardcoded NPC data in the AutoIt
    # scripts — they're used as entry outposts only. Name-resolution lives in
    # MAP_NAMES; they're omitted from OUTPOST_NPCS to keep that table focused
    # on outposts with actionable NPC data.
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
    572: "Catacombs of Kathandrax L3",
    573: "Rragar's Menagerie L1",
    574: "Rragar's Menagerie L2",
    575: "Rragar's Menagerie L3",
    584: "Arachni's Haunt L1",
    585: "Arachni's Haunt L2",
    615: "Bogroot Growths L1",
    616: "Bogroot Growths L2",
    617: "Raven's Point L1",
    618: "Raven's Point L2",
    619: "Raven's Point L3",
    621: "Jaga Moraine",
    630: "Frostmaw's Burrows L1",
    631: "Frostmaw's Burrows L2",
    632: "Frostmaw's Burrows L3",
    633: "Frostmaw's Burrows L4",
    634: "Frostmaw's Burrows L5",
    638: "Gadd's Encampment",
    640: "Rata Sum",
    641: "Sunspear Great Hall",
    642: "Eye of the North",
    643: "Sifhalla",
    644: "Sacnoth Valley",
    645: "Olafstead",
    648: "Doomlore Shrine",
    650: "Longeye's Ledge",
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
        "entry_outpost_name": "Central Transfer Chamber",
        "entry_explorable_map_id": 569,
        "level_map_ids": [570, 571],
    },
    "Rragar's Menagerie": {
        "entry_outpost_map_id": 648,
        "entry_outpost_name": "Doomlore Shrine",
        "level_map_ids": [573],
    },
    "Frostmaw's Burrows": {
        "entry_outpost_map_id": 643,
        "entry_outpost_name": "Sifhalla",
        "level_map_ids": [630, 631, 632, 633, 634],
    },
}


# =============================================================================
# Blessing NPC interaction patterns
# =============================================================================
# All blessings use the native "talk to priest" dialog pattern — walk to the
# blessing NPC, interact, then send dialog 0x84 (or 0x85 for Lightbringer /
# second-stage Sunspear). After sending dialog, verify receipt by checking
# the player's effects for one of the effect_ids listed below.
#
# Coords for most blessing NPCs are NOT hardcoded in the AutoIt scripts —
# they pass coords as arguments so the bot resolves them dynamically each
# run. Known explicit-coord blessings are listed under `known_locations`.

BLESSINGS: dict[str, dict] = {
    "asuran": {
        "name": "Asuran Blessing",
        "dialog_codes": [0x84],
        "effect_ids": [2434, 2435, 2436, 2481, 2548],
        "description": "Asura-rep blessing granted by Asura priests in "
                       "EotN outposts like Rata Sum and Central Transfer "
                       "Chamber.",
        "known_locations": [],
    },
    "norn": {
        "name": "Norn Blessing",
        "dialog_codes": [0x84],
        "effect_ids": [2469, 2470, 2471, 2472],
        "description": "Norn-rep blessing granted by Norn priests in "
                       "EotN outposts like Olafstead, Sifhalla.",
        "known_locations": [],
    },
    "dwarven": {
        "name": "Dwarven Blessing",
        "dialog_codes": [0x84],
        "effect_ids": [2445, 2446, 2447, 2448, 2549,
                       2565, 2566, 2567, 2568],
        "description": "Deldrimor-rep blessing granted by Dwarven priests in "
                       "EotN outposts like Doomlore Shrine, Central Transfer "
                       "Chamber, Longeye's Ledge.",
        "known_locations": [
            {"map_id": 631, "map_name": "Frostmaw's Burrows L2",
             "x": -11132.0, "y": -5546.0,
             "note": "Called via GetDwarvenBlessing in the Frostmaws script"},
        ],
    },
    "vanguard": {
        "name": "Ebon Vanguard Blessing",
        "dialog_codes": [0x84],
        "effect_ids": [2457, 2458, 2459, 2460],
        "description": "Vanguard-rep blessing granted by Ebon Vanguard "
                       "priests in EotN outposts like Eye of the North, "
                       "Longeye's Ledge.",
        "known_locations": [],
    },
    "sunspears": {
        "name": "Sunspear Blessing",
        "dialog_codes": [0x84, 0x85],
        "effect_ids": [1790, 1791, 1792, 1793, 1794, 1795, 1796],
        "description": "Sunspear-rep blessing from Sunspear priests in "
                       "Nightfall outposts. Send 0x84 first, then 0x85 to "
                       "confirm the blessing choice.",
        "known_locations": [],
    },
    "lightbringer": {
        "name": "Lightbringer Blessing",
        "dialog_codes": [0x85],
        "effect_ids": [1898, 1831, 1844, 1845, 1846,
                       1847, 1848, 1849, 1850, 1851],
        "description": "Lightbringer-rep blessing from priests in Realm of "
                       "Torment / Throne of Secrets outposts.",
        "known_locations": [],
    },
}


# =============================================================================
# Mercenary hero skillbar templates
# =============================================================================
# Base64-encoded skillbar templates captured from the Froggy HM dungeon script
# hero-loadout section. Gemma can pass these directly to the `load_skillbar`
# action if she wants to set up a standard merc team. `hero_id` is the value
# to pass to `add_hero` / `kick_hero`.

HERO_BUILDS: dict[str, dict] = {
    "Xandra": {
        "hero_id": 25,
        "profession": "Ritualist",
        "role": "Remove Hex / Communing",
        "skillbar_template": "OAOiAyk8gNtePuwJ00ZaNbJA",
        "variants": {
            "shields_up": "OAGjUhgMpOYTr3jLcCNdmWz3CA",
            "rragars": "OAOiAyk8gNtehzHH0E56MbJA",
        },
    },
    "Olias": {
        "hero_id": 14,
        "profession": "Necromancer",
        "role": "Blood is Power (BiP) battery",
        "skillbar_template": "OAhjQkGZIT3BVVCPSTTODTjTciA",
    },
    "Livia": {
        "hero_id": 21,
        "profession": "Ritualist",
        "role": "Xinrae resto healer",
        "skillbar_template": "OAhjYoHYIPWb7wnoqKNncDzqH",
        "variants": {
            "resto": "OAhjYoHYIPWb7wnoqKNncDzqHA",
        },
    },
    "Master of Whispers": {
        "hero_id": 4,
        "profession": "Necromancer",
        "role": "Minion Master",
        "skillbar_template": "OAljUwGpZSUBKgfBVVbh8Y7Y1YA",
        "variants": {
            "mm_charge_shields_up": "OAFTUYTWTiKQB8LoqaLktgr4tAA",
        },
    },
    "Gwen": {
        "hero_id": 24,
        "profession": "Mesmer",
        "role": "E-Surge",
        "skillbar_template": "OQhkAsC8gFKzJY6lDMd40hQG4iB",
        "variants": {
            "rragars_catacombs": "OQhkAsC8gFKyJM95ggb6DRGcxA",
        },
    },
    "Norgu": {
        "hero_id": 15,
        "profession": "Mesmer",
        "role": "Ineptitude",
        "skillbar_template": "OQhkAsC8gFKDNY6lDMd40hQG4iB",
    },
    "Razah": {
        "hero_id": 1,
        "profession": "Ritualist",
        "role": "Panic (Mesmer secondary)",
        "skillbar_template": "OQljAkBsZSvAIg5ZkAcQsA7Y1YA",
    },
    "Dunham": {
        "hero_id": None,  # mercenary hero — slot varies per account
        "profession": "Mesmer",
        "role": "Ineptitude with Shields Up",
        "skillbar_template": "OQFUAyAPmaSvAIg5ZkAcQsAcFvFA",
        "note": "Merc hero — specific hero_id depends on the player's "
                "Dunham template slot. Check AgentMgr for a hero named Dunham.",
    },
}

# All heroes default to aggression mode 1 (Guard) in the reference scripts.
HERO_DEFAULT_AGGRESSION = 1


# =============================================================================
# Quest givers and quest metadata (partial — only what's hardcoded)
# =============================================================================
# Each entry maps either a quest name or quest_id to giver NPC + coords.

QUESTS: dict[str, dict] = {
    "Tekks's War": {
        "quest_id": 825,
        "giver_npc_name": "Tekks",
        "giver_map_id": 558,
        "giver_map_name": "Sparkfly Swamp",
        "giver_x": 12396.0,
        "giver_y": 22407.0,
        "dialog_accept": 0x833901,
        "dialog_complete": 0x833907,
        "quest_handle": 0x339,
        "description": "Unlocks Bogroot Growths dungeon entry. Accept in "
                       "Sparkfly Swamp (map 558), then enter the dungeon.",
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
    """Return the full dungeon run procedure by name.

    Merges the detailed per-dungeon run data from `dungeon_runs.py` (entry
    outpost, quest, prep, level waypoints, chest, reward turn-in) with the
    short summary from DUNGEONS (map IDs) so callers always get the richest
    available info for the given name.
    """
    # Import locally to avoid a circular-import if dungeon_runs ever grows
    # and starts importing from farming_knowledge.
    from . import dungeon_runs

    rich = dungeon_runs.get_dungeon_run(name)
    if rich.get("success"):
        # Pull in the terse summary too (level_map_ids, etc.) if present.
        summary = DUNGEONS.get(name) or DUNGEONS.get(rich["name"])
        if summary:
            rich["level_map_ids"] = summary.get("level_map_ids", [])
        return rich

    # Fall back to the legacy terse summary if rich data isn't available.
    entry = DUNGEONS.get(name)
    if entry is None:
        return {
            "error": "unknown_dungeon",
            "known_dungeons": sorted(
                set(DUNGEONS.keys()) | set(dungeon_runs.DUNGEON_RUNS.keys())),
        }
    out = {"success": True, "name": name, **entry}
    if "entry_outpost_map_id" in entry:
        out["entry_outpost_name"] = MAP_NAMES.get(
            entry["entry_outpost_map_id"], entry.get("entry_outpost_name", "?"))
    return out


def get_blessing_info(blessing_type: str) -> dict:
    """Return how to get a blessing: dialog codes, effect IDs, known NPC locations."""
    key = blessing_type.lower().strip()
    info = BLESSINGS.get(key)
    if info is None:
        return {
            "error": "unknown_blessing",
            "blessing_type": blessing_type,
            "known_blessings": sorted(BLESSINGS.keys()),
        }
    return {"success": True, "blessing_type": key, "info": info}


def get_hero_build(hero_name: str) -> dict:
    """Return the standard Mercenary skillbar template for a hero by name."""
    # case-insensitive lookup
    for name, build in HERO_BUILDS.items():
        if name.lower() == hero_name.lower():
            return {"success": True, "hero_name": name, "build": build,
                    "default_aggression": HERO_DEFAULT_AGGRESSION}
    return {
        "error": "unknown_hero",
        "hero_name": hero_name,
        "known_heroes": sorted(HERO_BUILDS.keys()),
    }


def get_quest_info(key) -> dict:
    """Look up a quest by name (str) or quest_id (int)."""
    # Try by name first
    if isinstance(key, str):
        info = QUESTS.get(key)
        if info:
            return {"success": True, "quest_name": key, "info": info}
    # Fall back to quest_id search
    if isinstance(key, int) or (isinstance(key, str) and key.isdigit()):
        qid = int(key)
        for name, info in QUESTS.items():
            if info.get("quest_id") == qid:
                return {"success": True, "quest_name": name, "info": info}
    return {
        "error": "unknown_quest",
        "key": key,
        "known_quests": list(QUESTS.keys()),
    }
