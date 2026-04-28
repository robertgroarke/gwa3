// Froggy waypoint support helpers. Included by FroggyHMWaypointTraversal.h.

static bool IsBogrootRouteMap(uint32_t mapId) {
    return IsBogrootMapId(mapId);
}

static int GetFroggyRouteStartIndex(const Waypoint* wps, int count, uint32_t mapId) {
    const int nearestIdx = DungeonNavigation::GetNearestWaypointIndex(wps, count);
    float distanceFromLvl1Portal = -1.0f;
    if (mapId == MapIds::BOGROOT_GROWTHS_LVL2) {
        if (auto* me = AgentMgr::GetMyAgent()) {
            distanceFromLvl1Portal = AgentMgr::GetDistance(me->x, me->y,
                                                           BOGROOT_LVL1_TO_LVL2_PORTAL.x,
                                                           BOGROOT_LVL1_TO_LVL2_PORTAL.y);
        }
    }

    const int startIdx = ResolveRouteStartIndex(mapId, wps, count, nearestIdx, distanceFromLvl1Portal);
    if (mapId == MapIds::BOGROOT_GROWTHS_LVL1 && nearestIdx == 1 && startIdx == 0) {
        Log::Info("Froggy: Bogroot lvl1 forcing startIdx from blessing back to opening aggro leg");
    }
    if (mapId == MapIds::BOGROOT_GROWTHS_LVL2 && nearestIdx >= 20 && startIdx == 0) {
        Log::Info("Froggy: Bogroot lvl2 suppressing stale startIdx=%d while spawn handoff is unresolved (distFromLvl1Portal=%.0f)",
                  nearestIdx,
                  distanceFromLvl1Portal);
    }
    return startIdx;
}

static void LogFroggyWaypointState(const char* stage, const Waypoint* wps, int count, int waypointIndex) {
    DungeonNavigation::WaypointTelemetryOptions options;
    options.log_prefix = "Froggy";
    options.route_name = "Bogroot";
    options.nearest_enemy_range = TELEMETRY_NEAREST_ENEMY_RANGE;
    options.nearby_enemy_range = TELEMETRY_NEARBY_ENEMY_RANGE;
    DungeonNavigation::LogWaypointState(stage, wps, count, waypointIndex, options);
}

static void MoveFroggyWaypoint(float x, float y, float threshold) {
    (void)MoveToAndWait(x, y, threshold);
}

static void AggroMoveFroggyWaypoint(float x, float y, float fightRange) {
    AggroMoveToEx(x, y, fightRange);
}

static void MoveFroggyRouteWaypoint(const Waypoint& waypoint, float moveThreshold = 250.0f) {
    DungeonNavigation::MoveRouteWaypoint(
        waypoint,
        &MoveFroggyWaypoint,
        &AggroMoveFroggyWaypoint,
        &IsMapLoaded,
        moveThreshold);
}

static void MoveFroggyRouteWaypointDefault(const Waypoint& waypoint) {
    MoveFroggyRouteWaypoint(waypoint);
}

static void MoveFroggyRouteWaypointWithCombatLoot(const Waypoint& waypoint, int waypointIndex) {
    DungeonNavigation::MoveRouteWaypointWithCombatLoot(
        waypoint,
        waypointIndex,
        &MoveFroggyRouteWaypointDefault,
        &IsMapLoaded,
        &LootAfterCombatSweep,
        "Froggy");
}
