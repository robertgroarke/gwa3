// Froggy Bogroot boss waypoint handler. Included by FroggyHM.cpp before waypoint traversal.

static void MarkFroggyBossStarted(void*) {
    s_dungeonLoopTelemetry.boss_started = true;
}

static void ApplyFroggyBossResult(void*, const DungeonQuestRuntime::BossCompletionResult& result) {
    s_dungeonLoopTelemetry.chest_attempts += result.chest.open_attempts;
    s_dungeonLoopTelemetry.chest_successes += result.chest.open_successes;
    s_dungeonLoopTelemetry.reward_attempted = result.reward_attempted;
    s_dungeonLoopTelemetry.last_dialog_id = result.last_dialog_id;
    s_dungeonLoopTelemetry.reward_dialog_latched = result.reward_dialog_latched;
    s_dungeonLoopTelemetry.boss_completed = result.boss_completed;
    s_dungeonLoopTelemetry.final_map_id = result.final_map_id;
    s_dungeonLoopTelemetry.returned_to_sparkfly =
        result.post_reward.returned_expected_map &&
        s_dungeonLoopTelemetry.final_map_id == MapIds::SPARKFLY_SWAMP;
}

static DungeonQuestRuntime::BossCompletionOptions MakeFroggyBossCompletionOptions() {
    DungeonQuestRuntime::BossCompletionOptions options = {};
    options.aggro_move_to = &AggroMoveToEx;
    options.pickup_nearby_loot = &PickupNearbyLoot;
    options.move_to_point = &MoveToAndWait;
    options.open_chest_at = &OpenChestAt;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.salvage_reward_items = &MaintenanceMgr::IdentifyAndSalvageGoldItems;
    options.post_fight_loot_radius = BOSS_WAYPOINT_LOOT_RADIUS;
    options.post_fight_loot_delay_ms = BOSS_WAYPOINT_POST_FIGHT_LOOT_DELAY_MS;
    options.chest_x = BOSS_CHEST_X;
    options.chest_y = BOSS_CHEST_Y;
    options.chest_open_radius = BOSS_CHEST_OPEN_RADIUS;
    options.chest_loot_radius = BOSS_CHEST_LOOT_RADIUS;
    options.chest.first_loot_delay_ms = BOSS_CHEST_FIRST_LOOT_DELAY_MS;
    options.chest.retry_loot_delay_ms = BOSS_CHEST_RETRY_LOOT_DELAY_MS;
    options.reward_npc = GetRewardNpcAnchor();
    options.reward_stage.settle_timeout_ms = BOSS_REWARD_SETTLE_TIMEOUT_MS;
    options.reward_stage.settle_distance = BOSS_REWARD_SETTLE_DISTANCE;
    options.reward_stage.is_dead = &IsDead;
    options.reward_resolve.local_search_radius = BOSS_REWARD_LOCAL_NPC_SEARCH_RADIUS;
    options.reward_claim.quest_id = GWA3::QuestIds::TEKKS_WAR;
    options.reward_claim.reward_dialog_id = GWA3::DialogIds::TekksWar::QUEST_REWARD;
    options.reward_claim.npc_move_threshold = BOSS_REWARD_NPC_MOVE_THRESHOLD;
    options.reward_claim.interact_target_wait_ms = BOSS_REWARD_INTERACT_TARGET_WAIT_MS;
    options.reward_claim.interact_pass_wait_ms = BOSS_REWARD_INTERACT_PASS_WAIT_MS;
    options.reward_claim.interact_passes = BOSS_REWARD_INTERACT_PASSES;
    options.reward_claim.dialog_dwell_ms = BOSS_REWARD_DIALOG_DWELL_MS;
    options.reward_claim.target_settle_ms = BOSS_REWARD_TARGET_SETTLE_MS;
    options.reward_claim.advance_max_passes = BOSS_REWARD_ADVANCE_MAX_PASSES;
    options.reward_claim.accept_retry_timeout_ms = BOSS_REWARD_ACCEPT_RETRY_TIMEOUT_MS;
    options.reward_claim.retry_post_dialog_wait_ms = BOSS_REWARD_RETRY_POST_DIALOG_WAIT_MS;
    options.reward_claim.retry_refresh_delay_ms = BOSS_REWARD_RETRY_REFRESH_DELAY_MS;
    options.reward_claim.fallback_send_attempts = BOSS_REWARD_FALLBACK_SEND_ATTEMPTS;
    options.reward_claim.fallback_send_delay_ms = BOSS_REWARD_FALLBACK_SEND_DELAY_MS;
    options.reward_claim.fallback_refresh_delay_ms = BOSS_REWARD_FALLBACK_REFRESH_DELAY_MS;
    options.post_reward.expected_return_map_id = MapIds::SPARKFLY_SWAMP;
    options.post_reward.fallback_recovery_map_id = MapIds::GADDS_ENCAMPMENT;
    options.post_reward.long_transition_timeout_ms = BOSS_POST_REWARD_LONG_TOTAL_WAIT_MS;
    options.post_reward.short_transition_timeout_ms = BOSS_POST_REWARD_SHORT_TOTAL_WAIT_MS;
    options.post_reward.long_load_timeout_ms = BOSS_POST_REWARD_LONG_BOGROOT_WAIT_MS;
    options.post_reward.short_load_timeout_ms = BOSS_POST_REWARD_SHORT_BOGROOT_WAIT_MS;
    options.post_reward.dungeon_map_ids = BOGROOT_DUNGEON_MAPS;
    options.post_reward.dungeon_map_count = BOGROOT_DUNGEON_MAP_COUNT;
    options.post_reward.label = "Boss post-reward";
    options.log_prefix = "Froggy";
    options.label = "Boss reward";
    return options;
}

static void HandleBossWaypoint(const Waypoint& wp) {
    DungeonQuestRuntime::BossWaypointOptions options = {};
    options.completion = MakeFroggyBossCompletionOptions();
    options.on_started = &MarkFroggyBossStarted;
    options.on_result = &ApplyFroggyBossResult;

    (void)DungeonQuestRuntime::ExecuteBossWaypoint(
        wp.x,
        wp.y,
        wp.fight_range,
        options);
}
