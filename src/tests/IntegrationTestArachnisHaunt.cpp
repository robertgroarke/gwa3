#include "IntegrationTestInternal.h"
#include <bots/arachnis_haunt/ArachnisHaunt.h>
#include <bots/arachnis_haunt/ArachnisHauntBot.h>
#include <bots/common/BotFramework.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/packets/CtoS.h>
#include <Windows.h>
#include <ctime>

namespace GWA3::SmokeTest {

int RunArachnisHauntFeatureTest() {
    ResetIntegrationCounters();
    StartWatchdog();

    BeginIntegrationReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== ARACHNIS HAUNT FEATURE TEST ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

    WaitForPlayerWorldReady(15000);
    CtoS::Initialize();

    if (MapMgr::GetMapId() != GWA3::MapIds::RATA_SUM) {
        IntReport("Forcing clean Arachnis start from Rata Sum (current map=%u)", MapMgr::GetMapId());
        MapMgr::Travel(GWA3::MapIds::RATA_SUM);
        const DWORD travelStart = GetTickCount();
        while (MapMgr::GetMapId() != GWA3::MapIds::RATA_SUM &&
               (GetTickCount() - travelStart) < 60000u) {
            Sleep(500);
        }
        WaitForPlayerWorldReady(30000);
    }
    IntCheck("Started Arachnis test from Rata Sum", MapMgr::GetMapId() == GWA3::MapIds::RATA_SUM);
    if (MapMgr::GetMapId() != GWA3::MapIds::RATA_SUM) {
        EndIntegrationReport();
        StopWatchdog();
        return GetIntegrationFailedCount();
    }

    GWA3::Bot::ArachnisHauntBot::Register();
    GWA3::Bot::Start();
    const bool botStarted = GWA3::Bot::IsRunning();
    IntCheck("Arachnis bot thread started", botStarted);
    if (botStarted) {
        GWA3::Bot::SetState(GWA3::Bot::BotState::InTown);
    }

    bool sawMagusStones = MapMgr::GetMapId() == GWA3::MapIds::MAGUS_STONES;
    bool sawLevel1 = MapMgr::GetMapId() == GWA3::MapIds::ARACHNIS_HAUNT_LVL1;
    bool sawLevel2 = MapMgr::GetMapId() == GWA3::MapIds::ARACHNIS_HAUNT_LVL2;
    bool returnedToMagusAfterReward = false;
    bool sawErrorState = false;
    int level1EntryCount = 0;

    uint32_t lastMapId = 0xFFFFFFFFu;
    GWA3::Bot::BotState lastState = GWA3::Bot::BotState::Idle;
    DWORD lastProgressLog = 0u;
    const DWORD start = GetTickCount();

    constexpr DWORD kArachnisFeatureTimeoutMs = 7200000u;
    while (botStarted && (GetTickCount() - start) < kArachnisFeatureTimeoutMs) {
        const uint32_t mapId = MapMgr::GetMapId();
        const GWA3::Bot::BotState state = GWA3::Bot::GetState();

        sawMagusStones = sawMagusStones || mapId == GWA3::MapIds::MAGUS_STONES;
        sawLevel1 = sawLevel1 || mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1;
        sawLevel2 = sawLevel2 || mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL2;
        if (lastMapId != mapId && mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1) {
            ++level1EntryCount;
        }
        if (sawLevel2 && mapId == GWA3::MapIds::MAGUS_STONES) {
            returnedToMagusAfterReward = true;
        }
        sawErrorState = sawErrorState || state == GWA3::Bot::BotState::Error;

        const DWORD nowTicks = GetTickCount();
        if (mapId != lastMapId || state != lastState || (nowTicks - lastProgressLog) >= 10000u) {
            IntReport("  Progress: map=%u state=%d heroes=%u lvl1Entries=%d elapsed=%us",
                      mapId,
                      static_cast<int>(state),
                      PartyMgr::CountPartyHeroes(),
                      level1EntryCount,
                      static_cast<unsigned>((nowTicks - start) / 1000u));
            lastMapId = mapId;
            lastState = state;
            lastProgressLog = nowTicks;
        }

        if (ShouldAbortForRuntimeFailure()) {
            const char* reason = RuntimeFailureReason();
            IntReport("[FAIL] Arachnis feature test aborted: %s", reason);
            AddIntegrationFailure();
            break;
        }
        if (sawErrorState || returnedToMagusAfterReward) {
            break;
        }
        Sleep(1000);
    }

    if (!returnedToMagusAfterReward && !sawErrorState && !ShouldAbortForRuntimeFailure() &&
        botStarted && (GetTickCount() - start) >= kArachnisFeatureTimeoutMs) {
        IntReport("[FAIL] Arachnis feature test timed out before returning to Magus Stones");
        AddIntegrationFailure();
    }

    IntCheck("Reached Magus Stones", sawMagusStones);
    IntCheck("Entered Arachnis level 1", sawLevel1);
    IntCheck("Entered Arachnis level 1 twice for reward bounce", level1EntryCount >= 2);
    IntCheck("Entered Arachnis level 2", sawLevel2);
    IntCheck("Returned to Magus Stones after reward hand-in", returnedToMagusAfterReward);
    IntCheck("Bot avoided Error state", !sawErrorState);

    if (GWA3::Bot::IsRunning()) {
        GWA3::Bot::Stop();
    }

    IntReport("");
    IntReport("=== ARACHNIS HAUNT FEATURE TEST COMPLETE ===");
    IntReport("Passed: %d / Failed: %d / Skipped: %d",
              GetIntegrationPassedCount(), GetIntegrationFailedCount(), GetIntegrationSkippedCount());

    EndIntegrationReport();

    StopWatchdog();
    Log::Info("[INTG] Arachnis Haunt feature complete: %d passed, %d failed, %d skipped",
              GetIntegrationPassedCount(), GetIntegrationFailedCount(), GetIntegrationSkippedCount());
    return GetIntegrationFailedCount();
}

} // namespace GWA3::SmokeTest
