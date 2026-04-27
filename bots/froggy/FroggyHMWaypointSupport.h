// Froggy waypoint support helpers. Included by FroggyHMWaypointTraversal.h.

static bool IsBogrootRouteMap(uint32_t mapId) {
    return IsBogrootMapId(mapId);
}

static int GetFroggyRouteStartIndex(const Waypoint* wps, int count, uint32_t mapId) {
    const int nearestIdx = GetNearestWaypointIndex(wps, count);
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
    auto* me = AgentMgr::GetMyAgent();
    const float myX = me ? me->x : 0.0f;
    const float myY = me ? me->y : 0.0f;
    const float hp = me ? me->hp : 0.0f;
    const float distToWaypoint = me
        ? AgentMgr::GetDistance(myX, myY, wps[waypointIndex].x, wps[waypointIndex].y)
        : -1.0f;
    const float nearestEnemy = me ? DungeonCombat::GetNearestLivingEnemyDistance(TELEMETRY_NEAREST_ENEMY_RANGE) : -1.0f;
    const uint32_t nearbyEnemies = me ? DungeonCombat::CountLivingEnemiesInRange(TELEMETRY_NEARBY_ENEMY_RANGE) : 0;
    Log::Info("Froggy: Bogroot %s wp=%d(%s) map=%u loaded=%d alive=%d hp=%.3f pos=(%.0f, %.0f) distToWp=%.0f nearest=%d target=%u nearestEnemy=%.0f nearbyEnemies=%u",
              stage,
              waypointIndex,
              wps[waypointIndex].label ? wps[waypointIndex].label : "",
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              me && me->hp > 0.0f ? 1 : 0,
              hp,
              myX,
              myY,
              distToWaypoint,
              GetNearestWaypointIndex(wps, count),
              AgentMgr::GetTargetId(),
              nearestEnemy,
              nearbyEnemies);
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

static void MoveFroggyRouteWaypointWithCombatLoot(const Waypoint& waypoint, int waypointIndex) {
    if (waypoint.fight_range <= 0.0f || !IsMapLoaded()) {
        MoveFroggyRouteWaypoint(waypoint);
        return;
    }

    AggroMoveToEx(waypoint.x, waypoint.y, waypoint.fight_range);
    const int picked = LootAfterCombatSweep(
        waypoint.fight_range,
        waypoint.label ? waypoint.label : "waypoint");
    Log::Info("Froggy: Post-pack loot sweep wp=%d(%s) range=%.0f picked=%d",
              waypointIndex,
              waypoint.label ? waypoint.label : "",
              DungeonLoot::ComputePostCombatLootRange(waypoint.fight_range),
              picked);
}
