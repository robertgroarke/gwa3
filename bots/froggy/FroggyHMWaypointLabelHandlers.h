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
        MoveFroggyRouteWaypoint(wps[waypointIndex]);
        LogFroggyWaypointState("post-blessing-move", wps, count, waypointIndex);
        GrabDungeonBlessing(wps[waypointIndex].x, wps[waypointIndex].y);
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
            Log::Warn("Froggy: Dungeon Key step failed to secure boss key");
            return FroggyWaypointHandlerResult::StopRoute;
        }
        return FroggyWaypointHandlerResult::ContinueRoute;
    }
    case WaypointBehavior::OpenDungeonDoor:
        AggroMoveToEx(wps[waypointIndex].x, wps[waypointIndex].y, wps[waypointIndex].fight_range);
        LogFroggyWaypointState("post-dungeon-door-move", wps, count, waypointIndex);
        OpenDungeonDoorAt(wps[waypointIndex].x, wps[waypointIndex].y);
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
