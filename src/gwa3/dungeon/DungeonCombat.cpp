#include <gwa3/dungeon/DungeonCombat.h>

#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>

#include <Windows.h>
#include <cmath>

namespace GWA3::DungeonCombat {

namespace {

bool CallBool(BoolFn fn, bool fallback) { return fn ? fn() : fallback; }

bool CallAggroWaypointHook(const AggroWaypointCallbacks &callbacks,
                           const DungeonRoute::Waypoint &waypoint,
                           int waypointIndex, AggroWaypointPhase phase) {
  if (callbacks.on_waypoint == nullptr) {
    return true;
  }

  return callbacks.on_waypoint(
      waypoint, waypointIndex,
      DungeonRoute::ClassifyWaypointLabel(waypoint.label), phase,
      callbacks.user_data);
}

void CallWait(WaitFn fn, uint32_t ms) {
  if (fn) {
    fn(ms);
    return;
  }
  Sleep(ms);
}

float ResolveLocalClearRange(float fightRange,
                             const ClearEnemiesOptions &options) {
  const float expandedRange = fightRange + options.extra_clear_range;
  const float minimumLocalClearRange = options.minimum_local_clear_range > 0.0f
                                           ? options.minimum_local_clear_range
                                           : 0.0f;
  return expandedRange > minimumLocalClearRange ? expandedRange
                                                : minimumLocalClearRange;
}

void PrepareForLocalClear(float routeX, float routeY, float foeDistance,
                          float clearRange, const CombatCallbacks &callbacks,
                          const ClearEnemiesOptions &options) {
  if (!options.hold_movement_for_local_clear) {
    return;
  }

  Log::Info("DungeonCombat: holding movement for local clear target=(%.0f, "
            "%.0f) foeDist=%.0f clearRange=%.0f",
            routeX, routeY, foeDistance, clearRange);
  AgentMgr::CancelAction();
  if (options.pre_clear_cancel_wait_ms > 0u) {
    CallWait(callbacks.wait_ms, options.pre_clear_cancel_wait_ms);
  }
}

void ResumeAfterLocalClear(float routeX, float routeY,
                           const CombatCallbacks &callbacks,
                           const ClearEnemiesOptions &options) {
  if (!options.hold_movement_for_local_clear) {
    return;
  }

  AgentMgr::CancelAction();
  if (options.post_clear_cancel_wait_ms > 0u) {
    CallWait(callbacks.wait_ms, options.post_clear_cancel_wait_ms);
  }
  Log::Info(
      "DungeonCombat: resuming movement after local clear target=(%.0f, %.0f)",
      routeX, routeY);
}

} // namespace

float DistanceToPoint(float x, float y) {
  auto *me = AgentMgr::GetMyAgent();
  if (!me)
    return 999999.0f;
  if (!std::isfinite(me->x) || !std::isfinite(me->y) || !std::isfinite(x) ||
      !std::isfinite(y)) {
    return 999999.0f;
  }

  const float dist = AgentMgr::GetDistance(me->x, me->y, x, y);
  if (!std::isfinite(dist)) {
    return 999999.0f;
  }
  return dist;
}

float GetNearestLivingEnemyDistance(float maxRange) {
  auto *me = AgentMgr::GetMyAgent();
  if (!me)
    return maxRange;

  float bestDistSq = maxRange * maxRange;
  bool found = false;
  const uint32_t maxAgents = AgentMgr::GetMaxAgents();
  for (uint32_t i = 1; i < maxAgents; ++i) {
    auto *agent = AgentMgr::GetAgentByID(i);
    if (!agent || agent->type != 0xDBu)
      continue;
    auto *living = static_cast<AgentLiving *>(agent);
    if (living->allegiance != 3u || living->hp <= 0.0f)
      continue;
    const float distSq =
        AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
    if (distSq < bestDistSq) {
      bestDistSq = distSq;
      found = true;
    }
  }

  return found ? std::sqrt(bestDistSq) : maxRange;
}

bool CanMoveWithEnemyRangeGate(float range, float unrestrictedRange) {
  if (range < unrestrictedRange && GetNearestLivingEnemyDistance() < range) {
    return false;
  }
  return true;
}

uint32_t FindNearestLivingEnemy(float maxRange, float *outDistance) {
  auto *me = AgentMgr::GetMyAgent();
  if (!me)
    return 0u;

  const float maxDistSq = maxRange * maxRange;
  float bestDistSq = maxDistSq;
  uint32_t bestId = 0u;
  const uint32_t maxAgents = AgentMgr::GetMaxAgents();
  for (uint32_t i = 1; i < maxAgents; ++i) {
    auto *agent = AgentMgr::GetAgentByID(i);
    if (!agent || agent->type != 0xDBu)
      continue;
    auto *living = static_cast<AgentLiving *>(agent);
    if (living->allegiance != 3u || living->hp <= 0.0f)
      continue;
    const float distSq =
        AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
    if (distSq < bestDistSq) {
      bestDistSq = distSq;
      bestId = living->agent_id;
    }
  }

  if (outDistance) {
    *outDistance = bestId ? std::sqrt(bestDistSq) : 99999.0f;
  }
  return bestId;
}

uint32_t CountLivingEnemiesInRange(float maxRange) {
  auto *me = AgentMgr::GetMyAgent();
  if (!me)
    return 0u;

  const float maxDistSq = maxRange * maxRange;
  uint32_t count = 0u;
  const uint32_t maxAgents = AgentMgr::GetMaxAgents();
  for (uint32_t i = 1; i < maxAgents; ++i) {
    auto *agent = AgentMgr::GetAgentByID(i);
    if (!agent || agent->type != 0xDBu)
      continue;
    auto *living = static_cast<AgentLiving *>(agent);
    if (living->allegiance != 3u || living->hp <= 0.0f)
      continue;
    const float distSq =
        AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
    if (distSq <= maxDistSq) {
      ++count;
    }
  }
  return count;
}

bool WaitForEnemyClearDwell(float clearRange,
                            uint32_t dwellMs,
                            uint32_t timeoutMs,
                            const CombatCallbacks &callbacks,
                            uint32_t pollMs) {
  const DWORD start = GetTickCount();
  DWORD clearSince = 0u;

  while ((GetTickCount() - start) < timeoutMs) {
    if (CallBool(callbacks.is_dead, false) ||
        !CallBool(callbacks.is_map_loaded, MapMgr::GetIsMapLoaded()) ||
        PartyMgr::GetIsPartyDefeated()) {
      return false;
    }

    const uint32_t nearbyEnemies = CountLivingEnemiesInRange(clearRange);
    if (nearbyEnemies == 0u) {
      if (clearSince == 0u) {
        clearSince = GetTickCount();
      }
      if ((GetTickCount() - clearSince) >= dwellMs) {
        return true;
      }
    } else {
      clearSince = 0u;
    }

    CallWait(callbacks.wait_ms, pollMs);
  }

  return CountLivingEnemiesInRange(clearRange) == 0u;
}

void FlagAllHeroes(float x, float y) {
  if (GameThread::IsInitialized() && !GameThread::IsOnGameThread()) {
    GameThread::EnqueuePost([x, y]() { PartyMgr::FlagAll(x, y); });
    return;
  }
  PartyMgr::FlagAll(x, y);
}

void UnflagAllHeroes() {
  if (GameThread::IsInitialized() && !GameThread::IsOnGameThread()) {
    GameThread::EnqueuePost([]() { PartyMgr::UnflagAll(); });
    return;
  }
  PartyMgr::UnflagAll();
}

bool ClearEnemiesInArea(float fightRange, const CombatCallbacks &callbacks,
                        const ClearEnemiesOptions &options) {
  if (callbacks.wait_ms == nullptr || callbacks.fight_target == nullptr) {
    return false;
  }

  const float clearRange = ResolveLocalClearRange(fightRange, options);
  const DWORD clearStart = GetTickCount();
  DWORD quietStart = 0u;
  uint32_t currentTargetId = 0u;
  DWORD targetFightStart = 0u;
  DWORD lastFlagMs = 0u;
  DWORD lastTargetCallMs = 0u;
  DWORD lastFightMs = 0u;
  DWORD lastAttackMs = 0u;

  while ((GetTickCount() - clearStart) < options.timeout_ms) {
    if (CallBool(callbacks.is_dead, false) ||
        !CallBool(callbacks.is_map_loaded, MapMgr::GetIsMapLoaded())) {
      if (options.flag_heroes) {
        UnflagAllHeroes();
      }
      return false;
    }

    const uint32_t nearbyCount = CountLivingEnemiesInRange(clearRange);
    if (nearbyCount == 0u) {
      if (quietStart == 0u)
        quietStart = GetTickCount();
      if ((GetTickCount() - quietStart) >= options.quiet_confirmation_ms) {
        if (options.flag_heroes) {
          UnflagAllHeroes();
        }
        if (callbacks.pickup_loot && options.pickup_after_clear) {
          callbacks.pickup_loot(options.pickup_range);
        }
        return true;
      }
      CallWait(callbacks.wait_ms, options.idle_wait_ms);
      continue;
    }
    quietStart = 0u;

    float foeDistance = 99999.0f;
    const uint32_t foeId = FindNearestLivingEnemy(clearRange, &foeDistance);
    if (foeId == 0u) {
      CallWait(callbacks.wait_ms, options.idle_wait_ms);
      continue;
    }

    const DWORD now = GetTickCount();
    const bool targetChanged = (foeId != currentTargetId);
    if (targetChanged) {
      currentTargetId = foeId;
      targetFightStart = now;
      lastFlagMs = 0u;
      lastTargetCallMs = 0u;
      lastFightMs = 0u;
      lastAttackMs = 0u;
    } else if ((now - targetFightStart) > options.target_timeout_ms) {
      if (options.flag_heroes) {
        UnflagAllHeroes();
      }
      return false;
    }

    auto *foe = AgentMgr::GetAgentByID(foeId);
    if (!foe) {
      CallWait(callbacks.wait_ms, options.idle_wait_ms);
      continue;
    }
    auto *meCasting = AgentMgr::GetMyAgent();
    if (AgentMgr::IsCasting(meCasting)) {
      CallWait(callbacks.wait_ms, options.loop_wait_ms);
      continue;
    }

    if (options.flag_heroes &&
        (targetChanged || (now - lastFlagMs) >= options.flag_reissue_ms)) {
      FlagAllHeroes(foe->x, foe->y);
      lastFlagMs = now;
    }
    if (options.change_target &&
        (targetChanged ||
         (now - lastTargetCallMs) >= options.target_reissue_ms)) {
      AgentMgr::ChangeTarget(foeId);
      lastTargetCallMs = now;
    }
    if (options.call_target &&
        (targetChanged ||
         (now - lastTargetCallMs) >= options.target_reissue_ms)) {
      AgentMgr::CallTarget(foeId);
      lastTargetCallMs = now;
    }

    if (options.chase_during_clear && foeDistance > options.chase_distance &&
        callbacks.queue_move) {
      callbacks.queue_move(foe->x, foe->y);
      CallWait(callbacks.wait_ms, options.chase_wait_ms);
    }

    bool attackedNow = false;
    if (targetChanged || (now - lastAttackMs) >= options.attack_reissue_ms) {
      AgentMgr::Attack(foeId);
      lastAttackMs = now;
      attackedNow = true;
    }
    if (attackedNow) {
      CallWait(callbacks.wait_ms, 100u);
    }
    if (targetChanged || (now - lastFightMs) >= options.fight_reissue_ms) {
      callbacks.fight_target(foeId);
      lastFightMs = GetTickCount();
    }
    CallWait(callbacks.wait_ms, options.loop_wait_ms);
  }

  if (options.flag_heroes) {
    UnflagAllHeroes();
  }
  return false;
}

bool AdvanceWithAggro(float x, float y, float fightRange,
                      const CombatCallbacks &callbacks,
                      const AggroAdvanceOptions &options) {
  if (callbacks.queue_move == nullptr || callbacks.wait_ms == nullptr) {
    return false;
  }

  const float localClearRange =
      ResolveLocalClearRange(fightRange, options.clear_options);
  if (DistanceToPoint(x, y) <= options.arrival_threshold) {
    return true;
  }

  callbacks.queue_move(x, y);
  const DWORD start = GetTickCount();
  auto *me = AgentMgr::GetMyAgent();
  auto stuckMonitor =
      DungeonNavigation::MakeStuckMonitor(me ? me->x : 0.0f, me ? me->y : 0.0f);

  while (DistanceToPoint(x, y) > options.arrival_threshold &&
         (GetTickCount() - start) < options.timeout_ms) {
    if (CallBool(callbacks.is_dead, false) ||
        !CallBool(callbacks.is_map_loaded, MapMgr::GetIsMapLoaded())) {
      return false;
    }

    auto *meStuck = AgentMgr::GetMyAgent();
    if (meStuck) {
      const auto stuckResolution = DungeonNavigation::EvaluateStuckMonitor(
          meStuck->x, meStuck->y, x, y, stuckMonitor, GetTickCount(),
          options.stuck_minimum_progress, options.stuck_recovery_threshold,
          options.stuck_abort_threshold, options.stuck_recovery_radius);
      if (stuckResolution.issue_recovery_move) {
        callbacks.queue_move(stuckResolution.recovery_x,
                             stuckResolution.recovery_y);
        CallWait(callbacks.wait_ms, options.move_wait_ms);
      } else if (stuckResolution.abort_move) {
        return false;
      }
    }

    float nearestEnemyDist = 99999.0f;
    if (FindNearestLivingEnemy(localClearRange, &nearestEnemyDist) != 0u) {
      PrepareForLocalClear(x, y, nearestEnemyDist, localClearRange, callbacks,
                           options.clear_options);
      if (!ClearEnemiesInArea(fightRange, callbacks, options.clear_options)) {
        return false;
      }
      ResumeAfterLocalClear(x, y, callbacks, options.clear_options);
      callbacks.queue_move(x, y);
      continue;
    }

    callbacks.queue_move(x, y);
    CallWait(callbacks.wait_ms, options.move_wait_ms);
  }

  return DistanceToPoint(x, y) <= options.arrival_threshold;
}

DungeonNavigation::RouteFollowResult
FollowWaypointsWithAggro(const DungeonRoute::Waypoint *waypoints, int count,
                         uint32_t mapId, const CombatCallbacks &callbacks,
                         const DungeonNavigation::RouteFollowOptions &options,
                         const AggroAdvanceOptions &aggroOptions,
                         const AggroWaypointCallbacks &waypointCallbacks) {
  DungeonNavigation::RouteFollowResult result;
  if (waypoints == nullptr || count <= 0) {
    result.failed_index = 0;
    return result;
  }

  auto *me = AgentMgr::GetMyAgent();
  if (me == nullptr) {
    result.failed_index = 0;
    return result;
  }

  int i =
      DungeonRoute::FindNearestWaypointIndex(waypoints, count, me->x, me->y);
  int retriesUsed = 0;
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

    bool waypointCompleted = false;
    if (CallAggroWaypointHook(waypointCallbacks, waypoints[i], i,
                              AggroWaypointPhase::BeforeAdvance) &&
        AdvanceWithAggro(waypoints[i].x, waypoints[i].y, fightRange, callbacks,
                         waypointOptions) &&
        CallAggroWaypointHook(waypointCallbacks, waypoints[i], i,
                              AggroWaypointPhase::AfterAdvance)) {
      waypointCompleted = true;
    }

    if (waypointCompleted) {
      ++i;
      continue;
    }

    if (mapId != 0u && MapMgr::GetMapId() != mapId) {
      result.map_changed = true;
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
      nearestIndex = DungeonRoute::FindNearestWaypointIndex(waypoints, count,
                                                            me->x, me->y);
    }
    if (nearestIndex < i) {
      // Preserve confirmed forward progress: route retries should backtrack
      // from the failed waypoint, not from whichever earlier waypoint the
      // player drifted closest to during combat or knockback.
      nearestIndex = i;
    }

    int backtrackIndex = DungeonRoute::ComputeStuckBacktrackIndex(
        nearestIndex, options.backtrack_count);
    if (backtrackIndex >= i && i > 0) {
      backtrackIndex = i - 1;
    }

    i = backtrackIndex;
    ++retriesUsed;
  }

  result.completed = true;
  result.retries_used = retriesUsed;
  return result;
}

} // namespace GWA3::DungeonCombat
