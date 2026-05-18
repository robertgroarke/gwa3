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

#include "BehavioralTestGameThreadEnqueue.h"
#include "BehavioralTestGameThreadIdentity.h"
#include "BehavioralTestMovement.h"
#include "BehavioralTestTargetChange.h"
#include "BehavioralTestPing.h"
#include "BehavioralTestPacketSend.h"
#include "BehavioralTestInstanceTime.h"
#include "BehavioralTestInventory.h"
#include "BehavioralTestSkillbar.h"
#include "BehavioralTestFrameUi.h"
#include "BehavioralTestStability.h"

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

    RunBehavioralGameThreadEnqueueTest();
    RunBehavioralGameThreadIdentityTest();
    RunBehavioralMovementTest();
    RunBehavioralTargetChangeTest(myId);
    RunBehavioralPingTest();
    RunBehavioralPacketSendTest();
    RunBehavioralInstanceTimeTest();
    RunBehavioralInventoryTest();
    RunBehavioralSkillbarTest();
    RunBehavioralFrameUiTest();
    RunBehavioralStabilityTest(mapId);
    // --- Summary ---
    CmdReport("=== SUMMARY: %d passed, %d failed ===", s_cmdPassed, s_cmdFailed);
    CmdReport("Result: %s", s_cmdFailed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");

    fclose(s_cmdReport);
    s_cmdReport = nullptr;

    Log::Info("[CMD-TEST] Complete: %d passed, %d failed", s_cmdPassed, s_cmdFailed);
    return s_cmdFailed;
}

} // namespace GWA3::SmokeTest
