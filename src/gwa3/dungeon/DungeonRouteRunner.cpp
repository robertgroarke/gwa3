#include <gwa3/dungeon/DungeonRouteRunner.h>

#include <gwa3/core/Log.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonRuntime.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <algorithm>
#include <cstring>
#include <vector>

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

uint32_t CurrentLoopMapId(const DungeonLoopCallbacks& callbacks) {
    return callbacks.get_map_id ? callbacks.get_map_id() : MapMgr::GetMapId();
}

const char* LoopPrefix(const DungeonLoopOptions& options) {
    return options.log_prefix ? options.log_prefix : "Dungeon";
}

const char* LoopName(const DungeonLoopOptions& options) {
    return options.loop_name ? options.loop_name : "Dungeon";
}

int FindLoopLevelIndex(uint32_t map_id, const DungeonLoopOptions& options) {
    if (options.levels == nullptr || options.level_count <= 0) return -1;
    for (int i = 0; i < options.level_count; ++i) {
        if (options.levels[i].map_id == map_id) return i;
    }
    return -1;
}

bool HasReachedProgressLevel(const DungeonLoopCallbacks& callbacks, const DungeonLoopResult& result) {
    return result.reached_progress_level ||
           (callbacks.is_progress_level_reached && callbacks.is_progress_level_reached());
}

bool IsLoopObjectiveCompleted(const DungeonLoopCallbacks& callbacks) {
    return callbacks.is_loop_objective_completed && callbacks.is_loop_objective_completed();
}

void MarkProgressLevelReached(
    int level_index,
    const DungeonLoopCallbacks& callbacks,
    DungeonLoopResult& result,
    const DungeonLoopOptions& options) {
    if (level_index < options.progress_level_index || result.reached_progress_level) return;
    result.reached_progress_level = true;
    if (callbacks.on_progress_level_reached) {
        callbacks.on_progress_level_reached(level_index);
    }
}

void WaitForLoopLevelSpawnGate(
    int level_index,
    const DungeonLoopLevel& level,
    const DungeonLoopOptions& options) {
    if (level.spawn_ready_timeout_ms == 0u) return;
    const bool ready = DungeonRuntime::WaitForLevelSpawnReady(
        level.map_id,
        level.spawn_stale_anchor,
        level.spawn_stale_anchor_clearance,
        level.spawn_ready_timeout_ms,
        level.spawn_ready_poll_ms);
    auto* me = AgentMgr::GetMyAgent();
    Log::Info("%s: %s level %d spawn gate ready=%d player=(%.0f, %.0f)",
              LoopPrefix(options),
              LoopName(options),
              level_index,
              ready ? 1 : 0,
              me ? me->x : 0.0f,
              me ? me->y : 0.0f);
    if (level.spawn_settle_timeout_ms > 0u && level.spawn_settle_distance > 0.0f) {
        DungeonNavigation::WaitForLocalPositionSettle(
            level.spawn_settle_timeout_ms,
            level.spawn_settle_distance);
    }
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
    std::vector<int> waypoint_move_retries(static_cast<size_t>(count), 0);

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

        const auto move_result = callbacks.move_standard_waypoint(waypoint, i);
        if (IsRouteMap(map_id, callbacks)) {
            LogRouteWaypointState("post", waypoints, count, i, callbacks, options);
        }
        const uint32_t current_map_id = CurrentMapId(callbacks);
        if (move_result.map_changed || current_map_id != map_id) {
            Log::Info("%s: %s route map changed expected=%u actual=%u after wp=%d(%s)",
                      options.log_prefix ? options.log_prefix : "Dungeon",
                      options.route_name ? options.route_name : "route",
                      map_id,
                      current_map_id,
                      i,
                      waypoint.label ? waypoint.label : "");
            result.map_changed = true;
            result.stopped = true;
            result.final_index = i;
            return result;
        }
        if (move_result.dead || IsDead(callbacks)) {
            int restart_index = i;
            if (!callbacks.recover_wipe ||
                !callbacks.recover_wipe(waypoints, count, i, restart_index)) {
                result.stopped = true;
                result.final_index = i;
                return result;
            }
            i = restart_index - 1;
            continue;
        }
        if (!move_result.reached) {
            const int retries_used = waypoint_move_retries[static_cast<size_t>(i)];
            if (retries_used < options.max_waypoint_move_retries) {
                waypoint_move_retries[static_cast<size_t>(i)] = retries_used + 1;
                const int backtrack_index = std::max(0, i - options.waypoint_move_backtrack_steps);
                Log::Warn("%s: %s waypoint move failed wp=%d(%s) dist=%.0f threshold=%.0f retry=%d/%d backtrack=%d",
                          options.log_prefix ? options.log_prefix : "Dungeon",
                          options.route_name ? options.route_name : "route",
                          i,
                          waypoint.label ? waypoint.label : "",
                          move_result.final_distance,
                          move_result.threshold,
                          retries_used + 1,
                          options.max_waypoint_move_retries,
                          backtrack_index);
                i = backtrack_index - 1;
                continue;
            }

            Log::Warn("%s: %s waypoint move failed permanently wp=%d(%s) dist=%.0f threshold=%.0f map=%u",
                      options.log_prefix ? options.log_prefix : "Dungeon",
                      options.route_name ? options.route_name : "route",
                      i,
                      waypoint.label ? waypoint.label : "",
                      move_result.final_distance,
                      move_result.threshold,
                      current_map_id);
            result.stopped = true;
            result.final_index = i;
            return result;
        }
        result.final_index = i;
    }

    result.completed = true;
    return result;
}

DungeonLoopResult RunDungeonLoop(
    const DungeonLoopCallbacks& callbacks,
    const DungeonLoopOptions& options) {
    DungeonLoopResult result;
    if (options.levels == nullptr || options.level_count <= 0 ||
        options.entry_map_id == 0u || callbacks.follow_waypoints == nullptr) {
        result.final_map_id = CurrentLoopMapId(callbacks);
        result.unsupported_map = true;
        return result;
    }

    if (callbacks.reset_loop_state) {
        callbacks.reset_loop_state();
    }

    int refresh_retries = 0;
    while (true) {
        const uint32_t map_id = CurrentLoopMapId(callbacks);
        result.final_map_id = map_id;
        result.entry_refresh_retries = refresh_retries;
        result.objective_completed = IsLoopObjectiveCompleted(callbacks);
        const int level_index = FindLoopLevelIndex(map_id, options);
        if (level_index >= 0) {
            result.last_level_index = level_index;
            if (callbacks.on_level_started) {
                callbacks.on_level_started(level_index);
            }
            MarkProgressLevelReached(level_index, callbacks, result, options);
        }

        Log::Info("%s: %s loop iteration map=%u refreshRetries=%d reachedProgress=%d finalMap=%u",
                  LoopPrefix(options),
                  LoopName(options),
                  map_id,
                  refresh_retries,
                  HasReachedProgressLevel(callbacks, result) ? 1 : 0,
                  result.final_map_id);

        if (level_index >= 0) {
            const auto& level = options.levels[level_index];
            WaitForLoopLevelSpawnGate(level_index, level, options);
            callbacks.follow_waypoints(
                level.waypoints,
                level.waypoint_count,
                options.ignore_bot_running_for_routes);
            result.final_map_id = CurrentLoopMapId(callbacks);
            result.objective_completed = IsLoopObjectiveCompleted(callbacks);
            MarkProgressLevelReached(FindLoopLevelIndex(result.final_map_id, options), callbacks, result, options);

            if (callbacks.after_level_route) {
                RouteRunResult route_result = {};
                route_result.final_index = level.waypoint_count > 0 ? level.waypoint_count - 1 : -1;
                route_result.completed = result.final_map_id == map_id;
                route_result.map_changed = result.final_map_id != map_id;
                route_result.stopped = !route_result.completed;
                callbacks.after_level_route(level_index, route_result);
            }
        } else if (map_id == options.entry_map_id && !HasReachedProgressLevel(callbacks, result)) {
            if (refresh_retries < options.max_entry_refresh_retries_before_progress) {
                ++refresh_retries;
                Log::Info("%s: %s loop refreshing via entry map retry=%d",
                          LoopPrefix(options),
                          LoopName(options),
                          refresh_retries);
                const char* context = options.entry_refresh_context
                    ? options.entry_refresh_context
                    : "dungeon-loop-refresh";
                if (!callbacks.prepare_entry || !callbacks.prepare_entry(context)) {
                    Log::Info("%s: %s loop refresh aborted because prepare_entry failed",
                              LoopPrefix(options),
                              LoopName(options));
                    if (callbacks.record_entry_failure) {
                        (void)callbacks.record_entry_failure(context);
                    }
                    result.final_map_id = CurrentLoopMapId(callbacks);
                    result.returned_to_entry_map = result.final_map_id == options.entry_map_id;
                    return result;
                }
                if (callbacks.reset_entry_failures) {
                    (void)callbacks.reset_entry_failures(context);
                }
                if (!callbacks.enter_dungeon || !callbacks.enter_dungeon(context)) {
                    Log::Info("%s: %s loop refresh aborted because enter_dungeon failed",
                              LoopPrefix(options),
                              LoopName(options));
                    result.final_map_id = CurrentLoopMapId(callbacks);
                    result.returned_to_entry_map = result.final_map_id == options.entry_map_id;
                    return result;
                }
                continue;
            }

            result.entry_refresh_exhausted = true;
            result.returned_to_entry_map = true;
            result.final_map_id = map_id;
            Log::Info("%s: %s loop exhausted entry refresh retries before progress retryLimit=%d",
                      LoopPrefix(options),
                      LoopName(options),
                      options.max_entry_refresh_retries_before_progress);
            return result;
        } else if (map_id == options.entry_map_id && HasReachedProgressLevel(callbacks, result)) {
            result.completed = true;
            result.returned_to_entry_map = true;
            result.final_map_id = map_id;
            Log::Info("%s: %s loop completed with entry-map return after progress", LoopPrefix(options), LoopName(options));
            return result;
        } else {
            result.unsupported_map = true;
            Log::Info("%s: %s loop exiting on unsupported map=%u", LoopPrefix(options), LoopName(options), map_id);
            return result;
        }

        result.final_map_id = CurrentLoopMapId(callbacks);
        result.objective_completed = IsLoopObjectiveCompleted(callbacks);
        const int next_level_index = FindLoopLevelIndex(result.final_map_id, options);
        if (next_level_index >= 0) {
            MarkProgressLevelReached(next_level_index, callbacks, result, options);
            continue;
        }

        if (result.final_map_id == options.entry_map_id) {
            result.returned_to_entry_map = true;
            result.completed = HasReachedProgressLevel(callbacks, result);
            if (result.completed) {
                Log::Info("%s: %s loop completed with entry-map return after progress",
                          LoopPrefix(options),
                          LoopName(options));
                return result;
            }
            continue;
        }

        if (options.fallback_completion_map_id != 0u &&
            result.final_map_id == options.fallback_completion_map_id &&
            HasReachedProgressLevel(callbacks, result) &&
            result.objective_completed) {
            result.completed = true;
            result.completed_via_fallback_map = true;
            Log::Info("%s: %s loop completed with fallback map=%u after progress",
                      LoopPrefix(options),
                      LoopName(options),
                      result.final_map_id);
            return result;
        }

        Log::Info("%s: %s loop terminating because map=%u (expected entry map=%u)",
                  LoopPrefix(options),
                  LoopName(options),
                  result.final_map_id,
                  options.entry_map_id);
        result.unsupported_map = true;
        return result;
    }
}

} // namespace GWA3::DungeonRouteRunner
