#include <gwa3/advanced/Combat.h>

#include <gwa3/advanced/CombatRoutine.h>
#include <gwa3/advanced/Waypoint.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/advanced/CombatSkill.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>
#include <cmath>

namespace GWA3::AdvancedCombat {

namespace {

bool CallBool(BoolFn fn, bool fallback) { return fn ? fn() : fallback; }

void CallWait(WaitFn fn, uint32_t ms) {
  if (fn) {
    fn(ms);
    return;
  }
  Sleep(ms);
}

constexpr uint32_t LOCAL_CLEAR_COMMAND_SETTLE_TIMEOUT_MS = 2500u;
constexpr uint32_t LOCAL_CLEAR_COMMAND_SETTLE_QUIET_MS = 250u;
constexpr uint32_t LOCAL_CLEAR_COMMAND_SETTLE_POLL_MS = 50u;

bool IsCombatCommandLaneSettled() {
  return AgentMgr::IsCombatCommandSafe();
}

bool WaitForCombatCommandSettle(
    const char* label,
    const CombatCallbacks& callbacks,
    uint32_t timeoutMs = LOCAL_CLEAR_COMMAND_SETTLE_TIMEOUT_MS,
    uint32_t quietMs = LOCAL_CLEAR_COMMAND_SETTLE_QUIET_MS) {
  const DWORD start = GetTickCount();
  DWORD quietStart = 0u;

  while ((GetTickCount() - start) < timeoutMs) {
    if (CallBool(callbacks.is_dead, false) ||
        !CallBool(callbacks.is_map_loaded, MapMgr::GetIsMapLoaded()) ||
        PartyMgr::GetIsPartyDefeated()) {
      return false;
    }

    if (IsCombatCommandLaneSettled()) {
      if (quietStart == 0u) {
        quietStart = GetTickCount();
      }
      if ((GetTickCount() - quietStart) >= quietMs) {
        return true;
      }
    } else {
      quietStart = 0u;
    }

    CallWait(callbacks.wait_ms, LOCAL_CLEAR_COMMAND_SETTLE_POLL_MS);
  }

  const auto* me = AgentMgr::GetMyAgent();
  const bool moving = me && (std::fabs(me->move_x) > 0.01f ||
                             std::fabs(me->move_y) > 0.01f);
  const bool queueIdle = !CtoS::Initialize() || CtoS::IsBotshubQueueIdle();
  Log::Warn("Combat: command settle timeout label=%s moving=%d queueIdle=%d safe=%d",
            label != nullptr ? label : "",
            moving ? 1 : 0,
            queueIdle ? 1 : 0,
            AgentMgr::IsCombatCommandSafe() ? 1 : 0);
  return false;
}

struct SessionAggroFightContext {
  const SessionAggroFightProfile* profile = nullptr;
  void* post_loot_user_data = nullptr;
};

const SessionAggroFightProfile* g_activeSessionAggroSkillProfile = nullptr;

bool IsPlayerCarryingCombatBundle() {
  const auto* me = AgentMgr::GetMyAgent();
  if (!me) {
    return false;
  }

  if (me->weapon_item_type == 6u || me->offhand_item_type == 6u) {
    return true;
  }

  // Raven's Point torches can present as an equipped item with no normal
  // weapon type; attacking in that state is a no-op, but call target works.
  return me->weapon_item_id != 0u && me->weapon_type == 0u &&
         me->weapon_item_type == 0u;
}

const SessionAggroFightProfile* ResolveSessionAggroProfile(void* userData) {
  auto* context = static_cast<SessionAggroFightContext*>(userData);
  return context ? context->profile : nullptr;
}

void RecordSessionAggroTarget(void* userData, uint32_t targetId) {
  const auto* profile = ResolveSessionAggroProfile(userData);
  if (profile == nullptr || profile->session == nullptr) {
    return;
  }

  AdvancedCombatRoutine::ResetUsedSkills(*profile->session);
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

  AdvancedCombatRoutine::RecordAutoAttackAction(
      *profile->session,
      targetId,
      AdvancedSkill::ROLE_ATTACK | AdvancedSkill::ROLE_OFFENSIVE,
      actionStartMs);
}

void RecordSessionAggroAction(void* userData, uint32_t actionStartMs) {
  const auto* profile = ResolveSessionAggroProfile(userData);
  if (profile == nullptr || profile->session == nullptr || profile->on_action == nullptr) {
    return;
  }

  profile->on_action(profile->user_data, profile->session->last_action, actionStartMs);
}

void RecordSessionPostLoot(void* userData, float aggroRange, const char* reason) {
  auto* context = static_cast<SessionAggroFightContext*>(userData);
  const auto* profile = context ? context->profile : nullptr;
  if (profile == nullptr || profile->post_loot == nullptr) {
    return;
  }

  profile->post_loot(context->post_loot_user_data, aggroRange, reason);
}

void RecordRouteCombatTargetStats(void* userData, uint32_t targetId) {
  auto* stats = static_cast<RouteCombatStats*>(userData);
  if (stats == nullptr) {
    return;
  }

  ++stats->quick_step_attempts;
  stats->last_target_id = targetId;
}

void RecordRouteCombatActionStats(
    void* userData,
    const AdvancedCombatRoutine::SkillActionResult& action,
    uint32_t actionStartMs) {
  auto* stats = static_cast<RouteCombatStats*>(userData);
  if (stats == nullptr || !action.valid || action.started_at_ms < actionStartMs) {
    return;
  }

  if (action.used_skill) {
    ++stats->skill_steps;
  } else if (action.auto_attack) {
    ++stats->auto_attack_steps;
  }
}

void RecordRouteLocalClearPassStats(void* userData, int, uint32_t targetId) {
  auto* context = static_cast<RouteCombatContext*>(userData);
  if (context == nullptr || context->stats == nullptr) {
    return;
  }

  ++context->stats->settle_requests;
  context->stats->last_target_id = targetId;
}

void RouteCombatPostLoot(void* userData, float aggroRange, const char* reason) {
  auto* context = static_cast<RouteCombatContext*>(userData);
  if (context == nullptr || context->post_combat_loot == nullptr) {
    return;
  }

  (void)context->post_combat_loot(aggroRange, reason);
}

void RouteLocalClearFightInAggro(
    float aggroRange,
    bool careful,
    void* userData,
    bool waitForSkillCompletion,
    uint32_t maxFightMs) {
  FightEnemiesInAggroFromRouteContext(
      aggroRange,
      careful,
      userData,
      waitForSkillCompletion,
      maxFightMs);
}

int UseSessionSkillsInAggro(uint32_t targetId,
                            float aggroRange,
                            bool waitForCompletion,
                            const SessionAggroFightProfile& profile) {
  if (profile.session == nullptr) {
    return 0;
  }

  AdvancedCombatRoutine::AggroSkillUseOptions skill_options;
  skill_options.wait_for_completion = waitForCompletion;
  skill_options.aggro_range = aggroRange;
  skill_options.max_aftercast = profile.resolve_max_aftercast
      ? profile.resolve_max_aftercast(MapMgr::GetMapId(), profile.user_data)
      : profile.default_max_aftercast;
  skill_options.log_prefix = profile.log_prefix;
  return AdvancedCombatRoutine::UseSkillsInAggroTracked(
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

  Log::Info("Combat: holding movement for local clear target=(%.0f, "
            "%.0f) foeDist=%.0f clearRange=%.0f",
            routeX, routeY, foeDistance, clearRange);
  AgentMgr::CancelAction();
  if (options.pre_clear_cancel_wait_ms > 0u) {
    CallWait(callbacks.wait_ms, options.pre_clear_cancel_wait_ms);
  }
  (void)WaitForCombatCommandSettle("local-clear", callbacks);
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
      "Combat: resuming movement after local clear target=(%.0f, %.0f)",
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

  const char* prefix = options.log_prefix ? options.log_prefix : "Combat";
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
    (void)WaitForCombatCommandSettle(clearLabel, dwellCallbacks);
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
      const uint32_t nearbyAtLimit = CountLivingEnemiesInRange(policy.clear_range);
      const float nearestAtLimit =
          GetNearestLivingEnemyDistance(policy.clear_range + LOCAL_CLEAR_NEAREST_ENEMY_SCAN_PADDING);
      if (nearbyAtLimit <= 3u ||
          nearestAtLimit > (policy.clear_range + LOCAL_CLEAR_EXIT_DISTANCE_PADDING)) {
        Log::Info("%s: %s local clear early-exit target=%u waypoint=(%.0f, %.0f) nearby=%u nearest=%.0f budget=%lums",
                prefix,
                clearLabel,
                targetId,
                waypointX,
                waypointY,
                nearbyAtLimit,
                nearestAtLimit,
                static_cast<unsigned long>(GetTickCount() - localClearStart));
        if (callbacks.post_loot != nullptr) {
          callbacks.post_loot(callbacks.user_data, policy.clear_range, lootReason);
        }
        return true;
      }

      Log::Warn("%s: %s local clear held route target=%u waypoint=(%.0f, %.0f) nearby=%u nearest=%.0f after %d passes",
                prefix,
                clearLabel,
                targetId,
                waypointX,
                waypointY,
                nearbyAtLimit,
                nearestAtLimit,
                clearPasses);
      return false;
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

bool FlagAllHeroes(float x, float y) {
  if (!AgentMgr::IsCombatCommandSafe()) {
    return false;
  }
  if (GameThread::IsInitialized() && !GameThread::IsOnGameThread()) {
    GameThread::EnqueuePost([x, y]() {
      if (!AgentMgr::IsCombatCommandSafe()) {
        Log::Info("Combat: skipped queued hero flag until command lane settles");
        return;
      }
      PartyMgr::FlagAll(x, y);
    });
    return true;
  }
  PartyMgr::FlagAll(x, y);
  return true;
}

void UnflagAllHeroes() {
  if (GameThread::IsInitialized() && !GameThread::IsOnGameThread()) {
    GameThread::EnqueuePost([]() {
      if (!AgentMgr::IsCombatCommandSafe()) {
        Log::Info("Combat: skipped queued hero unflag until command lane settles");
        return;
      }
      PartyMgr::UnflagAll();
    });
    return;
  }
  if (!AgentMgr::IsCombatCommandSafe()) {
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
  uint32_t targetTimeoutResets = 0u;
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
      targetTimeoutResets = 0u;
      lastFlagMs = 0u;
      lastTargetCallMs = 0u;
      lastFightMs = 0u;
      lastAttackMs = 0u;
    } else if ((now - targetFightStart) > options.target_timeout_ms) {
      auto *meTimeout = AgentMgr::GetMyAgent();
      const uint32_t remainingNearby = CountLivingEnemiesInRange(clearRange);
      if (remainingNearby == 0u) {
        if (options.flag_heroes) {
          UnflagAllHeroes();
        }
        if (callbacks.pickup_loot && options.pickup_after_clear) {
          callbacks.pickup_loot(options.pickup_range);
        }
        return true;
      }
      auto *timeoutTarget = AgentMgr::GetAgentByID(foeId);
      if (targetTimeoutResets < 2u || timeoutTarget == nullptr) {
        ++targetTimeoutResets;
        Log::Warn("Combat: ClearEnemiesInArea target stale/reset target=%u "
                  "fightRange=%.0f clearRange=%.0f foeDist=%.0f "
                  "nearby=%u reset=%u player=(%.0f, %.0f) targetValid=%d",
                  foeId, fightRange, clearRange, foeDistance, remainingNearby,
                  targetTimeoutResets,
                  meTimeout ? meTimeout->x : 0.0f,
                  meTimeout ? meTimeout->y : 0.0f,
                  timeoutTarget != nullptr ? 1 : 0);
        if (timeoutTarget == nullptr) {
          currentTargetId = 0u;
        }
        targetFightStart = now;
        lastFlagMs = 0u;
        lastTargetCallMs = 0u;
        lastFightMs = 0u;
        lastAttackMs = 0u;
        CallWait(callbacks.wait_ms, options.loop_wait_ms);
        continue;
      }
      Log::Warn("Combat: ClearEnemiesInArea target timeout target=%u "
                "fightRange=%.0f clearRange=%.0f foeDist=%.0f "
                "nearby=%u player=(%.0f, %.0f)",
                foeId, fightRange, clearRange, foeDistance, remainingNearby,
                meTimeout ? meTimeout->x : 0.0f,
                meTimeout ? meTimeout->y : 0.0f);
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
    const bool carryingBundle = IsPlayerCarryingCombatBundle();
    auto *meCasting = AgentMgr::GetMyAgent();
    if (AgentMgr::IsCasting(meCasting)) {
      CallWait(callbacks.wait_ms, options.loop_wait_ms);
      continue;
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
    if (carryingBundle &&
        (targetChanged ||
         (now - lastTargetCallMs) >= options.target_reissue_ms)) {
      AgentMgr::CallTarget(foeId);
      lastTargetCallMs = now;
    }
    if (options.flag_heroes &&
        (targetChanged || (now - lastFlagMs) >= options.flag_reissue_ms)) {
      if (FlagAllHeroes(foe->x, foe->y)) {
        lastFlagMs = now;
      }
    }

    if (carryingBundle && foeDistance > BUNDLE_CARRY_SKILL_ENGAGE_RANGE &&
        callbacks.queue_move) {
      callbacks.queue_move(foe->x, foe->y);
      CallWait(callbacks.wait_ms, BUNDLE_CARRY_CHASE_WAIT_MS);
      continue;
    }

    if (options.chase_during_clear && foeDistance > options.chase_distance &&
        callbacks.queue_move) {
      callbacks.queue_move(foe->x, foe->y);
      CallWait(callbacks.wait_ms, options.chase_wait_ms);
    }

    bool attackedNow = false;
    if (!carryingBundle &&
        (targetChanged || (now - lastAttackMs) >= options.attack_reissue_ms)) {
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

  auto *meTimeout = AgentMgr::GetMyAgent();
  const uint32_t finalNearby = CountLivingEnemiesInRange(clearRange);
  if (finalNearby == 0u) {
    if (options.flag_heroes) {
      UnflagAllHeroes();
    }
    if (callbacks.pickup_loot && options.pickup_after_clear) {
      callbacks.pickup_loot(options.pickup_range);
    }
    Log::Info("Combat: ClearEnemiesInArea timeout resolved clean fightRange=%.0f "
              "clearRange=%.0f player=(%.0f, %.0f)",
              fightRange, clearRange, meTimeout ? meTimeout->x : 0.0f,
              meTimeout ? meTimeout->y : 0.0f);
    return true;
  }

  if (options.flag_heroes) {
    UnflagAllHeroes();
  }
  const uint32_t remainingTarget = currentTargetId;
  auto *target = remainingTarget != 0u ? AgentMgr::GetAgentByID(remainingTarget)
                                      : nullptr;
  Log::Warn("Combat: ClearEnemiesInArea timeout fightRange=%.0f clearRange=%.0f "
            "nearby=%u target=%u player=(%.0f, %.0f) targetPos=(%.0f, %.0f)",
            fightRange, clearRange, finalNearby,
            remainingTarget, meTimeout ? meTimeout->x : 0.0f,
            meTimeout ? meTimeout->y : 0.0f, target ? target->x : 0.0f,
            target ? target->y : 0.0f);
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
      AdvancedWaypoint::MakeStuckMonitor(me ? me->x : 0.0f, me ? me->y : 0.0f);

  while (DistanceToPoint(x, y) > options.arrival_threshold &&
         (GetTickCount() - start) < options.timeout_ms) {
    const bool isDead = CallBool(callbacks.is_dead, false);
    const bool isMapLoaded =
        CallBool(callbacks.is_map_loaded, MapMgr::GetIsMapLoaded());
    if (isDead || !isMapLoaded) {
      auto *meAbort = AgentMgr::GetMyAgent();
      Log::Warn("Combat: AdvanceWithAggro abort target=(%.0f, %.0f) "
                "dead=%d mapLoaded=%d partyDefeated=%d player=(%.0f, %.0f)",
                x, y, isDead ? 1 : 0, isMapLoaded ? 1 : 0,
                PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                meAbort ? meAbort->x : 0.0f, meAbort ? meAbort->y : 0.0f);
      return false;
    }

    auto *meStuck = AgentMgr::GetMyAgent();
    if (meStuck) {
      const auto stuckResolution = AdvancedWaypoint::EvaluateStuckMonitor(
          meStuck->x, meStuck->y, x, y, stuckMonitor, GetTickCount(),
          options.stuck_minimum_progress, options.stuck_recovery_threshold,
          options.stuck_abort_threshold, options.stuck_recovery_radius);
      if (stuckResolution.issue_recovery_move) {
        Log::Warn("Combat: AdvanceWithAggro recovery target=(%.0f, %.0f) "
                  "player=(%.0f, %.0f) recovery=(%.0f, %.0f) "
                  "dist=%.0f lowMove=%d clearRange=%.0f",
                  x, y, meStuck->x, meStuck->y,
                  stuckResolution.recovery_x, stuckResolution.recovery_y,
                  DistanceToPoint(x, y), stuckMonitor.low_movement_count,
                  localClearRange);
        callbacks.queue_move(stuckResolution.recovery_x,
                             stuckResolution.recovery_y);
        CallWait(callbacks.wait_ms, options.move_wait_ms);
      } else if (stuckResolution.abort_move) {
        Log::Warn("Combat: AdvanceWithAggro stuck abort target=(%.0f, %.0f) "
                  "player=(%.0f, %.0f) dist=%.0f lowMove=%d "
                  "clearRange=%.0f nearby=%u",
                  x, y, meStuck->x, meStuck->y, DistanceToPoint(x, y),
                  stuckMonitor.low_movement_count, localClearRange,
                  CountLivingEnemiesInRange(localClearRange));
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

  const float finalDist = DistanceToPoint(x, y);
  const bool arrived = finalDist <= options.arrival_threshold;
  if (!arrived) {
    auto *meTimeout = AgentMgr::GetMyAgent();
    Log::Warn("Combat: AdvanceWithAggro timeout target=(%.0f, %.0f) "
              "player=(%.0f, %.0f) dist=%.0f threshold=%.0f "
              "clearRange=%.0f nearby=%u elapsed=%lums",
              x, y, meTimeout ? meTimeout->x : 0.0f,
              meTimeout ? meTimeout->y : 0.0f, finalDist,
              options.arrival_threshold, localClearRange,
              CountLivingEnemiesInRange(localClearRange),
              static_cast<unsigned long>(GetTickCount() - start));
  }
  return arrived;
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

    const uint32_t bestTarget = AdvancedSkill::GetBestBalledEnemy(aggroRange);
    if (bestTarget == 0u) {
      break;
    }
    if (callbacks.record_target != nullptr) {
      callbacks.record_target(callbacks.user_data, bestTarget);
    }

    const bool carryingBundle = IsPlayerCarryingCombatBundle();
    if (carryingBundle) {
      AgentMgr::CallTarget(bestTarget);
      if (auto* target = AgentMgr::GetAgentByID(bestTarget)) {
        auto* me = AgentMgr::GetMyAgent();
        if (me && AgentMgr::GetDistance(me->x, me->y, target->x, target->y) >
                      BUNDLE_CARRY_SKILL_ENGAGE_RANGE) {
          AgentMgr::Move(target->x, target->y);
          CallWait(callbacks.wait_ms, BUNDLE_CARRY_CHASE_WAIT_MS);
        }
      }
    }

    bool attacked = false;
    if (!carryingBundle && AdvancedSkill::CanBasicAttack()) {
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
  const bool timedOutWithNearbyEnemy =
      elapsedFightMs >= options.max_fight_ms &&
      GetNearestLivingEnemyDistance() <= aggroRange &&
      !CallBool(callbacks.is_dead, false) &&
      MapMgr::GetIsMapLoaded();
  if (timedOutWithNearbyEnemy) {
    auto* me = AgentMgr::GetMyAgent();
    Log::Warn("%s: FightEnemiesInAggro budget hit elapsed=%lums aggroRange=%.0f player=(%.0f, %.0f) nearestEnemy=%.0f target=%u",
              options.log_prefix ? options.log_prefix : "Combat",
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

  if (callbacks.post_loot != nullptr && !timedOutWithNearbyEnemy) {
    callbacks.post_loot(callbacks.user_data, aggroRange, options.loot_reason);
  } else if (callbacks.post_loot != nullptr && timedOutWithNearbyEnemy) {
    Log::Warn("%s: FightEnemiesInAggro skipping post-loot because enemies remain in range",
              options.log_prefix ? options.log_prefix : "Combat");
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
  context.post_loot_user_data = profile.post_loot_user_data
      ? profile.post_loot_user_data
      : profile.user_data;

  AggroFightCallbacks callbacks = {};
  callbacks.is_dead = profile.is_dead;
  callbacks.wait_ms = profile.wait_ms;
  callbacks.use_skills = &UseSessionSkillsInAggroCallback;
  callbacks.record_target = &RecordSessionAggroTarget;
  callbacks.record_auto_attack = &RecordSessionFallbackAutoAttack;
  callbacks.record_action = &RecordSessionAggroAction;
  callbacks.post_loot = profile.post_loot ? &RecordSessionPostLoot : nullptr;
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

void FightEnemiesInAggroFromRouteContext(float aggroRange,
                                         bool careful,
                                         void* userData,
                                         bool waitForSkillCompletion,
                                         uint32_t maxFightMs) {
  auto* context = static_cast<RouteCombatContext*>(userData);
  if (context == nullptr || context->session == nullptr) {
    return;
  }

  SessionAggroFightProfile profile;
  profile.session = context->session;
  profile.wait_ms = context->wait_ms;
  profile.is_dead = context->is_dead;
  profile.post_loot = &RouteCombatPostLoot;
  profile.on_target = &RecordRouteCombatTargetStats;
  profile.on_action = &RecordRouteCombatActionStats;
  profile.resolve_max_aftercast = context->resolve_max_aftercast;
  profile.user_data = context->stats;
  profile.post_loot_user_data = context;
  profile.default_max_aftercast = context->default_max_aftercast;
  profile.log_prefix = context->log_prefix;

  AggroFightOptions options;
  options.careful = careful;
  options.wait_for_skill_completion = waitForSkillCompletion;
  options.max_fight_ms = maxFightMs;
  options.log_prefix = context->log_prefix;
  options.loot_reason =
      context->stats != nullptr && context->stats_loot_reason != nullptr
          ? context->stats_loot_reason
          : context->default_loot_reason;
  (void)FightEnemiesInAggroWithSession(aggroRange, profile, options);
}

void HoldRouteLocalClearFromContext(const char* label,
                                    float waypointX,
                                    float waypointY,
                                    float fightRange,
                                    uint32_t targetId,
                                    void* userData) {
  auto* context = static_cast<RouteCombatContext*>(userData);
  if (context == nullptr) {
    return;
  }

  const LocalClearProfile profile =
      context->resolve_local_clear_profile != nullptr
          ? context->resolve_local_clear_profile(MapMgr::GetMapId(), context->policy_user_data)
          : LocalClearProfile::StandardTraversal;
  const LocalClearPolicy policy = BuildLocalClearPolicy(
      profile,
      label,
      fightRange,
      context->stats != nullptr);

  HoldLocalClearCallbacks callbacks = {};
  callbacks.is_dead = context->is_dead;
  callbacks.is_map_loaded = context->is_map_loaded;
  callbacks.wait_ms = context->wait_ms;
  callbacks.fight_in_aggro = &RouteLocalClearFightInAggro;
  callbacks.post_loot = &RouteCombatPostLoot;
  callbacks.on_clear_pass = &RecordRouteLocalClearPassStats;
  callbacks.user_data = context;

  HoldLocalClearOptions options;
  options.target_id = targetId;
  options.log_prefix = context->log_prefix;
  (void)HoldForLocalClear(waypointX, waypointY, fightRange, policy, callbacks, options);
}

void HoldSpecialRouteLocalClearFromContext(float waypointX,
                                           float waypointY,
                                           float fightRange,
                                           uint32_t targetId,
                                           void* userData) {
  auto* context = static_cast<RouteCombatContext*>(userData);
  HoldRouteLocalClearFromContext(
      context != nullptr ? context->special_local_clear_label : "Route",
      waypointX,
      waypointY,
      fightRange,
      targetId,
      userData);
}

} // namespace GWA3::AdvancedCombat
