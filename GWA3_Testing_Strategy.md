# GWA3 Testing Strategy

## The Problem

`gwa3.dll` is an injected DLL. Most of its code only works inside a live GW.exe process. You can't run standard unit tests in a CI pipeline — there's no game process to inject into. But you also can't wait until everything is built to start testing. We need a layered approach.

---

## Three Testing Layers

```
Layer 1: Offline Unit Tests (no game, no injection)
  ├── Pattern parsing, struct layouts, header constants
  ├── Runs in any build environment
  └── Catches: typos, struct alignment, logic errors

Layer 2: Injection Smoke Tests (game running, DLL injected)
  ├── Pattern scanner finds addresses, reads game state
  ├── Requires running GW.exe
  └── Catches: wrong patterns, broken offsets, bad calling conventions

Layer 3: Behavioral Integration Tests (game running, commands sent)
  ├── Move character, use skills, click frames, travel maps
  ├── Requires logged-in character
  └── Catches: wrong function signatures, thread safety, state corruption
```

---

## Layer 1: Offline Unit Tests

These run without a game client. Build them from day one.

### What You Can Test Offline

**Struct layout validation (Phase 3 tickets: GWA3-007..015)**

The most critical offline test: verify every `#pragma pack` struct has fields at the exact byte offsets the game expects.

```cpp
// tests/test_structs.cpp
#include <gwa3/game/Agent.h>
#include <cassert>

void test_agent_struct_offsets() {
    // Offsets from GWA2_Assembly.au3 $AGENT_STRUCT_TEMPLATE
    static_assert(offsetof(Agent, id)                 == 0x024);
    static_assert(offsetof(Agent, z)                  == 0x028);
    static_assert(offsetof(Agent, x)                  == 0x0C0);
    static_assert(offsetof(Agent, y)                  == 0x0C4);
    static_assert(offsetof(Agent, health_percent)     == 0x130);
    static_assert(offsetof(Agent, energy_percent)     == 0x138);
    static_assert(offsetof(Agent, model_id)           == 0x154);
    static_assert(offsetof(Agent, primary_profession) == 0x164);
    static_assert(offsetof(Agent, level)              == 0x166);
    static_assert(sizeof(Agent)                       == 446);
}

void test_skill_struct_offsets() {
    static_assert(offsetof(Skill, id)              == 0x00);
    static_assert(offsetof(Skill, activation_time) == 0x30);
    static_assert(offsetof(Skill, recharge)        == 0x38);
    static_assert(offsetof(Skill, energy_cost)     == 0x3C);
}

void test_item_struct_offsets() {
    static_assert(offsetof(Item, id)       == 0x00);
    static_assert(offsetof(Item, model_id) == 0x20);
    static_assert(offsetof(Item, quantity) == 0x44);
}

void test_frame_struct_offsets() {
    static_assert(offsetof(Frame, callbacks)       == 0x0A8);
    static_assert(offsetof(Frame, child_offset_id) == 0x0B8);
    static_assert(offsetof(Frame, frame_id)        == 0x0BC);
    static_assert(offsetof(Frame, relation)        == 0x128);
    static_assert(offsetof(Frame, frame_hash_id)   == 0x134);
    static_assert(offsetof(Frame, frame_state)     == 0x18C);
    static_assert(sizeof(Frame)                    == 0x1C8);
}
```

These are `static_assert` — they fail at **compile time**, not runtime. If a struct offset is wrong, the build breaks immediately. This is the single most valuable test you can write.

**Packet header constants (GWA3-004)**

```cpp
// tests/test_headers.cpp
#include <gwa3/packets/Headers.h>

void test_header_values() {
    // Cross-reference against GWA2_Headers.au3
    static_assert(Packets::TRADE_PLAYER     == 0x00);
    static_assert(Packets::TRADE_CANCEL     == 0x01);
    static_assert(Packets::MOVE_TO_COORD    == 0x3E);
    static_assert(Packets::USE_SKILL        == 0x46);
    static_assert(Packets::MAP_TRAVEL       == 0xB1);
    static_assert(Packets::TARGET_AGENT     == 0xC1);
    // ... all 100+ headers
}
```

**Pattern parsing (GWA3-003)**

The scanner's pattern-to-bytes conversion can be tested offline:

```cpp
// tests/test_scanner.cpp
void test_pattern_parsing() {
    auto bytes = Scanner::ParsePattern("55 8B EC ?? 6A 00");
    assert(bytes.data[0] == 0x55);
    assert(bytes.data[1] == 0x8B);
    assert(bytes.data[2] == 0xEC);
    assert(bytes.mask[3] == '?');  // wildcard
    assert(bytes.data[4] == 0x6A);
    assert(bytes.data[5] == 0x00);
    assert(bytes.length  == 6);
}

void test_function_from_near_call() {
    // E8 xx xx xx xx at address 0x1000 with rel32 = 0x500
    // Target should be 0x1000 + 5 + 0x500 = 0x1505
    uint8_t code[] = { 0xE8, 0x00, 0x05, 0x00, 0x00 };
    auto target = Scanner::ResolveRelativeCall(
        reinterpret_cast<uintptr_t>(code), code);
    assert(target == reinterpret_cast<uintptr_t>(code) + 5 + 0x500);
}
```

**PE header parsing (GWA3-003)**

If you separate PE parsing from live memory access, you can test it against a sample PE file:

```cpp
void test_pe_section_parsing() {
    // Load any .exe/.dll from disk and parse its sections
    auto sections = Scanner::ParsePESections("test_data/sample.dll");
    assert(sections.text.start != 0);
    assert(sections.text.size > 0);
    assert(sections.rdata.start > sections.text.start);
}
```

**Helper functions (distance, state checks)**

```cpp
void test_distance_calculation() {
    Agent a = {}; a.x = 0; a.y = 0;
    Agent b = {}; b.x = 3; b.y = 4;
    assert(a.DistanceTo(b) == 5.0f);  // 3-4-5 triangle
}

void test_frame_state_flags() {
    Frame f = {};
    f.frame_state = 0x4;  // created only
    assert(f.IsCreated());
    assert(!f.IsHidden());
    assert(!f.IsDisabled());
    assert(f.IsClickable());

    f.frame_state = 0x204;  // created + hidden
    assert(f.IsCreated());
    assert(f.IsHidden());
    assert(!f.IsClickable());
}
```

### Build System Integration

```cmake
# CMakeLists.txt
option(GWA3_BUILD_TESTS "Build offline unit tests" ON)

if(GWA3_BUILD_TESTS)
    add_executable(gwa3_tests
        tests/test_structs.cpp
        tests/test_headers.cpp
        tests/test_scanner.cpp
        tests/test_helpers.cpp
    )
    target_link_libraries(gwa3_tests PRIVATE gwa3_core)

    # Run tests after build
    add_custom_command(TARGET gwa3_tests POST_BUILD
        COMMAND gwa3_tests
        COMMENT "Running offline unit tests..."
    )
endif()
```

**When to start:** Immediately. Write struct offset tests as you write each struct. If the `static_assert` compiles, the offset is correct.

---

## Layer 2: Injection Smoke Tests

These require a running GW.exe but don't send any commands. Read-only validation.

### First Injection Point: End of Phase 1 (GWA3-001 + GWA3-002 + GWA3-003 + GWA3-005)

The **earliest you can inject** is when you have:
1. A DLL that compiles (`GWA3-001`)
2. An injector that loads it (`GWA3-002`)
3. A pattern scanner (`GWA3-003`)
4. Patterns registered (`GWA3-005`)

At this point your DLL doesn't DO anything yet — no hooks, no commands. But you can **read game memory** and validate everything:

```cpp
// src/smoke_test.cpp — runs automatically in InitThread
void RunSmokeTest() {
    Log("=== GWA3 Smoke Test ===");

    // 1. Verify all patterns resolved
    int failures = Offsets::ResolveAll();
    Log("Patterns: %d/%d resolved (%d failures)",
        total - failures, total, failures);
    if (failures > 0) {
        Log("CRITICAL: Pattern scan failures. Aborting.");
        return;
    }

    // 2. Read agent ID (proves BasePointer + AgentBase + MyID work)
    uint32_t my_id = *reinterpret_cast<uint32_t*>(Offsets::MyID);
    Log("My agent ID: %d %s", my_id, my_id > 0 ? "OK" : "FAIL");

    // 3. Read map ID (proves InstanceInfo works)
    uint32_t map_id = MapMgr::GetMapID();
    Log("Map ID: %d %s", map_id, map_id > 0 ? "OK" : "FAIL");

    // 4. Read agent position (proves AgentBase + struct offsets)
    Agent* me = AgentMgr::GetMyAgent();
    if (me) {
        Log("Position: (%.1f, %.1f) HP: %.0f%% %s",
            me->x, me->y, me->health_percent * 100,
            me->health_percent > 0 ? "OK" : "FAIL");
    } else {
        Log("FAIL: Could not read own agent");
    }

    // 5. Read skillbar (proves SkillBase works)
    Skillbar* sb = SkillMgr::GetPlayerSkillbar();
    if (sb) {
        Log("Skillbar: [%d, %d, %d, %d, %d, %d, %d, %d]",
            sb->slots[0].skill_id, sb->slots[1].skill_id,
            sb->slots[2].skill_id, sb->slots[3].skill_id,
            sb->slots[4].skill_id, sb->slots[5].skill_id,
            sb->slots[6].skill_id, sb->slots[7].skill_id);
    }

    // 6. Read inventory (proves item struct offsets)
    Bag* backpack = ItemMgr::GetBag(1);
    if (backpack) {
        Log("Backpack: %d/%d slots used", backpack->item_count, backpack->max_slots);
    }

    // 7. Frame system (proves FrameArray + hash lookup)
    if (Offsets::FrameArray) {
        // Only test if at character select
        Frame* play = UIMgr::GetFrameByHash(UIMgr::Hashes::PlayButton);
        Log("Play button frame: 0x%08X %s",
            play, play ? "FOUND" : "not found (may not be at char select)");
    }

    Log("=== Smoke Test Complete ===");
}
```

**How to run:** Launch GW → get to character select or login → run `injector.exe` → check `gwa3_log.txt` or DebugView output.

### Pattern Health Check Tool (GWA3-035)

A dedicated smoke test that can be run after every GW client update:

```cpp
// tools/pattern_test.cpp
int main() {
    // 1. Find GW process
    // 2. Inject gwa3.dll
    // 3. Wait for smoke test to complete
    // 4. Read results from shared memory or log file
    // 5. Print report
    // 6. Eject DLL
    // 7. Exit with 0 (all pass) or 1 (any fail)
}
```

This is your regression test for GW client updates.

---

## Layer 3: Behavioral Integration Tests

These send commands to the game and verify outcomes. Requires a logged-in character.

### When You Can First Send Commands: End of Phase 2 (GWA3-006 + GWA3-010)

Once the game thread hook and packet sending work, you can test:

```cpp
void RunCommandTests() {
    // WARNING: These affect game state. Only run on a test character.

    // Test 1: Movement
    float start_x = AgentMgr::GetMyAgent()->x;
    float start_y = AgentMgr::GetMyAgent()->y;
    GameThread::Enqueue([=]() {
        CtoS::MoveToCoord(start_x + 200.0f, start_y);
    });
    Sleep(3000);
    float end_x = AgentMgr::GetMyAgent()->x;
    bool moved = abs(end_x - start_x) > 50.0f;
    Log("Movement test: %s (moved %.1f units)", moved ? "PASS" : "FAIL", end_x - start_x);

    // Test 2: Target change
    // Find any nearby NPC
    Agent** agents = AgentMgr::GetAgentArray();
    uint32_t npc_id = 0;
    for (int i = 0; i < AgentMgr::GetMaxAgents(); i++) {
        if (agents[i] && agents[i]->id != AgentMgr::GetMyID() && agents[i]->IsAlive()) {
            npc_id = agents[i]->id;
            break;
        }
    }
    if (npc_id) {
        GameThread::Enqueue([=]() { CtoS::ChangeTarget(npc_id); });
        Sleep(500);
        bool targeted = AgentMgr::GetCurrentTargetID() == npc_id;
        Log("Target test: %s", targeted ? "PASS" : "FAIL");
    }
}
```

### Frame UI Tests: End of Phase 4 (GWA3-020 + GWA3-021)

Must be at character select screen:

```cpp
void RunFrameTests() {
    // Test: Play button visible at character select
    Frame* play = UIMgr::GetFrameByHash(UIMgr::Hashes::PlayButton);
    assert(play != nullptr);
    assert(play->IsClickable());
    Log("Play button: visible=%d clickable=%d PASS", !play->IsHidden(), play->IsClickable());

    // Test: Click play button (CAUTION: will enter game!)
    // Only run if you want to actually log in
    // GameThread::Enqueue([]() { UIMgr::ButtonClickByHash(UIMgr::Hashes::PlayButton); });
}
```

---

## Testing Timeline by Phase

| Phase | When | What You Can Test | Layer |
|-------|------|-------------------|-------|
| **Phase 1A** (CMake) | Day 1 | Struct offsets compile (`static_assert`), header constants | Offline |
| **Phase 1B** (Scanner) | Day 2-3 | Pattern parsing, PE section parsing, near-call resolution | Offline |
| **Phase 1C** (First inject!) | Day 3-4 | **All patterns resolve against live GW** — first real validation | Injection smoke |
| **Phase 2** (GameThread + CtoS) | Day 5-7 | **Character moves, targets change** — first commands work | Behavioral |
| **Phase 3** (Structs) | Day 5-7 (parallel) | Agent/skill/item reads match in-game values | Injection smoke |
| **Phase 4** (Frame UI) | Day 8-10 | Frame hash lookup, ButtonClick at char select | Behavioral |
| **Phase 5** (Bridge) | Day 10-12 | AutoIt `DllCall` returns correct values | Behavioral |
| **Phase 6** (Integration) | Day 12-15 | Full Froggy HM flow | Behavioral |

---

## Automated Test Harness Design

### In-DLL Test Runner

Build a test runner into the DLL itself, triggered by a named event or command-line flag:

```cpp
// src/core/TestRunner.h
namespace GWA3::Tests {
    enum TestLevel {
        SMOKE,      // Read-only, safe always
        COMMANDS,   // Sends packets, needs logged-in char
        FRAMES,     // Clicks UI, needs char select screen
    };

    struct TestResult {
        const char* name;
        bool passed;
        const char* message;
    };

    // Run all tests at given level, return results
    std::vector<TestResult> RunTests(TestLevel level);

    // Log results to file
    void WriteReport(const std::vector<TestResult>& results, const char* path);
}
```

### Trigger Tests from AutoIt

Once the DLL bridge exists (Phase 5), AutoIt can drive tests:

```autoit
; test_gwa3_smoke.au3
#include "GWA3.au3"

GWA3_Init()

; Smoke tests
Local $scanStatus = GWA3_GetScanStatus()
ConsoleWrite("Scan failures: " & $scanStatus & @CRLF)
If $scanStatus > 0 Then Exit 1

Local $myId = GWA3_GetMyID()
ConsoleWrite("Agent ID: " & $myId & @CRLF)
If $myId <= 0 Then Exit 1

Local $mapId = GWA3_GetMapID()
ConsoleWrite("Map ID: " & $mapId & @CRLF)
If $mapId <= 0 Then Exit 1

Local $hp = GWA3_GetAgentHP($myId)
ConsoleWrite("HP: " & $hp & @CRLF)
If $hp <= 0 Or $hp > 1.0 Then Exit 1

ConsoleWrite("ALL SMOKE TESTS PASSED" & @CRLF)
Exit 0
```

### Existing Test Pattern to Follow

The project already has `test_no_gwca.au3` which validates the AutoIt scanning pipeline. The C++ smoke test follows the same pattern:

1. Resolve all scan patterns
2. Read critical game state (agent ID, map ID, HP)
3. Verify frame hashes resolve (at char select)
4. Write report to `tests/` directory
5. Exit with pass/fail code

---

## What to Test at Each Kanban Ticket

| Ticket | Offline Tests | Injection Tests | Behavioral Tests |
|--------|--------------|-----------------|------------------|
| GWA3-001 | Build compiles, DLL exports present | — | — |
| GWA3-002 | — | DLL loads into GW, `DllMain` fires | — |
| GWA3-003 | Pattern parsing, near-call resolution | All patterns resolve | — |
| GWA3-004 | `static_assert` all header values | — | — |
| GWA3-005 | Pattern table completeness | All offsets non-null | — |
| GWA3-006 | — | Hook installed, lambda executes | — |
| GWA3-007 | `static_assert` agent offsets | Read player agent, verify fields | — |
| GWA3-008 | `static_assert` skill offsets | Read skillbar, verify skill IDs | — |
| GWA3-009 | `static_assert` item offsets | Read backpack, verify item data | — |
| GWA3-010 | — | — | Move character, change target |
| GWA3-011..015 | `static_assert` struct offsets | Read map/party/quest/effect/chat | — |
| GWA3-016 | — | — | Move, attack, interact NPC |
| GWA3-017 | — | — | Use skill, load skillbar |
| GWA3-018 | — | — | Move item, use item, salvage |
| GWA3-019 | — | — | Travel to outpost, enter mission |
| GWA3-020 | `static_assert` frame offsets | Frame hash lookup finds known frames | — |
| GWA3-021 | — | — | Click Play button at char select |
| GWA3-022..024 | — | — | Party/quest/chat operations |
| GWA3-025 | `dumpbin /exports` lists all functions | `GetProcAddress` succeeds | DllCall returns valid data |
| GWA3-033 | — | — | 10+ Froggy HM runs |

---

## Summary

| Question | Answer |
|----------|--------|
| **Can we unit test?** | Yes — struct offsets (`static_assert`), header constants, pattern parsing, helper math. All offline, no game needed. |
| **When is first injection?** | End of Phase 1 (~day 3-4). Scanner resolves patterns, reads agent ID. Read-only, no hooks yet. |
| **When can we send commands?** | End of Phase 2 (~day 5-7). GameThread hook + CtoS packets. Character moves. |
| **When can we click UI?** | End of Phase 4 (~day 8-10). Frame system + ButtonClick. Char select works. |
| **What breaks most often?** | Struct offsets (wrong field position = garbage data) and scan patterns (GW update shifts bytes). Both are testable early. |
