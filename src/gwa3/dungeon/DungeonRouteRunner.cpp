#include <gwa3/dungeon/DungeonRouteRunner.h>

#include <gwa3/core/Log.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/managers/MapMgr.h>

#include <cstring>

namespace GWA3::DungeonRouteRunner {

namespace {

uint32_t CurrentMapId(const RouteRunCallbacks& callbacks) {
    return callbacks.get_map_id ? callbacks.get_map_id() : MapMgr::GetMapId();
}

bool IsBotRunning(const RouteRunCallbacks& callbacks) {
    return callbacks.is_bot_running ? callbacks.is_bot_running() : true;
}

bool IsDead(const RouteRunCallbacks& callbacks) {
    return callbacks.is_dead ? callbacks.is_dead() : false;
}

bool IsRouteMap(uint32_t map_id, const RouteRunCallbacks& callbacks) {
    return callbacks.is_route_map ? callbacks.is_route_map(map_id) : false;
}

int ResolveStartIndex(const DungeonRoute::Waypoint* waypoints,
                      int count,
                      uint32_t map_id,
                      const RouteRunCallbacks& callbacks) {
    if (callbacks.resolve_start_index) {
        return callbacks.resolve_start_index(waypoints, count, map_id);
    }
    return DungeonNavigation::GetNearestWaypointIndex(waypoints, count);
}

void LogWaypoint(int waypoint_index,
                 const DungeonRoute::Waypoint& waypoint,
                 const RouteRunCallbacks& callbacks,
                 const RouteRunOptions& options) {
    if (callbacks.log_waypoint) {
        callbacks.log_waypoint(waypoint_index, waypoint);
        return;
    }
    Log::Info("%s: Moving to waypoint %d: %s (%.0f, %.0f)",
              options.log_prefix ? options.log_prefix : "Dungeon",
              waypoint_index,
              waypoint.label ? waypoint.label : "",
              waypoint.x,
              waypoint.y);
}

void LogRouteWaypointState(const char* stage,
                           const DungeonRoute::Waypoint* waypoints,
                           int count,
                           int waypoint_index,
                           const RouteRunCallbacks& callbacks,
                           const RouteRunOptions& options) {
    if (!options.log_route_waypoint_state) return;
    if (callbacks.log_waypoint_state) {
        callbacks.log_waypoint_state(stage, waypoints, count, waypoint_index);
    }
}

} // namespace

RouteRunResult RunWaypointRoute(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    const RouteRunCallbacks& callbacks,
    const RouteRunOptions& options) {
    RouteRunResult result;
    if (!waypoints || count <= 0 || !callbacks.move_standard_waypoint) {
        result.stopped = true;
        return result;
    }

    const uint32_t map_id = CurrentMapId(callbacks);
    const int start_index = ResolveStartIndex(waypoints, count, map_id, callbacks);
    const bool use_progress_backtrack =
        options.use_nearest_progress_backtrack && !IsRouteMap(map_id, callbacks);
    auto progress_state = DungeonRoute::MakeWaypointProgressState(start_index);

    Log::Info("%s: %s route start map=%u startIdx=%d count=%d ignoreBotRunning=%d",
              options.log_prefix ? options.log_prefix : "Dungeon",
              options.route_name ? options.route_name : "route",
              map_id,
              start_index,
              count,
              options.ignore_bot_running ? 1 : 0);

    for (int i = start_index; i < count; ++i) {
        result.final_index = i;
        if (!options.ignore_bot_running && !IsBotRunning(callbacks)) {
            result.stopped = true;
            return result;
        }
        if (CurrentMapId(callbacks) != map_id) {
            Log::Info("%s: %s route map changed expected=%u actual=%u before wp=%d(%s)",
                      options.log_prefix ? options.log_prefix : "Dungeon",
                      options.route_name ? options.route_name : "route",
                      map_id,
                      CurrentMapId(callbacks),
                      i,
                      waypoints[i].label ? waypoints[i].label : "");
            result.map_changed = true;
            result.stopped = true;
            return result;
        }

        if (use_progress_backtrack) {
            const int current_nearest = DungeonNavigation::GetNearestWaypointIndex(waypoints, count);
            const auto progress = DungeonRoute::EvaluateWaypointProgress(current_nearest, progress_state);
            if (progress.backtrack) {
                Log::Info("%s: Waypoint stuck (nearest=%d unchanged 5x) - backtracking to %d",
                          options.log_prefix ? options.log_prefix : "Dungeon",
                          current_nearest,
                          progress.backtrack_index);
                i = progress.backtrack_index;
            }
        }

        if (IsDead(callbacks)) {
            int restart_index = i;
            if (!callbacks.recover_wipe ||
                !callbacks.recover_wipe(waypoints, count, i, restart_index)) {
                result.stopped = true;
                return result;
            }
            i = restart_index;
        }

        const auto& waypoint = waypoints[i];
        LogWaypoint(i, waypoint, callbacks, options);
        if (IsRouteMap(map_id, callbacks)) {
            LogRouteWaypointState("pre", waypoints, count, i, callbacks, options);
        }
        if (callbacks.update_telemetry) {
            callbacks.update_telemetry(i, waypoint);
        }

        const auto handled = callbacks.handle_special_waypoint
            ? callbacks.handle_special_waypoint(waypoints, count, i)
            : WaypointHandlerResult::NotHandled;
        if (handled == WaypointHandlerResult::StopRoute) {
            result.stopped = true;
            result.final_index = i;
            return result;
        }
        if (handled == WaypointHandlerResult::ContinueRoute) {
            result.final_index = i;
            continue;
        }

        callbacks.move_standard_waypoint(waypoint, i);
        if (IsRouteMap(map_id, callbacks)) {
            LogRouteWaypointState("post", waypoints, count, i, callbacks, options);
        }
        result.final_index = i;
    }

    result.completed = true;
    return result;
}

} // namespace GWA3::DungeonRouteRunner
