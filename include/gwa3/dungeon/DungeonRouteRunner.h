#pragma once

#include <gwa3/dungeon/DungeonRoute.h>

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

RouteRunResult RunWaypointRoute(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    const RouteRunCallbacks& callbacks,
    const RouteRunOptions& options = {});

} // namespace GWA3::DungeonRouteRunner
