// Froggy route checkpoint label handlers. Included by FroggyHMWaypointLabelHandlers.h.

static FroggyWaypointHandlerResult RecoverFroggyWaypointIfDead(
    const Waypoint* wps,
    int count,
    int& waypointIndex,
    WipeRecoveryContext context) {
    if (!IsDead()) {
        return FroggyWaypointHandlerResult::NotHandled;
    }

    int restartIdx = waypointIndex;
    if (!RecoverFromWaypointWipe(wps, count, waypointIndex, context, restartIdx)) {
        return FroggyWaypointHandlerResult::StopRoute;
    }
    waypointIndex = restartIdx;
    return FroggyWaypointHandlerResult::ContinueRoute;
}

static FroggyWaypointHandlerResult HandleFroggyDungeonDoorCheckpoint(
    const Waypoint* wps,
    int count,
    int& waypointIndex) {
    MoveFroggyRouteWaypoint(wps[waypointIndex]);
    LogFroggyWaypointState("post-dungeon-door-checkpoint-move", wps, count, waypointIndex);

    const auto recovery = RecoverFroggyWaypointIfDead(
        wps,
        count,
        waypointIndex,
        WipeRecoveryContext::DungeonDoorCheckpoint);
    if (recovery != FroggyWaypointHandlerResult::NotHandled) {
        return recovery;
    }

    DungeonCheckpoint::LockedDoorCheckpointOptions options;
    options.waypoints = wps;
    options.waypoint_count = count;
    options.current_index = waypointIndex;
    options.backtrack_steps = 3;
    options.move_before_check = false;
    options.log_prefix = "Froggy";
    options.checkpoint_name = "Dungeon Door Checkpoint";
    options.move_waypoint = &MoveFroggyWaypointForCheckpointReplay;
    options.get_nearest_waypoint = &DungeonNavigation::GetNearestWaypointIndex;
    const auto checkpoint = DungeonCheckpoint::HandleLockedDoorCheckpoint(options);
    waypointIndex = checkpoint.waypoint_index;
    return FroggyWaypointHandlerResult::ContinueRoute;
}

static FroggyWaypointHandlerResult HandleFroggyQuestDoorCheckpoint(
    const Waypoint* wps,
    int count,
    int& waypointIndex) {
    DungeonEntryRecovery::QuestDoorRecoveryOptions questRecoveryOptions;
    questRecoveryOptions.quest_id = GWA3::QuestIds::TEKKS_WAR;
    questRecoveryOptions.quest_ready.refresh_delay_ms = TEKKS_QUEST_REFRESH_DELAY_MS;
    questRecoveryOptions.quest_ready.post_set_active_delay_ms = TEKKS_SET_ACTIVE_DWELL_MS;
    questRecoveryOptions.waypoints = wps;
    questRecoveryOptions.waypoint_count = count;
    questRecoveryOptions.current_index = waypointIndex;
    questRecoveryOptions.backtrack_steps = 3;
    questRecoveryOptions.move_waypoint = &MoveFroggyWaypointForCheckpointReplay;
    questRecoveryOptions.after_move_waypoint = &LogQuestDoorCheckpointReplayWaypoint;
    questRecoveryOptions.return_to_quest_map = MakeFroggyReturnToSparkflyPlan("Froggy quest-door refresh");
    questRecoveryOptions.log_prefix = "Froggy";
    questRecoveryOptions.label = "Quest Door Checkpoint";
    const auto missingQuestRecovery = DungeonEntryRecovery::HandleQuestDoorRecovery(questRecoveryOptions);
    if (missingQuestRecovery.recovery_triggered) {
        LogBot("Quest Door Checkpoint reached without Tekks's War; returning to Sparkfly for quest refresh");
        LogFroggyWaypointState("quest-door-missing-quest-refresh-trigger", wps, count, waypointIndex);
        s_dungeonLoopTelemetry.final_map_id = missingQuestRecovery.final_map_id;
        s_dungeonLoopTelemetry.returned_to_sparkfly =
            missingQuestRecovery.returned_to_quest_map &&
            missingQuestRecovery.final_map_id == MapIds::SPARKFLY_SWAMP;
        return FroggyWaypointHandlerResult::StopRoute;
    }

    MoveFroggyRouteWaypoint(wps[waypointIndex]);
    LogFroggyWaypointState("post-quest-door-checkpoint-move", wps, count, waypointIndex);

    const auto recovery = RecoverFroggyWaypointIfDead(
        wps,
        count,
        waypointIndex,
        WipeRecoveryContext::QuestDoorCheckpoint);
    if (recovery != FroggyWaypointHandlerResult::NotHandled) {
        return recovery;
    }

    LogQuestDoorCheckpointDiagnostic(wps, waypointIndex);

    const int nearest = DungeonNavigation::GetNearestWaypointIndex(wps, count);
    if (nearest < waypointIndex) {
        LogBot("Failed first door at wp %d; nearest=%d, returning to Sparkfly for quest refresh",
               waypointIndex,
               nearest);
        LogFroggyWaypointState("quest-door-refresh-trigger", wps, count, waypointIndex);
        DungeonEntryRecovery::BacktrackReturnOptions returnOptions;
        returnOptions.waypoints = wps;
        returnOptions.waypoint_count = count;
        returnOptions.current_index = waypointIndex;
        returnOptions.backtrack_steps = 3;
        returnOptions.move_waypoint = &MoveFroggyWaypointForCheckpointReplay;
        returnOptions.after_move_waypoint = &LogQuestDoorCheckpointReplayWaypoint;
        returnOptions.return_to_quest_map = MakeFroggyReturnToSparkflyPlan("Froggy quest-door refresh");
        returnOptions.log_prefix = "Froggy";
        returnOptions.label = "Quest Door Checkpoint";
        const auto returned = DungeonEntryRecovery::ReplayBacktrackAndReturnToQuestMap(returnOptions);
        s_dungeonLoopTelemetry.final_map_id = returned.final_map_id;
        s_dungeonLoopTelemetry.returned_to_sparkfly =
            returned.returned_to_quest_map && returned.final_map_id == MapIds::SPARKFLY_SWAMP;
        return FroggyWaypointHandlerResult::StopRoute;
    }
    LogBot("Quest Door Checkpoint reached at wp %d nearest=%d", waypointIndex, nearest);
    return FroggyWaypointHandlerResult::ContinueRoute;
}
