// Froggy Bogroot boss waypoint handler. Included by FroggyHM.cpp before waypoint traversal.

#include "FroggyHMBossChestSupport.h"
#include "FroggyHMBossDialogHelpers.h"
#include "FroggyHMBossDialogSupport.h"
#include "FroggyHMBossPostRewardSupport.h"

static void HandleBossWaypoint(const Waypoint& wp) {
    s_dungeonLoopTelemetry.boss_started = true;
    AggroMoveToEx(wp.x, wp.y, wp.fight_range);
    WaitMs(BOSS_WAYPOINT_POST_FIGHT_LOOT_DELAY_MS);
    PickupNearbyLoot(BOSS_WAYPOINT_LOOT_RADIUS);

    // AutoIt does GetNearestSignpostToCoords + GoToSignpost twice here.
    if (!HandleBossChestLoot()) {
        return;
    }

    if (!StageBossRewardInteraction()) {
        return;
    }

    const uint32_t tekksId = FindBossRewardNpc();
    if (tekksId) {
        AcceptBossRewardFromNpc(tekksId);
    } else {
        AcceptBossRewardFallback();
    }

    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    s_dungeonLoopTelemetry.reward_dialog_latched =
        s_dungeonLoopTelemetry.last_dialog_id == GWA3::DialogIds::TekksWar::QUEST_REWARD;
    s_dungeonLoopTelemetry.boss_completed = true;

    const bool questRewardAccepted = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR) == nullptr;
    // Run salvage when either the quest cleared OR the reward dialog was latched.
    SalvageBossRewardGoldIfReady(questRewardAccepted);

    const bool returnedToSparkfly = WaitForBossPostRewardReturn(questRewardAccepted);
    s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
    s_dungeonLoopTelemetry.returned_to_sparkfly =
        returnedToSparkfly && s_dungeonLoopTelemetry.final_map_id == MapIds::SPARKFLY_SWAMP;
}
