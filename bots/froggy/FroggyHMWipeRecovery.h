// Froggy wipe recovery helpers. Included by FroggyHMTransitions.h.

using WipeRecoveryContext = DungeonCheckpoint::WaypointWipeRecoveryContext;

static DungeonCheckpoint::WaypointWipeRecoveryOptions MakeFroggyWipeRecoveryOptions(
    const Waypoint* wps,
    int count) {
    DungeonCheckpoint::WaypointWipeRecoveryOptions options;
    options.waypoints = wps;
    options.nearest_index = DungeonNavigation::GetNearestWaypointIndex(wps, count);
    options.waypoint_count = count;
    options.backtrack_steps = 2;
    options.wipe_count = &s_wipeCount;
    options.is_dead = &IsDead;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.return_to_outpost = &MapMgr::ReturnToOutpost;
    options.use_dp_removal = &UseDpRemovalIfNeeded;
    options.get_nearest_waypoint = &DungeonNavigation::GetNearestWaypointIndex;
    return options;
}

static bool RecoverFromWaypointWipe(const Waypoint* wps,
                                    int count,
                                    int currentIndex,
                                    WipeRecoveryContext context,
                                    int& outRestartIndex) {
    DungeonCheckpoint::RouteWipeRecoveryOptions options;
    options.waypoints = wps;
    options.waypoint_count = count;
    options.current_index = currentIndex;
    options.context = context;
    options.log_prefix = "Froggy";
    options.recovery = MakeFroggyWipeRecoveryOptions(wps, count);
    const auto recovery = DungeonCheckpoint::RecoverRouteWaypointWipe(options);

    if (recovery.returned_to_outpost) {
        return false;
    }

    outRestartIndex = recovery.restart_index;
    return true;
}
