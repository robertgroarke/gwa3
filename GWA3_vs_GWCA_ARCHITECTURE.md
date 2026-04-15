# GWA3 vs GWCA: Architecture Comparison

GWA3 is derived from GWCA (Guild Wars Client API) but diverges significantly in design philosophy. GWCA is a **plugin framework** — a shared library for tools like GWToolbox++ to build on. GWA3 is a **self-contained autonomous bot** with AI integration. This document maps every architectural difference.

## Reference Paths

| Codebase | Location |
|---|---|
| GWCA headers | `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/` |
| GWCA source | `toolbox/GWToolboxpp-master/Dependencies/GWCA/Source/` |
| GWA3 headers | `gwa3/include/gwa3/` |
| GWA3 source | `gwa3/src/` |

---

## 1. Manager Mapping

GWCA defines 21 managers. GWA3 preserves most names but merges, splits, and adds several.

### Direct 1:1 Matches

These exist in both codebases with the same name and roughly the same role:

AgentMgr, CameraMgr, ChatMgr, EffectMgr, FriendListMgr, GuildMgr, ItemMgr, MapMgr, MemoryMgr, PartyMgr, PlayerMgr, QuestMgr, StoCMgr, TradeMgr, UIMgr

### Merged or Renamed

| GWCA Manager | GWA3 Equivalent | What Changed |
|---|---|---|
| **SkillbarMgr** | `SkillMgr` | GWA3 merges skillbar array access with skill execution. Skillbar traversal (`GetSkillbarArrayBase()`) and `UseSkill()` / `UseHeroSkill()` live together because they share base pointer logic. Uses a 16-slot shellcode ring buffer instead of repeated VirtualAlloc. |
| **MerchantMgr** | `TradeMgr` | GWA3 folds merchant buy/sell/quote into TradeMgr. Merchant, crafting, and player trade share UI state (quantity prompts, dialog hooks, cart tracking), so separation would duplicate event handling. |
| **GameThreadMgr** | `GameThread` (in `core/`, not `managers/`) | Renamed and moved out of the manager layer. See §3 for the fundamental dispatch redesign. |
| **RenderMgr** | `RenderHook` (in `core/`) | GWCA treats rendering as a manager. GWA3 treats it as a hook — a pre-game bootstrap that self-disables after map load. |
| **EventMgr** | Split into `StoCMgr` + `CallbackRegistry` + `EventPush` | See §6. |

### GWA3-Only Managers (No GWCA Equivalent)

| Manager | Purpose |
|---|---|
| **DialogMgr** (`managers/DialogMgr.h`) | NPC dialog interaction management — tracking dialog state, waiting for specific dialog frames, coordinating multi-step dialog sequences. GWCA leaves this to consumer code. |
| **ChatLogMgr** (`managers/ChatLogMgr.h`) | Separates chat log capture from ChatMgr. Registers StoC callbacks for chat packets and maintains a queryable log buffer. |
| **MaintenanceMgr** (`managers/MaintenanceMgr.h`) | Ported from AutoIt `Utils-Maintenance.au3`. Rare skin detection (~200 known skins), gold management, salvage kit restocking, storage deposit logic. Pure bot-layer functionality. |
| **SkillMgr** (`managers/SkillMgr.h`) | Absorbs GWCA's SkillbarMgr plus adds hero skill management, sparkfly override, and the shellcode ring buffer for game-thread-safe skill execution. |

### GWCA Managers GWA3 Doesn't Implement

| GWCA Manager | Why Missing |
|---|---|
| **Module** (base lifecycle struct) | GWA3 uses explicit `Initialize()`/`Shutdown()` pairs instead of GWCA's Module pattern. See §8. |

---

## 2. Hook Architecture: Centralized vs Distributed

This is the largest philosophical difference between the two codebases.

### GWCA: Centralized Module System

GWCA defines a `Module` struct (`GWCA/Managers/Module.h`) that every manager implements:

```cpp
struct Module {
    const char  *name;
    void        *param;
    void       (*init_module)();
    void       (*exit_module)();
    void       (*enable_hooks)();   // Called from game thread
    void       (*disable_hooks)();  // Called from game thread
};

// Each manager exports one:
extern Module AgentModule;
extern Module StoCModule;
```

Hooks are installed/removed through this uniform 4-function lifecycle. All managers are registered in a central list and initialized together.

### GWA3: Distributed by Hook Type

GWA3 has **no unified hook manager**. Each subsystem owns its hooks independently:

| Hook | File | Technique | Purpose |
|---|---|---|---|
| **RenderHook** | `core/RenderHook.h/.cpp` | Mid-function patch + naked detour | Pre-game bootstrap (character select). 256-entry command queue. Self-disables after map load (~2300 frames). |
| **GameThread** | `core/GameThread.h/.cpp` | MinHook on frame callback | In-game dispatch (~60fps). Three queue types. POD storage. |
| **CtoSHook** | `packets/CtoSHook.h/.cpp` | Inline patch on PacketSend | Client-to-server interception. Watchdog re-patches every ~38s when game integrity checker restores original bytes. |
| **DialogHook** | `core/DialogHook.h/.cpp` | MinHook on UIMessage dispatcher | Dialog/frame tracking, `WaitForUIMessage()` primitives, `RecentUITrace()` for debugging dialog hangs. |
| **TraderHook** | `core/TraderHook.h/.cpp` | Function pointer patch | Captures merchant/crafter quote responses. |
| **TradePartnerHook** | `core/TradePartnerHook.h/.cpp` | Function pointer patch | Player-to-player trade event tracking. |
| **TargetLogHook** | `core/TargetLogHook.h/.cpp` | Function pointer patch | Target selection capture. |

**Why distributed?** Each hook uses a different technique (MinHook, inline patch, naked detour, function pointer). Isolating them means:
- Failures are debuggable per-hook (critical when one crash can take down the game client)
- Independent lifecycles (RenderHook disables after map load, others persist)
- No coupling between unrelated subsystems

---

## 3. Game Thread Dispatch: The CRT Heap Problem

This is the most important implementation difference. GWCA's approach has a known crash bug that GWA3 solves.

### GWCA: std::function Queue

```cpp
// GWCA/Managers/GameThreadMgr.h
void Enqueue(std::function<void()> f, bool force_enqueue = false);
```

`std::function` allocates on the CRT heap when the callable exceeds the small-buffer optimization threshold. On the game thread, CRT heap operations (`new`/`delete`) can corrupt GW's internal memory, causing a crash approximately 11 seconds later.

### GWA3: POD InlineTask with Raw Storage

```cpp
// gwa3/include/gwa3/core/GameThread.h
using RawInvoker = void(*)(void* storage);
void EnqueueRaw(RawInvoker invoker, const void* data, size_t dataSize);
```

Internally uses an `InlineTask` struct with stack-local storage:

```cpp
// gwa3/src/core/GameThread.cpp
struct InlineTask {
    using Invoker = void(*)(void* storage);
    Invoker invoke;
    alignas(8) char storage[96];  // No CRT heap allocation
    void operator()() { if (invoke) invoke(storage); }
};
```

GWA3 provides three dispatch queues:

| Queue | Drain Strategy | Use Case |
|---|---|---|
| `s_preQueue` | Bulk drain (all items per frame) | Most game commands |
| `s_serialPreQueue` | One item per frame | Matches GWA2 AutoIt semantics (some game operations need frame spacing) |
| `s_postQueue` | Bulk drain after game callback | Post-processing |

GWA3 also provides `Enqueue(std::function<void()>)` for convenience, but the critical path uses `EnqueueRaw()`.

---

## 4. Memory Scanning

### GWCA Scanner (`GWCA/Utilities/Scanner.h`)

Full-featured scanner with string search:

```
Initialize(moduleName | hModule)
Find(pattern, mask, offset, section)
FindInRange(pattern, mask, offset, start, end)
FindAssertion(file, message, line, offset)    // Xref from assertion strings
FindUseOfString(str | wstr, offset, section)  // String reference search
FindNthUseOfString(str, nth, offset)
FunctionFromNearCall(address)                 // E8 rel32 resolution
ToFunctionStart(address)                      // Backscan to prologue
GetSectionAddressRange(section)
IsValidPtr(pointer)
```

### GWA3 Scanner (`core/Scanner.h`)

Streamlined — drops string search, adds branch resolution:

```
Initialize(hModule)
Find(pattern, mask, offset)                  // Default .text section
FindInRange(pattern, mask, offset, start, size)
FindAssertion(file, message, offset)         // Simplified: no line number param
FunctionFromNearCall(address)
ResolveBranchChain(address, maxDepth)        // NEW: follows E8/E9/EB chains
ToFunctionStart(address, maxDistance)
GetTextSection() / GetRdataSection() / GetDataSection()
IsInitialized()
```

`FindUseOfString` is replaced by the Offsets system (§5).

---

## 5. Offset Resolution (GWA3-Only)

GWCA scatters pattern definitions across individual managers — each manager scans for its own offsets during `init_module()`. GWA3 centralizes this.

### `core/Offsets.h` / `core/Offsets.cpp`

```cpp
namespace GWA3::Offsets {
    enum class Priority { P0, P1, P2 };  // Required / Important / Optional
    enum class PatternType { Ptr, Func, Hook };

    bool ResolveAll();        // Single call resolves all registered patterns
    int GetResolvedCount();
    int GetFailedCount();

    // 50+ offsets organized by subsystem:
    extern uintptr_t BasePointer;     // Ptr
    extern uintptr_t PacketSend;      // Func
    extern uintptr_t Move;            // Func
    extern uintptr_t ChangeTarget;    // Func
    extern uintptr_t UseSkill;        // Func
    extern uintptr_t Render;          // Hook seam
    // ...
};
```

**Benefits over GWCA's approach:**
- Priority levels mean P0 failures abort initialization, P2 failures are tolerated
- Single `ResolveAll()` call in `dllmain.cpp` — all scanning happens in one place
- `PatternType` hints guide scan strategy
- Failure reporting is centralized (`GetFailedCount()`)

---

## 6. Event System: Three Layers

GWCA has a single event layer (StoC callbacks). GWA3 splits events into three layers serving different consumers.

### Layer 1: StoC Callbacks (both codebases)

Raw server-to-client packet dispatch. Nearly identical API:

```cpp
// Both GWCA and GWA3:
bool RegisterPacketCallback(HookEntry* entry, uint32_t header,
                            const PacketCallback& callback, int altitude);
bool RegisterPostPacketCallback(HookEntry* entry, uint32_t header,
                                const PacketCallback& callback);
```

GWA3 adds `EmulatePacket(PacketBase* packet)` for testing — GWCA doesn't have this.

### Layer 2: UI Message Callbacks (GWA3-only: `core/CallbackRegistry.h`)

Per-UIMessage-id callback dispatch with altitude sorting. Registered through `DialogHook` which intercepts the game's `UIMessage()` function.

### Layer 3: LLM Event Push (GWA3-only: `llm/EventPush.h`)

Registers StoC callbacks for key game events and converts them to JSON for the LLM bridge:
- MapLoaded, PartyDefeated, AgentState, SkillActivate, DamageDealt, ItemDropped, etc.

This layer exists because the LLM bridge needs a high-level event stream, not raw packet data.

---

## 7. Packet Handling: CtoS Architecture

### GWCA

GWCA has `CtoS::SendPacket()` as a thin wrapper around the game's native packet function. No interception of outgoing packets.

### GWA3: Split Architecture

GWA3 splits client-to-server into three components:

| Component | File | Role |
|---|---|---|
| **CtoS** | `packets/CtoS.h/.cpp` | Type-safe send wrappers: `SendPacket()`, `MoveToCoord()`, `Dialog()`, `ChangeTarget()`, `ActionAttack()`, `CancelAction()`. Also provides `PacketTapSnapshot` for diagnostics. |
| **CtoSHook** | `packets/CtoSHook.h/.cpp` | Inline hook on the game's `PacketSend` function. Allows interception and command queueing. Includes a watchdog thread that re-patches when the game's integrity checker restores the original bytes (~38 second cycle). |
| **Headers** | `packets/Headers.h` | Packet header constants (opcodes). |

The CtoSHook watchdog is unique to GWA3 — GWCA doesn't need it because GWToolbox++ handles anti-cheat differently.

---

## 8. Module Lifecycle

### GWCA: Dynamic Module System

```cpp
// Each manager is a Module with 4 lifecycle functions:
struct Module {
    void (*init_module)();
    void (*exit_module)();
    void (*enable_hooks)();
    void (*disable_hooks)();
};

// Registered in a central list, initialized together
```

Allows runtime enable/disable of individual managers.

### GWA3: Explicit Init Sequence

`dllmain.cpp` calls each subsystem in dependency order:

```
Phase 1 — Core:        Scanner → Offsets::ResolveAll() → Memory::EnableAllPatches()
Phase 2 — Hooks:       RenderHook → GameThread → DialogHook → CtoSHook → TraderHook → ...
Phase 3 — Managers:    AgentMgr → MapMgr → ItemMgr → StoCMgr → CtoS → ...
Phase 4 — High-Level:  Bot → LlmBridge
```

Shutdown reverses the order. No dynamic registration — the sequence is hardcoded and deterministic.

**Trade-off:** Less flexible than GWCA's module system, but simpler to debug and guarantees correct initialization order.

---

## 9. Game Type Definitions

Both codebases reverse-engineer the same GW memory structures. The types are nearly identical.

### Key Differences

| Aspect | GWCA | GWA3 |
|---|---|---|
| **Offset annotations** | Descriptive field names | Inline hex offset comments (`/* +h0000 */`, `/* +h0004 */`) |
| **Agent subclasses** | `AgentLiving`, `AgentItem`, `AgentGadget` | Same, cross-referenced with AutoIt `$AGENT_STRUCT_TEMPLATE` |
| **File organization** | `GameEntities/` directory | `game/` directory: Agent.h, Item.h, Skill.h, Map.h, Party.h, Quest.h, Player.h, Camera.h, Guild.h, Title.h, Effect.h |
| **TLink type** | Templated `TLink<T>` | Non-template `TLink` with `void* prev/next` (avoids template complexity for memory-mapped structs) |
| **Context pointers** | `Context/` directory (GameContext, MapContext, etc.) | Inlined into `GameTypes.h` |

---

## 10. Features Unique to GWA3

### LLM Bridge (`llm/`)

An entire AI integration layer with no GWCA equivalent:

| Component | File | Purpose |
|---|---|---|
| **IpcServer** | `llm/IpcServer.h` | Named pipe server (`\\.\pipe\gwa3_llm`). Length-prefixed JSON messages. Thread-safe. |
| **EventPush** | `llm/EventPush.h` | Converts StoC packets to high-level JSON events for the LLM. |
| **GameSnapshot** | `llm/GameSnapshot.h` | Point-in-time JSON serialization of full game state (agents, inventory, skillbar, enemies). |
| **ActionExecutor** | `llm/ActionExecutor.h` | Parses JSON action commands from the LLM, dispatches via GameThread. |
| **LlmBridge** | `llm/LlmBridge.h` | Orchestrator: coordinates IPC, events, snapshots, and action execution. |

Python bridge (`gwa3/bridge/`) connects to the named pipe and runs an LLM agent loop (Gemma 4 32B).

### Bot Framework (`bot/`)

Built-in dungeon automation:

- **BotFramework**: State machine (Idle → CharSelect → InTown → Traveling → InDungeon → Looting → Merchant → Maintenance)
- **Dungeon system**: Navigation, combat, dialog, loot, quest logic
- **Dungeon bots**: FroggyHM (Bogroot Growths), Kathandrax, Raven's Point, Rragar's Menagerie, Frostmaw's Burrows, Arachni's Haunt
- **BotModuleRegistry**: Dynamic bot plugin registration and runtime selection

### Inline Memory Patches (`core/Memory.h`)

Named patches with enable/disable:

```cpp
struct Patch { SetPatch(), SetRedirect(), Enable(), Disable() };
// Named patches: CameraUnlock, LevelDataBypass, MapPortBypass
```

GWCA patches memory but doesn't name or track patches as first-class objects.

### Dialog Tracking (`core/DialogHook.h`)

`WaitForUIMessage()`, `WaitForDialogUIMessage()`, and `RecentUITrace()` — primitives for multi-step dialog automation that GWCA leaves to consumer code.

### Watchdog Thread Protection

Both `RenderHook` and `CtoSHook` run watchdog threads that detect when the game's integrity checker restores patched bytes and re-apply the patches. GWCA relies on GWToolbox++ for this.

### Maintenance Manager (`managers/MaintenanceMgr.h`)

Bot-layer inventory management ported from AutoIt:
- Rare skin detection (~200 known skins)
- Gold management and storage deposit
- Salvage kit restocking
- Junk sale filtering

---

## 11. Features in GWCA Not in GWA3

| GWCA Feature | Status in GWA3 |
|---|---|
| **Dynamic module registration** | Not implemented — static init sequence |
| **FindUseOfString / FindNthUseOfString** | Replaced by centralized Offsets system |
| **RenderMgr** (as a manager) | Replaced by RenderHook (core hook) |
| **Trade-related UI widgets** | Partially — GWA3 automates trade but doesn't render UI |
| **Multi-consumer hook sharing** | Simplified — each hook serves one primary consumer |

---

## 12. Philosophy Summary

| Aspect | GWCA | GWA3 |
|---|---|---|
| **Purpose** | Plugin framework for human-operated tools | Autonomous bot with AI integration |
| **Hook design** | Centralized Module system | Distributed by hook type |
| **Game thread safety** | std::function (CRT heap risk) | POD InlineTask (stack-safe) |
| **Offset resolution** | Scattered per-manager | Centralized Offsets registry with priorities |
| **Packet handling** | Send-only | Send + intercept + watchdog |
| **Module lifecycle** | Dynamic registration | Static init sequence in dllmain |
| **Event system** | Single layer (StoC) | Three layers (StoC + UIMessage + LLM events) |
| **AI integration** | None | Full LLM bridge with IPC, snapshots, action execution |
| **Bot automation** | None (consumer responsibility) | Built-in dungeon bots with state machine |
| **Error philosophy** | Trust the host (GWToolbox) | Self-contained: watchdog threads, integrity checks, self-healing hooks |
