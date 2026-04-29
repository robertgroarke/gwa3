// Froggy Bogroot dungeon-loop runtime. Used by both the bot state machine and
// debug/test entrypoints so live Froggy follows the same route behavior that
// integration runs validate.

void ResetDungeonLoopTelemetry() {
    s_dungeonLoopTelemetry = {};
    ResetOpenedChestTracker("dungeon-loop-start");
}

DungeonLoopTelemetry GetDungeonLoopTelemetry() {
    s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    return s_dungeonLoopTelemetry;
}

static bool PrepareTekksDungeonEntryForLoop(const char*) {
    return PrepareTekksDungeonEntry();
}

static bool EnterBogrootFromSparkflyForLoop(const char*) {
    return EnterBogrootFromSparkfly();
}

static bool RecordTekksQuestEntryFailureForLoop(const char* context) {
    return RecordTekksQuestEntryFailureAndMaybeResetDialog(context);
}

static bool ResetTekksQuestEntryFailuresForLoop(const char* context) {
    ResetTekksQuestEntryFailures(context);
    return true;
}

static bool HasReachedBogrootLvl2ForLoop() {
    return s_dungeonLoopTelemetry.entered_lvl2;
}

static bool HasCompletedBogrootObjectiveForLoop() {
    return s_dungeonLoopTelemetry.boss_completed;
}

static void MarkBogrootLevelStartedForLoop(int levelIndex) {
    if (levelIndex == 0) {
        s_dungeonLoopTelemetry.started_in_lvl1 = true;
    } else if (levelIndex == 1) {
        s_dungeonLoopTelemetry.started_in_lvl2 = true;
    }
}

static void MarkBogrootProgressLevelReachedForLoop(int levelIndex) {
    if (levelIndex >= 1) {
        s_dungeonLoopTelemetry.entered_lvl2 = true;
    }
}

static void LogBogrootLevelRouteResultForLoop(
    int levelIndex,
    const DungeonRouteRunner::RouteRunResult&) {
    if (levelIndex == 0) {
        Log::Info("Froggy: Bogroot loop after lvl1 map=%u lastWp=%u(%s) returnedToSparkfly=%d",
                  MapMgr::GetMapId(),
                  s_dungeonLoopTelemetry.last_waypoint_index,
                  s_dungeonLoopTelemetry.last_waypoint_label,
                  s_dungeonLoopTelemetry.returned_to_sparkfly ? 1 : 0);
    } else if (levelIndex == 1) {
        Log::Info("Froggy: Bogroot loop after lvl2 map=%u bossStarted=%d bossCompleted=%d",
                  MapMgr::GetMapId(),
                  s_dungeonLoopTelemetry.boss_started ? 1 : 0,
                  s_dungeonLoopTelemetry.boss_completed ? 1 : 0);
    }
}

static bool RunDungeonLoopFromCurrentMap() {
    DungeonRouteRunner::DungeonLoopLevel levels[2] = {};
    levels[0].map_id = MapIds::BOGROOT_GROWTHS_LVL1;
    levels[0].waypoints = BOGROOT_LVL1;
    levels[0].waypoint_count = BOGROOT_LVL1_COUNT;
    levels[0].name = "Bogroot Level 1";
    levels[1].map_id = MapIds::BOGROOT_GROWTHS_LVL2;
    levels[1].waypoints = BOGROOT_LVL2;
    levels[1].waypoint_count = BOGROOT_LVL2_COUNT;
    levels[1].name = "Bogroot Level 2";
    levels[1].spawn_stale_anchor = {BOGROOT_LVL1_TO_LVL2_PORTAL.x, BOGROOT_LVL1_TO_LVL2_PORTAL.y};
    levels[1].spawn_stale_anchor_clearance = BOGROOT_LVL1_TO_LVL2_HANDOFF_CLEARANCE;
    levels[1].spawn_ready_timeout_ms = 10000u;
    levels[1].spawn_ready_poll_ms = 200u;
    levels[1].spawn_settle_timeout_ms = 1500u;
    levels[1].spawn_settle_distance = 24.0f;

    DungeonRouteRunner::DungeonLoopCallbacks callbacks = {};
    callbacks.get_map_id = &MapMgr::GetMapId;
    callbacks.follow_waypoints = &FollowWaypoints;
    callbacks.is_loop_objective_completed = &HasCompletedBogrootObjectiveForLoop;
    callbacks.is_progress_level_reached = &HasReachedBogrootLvl2ForLoop;
    callbacks.reset_loop_state = &ResetDungeonLoopTelemetry;
    callbacks.prepare_entry = &PrepareTekksDungeonEntryForLoop;
    callbacks.enter_dungeon = &EnterBogrootFromSparkflyForLoop;
    callbacks.record_entry_failure = &RecordTekksQuestEntryFailureForLoop;
    callbacks.reset_entry_failures = &ResetTekksQuestEntryFailuresForLoop;
    callbacks.on_level_started = &MarkBogrootLevelStartedForLoop;
    callbacks.on_progress_level_reached = &MarkBogrootProgressLevelReachedForLoop;
    callbacks.after_level_route = &LogBogrootLevelRouteResultForLoop;

    DungeonRouteRunner::DungeonLoopOptions options = {};
    options.levels = levels;
    options.level_count = static_cast<int>(sizeof(levels) / sizeof(levels[0]));
    options.progress_level_index = 1;
    options.entry_map_id = MapIds::SPARKFLY_SWAMP;
    options.fallback_completion_map_id = MapIds::GADDS_ENCAMPMENT;
    options.max_entry_refresh_retries_before_progress = 3;
    options.ignore_bot_running_for_routes = true;
    options.log_prefix = "Froggy";
    options.loop_name = "Bogroot";
    options.entry_refresh_context = "bogroot-loop-refresh";

    const auto result = DungeonRouteRunner::RunDungeonLoop(callbacks, options);
    s_dungeonLoopTelemetry.final_map_id = result.final_map_id;
    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    s_dungeonLoopTelemetry.returned_to_sparkfly =
        result.returned_to_entry_map &&
        s_dungeonLoopTelemetry.final_map_id == MapIds::SPARKFLY_SWAMP;
    return result.completed;
}
