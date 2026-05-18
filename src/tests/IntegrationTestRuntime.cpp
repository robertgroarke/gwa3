// Consolidated runtime and small integration feature wrappers.
// --- src/tests/IntegrationTestRuntime.cpp ---
#include "IntegrationTestInternal.h"

#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace GWA3::SmokeTest {

static FILE* s_intReport = nullptr;
static int s_intPassed = 0;
static int s_intFailed = 0;
static int s_intSkipped = 0;

void IntReport(const char* fmt, ...) {
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

void IntCheck(const char* name, bool condition) {
    if (condition) {
        s_intPassed++;
        IntReport("[PASS] %s", name);
    } else {
        s_intFailed++;
        IntReport("[FAIL] %s", name);
    }
}

void IntSkip(const char* name, const char* reason) {
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

void ResetIntegrationCounters() {
    s_intPassed = 0;
    s_intFailed = 0;
    s_intSkipped = 0;
}

void BeginIntegrationRun(const char* title) {
    ResetIntegrationCounters();
    BeginIntegrationReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("%s", title);
    IntReport("Timestamp: %s", timestamp);
    IntReport("");
}

void FinishIntegrationRunSummary() {
    IntReport("");
    IntReport("=== SUMMARY: %d passed, %d failed, %d skipped ===",
              s_intPassed,
              s_intFailed,
              s_intSkipped);
    EndIntegrationReport();
}

void BeginIntegrationReport() {
    if (s_intReport) {
        fclose(s_intReport);
        s_intReport = nullptr;
    }
    s_intReport = OpenIntReport();
}

void EndIntegrationReport() {
    if (s_intReport) {
        fclose(s_intReport);
        s_intReport = nullptr;
    }
}

int GetIntegrationPassedCount() {
    return s_intPassed;
}

int GetIntegrationFailedCount() {
    return s_intFailed;
}

int GetIntegrationSkippedCount() {
    return s_intSkipped;
}

void AddIntegrationFailure() {
    ++s_intFailed;
}
} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestGameplay.cpp ---
// Core gameplay interaction feature groups: movement, targeting, combat, and loot.

#include "IntegrationTestInternal.h"

#include <atomic>
#include <cmath>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

namespace GWA3::SmokeTest {

#include "IntegrationTestGameplaySupport.h"
#include "IntegrationTestGameplayCharSelectLogin.h"
#include "IntegrationTestGameplayHeroSetup.h"
#include "IntegrationTestGameplayMovement.h"
#include "IntegrationTestGameplayTargeting.h"
#include "IntegrationTestGameplaySkillActivation.h"
#include "IntegrationTestGameplayLootPickup.h"

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestIntrospection.cpp ---
// Read-heavy introspection and validation feature groups for player, camera, UI, and inventory state.

#include "IntegrationTestInternal.h"

#include <Windows.h>

#include <gwa3/game/MapIds.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/UIMgr.h>

namespace GWA3::SmokeTest {

#include "IntegrationTestIntrospectionPlayerData.h"
#include "IntegrationTestIntrospectionCamera.h"
#include "IntegrationTestIntrospectionClientInfo.h"
#include "IntegrationTestIntrospectionInventory.h"
#include "IntegrationTestIntrospectionAgentArray.h"
#include "IntegrationTestIntrospectionUIFrame.h"
#include "IntegrationTestIntrospectionAreaInfo.h"

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestOffsets.cpp ---
// Offset and patch validation feature groups.

#include "IntegrationTestInternal.h"

#include <gwa3/core/Memory.h>

namespace GWA3::SmokeTest {

#include "IntegrationTestOffsetsPostProcessEffect.h"
#include "IntegrationTestOffsetsGwEndScene.h"
#include "IntegrationTestOffsetsItemClick.h"
#include "IntegrationTestOffsetsRequestQuestInfo.h"
#include "IntegrationTestOffsetsFriendList.h"
#include "IntegrationTestOffsetsDrawOnCompass.h"
#include "IntegrationTestOffsetsChatColor.h"
#include "IntegrationTestOffsetsCameraUpdateBypass.h"
#include "IntegrationTestOffsetsTrade.h"
#include "IntegrationTestOffsetsLevelDataBypass.h"
#include "IntegrationTestOffsetsMapPortBypass.h"
#include "IntegrationTestOffsetsSendChat.h"
#include "IntegrationTestOffsetsAddToChatLog.h"
#include "IntegrationTestOffsetsSkipCinematic.h"

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestSystems.cpp ---
// Runtime systems coverage: guild, map state, rendering, hooks, and effects.

#include "IntegrationTestInternal.h"

#include <Windows.h>

#include <atomic>
#include <cmath>

#include <gwa3/core/TargetLogHook.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/GuildMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/managers/UIMgr.h>

namespace GWA3::SmokeTest {

#include "IntegrationTestSystemsGuildData.h"
#include "IntegrationTestSystemsMapStateQueries.h"
#include "IntegrationTestSystemsPingStability.h"
#include "IntegrationTestSystemsWeaponSetValidation.h"
#include "IntegrationTestSystemsAgentDistanceCrossCheck.h"
#include "IntegrationTestSystemsCameraControls.h"
#include "IntegrationTestSystemsRenderingToggle.h"
#include "IntegrationTestSystemsEffectArray.h"
#include "IntegrationTestSystemsStoCHook.h"

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestWorld.cpp ---
// World-transition and explorable/outpost interaction feature groups.

#include "IntegrationTestInternal.h"

#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/managers/GuildMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/game/MapIds.h>

#include <cmath>

namespace GWA3::SmokeTest {

#include "IntegrationTestWorldExplorableEntry.h"
#include "IntegrationTestWorldHeroFlagging.h"
#include "IntegrationTestWorldChatWriteLocal.h"
#include "IntegrationTestWorldSkillbarDataValidation.h"
#include "IntegrationTestWorldHardModeToggle.h"
#include "IntegrationTestWorldReturnToOutpost.h"
#include "IntegrationTestWorldPartyState.h"
#include "IntegrationTestWorldTargetLogHook.h"

} // namespace GWA3::SmokeTest
