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
    if (!RefreshTekksQuestReadyForDungeonEntry("quest-door checkpoint precheck")) {
        LogBot("Quest Door Checkpoint reached without Tekks's War; returning to Sparkfly for quest refresh");
        LogFroggyWaypointState("quest-door-missing-quest-refresh-trigger", wps, count, waypointIndex);
        ReplayFroggyCheckpointBacktrack(wps, count, waypointIndex, &LogQuestDoorCheckpointReplayWaypoint);
        const bool returned = ReturnToSparkflyFromBogroot();
        Log::Info("Froggy: Bogroot quest-door missing-quest refresh returned=%d finalMap=%u",
                  returned ? 1 : 0,
                  MapMgr::GetMapId());
        s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
        s_dungeonLoopTelemetry.returned_to_sparkfly =
            returned && MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
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
        ReplayFroggyCheckpointBacktrack(wps, count, waypointIndex, &LogQuestDoorCheckpointReplayWaypoint);
        const bool returned = ReturnToSparkflyFromBogroot();
        Log::Info("Froggy: Bogroot quest-door refresh returned=%d finalMap=%u",
                  returned ? 1 : 0,
                  MapMgr::GetMapId());
        s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
        s_dungeonLoopTelemetry.returned_to_sparkfly =
            returned && MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
        return FroggyWaypointHandlerResult::StopRoute;
    }
    LogBot("Quest Door Checkpoint reached at wp %d nearest=%d", waypointIndex, nearest);
    return FroggyWaypointHandlerResult::ContinueRoute;
}
