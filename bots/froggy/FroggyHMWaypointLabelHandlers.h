// Froggy route-label handlers. Included by FroggyHMWaypointTraversal.h.

enum class FroggyWaypointHandlerResult {
    NotHandled,
    ContinueRoute,
    StopRoute,
};

static void HandleFroggyLvl1ToLvl2Waypoint();

#include "FroggyHMWaypointCheckpointHandlers.h"

static FroggyWaypointHandlerResult HandleFroggySpecialWaypoint(
    const Waypoint* wps,
    int count,
    int& waypointIndex) {
    switch (ResolveWaypointBehavior(wps[waypointIndex].label)) {
    case WaypointBehavior::GrabBlessing:
        DungeonNavigation::HandleBlessingWaypoint(
            wps[waypointIndex],
            &MoveFroggyRouteWaypointDefault,
            &GrabDungeonBlessing);
        LogFroggyWaypointState("post-blessing-move", wps, count, waypointIndex);
        return FroggyWaypointHandlerResult::ContinueRoute;
    case WaypointBehavior::LevelTransition:
        HandleFroggyLvl1ToLvl2Waypoint();
        return FroggyWaypointHandlerResult::StopRoute;
    case WaypointBehavior::AcquireDungeonKey: {
        AggroMoveToEx(wps[waypointIndex].x, wps[waypointIndex].y, wps[waypointIndex].fight_range);
        LogFroggyWaypointState("post-dungeon-key-move", wps, count, waypointIndex);
        const bool keyAcquired = AcquireBogrootBossKey();
        Log::Info("Froggy: Dungeon Key step acquired=%d", keyAcquired ? 1 : 0);
        if (!keyAcquired) {
            Log::Warn("Froggy: Dungeon Key step failed to secure boss key; returning to outpost for maintenance/recovery");
            MapMgr::ReturnToOutpost();
            DungeonRuntime::WaitForMapReady(MapIds::GADDS_ENCAMPMENT, 60000u);
            s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
            return FroggyWaypointHandlerResult::StopRoute;
        }
        return FroggyWaypointHandlerResult::ContinueRoute;
    }
    case WaypointBehavior::OpenDungeonDoor:
        DungeonNavigation::HandleOpenDungeonDoorWaypoint(
            wps[waypointIndex],
            &AggroMoveToEx,
            &OpenDungeonDoorAt);
        LogFroggyWaypointState("post-dungeon-door-move", wps, count, waypointIndex);
        return FroggyWaypointHandlerResult::ContinueRoute;
    case WaypointBehavior::ValidateDungeonDoorCheckpoint:
        return HandleFroggyDungeonDoorCheckpoint(wps, count, waypointIndex);
    case WaypointBehavior::ValidateQuestDoorCheckpoint:
        return HandleFroggyQuestDoorCheckpoint(wps, count, waypointIndex);
    case WaypointBehavior::BossRewardSequence:
        if (strcmp(wps[waypointIndex].label, "Boss") == 0) {
            HandleBossWaypoint(wps[waypointIndex]);
            return FroggyWaypointHandlerResult::StopRoute;
        }
        return FroggyWaypointHandlerResult::NotHandled;
    default:
        return FroggyWaypointHandlerResult::NotHandled;
    }
}
