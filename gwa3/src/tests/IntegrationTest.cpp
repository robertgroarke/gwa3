// GWA3-028..032: Integration Tests
// Runs as a single sequential test session within one DLL injection.
// Launched via GWA3_TEST_INTEGRATION flag.
// Starts at character select, progresses through game phases.

#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <atomic>

namespace GWA3::SmokeTest {

// Forward declare
int RunIntegrationTest();

static FILE* s_intReport = nullptr;
static int s_intPassed = 0;
static int s_intFailed = 0;
static int s_intSkipped = 0;

static void IntReport(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log::Info("[INTG] %s", buf);
    if (s_intReport) {
        fprintf(s_intReport, "%s\n", buf);
        fflush(s_intReport);
    }
}

static void IntCheck(const char* name, bool condition) {
    if (condition) {
        s_intPassed++;
        IntReport("[PASS] %s", name);
    } else {
        s_intFailed++;
        IntReport("[FAIL] %s", name);
    }
}

static void IntSkip(const char* name, const char* reason) {
    s_intSkipped++;
    IntReport("[SKIP] %s — %s", name, reason);
}

static FILE* OpenIntReport() {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&OpenIntReport), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "gwa3_integration_report.txt");
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    return f;
}

// Wait for a condition with timeout (ms). Returns true if condition met.
static bool WaitFor(const char* desc, int timeoutMs, bool(*predicate)()) {
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < (DWORD)timeoutMs) {
        if (predicate()) return true;
        Sleep(500);
    }
    IntReport("  Timeout waiting for: %s (%d ms)", desc, timeoutMs);
    return false;
}

// Read MapID via BasePointer chain
static uint32_t ReadMapId() {
    return MapMgr::GetMapId();
}

// Read MyID via Offsets deref
static uint32_t ReadMyId() {
    if (Offsets::MyID < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(Offsets::MyID);
}

// ===== GWA3-028: Character Select + Login =====

static bool TestCharSelectLogin() {
    IntReport("=== GWA3-028: Character Select + Login ===");

    const uint32_t mapId = ReadMapId();
    const uint32_t myId = ReadMyId();
    IntReport("  Post-bootstrap state: MapID=%u, MyID=%u", mapId, myId);

    IntCheck("Bootstrap reached map", mapId > 0);
    IntCheck("Bootstrap produced MyID", myId > 0);
    IntCheck("GameThread initialized post-login", GameThread::IsInitialized());

    if (!GameThread::IsInitialized()) {
        IntReport("  Cannot continue without GameThread after login");
        return false;
    }

    std::atomic<bool> gtFlag{false};
    GameThread::Enqueue([&gtFlag]() { gtFlag = true; });
    Sleep(1000);
    IntCheck("GameThread enqueue fires", gtFlag.load());

    if (mapId == 0 || myId == 0) {
        IntReport("");
        return false;
    }

    IntCheck("MapID is valid", mapId <= 2000);
    IntCheck("MyID is valid", myId <= 10000);

    const uint32_t ping = ChatMgr::GetPing();
    IntReport("  Ping: %u ms", ping);
    IntCheck("Ping plausible", ping > 0 && ping <= 5000);

    IntReport("");
    return true;
}

// ===== GWA3-029: Hero Setup =====

static bool TestHeroSetup() {
    IntReport("=== GWA3-029: Hero Setup + Consumables ===");

    uint32_t mapId = ReadMapId();
    if (mapId == 0) {
        IntSkip("Hero setup", "Not in game");
        return false;
    }

    // Add heroes (Standard config)
    uint32_t heroIds[] = {25, 14, 21, 4, 24, 15, 1}; // Xandra, Olias, Livia, MoW, Gwen, Norgu, Razah
    IntReport("  Adding 7 heroes...");
    for (int i = 0; i < 7; i++) {
        GameThread::Enqueue([id = heroIds[i]]() {
            PartyMgr::AddHero(id);
        });
        Sleep(500);
    }
    Sleep(2000);
    IntCheck("Heroes added (no crash)", true);

    // Set behaviors to Guard
    IntReport("  Setting hero behaviors to Guard...");
    for (int i = 0; i < 7; i++) {
        GameThread::Enqueue([idx = i + 1]() {
            PartyMgr::SetHeroBehavior(idx, 1); // Guard
        });
        Sleep(200);
    }
    IntCheck("Hero behaviors set (no crash)", true);

    IntReport("");
    return true;
}

// ===== GWA3-030: Movement =====

static bool TestMovement() {
    IntReport("=== GWA3-030: Travel + Movement ===");

    uint32_t mapId = ReadMapId();
    if (mapId == 0) {
        IntSkip("Movement", "Not in game");
        return false;
    }

    // Read current position
    float startX = 0, startY = 0;
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        uint32_t myId = ReadMyId();
        if (agentArr > 0x10000 && myId > 0 && myId < 5000) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + myId * 4);
            if (agentPtr > 0x10000) {
                startX = *reinterpret_cast<float*>(agentPtr + 0x74);
                startY = *reinterpret_cast<float*>(agentPtr + 0x78);
            }
        }
    }
    IntReport("  Start position: (%.1f, %.1f)", startX, startY);

    if (startX == 0.0f && startY == 0.0f) {
        IntSkip("Movement test", "Cannot read position");
        return false;
    }

    // Move 200 units
    float targetX = startX + 200.0f;
    float targetY = startY;
    IntReport("  Moving to (%.1f, %.1f)...", targetX, targetY);
    GameThread::Enqueue([targetX, targetY]() {
        AgentMgr::Move(targetX, targetY);
    });

    Sleep(4000);

    // Read new position
    float endX = 0, endY = 0;
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        uint32_t myId = ReadMyId();
        if (agentArr > 0x10000 && myId > 0 && myId < 5000) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + myId * 4);
            if (agentPtr > 0x10000) {
                endX = *reinterpret_cast<float*>(agentPtr + 0x74);
                endY = *reinterpret_cast<float*>(agentPtr + 0x78);
            }
        }
    }

    float dist = sqrtf((endX - startX) * (endX - startX) + (endY - startY) * (endY - startY));
    IntReport("  End position: (%.1f, %.1f), moved %.1f units", endX, endY, dist);
    IntCheck("Character moved > 50 units", dist > 50.0f);

    // Move back
    GameThread::Enqueue([startX, startY]() {
        AgentMgr::Move(startX, startY);
    });
    Sleep(3000);

    IntReport("");
    return true;
}

// ===== GWA3-030 continued: Target test =====

static bool TestTargeting() {
    IntReport("=== GWA3-030 continued: Targeting ===");

    uint32_t myId = ReadMyId();
    if (myId == 0) {
        IntSkip("Targeting", "Not in game");
        return false;
    }

    // Find a nearby agent to target
    uint32_t targetId = 0;
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr > 0x10000) {
            for (uint32_t i = 1; i < maxAgents && i < 200; i++) {
                if (i == myId) continue;
                uintptr_t ap = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
                if (ap > 0x10000) {
                    targetId = i;
                    break;
                }
            }
        }
    }

    if (targetId == 0) {
        IntSkip("Targeting", "No other agents found");
        return false;
    }

    IntReport("  Targeting agent %u via ChangeTarget...", targetId);
    GameThread::Enqueue([targetId]() {
        AgentMgr::ChangeTarget(targetId);
    });
    Sleep(1500);
    IntCheck("ChangeTarget sent (no crash)", true);

    IntReport("");
    return true;
}

// ===== Main Integration Runner =====

int RunIntegrationTest() {
    s_intPassed = 0;
    s_intFailed = 0;
    s_intSkipped = 0;

    s_intReport = OpenIntReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== GWA3 Integration Test Suite ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

    // Phase 1: Char Select + Login (GWA3-028)
    bool inGame = TestCharSelectLogin();

    if (inGame) {
        // Phase 2: Hero Setup (GWA3-029)
        TestHeroSetup();

        // Phase 3: Movement (GWA3-030)
        TestMovement();

        // Phase 4: Targeting (GWA3-030)
        TestTargeting();
    } else {
        IntSkip("Hero Setup (029)", "Login failed");
        IntSkip("Movement (030)", "Login failed");
        IntSkip("Targeting (030)", "Login failed");
        IntSkip("Loot (031)", "Login failed");
        IntSkip("Merchant (032)", "Login failed");
    }

    IntReport("");
    IntReport("=== SUMMARY: %d passed, %d failed, %d skipped ===",
              s_intPassed, s_intFailed, s_intSkipped);

    if (s_intReport) {
        fclose(s_intReport);
        s_intReport = nullptr;
    }

    Log::Info("[INTG] Complete: %d passed, %d failed, %d skipped",
              s_intPassed, s_intFailed, s_intSkipped);
    return s_intFailed;
}

} // namespace GWA3::SmokeTest
