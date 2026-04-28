// Froggy level-transition waypoint handlers. Included by FroggyHMWaypointTraversal.h.

static void HandleFroggyLvl1ToLvl2Waypoint() {
    s_dungeonLoopTelemetry.lvl1_to_lvl2_started = true;
    DWORD start = GetTickCount();
    DWORD lastLogAt = 0;
    DWORD attempt = 0;
    uint32_t portalId = DungeonInteractions::FindNearestSignpost(
        BOGROOT_LVL1_TO_LVL2_PORTAL.x,
        BOGROOT_LVL1_TO_LVL2_PORTAL.y,
        BOGROOT_LVL1_TO_LVL2_PORTAL_SEARCH_RADIUS);
    LogBot("Lvl1 to Lvl2: AutoIt-aligned move=(%.0f, %.0f) portal=%u",
           BOGROOT_LVL1_TO_LVL2_PORTAL.x,
           BOGROOT_LVL1_TO_LVL2_PORTAL.y,
           portalId);
    LogLvl1ToLvl2TransitionState("start", portalId, 0, attempt);
    while ((GetTickCount() - start) < BOGROOT_LVL1_TO_LVL2_TIMEOUT_MS) {
        ++attempt;
        s_dungeonLoopTelemetry.lvl1_to_lvl2_attempts = attempt;
        const DWORD elapsed = GetTickCount() - start;
        if (elapsed - lastLogAt >= BOGROOT_LVL1_TO_LVL2_LOG_INTERVAL_MS) {
            const uint32_t refreshedPortalId = DungeonInteractions::FindNearestSignpost(
                BOGROOT_LVL1_TO_LVL2_PORTAL.x,
                BOGROOT_LVL1_TO_LVL2_PORTAL.y,
                BOGROOT_LVL1_TO_LVL2_PORTAL_SEARCH_RADIUS);
            if (refreshedPortalId != portalId) {
                LogBot("Lvl1 to Lvl2: portal refresh old=%u new=%u", portalId, refreshedPortalId);
                portalId = refreshedPortalId;
            }
            LogLvl1ToLvl2TransitionState("loop", portalId, elapsed, attempt);
            lastLogAt = elapsed;
        }
        AgentMgr::Move(BOGROOT_LVL1_TO_LVL2_PORTAL.x, BOGROOT_LVL1_TO_LVL2_PORTAL.y);
        WaitMs(BOGROOT_LVL1_TO_LVL2_MOVE_POLL_MS);
        if (MapMgr::GetMapId() == MapIds::BOGROOT_GROWTHS_LVL2) {
            LogLvl1ToLvl2TransitionState("entered_lvl2", portalId, GetTickCount() - start, attempt);
            s_dungeonLoopTelemetry.entered_lvl2 = true;
            const bool lvl2Ready = WaitForBogrootLvl2SpawnReady(BOGROOT_LVL2_SPAWN_READY_TIMEOUT_MS);
            auto* me = AgentMgr::GetMyAgent();
            Log::Info("Froggy: Lvl1 to Lvl2 spawn settle ready=%d player=(%.0f, %.0f) nearestLvl2Wp=%d",
                      lvl2Ready ? 1 : 0,
                      me ? me->x : 0.0f,
                      me ? me->y : 0.0f,
                      DungeonNavigation::GetNearestWaypointIndex(BOGROOT_LVL2, BOGROOT_LVL2_COUNT));
            WaitForLocalPositionSettle(BOGROOT_LVL2_SPAWN_SETTLE_TIMEOUT_MS, BOGROOT_LVL2_SPAWN_SETTLE_DISTANCE);
            return;
        }
    }
    LogLvl1ToLvl2TransitionState("timeout", portalId, GetTickCount() - start, attempt);
}
