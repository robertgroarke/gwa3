// Froggy waypoint traversal and route-label handlers. Included by FroggyHM.cpp
// while label behavior is progressively moved into shared dungeon helpers.

#include "FroggyHMWaypointSupport.h"
#include "FroggyHMCheckpointReplay.h"
#include "FroggyHMLvl2TransitionWaypoint.h"
#include "FroggyHMWaypointLabelHandlers.h"

static void FollowWaypoints(const Waypoint* wps, int count, bool ignoreBotRunning = false) {
    uint32_t mapId = MapMgr::GetMapId();
    int startIdx = GetFroggyRouteStartIndex(wps, count, mapId);
    const bool useNearestProgressBacktrack = !IsBogrootRouteMap(mapId);
    auto progressState = DungeonRoute::MakeWaypointProgressState(startIdx);

    Log::Info("Froggy: Bogroot FollowWaypoints start map=%u startIdx=%d count=%d ignoreBotRunning=%d",
              mapId,
              startIdx,
              count,
              ignoreBotRunning ? 1 : 0);

    for (int i = startIdx; i < count; i++) {
        if (!ignoreBotRunning && !Bot::IsRunning()) return;
        if (MapMgr::GetMapId() != mapId) {
            Log::Info("Froggy: Bogroot FollowWaypoints map changed expected=%u actual=%u before wp=%d(%s)",
                      mapId,
                      MapMgr::GetMapId(),
                      i,
                      wps[i].label ? wps[i].label : "");
            return;
        }

        if (useNearestProgressBacktrack) {
            const int currentNearest = GetNearestWaypointIndex(wps, count);
            const auto progress = DungeonRoute::EvaluateWaypointProgress(currentNearest, progressState);
            if (progress.backtrack) {
                LogBot("Waypoint stuck (nearest=%d unchanged 5x) - backtracking to %d",
                       currentNearest,
                       progress.backtrack_index);
                i = progress.backtrack_index;
            }
        }

        if (IsDead()) {
            int restartIdx = i;
            if (!RecoverFromWaypointWipe(wps, count, i, WipeRecoveryContext::Standard, restartIdx)) {
                return;
            }
            i = restartIdx;
        }

        LogBot("Moving to waypoint %d: %s (%.0f, %.0f)", i, wps[i].label, wps[i].x, wps[i].y);
        if (IsBogrootRouteMap(mapId)) {
            LogFroggyWaypointState("pre", wps, count, i);
        }
        s_dungeonLoopTelemetry.last_waypoint_index = static_cast<uint32_t>(i);
        s_dungeonLoopTelemetry.waypoint_iterations++;
        strncpy_s(s_dungeonLoopTelemetry.last_waypoint_label,
                  wps[i].label ? wps[i].label : "",
                  _TRUNCATE);

        const auto handled = HandleFroggySpecialWaypoint(wps, count, i);
        if (handled == FroggyWaypointHandlerResult::StopRoute) {
            return;
        }
        if (handled == FroggyWaypointHandlerResult::ContinueRoute) {
            continue;
        }

        // Standard waypoint - aggro move then loot sweep
        MoveFroggyRouteWaypointWithCombatLoot(wps[i], i);
        if (IsBogrootRouteMap(mapId)) {
            LogFroggyWaypointState("post", wps, count, i);
        }
    }
}
