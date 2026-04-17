"""Full-procedure dungeon run knowledge for the Gemma-driven bot.

Each entry in DUNGEON_RUNS captures everything needed to complete a dungeon
end-to-end: entry outpost, quest gating, prep (hard mode, consumables, hero
team, skillbars), per-level spawn + key waypoints, final chest, and the
quest-reward turn-in NPC.

Source: AutoIt dungeon scripts in
`Dungeons GWA Logic Censured NEW DEC06/`. The waypoint data is a curated
subset — the scripts have dozens of fine-grained MoveTo points per level;
we keep only structurally-meaningful spots (blessings, keys, doors, boss
engagement, chest, portal). Between key_points, Gemma should use
move_to_and_wait / MovePlayerNear's stuck-detection to navigate.

The `kind` field on each key_point tells the caller what action is expected:
    "blessing"         — interact with a priest and send the blessing dialog
    "quest_accept"     — interact with quest NPC, send dialog_accept
    "quest_complete"   — interact with quest NPC after run, send dialog_complete
    "checkpoint"       — just move here (gate/door/encounter wall)
    "key"              — pick up dungeon key (often via interact_signpost)
    "door"             — door that opens after a key is carried / boss killed
    "portal"           — transition to next level; `to_level` names target
    "boss_engagement"  — where the boss spawns / fight begins
    "end_chest"        — final reward chest; interact to loot
    "interact_gadget"  — brazier / obelisk / torch; usually part of a puzzle
"""

# -----------------------------------------------------------------------------
# Common hero teams (referenced from dungeon entries to avoid repetition)
# -----------------------------------------------------------------------------

STANDARD_MERC_TEAM = [
    {"slot": 1, "name": "Xandra", "hero_id": 25, "build": "Remove Hex Rit"},
    {"slot": 2, "name": "Olias", "hero_id": 14, "build": "BiP Necro"},
    {"slot": 3, "name": "Livia", "hero_id": 21, "build": "Xinrae Monk"},
    {"slot": 4, "name": "Master of Whispers", "hero_id": 4, "build": "MM Necro"},
    {"slot": 5, "name": "Gwen", "hero_id": 24, "build": "E-Surge Mesmer"},
    {"slot": 6, "name": "Norgu", "hero_id": 15, "build": "Ineptitude Mesmer"},
    {"slot": 7, "name": "Razah", "hero_id": 1, "build": "Panic Rit/Mesmer"},
]


# -----------------------------------------------------------------------------
# DUNGEON_RUNS — primary data table
# -----------------------------------------------------------------------------

DUNGEON_RUNS: dict[str, dict] = {
    "Bogroot Growths": {
        "entry_outpost": {"map_id": 638, "name": "Gadd's Encampment"},
        "quest": {
            "name": "Tekks's War",
            "quest_id": 825,
            "giver_npc": "Tekks",
            "giver_map_id": 558,
            "giver_map_name": "Sparkfly Swamp",
            "giver_x": 12396,
            "giver_y": 22407,
            "dialog_accept": 0x833901,
            "dialog_complete": 0x833907,
            "must_accept_before_entry": True,
            "note": (
                "From Gadd's Encampment, travel to Sparkfly Swamp (map 558), "
                "walk to Tekks at the coords above, interact, send dialog "
                "0x833901 to accept. This unlocks the Bogroot entry door on L1."
            ),
        },
        "prep": {
            "hard_mode": True,
            "recommended_consumables": ["Grail of Might", "Essence of Celerity",
                                         "Armor of Salvation"],
            "hero_team": STANDARD_MERC_TEAM,
            "blessings_before_entry": [],
        },
        "levels": [
            {
                "name": "Bogroot Growths L1",
                "map_id": 615,
                "spawn": {"x": 17026, "y": 2168},
                "key_points": [
                    {"kind": "blessing", "x": 19099, "y": 7762,
                     "blessing_type": "dwarven",
                     "note": "Dwarven priest near entry"},
                    {"kind": "checkpoint", "x": 14434, "y": 8000,
                     "note": "Quest door checkpoint (Tekks gate)"},
                    {"kind": "checkpoint", "x": 672, "y": 1105,
                     "note": "Mid-level convergence point"},
                    {"kind": "portal", "x": 7665, "y": -19050,
                     "to_level": "Bogroot Growths L2",
                     "note": "Stairs down to L2"},
                ],
                "hazards": [
                    "Elder Water Djinn popups — use Winter / interrupts"
                ],
            },
            {
                "name": "Bogroot Growths L2",
                "map_id": 616,
                "spawn": {"x": -11386, "y": -3871},
                "key_points": [
                    {"kind": "key", "x": 16854, "y": -5830,
                     "note": "Pick up Dungeon Key"},
                    {"kind": "door", "x": 17925, "y": -6197,
                     "note": "Boss area door (requires key)"},
                    {"kind": "boss_engagement", "x": 18334, "y": -8838,
                     "note": "First boss engagement"},
                    {"kind": "checkpoint", "x": 14035, "y": -17800,
                     "note": "Final boss approach"},
                    {"kind": "end_chest", "x": 14876, "y": -19033,
                     "note": "Bogroot end chest (signpost). Interact to loot."},
                ],
            },
        ],
        "reward_turnin": {
            "npc_name": "Tekks",
            "map_id": 558,
            "map_name": "Sparkfly Swamp",
            "x": 12396,
            "y": 22407,
            "dialog": 0x833907,
            "note": (
                "After looting the chest, return to Sparkfly Swamp (via map "
                "travel) and speak with Tekks to turn in Tekks's War for the "
                "quest reward."
            ),
        },
    },

    "Arachni's Haunt": {
        "entry_outpost": {"map_id": 640, "name": "Rata Sum"},
        "quest": {
            "name": "Hixx's Quest",
            "giver_npc": "Hixx",
            "giver_map_id": 569,
            "giver_map_name": "Magus Stones",
            "giver_x": -10150,
            "giver_y": -17087,
            "must_accept_before_entry": True,
            "note": (
                "From Rata Sum, enter Magus Stones (map 569) and find Hixx "
                "at the coords above. Accept his quest to unlock the dungeon "
                "entry portal."
            ),
        },
        "prep": {
            "hard_mode": False,
            "recommended_consumables": ["Grail of Might", "Essence of Celerity",
                                         "Armor of Salvation"],
            "hero_team": STANDARD_MERC_TEAM,
            "blessings_before_entry": ["asuran"],
            "blessing_coords_hint": {"asuran": {"x": 14862, "y": 13173,
                                                 "map_id": 640,
                                                 "note": "Asura priest in Rata Sum"}},
        },
        "levels": [
            {
                "name": "Arachni's Haunt L1",
                "map_id": 584,
                "spawn": None,  # not hardcoded in the AutoIt
                "key_points": [
                    {"kind": "interact_gadget", "note": "Asura Flame Staff pickups — required to light braziers in sub-zones L1_3 and L1_4"},
                    {"kind": "interact_gadget", "note": "Brazier lighting sequence (5 spider eggs each in L1_3 and L1_4)"},
                    {"kind": "door", "x": 2065, "y": 19738,
                     "note": "Dungeon door after braziers lit"},
                    {"kind": "portal", "to_level": "Arachni's Haunt L2"},
                ],
                "hazards": [
                    "Flame Staff mechanic — drop flame to light braziers; staff must be carried to progress"
                ],
            },
            {
                "name": "Arachni's Haunt L2",
                "map_id": 585,
                "key_points": [
                    {"kind": "interact_gadget", "note": "Spider web traps — avoid / clear"},
                    {"kind": "end_chest", "x": -17131, "y": 11601,
                     "note": "Final boss room + end chest"},
                ],
            },
        ],
        "reward_turnin": {
            "npc_name": "Hixx",
            "map_id": 569,
            "x": -10150,
            "y": -17087,
            "note": "Return to Magus Stones and speak with Hixx",
        },
    },

    "Raven's Point": {
        "entry_outpost": {"map_id": 645, "name": "Olafstead"},
        "quest": {
            "name": "Duncan's Dispatch",
            "giver_npc": "Duncan",
            "giver_map_id": 553,
            "giver_map_name": "Varajar Fells",
            "giver_x": -15526,
            "giver_y": 8811,
            "must_accept_before_entry": True,
        },
        "prep": {
            "hard_mode": False,
            "recommended_consumables": ["Grail of Might", "Essence of Celerity",
                                         "Armor of Salvation"],
            "hero_team": STANDARD_MERC_TEAM,
            "blessings_before_entry": ["norn"],
            "blessing_coords_hint": {"norn": {"x": -2034, "y": -4512,
                                               "map_id": 645,
                                               "note": "Norn priest in Olafstead"}},
        },
        "levels": [
            {
                "name": "Raven's Point L1",
                "map_id": 617,
                "spawn": {"x": -17536, "y": -14257},
                "key_points": [
                    {"kind": "interact_gadget", "x": -17536, "y": -14257,
                     "note": "First torch pickup — light torches to progress"},
                    {"kind": "interact_gadget", "x": -6255, "y": 2870,
                     "note": "Second torch pickup"},
                    {"kind": "key", "x": -6174, "y": 6594,
                     "note": "Dungeon key pickup"},
                    {"kind": "door", "x": -15612, "y": 6094,
                     "note": "Key door"},
                    {"kind": "portal", "to_level": "Raven's Point L2"},
                ],
                "hazards": ["Torch mechanic — torches must be carried to braziers"],
            },
            {
                "name": "Raven's Point L2",
                "map_id": 618,
                "key_points": [
                    {"kind": "interact_gadget", "note": "Three torch/brazier sequences"},
                    {"kind": "key", "x": -129581, "y": 15239,
                     "note": "Dungeon key pickup"},
                    {"kind": "door", "x": 3686, "y": 12128,
                     "note": "Boss lock door"},
                    {"kind": "portal", "to_level": "Raven's Point L3"},
                ],
            },
            {
                "name": "Raven's Point L3",
                "map_id": 619,
                "key_points": [
                    {"kind": "boss_engagement", "x": 12111, "y": 9604,
                     "note": "Duncan the Black — final boss, looping waypoints around this area"},
                    {"kind": "end_chest", "x": 12111, "y": 9604,
                     "note": "End chest near Duncan's body"},
                ],
            },
        ],
        "reward_turnin": {
            "npc_name": "Duncan",
            "map_id": 553,
            "x": -15526,
            "y": 8811,
            "note": "Return to Varajar Fells and speak with Duncan",
        },
    },

    "Catacombs of Kathandrax": {
        "entry_outpost": {"map_id": 648, "name": "Doomlore Shrine"},
        "quest": {
            "name": "Nya's Quest",
            "giver_npc": "Nya",
            "giver_map_id": 644,
            "giver_map_name": "Sacnoth Valley",
            "giver_x": 18329,
            "giver_y": -18134,
        },
        "prep": {
            "hard_mode": False,
            "hero_team": STANDARD_MERC_TEAM,
            "blessings_before_entry": ["dwarven", "vanguard"],
        },
        "levels": [
            {
                "name": "Catacombs of Kathandrax L1",
                "map_id": 570,
                "spawn": {"x": 17853, "y": -18048},
                "key_points": [
                    {"kind": "key", "x": 3684, "y": 2790, "note": "Dungeon key"},
                    {"kind": "door", "x": -6547, "y": -1781, "note": "Boss lock"},
                    {"kind": "portal", "to_level": "Catacombs of Kathandrax L2"},
                ],
            },
            {
                "name": "Catacombs of Kathandrax L2",
                "map_id": 571,
                "spawn": {"x": 17201, "y": -2324},
                "key_points": [
                    {"kind": "key", "x": -66, "y": -11246, "note": "Dungeon key"},
                    {"kind": "door", "x": -6481, "y": -9618, "note": "Boss lock"},
                    {"kind": "portal", "to_level": "Catacombs of Kathandrax L3"},
                ],
            },
            {
                "name": "Catacombs of Kathandrax L3",
                "map_id": 572,
                "spawn": {"x": -16944, "y": 10046},
                "key_points": [
                    {"kind": "key", "x": -10963, "y": 12814, "note": "Dungeon key"},
                    {"kind": "boss_engagement",
                     "note": "Kathandrax — final boss, looping waypoints"},
                    {"kind": "end_chest", "x": -239, "y": -583,
                     "note": "End chest"},
                ],
            },
        ],
        "reward_turnin": {
            "npc_name": "Nya",
            "map_id": 644,
            "x": 18329,
            "y": -18134,
        },
    },

    "Frostmaw's Burrows": {
        "entry_outpost": {"map_id": 643, "name": "Sifhalla"},
        "quest": {
            "name": "Latham's Quest",
            "giver_npc": "Latham",
            "giver_map_id": 621,
            "giver_map_name": "Jaga Moraine",
            "giver_x": 1012,
            "giver_y": 25505,
        },
        "prep": {
            "hard_mode": False,
            "hero_team": STANDARD_MERC_TEAM,
            "blessings_before_entry": ["norn", "dwarven"],
        },
        "levels": [
            {"name": "Frostmaw's Burrows L1", "map_id": 630,
             "spawn": {"x": -16298, "y": 18052},
             "key_points": [
                {"kind": "blessing", "x": -16179, "y": 17596,
                 "blessing_type": "dwarven"},
                {"kind": "portal", "x": -10760, "y": 10900,
                 "to_level": "Frostmaw's Burrows L2"},
             ]},
            {"name": "Frostmaw's Burrows L2", "map_id": 631,
             "spawn": {"x": 19252, "y": -3513},
             "key_points": [
                {"kind": "blessing", "x": 19072, "y": -3047,
                 "blessing_type": "dwarven"},
                {"kind": "portal", "x": 13875, "y": -19445,
                 "to_level": "Frostmaw's Burrows L3"},
             ]},
            {"name": "Frostmaw's Burrows L3", "map_id": 632,
             "spawn": {"x": -18707, "y": 9639},
             "key_points": [
                {"kind": "blessing", "x": 18540, "y": 9962,
                 "blessing_type": "dwarven"},
                {"kind": "portal", "x": 17887, "y": 15830,
                 "to_level": "Frostmaw's Burrows L4"},
             ]},
            {"name": "Frostmaw's Burrows L4", "map_id": 633,
             "spawn": {"x": -15181, "y": 16464},
             "key_points": [
                {"kind": "blessing", "x": -13752, "y": 16820,
                 "blessing_type": "dwarven"},
                {"kind": "portal", "to_level": "Frostmaw's Burrows L5"},
             ]},
            {"name": "Frostmaw's Burrows L5", "map_id": 634,
             "key_points": [
                {"kind": "boss_engagement",
                 "note": "Remnant of Antiquities — final boss"},
                {"kind": "end_chest", "note": "End chest after boss"},
             ]},
        ],
        "reward_turnin": {"npc_name": "Latham", "map_id": 621,
                          "x": 1012, "y": 25505},
    },

    "Rragar's Menagerie": {
        "entry_outpost": {"map_id": 648, "name": "Doomlore Shrine"},
        "quest": {
            "name": "Gron's Quest",
            "giver_npc": "Gron",
            "giver_map_id": 648,
            "giver_map_name": "Doomlore Shrine",
            "giver_x": -19166,
            "giver_y": 17980,
        },
        "prep": {
            "hard_mode": False,
            "hero_team": STANDARD_MERC_TEAM,
            "blessings_before_entry": ["vanguard"],
        },
        "levels": [
            {
                "name": "Rragar's Menagerie L1",
                "map_id": 573,
                "spawn": {"x": 4705, "y": -18700},
                "key_points": [
                    {"kind": "blessing", "x": 3975, "y": -18471,
                     "blessing_type": "vanguard"},
                    {"kind": "blessing", "x": -11685, "y": -12242,
                     "blessing_type": "vanguard"},
                    {"kind": "key", "x": 10292, "y": -3916, "note": "Dungeon key"},
                    {"kind": "door", "x": -16800, "y": -949, "note": "Boss lock"},
                    {"kind": "portal", "to_level": "Rragar's Menagerie L2"},
                ],
            },
            {
                "name": "Rragar's Menagerie L2",
                "map_id": 574,
                "spawn": {"x": 17891, "y": 15602},
                "key_points": [
                    {"kind": "blessing", "x": 18111, "y": 16274,
                     "blessing_type": "vanguard"},
                    {"kind": "blessing", "x": -6826, "y": -2666,
                     "blessing_type": "vanguard"},
                    {"kind": "key", "x": -11969, "y": -2882, "note": "Dungeon key"},
                    {"kind": "door", "x": 10605, "y": -15736, "note": "Boss lock"},
                    {"kind": "portal", "to_level": "Rragar's Menagerie L3"},
                ],
            },
            {
                "name": "Rragar's Menagerie L3",
                "map_id": 575,
                "spawn": {"x": 18445, "y": 15811},
                "key_points": [
                    {"kind": "blessing", "x": 18871, "y": 16634,
                     "blessing_type": "vanguard"},
                    {"kind": "blessing", "x": 4258, "y": -751,
                     "blessing_type": "vanguard"},
                    {"kind": "blessing", "x": -7268, "y": 17252,
                     "blessing_type": "vanguard"},
                    {"kind": "key", "x": 1294, "y": -9648, "note": "Dungeon key"},
                    {"kind": "door", "x": 534, "y": -375, "note": "Blast door"},
                    {"kind": "door", "x": -6925, "y": 13984, "note": "Boss lock"},
                    {"kind": "end_chest", "x": 130, "y": 17437,
                     "note": "Hidesplitter's Chest"},
                ],
            },
        ],
        "reward_turnin": {"npc_name": "Gron", "map_id": 648,
                          "x": -19166, "y": 17980},
    },
}


def get_dungeon_run(name: str) -> dict:
    """Return the full run procedure for a dungeon by display name."""
    # case-insensitive + apostrophe-forgiving lookup
    norm = lambda s: s.lower().replace("'", "").replace("'", "")
    for display_name, data in DUNGEON_RUNS.items():
        if norm(display_name) == norm(name):
            return {"success": True, "name": display_name, **data}
    return {
        "error": "unknown_dungeon",
        "query": name,
        "known_dungeons": sorted(DUNGEON_RUNS.keys()),
    }
