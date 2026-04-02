#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>
#include <cstdio>
#include <ctime>
#include <atomic>
#include <cmath>

namespace GWA3::SmokeTest {

// Forward declare — defined in SmokeTest.h
int RunBehavioralTest();

static FILE* s_cmdReport = nullptr;
static int s_cmdPassed = 0;
static int s_cmdFailed = 0;

static void CmdReport(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Log::Info("[CMD-TEST] %s", buf);
    if (s_cmdReport) {
        fprintf(s_cmdReport, "%s\n", buf);
        fflush(s_cmdReport);
    }
}

static void CmdCheck(const char* name, bool condition) {
    if (condition) {
        s_cmdPassed++;
        CmdReport("[PASS] %s", name);
    } else {
        s_cmdFailed++;
        CmdReport("[FAIL] %s", name);
    }
}

static FILE* OpenCmdReport() {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&OpenCmdReport), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "gwa3_command_report.txt");

    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    return f;
}

int RunBehavioralTest() {
    s_cmdPassed = 0;
    s_cmdFailed = 0;

    s_cmdReport = OpenCmdReport();
    if (!s_cmdReport) {
        Log::Error("[CMD-TEST] Cannot create report file");
        return -1;
    }

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    CmdReport("=== GWA3 Behavioral Test: Commands + Game Thread ===");
    CmdReport("Timestamp: %s", timestamp);
    CmdReport("");

    // --- Pre-check: are we logged in? ---
    uint32_t mapId = MapMgr::GetMapId();
    uint32_t myId = AgentMgr::GetMyId();
    CmdReport("Map ID: %u, My Agent ID: %u", mapId, myId);

    if (mapId == 0 || myId == 0) {
        CmdReport("[SKIP] Not logged in (at char select or loading). Behavioral tests require a logged-in character.");
        CmdReport("=== SUMMARY: %d passed, %d failed (SKIPPED — not in game) ===", s_cmdPassed, s_cmdFailed);
        fclose(s_cmdReport);
        s_cmdReport = nullptr;
        return 0; // Not a failure — just can't run these tests
    }

    // --- Test 1: Game Thread Enqueue ---
    CmdReport("--- Test 1: Game Thread Enqueue ---");
    {
        std::atomic<bool> flag{false};
        GameThread::Enqueue([&flag]() {
            flag = true;
        });
        Sleep(2000); // Wait for next frame tick
        CmdCheck("GameThread::Enqueue fires callback", flag.load());
    }
    CmdReport("");

    // --- Test 2: IsOnGameThread ---
    CmdReport("--- Test 2: IsOnGameThread ---");
    {
        std::atomic<bool> wasOnGameThread{false};
        GameThread::Enqueue([&wasOnGameThread]() {
            wasOnGameThread = GameThread::IsOnGameThread();
        });
        Sleep(1000);
        CmdCheck("IsOnGameThread() true inside Enqueue", wasOnGameThread.load());
        CmdCheck("IsOnGameThread() false from init thread", !GameThread::IsOnGameThread());
    }
    CmdReport("");

    // --- Test 3: Movement ---
    CmdReport("--- Test 3: Movement ---");
    {
        auto* me = AgentMgr::GetMyAgent();
        if (me) {
            float startX = me->x;
            float startY = me->y;
            CmdReport("Start position: (%.1f, %.1f)", startX, startY);

            // Move 200 units in X direction
            CtoS::MoveToCoord(startX + 200.0f, startY);
            Sleep(3000);

            me = AgentMgr::GetMyAgent(); // re-read
            if (me) {
                float endX = me->x;
                float endY = me->y;
                float dist = sqrtf((endX - startX) * (endX - startX) + (endY - startY) * (endY - startY));
                CmdReport("End position: (%.1f, %.1f), moved %.1f units", endX, endY, dist);
                CmdCheck("Character moved > 50 units", dist > 50.0f);
            } else {
                CmdCheck("Agent readable after move", false);
            }

            // Move back
            CtoS::MoveToCoord(startX, startY);
            Sleep(2000);
        } else {
            CmdReport("[SKIP] No player agent — cannot test movement");
        }
    }
    CmdReport("");

    // --- Test 4: Target Change ---
    CmdReport("--- Test 4: Target Change ---");
    {
        // Find any non-self agent nearby
        uint32_t targetId = 0;
        uint32_t maxAgents = AgentMgr::GetMaxAgents();
        for (uint32_t i = 1; i < maxAgents && i < 200; i++) {
            if (i == myId) continue;
            auto* a = AgentMgr::GetAgentByID(i);
            if (!a || a->agent_id == 0) continue;
            targetId = a->agent_id;
            break;
        }

        if (targetId != 0) {
            CmdReport("Targeting agent %u", targetId);
            AgentMgr::ChangeTarget(targetId);
            Sleep(1000);

            uint32_t currentTarget = AgentMgr::GetTargetId();
            CmdReport("Current target after change: %u", currentTarget);
            CmdCheck("Target changed to requested agent", currentTarget == targetId);
        } else {
            CmdReport("[SKIP] No other agents found to target");
        }
    }
    CmdReport("");

    // --- Test 5: Ping Read ---
    CmdReport("--- Test 5: Ping ---");
    {
        uint32_t ping = ChatMgr::GetPing();
        CmdReport("Ping: %u ms", ping);
        CmdCheck("Ping in plausible range (0-2000)", ping <= 2000);
    }
    CmdReport("");

    // --- Test 6: Packet Send (benign) ---
    CmdReport("--- Test 6: Benign Packet Send ---");
    {
        // Send a heartbeat — this is completely safe
        CtoS::SendPacket(1, 0x0A); // HEARTBEAT
        Sleep(1000);
        CmdCheck("Heartbeat packet sent without crash", true);
    }
    CmdReport("");

    // --- Test 7: Map Instance Time ---
    CmdReport("--- Test 7: Instance Time ---");
    {
        uint32_t time1 = MapMgr::GetInstanceTime();
        Sleep(1500);
        uint32_t time2 = MapMgr::GetInstanceTime();
        CmdReport("Instance time: %u -> %u", time1, time2);
        CmdCheck("Instance time is increasing", time2 > time1);
    }
    CmdReport("");

    // --- Test 8: Inventory Access ---
    CmdReport("--- Test 8: Inventory ---");
    {
        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            uint32_t gold = inv->gold_character;
            CmdReport("Character gold: %u", gold);
            CmdCheck("Inventory readable", true);
            CmdCheck("Gold is plausible (< 1M)", gold < 1000000);

            // Count items in backpack (bag index 1)
            Bag* backpack = ItemMgr::GetBag(1);
            if (backpack) {
                CmdReport("Backpack slots: %u items", backpack->items_count);
                CmdCheck("Backpack accessible", true);
            }
        } else {
            CmdReport("[SKIP] Inventory not accessible");
        }
    }
    CmdReport("");

    // --- Test 9: Skillbar Read ---
    CmdReport("--- Test 9: Skillbar ---");
    {
        Skillbar* bar = SkillMgr::GetPlayerSkillbar();
        if (bar) {
            bool hasAnySkill = false;
            for (int i = 0; i < 8; i++) {
                if (bar->skills[i].skill_id > 0) {
                    hasAnySkill = true;
                    CmdReport("  Slot %d: skill %u", i, bar->skills[i].skill_id);
                }
            }
            CmdCheck("Skillbar readable", true);
            CmdCheck("At least one skill equipped", hasAnySkill);
        } else {
            CmdReport("[SKIP] Skillbar not accessible");
        }
    }
    CmdReport("");

    // --- Test 10: Frame UI (read-only) ---
    CmdReport("--- Test 10: Frame UI ---");
    {
        // These lookups are safe even in-game (frames may not exist though)
        uintptr_t reconnect = UIMgr::GetFrameByHash(UIMgr::Hashes::ReconnectYes);
        bool reconnectVisible = UIMgr::IsFrameVisible(UIMgr::Hashes::ReconnectYes);
        CmdReport("ReconnectYes frame: 0x%08X, visible: %s", reconnect, reconnectVisible ? "yes" : "no");
        CmdCheck("Frame lookup does not crash", true);

        // Check frame array is populated
        uintptr_t root = UIMgr::GetRootFrame();
        CmdReport("Root frame: 0x%08X", root);
        CmdCheck("Root frame is non-null", root != 0);
    }
    CmdReport("");

    // --- Test 11: Game stable after all tests ---
    CmdReport("--- Test 11: Stability ---");
    {
        CmdReport("Waiting 5 seconds for stability check...");
        Sleep(5000);
        auto* meAfter = AgentMgr::GetMyAgent();
        CmdCheck("Agent still readable after tests", meAfter != nullptr);
        uint32_t mapAfter = MapMgr::GetMapId();
        CmdCheck("Still on same map", mapAfter == mapId);
    }
    CmdReport("");

    // --- Summary ---
    CmdReport("=== SUMMARY: %d passed, %d failed ===", s_cmdPassed, s_cmdFailed);
    CmdReport("Result: %s", s_cmdFailed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");

    fclose(s_cmdReport);
    s_cmdReport = nullptr;

    Log::Info("[CMD-TEST] Complete: %d passed, %d failed", s_cmdPassed, s_cmdFailed);
    return s_cmdFailed;
}

} // namespace GWA3::SmokeTest
