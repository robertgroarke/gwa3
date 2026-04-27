// Froggy map, waypoint, and transition telemetry helpers. Included by FroggyHM.cpp.

static bool IsBogrootMap(uint32_t mapId = 0) {
    if (mapId == 0) {
        mapId = MapMgr::GetMapId();
    }
    return IsBogrootMapId(mapId);
}

static int GetNearestWaypointIndex(const Waypoint* wps, int count) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    return DungeonRoute::FindNearestWaypointIndex(wps, count, me->x, me->y);
}

static bool WaitForBogrootLvl2SpawnReady(DWORD timeoutMs) {
    return DungeonRuntime::WaitForCondition(timeoutMs, []() {
        if (MapMgr::GetMapId() != MapIds::BOGROOT_GROWTHS_LVL2 ||
            !MapMgr::GetIsMapLoaded() ||
            AgentMgr::GetMyId() == 0) {
            return false;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (!me || me->hp <= 0.0f) {
            return false;
        }

        // During the zone handoff the client can briefly report the old level-1
        // portal coordinates even though the map id is already 616. Do not seed
        // the level-2 route until the player position has moved away from that
        // stale handoff point.
        const float distFromLvl1Portal = AgentMgr::GetDistance(me->x, me->y,
                                                               BOGROOT_LVL1_TO_LVL2_PORTAL.x,
                                                               BOGROOT_LVL1_TO_LVL2_PORTAL.y);
        return distFromLvl1Portal > BOGROOT_LVL1_TO_LVL2_HANDOFF_CLEARANCE;
    }, 200);
}


static void LogLvl1ToLvl2TransitionState(const char* stage, uint32_t portalId, DWORD elapsedMs, DWORD attempt) {
    auto* me = AgentMgr::GetMyAgent();
    const float meX = me ? me->x : 0.0f;
    const float meY = me ? me->y : 0.0f;
    const float distToExit = me ? AgentMgr::GetDistance(me->x, me->y,
                                                        BOGROOT_LVL1_TO_LVL2_PORTAL.x,
                                                        BOGROOT_LVL1_TO_LVL2_PORTAL.y) : -1.0f;
    const uint32_t mapId = MapMgr::GetMapId();
    const int loaded = MapMgr::GetIsMapLoaded() ? 1 : 0;
    const uint32_t targetId = AgentMgr::GetTargetId();

    float portalX = 0.0f;
    float portalY = 0.0f;
    float portalDist = -1.0f;
    uint32_t portalType = 0u;
    uint32_t portalGadget = 0u;
    if (portalId) {
        if (auto* portal = AgentMgr::GetAgentByID(portalId)) {
            portalX = portal->x;
            portalY = portal->y;
            portalType = portal->type;
            if (portal->type == 0x200) {
                portalGadget = static_cast<AgentGadget*>(portal)->gadget_id;
            }
            if (me) {
                portalDist = AgentMgr::GetDistance(me->x, me->y, portal->x, portal->y);
            }
        }
    }

    s_dungeonLoopTelemetry.map_loaded = loaded != 0;
    s_dungeonLoopTelemetry.player_alive = me != nullptr && me->hp > 0.0f;
    s_dungeonLoopTelemetry.player_hp = me ? me->hp : 0.0f;
    s_dungeonLoopTelemetry.player_x = meX;
    s_dungeonLoopTelemetry.player_y = meY;
    s_dungeonLoopTelemetry.target_id = targetId;
    s_dungeonLoopTelemetry.dist_to_exit = distToExit;
    s_dungeonLoopTelemetry.nearest_enemy_dist =
        me ? DungeonCombat::GetNearestLivingEnemyDistance(TELEMETRY_NEAREST_ENEMY_RANGE) : -1.0f;
    s_dungeonLoopTelemetry.nearby_enemy_count =
        me ? DungeonCombat::CountLivingEnemiesInRange(TELEMETRY_NEARBY_ENEMY_RANGE) : 0;
    s_dungeonLoopTelemetry.lvl1_portal_id = portalId;
    s_dungeonLoopTelemetry.lvl1_portal_x = portalX;
    s_dungeonLoopTelemetry.lvl1_portal_y = portalY;
    s_dungeonLoopTelemetry.lvl1_portal_dist = portalDist;

    LogBot("Lvl1 to Lvl2 [%s] attempt=%lu elapsed=%lums map=%u loaded=%d me=(%.0f, %.0f) distToExit=%.0f target=%u portal=%u type=0x%X gadget=%u portalPos=(%.0f, %.0f) portalDist=%.0f",
           stage,
           static_cast<unsigned long>(attempt),
           static_cast<unsigned long>(elapsedMs),
           mapId,
           loaded,
           meX,
           meY,
           distToExit,
           targetId,
           portalId,
           portalType,
           portalGadget,
           portalX,
           portalY,
           portalDist);
}

