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
  (Scanner)   (Offsets)     (FrameUI)    (ButtonClick)              (Bridge.h)     │
                                                                       │           │
                                                                       ▼           │
                                                                    GWA3-026      │
                                                                    (GWA3.au3)    │
                                                                       │           │
                                                                       ▼           │
                                                                    GWA3-027      │
                                                                    (Compat shim) │
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
Implement `core/GameThread.h` + `core/GameThread.cpp` — hook the game's main engine loop and process a thread-safe command queue each frame.

Uses MinHook to detour the `Engine` scan pattern (same function GWA2_Assembly.au3 hooks). The detour calls the original function, then drains a `std::queue<std::function<void()>>` protected by a mutex/spinlock.

**Acceptance Criteria:**
- [ ] `GameThread::Initialize()` — installs detour on Engine function via MinHook
- [ ] `GameThread::Shutdown()` — removes hook cleanly
- [ ] `GameThread::Enqueue(std::function<void()>)` — thread-safe push
- [ ] `GameThread::IsOnGameThread()` — returns true when called from hooked context
- [ ] Detour drains queue each frame (up to N commands per frame to prevent stalls)
- [ ] No deadlocks — mutex is only held briefly during push/pop
- [ ] Test: enqueue a lambda that logs "hello from game thread" — verify it fires
- [ ] Game runs stable for 10+ minutes with hook installed and idle queue

**Reference:** `GWA Censured/lib/botshub/GWA2_Assembly.au3` MainProc detour logic

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
- [ ] Validation: cast enchantment on self, confirm HasEffect returns true and duration is correct

**Reference:** `GWA2_Assembly.au3` `$BUFF_STRUCT_TEMPLATE`, `$EFFECT_STRUCT_TEMPLATE`

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
- [ ] Test: move item between bags, use consumable, salvage an item

**Reference:** `GWA2.au3` item functions, `Utils.au3` salvage/merchant functions, `GWA2_Assembly.au3` CommandEquipItem/CommandSalvage

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
- [ ] Test: travel from one outpost to another, set hard mode

**Reference:** `GWA2.au3` MoveMap/WaitMapLoading/EnterChallenge, `GWA2_Assembly.au3` LoadFinished hook

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

This is the **core RE research ticket** — everything from the 8 GWCA research docs feeds in here.

**Acceptance Criteria:**
- [ ] `Frame` struct with offsets: +0xA8 callbacks, +0xB8 child_offset_id, +0xBC frame_id, +0x128 relation, +0x134 frame_hash_id, +0x18C frame_state
- [ ] `Frame::IsCreated()`, `IsHidden()`, `IsDisabled()`, `IsClickable()`, `GetParent()`
- [ ] `GetFrameByHash(hash)` — walk frame array comparing +0x134
- [ ] `GetRootFrame()` — from scanned RootFrame offset
- [ ] `IsFrameVisible(hash)` — find frame + check state bits
- [ ] `SendFrameUIMessage(frame, msgid, wparam, lparam)` — build ECX = parent+0xA8, call game function
- [ ] `SendUIMessage(msgid, wparam, lparam)` — call global UIMessage dispatcher
- [ ] Known frame hash constants in `UIMgr::Hashes` namespace
- [ ] Test: `GetFrameByHash(PlayButton)` returns valid frame pointer at character select
- [ ] Test: `IsFrameVisible(ReconnectYes)` returns correct visibility state

**Reference:** `GWCA_ButtonClick_Research.md`, `GWCA_UIMessage_Research.md`, `GWA2_FrameUI.au3`

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

**Reference:** `GWCA_ButtonClick_Research.md` (complete chain), `GWA2_FrameUI.au3` `ClickButtonByHash()`

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
- [ ] Test: add 3 heroes, flag them to a position, change behavior

**Reference:** `GWA2.au3` hero/party functions, packets 0x15-0x1F, 0x9C-0xAF

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

### Epic 5: AutoIt Bridge

#### GWA3-025 — AutoIt Bridge DLL Exports

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | L |
| **Depends On** | GWA3-006, GWA3-016, GWA3-017, GWA3-018, GWA3-019, GWA3-020, GWA3-021, GWA3-022, GWA3-023, GWA3-024 |
| **Blocks** | GWA3-026 |
| **Parallel Group** | — |

**Description:**
Implement `exports/AutoItBridge.h` + `exports/AutoItBridge.cpp` — flat `extern "C" __declspec(dllexport)` functions for every manager API.

~120 exports covering agents, movement, skills, items, map, party, quests, trading, frame UI, chat, rendering.

**Acceptance Criteria:**
- [ ] All exports use `__cdecl` calling convention (AutoIt default)
- [ ] No C++ types in signatures — only `int`, `float`, `unsigned int`, `const wchar_t*`, `void`
- [ ] Every export wraps a manager call, handling game-thread enqueue internally
- [ ] `GWA3_Initialize()` — runs scanner + hooks, returns 0 on success
- [ ] `GWA3_Shutdown()` — cleans up all hooks
- [ ] `GWA3_GetScanStatus()` — returns count of failed patterns
- [ ] `.def` file or `__declspec(dllexport)` decorates all exports
- [ ] Verify with `dumpbin /exports gwa3.dll` — all ~120 functions listed
- [ ] Test from external process: `LoadLibrary("gwa3.dll")` + `GetProcAddress("GWA3_GetMyID")` returns valid pointer

**Reference:** `GWCA_CPP_Replacement_Plan.md` Phase 5 export list

---

#### GWA3-026 — GWA3.au3 Wrapper UDF

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-025 |
| **Blocks** | GWA3-027 |
| **Parallel Group** | — |

**Description:**
Create `GWA3.au3` — thin AutoIt UDF that wraps every DLL export in a native AutoIt function via `DllCall`.

**Acceptance Criteria:**
- [ ] `GWA3_Init()` / `GWA3_Shutdown()` — open/close DLL handle
- [ ] One wrapper function per DLL export (~120 functions)
- [ ] Consistent error handling — check `@error` after each `DllCall`
- [ ] Return types match AutoIt conventions (numbers, not pointers)
- [ ] Test from standalone `.au3` script: init, get agent ID, move, shutdown

**Reference:** `GWCA_CPP_Replacement_Plan.md` Phase 5B AutoIt wrapper example

---

#### GWA3-027 — GWA3_Compat.au3 Drop-in Shim

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | XL |
| **Depends On** | GWA3-026 |
| **Blocks** | GWA3-028, GWA3-029, GWA3-030, GWA3-031, GWA3-032 |
| **Parallel Group** | — |

**Description:**
Create `GWA3_Compat.au3` that maps every old GWA2 function name to the new GWA3 wrapper. This allows `GWA_Logic_Censured_NEW.au3` and `Froggy_HM_v1.6.au3` to work by just changing the `#include` line.

**Acceptance Criteria:**
- [ ] Maps all ~200 GWA2/Utils functions used by Froggy_HM and GWA_Logic to GWA3 equivalents
- [ ] Handles signature differences (e.g., old functions that take struct returns vs new flat returns)
- [ ] `GetMyAgent()` returns a compat struct or table that old code can index into
- [ ] `GetData()` / `SetData()` compatibility layer for struct field access
- [ ] `MemoryRead()` / `MemoryWrite()` fallback for any raw memory ops not covered by exports
- [ ] `Enqueue()` / `SendPacket()` compatibility (maps to `GWA3_SendPacket`)
- [ ] Compiles without errors when included in place of GWA2.au3

**Reference:** Full API surface analysis from agent research, `GWA Censured/lib/custom/GWA2_Compat.au3` (existing compat patterns)

---

### Epic 6: Integration Testing

> Each integration ticket tests a specific bot flow segment.
> **GWA3-028..032 can run in parallel** once GWA3-027 is done.

#### GWA3-028 — Integration: Character Select + Login

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
| **Estimate** | M |
| **Depends On** | GWA3-021, GWA3-027 |
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
| **Depends On** | GWA3-027 |
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
| **Depends On** | GWA3-027 |
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
| **Depends On** | GWA3-027 |
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
| **Depends On** | GWA3-027 |
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

---

#### GWA3-034 — Multi-Client Injector

| Field | Value |
|-------|-------|
| **Assignee** | |
| **Status** | `backlog` |
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

WAVE 5 (Bridge — needs all managers):
  ┌─────────────┐
  │ GWA3-025    │
  │ DLL Exports │
  └──────┬──────┘
         │
  ┌──────┴──────┐
  │ GWA3-026    │
  │ GWA3.au3    │
  └──────┬──────┘
         │
  ┌──────┴──────┐
  │ GWA3-027    │
  │ Compat Shim │
  └──────┬──────┘

WAVE 6 (Integration — all parallel, need compat shim):
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

## Ticket Summary

| ID | Title | Est | Depends On | Status |
|----|-------|-----|-----------|--------|
| **Epic 1: Foundation** | | | | |
| GWA3-001 | CMake Project + DLL Skeleton | M | — | `ready` |
| GWA3-002 | Standalone DLL Injector | M | 001 | `ready` |
| GWA3-003 | Pattern Scanner Engine | L | 001 | `ready` |
| GWA3-004 | Packet Header Constants | S | 001 | `ready` |
| GWA3-005 | Offset Registry + Pattern Defs | L | 001, 003 | `backlog` |
| **Epic 2: Commands** | | | | |
| GWA3-006 | Game Thread Hook + Queue | L | 002, 003, 005 | `backlog` |
| GWA3-010 | Packet Sending (CtoS) | M | 004, 005, 006 | `backlog` |
| **Epic 3: Structs** | | | | |
| GWA3-007 | Agent Struct | L | 001, 003, 005 | `backlog` |
| GWA3-008 | Skill Struct | M | 001, 003, 005 | `backlog` |
| GWA3-009 | Item & Bag Struct | M | 001, 003, 005 | `backlog` |
| GWA3-011 | Map & Instance Struct | S | 001, 003, 005 | `backlog` |
| GWA3-012 | Party & Hero Struct | M | 001, 003, 005 | `backlog` |
| GWA3-013 | Quest Struct | S | 001, 003, 005 | `backlog` |
| GWA3-014 | Effect & Buff Struct | M | 001, 003, 005 | `backlog` |
| GWA3-015 | Chat Struct + Constants | S | 001, 003, 005 | `backlog` |
| **Epic 4: Managers** | | | | |
| GWA3-016 | AgentMgr | L | 006, 007, 010, 014 | `backlog` |
| GWA3-017 | SkillMgr | M | 006, 008, 010 | `backlog` |
| GWA3-018 | ItemMgr | L | 006, 009, 010 | `backlog` |
| GWA3-019 | MapMgr | M | 006, 010, 011 | `backlog` |
| GWA3-020 | UIMgr: Frame System | L | 003, 005, 006 | `backlog` |
| GWA3-021 | UIMgr: ButtonClick | M | 020 | `backlog` |
| GWA3-022 | PartyMgr | M | 006, 010, 012 | `backlog` |
| GWA3-023 | QuestMgr + DialogMgr | S | 006, 010, 013 | `backlog` |
| GWA3-024 | ChatMgr + RenderMgr + TradeMgr | L | 005, 006, 010, 015 | `backlog` |
| **Epic 5: Bridge** | | | | |
| GWA3-025 | AutoIt Bridge DLL Exports | L | 006, 016-024 | `backlog` |
| GWA3-026 | GWA3.au3 Wrapper UDF | M | 025 | `backlog` |
| GWA3-027 | GWA3_Compat.au3 Drop-in Shim | XL | 026 | `backlog` |
| **Epic 6: Integration** | | | | |
| GWA3-028 | Integration: Char Select + Login | M | 021, 027 | `backlog` |
| GWA3-029 | Integration: Hero Setup + Consumables | M | 027 | `backlog` |
| GWA3-030 | Integration: Travel + Movement + Combat | L | 027 | `backlog` |
| GWA3-031 | Integration: Loot + Inventory + Salvage | M | 027 | `backlog` |
| GWA3-032 | Integration: Merchant + Crafting | M | 027 | `backlog` |
| **Epic 7: Hardening** | | | | |
| GWA3-033 | Full Froggy HM 10+ Runs | XL | 028-032 | `backlog` |
| GWA3-034 | Multi-Client Injector | M | 002 | `backlog` |
| GWA3-035 | Pattern Health Check Tool | M | 005 | `backlog` |
| GWA3-036 | Crash Protection + SEH | M | 033 | `backlog` |

**Total: 36 tickets across 7 epics.**

---

## Maximum Parallelism Schedule

| Wave | Tickets Running | Agent Slots Needed |
|------|----------------|-------------------|
| 1 | GWA3-001 | 1 |
| 2 | GWA3-002, GWA3-003, GWA3-004 | 3 |
| 3 | GWA3-005 (needs 003), GWA3-006 (needs 002+003+005) | 1-2 |
| 4 | GWA3-007, 008, 009, 011, 012, 013, 014, 015, 020 | **9** (max parallelism) |
| 5 | GWA3-010, 016, 017, 018, 019, 021, 022, 023, 024 | **9** (max parallelism) |
| 6 | GWA3-025 | 1 |
| 7 | GWA3-026 | 1 |
| 8 | GWA3-027 | 1 |
| 9 | GWA3-028, 029, 030, 031, 032 | **5** |
| 10 | GWA3-033 | 1 |
| 11 | GWA3-034, 035, 036 | 3 |

**Peak parallelism: 9 agents** (Waves 4-5, struct definitions and manager implementations).
