# GWA3 LLM Bridge

An autonomous AI agent that plays Guild Wars using a local LLM (Gemma 4). Give it an objective, walk away, come back to loot.

Important for multi-agent work:
- always launch Guild Wars through `GWLauncher`
- always inject only the exact launcher-returned PID
- always use an isolated build dir, DLL name, and pipe name per agent
- never share one mutable `gwa3/build` + `gwa3.dll` + `\\.\pipe\gwa3_llm` setup across multiple agents

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

# Optional: enables faster Kamadan WebSocket searches before player trades
pip install websockets>=12.0

# Pull Gemma 4 model (one-time download, ~16GB for quantized 27B)
ollama pull gemma4:27b
```

### Step 2: Start Ollama

Open a terminal and leave it running:

```bash
ollama serve
```

### Step 3: Configure an isolated agent build

Use one preset per active agent. Recommended presets:

- `disco` -> `build_disco`, `gwa3_disco.dll`, `\\.\pipe\gwa3_llm_disco`
- `beastrit` -> `build_beastrit`, `gwa3_beastrit.dll`, `\\.\pipe\gwa3_llm_beastrit`
- `trade` -> `build_trade`, `gwa3_trade.dll`, `\\.\pipe\gwa3_llm_trade`

Example for Disco Panic:

```bash
cd gwa3
cmake --preset disco
cmake --build --preset disco --target injector --target gwa3
```

### Step 4: Launch Guild Wars with GWLauncher

Use the account-specific AutoIt launcher script in `GWA Censured/debug_scripts/` and capture the exact PID it returns.

Do not:
- launch `Gw.exe` directly
- inject by "first GW process"
- reuse another agent's PID

### Step 5: Inject the isolated DLL in LLM mode

Example for Disco Panic after GWLauncher returns a PID:

```bash
cd gwa3/build_disco/bin/Release
injector.exe --pid 12345 --dll gwa3_disco.dll --llm
```

This sets the PID-scoped LLM mode flag and injects only the requested client. gwa3 handles character select automatically, waits for map load, then starts the named pipe server for that build.

### Step 6: Launch the agent on the matching pipe

Example for Disco Panic:

```bash
cd gwa3
set GWA3_PIPE_NAME=\\.\pipe\gwa3_llm_disco
python -m bridge \
  --pipe \\.\pipe\gwa3_llm_disco \
  --llm-url http://localhost:11434/v1 \
  --model gemma4:27b \
  --kamadan-timeout 10 \
  --kamadan-cache-ttl 120 \
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
[Gemma -> GW] set_hard_mode, kick_hero, kick_hero, add_hero, add_hero, add_hero
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

For player-to-player trading objectives, Gemma also uses recent Kamadan archive searches to estimate the current buy/sell range before agreeing to deals.

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
injector.exe --pid 12345 --dll gwa3_disco.dll --llm
injector.exe --pid 12345 --dll gwa3_trade.dll --llm
injector.exe --pid 12345 --dll gwa3_beastrit.dll --llm-advisory
```

Prefer explicit `--pid` + `--dll` in multi-client environments.

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
| `--pipe` | `\\.\pipe\gwa3_llm` | Named pipe path; use an isolated per-agent pipe like `\\.\pipe\gwa3_llm_disco` |
| `--kamadan-timeout` | `10.0` | Per-source timeout in seconds for Kamadan HTTP/WebSocket price searches |
| `--kamadan-cache-ttl` | `120.0` | Cache TTL in seconds for Kamadan search results |

## Per-Agent Workflow

### Disco Panic bridge work

```bash
cd gwa3
cmake --preset disco
cmake --build --preset disco --target injector --target gwa3
cd build_disco/bin/Release
injector.exe --pid 12345 --dll gwa3_disco.dll --llm
cd ../../
set GWA3_PIPE_NAME=\\.\pipe\gwa3_llm_disco
python -m bridge --pipe \\.\pipe\gwa3_llm_disco
```

### BEASTRIT Froggy work

```bash
cd gwa3
cmake --preset beastrit
cmake --build --preset beastrit --target injector --target gwa3
powershell -ExecutionPolicy Bypass -File tools/run_froggy_test.ps1 `
  -AccountIndex 0 `
  -BuildDir "c:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build_beastrit" `
  -DllName "gwa3_beastrit.dll"
```

### Trade harness work

```bash
cd gwa3
cmake --preset trade
cmake --build --preset trade --target injector --target gwa3
set GWA3_BUILD_DIR=c:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build_trade
set GWA3_DLL_NAME=gwa3_trade.dll
set GWA3_PIPE_NAME=\\.\pipe\gwa3_llm_trade
python -m bridge.tests --filter "test_player_trade*"
```

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

## Available Actions (52 tools)

**Movement (3):** `move_to`, `change_target`, `cancel_action`

**Combat (4):** `attack`, `call_target`, `use_skill`, `use_hero_skill`

**Interaction (4):** `interact_npc`, `interact_player`, `interact_signpost`, `dialog`

**Party/Hero (7):** `add_hero`, `kick_hero`, `flag_hero`, `flag_all`, `unflag_all`, `set_hero_behavior`, `lock_hero_target`

To clear a party, issue repeated `kick_hero` actions for the currently present hero IDs. The bulk `kick_all_heroes` action is intentionally not exposed because the legacy sentinel path is not confirmed reliable in `gwa3`.

**Travel (5):** `travel`, `enter_mission`, `return_to_outpost`, `set_hard_mode`, `skip_cinematic`

**Items (5):** `pick_up_item`, `use_item`, `equip_item`, `drop_item`, `move_item`

**Salvage/Identify (4):** `identify_item`, `salvage_start`, `salvage_materials`, `salvage_done`

**Skillbar (1):** `load_skillbar`

**Trade and Crafting (11):** `buy_materials`, `request_quote`, `transact_items`, `craft_item`, `initiate_trade`, `offer_trade_item`, `submit_trade_offer`, `accept_trade`, `cancel_trade`, `change_trade_offer`, `remove_trade_item`

**Bot Control (2):** `set_combat_mode`, `set_bot_state`

**Utility (5):** `send_chat`, `send_whisper`, `drop_gold`, `resign`, `wait`

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
- Is GW running? Did you inject the correct PID with the correct `--dll`?
- Does `--pipe` match the build's pipe name?
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

**Another agent is using another character**
- do not reuse `gwa3/build`
- do not reuse `gwa3.dll`
- do not reuse `\\.\pipe\gwa3_llm`
- build and run in a separate preset lane instead

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
│   └── requirements.txt         # httpx, pywin32, optional websockets
├── include/gwa3/llm/            # C++ headers
│   ├── IpcServer.h              # Named pipe server
│   ├── GameSnapshot.h           # State serialization
│   ├── ActionExecutor.h         # Action validation + dispatch
│   └── LlmBridge.h             # Coordinator
├── CMakePresets.json            # Per-agent isolated build presets
└── src/llm/                     # C++ implementation
    ├── IpcServer.cpp
    ├── GameSnapshot.cpp
    ├── ActionExecutor.cpp
    └── LlmBridge.cpp
```
