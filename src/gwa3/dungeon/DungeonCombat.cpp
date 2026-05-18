#include <gwa3/dungeon/DungeonCombat.h>

#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>

namespace GWA3::DungeonCombat {

namespace {

bool CallAggroWaypointHook(const AggroWaypointCallbacks& callbacks,
                           const DungeonRoute::Waypoint& waypoint,
                           int waypointIndex,
                           AggroWaypointPhase phase) {
  if (callbacks.on_waypoint == nullptr) {
    return true;
  }

  return callbacks.on_waypoint(
      waypoint,
      waypointIndex,
      DungeonRoute::ClassifyWaypointLabel(waypoint.label),
      phase,
      callbacks.user_data);
}

} // namespace

DungeonNavigation::RouteFollowResult
FollowWaypointsWithAggro(const DungeonRoute::Waypoint* waypoints,
                         int count,
                         uint32_t mapId,
                         const CombatCallbacks& callbacks,
                         const DungeonNavigation::RouteFollowOptions& options,
                         const AggroAdvanceOptions& aggroOptions,
                         const AggroWaypointCallbacks& waypointCallbacks) {
  DungeonNavigation::RouteFollowResult result;
  if (waypoints == nullptr || count <= 0) {
    result.failed_index = 0;
    return result;
  }

  auto* me = AgentMgr::GetMyAgent();
  if (me == nullptr) {
    result.failed_index = 0;
    return result;
  }

  int i = DungeonRoute::FindNearestWaypointIndex(waypoints, count, me->x, me->y);
  int retriesUsed = 0;
  bool replayingBacktrack = false;
  while (i < count) {
    if (mapId != 0u && MapMgr::GetMapId() != mapId) {
      result.map_changed = true;
      result.retries_used = retriesUsed;
      return result;
    }

    float tolerance = options.default_tolerance;
    float fightRange = aggroOptions.clear_options.minimum_engage_range;
    if (waypoints[i].fight_range > 0.0f) {
      fightRange = waypoints[i].fight_range;
      if (options.use_waypoint_fight_range_as_tolerance) {
        tolerance = waypoints[i].fight_range;
      }
    }

    auto waypointOptions = aggroOptions;
    waypointOptions.arrival_threshold = tolerance;
    waypointOptions.timeout_ms = options.waypoint_timeout_ms;
    waypointOptions.move_wait_ms = options.reissue_ms;

    const bool alreadyAtBacktrackWaypoint =
        replayingBacktrack &&
        callbacks.queue_move != nullptr &&
        AdvancedCombat::DistanceToPoint(waypoints[i].x, waypoints[i].y) <= tolerance;
    const bool beforeAdvanceOk =
        CallAggroWaypointHook(waypointCallbacks, waypoints[i], i, AggroWaypointPhase::BeforeAdvance);
    if (beforeAdvanceOk && alreadyAtBacktrackWaypoint) {
      callbacks.queue_move(waypoints[i].x, waypoints[i].y);
    }

    bool waypointCompleted = false;
    if (beforeAdvanceOk &&
        AdvancedCombat::AdvanceWithAggro(waypoints[i].x, waypoints[i].y, fightRange, callbacks, waypointOptions) &&
        CallAggroWaypointHook(waypointCallbacks, waypoints[i], i, AggroWaypointPhase::AfterAdvance)) {
      waypointCompleted = true;
    }

    if (waypointCompleted) {
      ++i;
      replayingBacktrack = false;
      continue;
    }

    if (mapId != 0u && MapMgr::GetMapId() != mapId) {
      result.map_changed = true;
      result.retries_used = retriesUsed;
      return result;
    }

    if (callbacks.is_dead != nullptr && callbacks.is_dead()) {
      result.failed_index = i;
      result.retries_used = retriesUsed;
      return result;
    }

    if (retriesUsed >= options.max_backtrack_retries) {
      result.failed_index = i;
      result.retries_used = retriesUsed;
      return result;
    }

    me = AgentMgr::GetMyAgent();
    int nearestIndex = i;
    if (me != nullptr) {
      nearestIndex = DungeonRoute::FindNearestWaypointIndex(waypoints, count, me->x, me->y);
    }
    if (nearestIndex < i) {
      // Preserve confirmed forward progress when combat or knockback pulls the player back.
      nearestIndex = i;
    }

    int backtrackIndex =
        DungeonRoute::ComputeStuckBacktrackIndex(nearestIndex, options.backtrack_count);
    if (backtrackIndex >= i && i > 0) {
      backtrackIndex = i - 1;
    }

    i = backtrackIndex;
    replayingBacktrack = true;
    ++retriesUsed;
  }

  result.completed = true;
  result.retries_used = retriesUsed;
  return result;
}

} // namespace GWA3::DungeonCombat
