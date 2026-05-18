#include "IntegrationTestInternal.h"
#include <bots/rragars_menagerie/RragarsMenagerie.h>
#include <bots/rragars_menagerie/RragarsMenagerieBot.h>
#include <bots/common/BotFramework.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/packets/CtoS.h>
#include <Windows.h>
#include <ctime>

namespace GWA3::SmokeTest {

int RunRragarsMenagerieFeatureTest() {
    ResetIntegrationCounters();
    StartWatchdog();

    BeginIntegrationReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== RRAGARS MENAGERIE FEATURE TEST ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

    WaitForPlayerWorldReady(15000);
    CtoS::Initialize();

    GWA3::Bot::RragarsMenagerieBot::Register();
    GWA3::Bot::Start();
    const bool botStarted = GWA3::Bot::IsRunning();
    IntCheck("Rragars bot thread started", botStarted);
    if (botStarted) {
        GWA3::Bot::SetState(GWA3::Bot::BotState::InTown);
    }

    bool sawDoomlore = MapMgr::GetMapId() == GWA3::MapIds::DOOMLORE_SHRINE;
    bool sawDalada = MapMgr::GetMapId() == GWA3::MapIds::DALADA_UPLANDS;
    bool sawGrothmar = MapMgr::GetMapId() == GWA3::MapIds::GROTHMAR_WARDOWNS;
    bool sawSacnoth = MapMgr::GetMapId() == GWA3::MapIds::SACNOTH_VALLEY;
    bool sawLevel1 = MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1;
    bool sawLevel2 = MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL2;
    bool sawLevel3 = MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3;
    bool returnedToDoomloreAfterReward = false;
    bool sawErrorState = false;

    uint32_t lastMapId = 0xFFFFFFFFu;
    GWA3::Bot::BotState lastState = GWA3::Bot::BotState::Idle;
    DWORD lastProgressLog = 0u;
    const DWORD start = GetTickCount();

    while (botStarted && (GetTickCount() - start) < 2700000u) {
        const uint32_t mapId = MapMgr::GetMapId();
        const GWA3::Bot::BotState state = GWA3::Bot::GetState();

        sawDoomlore = sawDoomlore || mapId == GWA3::MapIds::DOOMLORE_SHRINE;
        sawDalada = sawDalada || mapId == GWA3::MapIds::DALADA_UPLANDS;
        sawGrothmar = sawGrothmar || mapId == GWA3::MapIds::GROTHMAR_WARDOWNS;
        sawSacnoth = sawSacnoth || mapId == GWA3::MapIds::SACNOTH_VALLEY;
        sawLevel1 = sawLevel1 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1;
        sawLevel2 = sawLevel2 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL2;
        sawLevel3 = sawLevel3 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3;
        returnedToDoomloreAfterReward =
            returnedToDoomloreAfterReward ||
            (sawLevel3 && mapId == GWA3::MapIds::DOOMLORE_SHRINE);
        sawErrorState = sawErrorState || state == GWA3::Bot::BotState::Error;

        const DWORD nowTicks = GetTickCount();
        if (mapId != lastMapId || state != lastState || (nowTicks - lastProgressLog) >= 10000u) {
            IntReport("  Progress: map=%u state=%d heroes=%u elapsed=%us",
                      mapId,
                      static_cast<int>(state),
                      PartyMgr::CountPartyHeroes(),
                      static_cast<unsigned>((nowTicks - start) / 1000u));
            lastMapId = mapId;
            lastState = state;
            lastProgressLog = nowTicks;
        }

        if (ShouldAbortForRuntimeFailure()) {
            const char* reason = RuntimeFailureReason();
            IntReport("[FAIL] Rragars feature test aborted: %s", reason);
            AddIntegrationFailure();
            break;
        }
        if (sawErrorState || returnedToDoomloreAfterReward) {
            break;
        }
        Sleep(1000);
    }

    if (!returnedToDoomloreAfterReward && !sawErrorState && !ShouldAbortForRuntimeFailure() &&
        botStarted && (GetTickCount() - start) >= 2700000u) {
        IntReport("[FAIL] Rragars feature test timed out before returning to Doomlore");
        AddIntegrationFailure();
    }

    IntCheck("Reached Doomlore Shrine", sawDoomlore);
    IntCheck("Reached Dalada Uplands", sawDalada);
    IntCheck("Reached Grothmar Wardowns", sawGrothmar);
    IntCheck("Reached Sacnoth Valley", sawSacnoth);
    IntCheck("Entered Rragars level 1", sawLevel1);
    IntCheck("Entered Rragars level 2", sawLevel2);
    IntCheck("Entered Rragars level 3", sawLevel3);
    IntCheck("Returned to Doomlore after reward chest", returnedToDoomloreAfterReward);
    IntCheck("Bot avoided Error state", !sawErrorState);

    if (GWA3::Bot::IsRunning()) {
        GWA3::Bot::Stop();
    }

    IntReport("");
    IntReport("=== RRAGARS MENAGERIE FEATURE TEST COMPLETE ===");
    IntReport("Passed: %d / Failed: %d / Skipped: %d",
              GetIntegrationPassedCount(), GetIntegrationFailedCount(), GetIntegrationSkippedCount());

    EndIntegrationReport();

    StopWatchdog();
    Log::Info("[INTG] Rragars Menagerie feature complete: %d passed, %d failed, %d skipped",
              GetIntegrationPassedCount(), GetIntegrationFailedCount(), GetIntegrationSkippedCount());
    return GetIntegrationFailedCount();
}

} // namespace GWA3::SmokeTest
