# GWA3 LLM Bridge

Connect a local LLM (Gemma 4 32B) to Guild Wars through gwa3. The LLM observes real-time game state and controls the game via function calling — replacing hardcoded bot scripts with natural language instructions.

## Architecture

```
GW.exe (gwa3.dll)  <--Named Pipe-->  Python Bridge  <--HTTP-->  vLLM/Ollama (Gemma 4)
   IpcServer                          Agent Loop                  Tool-use API
   GameSnapshot                       Chat UI
   ActionExecutor                     Tool Dispatch
```

- **gwa3.dll** runs inside the game process, reads game state, and executes actions
- **Python bridge** connects to gwa3 via named pipe and to the LLM via OpenAI-compatible HTTP API
- **Gemma 4** receives game state snapshots, reasons about what to do, and issues tool calls that map to game actions

## Prerequisites

- **Guild Wars** client running
- **gwa3.dll** built (see main project README)
- **Python 3.10+**
- **Gemma 4 32B** running locally via vLLM or Ollama
- **GPU** with enough VRAM for 32B parameter model (~20GB+ for quantized, ~64GB for full precision)

## Quick Start (Step by Step)

### Step 1: One-time setup

```bash
# Install Python dependencies
cd gwa3/bridge
pip install -r requirements.txt

# Pull Gemma 4 model via Ollama (one-time download)
ollama pull gemma4:27b
```

### Step 2: Start Ollama

```bash
ollama serve
```

Leave this terminal running. Ollama serves an OpenAI-compatible API on `http://localhost:11434/v1`.

Verify it's working:
```bash
curl http://localhost:11434/v1/models
```

### Step 3: Launch Guild Wars

Launch GW normally (via GW Launcher, shortcut, etc.). Wait for the client to reach the character select screen or log in to a character.

### Step 4: Inject gwa3.dll in LLM mode

```bash
cd gwa3/build/bin/Release
injector.exe --llm
```

This does three things:
1. Auto-detects the running GW.exe window
2. Creates the `gwa3_llm_mode.flag` file
3. Injects `gwa3.dll` into the GW process

gwa3 handles character select automatically (clicks Play), waits for the map to load, then starts the named pipe server. Watch the gwa3 log for:

```
[LLM-Bridge] Initialized — listening on \\.\pipe\gwa3_llm
```

### Step 5: Run the Python bridge

```bash
cd gwa3
python -m bridge --llm-url http://localhost:11434/v1 --model gemma4:27b
```

The bridge connects to both the named pipe (gwa3) and the LLM API (Ollama), then starts the agent loop:

```
============================================================
  GWA3 LLM Bridge — Gemma 4 Agent
============================================================
  LLM:      http://localhost:11434/v1
  Model:    gemma4:27b
  Autonomy: tactical
  Pipe:     \\.\pipe\gwa3_llm
============================================================
[Bridge] Connecting to gwa3...
[Bridge] Connected to gwa3!
[Agent] Starting agent loop...
[Chat] Type messages to communicate with Gemma. Press Ctrl+C to quit.
```

### Step 6: Talk to Gemma

Type natural language instructions in the terminal:

```
> Farm Bogroot Growths in hard mode
> Go sell at the merchant
> What's in my inventory?
```

Press `Ctrl+C` to stop the bridge.

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
| `--llm-url` | `http://localhost:8000/v1` | OpenAI-compatible API endpoint |
| `--model` | `gemma-4-32b-it` | Model name to request |
| `--autonomy` | `tactical` | Autonomy level: `advisory`, `tactical`, or `full` |
| `--pipe` | `\\.\pipe\gwa3_llm` | Named pipe path (rarely needs changing) |

## Usage

Once connected, you'll see a terminal interface:

```
============================================================
  GWA3 LLM Bridge — Gemma 4 Agent
============================================================
  LLM:      http://localhost:8000/v1
  Model:    gemma-4-27b-it
  Autonomy: tactical
  Pipe:     \\.\pipe\gwa3_llm
============================================================
[Bridge] Connecting to gwa3...
[Bridge] Connected to gwa3!
[Agent] Starting agent loop...
[Chat] Type messages to communicate with Gemma. Press Ctrl+C to quit.
```

Type natural language instructions:

```
> Farm Bogroot Growths in hard mode
[Gemma] Setting up for Bogroot Growths HM. Adding heroes and traveling to Gadd's Encampment.
[Gemma -> GW] kick_all_heroes, add_hero, add_hero, add_hero, ...
[Gemma -> GW] set_hard_mode, travel
```

```
> Go sell items at the merchant
[Gemma] Returning to outpost to visit the merchant.
[Gemma -> GW] return_to_outpost
```

```
> What's my current HP and energy?
[Gemma] You're at 85% HP (442/520) and 62% energy (25/40). No immediate threats nearby.
```

## Autonomy Modes

| Mode | LLM Controls | Bot Code Handles |
|------|-------------|-----------------|
| **advisory** | High-level strategy (where to go, when to sell) | Combat rotations, pathfinding micro |
| **tactical** | Skill usage, targeting, movement decisions | Low-level action queuing |
| **full** | Everything — pure LLM decision making | Action execution only |

Start with `tactical`. Use `advisory` if the LLM's combat decisions are too slow, or `full` if you want maximum LLM control.

## Game State Snapshots

The bridge receives three tiers of game data at different intervals:

| Tier | Interval | Contents | Size |
|------|----------|----------|------|
| 1 | 200ms | Player position/HP/energy, skillbar recharges, map ID, party status | ~500 bytes |
| 2 | 500ms | Tier 1 + nearby agents (enemies, allies, items within 2500 range) | ~2-4KB |
| 3 | 2000ms | Tier 2 + full inventory, active effects/buffs, gold counts | ~5-10KB |

## Available Actions (34 tools)

The LLM can call any of these as function/tool calls:

**Movement:** `move_to`, `change_target`, `cancel_action`

**Combat:** `attack`, `call_target`, `use_skill`, `use_hero_skill`

**Interaction:** `interact_npc`, `interact_player`, `interact_signpost`, `dialog`

**Party/Hero:** `add_hero`, `kick_hero`, `kick_all_heroes`, `flag_hero`, `flag_all`, `unflag_all`, `set_hero_behavior`, `lock_hero_target`

**Travel:** `travel`, `enter_mission`, `return_to_outpost`, `set_hard_mode`, `skip_cinematic`

**Items:** `pick_up_item`, `use_item`, `equip_item`, `drop_item`, `move_item`

**Trade:** `buy_materials`, `request_quote`, `transact_items`

**Utility:** `send_chat`, `wait`

All actions are validated before execution (agent exists, skill not on recharge, coordinates in range, etc.) and rate-limited to 10 actions/second.

## Troubleshooting

**"Could not connect to gwa3 pipe"**
- Verify gwa3.dll is injected and the `gwa3_llm_mode.flag` file exists next to the DLL
- Check gwa3 log for `[LLM-Bridge] Initialized` message
- Make sure GW is past the character select screen (map must be loaded)

**LLM not responding / timeout**
- Verify vLLM or Ollama is running: `curl http://localhost:8000/v1/models`
- Check that the model name matches what's served
- 32B models need significant VRAM — monitor GPU usage

**Actions not executing**
- Check gwa3 log for `[LLM-Action]` messages — validation errors are logged
- Verify the game is in a state where actions make sense (map loaded, not in cinematic)
- Rate limiter caps at 10 actions/sec — the LLM may be sending too many

**Game stuttering**
- The IPC runs on a dedicated thread and should not block the game
- If snapshots are too frequent, the bridge can send a `subscribe` message to reduce tier frequency (not yet implemented — adjust `TIER*_INTERVAL_MS` constants in `LlmBridge.cpp`)

## File Structure

```
gwa3/
├── bridge/                      # Python bridge process
│   ├── __main__.py              # Entry point: python -m bridge
│   ├── ipc_client.py            # Named pipe client
│   ├── agent_loop.py            # Observe-think-act cycle
│   ├── llm_client.py            # OpenAI-compatible HTTP client
│   ├── tool_schema.py           # 34 tool definitions
│   ├── observation.py           # Snapshot window manager
│   ├── chat_interface.py        # Terminal chat UI
│   ├── config.py                # CLI args and defaults
│   └── requirements.txt         # Python dependencies
├── include/gwa3/llm/            # C++ headers
│   ├── IpcServer.h
│   ├── GameSnapshot.h
│   ├── ActionExecutor.h
│   └── LlmBridge.h
└── src/llm/                     # C++ implementation
    ├── IpcServer.cpp             # Named pipe server
    ├── GameSnapshot.cpp          # State serialization
    ├── ActionExecutor.cpp        # Action validation + dispatch
    └── LlmBridge.cpp            # Coordinator thread
```
