# GWA3 LLM Bridge

An autonomous AI agent that plays Guild Wars using a local LLM (Gemma 4). Give it an objective, walk away, come back to loot.

## How It Works

```
GW.exe (gwa3.dll)  <--Named Pipe-->  Python Bridge  <--HTTP-->  Ollama (Gemma 4)
   Reads game state                   Agent Loop                  Decides actions
   Executes actions                   Sends snapshots             Issues tool calls
```

1. **gwa3.dll** is injected into Guild Wars. It reads game memory (player, enemies, inventory, dialogs, merchants) and exposes 41 game actions.
2. **Python bridge** connects gwa3 to the LLM. It sends game state snapshots to Gemma and forwards Gemma's tool calls back to gwa3.
3. **Gemma 4** receives a standing objective ("Farm Bogroot Growths HM repeatedly") and plays the game autonomously — fighting, looting, selling, traveling, repeating — without human input.

## Quick Start

### Step 1: One-time setup

```bash
# Install Python dependencies
cd gwa3/bridge
pip install -r requirements.txt

# Pull Gemma 4 model (one-time download, ~16GB for quantized 27B)
ollama pull gemma4:27b
```

### Step 2: Start Ollama

Open a terminal and leave it running:

```bash
ollama serve
```

### Step 3: Launch Guild Wars

Launch GW normally (GW Launcher, shortcut, etc.). Get to the character select screen or log into a character.

### Step 4: Inject gwa3 in LLM mode

```bash
cd gwa3/build/bin/Release
injector.exe --llm
```

This auto-detects GW, sets the LLM mode flag, and injects. gwa3 handles character select automatically, waits for map load, then starts the named pipe server.

### Step 5: Launch the agent

```bash
cd gwa3
python -m bridge \
  --llm-url http://localhost:11434/v1 \
  --model gemma4:27b \
  --objective "Farm Bogroot Growths HM repeatedly. Sell loot when inventory is full."
```

Gemma starts playing immediately. No further input required.

### What you'll see

```
============================================================
  GWA3 LLM Bridge — Gemma 4 Autonomous Agent
============================================================
  LLM:       http://localhost:11434/v1
  Model:     gemma4:27b
  Autonomy:  tactical
  Objective: Farm Bogroot Growths HM repeatedly. Sell loot when inventory is full.
============================================================
[Bridge] Connecting to gwa3...
[Bridge] Connected to gwa3!
[Agent] Starting autonomous agent loop
[Agent] Objective: Farm Bogroot Growths HM repeatedly. Sell loot when inventory is full.
[Chat] Gemma is playing autonomously. Type to send messages (optional).
[Gemma -> GW] set_hard_mode, kick_all_heroes, add_hero, add_hero, add_hero
[Gemma -> GW] travel
[Gemma -> GW] enter_mission
[Gemma -> GW] move_to
[Gemma -> GW] use_skill, attack, use_skill
[Gemma -> GW] pick_up_item, pick_up_item
[Gemma] Run complete. Returning to sell.
[Gemma -> GW] return_to_outpost
[Gemma -> GW] interact_npc, transact_items, transact_items
[Gemma -> GW] travel
...
```

Gemma runs indefinitely. Press `Ctrl+C` to stop.

## Objectives

The `--objective` flag tells Gemma what to do. It stays in context across the entire session — Gemma never forgets what it's working on. Examples:

```bash
# Dungeon farming (most common)
--objective "Farm Bogroot Growths HM repeatedly. Sell loot when inventory is full."

# Material farming
--objective "Farm Vaettirs in HM at Jaga Moraine. Pick up all drops. Sell whites and blues, keep golds."

# Shopping
--objective "Go to Kamadan, buy 50 Iron Ingots and 25 Dust from the materials trader."

# Maintenance
--objective "Identify all gold items, salvage blues and purples for materials, store ectos in Xunlai."
```

If no objective is provided, Gemma defaults to: *"Farm continuously. Complete dungeon/mission runs, sell loot when inventory is full, restock consumables, and repeat."*

## Talking to Gemma (Optional)

While Gemma plays, you can type messages in the terminal to adjust its behavior:

```
> switch to normal mode
> go sell now, inventory is almost full
> stop farming, just stand in town
> what's your status?
```

This is entirely optional. Gemma will play for hours without any input.

## CLI Reference

### Injector

```bash
injector.exe --llm              # Inject in LLM mode (auto-detect GW)
injector.exe --llm --pid 12345  # Inject specific PID in LLM mode
injector.exe --llm --all        # Inject ALL GW instances in LLM mode
```

### Bridge

```bash
python -m bridge [options]
```

| Flag | Default | Description |
|------|---------|-------------|
| `--llm-url` | `http://localhost:8000/v1` | Ollama/vLLM API endpoint |
| `--model` | `gemma-4-32b-it` | Model name (check `ollama list`) |
| `--objective` | Generic farming | What Gemma should do |
| `--autonomy` | `tactical` | `advisory`, `tactical`, or `full` |
| `--pipe` | `\\.\pipe\gwa3_llm` | Named pipe path |

## Autonomy Modes

| Mode | Gemma Decides | Scripted Code Handles |
|------|--------------|----------------------|
| **advisory** | High-level strategy (where to go, when to sell) | Combat micro, pathfinding |
| **tactical** | Skills, targets, movement, inventory | Low-level action queuing |
| **full** | Everything | Pure action executor |

Start with `tactical` (default). Use `advisory` if Gemma's combat is too slow (~200ms per decision). Use `full` if you want Gemma to control every aspect.

## Game State

Gemma sees three tiers of data, sent at different frequencies:

| Tier | Interval | What Gemma Sees |
|------|----------|-----------------|
| 1 | 200ms | Player HP/energy/position, skillbar with recharges, map ID, party alive/dead status |
| 2 | 500ms | + All nearby enemies (HP, casting skill, hexed/enchanted), allies, ground items, hero skillbars, dialogs, merchant windows |
| 3 | 2s | + Full inventory with rarity and free slots, Xunlai storage, gold, active effects with time remaining |

All skill IDs, item IDs, and profession IDs are resolved to human-readable names before Gemma sees them.

## Available Actions (41 tools)

**Movement (3):** `move_to`, `change_target`, `cancel_action`

**Combat (4):** `attack`, `call_target`, `use_skill`, `use_hero_skill`

**Interaction (4):** `interact_npc`, `interact_player`, `interact_signpost`, `dialog`

**Party/Hero (8):** `add_hero`, `kick_hero`, `kick_all_heroes`, `flag_hero`, `flag_all`, `unflag_all`, `set_hero_behavior`, `lock_hero_target`

**Travel (5):** `travel`, `enter_mission`, `return_to_outpost`, `set_hard_mode`, `skip_cinematic`

**Items (5):** `pick_up_item`, `use_item`, `equip_item`, `drop_item`, `move_item`

**Salvage/Identify (4):** `identify_item`, `salvage_start`, `salvage_materials`, `salvage_done`

**Skillbar (1):** `load_skillbar`

**Trade (3):** `buy_materials`, `request_quote`, `transact_items`

**Utility (4):** `send_chat`, `drop_gold`, `wait`

All actions are validated (agent exists? skill recharged? item found?) and rate-limited to 10/second.

## Autonomous Behavior

Gemma's decision loop runs every ~300ms:

```
1. Am I alive?           → No: wait for recovery or return to outpost
2. Party defeated?       → return_to_outpost
3. Right map?            → No: travel to target area
4. In outpost?           → Set up party, hard mode, enter mission
5. In explorable?        → Move toward objectives, fight, loot
6. Inventory full?       → Return to outpost, sell/salvage, resume
7. Dialog open?          → Read options, choose the right one
8. Merchant open?        → Buy/sell as needed
```

Error recovery is built in:
- **Stuck** — if position doesn't change for 3+ cycles, tries a different movement
- **Wipe** — returns to outpost and restarts the run
- **Idle** — if no actions for 10 cycles, system nudges Gemma to act
- **Disconnect** — stops acting, waits for reconnection

## Troubleshooting

**"Could not connect to gwa3 pipe"**
- Is GW running? Did you run `injector.exe --llm`?
- Check gwa3 log for `[LLM-Bridge] Initialized`
- GW must be past character select (map loaded)

**LLM not responding / timeout**
- Is Ollama running? `curl http://localhost:11434/v1/models`
- Model name must match `ollama list` output
- Check GPU memory — 27B model needs ~16GB VRAM (quantized)

**Gemma not taking actions**
- Check terminal for `[Gemma -> GW]` lines
- If only `[Gemma]` text with no tool calls, the model may not support function calling — try vLLM instead of Ollama
- Try a more specific objective

**Actions failing**
- Check gwa3 log for `[LLM-Action]` error messages
- Common: "map_not_loaded", "agent_not_found", "skill_on_recharge"
- Rate limiter blocks if >10 actions/sec

## File Structure

```
gwa3/
├── bridge/                      # Python bridge (autonomous agent)
│   ├── __main__.py              # Entry point
│   ├── agent_loop.py            # Autonomous observe-think-act loop
│   ├── llm_client.py            # OpenAI-compatible HTTP client
│   ├── tool_schema.py           # 41 tool definitions
│   ├── observation.py           # Game state summary builder
│   ├── gamedata.py              # 2,978 skill + 1,091 item name lookups
│   ├── chat_interface.py        # Optional user chat
│   ├── config.py                # CLI args
│   ├── ipc_client.py            # Named pipe client
│   └── requirements.txt         # httpx, pywin32
├── include/gwa3/llm/            # C++ headers
│   ├── IpcServer.h              # Named pipe server
│   ├── GameSnapshot.h           # State serialization
│   ├── ActionExecutor.h         # Action validation + dispatch
│   └── LlmBridge.h             # Coordinator
└── src/llm/                     # C++ implementation
    ├── IpcServer.cpp
    ├── GameSnapshot.cpp
    ├── ActionExecutor.cpp
    └── LlmBridge.cpp
```
