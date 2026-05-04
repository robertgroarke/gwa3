#pragma once

#include <gwa3/advanced/Combat.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonRoute.h>

#include <cstdint>

namespace GWA3::DungeonCombat {
using namespace GWA3::AdvancedCombat;

enum class AggroWaypointPhase : uint8_t {
    BeforeAdvance,
    AfterAdvance,
};

using AggroWaypointHookFn = bool(*)(const DungeonRoute::Waypoint& waypoint,
                                    int waypointIndex,
                                    DungeonRoute::WaypointLabelKind labelKind,
                                    AggroWaypointPhase phase,
                                    void* userData);

struct AggroWaypointCallbacks {
    AggroWaypointHookFn on_waypoint = nullptr;
    void* user_data = nullptr;
};

DungeonNavigation::RouteFollowResult FollowWaypointsWithAggro(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    uint32_t mapId,
    const CombatCallbacks& callbacks,
    const DungeonNavigation::RouteFollowOptions& options = {},
    const AggroAdvanceOptions& aggroOptions = {},
    const AggroWaypointCallbacks& waypointCallbacks = {});
} // namespace GWA3::DungeonCombat
