# Building Our Own GWCA: C++ Replacement Plan

## Executive Summary

**Goal:** Replace the closed-source `gwca.dll` AND the entire AutoIt injection layer (GWA2_Assembly.au3, GWA2.au3, Utils.au3) with a custom C++ DLL built from our reverse-engineering research.

**Why C++ instead of staying in AutoIt?**

| Factor | AutoIt (current) | C++ DLL |
|--------|------------------|---------|
| Injection | External process — `WriteProcessMemory` + `CreateRemoteThread` per command | In-process — direct function calls, zero IPC overhead |
| Speed | Each command crosses process boundary (~1ms+ per call) | Nanosecond function calls, game-thread native |
| Hooking | Hand-assembled x86 shellcode strings in AutoIt | Standard hooking libraries (MinHook, Detours) or hand-rolled with proper tooling |
| Debugging | Console prints, no debugger | Full Visual Studio debugger, breakpoints in game process |
| Maintenance | Hex pattern strings, manual struct offsets | Proper C++ structs, IDE autocomplete, type safety |
| Stability | Fragile — one bad `WriteProcessMemory` corrupts game | In-process with proper error handling |
| Frame UI | Shellcode injected per click, standalone threads | Direct function calls on game thread |
| Bot logic | Must stay in AutoIt (orchestration layer) | Expose DLL exports that AutoIt calls via `DllCall` — or migrate bot logic to C++ entirely |

**The vision:** A single `gwa3.dll` injected into the GW process that provides the complete game API. Bot scripts can either:
1. **Stay in AutoIt** — call `gwa3.dll` exports via `DllCall()` (incremental migration)
2. **Move to C++** — bot logic compiled into the DLL itself (full migration)
3. **Move to Python** — use `ctypes` or a Python wrapper around the DLL (future option)

---

## Architecture Overview

```
CURRENT ARCHITECTURE:
┌─────────────────────┐     WriteProcessMemory      ┌──────────────┐
│  AutoIt Process      │ ─────────────────────────► │  GW.exe       │
│                      │     ReadProcessMemory       │              │
│  Froggy_HM.au3      │ ◄───────────────────────── │  gwca.dll    │
│  GWA2.au3            │     CreateRemoteThread      │  (injected)  │
│  GWA2_Assembly.au3   │ ─────────────────────────► │              │
│  Utils.au3           │                             │  Bot ASM     │
│  GWA2_FrameUI.au3   │                             │  (injected)  │
└─────────────────────┘                             └──────────────┘

TARGET ARCHITECTURE:
┌─────────────────────┐       DllCall()             ┌──────────────┐
│  AutoIt Process      │ ─────────────────────────► │  GW.exe       │
│                      │                             │              │
│  Froggy_HM.au3      │  (or bot logic moves       │  gwa3.dll    │
│  (thin orchestration)│   into the DLL entirely)   │  (our DLL)   │
│                      │                             │              │
└─────────────────────┘                             └──────────────┘
```

The target is dramatically simpler. One injection point. One DLL. No cross-process memory manipulation for every command.

---

## What We Know from RE Research

Our 8 research documents have reverse-engineered these GWCA internals:

### Fully Documented (Can Reimplement)

| Component | Research Doc | Confidence |
|-----------|-------------|------------|
| Frame struct layout (0x1C8 bytes, all key offsets) | ButtonClick, UIMessage | 95% |
| ButtonClick chain (MouseAction → SendFrameUIMsg) | ButtonClick | 100% |
| SendFrameUIMessage signature and calling convention | UIMessage, LiveDetour | 100% |
| SendUIMessage (global) signature | UIMessage | 85% |
| UIMessage enum values (20+ documented) | UIMessage | 85% |
| UIMessage payload structs (travel, items, skills, quests) | UIMessage | 80% |
| Frame message IDs (0x20-0x61, 0x7FFFFFF5) | UIMessage | 80% |
| Hook::CreateHook full signature and flow | HookInstaller, DeepDive | 75% |
| Hook record pool allocator (VirtualAlloc RWX pages) | HookRecordPool | 70% |
| Patch plan / stub writer (instruction relocation) | HookRecordPool | 50% |
| Frame hash lookup / traversal | ButtonClick, UIMessage | 90% |
| GWCA data section layout (+0x8A39C through +0x8A410) | All docs | 95% |
| Manager initialization pattern (scan → hook → relay) | UIMessage | 80% |
| Merchant dialog frame hierarchy | Crafting | 90% |
| Transaction function signature | Crafting | 95% |

### Already Implemented in AutoIt (Port to C++)

Our existing GWA2_Assembly.au3 + GWA2.au3 already implement:

| Component | AutoIt Implementation | Lines |
|-----------|----------------------|-------|
| Pattern scanner (48 patterns) | `RegisterAllScanPatterns()` + injected ASM | ~400 |
| Command queue (256-byte slots) | MainProc detour + `Enqueue()` | ~300 |
| Agent system (read/target/distance) | `GetAgent*()`, `GetMyAgent()` | ~500 |
| Skill system (use/recharge/data) | `UseSkill()`, `GetSkillPtr()` | ~300 |
| Item/inventory system | `GetItem*()`, `MoveItem()`, `UseItem()` | ~400 |
| Packet sending (100+ headers) | `SendPacket()` + header constants | ~200 |
| Movement (move/turn/strafe) | `Move()`, `MoveTo()` | ~200 |
| Hero management | `AddHero()`, `SetHeroBehaviour()` | ~200 |
| Party system | `GetParty*()`, invite/kick | ~150 |
| Trade system | Buy/sell/player trade | ~200 |
| Dialog system | `Dialog()`, quest interactions | ~100 |
| Salvage/identify | Session-based operations | ~150 |
| Map travel | `MoveMap()`, `WaitMapLoading()` | ~100 |
| Rendering hook | Toggle rendering on/off | ~50 |
| Engine hook | Main loop detour for command queue | ~100 |
| Chat hook | Message interception + callback | ~250 |
| Friend list | Add/remove/status | ~50 |
| Title system | Display/query titles | ~50 |

**Total: ~3,700 lines of AutoIt that map to C++ module implementations.**

---

## C++ Project Structure

```
gwa3/
├── CMakeLists.txt                    # Build system
├── include/
│   ├── gwa3.h                        # Public API (DLL exports)
│   ├── gwa3/
│   │   ├── core/
│   │   │   ├── Scanner.h             # Pattern scanning engine
│   │   │   ├── Hook.h                # Hook installation (MinHook or custom)
│   │   │   ├── GameThread.h          # Game thread command execution
│   │   │   ├── Memory.h              # Memory read/write helpers
│   │   │   └── Offsets.h             # All scanned addresses cached here
│   │   ├── game/
│   │   │   ├── Agent.h               # Agent struct + accessors
│   │   │   ├── Skill.h               # Skill struct + data
│   │   │   ├── Item.h                # Item struct + inventory
│   │   │   ├── Map.h                 # Map/instance data
│   │   │   ├── Party.h               # Party + hero data
│   │   │   ├── Quest.h               # Quest tracking
│   │   │   └── Effect.h              # Buffs/conditions
│   │   ├── managers/
│   │   │   ├── AgentMgr.h            # Agent commands (move, target, attack)
│   │   │   ├── SkillMgr.h            # Skill usage + hero skills
│   │   │   ├── ItemMgr.h             # Item manipulation + trade
│   │   │   ├── MapMgr.h              # Travel + instance management
│   │   │   ├── PartyMgr.h            # Party/hero management
│   │   │   ├── ChatMgr.h             # Chat send/receive
│   │   │   ├── TradeMgr.h            # Player + NPC trading
│   │   │   ├── QuestMgr.h            # Quest interactions
│   │   │   └── UIMgr.h               # Frame system + UIMessage
│   │   └── packets/
│   │       ├── CtoS.h                # Client-to-server packet sending
│   │       └── Headers.h             # All 100+ packet header constants
│   └── exports/
│       └── AutoItBridge.h            # DllCall-friendly C exports
├── src/
│   ├── dllmain.cpp                   # DLL entry point + initialization
│   ├── core/
│   │   ├── Scanner.cpp               # Port of GWA2_Assembly scan logic
│   │   ├── Hook.cpp                  # Hook installer (replaces GWCA's)
│   │   ├── GameThread.cpp            # Game thread hook + command queue
│   │   └── Offsets.cpp               # Pattern registration + resolution
│   ├── game/                         # Struct definitions (headers only mostly)
│   ├── managers/
│   │   ├── AgentMgr.cpp              # Port of GWA2 agent functions
│   │   ├── SkillMgr.cpp              # Port of skill functions
│   │   ├── ItemMgr.cpp               # Port of item functions
│   │   ├── MapMgr.cpp                # Port of map/travel functions
│   │   ├── PartyMgr.cpp              # Port of party/hero functions
│   │   ├── ChatMgr.cpp               # Port of chat hook + send
│   │   ├── TradeMgr.cpp              # Port of trade functions
│   │   ├── QuestMgr.cpp              # Port of quest functions
│   │   └── UIMgr.cpp                 # Frame system + ButtonClick
│   ├── packets/
│   │   └── CtoS.cpp                  # SendPacket implementation
│   └── exports/
│       └── AutoItBridge.cpp          # Flat C functions for DllCall
└── tools/
    ├── injector.cpp                  # Standalone DLL injector
    └── pattern_test.cpp              # Pattern validation tool
```

---

## Phased Implementation Plan

### Phase 1: Skeleton DLL + Injection + Scanner (Foundation)

**Goal:** A DLL that injects into GW.exe, finds all game functions via pattern scanning, and proves it can read game memory.

#### 1A: Project Setup

- Create Visual Studio / CMake project targeting x86 (GW is 32-bit)
- Configure for Release build with optimizations (Debug for development)
- Set output to `gwa3.dll`
- Add `dllmain.cpp` with `DLL_PROCESS_ATTACH` entry point
- Create a standalone injector (`injector.exe`) that:
  - Finds GW.exe by window class (`ArenaNet_Dx_Window_Class`)
  - Opens process with `PROCESS_ALL_ACCESS`
  - Allocates memory in target with `VirtualAllocEx`
  - Writes DLL path string
  - Creates remote thread calling `LoadLibraryA`
  - Waits for thread completion

```cpp
// dllmain.cpp - skeleton
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &InitThread, hModule, 0, nullptr);
    }
    return TRUE;
}

DWORD WINAPI InitThread(LPVOID hModule) {
    GWA3::Scanner::Initialize();     // Find all game functions
    GWA3::GameThread::Initialize();  // Hook game loop
    GWA3::Managers::Initialize();    // Set up all managers
    return 0;
}
```

#### 1B: Pattern Scanner

Port the scanning logic from GWA2_Assembly.au3. The scanner needs to:

1. Find the GW.exe module base address and size
2. Parse PE headers to locate `.text`, `.rdata`, `.data` sections
3. Scan sections for byte patterns with wildcard support
4. Support assertion-based patterns (scan for string literal, then find xref)

```cpp
namespace GWA3::Scanner {
    void Initialize(HMODULE gw_module = nullptr);

    // Byte pattern scan (equivalent to GWA2's AddPattern + ExecutePatternScan)
    uintptr_t Find(const char* pattern, const char* mask, int offset = 0);

    // Assertion scan (equivalent to GWA2's ResolveAssertionPatterns)
    uintptr_t FindAssertion(const char* source_file, const char* message, int offset = 0);

    // Resolve CALL/JMP targets
    uintptr_t FunctionFromNearCall(uintptr_t address);

    // Section boundaries
    struct Section { uintptr_t start; size_t size; };
    Section GetTextSection();
    Section GetRdataSection();
    Section GetDataSection();
}
```

**Port all 48 scan patterns** from `RegisterAllScanPatterns()` in GWA2_Assembly.au3:

```cpp
namespace GWA3::Offsets {
    // Core
    uintptr_t BasePointer;          // "506A0F6A00FF35" +8
    uintptr_t AgentBase;            // "8B0C9085C97419" -3
    uintptr_t MyID;                 // "83EC08568BF13B15" -3
    uintptr_t CurrentTarget;        // "83C4085F8BE55DC3..." -14

    // Movement
    uintptr_t Move;                 // "558BEC83EC208D45F0" +1
    uintptr_t Action;               // "8B7508578BF983FE09..." -3

    // Skills
    uintptr_t SkillBase;            // "69C6A40000005E" +9
    uintptr_t UseSkill;             // "85F6745B83FE1174" -295
    uintptr_t UseHeroSkill;         // "BA02000000B954080000" -89

    // Packets
    uintptr_t PacketSend;           // "C747540000000081E6" -79

    // ... all 48 patterns
    // Plus NEW patterns for frame system:
    uintptr_t SendFrameUIMsg;       // "83C1DCE8" resolved via call target
    uintptr_t FrameArray;           // FrMsg.cpp assertion
    uintptr_t SendUIMessage;        // "B900000000E8000000005DC3894508" -20
    uintptr_t RootFrame;            // NEW pattern needed
    uintptr_t GetChildFrame;        // NEW pattern needed

    bool ResolveAll();              // Run all scans, return false if any critical pattern fails
}
```

#### 1C: Proof of Life

Create a minimal test that proves the DLL is working:

```cpp
// In InitThread, after scanner:
auto agent_base = GWA3::Offsets::AgentBase;
auto my_id_ptr = GWA3::Offsets::MyID;
auto my_id = *reinterpret_cast<uint32_t*>(my_id_ptr);
// Log: "My agent ID: %d", my_id
// Log: "Agent base: 0x%08X", agent_base
// Log: "All %d patterns resolved successfully"
```

Logging options:
- `OutputDebugStringA` (view with DebugView/VS debugger)
- Write to `gwa3_log.txt` in GW directory
- Allocate a console with `AllocConsole()`

#### Deliverables
- [ ] CMake/VS project compiling to x86 `gwa3.dll`
- [ ] `injector.exe` that loads the DLL into a running GW client
- [ ] Pattern scanner resolving all 48 existing patterns + 3-5 new frame patterns
- [ ] Log output proving agent ID, map ID, and base pointer are correct
- [ ] Runs without crashing GW for 5+ minutes

---

### Phase 2: Game Thread Hook + Packet Sending (Commands)

**Goal:** Hook the game's main loop and send commands (movement, skills, packets) from within the game thread.

#### 2A: Game Thread Hook

The existing AutoIt code hooks the game engine loop via a detour. Port this:

```cpp
namespace GWA3::GameThread {
    // Hook the game's main update loop
    // This gives us a safe context to call game functions
    void Initialize();
    void Shutdown();

    // Queue a function to run on the game thread (thread-safe)
    void Enqueue(std::function<void()> fn);

    // Check if we're currently on the game thread
    bool IsOnGameThread();
}
```

**Implementation approach:**
- Scan for the `Engine` pattern (same one GWA2_Assembly uses)
- Install a detour using MinHook or manual JMP patching
- Detour checks a command queue each frame
- Commands are `std::function<void()>` objects pushed from any thread

This replaces the entire 256-byte slot command queue from AutoIt. Instead of serializing commands into binary structs and writing them cross-process, we just enqueue lambdas:

```cpp
// Old way (AutoIt): serialize struct, WriteProcessMemory, hope the ASM dispatcher handles it
// New way (C++):
GWA3::GameThread::Enqueue([=]() {
    auto move_fn = reinterpret_cast<void(__cdecl*)(float, float, uint32_t)>(Offsets::Move);
    move_fn(x, y, 0);
});
```

#### 2B: Packet Sending

Port `SendPacket()` — this is the backbone of 70% of bot commands:

```cpp
namespace GWA3::CtoS {
    // Send a raw game packet (equivalent to GWA2's SendPacket)
    // Must be called from game thread
    void SendPacket(uint32_t size, ...);

    // Type-safe wrappers for common packets
    void MoveToCoord(float x, float y);
    void Attack(uint32_t agent_id);
    void UseSkill(uint32_t skill_slot, uint32_t target_id = 0, uint32_t call_target = 0);
    void UseHeroSkill(uint32_t hero_index, uint32_t skill_slot, uint32_t target_id = 0);
    void Dialog(uint32_t dialog_id);
    void ChangeTarget(uint32_t agent_id);
    void InteractNPC(uint32_t agent_id);
    void MoveItem(uint32_t item_id, uint32_t bag_id, uint32_t slot);
    void UseItem(uint32_t item_id);
    void IdentifyItem(uint32_t item_id);
    void MapTravel(uint32_t map_id, uint32_t region = 0, uint32_t district = 0, uint32_t language = 0);
    void AddHero(uint32_t hero_id);
    void KickHero(uint32_t hero_id);
    void SetHeroBehavior(uint32_t hero_agent_id, uint32_t behavior);
    void LoadSkillbar(uint32_t skill_ids[8], uint32_t hero_index = 0);
    // ... all 100+ packet types from GWA2_Headers.au3
}
```

**All packet header constants** ported from GWA2_Headers.au3:

```cpp
namespace GWA3::Packets {
    // Trade
    constexpr uint32_t TRADE_PLAYER          = 0x00;
    constexpr uint32_t TRADE_CANCEL           = 0x01;
    constexpr uint32_t TRADE_ADD_ITEM         = 0x02;
    // ... all 100+ headers
    constexpr uint32_t MAP_TRAVEL             = 0xB1;
    constexpr uint32_t TARGET_AGENT           = 0xC1;
}
```

#### 2C: Movement Test

Prove the system works end-to-end:

```cpp
// From AutoIt via DllCall, or from internal test:
GWA3::GameThread::Enqueue([]() {
    GWA3::CtoS::MoveToCoord(player_x + 100.0f, player_y + 100.0f);
});
// Character should visibly move
```

#### Deliverables
- [ ] Game thread hook installed, processing commands each frame
- [ ] `SendPacket` working for raw packets
- [ ] Type-safe wrappers for movement, skills, targeting, dialog
- [ ] Test: character moves to coordinates via DLL command
- [ ] Test: skill usage works via DLL command
- [ ] No crashes over 10+ minutes of idle + periodic commands

---

### Phase 3: Game State Reading (Data Structures)

**Goal:** Read all game state — agents, items, skills, map, party — via proper C++ structs.

#### 3A: Agent System

Port the agent struct from GWA2_Assembly.au3's `$AGENT_STRUCT_TEMPLATE`:

```cpp
namespace GWA3 {

#pragma pack(push, 1)
struct Agent {
    /* +0x000 */ uint32_t vtable;
    /* +0x004 */ uint8_t  pad_004[0x20];
    /* +0x024 */ uint32_t id;
    /* +0x028 */ float    z;
    /* ... exact offsets from GWA2_Assembly template ... */
    /* +0x0C0 */ float    x;
    /* +0x0C4 */ float    y;
    /* +0x0C8 */ uint32_t plane;
    /* ... */
    /* +0x130 */ float    health_percent;   // 0.0 - 1.0
    /* +0x138 */ float    energy_percent;
    /* ... */
    /* +0x154 */ uint32_t model_id;
    /* +0x164 */ uint8_t  primary_profession;
    /* +0x165 */ uint8_t  secondary_profession;
    /* +0x166 */ uint8_t  level;
    /* +0x167 */ uint8_t  team;
    /* ... all fields from the 446-byte template ... */

    bool IsAlive() const { return health_percent > 0.0f; }
    bool IsDead() const { return health_percent <= 0.0f; }
    float DistanceTo(const Agent& other) const;
    float DistanceTo(float tx, float ty) const;
};
#pragma pack(pop)

namespace AgentMgr {
    Agent*   GetMyAgent();
    uint32_t GetMyID();
    Agent*   GetAgentByID(uint32_t id);
    Agent**  GetAgentArray();
    uint32_t GetMaxAgents();
    uint32_t GetCurrentTargetID();
}

} // namespace GWA3
```

The struct offsets come directly from GWA2_Assembly.au3's template string. We know every field's position — it's just a matter of writing the `#pragma pack` struct.

#### 3B: Skill System

```cpp
#pragma pack(push, 1)
struct Skill {
    /* +0x00 */ uint32_t id;
    /* +0x04 */ uint32_t campaign;
    /* +0x08 */ uint32_t type;
    /* +0x0C */ uint32_t special;
    /* +0x10 */ uint32_t profession;
    /* +0x14 */ uint32_t attribute;
    /* ... 40+ fields from $SKILL_STRUCT_TEMPLATE ... */
    /* +0x30 */ float    activation_time;
    /* +0x34 */ float    aftercast;
    /* +0x38 */ uint32_t recharge;
    /* +0x3C */ uint32_t energy_cost;
    /* +0x40 */ uint32_t adrenaline_cost;
};

struct SkillbarSlot {
    uint32_t adrenaline_a;
    uint32_t adrenaline_b;
    uint32_t recharge;
    uint32_t skill_id;
    uint32_t event;
};

struct Skillbar {
    uint32_t    agent_id;
    SkillbarSlot slots[8];
};
#pragma pack(pop)

namespace SkillMgr {
    Skill*   GetSkillByID(uint32_t skill_id);
    Skillbar* GetPlayerSkillbar();
    Skillbar* GetHeroSkillbar(uint32_t hero_index);
    bool     IsRecharged(uint32_t slot, uint32_t hero_index = 0);
    void     UseSkill(uint32_t slot, uint32_t target_id = 0);
    void     UseHeroSkill(uint32_t hero_index, uint32_t slot, uint32_t target_id = 0);
    void     LoadSkillbar(uint32_t skill_ids[8], uint32_t hero_index = 0);
}
```

#### 3C: Item & Inventory System

```cpp
#pragma pack(push, 1)
struct Item {
    /* +0x00 */ uint32_t id;
    /* +0x04 */ uint32_t agent_id;
    /* ... from $ITEM_STRUCT_TEMPLATE ... */
    /* +0x20 */ uint32_t model_id;
    /* +0x24 */ uint32_t type;
    /* +0x28 */ uint32_t dye_tint;
    /* +0x2C */ uint32_t value;
    /* +0x34 */ uint32_t interaction;   // rarity bits, identified, etc.
    /* +0x44 */ uint32_t quantity;
    /* +0x48 */ uint8_t  equipped;
    /* +0x49 */ uint8_t  profession;
    /* +0x4A */ uint8_t  slot;

    bool IsIdentified() const;
    bool IsSalvageable() const;
    uint8_t GetRarity() const;       // White/Blue/Purple/Gold/Green
};

struct Bag {
    uint32_t bag_type;
    uint32_t index;
    uint32_t item_count;
    uint32_t max_slots;
    Item**   items;                    // Array of item pointers
};
#pragma pack(pop)

namespace ItemMgr {
    Item* GetItemByID(uint32_t item_id);
    Item* GetItemBySlot(uint32_t bag_id, uint32_t slot);
    Bag*  GetBag(uint32_t bag_id);     // 1=Backpack, 2=Belt, 3=Bag1, 4=Bag2, ...
    uint32_t GetGoldOnCharacter();
    uint32_t GetGoldInStorage();

    void MoveItem(uint32_t item_id, uint32_t bag_id, uint32_t slot);
    void UseItem(uint32_t item_id);
    void DropItem(uint32_t item_id);
    void EquipItem(uint32_t item_id);
    void IdentifyItem(uint32_t item_id);
    void SalvageStart(uint32_t item_id, uint32_t kit_id);
    void SalvageMaterials();
    void SalvageDone();
    void DestroyItem(uint32_t item_id);

    // Merchant
    Item** GetMerchantItems();
    uint32_t GetMerchantItemCount();
    void BuyItem(uint32_t item_id, uint32_t quantity = 1);
    void SellItem(uint32_t item_id);
    void RequestQuote(uint32_t item_id);
}
```

#### 3D: Map, Party, Quest, Effects

```cpp
namespace MapMgr {
    uint32_t GetMapID();
    uint32_t GetMapType();           // 0=outpost, 1=explorable
    uint32_t GetRegion();
    uint32_t GetDistrict();
    bool     IsMapLoading();         // True during zone transitions
    bool     IsMapLoaded();
    uint32_t GetInstanceUptime();

    void Travel(uint32_t map_id, uint32_t district = 0);
    void ReturnToOutpost();
    void EnterMission();
    void SetHardMode(bool hard);
}

namespace PartyMgr {
    uint32_t GetPartySize();
    uint32_t GetHeroCount();
    uint32_t GetHeroAgentID(uint32_t hero_index);

    void AddHero(uint32_t hero_id);
    void KickHero(uint32_t hero_id);
    void KickAllHeroes();
    void AddHenchman(uint32_t npc_id);
    void KickHenchman(uint32_t npc_id);
    void LeaveParty();

    void SetHeroBehavior(uint32_t hero_index, uint32_t behavior);
    void FlagHero(uint32_t hero_index, float x, float y);
    void FlagAll(float x, float y);
    void LockHeroTarget(uint32_t hero_index, uint32_t target_id);
}

namespace QuestMgr {
    uint32_t GetActiveQuestID();
    void SetActiveQuest(uint32_t quest_id);
    void AbandonQuest(uint32_t quest_id);
}

namespace EffectMgr {
    struct Buff {
        uint32_t skill_id;
        uint32_t buff_id;
        uint32_t target_id;
    };
    struct Effect {
        uint32_t skill_id;
        uint32_t attribute_level;
        uint32_t effect_id;
        uint32_t agent_id;
        float    duration;
        uint32_t timestamp;
    };

    Buff*   GetBuffs(uint32_t* count);
    Effect* GetEffects(uint32_t* count);
    bool    HasEffect(uint32_t agent_id, uint32_t skill_id);
    float   GetEffectTimeRemaining(uint32_t agent_id, uint32_t skill_id);
}

namespace ChatMgr {
    void SendChat(const wchar_t* message, wchar_t channel = L'/');
    void SendWhisper(const wchar_t* target, const wchar_t* message);
    void WriteToChat(const wchar_t* message);
    // Hook for incoming messages (register callback)
    using ChatCallback = void(*)(uint32_t channel, const wchar_t* message);
    void RegisterChatCallback(ChatCallback cb);
}
```

#### Deliverables
- [ ] All game structs defined with correct `#pragma pack` offsets
- [ ] Agent array readable — player position, HP, profession confirmed correct
- [ ] Skill database accessible — skill names, costs, recharge times
- [ ] Item/bag system reading all inventory slots
- [ ] Map state (ID, loading, type) reading correctly
- [ ] Party/hero info accessible
- [ ] Test: log player position every second for 60 seconds — all values valid

---

### Phase 4: Frame UI System (The GWCA-Unique Part)

**Goal:** Implement native frame traversal and ButtonClick — the one capability that only GWCA provides today.

This is the phase where our RE research pays off directly. Everything before this phase could theoretically be built from GWA2's AutoIt code alone. Phase 4 requires the insights from the 8 research documents.

#### 4A: Frame System

```cpp
namespace GWA3 {

#pragma pack(push, 1)
struct Frame {
    /* +0x000 */ uint8_t  pad_000[0xA8];
    /* +0x0A8 */ uint32_t callbacks;         // Array<FrameInteractionCallback>
    /* +0x0AC */ uint8_t  pad_0AC[0x0C];
    /* +0x0B8 */ uint32_t child_offset_id;
    /* +0x0BC */ uint32_t frame_id;
    /* +0x0C0 */ uint8_t  pad_0C0[0x68];
    /* +0x128 */ uint32_t relation;          // FrameRelation: parent, siblings, hash
    /* +0x12C */ uint8_t  pad_12C[0x08];
    /* +0x134 */ uint32_t frame_hash_id;
    /* +0x138 */ uint8_t  pad_138[0x54];
    /* +0x18C */ uint32_t frame_state;       // Bitfield
    /* +0x190 */ uint8_t  pad_190[0x34];
    /* +0x1C4 */ uint32_t field_1C4;         // Button payload

    bool IsCreated() const  { return (frame_state & 0x4) != 0; }
    bool IsHidden() const   { return (frame_state & 0x200) != 0; }
    bool IsDisabled() const { return (frame_state & 0x10) != 0; }
    bool IsClickable() const { return IsCreated() && !IsHidden() && !IsDisabled(); }

    Frame* GetParent() const {
        if (!relation) return nullptr;
        return reinterpret_cast<Frame*>(relation - 0x128);
    }
};
#pragma pack(pop)

} // namespace GWA3
```

#### 4B: Frame Lookup

```cpp
namespace GWA3::UIMgr {
    // Scan-discovered function pointers
    // These replace GWCA data section reads (+0x8A39C, +0x8A37C, etc.)

    Frame* GetFrameByHash(uint32_t hash);
    Frame* GetRootFrame();
    Frame* GetChildFrame(Frame* parent, uint32_t child_offset);

    bool IsFrameVisible(uint32_t hash);

    // Known frame hashes (from research)
    namespace Hashes {
        constexpr uint32_t PlayButton       = 184818986;
        constexpr uint32_t PlayGreyed       = 41327607;
        constexpr uint32_t ReconnectYes     = 1398610279;
        constexpr uint32_t ReconnectNo      = 3600335809;
        constexpr uint32_t CreateButton     = 3372446797;
        constexpr uint32_t DeleteButton     = 3379687503;
        constexpr uint32_t LogOutButton     = 1117342925;
        constexpr uint32_t CharacterFrame   = 828467986;
        constexpr uint32_t MerchantRoot     = 3613855137;
        constexpr uint32_t CraftTab         = 1517397806;
        constexpr uint32_t CraftButton      = 835947118;
        constexpr uint32_t GoodbyeButton    = 3068881268;
    }
}
```

#### 4C: ButtonClick

The crown jewel — replicate GWCA's ButtonClick chain entirely in our C++:

```cpp
namespace GWA3::UIMgr {

    // Replicates GWCA's ButtonClick → MouseAction → SendFrameUIMsg chain
    // Must be called from game thread
    bool ButtonClick(Frame* frame) {
        if (!frame || !frame->IsClickable()) return false;

        Frame* parent = frame->GetParent();
        if (!parent) return false;

        // ECX for __thiscall = parent + 0xA8 (callback array)
        void* context = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(parent) + 0xA8
        );

        // Build kMouseAction struct
        struct MouseAction {
            uint32_t frame_id;
            uint32_t child_offset_id;
            uint32_t action_state;
        };

        // GWCA sends ONLY MouseUp (7), confirmed in research
        MouseAction action = {
            frame->frame_id,
            frame->child_offset_id,
            7  // MouseUp
        };

        // Call game's SendFrameUIMsg: __thiscall(ECX=context, msgid=0x31, wParam=&action, lParam=0)
        auto send_fn = reinterpret_cast<
            bool(__thiscall*)(void*, uint32_t, void*, void*)
        >(Offsets::SendFrameUIMsg);

        return send_fn(context, 0x31, &action, nullptr);
    }

    // Convenience: click by hash
    bool ButtonClickByHash(uint32_t hash) {
        Frame* frame = GetFrameByHash(hash);
        return ButtonClick(frame);
    }

    // Send arbitrary frame UI message
    bool SendFrameUIMessage(Frame* frame, uint32_t msgid, void* wparam, void* lparam = nullptr) {
        if (!frame) return false;
        Frame* parent = frame->GetParent();
        if (!parent) return false;
        void* context = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(parent) + 0xA8
        );
        auto send_fn = reinterpret_cast<
            bool(__thiscall*)(void*, uint32_t, void*, void*)
        >(Offsets::SendFrameUIMsg);
        return send_fn(context, msgid, wparam, lparam);
    }

    // Global UIMessage (non-frame)
    void SendUIMessage(uint32_t msgid, void* wparam = nullptr, void* lparam = nullptr) {
        auto send_fn = reinterpret_cast<
            void(__cdecl*)(uint32_t, void*, void*)
        >(Offsets::SendUIMessage);
        send_fn(msgid, wparam, lparam);
    }
}
```

**Key insight:** Because we're in-process, we call the game's `SendFrameUIMsg` directly. No need for GWCA's hook chain, replay stubs, or hook record pool. We just call the function. This is dramatically simpler than what GWCA does.

#### 4D: Hooks (Rendering, Map Load, Trade)

Port the 5 existing detours from GWA2_Assembly.au3:

```cpp
namespace GWA3::Hooks {
    // Each hook: detour game function, call original, add our logic

    // Engine/main loop — already done in Phase 2 for command queue
    // Rendering — toggle rendering on/off
    void InstallRenderHook();
    void SetRenderingEnabled(bool enabled);

    // Map load complete — fires callback when zone finishes loading
    using MapLoadCallback = void(*)();
    void InstallMapLoadHook();
    void RegisterMapLoadCallback(MapLoadCallback cb);

    // Trader — capture quote results
    void InstallTraderHook();
    uint32_t GetTraderQuoteID();
    uint32_t GetTraderCostValue();

    // Trade partner — track current trade partner
    void InstallTradePartnerHook();
    uint32_t GetTradePartnerID();

    // Chat — intercept incoming messages (from GWA2_Assembly_Chatlog.au3)
    void InstallChatHook();
}
```

For hook installation, use **MinHook** (MIT license, battle-tested, x86 support):

```cpp
#include <MinHook.h>

// Example: rendering hook
static decltype(&OriginalRenderFunc) g_originalRender = nullptr;

void __cdecl RenderDetour(/* params */) {
    if (g_renderingEnabled) {
        g_originalRender(/* params */);
    }
}

void InstallRenderHook() {
    MH_CreateHook(
        reinterpret_cast<void*>(Offsets::Render),
        &RenderDetour,
        reinterpret_cast<void**>(&g_originalRender)
    );
    MH_EnableHook(reinterpret_cast<void*>(Offsets::Render));
}
```

#### Deliverables
- [ ] Frame hash lookup finding all known frames (character select, merchant, etc.)
- [ ] ButtonClick working for Play button at character select
- [ ] ButtonClick working for merchant craft/buy buttons
- [ ] SendFrameUIMessage working for arbitrary frame messages
- [ ] All 5 hooks ported (render, engine, map load, trader, trade partner)
- [ ] Test: full character select → enter game flow via frame clicks
- [ ] Test: merchant interaction (open, browse, craft) via frame clicks

---

### Phase 5: AutoIt Bridge (DLL Exports)

**Goal:** Expose the entire C++ API as flat C functions callable from AutoIt's `DllCall()`.

#### 5A: Export Design

AutoIt's `DllCall` can only call C-style exports (no C++ name mangling, no objects). Design flat wrappers:

```cpp
// exports/AutoItBridge.h
extern "C" {

// ===== Initialization =====
__declspec(dllexport) int  __cdecl GWA3_Initialize();
__declspec(dllexport) void __cdecl GWA3_Shutdown();
__declspec(dllexport) int  __cdecl GWA3_GetScanStatus();  // Returns count of failed patterns

// ===== Agent System =====
__declspec(dllexport) int   __cdecl GWA3_GetMyID();
__declspec(dllexport) float __cdecl GWA3_GetMyX();
__declspec(dllexport) float __cdecl GWA3_GetMyY();
__declspec(dllexport) float __cdecl GWA3_GetAgentHP(int agent_id);
__declspec(dllexport) float __cdecl GWA3_GetAgentEnergy(int agent_id);
__declspec(dllexport) int   __cdecl GWA3_GetAgentProfession(int agent_id);
__declspec(dllexport) int   __cdecl GWA3_GetAgentModelID(int agent_id);
__declspec(dllexport) float __cdecl GWA3_GetAgentX(int agent_id);
__declspec(dllexport) float __cdecl GWA3_GetAgentY(int agent_id);
__declspec(dllexport) int   __cdecl GWA3_IsAgentDead(int agent_id);
__declspec(dllexport) float __cdecl GWA3_GetDistance(int agent_id_1, int agent_id_2);
__declspec(dllexport) int   __cdecl GWA3_GetMaxAgents();
__declspec(dllexport) int   __cdecl GWA3_GetCurrentTarget();

// ===== Movement =====
__declspec(dllexport) void __cdecl GWA3_Move(float x, float y);
__declspec(dllexport) void __cdecl GWA3_Attack(int target_id);
__declspec(dllexport) void __cdecl GWA3_ChangeTarget(int target_id);
__declspec(dllexport) void __cdecl GWA3_InteractNPC(int agent_id);
__declspec(dllexport) void __cdecl GWA3_CancelAction();

// ===== Skills =====
__declspec(dllexport) void __cdecl GWA3_UseSkill(int slot, int target_id);
__declspec(dllexport) void __cdecl GWA3_UseHeroSkill(int hero_index, int slot, int target_id);
__declspec(dllexport) int  __cdecl GWA3_IsRecharged(int slot, int hero_index);
__declspec(dllexport) int  __cdecl GWA3_GetSkillRecharge(int slot, int hero_index);
__declspec(dllexport) void __cdecl GWA3_LoadSkillbar(int s1, int s2, int s3, int s4,
                                                      int s5, int s6, int s7, int s8,
                                                      int hero_index);

// ===== Items =====
__declspec(dllexport) void __cdecl GWA3_UseItem(int item_id);
__declspec(dllexport) void __cdecl GWA3_MoveItem(int item_id, int bag_id, int slot);
__declspec(dllexport) void __cdecl GWA3_DropItem(int item_id);
__declspec(dllexport) void __cdecl GWA3_EquipItem(int item_id);
__declspec(dllexport) void __cdecl GWA3_IdentifyItem(int item_id);
__declspec(dllexport) void __cdecl GWA3_SalvageStart(int item_id, int kit_id);
__declspec(dllexport) void __cdecl GWA3_SalvageDone();
__declspec(dllexport) int  __cdecl GWA3_GetItemModelID(int bag_id, int slot);
__declspec(dllexport) int  __cdecl GWA3_GetItemQuantity(int bag_id, int slot);
__declspec(dllexport) int  __cdecl GWA3_GetItemRarity(int bag_id, int slot);
__declspec(dllexport) int  __cdecl GWA3_GetBagSize(int bag_id);
__declspec(dllexport) int  __cdecl GWA3_GetGoldCharacter();
__declspec(dllexport) int  __cdecl GWA3_GetGoldStorage();

// ===== Map =====
__declspec(dllexport) int  __cdecl GWA3_GetMapID();
__declspec(dllexport) int  __cdecl GWA3_GetMapType();
__declspec(dllexport) int  __cdecl GWA3_IsMapLoading();
__declspec(dllexport) void __cdecl GWA3_Travel(int map_id, int district);
__declspec(dllexport) void __cdecl GWA3_ReturnToOutpost();
__declspec(dllexport) void __cdecl GWA3_EnterMission();
__declspec(dllexport) void __cdecl GWA3_SetHardMode(int hard);

// ===== Party & Heroes =====
__declspec(dllexport) int  __cdecl GWA3_GetPartySize();
__declspec(dllexport) int  __cdecl GWA3_GetHeroCount();
__declspec(dllexport) int  __cdecl GWA3_GetHeroAgentID(int hero_index);
__declspec(dllexport) void __cdecl GWA3_AddHero(int hero_id);
__declspec(dllexport) void __cdecl GWA3_KickHero(int hero_id);
__declspec(dllexport) void __cdecl GWA3_KickAllHeroes();
__declspec(dllexport) void __cdecl GWA3_SetHeroBehavior(int hero_index, int behavior);
__declspec(dllexport) void __cdecl GWA3_FlagHero(int hero_index, float x, float y);
__declspec(dllexport) void __cdecl GWA3_LockHeroTarget(int hero_index, int target_id);

// ===== Dialog & Quests =====
__declspec(dllexport) void __cdecl GWA3_Dialog(int dialog_id);
__declspec(dllexport) void __cdecl GWA3_SetActiveQuest(int quest_id);
__declspec(dllexport) void __cdecl GWA3_AbandonQuest(int quest_id);
__declspec(dllexport) int  __cdecl GWA3_GetActiveQuestID();

// ===== Trading =====
__declspec(dllexport) void __cdecl GWA3_BuyItem(int item_id, int quantity);
__declspec(dllexport) void __cdecl GWA3_SellItem(int item_id);
__declspec(dllexport) void __cdecl GWA3_RequestQuote(int item_id);
__declspec(dllexport) int  __cdecl GWA3_GetTraderCostValue();

// ===== Frame UI =====
__declspec(dllexport) int  __cdecl GWA3_ButtonClickByHash(unsigned int hash);
__declspec(dllexport) int  __cdecl GWA3_IsFrameVisible(unsigned int hash);
__declspec(dllexport) int  __cdecl GWA3_IsReconnectDialogShowing();

// ===== Rendering =====
__declspec(dllexport) void __cdecl GWA3_SetRendering(int enabled);
__declspec(dllexport) int  __cdecl GWA3_GetPing();

// ===== Chat =====
__declspec(dllexport) void __cdecl GWA3_SendChat(const wchar_t* message, int channel);

// ===== Raw Packet (escape hatch) =====
__declspec(dllexport) void __cdecl GWA3_SendPacket(int size, int header,
                                                    int p1, int p2, int p3, int p4,
                                                    int p5, int p6, int p7, int p8);

// ===== Effects =====
__declspec(dllexport) int   __cdecl GWA3_HasEffect(int agent_id, int skill_id);
__declspec(dllexport) float __cdecl GWA3_GetEffectTimeRemaining(int agent_id, int skill_id);

// ===== Titles =====
__declspec(dllexport) int __cdecl GWA3_GetTitleProgress(int title_id);

}
```

#### 5B: AutoIt Wrapper UDF

Create a thin AutoIt UDF that wraps `DllCall`:

```autoit
; GWA3.au3 — AutoIt wrapper for gwa3.dll
#include-once

Global $g_hGWA3 = -1

Func GWA3_Init()
    $g_hGWA3 = DllOpen("gwa3.dll")
    If $g_hGWA3 = -1 Then Return False
    DllCall($g_hGWA3, "int:cdecl", "GWA3_Initialize")
    Return True
EndFunc

Func GWA3_GetMyID()
    Local $ret = DllCall($g_hGWA3, "int:cdecl", "GWA3_GetMyID")
    Return $ret[0]
EndFunc

Func GWA3_Move($x, $y)
    DllCall($g_hGWA3, "none:cdecl", "GWA3_Move", "float", $x, "float", $y)
EndFunc

Func GWA3_UseSkill($slot, $target = 0)
    DllCall($g_hGWA3, "none:cdecl", "GWA3_UseSkill", "int", $slot, "int", $target)
EndFunc

; ... one wrapper per export
```

This lets Froggy_HM.au3 migrate incrementally — replace `Move($x, $y)` calls with `GWA3_Move($x, $y)` one function at a time.

#### Deliverables
- [ ] All DLL exports compiling and linkable
- [ ] `GWA3.au3` wrapper UDF with all functions
- [ ] Test from AutoIt: `GWA3_Init()` → `GWA3_GetMyID()` returns correct ID
- [ ] Test from AutoIt: `GWA3_Move()` moves character
- [ ] Test from AutoIt: `GWA3_ButtonClickByHash()` clicks Play button

---

### Phase 6: Integration & Migration (Froggy HM)

**Goal:** Run Froggy HM using `gwa3.dll` instead of GWA2_Assembly.au3.

#### 6A: Create Compatibility Shim

Write `GWA3_Compat.au3` that maps old GWA2 function names to GWA3 calls:

```autoit
; GWA3_Compat.au3 — Drop-in replacement for GWA2.au3 + GWA2_Assembly.au3
; Maps old API to new gwa3.dll exports

Func GetMyID()
    Return GWA3_GetMyID()
EndFunc

Func GetMapID()
    Return GWA3_GetMapID()
EndFunc

Func GetMapLoading()
    Return GWA3_IsMapLoading()
EndFunc

Func Move($x, $y, $random = 0)
    GWA3_Move($x + Random(-$random, $random), $y + Random(-$random, $random))
EndFunc

; ... map all ~200 functions
```

This allows the 32K-line main logic file to work without changes — just swap the include from `GWA2.au3` to `GWA3_Compat.au3`.

#### 6B: Incremental Testing

Test each subsystem in the Froggy HM flow:

| Step | What to Test | Functions Involved |
|------|-------------|-------------------|
| 1 | Character select | `ButtonClickByHash`, `IsFrameVisible` |
| 2 | Map loading | `GetMapID`, `IsMapLoading`, map load hook |
| 3 | Hero setup | `AddHero`, `KickAllHeroes`, `LoadSkillbar`, `SetHeroBehavior` |
| 4 | Town consumables | `UseItem`, `GetItemBySlot`, inventory reads |
| 5 | Map travel | `Travel`, `WaitMapLoading` |
| 6 | Movement | `Move`, `MoveTo` (with stuck detection) |
| 7 | Combat | `UseSkill`, `Attack`, `ChangeTarget`, agent reads |
| 8 | Loot | `PickUpItem`, item reads, `IdentifyItem`, `SalvageStart` |
| 9 | Merchant | `ButtonClickByHash` for merchant UI, `BuyItem`, `SellItem` |
| 10 | Full loop | 10+ consecutive runs without crash |

#### 6C: Remove Old Injection Layer

Once Froggy HM runs stable on gwa3.dll:

- Remove `#include "GWA2_Assembly.au3"` — no more ASM injection
- Remove `#include "GWA2.au3"` — replaced by `GWA3_Compat.au3`
- The AutoIt process no longer needs `PROCESS_ALL_ACCESS` — it just calls DLL functions
- GW process runs with only `gwa3.dll` injected (no separate ASM blob, no gwca.dll)

#### Deliverables
- [ ] `GWA3_Compat.au3` mapping all used GWA2 functions
- [ ] Froggy HM completing character select → enter game
- [ ] Froggy HM completing a full dungeon run
- [ ] 10+ consecutive runs without crash
- [ ] Old GWA2_Assembly.au3 injection removed from boot sequence

---

### Phase 7: Hardening & Extended Features (Ongoing)

#### 7A: Pattern Maintenance

```cpp
// Built-in pattern validation on startup
bool Offsets::ResolveAll() {
    int failures = 0;
    struct PatternDef {
        const char* name;
        uintptr_t* target;
        const char* pattern;
        const char* mask;
        int offset;
        bool critical;
    };

    PatternDef patterns[] = {
        {"BasePointer",  &BasePointer,  "\x50\x6A\x0F\x6A\x00\xFF\x35", "xxxxxxx", 8, true},
        {"AgentBase",    &AgentBase,    "\x8B\x0C\x90\x85\xC9\x74\x19", "xxxxxxx", -3, true},
        // ... all patterns
    };

    for (auto& p : patterns) {
        *p.target = Scanner::Find(p.pattern, p.mask, p.offset);
        if (*p.target == 0) {
            Log("FAILED: %s", p.name);
            failures++;
            if (p.critical) return false;  // Abort on critical failure
        }
    }
    Log("Resolved %d/%d patterns (%d failures)", total - failures, total, failures);
    return failures == 0;
}
```

#### 7B: Crash Protection

```cpp
// Structured exception handling around game calls
__try {
    move_fn(x, y, 0);
} __except(EXCEPTION_EXECUTE_HANDLER) {
    Log("CRASH in Move(%f, %f) — exception 0x%08X", x, y, GetExceptionCode());
}
```

#### 7C: Multi-Client Support

The injector needs to handle multiple GW instances:

```cpp
// injector.exe
// Enumerate all GW windows
// Let user select which to inject
// Or inject into all of them
// Each GW process gets its own gwa3.dll instance
```

#### 7D: Future — Bot Logic in C++

Once gwa3.dll is stable, bot logic can optionally move into C++:

```cpp
// Inside gwa3.dll — no AutoIt needed
namespace GWA3::Bot {
    void FroggyHM_MainLoop() {
        while (running) {
            if (MapMgr::GetMapID() == MAP_BOGROOT_GROWTHS) {
                RunDungeonFloor1();
                RunDungeonFloor2();
                CollectLoot();
                ReturnToOutpost();
            }
        }
    }
}
```

This eliminates the AutoIt process entirely. The bot runs as a single DLL inside GW.

---

## Build Requirements

| Tool | Version | Purpose |
|------|---------|---------|
| Visual Studio 2022 | Community (free) | C++ compiler + debugger |
| Windows SDK | 10.0+ | Windows API headers |
| CMake | 3.20+ | Build system (optional, VS projects work too) |
| MinHook | 1.3.3+ | Hook installation library (MIT license) |
| Target arch | **x86 (32-bit)** | GW.exe is 32-bit — DLL must match |

**Critical:** The DLL MUST be compiled as x86. GW.exe is a 32-bit process. An x64 DLL cannot be loaded.

---

## Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|-----------|
| **Struct offset wrong** | High | Validate every struct field against live memory before trusting it |
| **GW client update breaks patterns** | High | Maintain 2+ patterns per critical function; assertion-based fallbacks |
| **Game crash from bad function call** | Medium | SEH wrappers, game-thread-only calls, null checks on all pointers |
| **MinHook conflict with game** | Low | MinHook is well-tested; alternative: manual 5-byte JMP patching |
| **Anti-cheat detection** | Low | GW has no active anti-cheat (2005 game, ArenaNet doesn't enforce) |
| **DllCall overhead from AutoIt** | Low | Each DllCall is ~1us — negligible vs game tick (16ms) |
| **Thread safety** | Medium | All game-state reads/writes on game thread via `Enqueue()` |

---

## Comparison: This Plan vs Previous Plan (AutoIt-Only)

| Aspect | Previous (Eliminate gwca.dll) | This Plan (C++ Replacement) |
|--------|------------------------------|----------------------------|
| **Scope** | Remove gwca.dll dependency from AutoIt | Replace entire injection layer with C++ DLL |
| **AutoIt changes** | Moderate — new scan patterns, command queue additions | Minimal — swap includes, use compatibility shim |
| **New code** | ~500 lines AutoIt | ~5,000-8,000 lines C++ |
| **Performance** | Same as today (cross-process) | 100-1000x faster per command (in-process) |
| **Debugging** | ConsoleWrite + trial and error | Visual Studio debugger with breakpoints |
| **Maintainability** | Hex strings in AutoIt | Type-safe C++ with IDE support |
| **Future potential** | Stuck in AutoIt | Path to C++ bot logic, Python scripting, etc. |
| **Effort** | 4-7 sessions | 10-15 sessions |
| **Risk** | Low (incremental changes) | Medium (new foundation, but cleaner) |

---

## Session Estimates

| Phase | Sessions | What Gets Built |
|-------|----------|----------------|
| Phase 1: Skeleton + Scanner | 2-3 | DLL loads, patterns resolve, reads agent ID |
| Phase 2: Game Thread + Packets | 2-3 | Commands execute (move, skill, packets) |
| Phase 3: Data Structures | 2-3 | All game state readable (agents, items, skills, map) |
| Phase 4: Frame UI | 2-3 | ButtonClick, frame lookup, hooks |
| Phase 5: AutoIt Bridge | 1-2 | DllCall exports, AutoIt wrapper UDF |
| Phase 6: Integration | 2-3 | Froggy HM running on gwa3.dll |
| Phase 7: Hardening | Ongoing | Stability, multi-client, crash protection |

**Minimum viable: ~12-15 sessions** to have Froggy HM running on the new C++ DLL.

**Critical path:** Phase 1 → Phase 2 → Phase 3 → Phase 5 → Phase 6. Phase 4 (Frame UI) can be deferred if character select is handled manually during testing.
