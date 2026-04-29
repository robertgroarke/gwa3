#pragma once

#include <gwa3/dungeon/DungeonRoute.h>
#include <gwa3/dungeon/DungeonRuntime.h>

#include <cstdint>

namespace GWA3::DungeonRouteRunner {

enum class WaypointHandlerResult : uint8_t {
    NotHandled,
    ContinueRoute,
    StopRoute,
};

struct RouteRunResult {
    bool completed = false;
    bool stopped = false;
    bool map_changed = false;
    int final_index = -1;
};

using BoolFn = bool(*)();
using GetMapIdFn = uint32_t(*)();
using ResolveStartIndexFn = int(*)(const DungeonRoute::Waypoint* waypoints, int count, uint32_t map_id);
using IsRouteMapFn = bool(*)(uint32_t map_id);
using LogWaypointStateFn = void(*)(const char* stage,
                                   const DungeonRoute::Waypoint* waypoints,
                                   int count,
                                   int waypoint_index);
using WipeRecoveryFn = bool(*)(const DungeonRoute::Waypoint* waypoints,
                               int count,
                               int current_index,
                               int& out_restart_index);
using SpecialWaypointHandlerFn = WaypointHandlerResult(*)(const DungeonRoute::Waypoint* waypoints,
                                                          int count,
                                                          int& waypoint_index);
using MoveStandardWaypointFn = void(*)(const DungeonRoute::Waypoint& waypoint,
                                       int waypoint_index);
using TelemetryUpdateFn = void(*)(int waypoint_index,
                                  const DungeonRoute::Waypoint& waypoint);
using LogWaypointFn = void(*)(int waypoint_index,
                              const DungeonRoute::Waypoint& waypoint);
using VoidFn = void(*)();
using ContextBoolFn = bool(*)(const char* context);
using LevelEventFn = void(*)(int level_index);
using LevelRouteFn = void(*)(const DungeonRoute::Waypoint* waypoints, int count, bool ignore_bot_running);
using LevelRouteLogFn = void(*)(int level_index, const RouteRunResult& route_result);

struct RouteRunCallbacks {
    GetMapIdFn get_map_id = nullptr;
    BoolFn is_bot_running = nullptr;
    BoolFn is_dead = nullptr;
    ResolveStartIndexFn resolve_start_index = nullptr;
    IsRouteMapFn is_route_map = nullptr;
    LogWaypointStateFn log_waypoint_state = nullptr;
    WipeRecoveryFn recover_wipe = nullptr;
    SpecialWaypointHandlerFn handle_special_waypoint = nullptr;
    MoveStandardWaypointFn move_standard_waypoint = nullptr;
    TelemetryUpdateFn update_telemetry = nullptr;
    LogWaypointFn log_waypoint = nullptr;
};

struct RouteRunOptions {
    bool ignore_bot_running = false;
    bool use_nearest_progress_backtrack = true;
    bool log_route_waypoint_state = true;
    const char* log_prefix = "Dungeon";
    const char* route_name = "route";
};

struct DungeonLoopLevel {
    uint32_t map_id = 0u;
    const DungeonRoute::Waypoint* waypoints = nullptr;
    int waypoint_count = 0;
    const char* name = nullptr;
    DungeonRuntime::TransitionAnchor spawn_stale_anchor = {};
    float spawn_stale_anchor_clearance = 0.0f;
    uint32_t spawn_ready_timeout_ms = 0u;
    uint32_t spawn_ready_poll_ms = 200u;
    uint32_t spawn_settle_timeout_ms = 0u;
    float spawn_settle_distance = 0.0f;
};

struct DungeonLoopCallbacks {
    GetMapIdFn get_map_id = nullptr;
    LevelRouteFn follow_waypoints = nullptr;
    BoolFn is_loop_objective_completed = nullptr;
    BoolFn is_progress_level_reached = nullptr;
    VoidFn reset_loop_state = nullptr;
    ContextBoolFn prepare_entry = nullptr;
    ContextBoolFn enter_dungeon = nullptr;
    ContextBoolFn record_entry_failure = nullptr;
    ContextBoolFn reset_entry_failures = nullptr;
    LevelEventFn on_level_started = nullptr;
    LevelEventFn on_progress_level_reached = nullptr;
    LevelRouteLogFn after_level_route = nullptr;
};

struct DungeonLoopOptions {
    const DungeonLoopLevel* levels = nullptr;
    int level_count = 0;
    int progress_level_index = 1;
    uint32_t entry_map_id = 0u;
    uint32_t fallback_completion_map_id = 0u;
    int max_entry_refresh_retries_before_progress = 3;
    bool ignore_bot_running_for_routes = true;
    const char* log_prefix = "Dungeon";
    const char* loop_name = "Dungeon";
    const char* entry_refresh_context = "dungeon-loop-refresh";
};

struct DungeonLoopResult {
    bool completed = false;
    bool returned_to_entry_map = false;
    bool completed_via_fallback_map = false;
    bool reached_progress_level = false;
    bool objective_completed = false;
    bool entry_refresh_exhausted = false;
    bool unsupported_map = false;
    int entry_refresh_retries = 0;
    int last_level_index = -1;
    uint32_t final_map_id = 0u;
};

RouteRunResult RunWaypointRoute(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    const RouteRunCallbacks& callbacks,
    const RouteRunOptions& options = {});
DungeonLoopResult RunDungeonLoop(
    const DungeonLoopCallbacks& callbacks,
    const DungeonLoopOptions& options);

} // namespace GWA3::DungeonRouteRunner
