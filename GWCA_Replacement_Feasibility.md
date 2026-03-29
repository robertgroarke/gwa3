# GWCA Replacement Feasibility Study

## Executive Summary

**Goal:** Replace the closed-source `gwca.dll` with a custom implementation ("GWA3") built from reverse-engineering research and existing AutoIt infrastructure.

**Verdict: Achievable.** The existing codebase already handles ~70% of what GWCA provides. The remaining 30% (frame UI system, hook infrastructure, UIMessage dispatch) has been thoroughly reverse-engineered across 8 research documents. A phased approach targeting Froggy HM's specific needs first makes this practical rather than theoretical.

---

## Current Architecture

### What gwca.dll Actually Does

GWCA (Guild Wars Client API) is a C++ DLL injected into the GW process. It provides:

| Layer | What It Does | Already Have Alternative? |
|-------|-------------|--------------------------|
| **Pattern Scanner** | Finds game functions by byte patterns at runtime | **YES** — GWA2_Assembly.au3 has its own 48-pattern scanner |
| **Hook Installer** | Patches game function entrypoints with JMP detours | **PARTIAL** — GWA2_Assembly.au3 installs 5 detours (Engine, Render, LoadFinished, Trader, TradePartner) |
| **Command Queue** | Queues bot commands for game-thread execution | **YES** — 256-byte slot queue via WriteProcessMemory |
| **Frame UI System** | Frame traversal, hash lookup, button clicks | **PARTIAL** — GWA2_FrameUI.au3 does this natively but still reads some GWCA data pointers |
| **UIMessage Dispatch** | Global + frame-local UI message routing with callbacks | **NO** — currently relies on GWCA's hooked dispatch |
| **Manager Modules** | ~15 manager classes (Agent, Item, Skill, Party, etc.) | **YES** — GWA2.au3 implements all of these via direct memory + packet send |
| **Hook Record Pool** | VirtualAlloc RWX pages with relocated replay stubs | **NO** — only needed if replicating GWCA's generic hooking |

### The Key Dependency

The bot currently requires gwca.dll for **one critical thing**: the frame-based UI system used for character select (Play button click) and merchant dialogs (crafting). Everything else — movement, skills, combat, inventory, trading, hero management, map travel — goes through GWA2_Assembly.au3's own injection.

Specifically, `GWA2_FrameUI.au3` currently:
1. Locates `gwca.dll` in the GW process
2. Reads data pointers from GWCA's data section (offsets `+0x8A39C`, `+0x8A3A0`, `+0x8A37C`, `+0x8A410`, `+0x8A3B0`)
3. Uses GWCA's `ButtonClick` function or calls the game's `SendFrameUIMsg` directly

**The research has already produced a working native alternative** in `GWA2_FrameUI.au3` that calls the game's `SendFrameUIMsg` directly without GWCA. The remaining work is eliminating all GWCA data pointer dependencies and stabilizing the native path.

---

## What We Have vs What We Need

### Already Working (No GWCA Required)

These systems are fully implemented in GWA2_Assembly.au3 + GWA2.au3:

| System | Implementation | Scan Patterns |
|--------|---------------|---------------|
| Agent reading | Direct memory read from AgentBase array | `AgentBase`, `MyID`, `CurrentTarget`, `AgentCopy*` |
| Movement | Command queue → `Move` function pointer | `Move`, `ClickCoords`, `Action`, `ActionBase` |
| Skill usage | Command queue → `UseSkill`/`UseHeroSkill` ptrs | `UseSkill`, `UseHeroSkill`, `SkillBase`, `SkillTimer` |
| Packet sending | Command queue → `PacketSend` function | `PacketSend`, `PacketLocation` |
| Item management | Packet headers (0x72 move, 0x7E use, 0x6C identify, 0x77-0x7B salvage) | `SalvageGlobal`, `Salvage` |
| NPC transactions | Direct `Transaction` function call | `Transaction`, `RequestQuote`, `BuyItemBase` |
| Hero management | Packet headers (0x1E add, 0x1F kick, 0x15 behavior, 0x1C skill) | `HeroCommand`, `HeroSkills`, `AiMode` |
| Map travel | Packet header 0xB1 | `InstanceInfo`, `Region`, `AreaInfo` |
| Map loading | Hook detour on load completion | `LoadFinished` (hook) |
| Rendering toggle | Hook detour on render loop | `Render` (hook) |
| Trade system | Packet headers + hook | `Trader` (hook), `TradePartner` (hook) |
| Chat | Packet header 0x64 + chat log hook | `ChatLogStart` (hook in Assembly_Chatlog.au3) |
| Dialog | Packet header 0x3B | `Dialog` |
| Attributes | `IncreaseAttribute`/`DecreaseAttribute` function calls | `AttributeInfo`, `IncreaseAttribute`, `DecreaseAttribute` |
| Quest tracking | Packet headers + memory reads | `ActiveQuest` |
| Difficulty toggle | Direct function call | `SetDifficulty` |
| Friend list | Direct function calls | `FriendList`, `PlayerStatus`, `AddFriend`, `RemoveFriend` |
| PE section parsing | Used for targeted scanning | In Utils.au3 |

**Total: 48 scan patterns, 100+ packet headers, 5 installed hooks, 80+ command functions.**

### Needs GWCA Replacement (The Actual Gap)

| Capability | Current State | What's Needed |
|-----------|---------------|---------------|
| **Frame hash lookup** | `GetFrameByHash()` in GWA2_FrameUI.au3 — reads FrameArray from BotsHub scan, walks hash table | Scan for FrameArray natively (already done via `FrMsg.cpp` assertion pattern) |
| **SendFrameUIMsg** | Scanned natively via `83 C1 DC E8` pattern in GWA2_FrameUI.au3 | Already working — just need to remove GWCA fallback paths |
| **ButtonClick** | Native shellcode in GWA2_FrameUI.au3 builds kMouseAction struct, calls SendFrameUIMsg | Working — needs stabilization and integration into command queue |
| **GWCA data pointers** | `_InitGWCAForButtonClick()` writes to GWCA offsets +0x8A39C/A0/7C/410/B0 | Eliminate entirely — store frame pointers in our own data section |
| **Root frame pointer** | Currently read from GWCA +0x8A410 or scanned | Scan for game's RootFrame directly (pattern exists in research) |
| **GetChildFrame** | Currently read from GWCA +0x8A37C | Scan for game function directly |
| **Frame hash seeds** | Currently read from GWCA +0x880F0-880F8 | Scan or hardcode (rarely changes) |
| **Character select flow** | Uses ButtonClick for Play button + disconnect popup handling | Already working natively in GWA2_FrameUI.au3 |
| **Merchant frame clicks** | Uses ButtonClick for craft/buy UI | test_craft_native.au3 proves this works without GWCA |

---

## Phased Implementation Plan

### Phase 0: Preparation & Validation (1-2 sessions)

**Goal:** Confirm all native scan patterns work on current GW client, establish test harness.

#### Tasks

1. **Audit current GWCA references**
   - Grep all `.au3` files for `gwca`, `8A39C`, `8A3A0`, `8A37C`, `8A410`, `8A3B0`, `880F0`
   - Catalog every line that reads from or writes to gwca.dll's data section
   - Map each reference to its native replacement (game function scan or own data storage)

2. **Verify native frame scans work**
   - Run `ExtendScanner_FrameUI()` on a fresh GW client (no gwca.dll loaded)
   - Confirm `SendFrameUIMsg` pattern `83 C1 DC E8` still resolves
   - Confirm `FrameArray` assertion pattern from GWA2_Assembly.au3 still resolves
   - Document the exact addresses found

3. **Create standalone test script**
   - `debug_scripts/test_no_gwca.au3` — boots a fresh GW client, runs full scan set, logs all addresses
   - Verify: all 48 GWA2_Assembly patterns + SendFrameUIMsg + FrameArray resolve without gwca.dll present
   - This is the **go/no-go gate** for the entire project

4. **Document scan pattern health**
   - Create a pattern status table: pattern name, hex bytes, expected result, actual result, pass/fail
   - Any pattern that fails on current client needs updating before proceeding

#### Deliverables
- [ ] Complete GWCA reference audit (file, line, what it reads, replacement strategy)
- [ ] `test_no_gwca.au3` passing all scans
- [ ] Pattern health report

---

### Phase 1: Native Frame Infrastructure (2-3 sessions)

**Goal:** Complete frame UI system that works without gwca.dll.

#### 1A: Native Frame Pointer Resolution

Currently `GWA2_FrameUI.au3` gets frame pointers from GWCA's data section. Replace with native scans.

| GWCA Offset | Purpose | Native Replacement |
|------------|---------|-------------------|
| `+0x8A39C` | Original SendFrameUIMsg game address | Already scanned natively via `83 C1 DC E8` pattern |
| `+0x8A3A0` | GWCA hook handle (replay stub) | **Not needed** — we call the game function directly |
| `+0x8A37C` | GetChildFrame game address | Add new scan pattern for this function |
| `+0x8A410` | RootFrame pointer | Add new scan pattern (research docs have the byte signature) |
| `+0x8A3B0` | Frame hash table address | Derive from FrameArray scan or add dedicated pattern |
| `+0x880F0-F8` | Hash seeds | Scan or extract from game binary at init |

**Concrete work:**
- Add 3-4 new scan patterns to `RegisterAllScanPatterns()` or `ExtendScanner_FrameUI()`
- Store results in GWA2's existing `$labels_map` via `SetLabel()`
- Remove all reads from `$gwcaBase + offset` in GWA2_FrameUI.au3
- Replace `_InitGWCAForButtonClick()` with `_InitNativeFrameUI()` that uses scanned addresses

#### 1B: Frame Hash Traversal

`GetFrameByHash()` currently works but depends on FrameArray from BotsHub's scan. Verify and harden:

- Confirm FrameArray scan returns the correct hash table base
- Validate hash traversal logic against multiple known frame hashes
- Test: resolve all 8 known character-select frame hashes without gwca.dll
- Test: resolve merchant dialog frame hashes (from GWCA_Crafting_Research.md)

#### 1C: Native ButtonClick via Command Queue

Currently `ClickButtonByHash()` injects standalone shellcode via `CreateRemoteThread`. This works but is fragile. Better approach: route through the existing command queue.

- Add new command type to GWA2_Assembly.au3's MainProc: `CommandFrameClick`
- Command struct: `{ptr funcPtr; dword frameAddr; dword msgId; dword actionState}`
- MainProc handler: build kMouseAction struct on stack, set ECX = frame+0xA8, call SendFrameUIMsg
- This ensures frame clicks execute on the game thread (same as all other commands)

#### Deliverables
- [ ] All GWCA data pointer reads replaced with native scans
- [ ] `_InitGWCAForButtonClick()` removed, replaced with `_InitNativeFrameUI()`
- [ ] `GetFrameByHash()` working without gwca.dll
- [ ] `ClickButtonByHash()` routed through command queue
- [ ] Test: Play button click at character select on fresh GW (no gwca.dll)
- [ ] Test: Merchant dialog craft click on fresh GW (no gwca.dll)

---

### Phase 2: UIMessage Native Dispatch (2-3 sessions)

**Goal:** Send global UIMessages without GWCA's hook chain.

#### Background

GWCA's UIMessage system intercepts the game's native `SendUIMessage` dispatcher and routes messages through a callback chain. For bot purposes, we don't need callbacks — we just need to **call the game's native UIMessage dispatcher directly**.

The research documents the game's UIMessage dispatcher:
- Found via scan pattern at offset -0x14 from a known signature
- Calling convention: `__cdecl SendUIMessage(UIMessage msgid, void* wParam, void* lParam)`

#### 2A: Scan for Native UIMessage Dispatcher

- The existing `UIMessage` scan pattern in GWA2_Assembly.au3 already finds this: `B900000000E8000000005DC3894508` at offset -20
- Verify this still resolves on current client
- The scanned address points to the game's own dispatcher — no GWCA hook needed

#### 2B: Add UIMessage Command to Queue

- Add `CommandUIMessage` to MainProc's command dispatch
- Struct: `{ptr funcPtr; dword msgId; ptr wParam; ptr lParam}`
- Handler: `call [funcPtr]` with cdecl convention
- This lets AutoIt send any UIMessage through the game thread

#### 2C: Implement Key UIMessage Wrappers

Based on research, these UIMessages replace packet-based operations with cleaner alternatives:

| UIMessage | ID | Purpose | Replaces |
|-----------|-----|---------|----------|
| kTravel | game-specific | Map travel | `$HEADER_MAP_TRAVEL` packet |
| kMoveItem | 0x30000005 relay | Move inventory item | `$HEADER_ITEM_MOVE` packet |
| kSendUseItem | 0x30000008 relay | Use consumable | `$HEADER_ITEM_USE` packet |
| kSendLoadSkillbar | 0x30000003 relay | Load skill bar | `$HEADER_LOAD_SKILLBAR` packet |
| kSendDialog | 0x30000001 relay | Send dialog | `$HEADER_DIALOG_SEND` packet |
| kLogout | game-specific | Logout / char select | Custom implementation |
| kChangeTarget | game-specific | Change target | `ChangeTarget` function |

**Important:** The 0x30000000-family relay messages are GWCA-internal. They require GWCA's hook chain to work. For a GWCA-free implementation, we should either:
- Call the underlying native game functions directly (preferred — we already do this for most operations)
- Or implement our own minimal hook on SendUIMessage to intercept relay messages

**Recommendation:** Skip relay messages entirely. The existing packet-based approach already works for all of these. UIMessage dispatch is only needed for operations that *cannot* be done via packets (e.g., some UI state changes).

#### Deliverables
- [ ] `UIMessage` scan pattern verified on current client
- [ ] `CommandUIMessage` added to command queue
- [ ] Wrapper functions for any UIMessages that provide capability beyond current packet system
- [ ] Test: UIMessage-based operations on fresh GW (no gwca.dll)

---

### Phase 3: Eliminate gwca.dll Injection (1-2 sessions)

**Goal:** Remove gwca.dll from the boot sequence entirely.

#### 3A: Update GWLauncher Integration

Currently the GW launch process injects gwca.dll before the bot attaches. Modify `GWLauncher.au3` to:
- Skip gwca.dll injection
- Launch GW client directly (or via GWLauncher with GWCA disabled)
- Verify the bot's own injection (GWA2RAPI header) still works standalone

#### 3B: Remove All GWCA Fallback Code

- Delete `_InitGWCAForButtonClick()` and all code that searches for `gwca.dll` module
- Remove GWCA data offset constants (`$GWCA_DATA_SENDFRAME_ORIG`, etc.)
- Clean up debug scripts that reference gwca.dll offsets
- Update `GWA2_FrameUI.au3` to only use native paths

#### 3C: Full Integration Test

Run the complete Froggy HM flow on a fresh GW client with no gwca.dll:

1. **Launch** — GW starts, bot attaches, all scans resolve
2. **Character select** — Play button click works via native frame click
3. **Map loading** — LoadFinished hook fires
4. **Outpost** — Hero setup, skill loading, consumable usage
5. **Travel** — Map travel to Bogroot Growths
6. **Combat** — Full dungeon run with skills, movement, targeting
7. **Loot** — Item pickup, identification, salvage
8. **Merchant** — Sell items, craft consumables (native frame clicks)
9. **Repeat** — Long-run stability (10+ consecutive runs)

#### Deliverables
- [ ] gwca.dll injection removed from launch sequence
- [ ] All GWCA references purged from codebase
- [ ] Froggy HM completing full runs without gwca.dll
- [ ] 10+ consecutive runs without crash

---

### Phase 4: Hardening & Extended Coverage (Ongoing)

**Goal:** Stability, maintainability, and coverage of edge cases.

#### 4A: Disconnect/Reconnect Handling

Character select interactions after disconnect require frame clicks:
- Disconnect popup YES/NO buttons (hashes: 1398610279, 3600335809)
- Character re-select and Play button
- Verify the native frame click handles all reconnect scenarios

#### 4B: Pattern Maintenance System

GW client updates can shift byte patterns. Build resilience:
- Create `pattern_health_check.au3` — runs all scans and reports pass/fail
- Log pattern match addresses to file for diff across client versions
- For each pattern, document what game function it targets and alternative signatures
- Consider assertion-based patterns (file + message string) as backup — these survive minor code changes

#### 4C: Extended Frame UI Operations

Beyond ButtonClick, document and implement:
- Dropdown selection (frame message 0x60)
- Checkbox toggle (frame messages 0x56/0x57)
- Text input (frame messages 0x5D/0x5A)
- Scroll/page change (frame message 0x7FFFFFF5)
- Tab selection (frame messages 0x59/0x5C)

These aren't needed for Froggy HM but expand capability for other bots.

#### 4D: Chat Hook Without GWCA

The chat log hook in `GWA2_Assembly_Chatlog.au3` is already independent of GWCA. Verify it still works without gwca.dll in the process.

#### Deliverables
- [ ] Disconnect/reconnect flow stable
- [ ] Pattern health check script
- [ ] Extended frame operations (if needed for other scripts)
- [ ] Chat hook verified standalone

---

## Technical Deep Dives

### How Frame Hash Lookup Works (Native)

The game maintains a hash table of UI frames. Each frame has a 32-bit hash at offset `+0x134` (within the FrameRelation struct at `+0x128`).

```
FrameArray (scanned via FrMsg.cpp assertion)
  → Array of frame pointers
  → Each frame: struct at base address
    → +0x128: FrameRelation (parent ptr, sibling ptrs, hash)
    → +0x134: frame_hash_id (uint32)
    → +0xB8:  child_offset_id
    → +0xBC:  frame_id
    → +0x18C: frame_state (bits: 0x4=created, 0x200=hidden, 0x10=disabled)
```

`GetFrameByHash()` walks the array comparing `+0x134` to the target hash. When found, returns the frame base address.

### How Native ButtonClick Works

Replicated from GWCA research — the exact chain:

```
1. GetFrameByHash(hash) → frame_ptr
2. Read frame_state at frame_ptr + 0x18C
3. Check: (state & 0x4) != 0 AND (state & 0x210) == 0  [created, not hidden/disabled]
4. Read frame_id at frame_ptr + 0xBC
5. Read child_offset_id at frame_ptr + 0xB8
6. Get parent frame: ptr at frame_ptr + 0x128, subtract 0x128
7. Compute ECX = parent_frame + 0xA8  [callback array, used as __thiscall this]
8. Build kMouseAction struct on stack:
     { frame_id, child_offset_id, action_state=7 }  [7 = MouseUp]
9. Call SendFrameUIMsg(ECX, msgid=0x31, wParam=&kMouseAction, lParam=0)
```

GWCA sends ONLY MouseUp (action_state=7). Not MouseDown+MouseUp. This is confirmed in the research.

### How the Command Queue Works

```
AutoIt side:                          Game process side:

Enqueue(struct_ptr, size)             MainProc (detoured game loop):
  │                                     │
  ├─ WriteProcessMemory(              ├─ Read QueueCounter
  │    dest = QueueBase +             ├─ If new commands:
  │           QueueCounter * 256,     │   ├─ Read 256-byte command slot
  │    src  = struct_ptr,             │   ├─ Dispatch by command type:
  │    size = size)                   │   │   ├─ CommandMove
  │                                   │   │   ├─ CommandUseSkill
  ├─ QueueCounter = (QueueCounter     │   │   ├─ CommandPacketSend
  │    + 1) mod QueueSize             │   │   ├─ CommandFrameClick  ← NEW
  │                                   │   │   └─ ...
                                      │   └─ Increment processed counter
```

Adding `CommandFrameClick` to this queue means frame clicks execute on the game thread, same as all other bot operations. This eliminates the need for standalone `CreateRemoteThread` calls.

### Scan Patterns That Must Work

These are the **critical** patterns for a GWCA-free bot. If any fail after a GW update, the bot breaks:

| Priority | Pattern | Hex | Finds |
|----------|---------|-----|-------|
| **P0** | BasePointer | `506A0F6A00FF35` | Root game data structure |
| **P0** | AgentBase | `8B0C9085C97419` | Agent array for all entities |
| **P0** | Move | `558BEC83EC208D45F0` | Movement function |
| **P0** | PacketSend | `C747540000000081E6` | Packet send function |
| **P0** | Engine | (hook pattern) | Main game loop for command queue |
| **P0** | Render | (hook pattern) | Rendering toggle |
| **P0** | LoadFinished | (hook pattern) | Map load detection |
| **P1** | SendFrameUIMsg | `83 C1 DC E8` | Frame UI message dispatch |
| **P1** | FrameArray | FrMsg.cpp assertion | Frame hash table |
| **P1** | UseSkill | `85F6745B83FE1174` | Skill activation |
| **P1** | SkillBase | `69C6A40000005E` | Skill database |
| **P1** | Transaction | `85FF741D8B4D14EB08` | NPC buy/sell |
| **P2** | UIMessage | `B900000000E8000000005DC3894508` | Global UI dispatch |
| **P2** | Dialog | `894B248B4B2883E900` | Dialog send |
| **P2** | All remaining 35 patterns | Various | Extended functionality |

---

## Risk Assessment

### Low Risk
- **Movement, skills, combat, inventory** — already working without GWCA, battle-tested across hundreds of runs
- **Packet sending** — direct memory injection, no GWCA dependency
- **Agent/item/skill reading** — direct memory reads, no GWCA dependency

### Medium Risk
- **Frame ButtonClick stability** — native path works in tests but hasn't had long-run validation
- **Pattern breakage on GW update** — any client update could shift patterns; need monitoring
- **Command queue frame click** — new command type needs careful testing for thread safety

### High Risk
- **Nothing** — the high-risk items (hook installer, replay stubs, code relocation) are all things we're choosing NOT to reimplement. By calling game functions directly instead of hooking them, we avoid GWCA's most complex subsystems entirely.

### Mitigations

| Risk | Mitigation |
|------|-----------|
| Pattern shifts | Assertion-based patterns (file+message) survive minor changes; maintain 2+ patterns per critical function |
| Frame click crashes | Queue-based execution ensures game-thread safety; add null checks on frame pointers |
| Merchant dialog changes | Frame hashes are content-based, not address-based — survive code changes if UI layout unchanged |
| Long-run stability | Phase 3C integration test demands 10+ consecutive runs before declaring success |

---

## What We're NOT Building

To keep scope realistic, these GWCA features are explicitly out of scope:

| Feature | Why Skip It |
|---------|------------|
| Generic hook installer | We don't need to hook arbitrary functions — we call them directly |
| Hook record pool (VirtualAlloc RWX pages) | Only needed for generic hooking's replay stubs |
| Code relocator (prologue analysis + rewrite) | Only needed for generic hooking |
| Callback altitude system | Only needed for GWCA's hook dispatch chain |
| UIMessage relay messages (0x30000000 family) | These are GWCA-internal; packet-based alternatives exist for all of them |
| Manager class hierarchy | Our flat function library is simpler and already works |
| GWCA's re-entrancy guards | Not needed when calling functions directly (no hook dispatch) |

---

## Architecture Comparison

```
CURRENT (with gwca.dll):

  GW.exe ←──── gwca.dll (hooks game functions)
    ↑               ↑
    │               │ reads data pointers
    │               │
    └── GWA2_Assembly.au3 (own injection, command queue)
              ↑
              │
        GWA2_FrameUI.au3 (frame clicks via GWCA ButtonClick or native)
              ↑
        Bot scripts (Froggy_HM, etc.)


TARGET (no gwca.dll):

  GW.exe
    ↑
    │ direct calls + memory reads
    │
    └── GWA2_Assembly.au3 (own injection, command queue, frame clicks)
              ↑
              │
        GWA2_FrameUI.au3 (all-native frame system)
              ↑
        Bot scripts (Froggy_HM, etc.)
```

The target architecture is actually **simpler** — one injection layer instead of two.

---

## Estimated Effort

| Phase | Sessions | Complexity | Dependencies |
|-------|----------|-----------|-------------|
| Phase 0: Validation | 1-2 | Low | None |
| Phase 1: Native Frames | 2-3 | Medium | Phase 0 pass |
| Phase 2: UIMessage | 2-3 | Medium | Phase 1 |
| Phase 3: Eliminate gwca.dll | 1-2 | Low | Phase 1 (Phase 2 optional) |
| Phase 4: Hardening | Ongoing | Low-Medium | Phase 3 |

**Critical path:** Phase 0 → Phase 1 → Phase 3. Phase 2 (UIMessage) is nice-to-have but not blocking — the existing packet-based approach handles everything Froggy HM needs.

**Minimum viable: 4-7 sessions** to have Froggy HM running without gwca.dll.

---

## Appendix A: All GWCA Data Pointers Used

From `GWA2_FrameUI.au3` and debug scripts:

| GWCA Offset | Constant Name | Purpose | Native Replacement |
|-------------|--------------|---------|-------------------|
| `+0x8A39C` | `$GWCA_DATA_SENDFRAME_ORIG` | Scanner-discovered SendFrameUIMsg address | Scan `83 C1 DC E8` pattern |
| `+0x8A3A0` | `$GWCA_DATA_SENDFRAME_HOOK` | Hook replay handle — GWCA wrapper returns false if NULL | **Not needed** — we call game function directly |
| `+0x8A37C` | `$GWCA_DATA_GETCHILDFRAME` | GetChildFrame game function | Add scan pattern |
| `+0x8A410` | `$GWCA_DATA_ROOTFRAME` | Root frame pointer | Add scan pattern |
| `+0x8A3B0` | `$GWCA_DATA_FRAMEHASHTBL` | Frame hash table base | Derive from FrameArray scan |
| `+0x880F0` | (unnamed) | Hash seed 1 | Scan or hardcode |
| `+0x880F8` | (unnamed) | Hash seed 2 | Scan or hardcode |

## Appendix B: Known Frame Hashes

| Hash | UI Element | Used By |
|------|-----------|---------|
| 184818986 | Character Select: Play Button | Login flow |
| 41327607 | Character Select: Play (Greyed) | Login flow |
| 1398610279 | Disconnect Popup: YES | Reconnect |
| 3600335809 | Disconnect Popup: NO | Reconnect |
| 3372446797 | Character Select: Create | — |
| 3379687503 | Character Select: Delete | — |
| 1117342925 | Log Out Button | — |
| 828467986 | Character Frame | Login flow |
| 1601494406 | Edit Account | — |
| 3613855137 | Merchant Root | Crafting |
| 1517397806 | Craft Tab | Crafting |
| 3738633661 | Sell Tab | Crafting |
| 835947118 | Craft Button | Crafting |
| 1214056301 | Item List | Crafting |
| 3068881268 | Goodbye Button | Crafting |
| 3332025202 | Party Formation | Party |
| 2874675009 | Inventory Window | Inventory |
| 1532320307 | Merchant Buy Button | Trading |

## Appendix C: Packet Headers (Complete Reference)

Copied from GWA2_Headers.au3 for reference. Total: 100+ headers across all game systems.

**Connection:** 0x08 disconnect, 0x09 ping reply, 0x0A heartbeat, 0x0B ping request

**Attributes:** 0x0E decrease, 0x0F increase, 0x10 load

**Quests:** 0x11 abandon, 0x12 request info, 0x14 set active

**Heroes:** 0x15 behavior, 0x16 lock target, 0x19 skill toggle, 0x1A flag single, 0x1B flag all, 0x1C use skill, 0x1E add, 0x1F kick

**Combat:** 0x23 call target, 0x24 attack agent, 0x26 action attack, 0x28 action cancel, 0x29 buff drop

**Movement:** 0x2B draw map, 0x2C drop item, 0x2F drop gold, 0x30 equip, 0x32 switch set, 0x33 interact player, 0x35 faction deposit, 0x38 buy guild cape, 0x39 interact NPC, 0x3B dialog, 0x3C skill equip, 0x3E move to coord, 0x3F item interact, 0x40 rotate

**Skills:** 0x41 profession change, 0x46 use skill, 0x49 trade initiate, 0x4A buy materials, 0x4C request quote, 0x4D transact items, 0x4F unequip, 0x51 signpost run, 0x53 open chest, 0x56 equip visibility, 0x57 show/hide, 0x58 title display, 0x59 title hide

**Inventory:** 0x5C set skillbar skill, 0x5D load skillbar, 0x60 HoM dialog, 0x63 cinematic skip, 0x64 send chat, 0x69 destroy item, 0x6B equip bag, 0x6C identify, 0x6D tome unlock, 0x70 apply dye, 0x72 move item, 0x73 accept unclaimed, 0x75 split stack

**Salvage:** 0x77 session open, 0x78 session cancel, 0x79 session done, 0x7A salvage materials, 0x7B salvage upgrade

**Gold/Items:** 0x7C change gold, 0x7E use item, 0x80 upgrade session open, 0x81 upgrade cancel, 0x82 upgrade valid, 0x83 upgrade armor 1, 0x86 upgrade armor 2

**Instance:** 0x87 load request spawn, 0x8F load request players, 0x90 load request items, 0x98 player attr set, 0x9B set difficulty

**Party:** 0x9C accept invite, 0x9D invite cancel, 0x9E accept refuse, 0x9F invite NPC, 0xA0 invite player, 0xA1 invite by name, 0xA2 leave, 0xA3 cancel enter challenge, 0xA5 enter challenge, 0xA7 return to outpost, 0xA8 kick NPC, 0xA9 kick player, 0xAA search seek, 0xAB search cancel, 0xAC search request join, 0xAD enter foreign mission, 0xAE search type, 0xAF ready status

**Travel:** 0xB0 guild hall, 0xB1 map travel, 0xB2 leave guild hall

**Guild:** 0xBD set officer, 0xBE announcements

**Targeting:** 0xC1 target agent

**Titles:** 0xF5 title update

**Trade (player):** 0x00 receive request, 0x01 cancel, 0x02 add item, 0x03 submit offer, 0x04 offer item, 0x05 remove item, 0x06 change offer, 0x07 accept
