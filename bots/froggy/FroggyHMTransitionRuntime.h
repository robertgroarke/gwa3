// Froggy map, waypoint, and transition telemetry helpers. Included by FroggyHM.cpp.

static bool WaitForBogrootLvl2SpawnReady(DWORD timeoutMs) {
    return DungeonRuntime::WaitForLevelSpawnReady(
        MapIds::BOGROOT_GROWTHS_LVL2,
        {BOGROOT_LVL1_TO_LVL2_PORTAL.x, BOGROOT_LVL1_TO_LVL2_PORTAL.y},
        BOGROOT_LVL1_TO_LVL2_HANDOFF_CLEARANCE,
        timeoutMs,
        200u);
}


static void LogLvl1ToLvl2TransitionState(const char* stage, uint32_t portalId, DWORD elapsedMs, DWORD attempt) {
    DungeonRuntime::TransitionTelemetry telemetry;
    DungeonRuntime::LogLevelTransitionTelemetry(
        "Froggy",
        "Lvl1 to Lvl2",
        stage,
        portalId,
        elapsedMs,
        attempt,
        {BOGROOT_LVL1_TO_LVL2_PORTAL.x, BOGROOT_LVL1_TO_LVL2_PORTAL.y},
        TELEMETRY_NEAREST_ENEMY_RANGE,
        TELEMETRY_NEARBY_ENEMY_RANGE,
        &telemetry);

    s_dungeonLoopTelemetry.map_loaded = telemetry.map_loaded;
    s_dungeonLoopTelemetry.player_alive = telemetry.player_alive;
    s_dungeonLoopTelemetry.player_hp = telemetry.player_hp;
    s_dungeonLoopTelemetry.player_x = telemetry.player_x;
    s_dungeonLoopTelemetry.player_y = telemetry.player_y;
    s_dungeonLoopTelemetry.target_id = telemetry.target_id;
    s_dungeonLoopTelemetry.dist_to_exit = telemetry.dist_to_exit;
    s_dungeonLoopTelemetry.nearest_enemy_dist = telemetry.nearest_enemy_dist;
    s_dungeonLoopTelemetry.nearby_enemy_count = telemetry.nearby_enemy_count;
    s_dungeonLoopTelemetry.lvl1_portal_id = telemetry.portal_id;
    s_dungeonLoopTelemetry.lvl1_portal_x = telemetry.portal_x;
    s_dungeonLoopTelemetry.lvl1_portal_y = telemetry.portal_y;
    s_dungeonLoopTelemetry.lvl1_portal_dist = telemetry.portal_dist;
}

