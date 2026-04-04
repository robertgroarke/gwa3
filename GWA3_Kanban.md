# GWA3 Kanban Board

> Task board for building `gwa3.dll` — a C++ replacement for gwca.dll + AutoIt injection layer.
> Designed for parallel agent execution. Each ticket has an assignee slot and explicit dependency chain.

---

## Legend

| Field | Meaning |
|-------|---------|
| **ID** | Unique ticket identifier (`GWA3-NNN`) |
| **Assignee** | Agent that owns this ticket (empty = unassigned) |
| **Depends On** | Must be completed before this ticket can start |
| **Blocks** | Tickets that cannot start until this one completes |
| **Parallel Group** | Tickets in the same group CAN run simultaneously |
| **Status** | `backlog` · `ready` · `in_progress` · `review` · `done` |
| **Estimate** | T-shirt size: S (< 1hr), M (1-3hr), L (3-6hr), XL (6+hr) |

---

## Dependency Graph (Critical Path)

```
GWA3-001 ──► GWA3-003 ──► GWA3-006 ──► GWA3-010 ──────────────────────────────►┐
  (CMake)      (Scanner)    (GameThread)  (CtoS Packets)                          │
                                │                                                  │
GWA3-002 ──►───────────────────┘                                                  │
  (Injector)                                                                       │
                                                                                   │
GWA3-004 ─────────────────────────────────────────────────────────────────────────►│
  (Headers.h)                                                                      │
                                                                                   │
         ┌─── GWA3-007 (Agent struct)                                              │
         │                                                                         │
GWA3-003 ┼─── GWA3-008 (Skill struct)    ── all structs feed into ──►  GWA3-016  │
  done   │                                                              (AgentMgr) │
         ├─── GWA3-009 (Item struct)                                    GWA3-017  │
         │                                                              (SkillMgr) │
         └─── GWA3-011..015 (Map/Party/Quest/Effect/Chat structs)       GWA3-018+ │
                                                                          │        │
                                                                          ▼        │
GWA3-003 ──► GWA3-005 ──► GWA3-020 ──► GWA3-021                   GWA3-025       │
  (Scanner)   (Offsets)     (FrameUI)    (ButtonClick)              (Bot Framework)│
                                                                       │           │
                                                                       ▼           │
                                                                    GWA3-026      │
                                                                    (Froggy C++)  │
                                                                       │           │
                                                                       ▼           │
                                                                    GWA3-028..032 │
                                                                    (Integration) │
                                                                       │           │
                                                                       ▼           │
                                                                    GWA3-033..036 │
                                                                    (Hardening)◄──┘
```

---

## Research Index

> Maps all 139 files in `research/GWCA_Disassembly_Research/` to the kanban ticket(s) they inform.
> Agents MUST read their assigned research files before starting implementation.
> Last audit: 2026-03-30 (139 files total)

### Cluster 1: Hook System (9 files) → GWA3-006, GWA3-037

| File | Key Finding | Ticket |
|------|-------------|--------|
| `GWCA_HookInstaller_Addendum.md` | `CreateHook(target, detour, out_original)` signature, replay trampoline generation | GWA3-006, GWA3-037 |
| `GWCA_HookInstaller_DeepDive.md` | `FUN_10029730` internals: prologue analysis, patch plan, hook record allocation | GWA3-037 |
| `GWCA_HookLifecycle_Addendum.md` | Public `EnableHooks`/`DisableHooks` exports, bulk vs per-hook toggle, two-system separation (detours + memory patcher) | GWA3-006, GWA3-037, GWA3-038 |
| `GWCA_HookRecordPool_Addendum.md` | VirtualAlloc RWX page pool, 0x20-byte slots, free-list allocator | GWA3-037 |
| `GWCA_HookToggle_Addendum.md` | Thread-safe patch writing: suspend peers → repair EIP → write detour → resume. VirtualProtect + FlushInstructionCache | GWA3-037 |
| `GWCA_ReplayStub_Layout_Addendum.md` | 0x2c-byte hook table entries, replay slot relocated code, instruction relocation templates (E8/E9/0F80) | GWA3-037 |
| `GWCA_LiveHookTable_Addendum.md` | Live dump: 39 detour entries, table at `DAT_1008b0c0`, replay page at `0x0A110000` | GWA3-037 |
| `GWCA_LiveDetour_Classification_Addendum.md` | UIModule owns 6 hooks; detour RVA → subsystem mapping | GWA3-020, GWA3-037 |
| `GWCA_Replacement_Feasibility.md` | Overall feasibility assessment, ~70% already covered by AutoIt | GWA3-001 |

### Cluster 2: Memory Patcher (5 files) → GWA3-038

| File | Key Finding | Ticket |
|------|-------------|--------|
| `GWCA_MemoryPatcher_LiveAddendum.md` | Patcher object: 0x10 bytes (target, patched, original, size, flag). Global vector at `DAT_1008a1F8`. 3 live patches. | GWA3-038 |
| `GWCA_MemoryPatcher_Ownership_Addendum.md` | `SetPatch()`/`SetRedirect()` flow. Camera patch + chat patch owners identified. | GWA3-038 |
| `GWCA_GameTargetPatch_Addendum.md` | Live patch semantics: camera unlock (`EB 0F`), level-data bypass (`EB`), map/port bypass (`90 90`) | GWA3-038 |
| `GWCA_PatchObject_Ownership_Addendum.md` | Object-to-global mapping: `DAT_10089f18` (camera), `DAT_10089fa4` (chat), `DAT_1008a188` (unnamed) | GWA3-038 |
| `GWCA_BuildConsistency_And_ExportSeam_Addendum.md` | RVA validation: static analysis must match injected binary. Export-anchored seams more stable than globals. | GWA3-005, GWA3-035 |

### Cluster 3: GameThread (50 files) → GWA3-006

**Core infrastructure (must-read for GWA3-006):**

| File | Key Finding | Ticket |
|------|-------------|--------|
| `GWCA_GameThreadQueue_Addendum.md` | Queue at `DAT_1008A0BC`, 0x28-byte entries, callable at +0x24. `Enqueue @ 0x10019D50`. Fast in-thread path. Critical section at `DAT_1008A098`. | GWA3-006 |
| `GWCA_GameThreadCallbackRegistry_Addendum.md` | Persistent per-frame callbacks: 0x30-byte altitude-sorted records. `RegisterGameThreadCallback @ 0x10019E70`. | GWA3-006 |
| `GWCA_GameThreadLifecycle_Addendum.md` | `EnableHooks @ 0x100196C0`, `ClearCalls @ 0x10019CB0`. Shared state flags. | GWA3-006 |
| `GWCA_GameThreadBootstrap_Addendum.md` | GameThread registered as first-class module in `GW::Initialize()`. Bootstrap order: scanner → hooks → enqueue → enable → post-hook. | GWA3-006 |
| `GWCA_GameThreadModuleSlots_Addendum.md` | Module record: init (+0x08), shutdown (+0x0C), enable (+0x10), disable (+0x14). Init scans `FrApi.cpp` / `renderElapsed >= 0`. | GWA3-006, GWA3-005 |
| `GWCA_GameThreadTarget_Addendum.md` | **CRITICAL**: Hook target is `FUN_006117E0` (frame/render callback, NOT OS thread). Found via `FindAssertion("FrApi.cpp", "renderElapsed >= 0")`. | GWA3-005, GWA3-006 |
| `GWCA_GameThread_GwCallback_Addendum.md` | Game callback: guarded by renderElapsed assertion, clamps elapsed to [0,1], runs update/render pipeline. | GWA3-006 |
| `GWCA_GameThread_DispatchSemantics_Addendum.md` | Frame message IDs: 0x24=activate, 0x2E=deactivate, 0x2D=raw coords, 0x31=final, 0x32=preflight | GWA3-020 |
| `GWCA_GameThread_HandlerExecutor_Addendum.md` | Callable record: 0x0C bytes (fn ptr, descriptor, metadata). Frame base recovery: `*this - 0x128`. | GWA3-020 |
| `GWCA_GameThread_MessagePath_Addendum.md` | Coord message submission: validation → normalization → packet assembly → dispatcher fan-out | GWA3-020 |

**Extended GameThread files (reference only, not required for implementation):**

The remaining ~90 `GWCA_GameThread_*` files fall into sub-clusters. None are bot-facing or blocking for any ticket:

| Sub-Cluster | Files | Domain | Bot Relevance |
|-------------|-------|--------|---------------|
| Frame ownership/layout | ~8 | Frame relations, ownership, region semantics | Reference for GWA3-020 edge cases |
| Channel/protocol | ~8 | Channel classification, extended protocol, slot verbs | Reference for GWA3-039 |
| Metrics/measurement | ~5 | Metric builders, aggregate grids, phase taxonomy | None — internal engine analytics |
| Setup/construction | ~6 | Constructor skeletons, setup callbacks, default sources | None — GWCA bootstrap internals |
| **Texture/Image/DXT** | **~19** | DXT decompression, ImgMem, atlas lifecycle, format crosswalk | **None — graphics pipeline** |
| **Serialization/Transfer** | **~11** | Block serialization, offset streams, transfer banks, compressed block orchestration | **None — graphics codec internals** |
| **Callback/Registry** | **~9** | Format callback dispatch, static registry installers, capability consumers | **None — graphics callback plumbing** |
| **Format Mechanics** | **~3** | DXT stage-1 families, alpha prepass, parent branch gating | **None — algorithm analysis** |
| Miscellaneous | ~10+ | Backend producer, text worker, interaction lifecycle, directional control FSM | Low — engine behavior docs |

**The 47 newest files (texture/DXT/serialization/callback clusters) are entirely graphics engine internals with zero intersection to bot automation. No new kanban tickets needed.**

### Cluster 4: Toolbox Integration (15 files) → GWA3-039

| File | Key Finding | Ticket |
|------|-------------|--------|
| `GWCA_Toolbox_ExportSeam_DeepDive.md` | Bootstrap globals, range-table lookup (0x0C-byte records), MapMgr export cluster at 0x1001CC30 | GWA3-039 |
| `GWCA_Toolbox_ModuleOwnership_Addendum.md` | Module handle = grouping key. Range table: `{module_handle, range_start, range_end}`. DAT_1008A418. | GWA3-039 |
| `GWCA_Toolbox_RegistrationPaths_Addendum.md` | 3 registration APIs: CreateUIComponent, FrameUIMessage, UIMessage. All funnel through `AddHookEntryByModule`. | GWA3-039 |
| `GWCA_Toolbox_StdFunction_Addendum.md` | MSVC std::function with SBO. Callback records: 0x30 bytes. Clone/invoke/destroy vtable. | GWA3-039 |
| `GWCA_Toolbox_RecordLayout_Addendum.md` | 0x30-byte record: altitude(+0x00), HookEntry*(+0x04), function_sbo(+0x08..+0x2B), function_impl(+0x2C) | GWA3-039 |
| `GWCA_Toolbox_RegistryVector_Addendum.md` | Listener vector structure and record layout | GWA3-039 |
| `GWCA_Toolbox_ListenerConstructor_Addendum.md` | Fixed 0x30-byte listener record allocation/construction | GWA3-039 |
| `GWCA_Toolbox_CallableCluster_Addendum.md` | Callable destructor and ownership cleanup | GWA3-039 |
| `GWCA_Toolbox_ContainerHelpers_Addendum.md` | Hash-based node maps vs contiguous vectors | GWA3-039 |
| `GWCA_Toolbox_RemovalPaths_Addendum.md` | Inverse removal paths mirroring registration | GWA3-039 |
| `GWCA_Toolbox_SeededCallable_Addendum.md` | Bootstrap-seeded callable for grouped cleanup | GWA3-039 |
| Remaining 4 Toolbox files | VTable boundary, listener path correction, subobject dispatch, callable holder helpers | GWA3-039 |

### Cluster 5: Key/Lambda/Preference (8 files) → GWA3-040

| File | Key Finding | Ticket |
|------|-------------|--------|
| `GWCA_KeyAdapter_Addendum.md` | Keys = frame-message adapters. RegisterKeydownCallback wraps user fn → RegisterFrameUIMessageCallback(msg=0x20) | GWA3-040 |
| `GWCA_KeyLambdaVTable_Addendum.md` | Keydown/keyup adapter sibling lambda vtables | GWA3-040 |
| `GWCA_KeypressLambda_Addendum.md` | Keypress = immediate 0x20 + deferred 0x22 via GameThread::Enqueue | GWA3-040 |
| `GWCA_LambdaConvention_Addendum.md` | Broader GWCA lambda/vtable convention for all callback types | GWA3-039, GWA3-040 |
| `GWCA_SetPreference_OverloadMatrix_Addendum.md` | 4 overloads: String, Enum, Flag (deferred replay), Number (direct renderer logic) | GWA3-040 |
| `GWCA_StringPreferenceLambda_Addendum.md` | String preference deferred-task lambda pattern | GWA3-040 |

### Cluster 6: Frame UI (6 files) → GWA3-020, GWA3-021

| File | Key Finding | Ticket |
|------|-------------|--------|
| `GWCA_UIMessage_Research.md` | Complete frame/UIMessage architecture. 20+ message payloads documented. Two dispatch systems (global + frame). | GWA3-020 |
| `GWCA_ButtonClick_Research.md` | Full ButtonClick chain. Frame offsets confirmed. `_WriteLE32` bug documented. | GWA3-021 |
| `CharSelect_ButtonClick_Research.md` | Character select flow. Must execute from game thread. CreateRemoteThread deadlocks. | GWA3-021, GWA3-028 |
| `GWCA_UIMessage_HookChain_Addendum.md` | DAT_1008a39C (raw target) vs DAT_1008a3A0 (replay handle). UIModule bootstrap. | GWA3-020 |
| `GWCA_UIMessage_LiveDetour_Addendum.md` | Live values: GwBase=0x00D30000, SendFrameUIMsg=0x00F586D0, detour at gwca+0x26860 | GWA3-020 |
| `GWCA_Crafting_Research.md` | Merchant dialog frame hierarchy. Craft tab/button hashes. TransactItem signature. | GWA3-021, GWA3-032 |

### Cluster 7: Injection & Initialization (2 files) → GWA3-002

| File | Key Finding | Ticket |
|------|-------------|--------|
| `Local_GW_Image_And_GWCA_Injection_Guide.md` | Complete injection workflow. Must route UI to game thread via rendering hook. Labels: FrameArray, QueueCounter, QueueBase. | GWA3-002, GWA3-006 |
| `Py4GW_GWCA_Botting_Research.md` | Py4GW API surface: PyPlayer, PyParty, PyInventory, PySkillbar, PyUIManager. Transport: UI messages + native calls + frame clicks. | GWA3-025, GWA3-041 |

### Cluster 8: Build Validation (1 file) → GWA3-035

| File | Key Finding | Ticket |
|------|-------------|--------|
| `GWCA_BuildConsistency_And_ExportSeam_Addendum.md` | Static RVAs must be verified against injected binary. Export-anchored seams > fragile globals. | GWA3-005, GWA3-035 |

---

## BACKLOG

---

### Epic 1: Project Foundation

#### GWA3-001 — CMake Project + DLL Skeleton

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `ready` |
| **Estimate** | M |
| **Depends On** | none |
| **Blocks** | GWA3-002, GWA3-003, GWA3-004, GWA3-005, GWA3-007, GWA3-008, GWA3-009 |
| **Parallel Group** | — |

**Description:**
Create the `gwa3/` project root with CMake build system targeting **x86 (Win32)**.

**Acceptance Criteria:**
- [ ] `CMakeLists.txt` at `gwa3/` root, generates VS solution or Ninja build
- [ ] Compiles to `gwa3.dll` (32-bit)
- [ ] `dllmain.cpp` with `DLL_PROCESS_ATTACH` that spawns init thread
- [ ] Logging module (`core/Log.h`) — writes to `gwa3_log.txt` and `OutputDebugStringA`
- [ ] Directory structure matches plan: `include/gwa3/`, `src/core/`, `src/managers/`, `src/packets/`, `src/exports/`, `tools/`
- [ ] MinHook vendored or fetched via CMake FetchContent
- [ ] `.gitignore` for build artifacts
- [ ] Builds clean with MSVC in both Debug and Release

---

#### GWA3-002 — Standalone DLL Injector

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `ready` |
| **Estimate** | M |
| **Depends On** | GWA3-001 |
| **Blocks** | GWA3-006, GWA3-034 |
| **Parallel Group** | PG-FOUNDATION (with GWA3-003, GWA3-004, GWA3-005) |

**Description:**
Build `tools/injector.cpp` → `injector.exe` that loads `gwa3.dll` into a running GW process.

**Acceptance Criteria:**
- [ ] Finds GW.exe by window class `ArenaNet_Dx_Window_Class`
- [ ] If multiple GW windows, lists them with PID + character name (if readable) and prompts user
- [ ] `OpenProcess` with `PROCESS_ALL_ACCESS`
- [ ] `VirtualAllocEx` + `WriteProcessMemory` to write DLL path
- [ ] `CreateRemoteThread` calling `LoadLibraryA`
- [ ] Waits for thread completion, reports success/failure
- [ ] Accepts optional `--pid <N>` argument for specific process
- [ ] Prints `gwa3.dll` base address on success

**Required Reading:**
- `research/GWCA_Disassembly_Research/Local_GW_Image_And_GWCA_Injection_Guide.md` — complete injection workflow, safe vs unsafe operations, rendering hook queue

---

#### GWA3-003 — Pattern Scanner Engine

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `ready` |
| **Estimate** | L |
| **Depends On** | GWA3-001 |
| **Blocks** | GWA3-005, GWA3-006, GWA3-007, GWA3-008, GWA3-009, GWA3-011, GWA3-012, GWA3-013, GWA3-014, GWA3-015, GWA3-020 |
| **Parallel Group** | PG-FOUNDATION (with GWA3-002, GWA3-004, GWA3-005) |

**Description:**
Implement `core/Scanner.h` + `core/Scanner.cpp` — the byte-pattern scanning engine.

Port logic from `GWA2_Assembly.au3`'s `ExecutePatternScan()` and `ResolveAssertionPatterns()`. Since we're in-process, scanning is direct memory reads — no `ReadProcessMemory` needed.

**Acceptance Criteria:**
- [ ] `Scanner::Initialize(HMODULE)` — finds GW.exe base, parses PE headers, locates `.text`/`.rdata`/`.data` sections
- [ ] `Scanner::Find(pattern, mask, offset)` — byte pattern scan with `?` wildcards, returns address + offset
- [ ] `Scanner::FindAssertion(source_file, message, offset)` — finds string literal in `.rdata`, locates xref in `.text`, returns address + offset
- [ ] `Scanner::FunctionFromNearCall(address)` — resolves `E8 rel32` CALL instructions to target
- [ ] Section getters: `GetTextSection()`, `GetRdataSection()`, `GetDataSection()`
- [ ] Scans complete in < 2 seconds for all patterns
- [ ] Unit test: scan for `BasePointer` pattern returns non-null on live GW client

**Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` lines ~100-400 (scan registration + execution)

---

#### GWA3-004 — Packet Header Constants

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `ready` |
| **Estimate** | S |
| **Depends On** | GWA3-001 |
| **Blocks** | GWA3-010, GWA3-016, GWA3-017, GWA3-018, GWA3-019, GWA3-022, GWA3-023 |
| **Parallel Group** | PG-FOUNDATION (with GWA3-002, GWA3-003, GWA3-005) |

**Description:**
Port all 100+ packet header constants from `GWA2_Headers.au3` into `packets/Headers.h`.

**Acceptance Criteria:**
- [ ] `namespace GWA3::Packets` with `constexpr uint32_t` for every header
- [ ] Organized by category with comments (Trade, Connection, Quest, Hero, Movement, Inventory, Skills, Instance, Party, Travel, Guild, Chat, Titles)
- [ ] Values exactly match GWA2_Headers.au3 (verified by diff)
- [ ] Header-only file, no cpp needed

**Reference:** `GWA Censured/lib/botshub/GWA2_Headers.au3` (complete file), `BotsHub-latest/lib/GWA2_Headers.au3` (cross-reference)

---

#### GWA3-005 — Offset Registry + Pattern Definitions

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `ready` |
| **Estimate** | L |
| **Depends On** | GWA3-001, GWA3-003 |
| **Blocks** | GWA3-006, GWA3-007, GWA3-008, GWA3-009, GWA3-010, GWA3-011, GWA3-012, GWA3-013, GWA3-014, GWA3-015, GWA3-016, GWA3-020 |
| **Parallel Group** | PG-FOUNDATION (with GWA3-002, GWA3-004) |

**Description:**
Implement `core/Offsets.h` + `core/Offsets.cpp` — register all 48 existing scan patterns + 5 new frame UI patterns, resolve them via the Scanner, cache results.

**Acceptance Criteria:**
- [ ] `Offsets::ResolveAll()` — runs every pattern, returns false if any P0/P1 fails
- [ ] All 48 patterns from `RegisterAllScanPatterns()` in GWA2_Assembly.au3 ported
- [ ] 5 new frame patterns added: `SendFrameUIMsg`, `FrameArray`, `SendUIMessage`, `RootFrame`, `GetChildFrame`
- [ ] Each offset stored as `uintptr_t` in the `Offsets` namespace
- [ ] Pattern table with metadata: name, hex bytes, mask, offset, priority (P0/P1/P2), assertion flag
- [ ] Log output on init: resolved count, failed count, failed names
- [ ] Integration test: inject into GW, call `ResolveAll()`, all P0 patterns succeed

**Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` `RegisterAllScanPatterns()`, `GWA Censured/lib/custom/GWA2_FrameUI.au3` `ExtendScanner_FrameUI()`

---

### Epic 2: Game Thread + Command Execution

#### GWA3-006 — Game Thread Hook + Command Queue

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-002, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-010, GWA3-016, GWA3-017, GWA3-018, GWA3-019, GWA3-020, GWA3-021, GWA3-022, GWA3-023, GWA3-024, GWA3-025 |
| **Parallel Group** | — |

**Description:**
Implement `core/GameThread.h` + `core/GameThread.cpp` — hook the game's frame/render callback and process a thread-safe command queue each frame.

**CRITICAL RESEARCH**: The game thread target is NOT a thread procedure — it's a frame callback found via `Scanner::FindAssertion("FrApi.cpp", "renderElapsed >= 0")`. GWCA's research confirms this resolves to `FUN_006117E0` in the game. The detour runs before the original callback, draining a singleshot queue + persistent callback registry.

Uses MinHook to detour the scanned target. Implements two execution planes:
1. **Singleshot queue** — one-time deferred tasks (like GWCA's 0x28-byte entry queue at `DAT_1008A0BC`)
2. **Persistent callbacks** — per-frame hooks (like GWCA's 0x30-byte altitude-sorted registry at `DAT_1008A0D4`)

**Acceptance Criteria:**
- [ ] `GameThread::Initialize()` — scans for `FrApi.cpp`/`renderElapsed >= 0` assertion target, installs detour via MinHook
- [ ] `GameThread::Shutdown()` — removes hook, destroys critical section
- [ ] `GameThread::Enqueue(std::function<void()>)` — thread-safe push to singleshot queue with fast in-thread path (skip queue if already on game thread)
- [ ] `GameThread::RegisterCallback(HookEntry*, callback, altitude)` — persistent per-frame callback, altitude-sorted
- [ ] `GameThread::RemoveCallback(HookEntry*)` — remove persistent callback
- [ ] `GameThread::IsOnGameThread()` — returns true when called from hooked context (reentrancy flag)
- [ ] Critical section protects all queue/registry access
- [ ] Detour pattern: `EnterHook() → drain queue → call persistent callbacks → call original → LeaveHook()`
- [ ] No deadlocks — mutex is only held briefly during push/pop
- [ ] Test: enqueue a lambda that logs "hello from game thread" — verify it fires
- [ ] Game runs stable for 10+ minutes with hook installed and idle queue

**Required Reading:**
- `research/GWCA_Disassembly_Research/GWCA_GameThreadQueue_Addendum.md` — queue structure, Enqueue semantics
- `research/GWCA_Disassembly_Research/GWCA_GameThreadCallbackRegistry_Addendum.md` — persistent callback registry
- `research/GWCA_Disassembly_Research/GWCA_GameThreadTarget_Addendum.md` — **scan target identification**
- `research/GWCA_Disassembly_Research/GWCA_GameThreadBootstrap_Addendum.md` — initialization order
- `research/GWCA_Disassembly_Research/GWCA_GameThreadModuleSlots_Addendum.md` — module lifecycle slots
- `research/GWCA_Disassembly_Research/GWCA_GameThreadLifecycle_Addendum.md` — enable/disable/clear
- `research/GWCA_Disassembly_Research/GWCA_GameThread_GwCallback_Addendum.md` — game-side callback analysis
- `research/GWCA_Disassembly_Research/Local_GW_Image_And_GWCA_Injection_Guide.md` — injection workflow

**Also Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` MainProc detour logic

---

#### GWA3-010 — Packet Sending (CtoS)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-004, GWA3-005, GWA3-006 |
| **Blocks** | GWA3-016, GWA3-017, GWA3-018, GWA3-019, GWA3-022, GWA3-023 |
| **Parallel Group** | PG-PACKETS (with GWA3-016..019 once this completes) |

**Description:**
Implement `packets/CtoS.h` + `packets/CtoS.cpp` — call the game's `PacketSend` function to send client-to-server packets.

**Acceptance Criteria:**
- [ ] `CtoS::SendPacket(uint32_t size, ...)` — variadic raw packet send, enqueues on game thread
- [ ] Type-safe wrappers: `MoveToCoord(x, y)`, `Dialog(id)`, `ChangeTarget(id)`, `MapTravel(id, region, district, lang)`
- [ ] All wrappers assert they're on game thread (or auto-enqueue)
- [ ] Test: `CtoS::MoveToCoord()` — character visibly moves
- [ ] Test: `CtoS::ChangeTarget()` — target changes in game UI
- [ ] Test: `CtoS::Dialog()` — NPC dialog advances

**Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` CommandPacketSend, `GWA Censured/lib/botshub/Utils.au3` `SendPacket()`

---

### Epic 3: Game Data Structures

> **All struct tickets in this epic can run in parallel** once GWA3-003 and GWA3-005 are done.
> They have no dependencies on each other — each agent writes independent header files.

#### GWA3-007 — Agent Struct + Accessors

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-001, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-016 |
| **Parallel Group** | PG-STRUCTS (with GWA3-008, GWA3-009, GWA3-011..015) |

**Description:**
Define `game/Agent.h` with the full `#pragma pack(push,1)` Agent struct (446 bytes), plus `AgentMgr` read-only accessors.

**Acceptance Criteria:**
- [ ] `Agent` struct with every field from `$AGENT_STRUCT_TEMPLATE` at correct offset
- [ ] Helper methods: `IsAlive()`, `IsDead()`, `DistanceTo(Agent&)`, `DistanceTo(float,float)`
- [ ] `GetMyAgent()`, `GetMyID()`, `GetAgentByID(id)`, `GetAgentArray()`, `GetMaxAgents()`, `GetCurrentTargetID()`
- [ ] Validation test: read player agent, confirm `x`, `y`, `health_percent`, `model_id`, `primary_profession` match in-game values
- [ ] Stress test: iterate full agent array (512+ agents), no crashes

**Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` agent struct template (search `AGENT_STRUCT_TEMPLATE`), `GWA2.au3` `GetAgent*()` functions

---

#### GWA3-008 — Skill Struct + Accessors

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-001, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-017 |
| **Parallel Group** | PG-STRUCTS (with GWA3-007, GWA3-009, GWA3-011..015) |

**Description:**
Define `game/Skill.h` with `Skill`, `SkillbarSlot`, `Skillbar` structs.

**Acceptance Criteria:**
- [ ] `Skill` struct with 40+ fields from `$SKILL_STRUCT_TEMPLATE`
- [ ] `SkillbarSlot` and `Skillbar` structs
- [ ] `GetSkillByID(id)`, `GetPlayerSkillbar()`, `GetHeroSkillbar(index)`, `IsRecharged(slot, hero)`
- [ ] Validation: read player skillbar, confirm 8 skill IDs match in-game
- [ ] Validation: read skill data for known skill (e.g., Healing Signet), confirm activation time

**Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` skill struct template, `GWA2.au3` `GetSkill*()`, `GetSkillbar*()`

---

#### GWA3-009 — Item & Bag Structs + Accessors

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-001, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-018 |
| **Parallel Group** | PG-STRUCTS (with GWA3-007, GWA3-008, GWA3-011..015) |

**Description:**
Define `game/Item.h` with `Item`, `Bag` structs and inventory accessors.

**Acceptance Criteria:**
- [ ] `Item` struct with all fields from `$ITEM_STRUCT_TEMPLATE` (88 bytes)
- [ ] `Bag` struct with type, index, count, max_slots, item pointer array
- [ ] Helper methods: `IsIdentified()`, `IsSalvageable()`, `GetRarity()`, `GetQuantity()`
- [ ] `GetItemByID(id)`, `GetItemBySlot(bag, slot)`, `GetBag(bag_id)`, `GetGoldOnCharacter()`, `GetGoldInStorage()`
- [ ] `GetMerchantItems()`, `GetMerchantItemCount()`
- [ ] Validation: read backpack contents, confirm model IDs and quantities match in-game

**Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` item struct template, `GWA2.au3` `GetItem*()`, `GetBag*()`

---

#### GWA3-011 — Map & Instance Struct

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-001, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-019 |
| **Parallel Group** | PG-STRUCTS |

**Description:**
Define `game/Map.h` — map ID, type, region, district, loading state, instance uptime.

**Acceptance Criteria:**
- [ ] `GetMapID()`, `GetMapType()`, `GetRegion()`, `GetDistrict()`, `IsMapLoading()`, `IsMapLoaded()`, `GetInstanceUptime()`
- [ ] Validation: confirm map ID matches current location

**Reference:** `GWA2.au3` `GetMap*()`, offsets `InstanceInfo`, `Region`, `AreaInfo`

---

#### GWA3-012 — Party & Hero Struct

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-001, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-022 |
| **Parallel Group** | PG-STRUCTS |

**Description:**
Define `game/Party.h` — party members, hero agent IDs, hero count.

**Acceptance Criteria:**
- [ ] `GetPartySize()`, `GetHeroCount()`, `GetHeroAgentID(index)`, `GetHeroProfession(index)`
- [ ] Validation: add heroes in-game, confirm count and agent IDs match

**Reference:** `GWA2.au3` `GetParty*()`, `GetHero*()`

---

#### GWA3-013 — Quest Struct

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-001, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-023 |
| **Parallel Group** | PG-STRUCTS |

**Description:**
Define `game/Quest.h` — active quest ID, quest log access.

**Acceptance Criteria:**
- [ ] `GetActiveQuestID()`
- [ ] Validation: set active quest in-game, confirm ID matches

**Reference:** `GWA2.au3` `GetActiveQuest()`, offset `ActiveQuest`

---

#### GWA3-014 — Effect & Buff Struct

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-001, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-016 |
| **Parallel Group** | PG-STRUCTS |

**Description:**
Define `game/Effect.h` — `Buff`, `Effect` structs, effect query functions.

**Acceptance Criteria:**
- [ ] `Buff` struct: skill_id, buff_id, target_id
- [ ] `Effect` struct: skill_id, attribute_level, effect_id, agent_id, duration, timestamp
- [ ] `GetBuffs(count)`, `GetEffects(count)`, `HasEffect(agent_id, skill_id)`, `GetEffectTimeRemaining(agent_id, skill_id)`
- [ ] `DropBuff(buff_id)` — remove specific buff/enchantment
- [ ] `GetAlcoholLevel()` — drunkard title tracking
- [ ] `GetPlayerEffectBySkillId(skill_id)` — direct effect lookup by skill
- [ ] `GetPlayerBuffBySkillId(skill_id)` — direct buff lookup by skill
- [ ] `GetAgentEffects(agent_id)` / `GetAgentBuffs(agent_id)` — per-agent lookups
- [ ] Validation: cast enchantment on self, confirm HasEffect returns true and duration is correct

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/EffectMgr.h`
**AutoIt Reference:** `GWA2_Assembly.au3` `$BUFF_STRUCT_TEMPLATE`, `$EFFECT_STRUCT_TEMPLATE`

---

#### GWA3-015 — Chat Struct + Constants

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-001, GWA3-003, GWA3-005 |
| **Blocks** | GWA3-024 |
| **Parallel Group** | PG-STRUCTS |

**Description:**
Define `game/Chat.h` — chat channel IDs, chat log struct, message struct.

**Acceptance Criteria:**
- [ ] Channel enum: Alliance=0, All=3, Guild=9, WhisperSent=10, Trade=12, Advisory=13, WhisperRecv=14, Team=11
- [ ] Chat log entry struct: channel (dword) + message (wchar[256])
- [ ] Header-only, no cpp needed

**Reference:** `GWA2_Assembly_Chatlog.au3` `$CHAT_LOG_STRUCT`, channel constants

---

### Epic 4: Manager Modules (Game Commands)

> Managers depend on their corresponding struct ticket + GameThread + CtoS.
> **All managers can run in parallel** once their deps are met.

#### GWA3-016 — AgentMgr (Movement + Targeting + Combat)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-006, GWA3-007, GWA3-010, GWA3-014 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS (with GWA3-017..024) |

**Description:**
Implement `managers/AgentMgr.cpp` — movement, targeting, attack, NPC interaction.

**Acceptance Criteria:**
- [ ] `Move(x, y)` — calls scanned Move function on game thread
- [ ] `ChangeTarget(agent_id)` — calls scanned ChangeTarget function
- [ ] `Attack(agent_id)` — sends attack packet
- [ ] `InteractNPC(agent_id)` — sends interact packet
- [ ] `InteractSignpost(agent_id)` — sends signpost interact
- [ ] `CancelAction()` — sends cancel packet
- [ ] `CallTarget(target_type)` — calls target for party
- [ ] Test: move character, attack enemy, interact with NPC

**Reference:** `GWA2.au3` Move/Attack/ChangeTarget/GoNPC, `GWA2_Assembly.au3` CommandMove/CommandAction/CommandChangeTarget

---

#### GWA3-017 — SkillMgr (Skill Usage + Hero Skills)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-006, GWA3-008, GWA3-010 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/SkillMgr.cpp` — skill usage, hero skill commands, skillbar loading.

**Acceptance Criteria:**
- [ ] `UseSkill(slot, target)` — calls scanned UseSkill function
- [ ] `UseHeroSkill(hero_index, slot, target)` — calls scanned UseHeroSkill function
- [ ] `LoadSkillbar(skill_ids[8], hero_index)` — sends load skillbar packet
- [ ] `SetSkillbarSkill(slot, skill_id, hero_index)` — single skill change
- [ ] `ToggleHeroSkillSlot(hero_index, slot)` — enable/disable hero skill
- [ ] Test: use skill on target, load a skill template

**Reference:** `GWA2.au3` UseSkill/UseHeroSkill/LoadSkillBar, `GWA2_Assembly.au3` CommandUseSkill

---

#### GWA3-018 — ItemMgr (Inventory + Salvage + Trade)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-006, GWA3-009, GWA3-010 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/ItemMgr.cpp` — item manipulation, salvage sessions, merchant buy/sell.

**Acceptance Criteria:**
- [ ] `MoveItem(item_id, bag_id, slot)` — sends move packet
- [ ] `UseItem(item_id)` — sends use packet
- [ ] `DropItem(item_id)` — sends drop packet
- [ ] `EquipItem(item_id)` — sends equip packet
- [ ] `IdentifyItem(item_id)` — sends identify packet
- [ ] `SalvageStart(item_id, kit_id)`, `SalvageMaterials()`, `SalvageDone()` — full salvage session
- [ ] `DestroyItem(item_id)` — sends destroy packet
- [ ] `BuyItem(item_id, quantity)`, `SellItem(item_id)`, `RequestQuote(item_id)` — merchant ops
- [ ] `ChangeGold(char_amount, storage_amount)` — gold management
- [ ] `PickUpItem(item_id, call_target)` — pick up ground item (CRITICAL for loot)
- [ ] `OpenXunlaiWindow()` — open storage chest dialog
- [ ] `CanAccessXunlaiChest()` — check storage availability
- [ ] `DepositGold(amount)` / `WithdrawGold(amount)` — gold management
- [ ] `GetEquipmentVisibility(type)` / `SetEquipmentVisibility(type, state)` — helm/cape/costume toggle
- [ ] Test: move item between bags, use consumable, salvage an item, pick up ground item

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/ItemMgr.h`
**AutoIt Reference:** `GWA2.au3` item functions, `Utils.au3` salvage/merchant functions

---

#### GWA3-019 — MapMgr (Travel + Instance)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-006, GWA3-010, GWA3-011 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/MapMgr.cpp` — map travel, instance management, difficulty.

**Acceptance Criteria:**
- [ ] `Travel(map_id, district)` — sends map travel packet
- [ ] `ReturnToOutpost()` — sends return packet
- [ ] `EnterMission()` — calls scanned EnterMission function
- [ ] `SetHardMode(bool)` — calls scanned SetDifficulty function
- [ ] `TravelGuildHall()`, `LeaveGuildHall()` — guild hall packets
- [ ] Install MapLoad hook — fires callback when zone load completes
- [ ] `WaitMapLoading(target_map, timeout)` — blocking wait helper
- [ ] `SkipCinematic()` — skip dungeon/mission cutscenes
- [ ] `GetIsInCinematic()` — detect cinematic state
- [ ] `EnterChallenge()` / `CancelEnterChallenge()` — enter mission/dungeon
- [ ] `GetFoesKilled()` / `GetFoesToKill()` — vanquish progress tracking
- [ ] `QueryAltitude(pos, radius)` — terrain height at position
- [ ] `GetPathingMap()` — pathfinding geometry access
- [ ] `GetInstanceTime()` — instance uptime in milliseconds
- [ ] Test: travel from one outpost to another, set hard mode, skip cinematic

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/MapMgr.h`
**AutoIt Reference:** `GWA2.au3` MoveMap/WaitMapLoading/EnterChallenge, `GWA2_Assembly.au3` LoadFinished hook

---

#### GWA3-020 — UIMgr: Frame System (Hash Lookup + SendFrameUIMsg)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-003, GWA3-005, GWA3-006 |
| **Blocks** | GWA3-021 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/UIMgr.cpp` — frame struct, hash-based frame lookup, `SendFrameUIMessage`, `SendUIMessage`.

This is the **core RE research ticket** — the GWCA research docs are the primary source for this implementation.

**Acceptance Criteria:**
- [ ] `Frame` struct with offsets: +0xA8 callbacks, +0xB8 child_offset_id, +0xBC frame_id, +0x128 relation, +0x134 frame_hash_id, +0x18C frame_state
- [ ] `Frame::IsCreated()`, `IsHidden()`, `IsDisabled()`, `IsClickable()`, `GetParent()`
- [ ] `GetFrameByHash(hash)` — walk frame array comparing +0x134
- [ ] `GetRootFrame()` — from scanned RootFrame offset
- [ ] `IsFrameVisible(hash)` — find frame + check state bits
- [ ] `SendFrameUIMessage(frame, msgid, wparam, lparam)` — build ECX = parent+0xA8, call game function
- [ ] `SendUIMessage(msgid, wparam, lparam)` — call global UIMessage dispatcher
- [ ] Known frame hash constants in `UIMgr::Hashes` namespace
- [ ] Frame message dispatcher semantics: 0x24=activate, 0x2E=deactivate, 0x31=final, 0x32=preflight
- [ ] Callable record structure: 0x0C bytes (fn ptr, descriptor, metadata) for handler invocation
- [ ] Test: `GetFrameByHash(PlayButton)` returns valid frame pointer at character select
- [ ] Test: `IsFrameVisible(ReconnectYes)` returns correct visibility state

**Required Reading:**
- `research/GWCA_Disassembly_Research/GWCA_UIMessage_Research.md` — complete UIMessage architecture
- `research/GWCA_Disassembly_Research/GWCA_UIMessage_HookChain_Addendum.md` — DAT_1008a39C vs DAT_1008a3A0
- `research/GWCA_Disassembly_Research/GWCA_UIMessage_LiveDetour_Addendum.md` — live runtime values
- `research/GWCA_Disassembly_Research/GWCA_GameThread_DispatchSemantics_Addendum.md` — frame message IDs
- `research/GWCA_Disassembly_Research/GWCA_GameThread_HandlerExecutor_Addendum.md` — callable record invocation
- `research/GWCA_Disassembly_Research/GWCA_GameThread_MessagePath_Addendum.md` — coord message submission
- `research/GWCA_Disassembly_Research/GWCA_LiveDetour_Classification_Addendum.md` — UIModule's 6 hooks

**Also Reference:** `GWA Censured/lib/custom/GWA2_FrameUI.au3`

---

#### GWA3-021 — UIMgr: ButtonClick

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-020 |
| **Blocks** | GWA3-025, GWA3-028 |
| **Parallel Group** | — |

**Description:**
Implement `ButtonClick` and `ButtonClickByHash` — replicate GWCA's MouseAction → SendFrameUIMsg chain.

**Acceptance Criteria:**
- [ ] `ButtonClick(Frame*)` — validates state, gets parent, computes ECX, builds kMouseAction with action_state=7 (MouseUp only), calls SendFrameUIMsg with msgid=0x31
- [ ] `ButtonClickByHash(hash)` — convenience wrapper
- [ ] `ClickFrameMessage(frame, msgid, wparam_data)` — generic frame message sender for non-button controls
- [ ] Test: click Play button at character select — enters game
- [ ] Test: click merchant Craft button — crafts item
- [ ] Test: click Reconnect YES — handles disconnect popup

**Required Reading:**
- `research/GWCA_Disassembly_Research/GWCA_ButtonClick_Research.md` — full ButtonClick chain, frame offsets, `_WriteLE32` bug
- `research/GWCA_Disassembly_Research/CharSelect_ButtonClick_Research.md` — character select flow, game thread requirement
- `research/GWCA_Disassembly_Research/GWCA_Crafting_Research.md` — merchant dialog frame hashes, TransactItem signature

**Also Reference:** `GWA Censured/lib/custom/GWA2_FrameUI.au3` `ClickButtonByHash()`

---

#### GWA3-022 — PartyMgr (Heroes + Henchmen)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-006, GWA3-010, GWA3-012 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/PartyMgr.cpp` — hero/henchman management, flagging, behavior.

**Acceptance Criteria:**
- [ ] `AddHero(id)`, `KickHero(id)`, `KickAllHeroes()`
- [ ] `AddHenchman(id)`, `KickHenchman(id)`
- [ ] `SetHeroBehavior(hero_index, behavior)` — fight/guard/avoid
- [ ] `FlagHero(hero_index, x, y)`, `FlagAll(x, y)`
- [ ] `LockHeroTarget(hero_index, target_id)`
- [ ] `LeaveParty()`, `InvitePlayer(name)`, `KickPlayer(id)`
- [ ] `Tick(flag)` / `SetTickToggle(enable)` — party ready check
- [ ] `UnflagHero(hero_index)` / `UnflagAll()` — cancel hero flags
- [ ] `GetIsPartyDefeated()` — wipe detection
- [ ] `GetIsPartyInHardMode()` / `GetIsHardModeUnlocked()` — hard mode state
- [ ] `ReturnToOutpost()` — return after defeat
- [ ] `SetPetBehavior(owner_agent_id, behavior)` — pet AI control
- [ ] `RespondToPartyRequest(party_id, accept)` — accept/decline party invites
- [ ] Test: add 3 heroes, flag them to a position, change behavior, detect wipe

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/PartyMgr.h`
**AutoIt Reference:** `GWA2.au3` hero/party functions, packets 0x15-0x1F, 0x9C-0xAF

---

#### GWA3-023 — QuestMgr + DialogMgr

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-006, GWA3-010, GWA3-013 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/QuestMgr.cpp` — quest tracking, dialog responses.

**Acceptance Criteria:**
- [ ] `Dialog(dialog_id)` — sends dialog packet
- [ ] `SetActiveQuest(quest_id)` — sends set active packet
- [ ] `AbandonQuest(quest_id)` — sends abandon packet
- [ ] `SkipCinematic()` — sends cinematic skip packet
- [ ] Test: accept a quest via dialog, set it active

**Reference:** `GWA2.au3` Dialog/AbandonQuest, packets 0x3B, 0x11, 0x14, 0x63

---

#### GWA3-024 — ChatMgr + Rendering Hook + TradeMgr

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-006, GWA3-005, GWA3-010, GWA3-015 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement remaining managers: chat send/receive hook, rendering toggle hook, player trade, friend list, titles.

**Acceptance Criteria:**
- [ ] **ChatMgr:** `SendChat(message, channel)`, `SendWhisper(target, message)`, `WriteToChat(message)`
- [ ] **ChatMgr:** Chat receive hook — detour chat log function, fire registered callback with channel + message
- [ ] **RenderMgr:** Install render hook, `SetRenderingEnabled(bool)`, `GetPing()`
- [ ] **TradeMgr:** `InitiateTrade(agent_id)`, `OfferItem(item_id)`, `SubmitOffer()`, `AcceptTrade()`, `CancelTrade()`
- [ ] **TradeMgr:** Install trader hook — capture quote ID and cost value, `GetTraderCostValue()`
- [ ] **FriendMgr:** `AddFriend(name)`, `RemoveFriend(name)`, `SetPlayerStatus(status)`
- [ ] **TitleMgr:** `GetTitleProgress(title_id)`, `SetDisplayedTitle(title_id)`
- [ ] Test: send chat message, toggle rendering off/on, read ping value

**Reference:** `GWA2_Assembly_Chatlog.au3`, `GWA2_Assembly.au3` RenderingMod/TraderProc, `GWA2.au3` chat/trade/friend functions

---

### Epic 5: C++ Bot Module

> **Architecture decision:** Bot logic lives inside gwa3.dll, not in AutoIt.
> The DLL is both the game API AND the bot. One injection, zero IPC.
> See [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md) for testing approach.
>
> **Future option:** Add a named pipe/TCP IPC server later (GWA3-026) to allow
> external scripting in Python/AutoIt/Lua without recompilation.

#### GWA3-025 — Bot Framework + State Machine Core

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-006, GWA3-016..024, GWA3-049, GWA3-051 |
| **Blocks** | GWA3-026, GWA3-028..032 |
| **Parallel Group** | — |

**Description:**
Implement the bot framework inside gwa3.dll: a bot thread, state machine infrastructure, configuration loading, and logging.

The bot thread spawns during `InitThread` (after scanner + hooks are ready) and runs a main loop that dispatches to the active bot module's state machine. All game commands go through `GameThread::Enqueue()`.

**Architecture:**

```cpp
// src/bot/BotFramework.h
namespace GWA3::Bot {
    enum class BotState { Idle, CharSelect, InTown, Traveling, InDungeon, Looting, Merchant, Error };

    struct BotConfig {
        bool use_consets;
        bool use_stones;
        bool hard_mode;
        bool disable_rendering;
        uint32_t hero_config[7];     // hero IDs
        uint32_t skill_template[8];  // skill bar
        // ... loaded from .ini or hardcoded
    };

    void Start();                     // Spawn bot thread
    void Stop();                      // Signal bot thread to exit
    bool IsRunning();
    BotState GetState();

    // Bot modules register themselves
    using StateHandler = std::function<BotState(BotConfig&)>;
    void RegisterStateHandler(BotState state, StateHandler handler);
}
```

**Acceptance Criteria:**
- [ ] `Bot::Start()` / `Bot::Stop()` — spawn/kill bot thread
- [ ] State machine loop: reads current state → calls handler → transitions to returned state
- [ ] `BotConfig` struct loaded from INI file or defaults
- [ ] Logging to `gwa3_bot.log` with timestamps and state transitions
- [ ] Clean shutdown: bot thread exits, game hooks remain active
- [ ] Console output via `AllocConsole()` for real-time monitoring
- [ ] Error state: catches exceptions in handlers, logs, transitions to Error
- [ ] Test: start bot, verify it enters Idle state, stop bot cleanly

---

#### GWA3-026 — Froggy HM Bot Module (C++ Port)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | XL |
| **Depends On** | GWA3-025 |
| **Blocks** | GWA3-028..032 |
| **Parallel Group** | — |

**Description:**
Port Froggy HM bot logic from `Froggy_HM_v1.6.au3` to C++ as a bot module inside gwa3.dll.

**What gets ported:**
- Character select → Play button click
- Town setup: hero roster, skillbar loading, consumables, hard mode toggle, title activation
- Travel to Bogroot Growths
- Floor 1 + Floor 2 waypoint routes with combat
- Loot collection, identification, salvage
- Merchant sell/craft cycle
- Loop back to character select or retry on wipe
- Run statistics (time, loot, deaths)

**What does NOT get ported (stays as API calls):**
- All game interaction goes through the manager APIs (AgentMgr, SkillMgr, ItemMgr, etc.)
- No raw memory reads — everything through typed C++ structs
- No packet header constants in bot logic — use manager wrappers

```cpp
// src/bot/FroggyHM.h
namespace GWA3::Bot::Froggy {
    void Register();  // Register state handlers with framework

    // State handlers (each returns next state)
    BotState HandleCharSelect(BotConfig& cfg);
    BotState HandleTownSetup(BotConfig& cfg);
    BotState HandleTravel(BotConfig& cfg);
    BotState HandleFloor1(BotConfig& cfg);
    BotState HandleFloor2(BotConfig& cfg);
    BotState HandleLoot(BotConfig& cfg);
    BotState HandleMerchant(BotConfig& cfg);
    BotState HandleWipe(BotConfig& cfg);
}
```

**Source material:**
- `GWA Censured/Froggy_HM_v1.6.au3` — main script (~2K lines)
- `GWA Censured/GWA_Logic_Censured_NEW.au3` — farm route logic, waypoints
- `GWA Censured/lib/custom/BotCore-Waypoints.au3` — waypoint movement
- `GWA Censured/lib/custom/BotCore-Combat.au3` — combat AI
- `GWA Censured/lib/custom/BotCore-Loot.au3` — loot pickup rules
- `GWA Censured/hero_configs/` — hero build definitions

**Acceptance Criteria:**
- [ ] All Froggy HM states implemented as C++ handler functions
- [ ] Waypoint arrays ported (Floor 1 + Floor 2 coordinates)
- [ ] Combat AI: target selection, skill priority, hero skill usage
- [ ] Loot rules: pickup filter by rarity/model ID, salvage rules, sell rules
- [ ] Hero setup: add heroes, load skillbars, set behaviors
- [ ] Consumable usage: consets, stones, scrolls
- [ ] Merchant interaction: sell junk, craft consumables via frame clicks
- [ ] Wipe recovery: detect party defeated → return to outpost → retry
- [ ] Run counter and timing statistics logged
- [ ] Title progress logged between runs
- [ ] Compiles and links into gwa3.dll

---

#### GWA3-026 — IPC Server (Optional: External Scripting Support)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-025 |
| **Blocks** | — |
| **Parallel Group** | — |

**Description:**
**OPTIONAL.** Add a named pipe or TCP server to gwa3.dll that exposes the game API to external processes. Enables bot scripting in Python, AutoIt, Lua, or any language with socket/pipe support.

Only build this if we want to iterate on bot logic without recompilation, or want to support multiple scripting languages.

**Protocol:** JSON-RPC over named pipe (`\\.\pipe\gwa3`) or TCP (`localhost:9999`).

```json
// Request
{"method": "GetMyID", "id": 1}
// Response
{"result": 12345, "id": 1}

// Request
{"method": "Move", "params": {"x": -5765.0, "y": -5468.0}, "id": 2}
// Response
{"result": true, "id": 2}
```

**Acceptance Criteria:**
- [ ] Named pipe server starts in background thread on DLL init
- [ ] JSON-RPC protocol with method dispatch to manager functions
- [ ] Supports ~50 most-used API calls (movement, skills, items, map, party, UI)
- [ ] Thread-safe: pipe handler enqueues commands via `GameThread::Enqueue()`
- [ ] Python client library: `gwa3.py` with `connect()`, `get_my_id()`, `move()`, etc.
- [ ] Handles multiple concurrent connections
- [ ] Test: Python script connects, reads agent ID, moves character

---

### Epic 6: Integration Testing

> Each integration ticket tests a specific bot flow segment.
> **GWA3-028..032 can run in parallel** once GWA3-026 is done.

#### GWA3-028 — Integration: Character Select + Login

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-021, GWA3-026 |
| **Blocks** | GWA3-033 |
| **Parallel Group** | PG-INTEGRATION (with GWA3-029..032) |

**Description:**
End-to-end test: GW at character select → click Play → enter game → verify agent data.

**Acceptance Criteria:**
- [ ] `GWA3_ButtonClickByHash(PlayButton)` enters game from char select
- [ ] `GWA3_IsFrameVisible(PlayGreyed)` correctly detects greyed state
- [ ] Reconnect popup handling: `GWA3_IsReconnectDialogShowing()` + click YES
- [ ] After entering game: `GWA3_GetMyID()` returns valid ID, `GWA3_GetMapID()` returns expected map
- [ ] Passes 5 consecutive login cycles without crash

---

#### GWA3-029 — Integration: Hero Setup + Consumables

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-026 |
| **Blocks** | GWA3-033 |
| **Parallel Group** | PG-INTEGRATION |

**Description:**
Test hero team formation and consumable usage in town.

**Acceptance Criteria:**
- [ ] Kick all heroes, add specific heroes by ID
- [ ] Load skillbar templates for player and each hero
- [ ] Set hero behaviors (fight/guard)
- [ ] Use consumables (conset, stones) from inventory
- [ ] Read party size and hero agent IDs — all correct
- [ ] No crashes during setup sequence

---

#### GWA3-030 — Integration: Travel + Movement + Combat

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-026 |
| **Blocks** | GWA3-033 |
| **Parallel Group** | PG-INTEGRATION |

**Description:**
Test map travel → movement waypoints → combat loop.

**Acceptance Criteria:**
- [ ] Travel from outpost to explorable area (e.g., Bogroot Growths)
- [ ] `WaitMapLoading` works correctly with map load hook
- [ ] Move to a sequence of waypoints
- [ ] Target enemies via agent array scan
- [ ] Use skills on targets, verify skill recharge tracking
- [ ] Characters survive a fight (HP tracking works)
- [ ] Complete a dungeon floor worth of movement + combat

---

#### GWA3-031 — Integration: Loot + Inventory + Salvage

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-026 |
| **Blocks** | GWA3-033 |
| **Parallel Group** | PG-INTEGRATION |

**Description:**
Test item pickup, identification, salvage, and inventory management.

**Acceptance Criteria:**
- [ ] Pick up dropped items from ground
- [ ] Read item properties (model ID, rarity, identified status)
- [ ] Identify items with kit
- [ ] Salvage items (full salvage session: open → materials → done)
- [ ] Move items between bags
- [ ] Destroy junk items
- [ ] No inventory corruption

---

#### GWA3-032 — Integration: Merchant + Crafting

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-026 |
| **Blocks** | GWA3-033 |
| **Parallel Group** | PG-INTEGRATION |

**Description:**
Test NPC merchant interaction via frame clicks + packet trading.

**Acceptance Criteria:**
- [ ] Open merchant dialog via NPC interact
- [ ] Click craft tab via `ButtonClickByHash(CraftTab)`
- [ ] Click craft button via `ButtonClickByHash(CraftButton)`
- [ ] Buy items from merchant via `BuyItem`
- [ ] Sell items to merchant via `SellItem`
- [ ] Close dialog via `ButtonClickByHash(GoodbyeButton)`
- [ ] No crashes during merchant flow

---

### Epic 7: Hardening + Production Readiness

#### GWA3-033 — Full Froggy HM End-to-End

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | XL |
| **Depends On** | GWA3-028, GWA3-029, GWA3-030, GWA3-031, GWA3-032 |
| **Blocks** | GWA3-036 |
| **Parallel Group** | — |

**Description:**
Run Froggy_HM_v1.6.au3 on the GWA3 stack (gwa3.dll + GWA3_Compat.au3) for 10+ consecutive runs.

**Acceptance Criteria:**
- [ ] Character select → enter game → hero setup → travel → dungeon clear → loot → merchant → repeat
- [ ] 10 consecutive successful runs without crash
- [ ] No memory leaks (GW process memory stable over time)
- [ ] Rendering toggle works (disable during dungeon, enable in town)
- [ ] Chat hook fires correctly for whisper/trade messages
- [ ] Run time per loop comparable to old AutoIt+gwca stack
- [ ] Title tracking works — Vanguard/Norn/Asura/Deldrimor points update between runs
- [ ] Cinematic skip works — dungeon cutscenes are bypassed
- [ ] Wipe detection works — `GetIsPartyDefeated()` triggers retry logic
- [ ] Hard mode state reads correctly — `GetIsPartyInHardMode()` matches toggle
- [ ] String decoding works — quest objectives display correctly in logs
- [ ] Ground loot pickup works — `PickUpItem()` collects dropped items
- [ ] Instance time tracked — run timing uses `GetInstanceTime()` not system clock

---

#### GWA3-034 — Multi-Client Injector

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | M |
| **Depends On** | GWA3-002 |
| **Blocks** | — |
| **Parallel Group** | PG-HARDENING (with GWA3-035, GWA3-036) |

**Description:**
Extend injector.exe to handle multiple GW clients simultaneously.

**Acceptance Criteria:**
- [ ] `--all` flag injects into all running GW instances
- [ ] `--list` flag shows all GW windows with PID + character name
- [ ] Each GW process gets independent gwa3.dll instance
- [ ] Inject/eject without affecting other clients
- [ ] Works with GWLauncher multi-client setup

---

#### GWA3-035 — Pattern Health Check Tool

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-005 |
| **Blocks** | — |
| **Parallel Group** | PG-HARDENING |

**Description:**
Build `tools/pattern_test.cpp` → `pattern_test.exe` that injects, runs all scans, reports pass/fail, and ejects. For verifying patterns after GW client updates.

**Acceptance Criteria:**
- [ ] Injects gwa3.dll, calls `GWA3_Initialize()`, reads `GWA3_GetScanStatus()`
- [ ] Dumps full pattern report: name, expected section, found address, pass/fail
- [ ] Saves report to `pattern_report.txt` with timestamp
- [ ] Non-zero exit code if any P0/P1 pattern fails
- [ ] Can be run from CI or batch script

---

#### GWA3-036 — Crash Protection + SEH Wrappers

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-033 |
| **Blocks** | — |
| **Parallel Group** | PG-HARDENING |

**Description:**
Add structured exception handling around all game function calls. Log crashes instead of killing the game process.

**Acceptance Criteria:**
- [ ] SEH `__try/__except` wrappers around every game function call in managers
- [ ] Crash log: function name, parameters, exception code, timestamp
- [ ] Game continues running after a caught exception (graceful degradation)
- [ ] Null pointer checks on all agent/item/frame pointers before dereference
- [ ] Thread safety audit: no data races between game thread and AutoIt DllCall thread

---

### Epic 8: Research-Derived Capabilities

> These tickets cover capabilities discovered in the research that the original plan missed.
> They expand GWA3 beyond what the AutoIt codebase currently provides.

#### GWA3-037 — Hook Engine (Optional: Custom Hooking Library)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | XL |
| **Depends On** | GWA3-006 |
| **Blocks** | — |
| **Parallel Group** | PG-RESEARCH-EXT (with GWA3-038..041) |

**Description:**
Build a custom hooking engine modeled on GWCA's architecture, as an alternative/complement to MinHook. This is OPTIONAL — MinHook works fine for our needs — but the research fully documents how to build one.

Only pursue this if MinHook proves insufficient (e.g., can't hook certain targets, conflicts with game code).

**What the research provides:**
- Hook table: 0x2c-byte entries in heap array (target, detour, replay slot, saved bytes, flags, relocation tables)
- Replay stub pool: VirtualAlloc RWX pages, 0x20-byte slots, free-list allocator
- Instruction relocator: handles E8 (CALL), E9 (JMP), 0F80+ (long conditional), EB (short JMP), C2 (RET)
- Thread-safe toggling: suspend peers → repair EIP via relocation byte arrays → write/remove patch → resume
- Two patch modes: 5-byte at target (normal) or 7-byte at target-5 with `EB F9` trampoline (special)

**Acceptance Criteria:**
- [ ] Hook table management with configurable capacity
- [ ] Replay stub allocator (RWX page pool, 0x20-byte slots)
- [ ] Instruction relocator for displaced prologue bytes
- [ ] Thread-safe enable/disable with EIP repair
- [ ] `CreateHook(target, detour, out_original)` matching GWCA's signature
- [ ] Validate against live hook table dump from research

**Required Reading:**
- `research/GWCA_Disassembly_Research/GWCA_ReplayStub_Layout_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_HookToggle_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_HookLifecycle_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_LiveHookTable_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_HookInstaller_DeepDive.md`
- `research/GWCA_Disassembly_Research/GWCA_HookRecordPool_Addendum.md`

---

#### GWA3-038 — Memory Patcher Module

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-005, GWA3-006 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-RESEARCH-EXT |

**Description:**
Implement a memory patcher separate from the detour hook system. GWCA uses this for 3 specific game-side patches: camera unlock, level-data bypass, and map/port bypass.

**Key distinction from hooks:** Memory patching is direct byte-buffer replacement — no replay stubs, no instruction relocation, no thread suspension. Simpler lifecycle: `SetPatch()` stages → `EnableHooks()` applies → `DisableHooks()` restores.

**Acceptance Criteria:**
- [ ] `MemoryPatcher` struct: target address, patched-bytes buffer, original-bytes buffer, size, enabled flag
- [ ] `SetPatch(address, bytes, size)` — stages patch without applying
- [ ] `SetRedirect(address, target)` — 5-byte JMP redirect helper
- [ ] `EnablePatches()` — apply all staged patches (VirtualProtect → write → restore protection)
- [ ] `DisablePatches()` — restore all original bytes
- [ ] Global patcher vector with independent enable flag
- [ ] Implement the 3 known patches:
  - Camera unlock: `EB 0F` at camera update (skips float copy-back)
  - Level-data bypass: `EB` replacing `74` (JZ → unconditional JMP)
  - Map/port bypass: `90 90` replacing `75 0C` (NOP out conditional branch)
- [ ] Test: enable camera unlock, verify camera is freed

**Required Reading:**
- `research/GWCA_Disassembly_Research/GWCA_MemoryPatcher_LiveAddendum.md`
- `research/GWCA_Disassembly_Research/GWCA_MemoryPatcher_Ownership_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_GameTargetPatch_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_PatchObject_Ownership_Addendum.md`

---

#### GWA3-039 — Callback Registry + Module Ownership

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | L |
| **Depends On** | GWA3-006, GWA3-020 |
| **Blocks** | GWA3-040 |
| **Parallel Group** | PG-RESEARCH-EXT |

**Description:**
Implement the GWCA-style callback registration system: three registration APIs for UI component creation, frame UI messages, and global UI messages. Includes module ownership tracking for bulk cleanup.

This enables external consumers (future plugins, Python bindings) to register callbacks on game events — not just send commands.

**Acceptance Criteria:**
- [ ] `RegisterUIMessageCallback(HookEntry*, UIMessage, callback, altitude)` — persistent callback on global UI messages
- [ ] `RegisterFrameUIMessageCallback(HookEntry*, UIMessage, callback, altitude)` — persistent callback on frame messages
- [ ] `RemoveUIMessageCallback(HookEntry*)` — clean removal
- [ ] Callback records: 0x30 bytes (altitude, HookEntry*, std::function SBO, function_impl pointer)
- [ ] Altitude-sorted insertion for priority ordering
- [ ] Module ownership tracking: range-table maps callback addresses to module handles
- [ ] Bulk cleanup: `RemoveAllCallbacksForModule(module_handle)`
- [ ] Thread-safe: all registration/removal goes through critical section
- [ ] Test: register callback on kMapLoaded, verify it fires on zone transition

**Required Reading:**
- `research/GWCA_Disassembly_Research/GWCA_Toolbox_RegistrationPaths_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_Toolbox_ModuleOwnership_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_Toolbox_StdFunction_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_Toolbox_RecordLayout_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_Toolbox_RegistryVector_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_LambdaConvention_Addendum.md`

---

#### GWA3-040 — Key Input + Preference System

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-006, GWA3-020, GWA3-039 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-RESEARCH-EXT |

**Description:**
Implement keyboard input dispatch (keydown/keyup/keypress) and game preference management. Research reveals keys are NOT a separate system — they're frame-message adapters on messages 0x20 (keydown) and 0x22 (keyup).

**Acceptance Criteria:**
- [ ] `Keydown(ControlAction)` — send frame message 0x20 with action code
- [ ] `Keyup(ControlAction)` — send frame message 0x22 with action code
- [ ] `Keypress(ControlAction)` — immediate 0x20, then deferred 0x22 via GameThread::Enqueue (two-stage choreography)
- [ ] `RegisterKeydownCallback(HookEntry*, callback, altitude)` — adapter wrapping user callback → RegisterFrameUIMessageCallback(msg=0x20)
- [ ] `RegisterKeyupCallback(HookEntry*, callback, altitude)` — same for 0x22
- [ ] `SetPreference(EnumPreference, value)` — deferred lambda via GameThread::Enqueue
- [ ] `SetPreference(FlagPreference, value)` — deferred lambda
- [ ] `SetPreference(NumberPreference, value)` — special: contains renderer logic
- [ ] `SetPreference(StringPreference, value)` — deferred lambda
- [ ] ControlAction enum: Interact=0x80, MoveForward=0xAD, TargetNearestEnemy=0x93, UseSkill1-8=0xA4-0xAB, etc.
- [ ] Test: `Keypress(TargetNearestEnemy)` selects nearest enemy
- [ ] Test: `SetPreference(FlagPreference::ShowChatTimestamps, true)` toggles setting

**Required Reading:**
- `research/GWCA_Disassembly_Research/GWCA_KeyAdapter_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_KeypressLambda_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_SetPreference_OverloadMatrix_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_KeyLambdaVTable_Addendum.md`
- `research/GWCA_Disassembly_Research/GWCA_StringPreferenceLambda_Addendum.md`

---

#### GWA3-049 — PlayerMgr (Titles + Profession + Player Data)

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | M |
| **Depends On** | GWA3-005, GWA3-006, GWA3-010 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/PlayerMgr.cpp` — title tracking, player data, secondary profession change. Title functions are called ~300 times in Froggy HM (GetVanguardTitle, GetNornTitle, GetAsuraTitle, GetDeldrimorTitle).

**Acceptance Criteria:**
- [ ] `SetActiveTitle(TitleID)` — display title for reputation gain
- [ ] `RemoveActiveTitle()` — hide title
- [ ] `GetTitleTrack(TitleID)` — get title progress data
- [ ] `GetTitleData(TitleID)` — get title client data (current/max points)
- [ ] `GetActiveTitleId()` — which title is displayed
- [ ] `GetPlayerName(player_id)` — resolve player name
- [ ] `GetPlayerByID(player_id)` — get player struct
- [ ] `GetPlayerArray()` — all players in instance
- [ ] `GetAmountOfPlayersInInstance()` — player count
- [ ] `ChangeSecondProfession(profession, hero_index)` — change secondary prof for player or hero
- [ ] Test: read title progress, confirm matches in-game display

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/PlayerMgr.h`

---

#### GWA3-050 — MemoryMgr (Version + Timers + Window Handle)

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | S |
| **Depends On** | GWA3-005 |
| **Blocks** | GWA3-025, GWA3-035 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/MemoryMgr.cpp` — GW client version, skill timer, window handle, in-process memory allocation.

**Acceptance Criteria:**
- [ ] `GetGWVersion()` — client build number (critical for pattern validation)
- [ ] `GetSkillTimer()` — global skill timer (used for recharge calculations)
- [ ] `GetGWWindowHandle()` — HWND of game window
- [ ] `GetPersonalDir(buf, size)` — GW user data directory
- [ ] `MemAlloc(size)` / `MemFree(ptr)` — game heap allocation (for in-process data)
- [ ] Test: GetGWVersion returns plausible build number

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/MemoryMgr.h`

---

#### GWA3-051 — String Encoding/Decoding (AsyncDecodeStr)

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | M |
| **Depends On** | GWA3-005, GWA3-006 |
| **Blocks** | GWA3-025 |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement string encoding/decoding for game text (quest objectives, item names, NPC names, skill descriptions). The game uses encoded `wchar_t*` strings that must be decoded asynchronously via a game function.

The existing AutoIt code uses `ValidateAsyncDecodeStr` (scanned via assertion pattern) for this.

**Acceptance Criteria:**
- [ ] `AsyncDecodeStr(enc_str, buffer, size)` — decode encoded string to readable text (blocking wrapper)
- [ ] `AsyncDecodeStr(enc_str, callback, param, language)` — async version with callback
- [ ] `IsValidEncStr(enc_str)` — validate encoded string format
- [ ] `UInt32ToEncStr(value, buffer, count)` — encode integer to game string format
- [ ] `EncStrToUInt32(enc_str)` — decode game string to integer
- [ ] Test: decode a known quest name, verify readable output

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/UIMgr.h` (string functions section)
**AutoIt Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` `ValidateAsyncDecodeStr` pattern

---

#### GWA3-052 — CameraMgr

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | S |
| **Depends On** | GWA3-005, GWA3-006 |
| **Blocks** | — |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/CameraMgr.cpp` — camera control, field of view, unlock, fog toggle. Nice-to-have for debugging and observation, not required for core Froggy HM loop.

**Acceptance Criteria:**
- [ ] `GetCamera()` — get Camera struct pointer
- [ ] `SetMaxDist(dist)` — set max camera zoom distance
- [ ] `SetFieldOfView(fov)` — adjust FOV
- [ ] `UnlockCam(flag)` — free camera movement
- [ ] `GetCameraUnlock()` — check if camera is unlocked
- [ ] `SetFog(flag)` — toggle fog rendering
- [ ] `GetYaw()` — current camera rotation
- [ ] `ComputeCamPos(dist)` — compute camera position at distance
- [ ] Test: unlock camera, set max distance to 2000, verify zoom works

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/CameraMgr.h`

---

#### GWA3-053 — StoCMgr (Server-to-Client Packet Callbacks)

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | L |
| **Depends On** | GWA3-005, GWA3-006 |
| **Blocks** | — |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/StoCMgr.cpp` — register callbacks for incoming server-to-client packets. Enables reactive event monitoring (damage events, loot drops, NPC spawns) without polling game state.

Nice-to-have — the bot currently works by polling memory state. StoC callbacks would improve responsiveness but aren't blocking.

**Acceptance Criteria:**
- [ ] `RegisterPacketCallback(entry, header, callback, altitude)` — register handler for incoming packet type
- [ ] `RegisterPostPacketCallback(entry, header, callback)` — post-processing callback
- [ ] `RemoveCallback(header, entry)` — remove specific callback
- [ ] `RemoveCallbacks(entry)` — remove all callbacks for an entry
- [ ] `EmulatePacket(packet)` — inject fake packet for testing
- [ ] Test: register callback for agent spawn packet, verify it fires when NPC appears

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/StoCMgr.h`

---

#### GWA3-054 — GuildMgr

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | S |
| **Depends On** | GWA3-005, GWA3-006 |
| **Blocks** | — |
| **Parallel Group** | PG-MANAGERS |

**Description:**
Implement `managers/GuildMgr.cpp` — guild data reading, guild hall travel. Low priority — TravelGH/LeaveGH already handled via packet headers.

**Acceptance Criteria:**
- [ ] `GetPlayerGuild()` — get player's guild info
- [ ] `GetGuildArray()` — all guilds in context
- [ ] `TravelGH()` / `TravelGH(GHKey)` — travel to guild hall
- [ ] `LeaveGH()` — leave guild hall
- [ ] `GetPlayerGuildAnnouncement()` — guild announcement text
- [ ] Test: read guild name, travel to guild hall

**GWCA Reference:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/GuildMgr.h`

---

#### GWA3-041 — Research Digest: Py4GW API Surface Audit

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-025 |
| **Blocks** | — |
| **Parallel Group** | PG-RESEARCH-EXT |

**Description:**
Audit Py4GW's API surface (documented in research) against our GWA3 export list. Identify any capabilities Py4GW exposes that we're missing, and add them to the bridge exports if relevant.

Py4GW provides: PyPlayer, PyParty, PyInventory, PyQuest, PyMerchant, PySkillbar, PyUIManager, PyScanner, PyCallback. Their transport layer uses UIMessages, native calls, chat commands, and frame clicks — same as us.

**Acceptance Criteria:**
- [ ] Cross-reference Py4GW's visible APIs against GWA3 manager API surface
- [ ] Document gaps (if any) as new export tickets
- [ ] Particularly check: `DepositFaction_Func`, `SetActiveTitle_Func`, `RawSendUIMessage` — do we expose equivalents?
- [ ] Report: which Py4GW features map 1:1 to our exports, which need new work

**Required Reading:**
- `research/GWCA_Disassembly_Research/Py4GW_GWCA_Botting_Research.md`

---

### Epic 9: Testing Infrastructure

> Testing happens in 3 layers: offline unit tests (no game), injection smoke tests (read-only),
> and behavioral tests (send commands). See [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md) for full rationale.
>
> Test tickets are designed to run **in parallel with their corresponding implementation tickets**.
> An agent building GWA3-007 (Agent Struct) should also write GWA3-043 (struct offset tests) as part of the same work.

#### GWA3-042 — Test Harness + CMake Test Target

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | M |
| **Depends On** | GWA3-001 |
| **Blocks** | GWA3-043, GWA3-044, GWA3-045, GWA3-046, GWA3-047 |
| **Parallel Group** | PG-FOUNDATION (with GWA3-002, GWA3-003, GWA3-004) |

**Description:**
Create the offline test infrastructure: a `tests/` directory, CMake test target (`gwa3_tests.exe`), and a test runner macro. This runs without a game client — pure compile-time and logic tests.

**Acceptance Criteria:**
- [ ] `tests/` directory with `CMakeLists.txt` or integrated into root CMakeLists
- [ ] `gwa3_tests.exe` target compiles as x86, links against `gwa3_core` (static lib or object lib — NOT the DLL itself)
- [ ] Test runner macro: `GWA3_TEST(name, body)` that logs pass/fail and aborts on first failure
- [ ] `static_assert` helper: `GWA3_CHECK_OFFSET(StructType, field, expected_offset)`
- [ ] Post-build step: `add_custom_command(POST_BUILD COMMAND gwa3_tests)` — tests run on every build
- [ ] Passes with zero tests (empty harness compiles and exits 0)

**Reference:** [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md) Layer 1

---

#### GWA3-043 — Offline Tests: Struct Offset Validation

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | M |
| **Depends On** | GWA3-042, GWA3-007..015, GWA3-020, GWA3-049..050 |
| **Blocks** | GWA3-045 |
| **Parallel Group** | — (built incrementally as each struct ticket completes) |

**Description:**
`static_assert` tests for every game struct's field offsets and total size. These catch the #1 bug class (wrong field position → garbage data) at **compile time**.

**Implementation note:** This ticket is worked on incrementally. Each agent completing a struct ticket (GWA3-007..015, GWA3-020) should add its offset assertions before marking the struct ticket done. This ticket tracks the aggregate.

**Acceptance Criteria:**
- [ ] `tests/test_struct_offsets.cpp` with `static_assert` for every field in:
  - `Agent` (446 bytes, ~30 fields: id, x, y, z, hp, energy, model_id, profession, level, team, etc.)
  - `Skill` (~128 bytes, ~15 key fields: id, activation_time, recharge, energy_cost, etc.)
  - `SkillbarSlot` and `Skillbar`
  - `Item` (88 bytes, ~12 fields: id, model_id, type, value, quantity, equipped, etc.)
  - `Bag`
  - `Buff`, `Effect`
  - `Frame` (0x1C8 bytes: callbacks, child_offset_id, frame_id, relation, hash_id, state)
  - `Title`, `TitleTier`, `TitleClientData` (from GWA3-049)
  - `Camera` (position, yaw, pitch, FOV, zoom — from GWA3-052 if implemented)
  - `Player` (from GWA3-049)
- [ ] `sizeof()` checks for every struct
- [ ] All assertions pass at compile time (build fails if any offset is wrong)
- [ ] Cross-referenced against BOTH `GWA2_Assembly.au3` struct templates AND GWCA headers at `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/GameEntities/`

**Reference:** [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md) "Struct layout validation"

---

#### GWA3-044 — Offline Tests: Header Constants + Pattern Parsing

| Field | Value |
|-------|-------|
| **Assignee** | agent |
| **Status** | `done` |
| **Estimate** | S |
| **Depends On** | GWA3-042, GWA3-003, GWA3-004 |
| **Blocks** | — |
| **Parallel Group** | PG-OFFLINE-TESTS (with GWA3-043) |

**Description:**
Offline tests for packet header constants (values match GWA2_Headers.au3) and scanner pattern parsing logic (hex string → byte array, near-call resolution, wildcard masks).

**Acceptance Criteria:**
- [ ] `tests/test_headers.cpp` — `static_assert` for all 100+ packet header constants against GWA2_Headers.au3 values
- [ ] `tests/test_scanner_logic.cpp`:
  - Pattern string parsing: `"55 8B EC ?? 6A 00"` → correct bytes + mask
  - Wildcard handling: `??` positions marked in mask
  - Near-call resolution: `E8 <rel32>` at known address → correct target
  - Edge cases: empty pattern, all-wildcards, pattern longer than section
- [ ] All tests pass as part of `gwa3_tests.exe`

**Reference:** [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md) "Pattern parsing" and "Packet header constants"

---

#### GWA3-045 — Injection Smoke Test: Pattern Scan + State Read

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-002, GWA3-005, GWA3-043 |
| **Blocks** | GWA3-046 |
| **Parallel Group** | — |

**Description:**
First live injection test. The DLL injects into GW.exe, resolves all scan patterns, reads game state, and writes a pass/fail report. **Read-only** — no hooks, no commands, no game state changes.

This is the **go/no-go gate** for Phase 2. If this passes, the foundation is solid.

**Acceptance Criteria:**
- [ ] `src/core/SmokeTest.h/cpp` — runs automatically during `InitThread` if `GWA3_SMOKE_TEST` env var or flag is set
- [ ] Validates all P0/P1 scan patterns resolve (non-null, non-negative-one)
- [ ] Reads and logs (core — from original plan):
  - Player agent ID (from `Offsets::MyID`)
  - Map ID (from `Offsets::InstanceInfo`)
  - Player position X, Y (from agent struct)
  - Player HP % (from agent struct)
  - Skillbar slot IDs (from skillbar struct)
  - Backpack slot count (from bag struct)
  - Ping value
- [ ] Reads and logs (from gap analysis — new managers):
  - GW client version via `MemoryMgr::GetGWVersion()` — plausible build number
  - Skill timer via `MemoryMgr::GetSkillTimer()` — non-zero, increasing
  - GW window handle via `MemoryMgr::GetGWWindowHandle()` — valid HWND
  - Active title ID via `PlayerMgr::GetActiveTitleId()` — valid enum value
  - Title progress via `PlayerMgr::GetTitleTrack(Vanguard)` — returns non-null struct
  - Player name via `PlayerMgr::GetPlayerName()` — non-null, non-empty wchar_t*
  - Instance time via `MapMgr::GetInstanceTime()` — non-zero, increasing
  - Party defeated state via `PartyMgr::GetIsPartyDefeated()` — returns false (in outpost)
  - Hard mode state via `PartyMgr::GetIsPartyInHardMode()` — boolean plausible
  - Foes killed/to kill via `MapMgr::GetFoesKilled()`/`GetFoesToKill()` — 0 in outpost
  - Alcohol level via `EffectMgr::GetAlcoholLevel()` — returns value 0-5
- [ ] Reads and logs (string decoding — critical utility):
  - `AsyncDecodeStr` on a known encoded string — decodes without crash
  - `IsValidEncStr` on valid/invalid inputs — correct true/false
- [ ] At character select: validates `FrameArray` resolves, `GetFrameByHash(PlayButton)` finds frame
- [ ] Writes report to `gwa3_smoke_report.txt` with timestamp, pattern count, pass/fail per check
- [ ] Exits with summary: `X/Y checks passed`
- [ ] Game does not crash during or after smoke test (10+ minutes idle)
- [ ] Works on both character select screen AND logged-in-to-map states

**How to run:** Launch GW → `injector.exe --smoke` → check `gwa3_smoke_report.txt`

**Reference:** [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md) Layer 2, also mirrors `GWA Censured/debug_scripts/test_no_gwca.au3` pattern

---

#### GWA3-046 — Behavioral Test: Commands + Game Thread

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | XL |
| **Depends On** | GWA3-006, GWA3-010, GWA3-045, GWA3-049, GWA3-050, GWA3-051 |
| **Blocks** | GWA3-047 |
| **Parallel Group** | — |

**Description:**
First behavioral test. Sends commands through the game thread hook and verifies outcomes by reading game state before/after. Requires a logged-in character in an outpost.

**Acceptance Criteria:**
- [ ] `src/core/BehavioralTest.h/cpp` — triggered by `GWA3_TEST_COMMANDS` flag
- [ ] **Movement test:**
  - Record start position
  - `CtoS::MoveToCoord(start_x + 200, start_y)`
  - Wait 3s
  - Read end position
  - Pass if moved > 50 units
- [ ] **Target test:**
  - Find nearest non-self alive agent
  - `CtoS::ChangeTarget(agent_id)`
  - Wait 500ms
  - Pass if `GetCurrentTargetID() == agent_id`
- [ ] **Game thread validation:**
  - Enqueue lambda that sets a flag
  - Wait 1s
  - Pass if flag is set (proves game thread is processing queue)
- [ ] **Packet send validation:**
  - Send a benign packet (e.g., ping reply header)
  - Pass if no crash after 5s
- [ ] **Title management test:**
  - `PlayerMgr::SetActiveTitle(Vanguard)` — set displayed title
  - Wait 500ms
  - `PlayerMgr::GetActiveTitleId()` — verify matches Vanguard
  - `PlayerMgr::RemoveActiveTitle()` — clear it
- [ ] **Profession change test (hero):**
  - Read hero's current secondary profession
  - `PlayerMgr::ChangeSecondProfession(Mesmer, hero_index)` — change hero secondary
  - Wait 500ms
  - Read hero's new secondary — verify it changed
  - Restore original profession
- [ ] **Buff/effect test:**
  - If character has any active effects: `EffectMgr::GetPlayerEffects()` returns non-empty array
  - `EffectMgr::GetPlayerEffectBySkillId(known_skill)` — returns non-null if buff active
  - If possible: `EffectMgr::DropBuff(buff_id)` — removes buff, verify removed
- [ ] **Cinematic test (if in explorable with cinematic):**
  - `MapMgr::GetIsInCinematic()` — detect state
  - `MapMgr::SkipCinematic()` — skip if active, verify state changes
- [ ] **Instance info test:**
  - `MapMgr::GetInstanceTime()` — read twice with 1s delay, verify second > first
  - `MapMgr::GetFoesKilled()` / `GetFoesToKill()` — valid counts in explorable, 0 in outpost
- [ ] **String decode test:**
  - Get any agent's encoded name via `AgentMgr::GetAgentEncName()`
  - `AsyncDecodeStr(enc_name, buffer, size)` — decode without crash
  - Verify decoded buffer is non-empty and contains readable characters
- [ ] All tests log pass/fail to `gwa3_command_report.txt`
- [ ] Game remains stable for 5 minutes after tests complete

**How to run:** Log in to a character in any outpost → `injector.exe --test-commands` → check report

**Reference:** [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md) Layer 3

---

#### GWA3-047 — Behavioral Test: Frame UI + ButtonClick

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-020, GWA3-021, GWA3-046 |
| **Blocks** | GWA3-028 |
| **Parallel Group** | — |

**Description:**
Frame UI behavioral test. Validates frame hash lookup and ButtonClick at character select screen. This is the last gate before integration testing.

**Acceptance Criteria:**
- [ ] **Frame lookup tests (read-only, always safe):**
  - `GetFrameByHash(PlayButton)` returns non-null
  - `GetFrameByHash(PlayGreyed)` returns non-null (may or may not be visible)
  - `GetFrameByHash(ReconnectYes)` — test visibility check
  - `GetFrameByHash(CharacterFrame)` returns non-null
  - All returned frames have valid `frame_id` and `frame_hash_id` fields
  - Frame state bits are plausible (created bit set, etc.)
- [ ] **Parent traversal test:**
  - Get Play button frame
  - Call `GetParent()` — returns non-null
  - Parent address is sane (within game memory range)
  - `parent + 0xA8` (callback array) is readable
- [ ] **ButtonClick test (DESTRUCTIVE — enters game!):**
  - Must be at character select with a character highlighted
  - `ButtonClickByHash(PlayButton)` — returns true
  - Wait for map loading to start (`IsMapLoading()` becomes true within 10s)
  - Pass if game begins loading a map
- [ ] Results logged to `gwa3_frame_report.txt`

**How to run:** Launch GW, reach character select → `injector.exe --test-frames` → check report

**Reference:** [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md) "Frame UI Tests", `research/GWCA_Disassembly_Research/CharSelect_ButtonClick_Research.md`

---

#### GWA3-048 — Bot Framework Smoke Test

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-025 |
| **Blocks** | GWA3-028, GWA3-029, GWA3-030, GWA3-031, GWA3-032 |
| **Parallel Group** | — |

**Description:**
Validates that the bot framework (GWA3-025) works correctly: thread lifecycle, state machine transitions, config loading, and logging. Runs inside gwa3.dll triggered by `GWA3_TEST_BOT` flag.

**Acceptance Criteria:**
- [ ] `Bot::Start()` spawns bot thread — verify thread is alive
- [ ] `Bot::GetState()` returns `Idle` initially
- [ ] Register a test state handler that transitions Idle → CharSelect → InTown → Idle
- [ ] Verify all transitions fire in correct order (logged)
- [ ] `BotConfig` loads from INI file — verify hero IDs, skill template, flags parsed
- [ ] `BotConfig` falls back to defaults when INI missing
- [ ] Error handler catches thrown exception → transitions to Error state → logged
- [ ] `Bot::Stop()` signals thread exit — verify thread terminates within 5s
- [ ] Console output shows real-time state transitions
- [ ] Log file `gwa3_bot.log` created with timestamps
- [ ] Bot framework does NOT send any game commands (pure state machine test)
- [ ] Game stable for 5 minutes with bot thread running idle
- [ ] All manager APIs accessible from bot thread via `GameThread::Enqueue()`

**Reference:** [GWA3_Testing_Strategy.md](GWA3_Testing_Strategy.md)

---

## Parallel Execution Summary

```
TIME ──────────────────────────────────────────────────────────────────►

WAVE 1 (Foundation — no deps):
  ┌─────────────┬──────────────┬───────────────┬────────────────┐
  │ GWA3-001    │              │               │                │
  │ CMake Setup │              │               │                │
  └──────┬──────┘              │               │                │
         │                     │               │                │
WAVE 2 (Foundation — depends on 001 only):
  ┌──────┴──────┬──────────────┼───────────────┼────────────────┐
  │ GWA3-002    │ GWA3-003     │ GWA3-004      │ GWA3-005*      │
  │ Injector    │ Scanner      │ Headers.h     │ Offsets*        │
  └─────────────┴──────┬───────┴───────────────┴────────┬───────┘
                       │            * needs 003 done     │
WAVE 3 (Structs — all parallel, need 003+005):           │
  ┌────────┬────────┬────────┬────────┬────────┬────────┬┴───────┐
  │ 007    │ 008    │ 009    │ 011    │ 012    │ 013    │ 014    │
  │ Agent  │ Skill  │ Item   │ Map    │ Party  │ Quest  │ Effect │
  │ struct │ struct │ struct │ struct │ struct │ struct │ struct │
  └───┬────┴───┬────┴───┬────┴───┬────┴───┬────┴───┬────┴───┬────┘
      │        │        │        │        │        │    ┌───┘
      │    Also parallel:  GWA3-006 (GameThread — needs 002+003+005)
      │                    GWA3-015 (Chat struct)
      │                    GWA3-020 (Frame UI — needs 003+005+006)
      │                    │
WAVE 4 (Managers + Packets — all parallel, need GameThread + structs):
  ┌───┴────┬────┴───┬────┴───┬────┴───┬────┴───┬────┴───┬───────┐
  │ 010    │ 016    │ 017    │ 018    │ 019    │ 022    │ 023   │
  │ CtoS   │ Agent  │ Skill  │ Item   │ Map    │ Party  │ Quest │
  │ Packet │ Mgr    │ Mgr    │ Mgr    │ Mgr    │ Mgr    │ Mgr   │
  └────────┴────────┴────────┴────────┴────────┴────────┴───────┘
  Also: GWA3-021 (ButtonClick — needs 020)
        GWA3-024 (Chat/Render/Trade Mgr)

WAVE 5 (Bot Module — needs all managers):
  ┌─────────────┐
  │ GWA3-025    │
  │ Bot Framewk │
  └──────┬──────┘
         │
  ┌──────┴──────┐
  │ GWA3-026    │
  │ Froggy C++  │
  └──────┬──────┘

WAVE 6 (Integration — all parallel, need Froggy module):
  ┌────────┬────────┬────────┬────────┬────────┐
  │ 028    │ 029    │ 030    │ 031    │ 032    │
  │ Login  │ Heroes │ Combat │ Loot   │ Merch  │
  └───┬────┴───┬────┴───┬────┴───┬────┴───┬────┘
      │        │        │        │        │
WAVE 7 (Endgame — needs all integration):
  ┌───┴────────┴────────┴────────┴────────┴────┐
  │ GWA3-033: Full Froggy HM 10+ runs          │
  └──────────────────┬─────────────────────────┘
                     │
  Also parallel anytime after deps:
  GWA3-034 (Multi-client), GWA3-035 (Pattern tool), GWA3-036 (SEH)
```

---

### Epic 11: LLM Bridge Gaps

> Tickets for missing game state observations and actions identified during the Froggy gap analysis.
> These fill the remaining holes needed for Gemma 4 to fully replicate Froggy's decision-making.
> All depend on existing manager infrastructure and the LLM bridge (implemented in `src/llm/`).

#### GWA3-055 — MoraleMgr: Read Party Morale

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-005, GWA3-006 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-OBS |

**Description:**
Find the memory offset for party morale (death penalty / morale boost). Froggy uses `GetMorale()` to decide when to use DP-removal sweets and when to resign. Without this, Gemma can't make DP recovery decisions.

**Acceptance Criteria:**
- [ ] Scan for morale value in game memory (int, range -60 to +10)
- [ ] Add `GetMorale()` to PartyMgr or a new MoraleMgr
- [ ] Expose morale in GameSnapshot Tier 1
- [ ] Test: verify morale reads correctly after deaths and morale boosts

**Reference:** AutoIt `GetMorale()` in `BotsHub-latest/lib/GWA2.au3`. GWCA `PartyMgr::GetMorale()`.

---

#### GWA3-056 — Quest State Reader: Active Quest + Objectives

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-005, GWA3-006 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-OBS |

**Description:**
Read the active quest state from game memory: which quest is active, its current objectives, completion state, and map markers. Froggy uses `AcceptQuest`, `QuestReward`, and checks quest log state to know when dungeon quests are complete. Gemma needs this to know when objectives are met.

**Acceptance Criteria:**
- [ ] Read active quest ID from memory
- [ ] Read quest log entries: quest ID, state (active/complete), map_from, map_to
- [ ] Read quest objective text (encoded, decode via EncStrCodec)
- [ ] Expose active quest + objectives in GameSnapshot Tier 2
- [ ] Test: accept a quest, verify state transitions

**Reference:** GWCA `QuestMgr` — `GetActiveQuest()`, `GetQuestLog()`, quest struct at `Quest.h`.

---

#### GWA3-057 — Vanquish Progress: Foes Killed / Foes To Kill

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-005, GWA3-006 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-OBS |

**Description:**
Read vanquish/area progress counters: foes killed and total foes in the explorable area. AutoIt exposes `GetFoesKilled()` and `GetFoesToKill()`. Useful for vanquish runs and knowing when an area is clear.

**Acceptance Criteria:**
- [ ] Find memory offsets for foes_killed and foes_to_kill counters
- [ ] Add to MapMgr or a dedicated VanquishMgr
- [ ] Expose in GameSnapshot Tier 1 (when in explorable area)
- [ ] Test: enter explorable, kill foes, verify counter increments

**Reference:** AutoIt `GetFoesKilled()`, `GetFoesToKill()`, `GetAreaVanquished()` in `GWA2.au3`.

---

#### GWA3-058 — Title Progression Snapshot

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-049 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-OBS |

**Description:**
Expose EotN title progression (Vanguard, Norn, Asura, Deldrimor) and core titles (Sunspear, Lightbringer, Survivor) in the game snapshot. PlayerMgr already has `GetTitleTrack()` — wire it into GameSnapshot Tier 3. Froggy uses these to validate run completion and optimize farming route selection.

**Acceptance Criteria:**
- [ ] Read EotN title points via `GetTitleTrack()` for IDs 28-31
- [ ] Read Sunspear (20), Lightbringer (21), Survivor (11)
- [ ] Include in GameSnapshot Tier 3 as `titles` object
- [ ] Include current rank name if decodable

---

#### GWA3-059 — Map Loading 3-State Detection

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-005 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-OBS |

**Description:**
Distinguish between 3 map loading states: `0` = not loaded (loading screen), `1` = loaded (in game), `2` = disconnected/error. Currently the bridge only exposes a boolean `is_loaded`. Froggy uses the 3-state value to detect disconnects and bail out early.

**Acceptance Criteria:**
- [ ] Find memory location for full map loading state (0/1/2)
- [ ] Expose as `map.loading_state` in GameSnapshot (alongside existing `is_loaded`)
- [ ] Test: verify state 0 during map transitions, state 2 on disconnect

**Reference:** AutoIt `GetMapLoading()` in `GWA2_Extensions.au3`. GWCA `MapMgr::GetInstanceType()`.

---

#### GWA3-060 — Agent Name Decoding

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-051, GWA3-005 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-OBS |

**Description:**
Decode encoded NPC and player names so the LLM sees "Mergoyle" instead of raw encoded bytes. Use `EncStrCodec` (GWA3-051) to decode `name_enc` strings from Agent and Player structs. Expose decoded names for nearby agents in the snapshot.

**Acceptance Criteria:**
- [ ] Decode `player->name_enc` for players
- [ ] Decode NPC names from the game's string table (model-id based)
- [ ] Include `name` field in nearby agent entries
- [ ] Handle edge cases: empty strings, unresolvable names

---

#### GWA3-061 — Dialog Body Decoding

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-051, GWA3-053 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-OBS |

**Description:**
The dialog body text from StoC packets is an encoded `wchar_t` string with embedded `<a=DIALOG_ID>label</a>` tags. Currently we pass the raw encoded string. Implement decoding using the game's `AsyncDecodeStr` function (as GWToolbox does) or our `EncStrCodec`, so Gemma sees readable dialog text instead of encoded gibberish.

**Acceptance Criteria:**
- [ ] Decode dialog body encoded strings to readable text
- [ ] Parse embedded `<a=ID>label</a>` tags to extract inline dialog options
- [ ] Decode dialog button label strings
- [ ] Expose decoded text in GameSnapshot `dialog.body` field

**Reference:** GWToolbox `DialogModule.cpp` — calls `GW::UI::AsyncDecodeStr()`.

---

#### GWA3-062 — Resign Action

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-010 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-ACTIONS |

**Description:**
Add a `resign` action to the LLM bridge. In Guild Wars, resigning is done via chat command `/resign`. Froggy uses `ResignAndReturn()` which sends `/resign` then waits for the resign vote to pass before returning to outpost.

**Acceptance Criteria:**
- [ ] Add `resign` action to ActionExecutor — sends `/resign` via ChatMgr
- [ ] Add tool schema in Python bridge
- [ ] Test: resign in an explorable area, verify return to outpost

---

#### GWA3-063 — Chest Interaction + Lockpick Support

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-010, GWA3-016 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-ACTIONS |

**Description:**
Froggy opens dungeon chests via `OpenChest()` which interacts with the nearest chest signpost. The LLM can already `interact_signpost(agent_id)`, but needs to know which agents are chests (vs other gadgets) and track which chests have been opened to avoid re-interaction. May need lockpick usage logic if chest requires a key.

**Acceptance Criteria:**
- [ ] Identify chest agents by gadget ID or model (differentiate from other signposts)
- [ ] Add `is_chest` flag to gadget agents in GameSnapshot
- [ ] Optionally: add `open_chest(agent_id)` action as a convenience wrapper
- [ ] Document lockpick item model IDs for the LLM

---

#### GWA3-064 — Craft Item Action

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-010, GWA3-053 |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-ACTIONS |

**Description:**
Implement crafter NPC transaction flow. Froggy's maintenance routine crafts grails and longswords via specific NPC dialogs + crafting packets. The LLM needs: detect crafter window, see available recipes, and execute craft transactions. May reuse existing merchant item list + dialog system, or need dedicated CraftMgr.

**Acceptance Criteria:**
- [ ] Detect crafter NPC window open (similar to merchant detection)
- [ ] Read available craft recipes (model IDs + material requirements)
- [ ] Add `craft_item(recipe_id)` or reuse `transact_items` for crafting
- [ ] Test: interact with crafter NPC, craft an item, verify inventory change

**Reference:** AutoIt `CraftItemSafe()` in `GWA2_Crafting.au3`. GWCA `Merchant::TransactionType::CrafterBuy (3)`.

---

#### GWA3-065 — StoC Event Push to Bridge

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-053, LLM IPC |
| **Blocks** | — |
| **Parallel Group** | PG-LLM-EVENTS |

**Description:**
Register StoC packet callbacks for discrete game events and push them to the bridge as `event` messages (instead of waiting for the next snapshot). Events to intercept: map change, party wipe, agent killed, item dropped, chest opened, chat message received, skill activated. This gives Gemma real-time event awareness between snapshot ticks.

**Acceptance Criteria:**
- [ ] Register StoC callbacks for key packet types (map load, agent death, item drop, chat)
- [ ] Serialize events as JSON `{"type": "event", "event": "...", ...}`
- [ ] Send via IpcServer::Send immediately (not batched)
- [ ] Add event display in Python observation summary
- [ ] Test: kill an enemy, verify event appears in bridge output

---

#### GWA3-066 — Advisory Mode: LLM Overrides FroggyHM

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-025, GWA3-026, LLM Bridge |
| **Blocks** | — |
| **Parallel Group** | — |

**Description:**
Implement advisory autonomy mode where the LLM handles high-level strategy (when to sell, which route, priority targeting) while FroggyHM's scripted routines handle combat micro and pathfinding. The LLM can override bot state transitions via the `BotState::LLMControlled` enum. When in advisory mode, Froggy's state machine runs normally but the LLM can inject state change commands ("go to merchant now", "return to outpost").

**Acceptance Criteria:**
- [ ] Add `set_bot_state(state)` action to ActionExecutor
- [ ] LLM can read current bot state from GameSnapshot
- [ ] LLM can override state transitions (e.g., force Merchant state)
- [ ] Froggy continues running its handlers for the current state
- [ ] "Hold" behavior when LLM is thinking (maintain current action)
- [ ] Test: LLM overrides from InDungeon to Merchant mid-run

---

### Epic 12: LLM Bridge Integration Tests

> End-to-end tests that validate the LLM bridge through the named pipe — the same path Gemma uses.
> Python test script connects to `\\.\pipe\gwa3_llm`, reads snapshots, sends actions, verifies results.
> Run via: `python -m bridge.tests` (requires gwa3 injected with `--llm`)

#### GWA3-067 — Bridge Test Infrastructure

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | — |
| **Blocks** | GWA3-068 through GWA3-087 |
| **Parallel Group** | — |

**Description:**
Create the Python test framework: runner with discovery/execution/timeouts/reporting, `BridgeTestCase` base class with pipe helpers (`wait_for_snapshot`, `send_action`, `wait_for_state_change`), and shared assertion utilities.

**Acceptance Criteria:**
- [ ] `python -m bridge.tests` runs and produces pass/fail summary
- [ ] Base class connects to pipe, receives at least one snapshot
- [ ] Per-test timeout (30s default) prevents hangs
- [ ] `--filter` flag runs subset of tests
- [ ] `send_action` correlates `action_result` by `request_id`
- [ ] `wait_for_snapshot(tier=N)` drains pipe until correct tier arrives

**Files:** `bridge/tests/__init__.py`, `__main__.py`, `runner.py`, `base.py`, `helpers.py`

---

#### GWA3-068 — IPC Protocol Tests (Category A)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-067 |
| **Blocks** | GWA3-069 through GWA3-087 |
| **Parallel Group** | — |

**Description:**
Validate the named pipe transport layer: connection, message framing, heartbeat, snapshot delivery, and rate limiting.

**Acceptance Criteria:**
- [ ] `test_pipe_connect` — connect succeeds
- [ ] `test_pipe_disconnect_reconnect` — reconnect works after disconnect
- [ ] `test_message_framing` — messages have 4-byte length prefix + valid JSON
- [ ] `test_heartbeat_reception` — heartbeat arrives within 10s
- [ ] `test_snapshot_reception` — snapshot arrives within 3s
- [ ] `test_rapid_send_rate_limiter` — 15 actions in <1s, at least one returns `rate_limited`

**File:** `bridge/tests/test_a_ipc.py`

---

#### GWA3-069 through GWA3-077 — Observation Tests (Categories B1-B12)

| ID | Section | Est | Tests |
|----|---------|-----|-------|
| GWA3-069 | B1: Player state (`me`) | S | agent_id > 0, pos non-zero, HP 0-1, professions 0-10, level 1-20, state booleans |
| GWA3-070 | B2: Skillbar | S | 8 slots, slot indices 0-7, constant data on non-zero skills, recharge ≥ 0 |
| GWA3-071 | B3: Map state | S | map_id > 0, is_loaded bool, instance_time ≥ 0, region integer |
| GWA3-072 | B4: Party state | S | size ≥ 1, members array, self present, is_defeated bool, dead_count |
| GWA3-073 | B5: Nearby agents | M | Array, core fields, living fields (hp/allegiance/casting/hex/enchant), item fields (model_id), gadget type |
| GWA3-074 | B6: Hero skillbars | S | Array, each has agent_id + 8-slot skillbar + casting state |
| GWA3-075 | B7-B9: Dialog, merchant, chat | M | Dialog closed state, merchant closed state, chat array with channel/sender/message |
| GWA3-076 | B10-B11: Inventory & storage | M | Gold ≥ 0, bags 1-4, items with rarity, free_slots_total, storage panes |
| GWA3-077 | B12: Effects & tier progression | S | Effects with skill_id/time_remaining/type. Tier 1→2→3 progressive field addition. All snapshots have type/tier/tick. |

All depend on GWA3-068. All read-only. All in **PG-BRIDGE-OBS** parallel group.

**File:** `bridge/tests/test_b_observations.py`

---

#### GWA3-078 through GWA3-086 — Action Tests (Categories C1-C12)

| ID | Actions | Est | Deps | Tests |
|----|---------|-----|------|-------|
| GWA3-078 | C1-2: move_to, change_target, cancel_action | M | 068, 069 | Valid/invalid params, position/target state changes |
| GWA3-079 | C3: attack, use_skill, use_hero_skill, call_target | M | 068, 069 | Valid/invalid, skill_on_recharge, combat state (requires explorable) |
| GWA3-080 | C4: add/kick hero, set_hero_behavior, flag/unflag | M | 068, 072 | Party size changes, invalid_behavior error, restore party after |
| GWA3-081 | C5: travel, set_hard_mode, return_to_outpost, enter_mission, skip_cinematic | L | 068, 071 | Map ID changes (15s each), invalid_map_id, restore location after |
| GWA3-082 | C6: pick_up_item, use/equip/drop/move_item, drop_gold | M | 068, 076 | Missing params, item_not_found, agent_not_found |
| GWA3-083 | C7: salvage_start, salvage_materials, salvage_done, identify_item | M | 068, 076 | Missing params, item/kit_not_found |
| GWA3-084 | C8: buy_materials, request_quote, transact_items | M | 068, 075 | Missing params, quote state in merchant snapshot |
| GWA3-085 | C9: interact_npc, dialog | M | 068, 073 | NPC interaction → dialog opens, buttons appear, dialog send |
| GWA3-086 | C10-12: load_skillbar, send_chat, wait, drop_gold | M | 068, 070 | Wrong array size, empty_message, skillbar/chat state changes |

All in **PG-BRIDGE-ACT** parallel group.

**File:** `bridge/tests/test_c_actions.py`

---

#### GWA3-087 — Validation & Error Handling (Category D)

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-068 |
| **Blocks** | — |
| **Parallel Group** | PG-BRIDGE-ACT |

**Description:**
Test the ActionExecutor's validation and error paths: rate limiting, unknown actions, empty names, missing/malformed params.

**Acceptance Criteria:**
- [ ] `test_rate_limiter` — 15 rapid actions → at least one `rate_limited` error
- [ ] `test_unknown_action` — `nonexistent_action` → `unknown_action` error
- [ ] `test_empty_action_name` — empty name → `empty_action_name` error
- [ ] `test_agent_not_found` — `change_target(99999999)` → `agent_not_found`
- [ ] `test_skill_on_recharge` — use skill, immediately re-use → `skill_on_recharge`
- [ ] `test_invalid_slot` — `use_skill(slot=8)` → `invalid_slot`
- [ ] `test_coordinates_out_of_range` — `move_to(999999, 999999)` → `coordinates_out_of_range`

**File:** `bridge/tests/test_d_validation.py`

---

### Epic 14: C++ Froggy Feature Parity

> Port the remaining AutoIt Froggy capabilities to C++.
> The C++ FroggyHM currently does movement + auto-attack + basic merchant selling.
> These tickets add the missing combat, loot, maintenance, and recovery systems.

#### GWA3-097 — Skill Casting During Combat

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | XL |
| **Depends On** | GWA3-017, GWA3-026 |
| **Blocks** | GWA3-033 |
| **Parallel Group** | — |

**Description:**
Replace the auto-attack-only combat in `AggroMoveToEx()` with intelligent skill usage. AutoIt's `Fight()` loop casts skills based on type (hex, pressure, heal, enchant removal, interrupt). The C++ version needs a skill rotation system that reads the skillbar, categorizes skills by type using `SkillMgr::GetSkillConstantData()`, and casts them appropriately.

**Acceptance Criteria:**
- [ ] `CacheSkillBar()` equivalent: analyze 8 skills on login, categorize by type
- [ ] During combat: cast offensive skills (hexes, damage) on target
- [ ] Cast defensive skills (heals, enchants) on self/allies when HP low
- [ ] Respect recharge timers — skip skills still cooling down
- [ ] Respect energy — don't cast if insufficient energy
- [ ] Call target before engaging priority enemies (monks/healers)
- [ ] Use interrupt skills when enemy is casting a dangerous spell
- [ ] Test: complete a Bogroot run using skills, not just auto-attack

**Reference:** AutoIt `BotCore-Combat.au3` — `Fight()`, `UseSkills()`, `UseSkillSmart()`, `CanUse()`, `CanCast()`

---

#### GWA3-098 — Loot Pickup Policy

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-018, GWA3-026 |
| **Blocks** | GWA3-033 |
| **Parallel Group** | PG-FROGGY |

**Description:**
Implement item pickup during dungeon runs. AutoIt's `CanPickUpEx()` uses model-ID whitelists (quest items, tomes, consumables, DP removal) and rarity rules (always pick gold/green). Also needs chest detection + opening via `interact_signpost` on chest gadgets.

**Acceptance Criteria:**
- [ ] Pick up gold-rarity and green-rarity items automatically after fights
- [ ] Pick up items on the always-pickup list (quest items, tomes, ectos)
- [ ] Detect and open dungeon chests (use `is_chest` flag from gadget agents)
- [ ] Don't pick up items owned by other players (check `owner` field)
- [ ] Don't pick up if inventory is full (check `CountFreeSlots()`)
- [ ] Integrate into `FollowWaypoints()` — pick up between fights, not during

**Reference:** AutoIt `BotCore-Loot.au3` — `PickupLootEx()`, `CanPickUpEx()`, `CheckForChest()`

---

#### GWA3-099 — Item Identification

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-018, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Before selling gold items, identify them with an ID kit. Unidentified items sell for base value; identified ones sell for full value. Need to find ID kits in inventory, loop through unidentified gold items, and call `ItemMgr::IdentifyItem()`.

**Acceptance Criteria:**
- [ ] Find identification kit in inventory by model ID
- [ ] Iterate inventory for unidentified gold-rarity items
- [ ] `ItemMgr::IdentifyItem(itemId, kitId)` for each
- [ ] Wait between identifications (game rate limit)
- [ ] Skip if no ID kits available
- [ ] Integrate into `HandleMerchant()` before selling

---

#### GWA3-100 — Salvage System

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-018, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Salvage white/blue items for materials instead of vendoring. Uses salvage kit + item → materials. AutoIt does `SalvageSessionOpen()` → `SalvageMaterials()` → `SalvageSessionDone()` per item.

**Acceptance Criteria:**
- [ ] Find salvage kit in inventory
- [ ] Iterate salvageable items (white/blue non-quest, non-kit items)
- [ ] Open salvage session, extract materials, close session
- [ ] Wait between salvage operations
- [ ] Integrate into `HandleMerchant()` or `HandleMaintenance()`

---

#### GWA3-101 — Hero Skillbar Loading

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-017, GWA3-022, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Load hero skill templates from `hero_configs/` text files instead of relying on whatever the heroes already have equipped. AutoIt reads skill IDs + attribute distributions from files and calls `LoadSkillbar()` per hero.

**Acceptance Criteria:**
- [ ] Parse hero config file format (hero_id, skill_ids[8], attributes)
- [ ] `SkillMgr::LoadSkillbar(skillIds, heroIndex)` for each hero
- [ ] Load during `HandleTownSetup()` after adding heroes
- [ ] Support multiple config files (Standard.txt, Mercs.txt)
- [ ] Config file path from `BotConfig::hero_config_file`

**Reference:** AutoIt `BotCore-HeroSetup.au3` — `LoadHeroConfigFromFile()`

---

#### GWA3-102 — Consumable Usage

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-018, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Use consumables before dungeon runs: consets (Grail of Might, Essence of Celerity, Armor of Salvation), EotN blessings, and stones. Check if already buffed via `EffectMgr::HasEffect()`, find consumable in inventory, use it.

**Acceptance Criteria:**
- [ ] Check conset buff status before each run
- [ ] Find conset items in inventory by model ID
- [ ] `ItemMgr::UseItem()` for each missing conset
- [ ] Check blessing status, use blessing scrolls if needed
- [ ] Configurable via `BotConfig::use_consets` / `use_stones`
- [ ] Integrate into `HandleTownSetup()` or `HandleMaintenance()`

---

#### GWA3-103 — Kit & Material Purchasing

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-024, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Buy identification kits and salvage kits from merchant when running low. AutoIt's `BuyKitsUntilTarget()` buys kits up to a target count. Needs merchant interaction → purchase via `BuyMaterials()` or `TransactItems()`.

**Acceptance Criteria:**
- [ ] Count current ID kits and salvage kits in inventory
- [ ] If below threshold, interact with merchant NPC
- [ ] Buy kits up to target count (e.g., 3 ID kits, 3 salvage kits)
- [ ] Integrate into `HandleMaintenance()`

---

#### GWA3-104 — Conset Crafting

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-024, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Craft consets at Embark Beach (or Gadd's) crafter NPCs. Each conset requires specific materials + gold. AutoIt travels to specific NPCs (Eyja, Kwat, Alcus), opens crafter window, and crafts.

**Acceptance Criteria:**
- [ ] Check conset count in inventory
- [ ] If below threshold, travel to crafter NPC location
- [ ] Interact with crafter, open crafting dialog
- [ ] `TransactItems(type=3, quantity, itemId)` to craft
- [ ] Buy missing materials from material trader first if needed
- [ ] Integrate into `HandleMaintenance()` when consets are low

---

#### GWA3-105 — Xunlai Chest Operations

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-018, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Interact with Xunlai chest to deposit/withdraw items and gold. Already have storage bag reading (bags 8-16), but need the NPC interaction to open the storage panel and the item move operations.

**Acceptance Criteria:**
- [ ] Find Xunlai chest NPC and interact
- [ ] Deposit specific items to storage (ectos, valuable materials)
- [ ] Withdraw items from storage (craft materials for consets)
- [ ] `ItemMgr::MoveItem()` between backpack and storage bags
- [ ] Integrate into `HandleMaintenance()`

---

#### GWA3-106 — Wipe Recovery

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
When party wipes, calculate the best restart waypoint based on current position and map, use DP removal items, and restart the run from the nearest safe point instead of always returning to outpost.

**Acceptance Criteria:**
- [ ] Detect wipe (party defeated or player dead)
- [ ] Calculate nearest resurrection shrine / restart waypoint
- [ ] Use DP removal sweets when morale < -40
- [ ] Restart dungeon run from calculated waypoint
- [ ] Fall back to return_to_outpost if recovery impossible

---

#### GWA3-107 — Hero Flagging

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-022, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Flag heroes to specific positions during combat encounters for optimal positioning. AutoIt flags heroes at key dungeon locations.

**Acceptance Criteria:**
- [ ] Flag heroes at waypoint-specific positions during dungeon runs
- [ ] Unflag after combat is resolved
- [ ] Use `PartyMgr::FlagHero()` / `FlagAll()` / `UnflagAll()`

---

#### GWA3-108 — Quest Dialog Retry

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | S |
| **Depends On** | GWA3-023, GWA3-026 |
| **Blocks** | — |
| **Parallel Group** | PG-FROGGY |

**Description:**
Quest accept and reward with retry logic. AutoIt retries dialog interactions if they fail (NPC not responding, dialog not opening). Current C++ just sends one dialog call without verification.

**Acceptance Criteria:**
- [ ] Send dialog, verify quest state changed
- [ ] Retry up to 3 times with 1s delay between attempts
- [ ] Handle quest reward claiming with verification
- [ ] Integrate into `HandleDungeon()` quest acceptance

---

## Ticket Summary

| ID | Title | Est | Depends On | Status |
|----|-------|-----|-----------|--------|
| **Epic 1: Foundation** | | | | |
| GWA3-001 | CMake Project + DLL Skeleton | M | — | `done` |
| GWA3-002 | Standalone DLL Injector | M | 001 | `done` |
| GWA3-003 | Pattern Scanner Engine | L | 001 | `done` |
| GWA3-004 | Packet Header Constants | S | 001 | `done` |
| GWA3-005 | Offset Registry + Pattern Defs | L | 001, 003 | `done` |

> **Note on GWA3-005:** Must include the GameThread scan target `FindAssertion("FrApi.cpp", "renderElapsed >= 0")` documented in `GWCA_GameThreadTarget_Addendum.md` and `GWCA_GameThreadModuleSlots_Addendum.md`. Also reference `GWCA_BuildConsistency_And_ExportSeam_Addendum.md` for RVA validation methodology.
| **Epic 2: Commands** | | | | |
| GWA3-006 | Game Thread Hook + Queue | L | 002, 003, 005 | `done` |
| GWA3-010 | Packet Sending (CtoS) | M | 004, 005, 006 | `done` |
| **Epic 3: Structs** | | | | |
| GWA3-007 | Agent Struct | L | 001, 003, 005 | `done` |
| GWA3-008 | Skill Struct | M | 001, 003, 005 | `done` |
| GWA3-009 | Item & Bag Struct | M | 001, 003, 005 | `done` |
| GWA3-011 | Map & Instance Struct | S | 001, 003, 005 | `done` |
| GWA3-012 | Party & Hero Struct | M | 001, 003, 005 | `done` |
| GWA3-013 | Quest Struct | S | 001, 003, 005 | `done` |
| GWA3-014 | Effect & Buff Struct | M | 001, 003, 005 | `done` |
| GWA3-015 | Chat Struct + Constants | S | 001, 003, 005 | `done` |
| **Epic 4: Managers** | | | | |
| GWA3-016 | AgentMgr | L | 006, 007, 010, 014 | `done` |
| GWA3-017 | SkillMgr | M | 006, 008, 010 | `done` |
| GWA3-018 | ItemMgr | L | 006, 009, 010 | `done` |
| GWA3-019 | MapMgr | M | 006, 010, 011 | `done` |
| GWA3-020 | UIMgr: Frame System | L | 003, 005, 006 | `done` |
| GWA3-021 | UIMgr: ButtonClick | M | 020 | `done` |
| GWA3-022 | PartyMgr | M | 006, 010, 012 | `done` |
| GWA3-023 | QuestMgr + DialogMgr | S | 006, 010, 013 | `done` |
| GWA3-024 | ChatMgr + RenderMgr + TradeMgr | L | 005, 006, 010, 015 | `done` |
| **Epic 5: C++ Bot Module** | | | | |
| GWA3-025 | Bot Framework + State Machine | L | 006, 016-024, 049, 051 | `done` |
| GWA3-026 | Froggy HM Bot Module (C++ Port) | XL | 025 | `done` |
| GWA3-027 | IPC Server (Optional) | L | 025 | `done` |
| **Epic 6: Integration** | | | | |
| GWA3-028 | Integration: Char Select + Login | M | 021, 026, **047** | `backlog` |
| GWA3-029 | Integration: Hero Setup + Consumables | M | 026, **046** | `backlog` |
| GWA3-030 | Integration: Travel + Movement + Combat | L | 026, **046** | `backlog` |
| GWA3-031 | Integration: Loot + Inventory + Salvage | M | 026, **046** | `backlog` |
| GWA3-032 | Integration: Merchant + Crafting | M | 026, **046** | `backlog` |
| **Epic 7: Hardening** | | | | |
| GWA3-033 | Full Froggy HM 10+ Runs | XL | 028-032 | `backlog` |
| GWA3-034 | Multi-Client Injector | M | 002 | `done` |
| GWA3-035 | Pattern Health Check Tool | M | 005 | `done` |
| GWA3-036 | Crash Protection + SEH | M | 033 | `backlog` |
| **Epic 8: Research-Derived** | | | | |
| GWA3-037 | Hook Engine (Optional Custom) | XL | 006 | `backlog` |
| GWA3-038 | Memory Patcher Module | M | 005, 006 | `done` |
| GWA3-039 | Callback Registry + Module Ownership | L | 006, 020 | `done` |
| GWA3-040 | Key Input + Preference System | M | 006, 020, 039 | `backlog` |
| GWA3-041 | Py4GW API Surface Audit | S | 025 | `backlog` |
| **Epic 9: Testing** | | | | |
| GWA3-042 | Test Harness + CMake Target | M | 001 | `done` |
| GWA3-043 | Offline: Struct Offset Validation | M | 042, 007-015, 020 | `done` |
| GWA3-044 | Offline: Headers + Pattern Parsing | S | 042, 003, 004 | `done` |
| GWA3-045 | Injection Smoke: Patterns + State Read | L | 002, 005, 043 | `done` |
| GWA3-046 | Behavioral: Commands + Game Thread | XL | 006, 010, 045, 049-051 | `done` |
| GWA3-047 | Behavioral: Frame UI + ButtonClick | M | 020, 021, 046 | `backlog` |
| GWA3-048 | Bot Framework Smoke Test | M | 025 | `done` |
| **Epic 10: GWCA Header Parity** | | | | |
| GWA3-049 | PlayerMgr (Titles + Profession) | M | 005, 006, 010 | `done` |
| GWA3-050 | MemoryMgr (Version + Timer + Window) | S | 005 | `done` |
| GWA3-051 | String Encoding/Decoding | M | 005, 006 | `done` |
| GWA3-052 | CameraMgr | S | 005, 006 | `done` |
| GWA3-053 | StoCMgr (Packet Callbacks) | L | 005, 006 | `done` |
| GWA3-054 | GuildMgr | S | 005, 006 | `done` |

| **Epic 11: LLM Bridge Gaps** | | | | |
| GWA3-055 | MoraleMgr — Read Party Morale | M | 005, 006 | `done` |
| GWA3-056 | Quest State Reader — Active Quest + Objectives | L | 005, 006 | `done` |
| GWA3-057 | Vanquish Progress — Foes Killed / Foes To Kill | M | 005, 006 | `done` |
| GWA3-058 | Title Progression Snapshot — EotN + Core Titles | S | 049 | `done` |
| GWA3-059 | Map Loading 3-State — Not Loaded / Loaded / Disconnected | S | 005 | `done` |
| GWA3-060 | Agent Name Decoding — Decode Encoded NPC/Player Names | M | 051, 005 | `done` |
| GWA3-061 | Dialog Body Decoding — AsyncDecodeStr for Dialog Text | M | 051, 053 | `done` |
| GWA3-062 | Resign Action — Resign + Return to Outpost Combo | S | 010 | `done` |
| GWA3-063 | Chest Interaction — Open Chest + Lockpick Support | S | 010, 016 | `done` |
| GWA3-064 | Craft Item Action — Crafter NPC Transaction Flow | M | 010, 053 | `done` |
| GWA3-065 | StoC Event Push — Map Change / Kill / Loot / Chat Events to Bridge | L | 053, LLM IPC | `done` |
| GWA3-066 | Advisory Mode — LLM Overrides FroggyHM State Machine | L | 025, 026, LLM Bridge | `done` |

| **Epic 12: LLM Bridge Integration Tests** | | | | |
| GWA3-067 | Bridge Test Infrastructure (runner, base, helpers) | L | — | `done` |
| GWA3-068 | IPC Protocol Tests (Category A) | M | 067 | `done` |
| GWA3-069 | Observation: Player State (B1) | S | 068 | `done` |
| GWA3-070 | Observation: Skillbar (B2) | S | 068 | `done` |
| GWA3-071 | Observation: Map State (B3) | S | 068 | `done` |
| GWA3-072 | Observation: Party State (B4) | S | 068 | `done` |
| GWA3-073 | Observation: Nearby Agents (B5) | M | 068 | `done` |
| GWA3-074 | Observation: Hero Skillbars (B6) | S | 068 | `done` |
| GWA3-075 | Observation: Dialog, Merchant, Chat (B7-B9) | M | 068 | `done` |
| GWA3-076 | Observation: Inventory & Storage (B10-B11) | M | 068 | `done` |
| GWA3-077 | Observation: Effects & Tier Progression (B12) | S | 068 | `done` |
| GWA3-078 | Action: Movement & Targeting (C1-C2) | M | 068, 069 | `done` |
| GWA3-079 | Action: Combat (C3) | M | 068, 069 | `done` |
| GWA3-080 | Action: Party & Hero (C4) | M | 068, 072 | `done` |
| GWA3-081 | Action: Travel (C5) | L | 068, 071 | `done` |
| GWA3-082 | Action: Items (C6) | M | 068, 076 | `done` |
| GWA3-083 | Action: Salvage & Identify (C7) | M | 068, 076 | `done` |
| GWA3-084 | Action: Trade (C8) | M | 068, 075 | `done` |
| GWA3-085 | Action: Dialog & NPC Interaction (C9) | M | 068, 073 | `done` |
| GWA3-086 | Action: Skillbar, Chat, Utility (C10-C12) | M | 068, 070 | `done` |
| GWA3-087 | Validation & Error Handling (Category D) | M | 068 | `done` |

| **Epic 13: Epic 11 Feature Tests** | | | | |
| GWA3-088 | Test: Morale, Vanquish, Map Loading 3-State (B-obs) | S | 068 | `done` |
| GWA3-089 | Test: Quest State, Titles (B-obs) | S | 068 | `done` |
| GWA3-090 | Test: Dialog Decoding, Chat Log Channels (B-obs) | M | 068 | `done` |
| GWA3-091 | Test: Chest Identification, Agent Names (B-obs) | S | 068 | `done` |
| GWA3-092 | Test: Merchant Items, Craft Item Action (C-act) | M | 068, 075 | `done` |
| GWA3-093 | Test: Resign, Set Bot State Actions (C-act) | S | 068 | `done` |
| GWA3-094 | Test: StoC Event Push (event messages on pipe) | M | 068 | `done` |
| GWA3-095 | Test: Advisory Mode (Froggy + LLM coexistence) | L | 068 | `done` |
| GWA3-096 | Test: Bot State in Snapshot + State Override (C-act) | M | 068 | `done` |

| **Epic 14: C++ Froggy Feature Parity** | | | | |
| GWA3-097 | Skill Casting During Combat (UseSkills / Fight loop) | XL | 017, 026 | `done` |
| GWA3-098 | Loot Pickup Policy (CanPickUp, chest opening, quest items) | L | 018, 026 | `done` |
| GWA3-099 | Item Identification (ID kit usage before selling) | M | 018, 026 | `done` |
| GWA3-100 | Salvage System (salvage kits, trophy/material salvage) | M | 018, 026 | `done` |
| GWA3-101 | Hero Skillbar Loading (load from hero_configs/ files) | M | 017, 022, 026 | `done` |
| GWA3-102 | Consumable Usage (consets, blessings, stones before runs) | M | 018, 026 | `backlog` |
| GWA3-103 | Kit & Material Purchasing (buy ID kits, salvage kits) | M | 024, 026 | `backlog` |
| GWA3-104 | Conset Crafting (Grail, Essence, Armor at Embark Beach) | L | 024, 026 | `backlog` |
| GWA3-105 | Xunlai Chest Operations (deposit/withdraw items + gold) | M | 018, 026 | `backlog` |
| GWA3-106 | Wipe Recovery (smart restart waypoints, morale-based DP) | M | 026 | `backlog` |
| GWA3-107 | Hero Flagging (position heroes at specific combat locations) | S | 022, 026 | `backlog` |
| GWA3-108 | Quest Dialog Retry (accept/reward with validation + retry) | S | 023, 026 | `backlog` |

**Total: 108 tickets across 14 epics.**

---

## Maximum Parallelism Schedule

| Wave | Tickets Running | Agent Slots Needed |
|------|----------------|-------------------|
| 1 | GWA3-001 | 1 |
| 2 | GWA3-002, GWA3-003, GWA3-004, **GWA3-042** (test harness) | 4 |
| 3 | GWA3-005, **GWA3-044** (offline: headers + patterns) | 2 |
| 4 | GWA3-006, GWA3-007..015, GWA3-020, **GWA3-043** (offline: struct offsets — built incrementally) | **10+** |
| 5 | GWA3-010, 016..019, 021..024, **GWA3-045** (injection smoke) | **10+** |
| 6 | GWA3-025 (bot framework), **GWA3-046** (behavioral: commands), **GWA3-048** (bot smoke) | 3 |
| 7 | GWA3-026 (Froggy C++), **GWA3-047** (behavioral: frames) | 2 |
| 9 | GWA3-028..032 (integration) | **5** |
| 10 | GWA3-033 (full Froggy) | 1 |
| 11 | GWA3-034..041 (hardening + research-derived) | **8** |

**Peak parallelism: 10+ agents** (Waves 4-5, struct definitions + managers + test writing).
**Test tickets run alongside implementation tickets — not after. Each struct/manager agent writes its tests as part of the implementation.**
