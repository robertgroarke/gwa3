// Froggy Bogroot boss waypoint handler. Included by FroggyHM.cpp before waypoint traversal.

#include "FroggyHMBossPostRewardSupport.h"

static void HandleBossWaypoint(const Waypoint& wp) {
    s_dungeonLoopTelemetry.boss_started = true;
    AggroMoveToEx(wp.x, wp.y, wp.fight_range);
    WaitMs(BOSS_WAYPOINT_POST_FIGHT_LOOT_DELAY_MS);
    PickupNearbyLoot(BOSS_WAYPOINT_LOOT_RADIUS);

    // AutoIt does GetNearestSignpostToCoords + GoToSignpost twice here.
    DungeonLoot::BossChestLootOptions chestOptions;
    chestOptions.log_prefix = "Froggy";
    chestOptions.first_loot_delay_ms = BOSS_CHEST_FIRST_LOOT_DELAY_MS;
    chestOptions.retry_loot_delay_ms = BOSS_CHEST_RETRY_LOOT_DELAY_MS;
    const auto chestResult = DungeonLoot::OpenBossChestAndLoot(
        BOSS_CHEST_X,
        BOSS_CHEST_Y,
        BOSS_CHEST_OPEN_RADIUS,
        BOSS_CHEST_LOOT_RADIUS,
        &MoveToAndWait,
        &OpenChestAt,
        &PickupNearbyLoot,
        &DungeonRuntime::WaitMs,
        chestOptions);
    s_dungeonLoopTelemetry.chest_attempts += chestResult.open_attempts;
    s_dungeonLoopTelemetry.chest_successes += chestResult.open_successes;
    if (!chestResult.completed) {
        return;
    }

    DungeonQuestRuntime::RewardNpcStageOptions stageOptions;
    stageOptions.log_prefix = "Froggy";
    stageOptions.settle_timeout_ms = BOSS_REWARD_SETTLE_TIMEOUT_MS;
    stageOptions.settle_distance = BOSS_REWARD_SETTLE_DISTANCE;
    stageOptions.is_dead = &IsDead;
    const auto stageResult = DungeonQuestRuntime::StageRewardNpcInteraction(
        GetRewardNpcAnchor(),
        stageOptions);
    if (!stageResult.reached) {
        return;
    }
    s_dungeonLoopTelemetry.reward_attempted = true;

    DungeonQuestRuntime::RewardNpcResolveOptions resolveOptions;
    resolveOptions.local_search_radius = BOSS_REWARD_LOCAL_NPC_SEARCH_RADIUS;
    resolveOptions.log_prefix = "Froggy";
    const uint32_t tekksId = DungeonQuestRuntime::ResolveRewardNpc(
        GetRewardNpcAnchor(),
        resolveOptions).npc_id;
    DungeonQuestRuntime::BossRewardClaimOptions rewardOptions = {};
    rewardOptions.quest_id = GWA3::QuestIds::TEKKS_WAR;
    rewardOptions.reward_dialog_id = GWA3::DialogIds::TekksWar::QUEST_REWARD;
    rewardOptions.npc_move_threshold = BOSS_REWARD_NPC_MOVE_THRESHOLD;
    rewardOptions.interact_target_wait_ms = BOSS_REWARD_INTERACT_TARGET_WAIT_MS;
    rewardOptions.interact_pass_wait_ms = BOSS_REWARD_INTERACT_PASS_WAIT_MS;
    rewardOptions.interact_passes = BOSS_REWARD_INTERACT_PASSES;
    rewardOptions.dialog_dwell_ms = BOSS_REWARD_DIALOG_DWELL_MS;
    rewardOptions.target_settle_ms = BOSS_REWARD_TARGET_SETTLE_MS;
    rewardOptions.advance_max_passes = BOSS_REWARD_ADVANCE_MAX_PASSES;
    rewardOptions.accept_retry_timeout_ms = BOSS_REWARD_ACCEPT_RETRY_TIMEOUT_MS;
    rewardOptions.retry_post_dialog_wait_ms = BOSS_REWARD_RETRY_POST_DIALOG_WAIT_MS;
    rewardOptions.retry_refresh_delay_ms = BOSS_REWARD_RETRY_REFRESH_DELAY_MS;
    rewardOptions.fallback_send_attempts = BOSS_REWARD_FALLBACK_SEND_ATTEMPTS;
    rewardOptions.fallback_send_delay_ms = BOSS_REWARD_FALLBACK_SEND_DELAY_MS;
    rewardOptions.fallback_refresh_delay_ms = BOSS_REWARD_FALLBACK_REFRESH_DELAY_MS;
    rewardOptions.log_prefix = "Froggy";
    rewardOptions.label = "Boss reward";
    (void)DungeonQuestRuntime::ClaimBossReward(tekksId, rewardOptions);

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
