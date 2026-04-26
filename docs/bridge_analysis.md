# GWA3 LLM Bridge: Technical Architecture & API Specification

## 1. Overview
The GWA3 LLM Bridge is an autonomous agent system that enables a Large Language Model (LLM) to interact with Guild Wars. It implements an **Observe-Think-Act** loop, transforming raw game state into high-level reasoning and subsequent in-game actions.

## 2. System Architecture

### 2.1 High-Level Flow
`Game Process` $\rightarrow$ `Observation Layer` $\rightarrow$ `Bridge IPC` $\rightarrow$ `LLM Client` $\rightarrow$ `Agent Logic` $\rightarrow$ `Action Command` $\rightarrow$ `Game Interface`

### 2.2 Component Deep Dive

#### A. The LLM Client (`llm_client.py`)
The client serves as the communication layer between the bridge and an OpenAI-compatible API (vLLM/Ollama).
- **Async Communication**: Uses `httpx.AsyncClient` for non-blocking requests.
- **Tool Integration**: Supports the `tools` and `tool_choice` parameters, allowing the LLM to execute structured function calls rather than just generating text.
- **Data Models**: 
    - `ToolCall`: Encapsulates the tool name and arguments.
    - `LLMResponse`: A parsed object containing the content, tool calls, and token usage.

#### B. The Agent Loop (The Brain)
The agent manages the cognitive cycle:
1. **Observation**: Aggregates current game state (HP, Position, Enemies, Logs).
2. **Contextualization**: Merges the state with a comprehensive `SYSTEM_PROMPT` and historical context.
3. **Inference**: Requests a decision from the LLM.
4. **Execution**: Parses the `ToolCall` and sends the corresponding command to the game.

#### C. The Bridge (IPC)
The bridge acts as the translator between Python and the Game Process. It ensures that:
- Data is serialized into JSON for the LLM.
- Game commands are validated before being sent to the game engine.

## 3. API Specification

### 3.1 LLM Client Interface
**Method**: `chat_completion`
- **Inputs**:
    - `messages`: List of chat messages (System, User, Assistant).
    - `tools`: List of available tool definitions (JSON Schema).
    - `tool_choice`: Defaults to `"auto"`.
    - `temperature`: Defaults to `0.3` (low randomness for stability).
- **Output**: `LLMResponse` object.

### 3.2 Game State Schema (Observation)
The agent expects a state snapshot containing:
- **Player Stats**: Health, Energy, Current Map.
- **Combat Data**: Active target, enemy health, party status.
- **Environmental Data**: Current location, nearby NPCs/Players.
- **Logs**: Recent chat/combat log entries for situational awareness.

## 4. Analysis & Recommendations

### 4.1 Bottlenecks
- **Inference Latency**: The primary delay is the time taken for the LLM to generate a response (1-3 seconds).
- **Context Window**: Long sessions may lead to token exhaustion if history isn't managed.

### 4.2 Proposed Optimizations
- **State Deltas**: Instead of sending the full state every loop, send only the changes since the last update.
- **Heuristic Override**: Implement a "Fast Path" for critical actions (e.g., emergency healing) that bypasses the LLM.
- **Chain-of-Thought**: Force the LLM to output a `Reasoning` field before the `ToolCall` to improve decision accuracy.

## 5. KickAllHeroes Regression Notes

### 5.1 Evidence Timeline
- **Last known good integration-style hero clear in preserved logs**:
  - `gwa3/build/bin/Release/gwa3_log_premerchant_20260406_043224.txt`
  - Example good lines:
    - `2026-04-05 12:17:37 [INTG] [PASS] KickAllHeroes removed party heroes`
    - `2026-04-05 12:17:41 [INTG] [PASS] Heroes re-added to party`
  - The same file contains many additional good hero-clear runs through the afternoon of **April 5, 2026**.
- **First clearly bad Froggy-era hero clear in preserved logs**:
  - `gwa3/build/bin/Release/gwa3_log_premerchant_20260406_043224.txt`
  - Example bad lines:
    - `2026-04-06 03:58:06 [FROGGY-TEST] Party heroes after clear: 7`
    - `2026-04-06 03:58:06 [FROGGY-TEST] [FAIL] Existing heroes cleared before setup`
- **Current reproduced bad state**:
  - `gwa3/build/bin/Release/gwa3_log.txt`
  - Example lines:
    - `2026-04-08 03:56:00 PartyMgr: Froggy before KickAllHeroes ... heroes=7`
    - `2026-04-08 03:56:04 PartyMgr: Froggy after KickAllHeroes timeout ... heroes=7`
    - `2026-04-08 03:56:04 [FROGGY-TEST] [FAIL] Existing heroes cleared before setup`

### 5.2 Important Surprise
- `PartyMgr::KickAllHeroes()` itself does **not** appear to be the main source of change.
- The committed implementation was stable for a while:
  - `b2fbddf` switched hero kick-all to the `0x26` sentinel.
  - Current committed code still sends that same sentinel.
- The biggest architecture churn happened in the **CtoS transport**, not in `PartyMgr`.

### 5.3 Commit Window That Matters
- Last known good code/log era appears to be around:
  - `050678e` (`2026-04-05 14:48`) and nearby commits
- Immediately after that, the packet-send architecture changed repeatedly:
  - `81deef6` `2026-04-05 17:14` `WIP: CtoS raw dispatch + InlineTask in-place call fix`
  - `ee056ce` `2026-04-05 18:34` `WIP: CtoS dispatch - PacketSend needs separate hook context`
  - `adaa660` `2026-04-05 18:50` `Add CtoSHook for mid-function CtoS dispatch with watchdog re-patch`
  - `66c0090` `2026-04-05 19:18` `Fix CtoS: bare-minimum CtoSHook detour + shellcode dispatch working`
  - `0a5f821` `2026-04-06 10:46` `Fix CtoS: route packets through PacketSenderThread instead of dead shellcode detour`
  - `7deff35` `2026-04-06 12:06` `CtoS: PacketSend works from init context, crashes after GameThread hook`
  - `7991a77` `2026-04-06 12:19` `CtoS: sender thread + MH_DisableHook batch dispatch (~33% success rate)`
  - `ec6de23` `2026-04-06 12:52` `GameThread: replace MinHook with VEH INT3 hook + sender thread for CtoS`
  - `1344d82` `2026-04-07 04:24` `VEH INT3 hook: deferred GameThread init, RenderHook bootstrap, bisect all OK`
  - `f4e60bf` `2026-04-07 05:00` `Revert VEH, implement Engine inline hook for CtoS dispatch`

### 5.4 Relevant Code Changes By Area

#### A. `PartyMgr`
- File: `gwa3/src/managers/PartyMgr.cpp`
- `KickAllHeroes()` stayed simple in committed history:
  - older and current committed behavior: `SendPacket(2, HERO_KICK, 0x26u)`
- Later `PartyMgr` changes mostly added:
  - native `AddHero` resolution
  - party readers (`CountPartyHeroes`, `GetCalledTargetId`)
  - richer party-state helpers
- That makes `PartyMgr` a **low-probability regression source** for kick-all.

#### B. `CtoS` transport
- File: `gwa3/src/packets/CtoS.cpp`
- This is the highest-risk area.
- Before the regression window:
  - packets were sent via a simpler `RenderHook` shellcode path
  - fallback was `GameThread::Enqueue`
- After the regression window:
  - transport was repeatedly rewritten through:
    - raw dispatch experiments
    - mid-function `CtoSHook`
    - sender thread
    - VEH INT3
    - Engine inline hook
- If hero-kick is now silently doing nothing, this packet path is the strongest place to suspect.

#### C. bootstrap / initialization order
- File: `gwa3/src/dllmain.cpp`
- Initialization order changed materially:
  - older flow initialized `GameThread` first
  - newer flow defers `GameThread` until after bootstrap
  - `CtoS::Initialize()` now happens in a different lifecycle context
- If packet transport depends on thread affinity, hook install order, or `PacketLocation` freshness, this can change behavior even when higher-level manager calls are unchanged.

#### D. test logic
- Files:
  - `gwa3/src/tests/IntegrationTestGameplay.cpp`
  - `gwa3/src/tests/IntegrationTestEpic14.cpp`
- Recent test changes mostly made assertions more honest:
  - checking actual party counts
  - dumping raw party state
  - retrying kick once in Froggy
- These changes did **not** create the regression; they made it visible.

### 5.5 One Oddity In The Logs
- Preserved logs from `2026-04-05 15:22` show:
  - `PartyMgr: KickAllHeroes entering`
  - `PartyMgr: KickAllHeroes found 7 heroes, kicking individually`
  - `PartyMgr: Kicking hero 30`, etc.
- That exact implementation does **not** appear in current git history for `PartyMgr.cpp`.
- Most likely explanation:
  - there was a local/uncommitted working-tree experiment at that moment, or
  - the preserved log captured a transient state not represented by a committed revision.
- This matters because it means at least one known-good log may reflect a local variant, not a clean git commit.

### 5.6 Current Best Hypothesis
- The hero-kick regression is most likely not a `PartyMgr` logic bug.
- The highest-probability root cause is a **CtoS dispatch regression introduced after the last known good logs**, especially in the packet-send architecture churn between:
  - `050678e`
  - and the first CtoS rewrite commits starting at `81deef6`

### 5.6.1 Critical Working-Tree Finding
- There is also a **current uncommitted local regression** in:
  - `gwa3/src/packets/CtoS.cpp`
- The current working tree differs from `HEAD` in a way that can directly break packet sends:
  - `EngineDetourNaked()` now dequeues queued commands into `s_savedCommand`
  - but the actual execution line is commented out:
    - `// call dword ptr [s_savedCommand]`
- In the same local diff:
  - `SendPacket()` no longer uses the `PacketSenderThread` ring-buffer path from `HEAD`
  - it instead builds shellcode and enqueues it to the Engine queue
  - so if the detour never executes `s_savedCommand`, queued CtoS packets can become silent no-ops
- This means there are **two overlapping problems to keep separate**:
  1. a real historical regression window in committed CtoS transport changes
  2. a current local working-tree change that can independently suppress packet execution

### 5.7 Best Next Debugging Step
- Compare hero-kick behavior at:
  - `050678e`
  - `81deef6`
  - `66c0090`
  - `0a5f821`
  - `f4e60bf`
- Specifically instrument `CtoS::SendPacket()` for `HERO_KICK` to log:
  - header
  - size
  - final `PacketLocation`
  - calling context / dispatch path taken
  - whether the packet was queued, dropped, or executed
- This should tell us which transport rewrite is the first one where `HERO_KICK(0x26)` stops producing an observable server/client party update.
