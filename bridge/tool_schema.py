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

AGGRO_MOVE_TO = _tool(
    "aggro_move_to",
    "Walk to a position AND fight anything that enters fight_range along "
    "the way. Internally runs Froggy's AggroMoveToEx -> "
    "FightEnemiesInAggro loop: when an enemy is in aggro it picks the "
    "best target, calls native Attack, and cycles your skillbar via "
    "UseSkillsInSlotOrder (so the PLAYER casts skills too — not just "
    "heroes). It also re-issues the move on interrupt and sidesteps on "
    "stuck detection. This is a long, blocking walk-and-fight (can take "
    "minutes). Prefer it over plain move_to for any explorable segment "
    "with live enemies. fight_range defaults to 1350 units.",
    {
        "properties": {
            "x": {"type": "number", "description": "X coordinate"},
            "y": {"type": "number", "description": "Y coordinate"},
            "fight_range": {
                "type": "number",
                "description": "Combat engagement radius in game units (default 1350)",
            },
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

OPEN_MERCHANT = _tool(
    "open_merchant",
    "Open a merchant window using the proven merchant-specific GoNPC interaction cadence.",
    {
        "properties": {
            "agent_id": {"type": "integer", "description": "Merchant NPC agent ID"},
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

# --- Quest log ---

SET_ACTIVE_QUEST = _tool(
    "set_active_quest",
    "Mark a quest in the quest log as the active quest. The active quest is the "
    "one whose marker is drawn on the compass and whose objective text is shown "
    "in the tracker. Pass a quest_id visible in the latest quests.quest_log. "
    "Quest must already be in the log — this does not accept or pick up a quest.",
    {
        "properties": {
            "quest_id": {
                "type": "integer",
                "description": "Quest ID from quests.quest_log",
            },
        },
        "required": ["quest_id"],
    },
)

ABANDON_QUEST = _tool(
    "abandon_quest",
    "Abandon (remove) a quest from the quest log. Irreversible — the quest "
    "must be re-accepted from its giver NPC if you change your mind. "
    "quest_id must be present in the current quests.quest_log (snapshot).",
    {
        "properties": {
            "quest_id": {
                "type": "integer",
                "description": "Quest ID from quests.quest_log to abandon",
            },
        },
        "required": ["quest_id"],
    },
)

REQUEST_QUEST_INFO = _tool(
    "request_quest_info",
    "Ask the server to populate full quest text (description + objectives) "
    "for a quest that is in the log but has not been inspected yet. After "
    "this fires, a subsequent snapshot usually carries the populated text "
    "on quests.active_quest (for the active one).",
    {
        "properties": {
            "quest_id": {
                "type": "integer",
                "description": "Quest ID from quests.quest_log to fetch info for",
            },
        },
        "required": ["quest_id"],
    },
)

OPEN_QUEST_LOG = _tool(
    "open_quest_log",
    "Toggle the in-game Quest Log window (same effect as pressing 'L' "
    "in-game). Primarily useful as a side-effect: opening the window "
    "makes GW render decoded quest-name text to UI label frames, which "
    "the snapshot reader can then pick up. Calling it again closes the "
    "window. No parameters.",
    {"properties": {}, "required": []},
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

OPEN_XUNLAI = _tool(
    "open_xunlai",
    "Open the Xunlai storage chest via the raw GoNPC (INTERACT_NPC) "
    "packet. Call this on a nearby Xunlai Jingwei agent BEFORE firing "
    "withdraw_gold / deposit_gold or any MoveItem to/from storage bags "
    "— CHANGE_GOLD and storage MoveItem packets are only server-legal "
    "for a short window (~15s) after this GoNPC lands, and firing them "
    "otherwise disconnects the client with Code=007. The open sequence "
    "also auto-closes the UI dialog (it disrupts player agent reads).",
    {
        "properties": {
            "agent_id": {
                "type": "integer",
                "description": "Agent ID of the Xunlai Jingwei NPC",
            },
        },
        "required": ["agent_id"],
    },
)

MERCHANT_BUY = _tool(
    "merchant_buy",
    "Buy an item from an open merchant window using the native Transaction "
    "path (safe: avoids raw packet 0x4D which can crash the client). "
    "item_id must come from the current merchant.items list. quantity "
    "defaults to 1. Requires merchant.is_open == true.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID from merchant.items"},
            "quantity": {"type": "integer", "description": "Quantity to buy (default 1)"},
        },
        "required": ["item_id"],
    },
)

MERCHANT_SELL = _tool(
    "merchant_sell",
    "Sell an inventory item to an open merchant using the native "
    "Transaction path (safe). item_id is an inventory item_id. quantity "
    "is optional — 0 or omitted sells the full stack.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Inventory item_id to sell"},
            "quantity": {
                "type": "integer",
                "description": "Quantity to sell (0 = full stack, default 0)",
            },
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
    "Craft an item at a crafter NPC. Must have the crafter window open (merchant.is_open). "
    "The item_id comes from merchant.items in the snapshot. "
    "For the proven UIMessage path, provide model_id + material_model_ids + material_quantities. "
    "Without those, falls back to raw TransactItems(type=3). "
    "Requires sufficient gold and materials in inventory.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Item ID from crafter's item list"},
            "quantity": {"type": "integer", "description": "Number to craft (default 1, max 5 per call)"},
            "gold": {"type": "integer", "description": "Total gold cost (default 250 * quantity)"},
            "model_id": {"type": "integer", "description": "Model ID of item to craft (enables proven UIMessage path)"},
            "material_model_ids": {
                "type": "array",
                "items": {"type": "integer"},
                "description": "Model IDs of required materials (e.g. [948, 929] for Iron+Dust)",
            },
            "material_quantities": {
                "type": "array",
                "items": {"type": "integer"},
                "description": "Quantity of each material per craft (e.g. [50, 50])",
            },
        },
        "required": ["item_id"],
    },
)

WITHDRAW_GOLD = _tool(
    "withdraw_gold",
    "Withdraw gold from Xunlai storage to character. Must be near the Xunlai chest (interact first).",
    {
        "properties": {
            "amount": {"type": "integer", "description": "Gold amount to withdraw"},
        },
        "required": ["amount"],
    },
)

DEPOSIT_GOLD = _tool(
    "deposit_gold",
    "Deposit gold from character to Xunlai storage. Must be near the Xunlai chest (interact first).",
    {
        "properties": {
            "amount": {"type": "integer", "description": "Gold amount to deposit"},
        },
        "required": ["amount"],
    },
)

QUERY_STATE = _tool(
    "query_state",
    "Request an immediate fresh tier-3 snapshot (inventory, gold, merchant, agents). "
    "Use after state-changing actions (buy, craft, withdraw) to get up-to-date data "
    "without waiting for the normal 2-second snapshot cadence.",
    {
        "properties": {},
        "required": [],
    },
)

TRADER_BUY = _tool(
    "trader_buy",
    "Buy one pack (10 units) of a material from the material trader. "
    "Handles quote + transact automatically via the native RequestQuote/Transaction functions. "
    "Must have the material trader window open (merchant.is_open). "
    "Provide either model_id (material model, e.g. 948 for Iron Ingot) or item_id (virtual item ID). "
    "Errors to check: 'out_of_stock' means the trader has no supply of this material "
    "(server returned price=0) — DO NOT retry the same model_id in this district; skip any "
    "recipes that depend on it and consider changing district. 'quote_failed' is a transient "
    "protocol/hook issue that may succeed on retry. 'virtual_item_not_found' means the "
    "merchant isn't actually open or the item isn't on this trader's list.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Virtual item ID (if known)"},
            "model_id": {"type": "integer", "description": "Material model ID (e.g. 948=Iron, 929=Dust, 921=Bone, 933=Feather)"},
        },
        "required": [],
    },
)

INITIATE_TRADE = _tool(
    "initiate_trade",
    "Initiate a player-to-player trade with another player agent.",
    {
        "properties": {
            "agent_id": {"type": "integer", "description": "Target player agent ID"},
            "player_number": {"type": "integer", "description": "Optional target player number from snapshot"},
        },
        "required": ["agent_id"],
    },
)

OFFER_TRADE_ITEM = _tool(
    "offer_trade_item",
    "Offer an inventory item into the current player trade window.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Inventory item ID to offer"},
            "quantity": {"type": "integer", "description": "Quantity to offer (default 1)"},
        },
        "required": ["item_id"],
    },
)

OFFER_TRADE_ITEM_PROMPT_MAX = _tool(
    "offer_trade_item_prompt_max",
    "Offer a full stackable inventory stack via the native trade quantity prompt Max path.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Stackable inventory item ID to offer"},
        },
        "required": ["item_id"],
    },
)

OFFER_TRADE_ITEM_PROMPT_DEFAULT = _tool(
    "offer_trade_item_prompt_default",
    "Offer the default quantity (1) for a stackable inventory item via the native trade quantity prompt.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Stackable inventory item ID to offer"},
        },
        "required": ["item_id"],
    },
)

OFFER_TRADE_ITEM_PROMPT_QUANTITY = _tool(
    "offer_trade_item_prompt_quantity",
    "Offer an exact quantity for a stackable inventory item via the native trade quantity prompt.",
    {
        "properties": {
            "item_id": {"type": "integer", "description": "Stackable inventory item ID to offer"},
            "quantity": {"type": "integer", "description": "Exact stack quantity to offer"},
        },
        "required": ["item_id", "quantity"],
    },
)

SUBMIT_TRADE_OFFER = _tool(
    "submit_trade_offer",
    "Submit your current player-trade offer, optionally with gold.",
    {
        "properties": {
            "gold": {"type": "integer", "description": "Gold to include in the offer (default 0)"},
        },
        "required": [],
    },
)

ACCEPT_TRADE = _tool(
    "accept_trade",
    "Accept the current player trade after both sides have submitted.",
    {"properties": {}, "required": []},
)

CANCEL_TRADE = _tool(
    "cancel_trade",
    "Cancel or close the current player trade.",
    {"properties": {}, "required": []},
)

CHANGE_TRADE_OFFER = _tool(
    "change_trade_offer",
    "Cancel a previously submitted player-trade offer so it can be edited.",
    {"properties": {}, "required": []},
)

REMOVE_TRADE_ITEM = _tool(
    "remove_trade_item",
    "Remove an item from the current player trade offer.",
    {
        "properties": {
            "slot_or_item_id": {"type": "integer", "description": "Trade slot index or current trade item identifier"},
        },
        "required": ["slot_or_item_id"],
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

SEND_WHISPER = _tool(
    "send_whisper",
    "Send a private whisper to a specific player by character name.",
    {
        "properties": {
            "recipient": {"type": "string", "description": "Exact character name to whisper"},
            "message": {"type": "string", "description": "Whisper message text"},
        },
        "required": ["recipient", "message"],
    },
)

SET_COMBAT_MODE = _tool(
    "set_combat_mode",
    "Switch combat handling between built-in bot logic and LLM control. "
    "In 'builtin' mode, Froggy's skill rotation handles combat automatically. "
    "In 'llm' mode, Froggy only auto-attacks and YOU control all skill usage "
    "via use_skill, use_hero_skill, change_target, etc.",
    {
        "properties": {
            "mode": {
                "type": "string",
                "description": "Combat mode: 'builtin' (bot handles skills) or 'llm' (you handle skills)",
            },
        },
        "required": ["mode"],
    },
)

SET_BOT_STATE = _tool(
    "set_bot_state",
    "Override the Froggy bot's current state (advisory mode only). "
    "States: idle, in_town, traveling, in_dungeon, looting, merchant, "
    "maintenance, llm_controlled. Use llm_controlled to take full control.",
    {
        "properties": {
            "state": {
                "type": "string",
                "description": "Bot state: idle, in_town, traveling, in_dungeon, looting, merchant, maintenance, llm_controlled",
            },
        },
        "required": ["state"],
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

# --- Price Discovery ---

SEARCH_TRADE_PRICES = _tool(
    "search_trade_prices",
    "Search Kamadan trade chat history for recent buy/sell offers. "
    "Use this BEFORE trading to understand current market prices. "
    "Returns recent trade messages mentioning the item. "
    "Look for WTS (want to sell) and WTB (want to buy) patterns and "
    "extract price ranges from the messages.",
    {
        "properties": {
            "query": {
                "type": "string",
                "description": "Item name to search for (e.g., 'Ecto', 'Armbraces', 'Diamond')",
            },
            "count": {
                "type": "integer",
                "description": "Max results to return (default 10, max 25)",
            },
        },
        "required": ["query"],
    },
)


# --- Farming Knowledge (static lookups, no game-thread round trip) ---

GET_RECIPE = _tool(
    "get_recipe",
    "Look up the crafting recipe for a consumable by model_id. Returns the "
    "required materials (model_id + quantity PER CRAFT), the crafter NPC name, "
    "the outpost map_id where the crafter lives, the crafter's (x, y) "
    "coordinates, and the gold cost per craft. Use this before attempting to "
    "craft a consumable so you know what materials to buy first. "
    "Known consumable model_ids: 24861 (Grail of Might), 24859 (Essence of "
    "Celerity), 24860 (Armor of Salvation).",
    {
        "properties": {
            "consumable_model_id": {
                "type": "integer",
                "description": "Model ID of the consumable to craft",
            },
        },
        "required": ["consumable_model_id"],
    },
)

GET_OUTPOST_INFO = _tool(
    "get_outpost_info",
    "Look up the key NPC locations in an outpost by map_id. Returns material "
    "trader coords, Xunlai storage chest coords, merchant coords, and any "
    "known crafters with their (x, y) positions. If the outpost has no "
    "hardcoded coords but has a known material trader NPC model_id, the "
    "response includes that model_id so you can scan the agent list for it.",
    {
        "properties": {
            "map_id": {
                "type": "integer",
                "description": "Outpost map ID (e.g. 857=Embark Beach, 638=Gadd's Encampment)",
            },
        },
        "required": ["map_id"],
    },
)

GET_MATERIAL_INFO = _tool(
    "get_material_info",
    "Look up the human-readable name of a material by model_id. Returns "
    "{name} for known materials. Useful when you see a material in inventory "
    "or at a trader and want to match it to a recipe ingredient.",
    {
        "properties": {
            "model_id": {
                "type": "integer",
                "description": "Material model ID",
            },
        },
        "required": ["model_id"],
    },
)

GET_DUNGEON_INFO = _tool(
    "get_dungeon_info",
    "Return the full procedure to complete a dungeon end-to-end. Response "
    "fields (when available):\n"
    "  entry_outpost: {map_id, name} — town where prep happens\n"
    "  quest: {name, giver_npc, giver_map_id, giver_x, giver_y, "
    "dialog_accept, dialog_complete, must_accept_before_entry, note} — "
    "required quest, if any\n"
    "  prep: {hard_mode, recommended_consumables[], hero_team[], "
    "blessings_before_entry[], blessing_coords_hint{}}\n"
    "  levels[]: each {name, map_id, spawn:{x,y}, key_points[], hazards[]}\n"
    "  key_points[]: each {kind, x, y, note, to_level?, blessing_type?}\n"
    "    kind values: blessing, quest_accept, checkpoint, key, door, "
    "portal, boss_engagement, end_chest, interact_gadget\n"
    "  reward_turnin: {npc_name, map_id, x, y, dialog, note} — where to "
    "return for quest reward\n"
    "Known dungeons: 'Bogroot Growths', 'Arachni\\'s Haunt', "
    "'Raven\\'s Point', 'Catacombs of Kathandrax', 'Rragar\\'s Menagerie', "
    "'Frostmaw\\'s Burrows'. Name lookup is case- and apostrophe-"
    "insensitive.",
    {
        "properties": {
            "name": {
                "type": "string",
                "description": "Dungeon display name (e.g. 'Bogroot Growths')",
            },
        },
        "required": ["name"],
    },
)

GET_BLESSING_INFO = _tool(
    "get_blessing_info",
    "Look up how to acquire a reputation blessing: dialog codes to send, "
    "effect_ids to verify on your effects list after, and known NPC "
    "locations when hardcoded. Known blessing types: 'asuran', 'norn', "
    "'dwarven', 'vanguard', 'sunspears', 'lightbringer'. The protocol is: "
    "walk to blessing NPC, interact_npc, send_dialog(code) — after the "
    "server processes it, one of the effect_ids should appear on 'me.effects'.",
    {
        "properties": {
            "blessing_type": {
                "type": "string",
                "description": "Blessing name: asuran, norn, dwarven, vanguard, sunspears, lightbringer",
            },
        },
        "required": ["blessing_type"],
    },
)

GET_HERO_BUILD = _tool(
    "get_hero_build",
    "Look up the standard Mercenary skillbar template for a hero by name. "
    "Returns {hero_id, profession, role, skillbar_template, variants}. "
    "The skillbar_template is a base64 string that can be passed to "
    "load_skillbar. hero_id is the numeric ID to pass to add_hero / "
    "kick_hero. Known heroes: Xandra, Olias, Livia, Master of Whispers, "
    "Gwen, Norgu, Razah, Dunham.",
    {
        "properties": {
            "hero_name": {
                "type": "string",
                "description": "Hero display name (e.g. 'Xandra', 'Gwen')",
            },
        },
        "required": ["hero_name"],
    },
)

GET_QUEST_INFO = _tool(
    "get_quest_info",
    "Look up quest giver NPC + coords + dialog codes for a quest by name or "
    "quest_id. Currently covers 'Tekks's War' (825) — required to unlock "
    "Bogroot Growths. Returns {giver_npc_name, giver_map_id, giver_x, "
    "giver_y, dialog_accept, dialog_complete, description}.",
    {
        "properties": {
            "key": {
                "type": ["string", "integer"],
                "description": "Quest name (e.g. 'Tekks\\'s War') or numeric quest_id (e.g. 825)",
            },
        },
        "required": ["key"],
    },
)


# All tools in a single list for passing to the LLM
ALL_TOOLS = [
    # Movement
    MOVE_TO,
    AGGRO_MOVE_TO,
    CHANGE_TARGET,
    CANCEL_ACTION,
    # Combat
    ATTACK,
    CALL_TARGET,
    USE_SKILL,
    USE_HERO_SKILL,
    # Interaction
    INTERACT_NPC,
    OPEN_MERCHANT,
    INTERACT_PLAYER,
    INTERACT_SIGNPOST,
    DIALOG,
    # Quest log
    SET_ACTIVE_QUEST,
    ABANDON_QUEST,
    REQUEST_QUEST_INFO,
    OPEN_QUEST_LOG,
    # Party/Hero
    ADD_HERO,
    KICK_HERO,
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
    OPEN_XUNLAI,
    MERCHANT_BUY,
    MERCHANT_SELL,
    REQUEST_QUOTE,
    TRANSACT_ITEMS,
    CRAFT_ITEM,
    TRADER_BUY,
    WITHDRAW_GOLD,
    DEPOSIT_GOLD,
    QUERY_STATE,
    INITIATE_TRADE,
    OFFER_TRADE_ITEM,
    OFFER_TRADE_ITEM_PROMPT_MAX,
    OFFER_TRADE_ITEM_PROMPT_DEFAULT,
    OFFER_TRADE_ITEM_PROMPT_QUANTITY,
    SUBMIT_TRADE_OFFER,
    ACCEPT_TRADE,
    CANCEL_TRADE,
    CHANGE_TRADE_OFFER,
    REMOVE_TRADE_ITEM,
    # Bot control (advisory mode)
    SET_COMBAT_MODE,
    SET_BOT_STATE,
    # Utility
    SEND_CHAT,
    SEND_WHISPER,
    DROP_GOLD,
    RESIGN,
    WAIT,
    # Price Discovery
    SEARCH_TRADE_PRICES,
    # Farming Knowledge (local static lookups, no game-thread round trip)
    GET_RECIPE,
    GET_OUTPOST_INFO,
    GET_MATERIAL_INFO,
    GET_DUNGEON_INFO,
    GET_BLESSING_INFO,
    GET_HERO_BUILD,
    GET_QUEST_INFO,
]
