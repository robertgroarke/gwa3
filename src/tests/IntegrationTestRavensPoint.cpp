#include "IntegrationTestInternal.h"
#include <bots/ravens_point/RavensPoint.h>
#include <bots/ravens_point/RavensPointBot.h>
#include <bots/common/BotFramework.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/packets/CtoS.h>
#include <Windows.h>
#include <ctime>

namespace GWA3::SmokeTest {

int RunRavensPointFeatureTest() {
    ResetIntegrationCounters();
    StartWatchdog();

    BeginIntegrationReport();

    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    IntReport("=== RAVENS POINT FEATURE TEST ===");
    IntReport("Timestamp: %s", timestamp);
    IntReport("");

    WaitForPlayerWorldReady(15000);
    CtoS::Initialize();

    GWA3::Bot::RavensPointBot::Register();
    GWA3::Bot::Start();
    const bool botStarted = GWA3::Bot::IsRunning();
    IntCheck("Raven bot thread started", botStarted);
    if (botStarted) {
        GWA3::Bot::SetState(GWA3::Bot::BotState::InTown);
    }

    bool sawOlafstead = MapMgr::GetMapId() == GWA3::MapIds::OLAFSTEAD;
    bool sawVarajar = MapMgr::GetMapId() == GWA3::MapIds::VARAJAR_FELLS_1;
    bool sawLevel1 = MapMgr::GetMapId() == GWA3::MapIds::RAVENS_POINT_LVL1;
    bool sawLevel2 = MapMgr::GetMapId() == GWA3::MapIds::RAVENS_POINT_LVL2;
    bool sawLevel3 = MapMgr::GetMapId() == GWA3::MapIds::RAVENS_POINT_LVL3;
    bool returnedToVarajarAfterLevel3 = false;
    bool sawErrorState = false;

    uint32_t lastMapId = 0xFFFFFFFFu;
    GWA3::Bot::BotState lastState = GWA3::Bot::BotState::Idle;
    DWORD lastProgressLog = 0u;
    const DWORD start = GetTickCount();

    while (botStarted && (GetTickCount() - start) < 3600000u) {
        const uint32_t mapId = MapMgr::GetMapId();
        const GWA3::Bot::BotState state = GWA3::Bot::GetState();

        sawOlafstead = sawOlafstead || mapId == GWA3::MapIds::OLAFSTEAD;
        sawVarajar = sawVarajar || mapId == GWA3::MapIds::VARAJAR_FELLS_1;
        sawLevel1 = sawLevel1 || mapId == GWA3::MapIds::RAVENS_POINT_LVL1;
        sawLevel2 = sawLevel2 || mapId == GWA3::MapIds::RAVENS_POINT_LVL2;
        sawLevel3 = sawLevel3 || mapId == GWA3::MapIds::RAVENS_POINT_LVL3;
        returnedToVarajarAfterLevel3 = returnedToVarajarAfterLevel3 ||
                                       (sawLevel3 && mapId == GWA3::MapIds::VARAJAR_FELLS_1);
        sawErrorState = sawErrorState || state == GWA3::Bot::BotState::Error;

        const DWORD nowTicks = GetTickCount();
        if (mapId != lastMapId || state != lastState || (nowTicks - lastProgressLog) >= 10000u) {
            const auto* ravenQuest = QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT);
            IntReport("  Progress: map=%u state=%d heroes=%u activeQuest=0x%X ravenPresent=%d ravenLogState=%u level2=%d level3=%d returned=%d elapsed=%us",
                      mapId,
                      static_cast<int>(state),
                      PartyMgr::CountPartyHeroes(),
                      QuestMgr::GetActiveQuestId(),
                      ravenQuest != nullptr ? 1 : 0,
                      ravenQuest ? ravenQuest->log_state : 0u,
                      sawLevel2 ? 1 : 0,
                      sawLevel3 ? 1 : 0,
                      returnedToVarajarAfterLevel3 ? 1 : 0,
                      static_cast<unsigned>((nowTicks - start) / 1000u));
            lastMapId = mapId;
            lastState = state;
            lastProgressLog = nowTicks;
        }

        if (ShouldAbortForRuntimeFailure()) {
            const char* reason = RuntimeFailureReason();
            IntReport("[FAIL] Raven feature test aborted: %s", reason);
            AddIntegrationFailure();
            break;
        }
        if (sawErrorState || returnedToVarajarAfterLevel3) {
            break;
        }
        Sleep(1000);
    }

    if (!returnedToVarajarAfterLevel3 && !sawErrorState && !ShouldAbortForRuntimeFailure() &&
        botStarted && (GetTickCount() - start) >= 3600000u) {
        IntReport("[FAIL] Raven feature test timed out before completing and returning to Varajar");
        AddIntegrationFailure();
    }

    IntCheck("Reached Olafstead", sawOlafstead);
    IntCheck("Reached Varajar Fells", sawVarajar);
    IntCheck("Entered Ravens Point level 1", sawLevel1);
    IntCheck("Entered Ravens Point level 2", sawLevel2);
    IntCheck("Entered Ravens Point level 3", sawLevel3);
    IntCheck("Returned to Varajar after Raven clear", returnedToVarajarAfterLevel3);
    IntCheck("Bot avoided Error state", !sawErrorState);

    if (GWA3::Bot::IsRunning()) {
        GWA3::Bot::Stop();
    }

    IntReport("");
    IntReport("=== RAVENS POINT FEATURE TEST COMPLETE ===");
    IntReport("Passed: %d / Failed: %d / Skipped: %d",
              GetIntegrationPassedCount(), GetIntegrationFailedCount(), GetIntegrationSkippedCount());

    EndIntegrationReport();

    StopWatchdog();
    Log::Info("[INTG] Ravens Point feature complete: %d passed, %d failed, %d skipped",
              GetIntegrationPassedCount(), GetIntegrationFailedCount(), GetIntegrationSkippedCount());
    return GetIntegrationFailedCount();
}

} // namespace GWA3::SmokeTest
