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

bool HasConfiguredWipeRecovery(const DungeonCheckpoint::WaypointWipeRecoveryOptions& recovery) {
    return recovery.is_dead != nullptr ||
           recovery.wait_ms != nullptr ||
           recovery.return_to_outpost != nullptr ||
           recovery.use_dp_removal != nullptr ||
           recovery.wipe_count != nullptr ||
           recovery.get_nearest_waypoint != nullptr;
}

DungeonCheckpoint::WaypointWipeRecoveryOptions BuildWipeRecoveryOptions(
    const DungeonCheckpoint::WaypointWipeRecoveryOptions& base,
    const DungeonRoute::Waypoint* waypoints,
    int count,
    BoolFn is_dead) {
    auto recovery = base;
    recovery.waypoints = waypoints;
    recovery.waypoint_count = count;
    recovery.nearest_index = DungeonNavigation::GetNearestWaypointIndex(waypoints, count);
    if (!recovery.is_dead) {
        recovery.is_dead = is_dead;
    }
    if (!recovery.wait_ms) {
        recovery.wait_ms = &DungeonRuntime::WaitMs;
    }
    if (!recovery.get_nearest_waypoint) {
        recovery.get_nearest_waypoint = &DungeonNavigation::GetNearestWaypointIndex;
    }
    return recovery;
}

bool RecoverRouteWipeWithOptions(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int current_index,
    int& out_restart_index,
    DungeonCheckpoint::WaypointWipeRecoveryContext context,
    const DungeonCheckpoint::WaypointWipeRecoveryOptions& recovery_options,
    BoolFn is_dead,
    const char* log_prefix) {
    if (!HasConfiguredWipeRecovery(recovery_options) && !is_dead) {
        return false;
    }

    DungeonCheckpoint::RouteWipeRecoveryOptions options;
    options.waypoints = waypoints;
    options.waypoint_count = count;
    options.current_index = current_index;
    options.context = context;
    options.log_prefix = log_prefix ? log_prefix : "Dungeon";
    options.recovery = BuildWipeRecoveryOptions(recovery_options, waypoints, count, is_dead);

    const auto recovery = DungeonCheckpoint::RecoverRouteWaypointWipe(options);
    if (recovery.returned_to_outpost) {
        return false;
    }
    out_restart_index = recovery.restart_index;
    return true;
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

const char* PrefixOrDefault(const char* prefix) {
    return prefix ? prefix : "Dungeon";
}

void LogDefaultQuestDoorDiagnostic(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int waypoint_index,
    const RouteLabelExecutorOptions& options) {
    if (!waypoints || waypoint_index < 0 || waypoint_index >= count) return;

    auto* me = AgentMgr::GetMyAgent();
    const float my_x = me ? me->x : 0.0f;
    const float my_y = me ? me->y : 0.0f;
    const float dist_to_checkpoint = me
        ? AgentMgr::GetDistance(my_x, my_y, waypoints[waypoint_index].x, waypoints[waypoint_index].y)
        : -1.0f;
    Log::Info("%s: Quest Door Checkpoint diag: pos=(%.0f, %.0f) distToCheckpoint=%.0f alive=%d mapLoaded=%d",
              PrefixOrDefault(options.log_prefix),
              my_x,
              my_y,
              dist_to_checkpoint,
              me && me->hp > 0.0f ? 1 : 0,
              MapMgr::GetIsMapLoaded() ? 1 : 0);
}

void LogRouteLabelState(const char* stage,
                        const DungeonRoute::Waypoint* waypoints,
                        int count,
                        int waypoint_index,
                        const RouteLabelExecutorOptions& options) {
    if (options.log_waypoint_state) {
        options.log_waypoint_state(stage, waypoints, count, waypoint_index);
    }
}

WaypointHandlerResult RecoverRouteLabelWipeIfDead(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int& waypoint_index,
    DungeonCheckpoint::WaypointWipeRecoveryContext context,
    const RouteLabelExecutorOptions& options) {
    if (!options.is_dead || !options.is_dead()) {
        return WaypointHandlerResult::NotHandled;
    }

    int restart_index = waypoint_index;
    if (options.recover_wipe) {
        if (!options.recover_wipe(waypoints, count, waypoint_index, context, restart_index)) {
            return WaypointHandlerResult::StopRoute;
        }
    } else if (!RecoverRouteWipeWithOptions(
                   waypoints,
                   count,
                   waypoint_index,
                   restart_index,
                   context,
                   options.wipe_recovery,
                   options.is_dead,
                   options.log_prefix)) {
        return WaypointHandlerResult::StopRoute;
    }
    waypoint_index = restart_index;
    return WaypointHandlerResult::ContinueRoute;
}

void UpdateReturnTelemetry(
    const DungeonEntryRecovery::BacktrackReturnResult& result,
    const RouteLabelExecutorOptions& options) {
    if (options.update_return_to_quest_map) {
        options.update_return_to_quest_map(result.final_map_id, result.returned_to_quest_map);
    }
}

void UpdateReturnTelemetry(
    const DungeonEntryRecovery::QuestDoorRecoveryResult& result,
    const RouteLabelExecutorOptions& options) {
    if (options.update_return_to_quest_map) {
        options.update_return_to_quest_map(result.final_map_id, result.returned_to_quest_map);
    }
}

} // namespace

WaypointHandlerResult ExecuteRouteLabelWaypoint(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int& waypoint_index,
    const RouteLabelExecutorOptions& options) {
    if (waypoints == nullptr || waypoint_index < 0 || waypoint_index >= count) {
        return WaypointHandlerResult::NotHandled;
    }

    const auto& waypoint = waypoints[waypoint_index];
    const auto label_kind = DungeonRoute::ClassifyWaypointLabel(waypoint.label);
    switch (label_kind) {
    case DungeonRoute::WaypointLabelKind::Blessing:
        if (options.move_route_waypoint == nullptr || options.grab_blessing == nullptr) {
            return WaypointHandlerResult::NotHandled;
        }
        (void)DungeonNavigation::HandleBlessingWaypoint(
            waypoint,
            options.move_route_waypoint,
            options.grab_blessing);
        LogRouteLabelState("post-blessing-move", waypoints, count, waypoint_index, options);
        return WaypointHandlerResult::ContinueRoute;

    case DungeonRoute::WaypointLabelKind::LevelTransition:
        if (options.handle_level_transition == nullptr) {
            return WaypointHandlerResult::NotHandled;
        }
        options.handle_level_transition(waypoint);
        return WaypointHandlerResult::StopRoute;

    case DungeonRoute::WaypointLabelKind::DungeonKey: {
        const auto move_key = options.move_key_waypoint
            ? options.move_key_waypoint
            : options.move_route_waypoint;
        if (move_key == nullptr || options.acquire_dungeon_key == nullptr) {
            return WaypointHandlerResult::NotHandled;
        }
        (void)move_key(waypoint);
        LogRouteLabelState("post-dungeon-key-move", waypoints, count, waypoint_index, options);
        const bool key_acquired = options.acquire_dungeon_key();
        Log::Info("%s: Dungeon Key step acquired=%d",
                  PrefixOrDefault(options.log_prefix),
                  key_acquired ? 1 : 0);
        if (!key_acquired) {
            Log::Warn("%s: Dungeon Key step failed to secure boss key; returning to outpost for maintenance/recovery",
                      PrefixOrDefault(options.log_prefix));
            if (options.return_to_outpost) {
                options.return_to_outpost();
            }
            if (options.wait_for_map_ready && options.recovery_outpost_map_id != 0u) {
                (void)options.wait_for_map_ready(
                    options.recovery_outpost_map_id,
                    options.recovery_outpost_timeout_ms);
            }
            if (options.update_return_to_quest_map) {
                options.update_return_to_quest_map(MapMgr::GetMapId(), false);
            }
            return WaypointHandlerResult::StopRoute;
        }
        return WaypointHandlerResult::ContinueRoute;
    }

    case DungeonRoute::WaypointLabelKind::DungeonDoor:
        if (options.aggro_move_to == nullptr || options.open_dungeon_door_at == nullptr) {
            return WaypointHandlerResult::NotHandled;
        }
        (void)DungeonNavigation::HandleOpenDungeonDoorWaypoint(
            waypoint,
            options.aggro_move_to,
            options.open_dungeon_door_at);
        LogRouteLabelState("post-dungeon-door-move", waypoints, count, waypoint_index, options);
        return WaypointHandlerResult::ContinueRoute;

    case DungeonRoute::WaypointLabelKind::DungeonDoorCheckpoint: {
        if (options.move_route_waypoint == nullptr ||
            options.move_checkpoint_waypoint == nullptr ||
            options.get_nearest_waypoint == nullptr) {
            return WaypointHandlerResult::NotHandled;
        }
        (void)options.move_route_waypoint(waypoint);
        LogRouteLabelState("post-dungeon-door-checkpoint-move", waypoints, count, waypoint_index, options);

        const auto recovery = RecoverRouteLabelWipeIfDead(
            waypoints,
            count,
            waypoint_index,
            DungeonCheckpoint::WaypointWipeRecoveryContext::DungeonDoorCheckpoint,
            options);
        if (recovery != WaypointHandlerResult::NotHandled) {
            return recovery;
        }

        DungeonCheckpoint::LockedDoorCheckpointOptions checkpoint_options;
        checkpoint_options.waypoints = waypoints;
        checkpoint_options.waypoint_count = count;
        checkpoint_options.current_index = waypoint_index;
        checkpoint_options.backtrack_steps = options.dungeon_door_backtrack_steps;
        checkpoint_options.move_before_check = false;
        checkpoint_options.log_prefix = PrefixOrDefault(options.log_prefix);
        checkpoint_options.checkpoint_name = "Dungeon Door Checkpoint";
        checkpoint_options.move_waypoint = options.move_checkpoint_waypoint;
        checkpoint_options.get_nearest_waypoint = options.get_nearest_waypoint;
        const auto checkpoint = DungeonCheckpoint::HandleLockedDoorCheckpoint(checkpoint_options);
        waypoint_index = checkpoint.waypoint_index;
        return WaypointHandlerResult::ContinueRoute;
    }

    case DungeonRoute::WaypointLabelKind::QuestDoorCheckpoint: {
        if (options.move_route_waypoint == nullptr ||
            options.move_checkpoint_waypoint == nullptr ||
            options.get_nearest_waypoint == nullptr) {
            return WaypointHandlerResult::NotHandled;
        }

        DungeonEntryRecovery::QuestDoorRecoveryOptions quest_recovery_options;
        quest_recovery_options.quest_id = options.quest_id;
        quest_recovery_options.quest_ready = options.quest_ready;
        quest_recovery_options.waypoints = waypoints;
        quest_recovery_options.waypoint_count = count;
        quest_recovery_options.current_index = waypoint_index;
        quest_recovery_options.backtrack_steps = options.quest_door_backtrack_steps;
        quest_recovery_options.move_waypoint = options.move_checkpoint_waypoint;
        quest_recovery_options.after_move_waypoint = options.after_quest_door_backtrack_waypoint;
        quest_recovery_options.return_to_quest_map = options.return_to_quest_map;
        quest_recovery_options.log_prefix = PrefixOrDefault(options.log_prefix);
        quest_recovery_options.label = "Quest Door Checkpoint";
        const auto missing_quest_recovery =
            DungeonEntryRecovery::HandleQuestDoorRecovery(quest_recovery_options);
        if (missing_quest_recovery.recovery_triggered) {
            Log::Info("%s: Quest Door Checkpoint reached without quest; returning to quest map for refresh",
                      PrefixOrDefault(options.log_prefix));
            LogRouteLabelState("quest-door-missing-quest-refresh-trigger", waypoints, count, waypoint_index, options);
            UpdateReturnTelemetry(missing_quest_recovery, options);
            return WaypointHandlerResult::StopRoute;
        }

        (void)options.move_route_waypoint(waypoint);
        LogRouteLabelState("post-quest-door-checkpoint-move", waypoints, count, waypoint_index, options);

        const auto recovery = RecoverRouteLabelWipeIfDead(
            waypoints,
            count,
            waypoint_index,
            DungeonCheckpoint::WaypointWipeRecoveryContext::QuestDoorCheckpoint,
            options);
        if (recovery != WaypointHandlerResult::NotHandled) {
            return recovery;
        }

        if (options.quest_door_diagnostic) {
            options.quest_door_diagnostic(waypoints, waypoint_index);
        } else {
            LogDefaultQuestDoorDiagnostic(waypoints, count, waypoint_index, options);
        }

        const int nearest = options.get_nearest_waypoint(waypoints, count);
        if (nearest < waypoint_index) {
            Log::Info("%s: Failed first door at wp %d; nearest=%d, returning to quest map for refresh",
                      PrefixOrDefault(options.log_prefix),
                      waypoint_index,
                      nearest);
            LogRouteLabelState("quest-door-refresh-trigger", waypoints, count, waypoint_index, options);
            DungeonEntryRecovery::BacktrackReturnOptions return_options;
            return_options.waypoints = waypoints;
            return_options.waypoint_count = count;
            return_options.current_index = waypoint_index;
            return_options.backtrack_steps = options.quest_door_backtrack_steps;
            return_options.move_waypoint = options.move_checkpoint_waypoint;
            return_options.after_move_waypoint = options.after_quest_door_backtrack_waypoint;
            return_options.return_to_quest_map = options.return_to_quest_map;
            return_options.log_prefix = PrefixOrDefault(options.log_prefix);
            return_options.label = "Quest Door Checkpoint";
            const auto returned = DungeonEntryRecovery::ReplayBacktrackAndReturnToQuestMap(return_options);
            UpdateReturnTelemetry(returned, options);
            return WaypointHandlerResult::StopRoute;
        }
        Log::Info("%s: Quest Door Checkpoint reached at wp %d nearest=%d",
                  PrefixOrDefault(options.log_prefix),
                  waypoint_index,
                  nearest);
        return WaypointHandlerResult::ContinueRoute;
    }

    case DungeonRoute::WaypointLabelKind::Boss:
        if (options.handle_boss_reward == nullptr ||
            std::strcmp(waypoint.label ? waypoint.label : "", options.boss_label ? options.boss_label : "Boss") != 0) {
            return WaypointHandlerResult::NotHandled;
        }
        options.handle_boss_reward(waypoint);
        return WaypointHandlerResult::StopRoute;

    default:
        return WaypointHandlerResult::NotHandled;
    }
}

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
            const bool recovered = callbacks.recover_wipe
                ? callbacks.recover_wipe(waypoints, count, i, restart_index)
                : RecoverRouteWipeWithOptions(
                    waypoints,
                    count,
                    i,
                    restart_index,
                    DungeonCheckpoint::WaypointWipeRecoveryContext::Standard,
                    options.wipe_recovery,
                    callbacks.is_dead,
                    options.log_prefix);
            if (!recovered) {
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
            : (options.execute_route_label_waypoints
                ? ExecuteRouteLabelWaypoint(waypoints, count, i, options.route_label_options)
                : WaypointHandlerResult::NotHandled);
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
            const bool recovered = callbacks.recover_wipe
                ? callbacks.recover_wipe(waypoints, count, i, restart_index)
                : RecoverRouteWipeWithOptions(
                    waypoints,
                    count,
                    i,
                    restart_index,
                    DungeonCheckpoint::WaypointWipeRecoveryContext::Standard,
                    options.wipe_recovery,
                    callbacks.is_dead,
                    options.log_prefix);
            if (!recovered) {
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
