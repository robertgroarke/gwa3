"""OpenAI function-calling tool definitions for gwa3 game actions."""


def _tool(name: str, description: str, parameters: dict) -> dict:
    """Helper to build an OpenAI-format tool definition."""
    return {
        "type": "function",
        "function": {
            "name": name,
            "description": description,
            "parameters": {
                "type": "object",
                "properties": parameters.get("properties", {}),
                "required": parameters.get("required", []),
            },
        },
    }


# --- Movement ---

MOVE_TO = _tool(
    "move_to",
    "Move the character to a position on the map. The character will pathfind to the destination.",
    {
        "properties": {
            "x": {"type": "number", "description": "X coordinate"},
            "y": {"type": "number", "description": "Y coordinate"},
        },
        "required": ["x", "y"],
    },
)

CHANGE_TARGET = _tool(
    "change_target",
    "Change the current target to the specified agent.",
    {
        "properties": {
            "agent_id": {"type": "integer", "description": "Agent ID to target"},
        },
        "required": ["agent_id"],
    },
)

CANCEL_ACTION = _tool(
    "cancel_action",
    "Cancel the current action (movement, attack, skill cast).",
    {"properties": {}, "required": []},
)

# --- Combat ---

ATTACK = _tool(
    "attack",
    "Attack the specified agent. Must be a living, alive enemy.",
    {
        "properties": {
            "agent_id": {"type": "integer", "description": "Agent ID to attack"},
        },
        "required": ["agent_id"],
    },
)

CALL_TARGET = _tool(
    "call_target",
    "Call the specified agent as a target for the party.",
    {
        "properties": {
            "agent_id": {"type": "integer", "description": "Agent ID to call"},
        },
        "required": ["agent_id"],
    },
)

USE_SKILL = _tool(
    "use_skill",
    "Use a skill from the skillbar. Slot is 0-7. Optionally target a specific agent.",
    {
        "properties": {
            "slot": {"type": "integer", "description": "Skill slot (0-7)"},
            "target_agent_id": {
                "type": "integer",
                "description": "Target agent ID (0 for self/no target)",
            },
        },
        "required": ["slot"],
    },
)

USE_HERO_SKILL = _tool(
    "use_hero_skill",
    "Command a hero to use a skill. Hero index is 1-7, slot is 0-7.",
    {
        "properties": {
            "hero_index": {"type": "integer", "description": "Hero index (1-7)"},
            "slot": {"type": "integer", "description": "Skill slot (0-7)"},
            "target_agent_id": {
                "type": "integer",
                "description": "Target agent ID (0 for default)",
            },
        },
        "required": ["hero_index", "slot"],
    },
)

# --- Interaction ---

INTERACT_NPC = _tool(
    "interact_npc",
    "Interact with an NPC (talk, open dialog).",
    {
        "properties": {
            "agent_id": {"type": "integer", "description": "NPC agent ID"},
        },
        "required": ["agent_id"],
    },
)

INTERACT_PLAYER = _tool(
    "interact_player",
    "Interact with another player.",
    {
        "properties": {
            "agent_id": {"type": "integer", "description": "Player agent ID"},
        },
        "required": ["agent_id"],
    },
)

INTERACT_SIGNPOST = _tool(
    "interact_signpost",
    "Interact with a signpost or gadget (dungeon entrance, chest, etc.).",
    {
        "properties": {
            "agent_id": {"type": "integer", "description": "Signpost/gadget agent ID"},
        },
        "required": ["agent_id"],
    },
)

DIALOG = _tool(
    "dialog",
    "Click a dialog button by its ID (for NPC conversations, quest accept/reward).",
    {
        "properties": {
            "dialog_id": {"type": "integer", "description": "Dialog button ID"},
        },
        "required": ["dialog_id"],
    },
)

# --- Party/Hero ---

ADD_HERO = _tool(
    "add_hero",
    "Add a hero to the party by hero ID.",
    {
        "properties": {
            "hero_id": {"type": "integer", "description": "Hero ID to add"},
        },
        "required": ["hero_id"],
    },
)

KICK_HERO = _tool(
    "kick_hero",
    "Remove a hero from the party by hero ID.",
    {
        "properties": {
            "hero_id": {"type": "integer", "description": "Hero ID to remove"},
        },
        "required": ["hero_id"],
    },
)

KICK_ALL_HEROES = _tool(
    "kick_all_heroes",
    "Remove all heroes from the party.",
    {"properties": {}, "required": []},
)

FLAG_HERO = _tool(
    "flag_hero",
    "Flag a specific hero to a position. Hero index is 1-7.",
    {
        "properties": {
            "hero_index": {"type": "integer", "description": "Hero index (1-7)"},
            "x": {"type": "number", "description": "X coordinate"},
            "y": {"type": "number", "description": "Y coordinate"},
        },
        "required": ["hero_index", "x", "y"],
    },
)

FLAG_ALL = _tool(
    "flag_all",
    "Flag all heroes and henchmen to a position.",
    {
        "properties": {
            "x": {"type": "number", "description": "X coordinate"},
            "y": {"type": "number", "description": "Y coordinate"},
        },
        "required": ["x", "y"],
    },
)

UNFLAG_ALL = _tool(
    "unflag_all",
    "Remove all hero flags, allowing heroes to follow normally.",
    {"properties": {}, "required": []},
)

SET_HERO_BEHAVIOR = _tool(
    "set_hero_behavior",
    "Set a hero's AI behavior. 0=fight, 1=guard, 2=avoid combat.",
    {
        "properties": {
            "hero_index": {"type": "integer", "description": "Hero index (1-7)"},
            "behavior": {
                "type": "integer",
                "description": "Behavior mode: 0=fight, 1=guard, 2=avoid",
            },
        },
        "required": ["hero_index", "behavior"],
    },
)

LOCK_HERO_TARGET = _tool(
    "lock_hero_target",
    "Lock a hero onto a specific target. Use target_id=0 to unlock.",
    {
        "properties": {
            "hero_index": {"type": "integer", "description": "Hero index (1-7)"},
            "target_id": {
                "type": "integer",
                "description": "Target agent ID (0 to unlock)",
            },
        },
        "required": ["hero_index", "target_id"],
    },
)

# --- Travel ---

TRAVEL = _tool(
    "travel",
    "Map travel to an outpost by map ID. Optionally specify district/region.",
    {
        "properties": {
            "map_id": {"type": "integer", "description": "Destination map ID"},
            "district": {
                "type": "integer",
                "description": "District number (0 for default)",
            },
            "region": {
                "type": "integer",
                "description": "Region number (0 for default)",
            },
        },
        "required": ["map_id"],
    },
)

ENTER_MISSION = _tool(
    "enter_mission",
    "Enter the mission/explorable area from the current outpost.",
    {"properties": {}, "required": []},
)

RETURN_TO_OUTPOST = _tool(
    "return_to_outpost",
    "Return to the last outpost from an explorable area.",
    {"properties": {}, "required": []},
)

SET_HARD_MODE = _tool(
    "set_hard_mode",
    "Enable or disable hard mode.",
    {
        "properties": {
            "enabled": {"type": "boolean", "description": "True for hard mode"},
        },
        "required": ["enabled"],
    },
)

SKIP_CINEMATIC = _tool(
    "skip_cinematic",
    "Skip the current cinematic/cutscene.",
    {"properties": {}, "required": []},
)

# --- Items ---

PICK_UP_ITEM = _tool(
    "pick_up_item",
    "Pick up a dropped item by its agent ID (not item ID).",
    {
        "properties": {
            "agent_id": {
                "type": "integer",
                "description": "Agent ID of the item on the ground",
            },
        },
        "required": ["agent_id"],
    },
)

USE_ITEM = _tool(
    "use_item",
    "Use an item from inventory by item ID.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID to use"},
        },
        "required": ["item_id"],
    },
)

EQUIP_ITEM = _tool(
    "equip_item",
    "Equip an item from inventory by item ID.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID to equip"},
        },
        "required": ["item_id"],
    },
)

DROP_ITEM = _tool(
    "drop_item",
    "Drop an item on the ground by item ID.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID to drop"},
        },
        "required": ["item_id"],
    },
)

MOVE_ITEM = _tool(
    "move_item",
    "Move an item to a specific bag and slot.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID to move"},
            "bag_id": {"type": "integer", "description": "Destination bag index"},
            "slot": {"type": "integer", "description": "Destination slot"},
        },
        "required": ["item_id", "bag_id", "slot"],
    },
)

# --- Salvage & Identify ---

IDENTIFY_ITEM = _tool(
    "identify_item",
    "Identify an unidentified item using an identification kit.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID to identify"},
            "kit_id": {"type": "integer", "description": "Identification kit item ID"},
        },
        "required": ["item_id", "kit_id"],
    },
)

SALVAGE_START = _tool(
    "salvage_start",
    "Start a salvage session: open the salvage dialog for an item using a salvage kit.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID to salvage"},
            "kit_id": {"type": "integer", "description": "Salvage kit item ID"},
        },
        "required": ["item_id", "kit_id"],
    },
)

SALVAGE_MATERIALS = _tool(
    "salvage_materials",
    "During an active salvage session, salvage for materials (common materials).",
    {"properties": {}, "required": []},
)

SALVAGE_DONE = _tool(
    "salvage_done",
    "End the current salvage session.",
    {"properties": {}, "required": []},
)

# --- Skillbar ---

LOAD_SKILLBAR = _tool(
    "load_skillbar",
    "Load a full skillbar (8 skill IDs). hero_index=0 for player, 1-7 for heroes.",
    {
        "properties": {
            "skill_ids": {
                "type": "array",
                "items": {"type": "integer"},
                "description": "Array of 8 skill IDs",
            },
            "hero_index": {
                "type": "integer",
                "description": "0=player, 1-7=hero (default 0)",
            },
        },
        "required": ["skill_ids"],
    },
)

DROP_GOLD = _tool(
    "drop_gold",
    "Drop gold coins on the ground.",
    {
        "properties": {
            "amount": {"type": "integer", "description": "Amount of gold to drop"},
        },
        "required": ["amount"],
    },
)

# --- Trade ---

BUY_MATERIALS = _tool(
    "buy_materials",
    "Buy materials from a merchant by model ID and quantity.",
    {
        "properties": {
            "model_id": {"type": "integer", "description": "Material model ID"},
            "quantity": {"type": "integer", "description": "Quantity to buy"},
        },
        "required": ["model_id", "quantity"],
    },
)

REQUEST_QUOTE = _tool(
    "request_quote",
    "Request a price quote for an item from a merchant.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID to quote"},
        },
        "required": ["item_id"],
    },
)

TRANSACT_ITEMS = _tool(
    "transact_items",
    "Execute a transaction with an NPC merchant/crafter/trader. "
    "Type: 1=merchant buy, 3=crafter buy (craft item), 11=merchant sell, "
    "12=trader buy, 13=trader sell. "
    "The item_id must come from the merchant.items list in the snapshot.",
    {
        "properties": {
            "type": {
                "type": "integer",
                "description": "Transaction type: 1=merchant buy, 3=craft, 11=sell, 12=trader buy, 13=trader sell",
            },
            "quantity": {"type": "integer", "description": "Quantity to transact"},
            "item_id": {"type": "integer", "description": "Item ID from merchant item list"},
        },
        "required": ["type", "quantity", "item_id"],
    },
)

CRAFT_ITEM = _tool(
    "craft_item",
    "Craft an item at a crafter NPC. Shorthand for transact_items with type=3. "
    "Must have the crafter window open (merchant.is_open). "
    "The item_id comes from merchant.items in the snapshot. "
    "Requires sufficient gold and materials in inventory.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID from crafter's item list"},
            "quantity": {"type": "integer", "description": "Number to craft (default 1)"},
        },
        "required": ["item_id"],
    },
)

# --- Utility ---

SEND_CHAT = _tool(
    "send_chat",
    "Send a chat message. Channel: 'all', 'team'/'party', 'guild', 'trade'.",
    {
        "properties": {
            "message": {"type": "string", "description": "Message text"},
            "channel": {
                "type": "string",
                "description": "Chat channel: all, team, guild, trade",
            },
        },
        "required": ["message", "channel"],
    },
)

RESIGN = _tool(
    "resign",
    "Resign from the current mission/explorable area. Sends /resign in chat. "
    "If all party members resign, the party returns to the outpost.",
    {"properties": {}, "required": []},
)

WAIT = _tool(
    "wait",
    "Wait for a specified number of milliseconds before the next action.",
    {
        "properties": {
            "milliseconds": {
                "type": "integer",
                "description": "Time to wait in ms",
            },
        },
        "required": ["milliseconds"],
    },
)


# All tools in a single list for passing to the LLM
ALL_TOOLS = [
    # Movement
    MOVE_TO,
    CHANGE_TARGET,
    CANCEL_ACTION,
    # Combat
    ATTACK,
    CALL_TARGET,
    USE_SKILL,
    USE_HERO_SKILL,
    # Interaction
    INTERACT_NPC,
    INTERACT_PLAYER,
    INTERACT_SIGNPOST,
    DIALOG,
    # Party/Hero
    ADD_HERO,
    KICK_HERO,
    KICK_ALL_HEROES,
    FLAG_HERO,
    FLAG_ALL,
    UNFLAG_ALL,
    SET_HERO_BEHAVIOR,
    LOCK_HERO_TARGET,
    # Travel
    TRAVEL,
    ENTER_MISSION,
    RETURN_TO_OUTPOST,
    SET_HARD_MODE,
    SKIP_CINEMATIC,
    # Items
    PICK_UP_ITEM,
    USE_ITEM,
    EQUIP_ITEM,
    DROP_ITEM,
    MOVE_ITEM,
    # Salvage & Identify
    IDENTIFY_ITEM,
    SALVAGE_START,
    SALVAGE_MATERIALS,
    SALVAGE_DONE,
    # Skillbar
    LOAD_SKILLBAR,
    # Trade & Crafting
    BUY_MATERIALS,
    REQUEST_QUOTE,
    TRANSACT_ITEMS,
    CRAFT_ITEM,
    # Utility
    SEND_CHAT,
    DROP_GOLD,
    RESIGN,
    WAIT,
]
