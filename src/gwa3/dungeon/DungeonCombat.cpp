#include <gwa3/dungeon/DungeonCombat.h>

#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/dungeon/DungeonSkill.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/SkillMgr.h>

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

struct SessionAggroFightContext {
  const SessionAggroFightProfile* profile = nullptr;
};

const SessionAggroFightProfile* g_activeSessionAggroSkillProfile = nullptr;

const SessionAggroFightProfile* ResolveSessionAggroProfile(void* userData) {
  auto* context = static_cast<SessionAggroFightContext*>(userData);
  return context ? context->profile : nullptr;
}

void RecordSessionAggroTarget(void* userData, uint32_t targetId) {
  const auto* profile = ResolveSessionAggroProfile(userData);
  if (profile == nullptr || profile->session == nullptr) {
    return;
  }

  DungeonCombatRoutine::ResetUsedSkills(*profile->session);
  if (profile->on_target != nullptr) {
    profile->on_target(profile->user_data, targetId);
  }
}

void RecordSessionFallbackAutoAttack(void* userData,
                                     uint32_t targetId,
                                     uint32_t actionStartMs) {
  const auto* profile = ResolveSessionAggroProfile(userData);
  if (profile == nullptr || profile->session == nullptr) {
    return;
  }

  DungeonCombatRoutine::RecordAutoAttackAction(
      *profile->session,
      targetId,
      DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE,
      actionStartMs);
}

void RecordSessionAggroAction(void* userData, uint32_t actionStartMs) {
  const auto* profile = ResolveSessionAggroProfile(userData);
  if (profile == nullptr || profile->session == nullptr || profile->on_action == nullptr) {
    return;
  }

  profile->on_action(profile->user_data, profile->session->last_action, actionStartMs);
}

int UseSessionSkillsInAggro(uint32_t targetId,
                            float aggroRange,
                            bool waitForCompletion,
                            const SessionAggroFightProfile& profile) {
  if (profile.session == nullptr) {
    return 0;
  }

  DungeonCombatRoutine::AggroSkillUseOptions skill_options;
  skill_options.wait_for_completion = waitForCompletion;
  skill_options.aggro_range = aggroRange;
  skill_options.max_aftercast = profile.resolve_max_aftercast
      ? profile.resolve_max_aftercast(MapMgr::GetMapId(), profile.user_data)
      : profile.default_max_aftercast;
  skill_options.log_prefix = profile.log_prefix;
  return DungeonCombatRoutine::UseSkillsInAggroTracked(
      *profile.session,
      targetId,
      profile.wait_ms,
      profile.is_dead,
      skill_options);
}

int UseSessionSkillsInAggroCallback(uint32_t targetId,
                                    float aggroRange,
                                    bool waitForCompletion) {
  return g_activeSessionAggroSkillProfile
      ? UseSessionSkillsInAggro(targetId, aggroRange, waitForCompletion, *g_activeSessionAggroSkillProfile)
      : 0;
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

bool HoldForLocalClear(float waypointX,
                       float waypointY,
                       float fightRange,
                       const LocalClearPolicy& policy,
                       const HoldLocalClearCallbacks& callbacks,
                       const HoldLocalClearOptions& options) {
  if (callbacks.wait_ms == nullptr || callbacks.fight_in_aggro == nullptr) {
    return false;
  }

  CombatCallbacks dwellCallbacks = {};
  dwellCallbacks.is_dead = callbacks.is_dead;
  dwellCallbacks.is_map_loaded = callbacks.is_map_loaded;
  dwellCallbacks.wait_ms = callbacks.wait_ms;

  const char* prefix = options.log_prefix ? options.log_prefix : "DungeonCombat";
  const char* clearLabel = policy.clear_label ? policy.clear_label : "Route";
  const char* lootReason = policy.loot_reason ? policy.loot_reason : "local-clear";
  const uint32_t targetId = options.target_id;
  const DWORD localClearStart = GetTickCount();
  int clearPasses = 0;

  while ((GetTickCount() - localClearStart) < policy.local_clear_budget_ms) {
    if (CallBool(callbacks.is_dead, false) ||
        !CallBool(callbacks.is_map_loaded, MapMgr::GetIsMapLoaded()) ||
        PartyMgr::GetIsPartyDefeated()) {
      return false;
    }

    const uint32_t nearbyBefore = CountLivingEnemiesInRange(policy.clear_range);
    if (nearbyBefore == 0u &&
        WaitForEnemyClearDwell(policy.clear_range,
                               policy.quiet_dwell_ms,
                               policy.initial_dwell_timeout_ms,
                               dwellCallbacks,
                               LOCAL_CLEAR_DWELL_POLL_MS)) {
      if (callbacks.post_loot != nullptr) {
        callbacks.post_loot(callbacks.user_data, policy.clear_range, lootReason);
      }
      return true;
    }

    ++clearPasses;
    if (callbacks.on_clear_pass != nullptr) {
      callbacks.on_clear_pass(callbacks.user_data, clearPasses, targetId);
    }

    Log::Info("%s: %s local clear pass=%d target=%u waypoint=(%.0f, %.0f) clearRange=%.0f nearbyBefore=%u",
              prefix,
              clearLabel,
              clearPasses,
              targetId,
              waypointX,
              waypointY,
              policy.clear_range,
              nearbyBefore);

    AgentMgr::CancelAction();
    CallWait(callbacks.wait_ms, LOCAL_CLEAR_PRE_FIGHT_CANCEL_DWELL_MS);
    callbacks.fight_in_aggro(
        policy.clear_range,
        false,
        callbacks.user_data,
        true,
        policy.fight_budget_ms);
    AgentMgr::CancelAction();
    CallWait(callbacks.wait_ms, LOCAL_CLEAR_POST_FIGHT_CANCEL_DWELL_MS);

    const uint32_t nearbyAfter = CountLivingEnemiesInRange(policy.clear_range);
    const float nearestAfter =
        GetNearestLivingEnemyDistance(policy.clear_range + LOCAL_CLEAR_NEAREST_ENEMY_SCAN_PADDING);
    Log::Info("%s: %s local clear result pass=%d target=%u nearbyAfter=%u nearestAfter=%.0f",
              prefix,
              clearLabel,
              clearPasses,
              targetId,
              nearbyAfter,
              nearestAfter);

    if (nearbyAfter == 0u &&
        nearestAfter > (policy.clear_range + LOCAL_CLEAR_EXIT_DISTANCE_PADDING)) {
      if (callbacks.post_loot != nullptr) {
        callbacks.post_loot(callbacks.user_data, policy.clear_range, lootReason);
      }
      return true;
    }

    if (WaitForEnemyClearDwell(policy.clear_range,
                               policy.quiet_dwell_ms,
                               policy.settle_dwell_timeout_ms,
                               dwellCallbacks,
                               LOCAL_CLEAR_DWELL_POLL_MS)) {
      if (callbacks.post_loot != nullptr) {
        callbacks.post_loot(callbacks.user_data, policy.clear_range, lootReason);
      }
      return true;
    }

    if (policy.single_pass && clearPasses >= policy.max_clear_passes) {
      Log::Info("%s: %s local clear early-exit target=%u waypoint=(%.0f, %.0f) nearby=%u nearest=%.0f budget=%lums",
                prefix,
                clearLabel,
                targetId,
                waypointX,
                waypointY,
                CountLivingEnemiesInRange(policy.clear_range),
                GetNearestLivingEnemyDistance(policy.clear_range + LOCAL_CLEAR_NEAREST_ENEMY_SCAN_PADDING),
                static_cast<unsigned long>(GetTickCount() - localClearStart));
      if (callbacks.post_loot != nullptr) {
        callbacks.post_loot(callbacks.user_data, policy.clear_range, lootReason);
      }
      return true;
    }
  }

  Log::Warn("%s: %s local clear timeout target=%u waypoint=(%.0f, %.0f) nearby=%u",
            prefix,
            clearLabel,
            targetId,
            waypointX,
            waypointY,
            CountLivingEnemiesInRange(ComputeLocalClearRange(fightRange)));
  return false;
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

bool FightEnemiesInAggro(float aggroRange,
                         const AggroFightCallbacks& callbacks,
                         const AggroFightOptions& options) {
  if (callbacks.use_skills == nullptr) {
    return false;
  }

  const DWORD fightStart = GetTickCount();
  const bool enableSkillOverride =
      options.restricted_skill_override_map_id != 0u &&
      MapMgr::GetMapId() == options.restricted_skill_override_map_id;
  if (enableSkillOverride) {
    SkillMgr::SetRestrictedMapPlayerUseSkillOverride(true);
  }

  bool ranPass = false;
  while (GetNearestLivingEnemyDistance() <= aggroRange &&
         !CallBool(callbacks.is_dead, false) &&
         MapMgr::GetIsMapLoaded() &&
         !PartyMgr::GetIsPartyDefeated() &&
         (GetTickCount() - fightStart) < options.max_fight_ms) {
    if (options.careful) {
      AgentMgr::CancelAction();
    }

    const uint32_t bestTarget = DungeonSkill::GetBestBalledEnemy(aggroRange);
    if (bestTarget == 0u) {
      break;
    }
    if (callbacks.record_target != nullptr) {
      callbacks.record_target(callbacks.user_data, bestTarget);
    }

    bool attacked = false;
    if (DungeonSkill::CanBasicAttack()) {
      AgentMgr::Attack(bestTarget);
      attacked = true;
    }
    CallWait(callbacks.wait_ms, 100u);

    if (options.careful) {
      if (auto* target = AgentMgr::GetAgentByID(bestTarget)) {
        AgentMgr::Move(target->x, target->y);
      }
      CallWait(callbacks.wait_ms, 300u);
    }

    const DWORD actionStart = GetTickCount();
    const int usedSkills = callbacks.use_skills(
        bestTarget,
        aggroRange,
        options.wait_for_skill_completion);
    if (usedSkills <= 0 && attacked && callbacks.record_auto_attack != nullptr) {
      callbacks.record_auto_attack(callbacks.user_data, bestTarget, actionStart);
    }
    if (callbacks.record_action != nullptr) {
      callbacks.record_action(callbacks.user_data, actionStart);
    }

    ranPass = true;
    CallWait(callbacks.wait_ms, 100u);
  }

  const DWORD elapsedFightMs = GetTickCount() - fightStart;
  if (elapsedFightMs >= options.max_fight_ms &&
      GetNearestLivingEnemyDistance() <= aggroRange &&
      !CallBool(callbacks.is_dead, false) &&
      MapMgr::GetIsMapLoaded()) {
    auto* me = AgentMgr::GetMyAgent();
    Log::Warn("%s: FightEnemiesInAggro budget hit elapsed=%lums aggroRange=%.0f player=(%.0f, %.0f) nearestEnemy=%.0f target=%u",
              options.log_prefix ? options.log_prefix : "DungeonCombat",
              static_cast<unsigned long>(elapsedFightMs),
              aggroRange,
              me ? me->x : 0.0f,
              me ? me->y : 0.0f,
              GetNearestLivingEnemyDistance(aggroRange + 500.0f),
              AgentMgr::GetTargetId());
  }

  if (enableSkillOverride) {
    SkillMgr::SetRestrictedMapPlayerUseSkillOverride(false);
  }

  if (callbacks.post_loot != nullptr) {
    callbacks.post_loot(callbacks.user_data, aggroRange, options.loot_reason);
  }
  return ranPass;
}

bool FightEnemiesInAggroWithSession(float aggroRange,
                                    const SessionAggroFightProfile& profile,
                                    const AggroFightOptions& options) {
  if (profile.session == nullptr) {
    return false;
  }

  SessionAggroFightContext context;
  context.profile = &profile;

  AggroFightCallbacks callbacks = {};
  callbacks.is_dead = profile.is_dead;
  callbacks.wait_ms = profile.wait_ms;
  callbacks.use_skills = &UseSessionSkillsInAggroCallback;
  callbacks.record_target = &RecordSessionAggroTarget;
  callbacks.record_auto_attack = &RecordSessionFallbackAutoAttack;
  callbacks.record_action = &RecordSessionAggroAction;
  callbacks.post_loot = profile.post_loot;
  callbacks.user_data = &context;

  auto fight_options = options;
  if (fight_options.log_prefix == nullptr) {
    fight_options.log_prefix = profile.log_prefix;
  }

  const auto* previousProfile = g_activeSessionAggroSkillProfile;
  g_activeSessionAggroSkillProfile = &profile;
  const bool ranPass = FightEnemiesInAggro(aggroRange, callbacks, fight_options);
  g_activeSessionAggroSkillProfile = previousProfile;
  return ranPass;
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
