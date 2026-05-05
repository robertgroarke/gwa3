#include <bots/ravens_point/RavensPointBot.h>

#include <gwa3/advanced/Effects.h>
#include <gwa3/advanced/Interactions.h>
#include <bots/common/BotFramework.h>
#include <gwa3/advanced/Inventory.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonItemActions.h>
#include <gwa3/dungeon/DungeonLoot.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <bots/ravens_point/RavensPoint.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/game/SkillIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>

#include <Windows.h>

namespace GWA3::Bot::RavensPointBot {

using namespace GWA3::Bot;
using namespace GWA3::Bot::RavensPoint;

namespace {

constexpr DungeonQuest::TravelPoint kVarajarBlessingPrepPath[] = {
    {-3393.0f, -1985.0f},
    {-2545.0f, -3501.0f},
    {-3926.0f, -4650.0f},
};
constexpr uint32_t kUnlitTorchModelId = 22342u;
constexpr float kLegacyWaypointSignpostSearchRadius = 1500.0f;
constexpr float kLegacyWaypointLootSearchRadius = 18000.0f;
constexpr uint32_t kLegacyWaypointInteractDelayMs = 100u;
constexpr uint32_t kLegacyWaypointLootDelayMs = 500u;
constexpr uint32_t kDeldrimorTitleId = 0x27u;
constexpr uint32_t kDungeonBlessingDialogId = 0x84u;
constexpr int kRouteWipeRecoveryMaxAttempts = 3;

uint32_t s_wipeCount = 0u;

struct RavensAggroWaypointContext {
  const char *route_name = "";
  bool enable_legacy_waypoint_hooks = true;
};

void WaitMs(uint32_t ms) { Sleep(ms); }

bool HasDwarvenBlessing() {
  const uint32_t me = AgentMgr::GetMyId();
  if (me == 0u) {
    return false;
  }

  static constexpr uint32_t kDwarvenBlessingEffects[] = {
      GWA3::SkillIds::DWARVEN_RAIDER,
      GWA3::SkillIds::DWARVEN_RAIDER_ID_2446,
      GWA3::SkillIds::DWARVEN_RAIDER_ID_2447,
      GWA3::SkillIds::DWARVEN_RAIDER_ID_2448,
      GWA3::SkillIds::GREAT_DWARFS_BLESSING,
      GWA3::SkillIds::VETERAN_DWARVEN_RAIDER,
      GWA3::SkillIds::DWARVEN_RAIDER_ID_2565,
      GWA3::SkillIds::DWARVEN_RAIDER_ID_2566,
      GWA3::SkillIds::DWARVEN_RAIDER_ID_2567,
      GWA3::SkillIds::DWARVEN_RAIDER_ID_2568,
      GWA3::SkillIds::GREAT_DWARFS_BLESSING_ID_2570,
  };
  for (const uint32_t effectId : kDwarvenBlessingEffects) {
    if (EffectMgr::HasEffect(me, effectId)) {
      return true;
    }
  }
  return false;
}

bool IsQuestReadyForEntry(const Quest *quest) {
  return quest != nullptr && (quest->log_state & 0x02u) == 0u;
}

bool IsRavensPointQuestComplete() {
  auto *quest = QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT);
  return quest != nullptr && (quest->log_state & 0x02u) != 0u;
}

void LogQuestSnapshot(const char *context) {
  auto *quest = QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT);
  LogBot("Ravens: quest snapshot after %s active=0x%X present=%d logState=%u "
         "completed=%d",
         context, QuestMgr::GetActiveQuestId(), quest != nullptr ? 1 : 0,
         quest ? quest->log_state : 0u,
         quest && ((quest->log_state & 0x02u) != 0u) ? 1 : 0);
}

bool MoveToTravelPoint(const DungeonQuest::TravelPoint &point, uint32_t mapId,
                       float tolerance = 250.0f, uint32_t timeoutMs = 30000u) {
  return DungeonNavigation::MoveToAndWait(point.x, point.y, tolerance,
                                          timeoutMs, 1000u, mapId)
      .arrived;
}

bool MoveToPointForEffect(float x, float y, float threshold) {
  return MoveToTravelPoint({x, y}, MapMgr::GetMapId(), threshold, 20000u);
}

bool ZoneThroughPoint(float x, float y, uint32_t targetMapId,
                      uint32_t timeoutMs = 60000u) {
  const DWORD start = GetTickCount();
  while ((GetTickCount() - start) < timeoutMs) {
    AgentMgr::Move(x, y);
    if (MapMgr::GetMapId() == targetMapId) {
      return true;
    }
    if (DungeonNavigation::WaitForMapId(targetMapId, 250u)) {
      return true;
    }
    WaitMs(250u);
  }
  return false;
}

DungeonQuest::TravelPoint GetTransitionPushPoint(
    RouteId routeId, const DungeonRoute::Waypoint &fallback) {
  switch (routeId) {
  case RouteId::Level1Exit:
    return {-19600.0f, 9780.0f};
  case RouteId::Level2Exit:
    return {3400.0f, 18000.0f};
  default:
    return {fallback.x, fallback.y};
  }
}

bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs = 15000u) {
  const DWORD start = GetTickCount();
  while ((GetTickCount() - start) < timeoutMs) {
    if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
      WaitMs(250u);
      continue;
    }

    if (AgentMgr::GetMyId() == 0u) {
      WaitMs(250u);
      continue;
    }

    auto *me = AgentMgr::GetMyAgent();
    if (!me || me->hp <= 0.0f) {
      WaitMs(250u);
      continue;
    }

    if (me->x == 0.0f && me->y == 0.0f) {
      WaitMs(250u);
      continue;
    }

    return true;
  }
  return false;
}

bool WaitForPostZoneMapReady(uint32_t mapId, uint32_t timeoutMs = 30000u) {
  const DWORD start = GetTickCount();
  while ((GetTickCount() - start) < timeoutMs) {
    const DWORD elapsed = GetTickCount() - start;
    const uint32_t remaining =
        elapsed >= timeoutMs ? 0u : static_cast<uint32_t>(timeoutMs - elapsed);
    if (!WaitForMapReady(mapId, remaining < 1000u ? remaining : 1000u)) {
      continue;
    }

    const uint32_t heroes = PartyMgr::CountPartyHeroes();
    if (heroes < 7u) {
      WaitMs(250u);
      continue;
    }

    auto *me = AgentMgr::GetMyAgent();
    Log::Info("Ravens: post-zone map %u ready heroes=%u activeQuest=0x%X "
              "ravenLogState=%u player=(%.0f, %.0f)",
              mapId, heroes, QuestMgr::GetActiveQuestId(),
              QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
                  ? QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
                        ->log_state
                  : 0u,
              me ? me->x : 0.0f, me ? me->y : 0.0f);
    return true;
  }

  auto *me = AgentMgr::GetMyAgent();
  Log::Info("Ravens: post-zone map %u not ready after %u ms "
            "(currentMap=%u loaded=%d myId=%u me=%p hp=%.2f pos=(%.0f,%.0f) "
            "heroes=%u activeQuest=0x%X ravenLogState=%u)",
            mapId, timeoutMs, MapMgr::GetMapId(),
            MapMgr::GetIsMapLoaded() ? 1 : 0, AgentMgr::GetMyId(),
            static_cast<void *>(me), me ? me->hp : -1.0f,
            me ? me->x : 0.0f, me ? me->y : 0.0f,
            PartyMgr::CountPartyHeroes(), QuestMgr::GetActiveQuestId(),
            QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
                ? QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
                      ->log_state
                : 0u);
  return false;
}

int GetNearestWaypointIndexForRoute(const RouteDefinition &route) {
  auto *me = AgentMgr::GetMyAgent();
  if (!me) {
    return -1;
  }

  return DungeonRoute::FindNearestWaypointIndex(
      route.waypoints, route.waypoint_count, me->x, me->y);
}

bool FollowTravelPath(const DungeonQuest::TravelPoint *points, int count,
                      uint32_t mapId, float tolerance = 250.0f) {
  if (points == nullptr || count <= 0) {
    return false;
  }

  for (int i = 0; i < count; ++i) {
    if (!MoveToTravelPoint(points[i], mapId, tolerance)) {
      LogBot(
          "Ravens: failed moving to travel point %d at (%.0f, %.0f) on map %u",
          i, points[i].x, points[i].y, mapId);
      return false;
    }
  }

  return true;
}

int PickupNearbyLoot(float maxRange) {
  return DungeonLoot::PickUpNearbyLoot(
      maxRange, &WaitMs, &DungeonBuiltinCombat::IsPlayerOrPartyDead,
      DungeonLoot::MakeLootPickupOptions(
          "Ravens", &DungeonLoot::IsWorldReadyForLootWithPlayerAgent));
}

void MoveToPointForLoot(float x, float y, float threshold) {
  (void)MoveToTravelPoint({x, y}, MapMgr::GetMapId(), threshold, 20000u);
}

void CombatMoveToCurrentMap(float x, float y, float fightRange) {
  (void)DungeonBuiltinCombat::MoveToPointWithAggro(
      x, y, MapMgr::GetMapId(), 250.0f, fightRange, 180000u);
}

void UseDpRemovalIfNeeded() {
  DungeonItemActions::UseItemOptions options;
  options.delay_ms = 5000u;
  const auto result =
      DungeonItemActions::UseDpRemovalSweetIfNeeded(&s_wipeCount, &WaitMs,
                                                    options);
  if (result.used_model_id != 0u) {
    LogBot("Ravens: used DP removal sweet model=%u after %u wipes",
           result.used_model_id, result.previous_wipe_count);
    Log::Info("Ravens: used DP removal sweet model=%u after %u wipes",
              result.used_model_id, result.previous_wipe_count);
  }
}

bool AcquireDwarvenBlessingAt(float x, float y, uint32_t mapId,
                              const char *context) {
  if (HasDwarvenBlessing()) {
    LogBot("Ravens: Dwarven blessing already active for %s",
           context ? context : "<unknown>");
    return true;
  }

  if (!MoveToTravelPoint({x, y}, mapId, 300.0f, 20000u)) {
    LogBot("Ravens: failed reaching Dwarven blessing for %s at (%.0f, %.0f)",
           context ? context : "<unknown>", x, y);
    Log::Info(
        "Ravens: failed reaching Dwarven blessing for %s at (%.0f, %.0f)",
        context ? context : "<unknown>", x, y);
    return false;
  }

  (void)GWA3::AdvancedEffects::EnsureActiveTitle(kDeldrimorTitleId, 1000u,
                                                 &WaitMs);

  GWA3::AdvancedInteractions::InteractCandidate candidates[2] = {};
  const size_t count =
      GWA3::AdvancedInteractions::CollectNearestInteractCandidates(
          x, y, 900.0f, 1500.0f, candidates, 2u);
  if (count == 0u) {
    LogBot("Ravens: no Dwarven blessing interactable near (%.0f, %.0f) for %s",
           x, y, context ? context : "<unknown>");
    Log::Info(
        "Ravens: no Dwarven blessing interactable near (%.0f, %.0f) for %s",
        x, y, context ? context : "<unknown>");
    return false;
  }

  for (size_t i = 0; i < count && !HasDwarvenBlessing(); ++i) {
    const auto &candidate = candidates[i];
    (void)MoveToPointForEffect(candidate.x, candidate.y,
                               candidate.use_signpost ? 120.0f : 90.0f);

    GWA3::AdvancedInteractions::CandidateDialogOptions options;
    options.dialog_id = kDungeonBlessingDialogId;
    options.candidate_index = i;
    options.interact_attempts = 3;
    options.log_prefix = "Ravens: Dwarven blessing";
    options.wait_ms = &WaitMs;
    options.stop_condition = &HasDwarvenBlessing;
    const auto result =
        GWA3::AdvancedInteractions::InteractCandidateAndSendDialog(candidate,
                                                                   options);
    Log::Info("Ravens: Dwarven blessing candidate[%u] agent=%u dialog=%d "
              "confirmed=%d context=%s",
              static_cast<unsigned>(i), candidate.agent_id,
              result.dialog_sent ? 1 : 0, result.confirmed ? 1 : 0,
              context ? context : "<unknown>");
  }

  const bool confirmed = HasDwarvenBlessing();
  LogBot("Ravens: Dwarven blessing result for %s confirmed=%d",
         context ? context : "<unknown>", confirmed ? 1 : 0);
  Log::Info("Ravens: Dwarven blessing result for %s confirmed=%d",
            context ? context : "<unknown>", confirmed ? 1 : 0);
  return confirmed;
}

bool AcquireDungeonKeyAtLootObjective(const LootObjective &loot,
                                      const char *context) {
  DungeonLoot::BossKeyAcquireOptions options;
  options.key_x = loot.pickup_point.x;
  options.key_y = loot.pickup_point.y;
  options.key_scan_range = 2500.0f;
  options.move_fight_range = 1600.0f;
  options.wide_loot_range = 18000.0f;
  options.final_loot_range = 5000.0f;
  options.passes = loot.pickup_retries > 0
                       ? static_cast<uint32_t>(loot.pickup_retries)
                       : 3u;
  options.log_prefix = "Ravens";
  options.combat_move_to = &CombatMoveToCurrentMap;
  options.pickup_nearby_loot = &PickupNearbyLoot;
  options.move_to_point = &MoveToPointForLoot;
  options.wait_ms = &WaitMs;
  options.is_dead = &DungeonBuiltinCombat::IsPlayerOrPartyDead;
  options.loot = DungeonLoot::MakeLootPickupOptions(
      "Ravens", &DungeonLoot::IsWorldReadyForLootWithPlayerAgent);
  options.force_pickup.log_prefix = "Ravens";

  LogBot("Ravens: acquiring dungeon key for %s at (%.0f, %.0f)",
         context ? context : "<unknown>", loot.pickup_point.x,
         loot.pickup_point.y);
  const bool acquired = DungeonLoot::AcquireBossKey(options);
  LogBot("Ravens: dungeon key acquire result for %s = %d",
         context ? context : "<unknown>", acquired ? 1 : 0);
  return acquired;
}

bool HandleRavensAggroWaypoint(const DungeonRoute::Waypoint &waypoint,
                               int waypointIndex,
                               DungeonRoute::WaypointLabelKind labelKind,
                               DungeonCombat::AggroWaypointPhase phase,
                               void *userData) {
  if (phase != DungeonCombat::AggroWaypointPhase::AfterAdvance) {
    return true;
  }

  switch (labelKind) {
  case DungeonRoute::WaypointLabelKind::BossLock:
  case DungeonRoute::WaypointLabelKind::Chest:
  case DungeonRoute::WaypointLabelKind::Signpost:
    break;
  default:
    return true;
  }

  const auto *context =
      static_cast<const RavensAggroWaypointContext *>(userData);
  if (context && !context->enable_legacy_waypoint_hooks) {
    return true;
  }
  const bool handled = DungeonBundle::InteractSignpostAndPickUpLootNearPoint(
      waypoint.x, waypoint.y, kLegacyWaypointSignpostSearchRadius, 2,
      kLegacyWaypointInteractDelayMs, kLegacyWaypointLootSearchRadius,
      kLegacyWaypointLootDelayMs);
  Log::Info(
      "Ravens: legacy waypoint hook route=%s index=%d label=%s handled=%d",
      context && context->route_name ? context->route_name : "<unknown>",
      waypointIndex, waypoint.label ? waypoint.label : "", handled ? 1 : 0);
  return true;
}

bool TryRecoverRouteWipe(const DungeonRoute::Waypoint *slice, int sliceCount,
                         int failedIndex, const char *context,
                         int absoluteStartIndex, int *nextStartIndex) {
  if (!DungeonBuiltinCombat::IsPlayerOrPartyDead()) {
    return false;
  }

  DungeonCheckpoint::RouteWipeRecoveryOptions recoveryOptions;
  recoveryOptions.waypoints = slice;
  recoveryOptions.waypoint_count = sliceCount;
  recoveryOptions.current_index = failedIndex >= 0 ? failedIndex : 0;
  recoveryOptions.log_prefix = "Ravens";
  recoveryOptions.recovery.waypoints = slice;
  recoveryOptions.recovery.waypoint_count = sliceCount;
  recoveryOptions.recovery.nearest_index = failedIndex >= 0 ? failedIndex : 0;
  recoveryOptions.recovery.backtrack_steps = 2;
  recoveryOptions.recovery.wipe_count = &s_wipeCount;
  recoveryOptions.recovery.is_dead = &DungeonBuiltinCombat::IsPlayerOrPartyDead;
  recoveryOptions.recovery.wait_ms = &WaitMs;
  recoveryOptions.recovery.return_to_outpost = &MapMgr::ReturnToOutpost;
  recoveryOptions.recovery.use_dp_removal = &UseDpRemovalIfNeeded;
  recoveryOptions.recovery.get_nearest_waypoint =
      &DungeonNavigation::GetNearestWaypointIndex;

  const auto recovery =
      DungeonCheckpoint::RecoverRouteWaypointWipe(recoveryOptions);
  if (!recovery.recovered) {
    LogBot("Ravens: wipe recovery failed for %s returnedToOutpost=%d",
           context ? context : "<unknown>",
           recovery.returned_to_outpost ? 1 : 0);
    return false;
  }

  if (nextStartIndex != nullptr) {
    *nextStartIndex = absoluteStartIndex + recovery.restart_index;
  }
  LogBot("Ravens: recovered wipe for %s; restarting at waypoint %d",
         context ? context : "<unknown>",
         absoluteStartIndex + recovery.restart_index);
  return true;
}

DungeonNavigation::RouteFollowResult FollowWaypointSliceWithRetries(
    RouteId routeId, const RouteDefinition &route, int startIndex,
    int waypointCount, const char *context,
    const DungeonNavigation::RouteFollowOptions &options,
    bool enableLegacyWaypointHooks) {
  DungeonNavigation::RouteFollowResult followResult;
  if (startIndex < 0 || waypointCount <= 0 ||
      startIndex >= route.waypoint_count) {
    followResult.failed_index = startIndex;
    return followResult;
  }

  const int boundedCount =
      (startIndex + waypointCount) > route.waypoint_count
          ? (route.waypoint_count - startIndex)
          : waypointCount;
  const int endIndex = startIndex + boundedCount;
  int currentStartIndex = startIndex;
  int recoveryAttempts = 0;

  while (currentStartIndex < endIndex) {
    const auto *slice = route.waypoints + currentStartIndex;
    const int currentCount = endIndex - currentStartIndex;

    if (UsesAggroTraversal(routeId)) {
      auto aggroRouteOptions = options;
      aggroRouteOptions.reissue_ms = 100u;

      DungeonCombat::AggroAdvanceOptions aggroOptions;
      DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(
          aggroOptions, aggroRouteOptions.waypoint_timeout_ms, true);
      aggroOptions.move_wait_ms = 100u;
      aggroOptions.timeout_ms = aggroRouteOptions.waypoint_timeout_ms;
      if (routeId == RouteId::Level2Torch2 ||
          routeId == RouteId::Level2Torch3) {
        // These torch chest approaches are narrow route sections. AutoIt's
        // AggroMoveToEx only gated on the waypoint fight range here; the
        // shared 1600 local-clear floor can over-stop on edge foes and wipe.
        aggroOptions.clear_options.minimum_local_clear_range = 0.0f;
        aggroOptions.clear_options.extra_clear_range = 0.0f;
        aggroOptions.stuck_recovery_threshold = 30;
        aggroOptions.stuck_abort_threshold = 240;
        aggroOptions.stuck_recovery_radius = 900.0f;
      }
      if (routeId == RouteId::Level1Torch2 && currentStartIndex >= 4) {
        // The last Level1Torch2 slice is the chest approach. Keep the early
        // route on Froggy-style clearing, then avoid timing out on edge foes
        // once the run is already committed to the torch chest.
        aggroOptions.clear_options.minimum_local_clear_range = 0.0f;
        aggroOptions.clear_options.extra_clear_range = 0.0f;
        aggroOptions.stuck_recovery_threshold = 30;
        aggroOptions.stuck_abort_threshold = 240;
        aggroOptions.stuck_recovery_radius = 900.0f;
      }
      if (routeId == RouteId::Level1DoorKey && currentStartIndex < 3) {
        // The key approach is the same short AutoIt-style aggro hop used
        // before repeated key pickup passes. Avoid over-clearing side packs
        // before the bot has the key.
        aggroOptions.clear_options.minimum_local_clear_range = 0.0f;
        aggroOptions.clear_options.extra_clear_range = 0.0f;
        aggroOptions.stuck_recovery_threshold = 30;
        aggroOptions.stuck_abort_threshold = 240;
        aggroOptions.stuck_recovery_radius = 900.0f;
      }
      if (routeId == RouteId::Level2Door) {
        aggroOptions.clear_options.minimum_local_clear_range = 1600.0f;
        aggroOptions.clear_options.extra_clear_range = 0.0f;
        aggroOptions.clear_options.change_target = true;
        aggroOptions.clear_options.call_target = true;
        aggroOptions.clear_options.chase_during_clear = true;
        aggroOptions.clear_options.chase_distance = 900.0f;
      }
      RavensAggroWaypointContext waypointContext;
      waypointContext.route_name = route.name;
      waypointContext.enable_legacy_waypoint_hooks = enableLegacyWaypointHooks;
      DungeonCombat::AggroWaypointCallbacks waypointCallbacks;
      waypointCallbacks.on_waypoint = &HandleRavensAggroWaypoint;
      waypointCallbacks.user_data = &waypointContext;
      followResult = DungeonCombat::FollowWaypointsWithAggro(
          slice, currentCount, route.map_id,
          DungeonBuiltinCombat::MakeCombatCallbacks(), aggroRouteOptions,
          aggroOptions, waypointCallbacks);
    } else {
      followResult =
          DungeonNavigation::FollowWaypoints(slice, currentCount, route.map_id,
                                             options);
    }
    if (followResult.completed || followResult.map_changed) {
      return followResult;
    }

    if (followResult.failed_index >= 0 &&
        followResult.failed_index < currentCount) {
      const int absoluteIndex = currentStartIndex + followResult.failed_index;
      if (recoveryAttempts < kRouteWipeRecoveryMaxAttempts) {
        int recoveredStartIndex = currentStartIndex;
        if (TryRecoverRouteWipe(slice, currentCount, followResult.failed_index,
                                context, currentStartIndex,
                                &recoveredStartIndex)) {
          ++recoveryAttempts;
          if (recoveredStartIndex < currentStartIndex) {
            currentStartIndex = recoveredStartIndex;
          } else if (recoveredStartIndex > absoluteIndex) {
            currentStartIndex = absoluteIndex;
          } else {
            currentStartIndex = recoveredStartIndex;
          }
          continue;
        }
      }
      followResult.failed_index = absoluteIndex - startIndex;
      LogBot("Ravens: failed %s at waypoint %d (%s) after %d retries", context,
             absoluteIndex, route.waypoints[absoluteIndex].label,
             followResult.retries_used);
      Log::Info("Ravens: failed %s at waypoint %d (%s) after %d retries",
                context, absoluteIndex, route.waypoints[absoluteIndex].label,
                followResult.retries_used);
    } else {
      LogBot("Ravens: failed %s after %d retries", context,
             followResult.retries_used);
      Log::Info("Ravens: failed %s after %d retries", context,
                followResult.retries_used);
    }
    return followResult;
  }

  followResult.completed = true;
  return followResult;
}

bool FollowRouteWithRetries(
    RouteId routeId, const char *context,
    const DungeonNavigation::RouteFollowOptions &options) {
  const RouteDefinition &route = GetRouteDefinition(routeId);
  const bool enableLegacyWaypointHooks = FindTorchObjective(routeId) == nullptr;
  auto followResult = FollowWaypointSliceWithRetries(
      routeId, route, 0, route.waypoint_count, context, options,
      enableLegacyWaypointHooks);
  return followResult.completed || followResult.map_changed;
}

bool TryRecoverTorchChestApproach(RouteId routeId, const RouteDefinition &route) {
  if (route.waypoint_count <= 0) {
    return false;
  }

  const TorchObjective *torch = FindTorchObjective(routeId);
  if (torch == nullptr) {
    return false;
  }

  const char *routeName = route.name ? route.name : "<unknown>";
  auto *me = AgentMgr::GetMyAgent();
  const float chestDistance =
      me ? AgentMgr::GetDistance(me->x, me->y, torch->chest.x, torch->chest.y)
         : 999999.0f;
  LogBot("Ravens: %s chest approach recovery start player=(%.0f, %.0f) "
         "chestDist=%.0f",
         routeName, me ? me->x : 0.0f, me ? me->y : 0.0f, chestDistance);
  Log::Info("Ravens: %s chest approach recovery start player=(%.0f, %.0f) "
            "chestDist=%.0f",
            routeName, me ? me->x : 0.0f, me ? me->y : 0.0f, chestDistance);

  if (chestDistance <= 1600.0f) {
    Log::Info("Ravens: %s recovery accepted existing chest proximity",
              routeName);
    return true;
  }

  const auto &approach = route.waypoints[route.waypoint_count - 1];
  DungeonCombat::AggroAdvanceOptions aggroOptions;
  DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(
      aggroOptions, 180000u, true);
  aggroOptions.arrival_threshold = 650.0f;
  aggroOptions.move_wait_ms = 100u;
  aggroOptions.clear_options.minimum_local_clear_range = 0.0f;
  aggroOptions.clear_options.extra_clear_range = 0.0f;
  aggroOptions.stuck_recovery_threshold = 30;
  aggroOptions.stuck_abort_threshold = 240;
  aggroOptions.stuck_recovery_radius = 900.0f;

  const bool advanced = GWA3::AdvancedCombat::AdvanceWithAggro(
      approach.x, approach.y, approach.fight_range,
      DungeonBuiltinCombat::MakeCombatCallbacks(), aggroOptions);

  me = AgentMgr::GetMyAgent();
  const float finalChestDistance =
      me ? AgentMgr::GetDistance(me->x, me->y, torch->chest.x, torch->chest.y)
         : 999999.0f;
  LogBot("Ravens: %s chest recovery advanced=%d player=(%.0f, %.0f) "
         "chestDist=%.0f",
         routeName, advanced ? 1 : 0, me ? me->x : 0.0f, me ? me->y : 0.0f,
         finalChestDistance);
  Log::Info("Ravens: %s chest recovery advanced=%d player=(%.0f, "
            "%.0f) chestDist=%.0f",
            routeName, advanced ? 1 : 0, me ? me->x : 0.0f,
            me ? me->y : 0.0f,
            finalChestDistance);

  return advanced || finalChestDistance <= 1600.0f;
}

bool MoveToQuestNpc(const DungeonQuest::QuestCyclePlan &plan,
                    const char *context) {
  if (!DungeonBuiltinCombat::MoveToPointWithAggro(
          plan.npc.x, plan.npc.y, plan.start_map_id, 300.0f,
          plan.npc.search_radius, 120000u)) {
    LogBot("Ravens: failed moving to quest NPC for %s", context);
    return false;
  }
  return true;
}

bool ExecuteQuestDialogs(const DungeonQuest::QuestCyclePlan &plan,
                         const DungeonQuest::DialogPlan &dialogPlan,
                         const char *context) {
  DungeonQuestRuntime::DialogExecutionOptions dialogOptions;
  dialogOptions.move_to_actual_npc = true;
  dialogOptions.move_to_npc_tolerance = 120.0f;
  dialogOptions.move_to_npc_timeout_ms = 20000u;
  dialogOptions.cancel_action_before_interact = true;
  dialogOptions.clear_dialog_state_before_interact = true;
  dialogOptions.require_dialog_before_send = true;
  dialogOptions.pre_interact_settle_ms = 500u;
  dialogOptions.change_target_delay_ms = 250u;
  dialogOptions.interact_count = 3;
  dialogOptions.interact_delay_ms = 1500u;
  dialogOptions.post_interact_delay_ms = 1000u;
  dialogOptions.dialog_wait_timeout_ms = 2500u;
  dialogOptions.repeat_delay_ms = 750u;
  dialogOptions.max_retries_per_dialog = 2;
  dialogOptions.use_direct_npc_interact = true;

  if (!MoveToQuestNpc(plan, context)) {
    return false;
  }
  if (!DungeonQuestRuntime::InteractNearestNpcAndSendDialogPlan(
          plan.npc, dialogPlan, dialogOptions)) {
    LogBot("Ravens: dialog plan failed during %s", context);
    return false;
  }
  LogQuestSnapshot(context);
  return true;
}

bool VerifyQuestState(bool expectPresent, const char *context,
                      bool requireReadyForEntry = false) {
  DungeonQuestRuntime::QuestVerificationOptions options;
  options.timeout_ms = expectPresent ? 8000u : 5000u;
  options.require_not_completed_when_present = requireReadyForEntry;
  if (!DungeonQuestRuntime::WaitForQuestState(GWA3::QuestIds::RAVENS_POINT, expectPresent,
                                              options)) {
    LogBot("Ravens: quest 0x%X %s after %s", GWA3::QuestIds::RAVENS_POINT,
           expectPresent ? "missing" : "still present", context);
    return false;
  }

  auto *quest = QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT);
  if (expectPresent && requireReadyForEntry && !IsQuestReadyForEntry(quest)) {
    LogBot("Ravens: quest 0x%X present but not ready after %s (logState=%u)",
           GWA3::QuestIds::RAVENS_POINT, context, quest ? quest->log_state : 0u);
    return false;
  }
  LogBot("Ravens: quest 0x%X %s after %s (active=0x%X logState=%u)",
         GWA3::QuestIds::RAVENS_POINT,
         expectPresent ? (requireReadyForEntry ? "ready" : "verified")
                       : "cleared",
         context, QuestMgr::GetActiveQuestId(), quest ? quest->log_state : 0u);
  return true;
}

bool ExecuteQuestApproach(const DungeonQuest::QuestCyclePlan & /*plan*/) {
  DungeonNavigation::RouteFollowOptions options;
  options.waypoint_timeout_ms = 120000u;
  options.max_backtrack_retries = 3;
  return FollowRouteWithRetries(RouteId::QuestApproach, "quest approach",
                                options);
}

bool ExecuteQuestReturn() {
  DungeonNavigation::RouteFollowOptions options;
  options.waypoint_timeout_ms = 120000u;
  options.max_backtrack_retries = 3;
  return FollowRouteWithRetries(RouteId::QuestReturn, "quest return", options);
}

bool ExecuteQuestCycle() {
  const auto plan = GetQuestCyclePlan();
  if (!DungeonQuest::IsValidQuestCyclePlan(plan)) {
    return false;
  }

  auto *quest = QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT);
  Log::Info("Ravens: quest cycle start map=%u active=0x%X present=%d "
            "logState=%u",
            MapMgr::GetMapId(), QuestMgr::GetActiveQuestId(),
            quest != nullptr ? 1 : 0, quest ? quest->log_state : 0u);
  if (IsQuestReadyForEntry(quest)) {
    LogBot("Ravens: quest already ready at cycle start (active=0x%X "
           "logState=%u); skipping reward bootstrap",
           QuestMgr::GetActiveQuestId(), quest ? quest->log_state : 0u);
    if (QuestMgr::GetActiveQuestId() != GWA3::QuestIds::RAVENS_POINT) {
      QuestMgr::SetActiveQuest(GWA3::QuestIds::RAVENS_POINT);
      WaitMs(500u);
    }
    if (!VerifyQuestState(true, "existing quest state", true)) {
      return false;
    }
    Log::Info("Ravens: existing quest ready; proceeding to live entry");
  } else {
    LogBot("Ravens: starting reward/quest bootstrap");
    Log::Info("Ravens: starting reward/quest bootstrap");
    if (!ExecuteQuestDialogs(plan, plan.reward_dialog, "reward bootstrap")) {
      Log::Info("Ravens: reward bootstrap dialog failed");
      return false;
    }

    if (!ExecuteQuestApproach(plan)) {
      LogBot("Ravens: failed on quest approach before reward bounce");
      Log::Info("Ravens: failed on quest approach before reward bounce");
      return false;
    }
    if (!MoveToTravelPoint(plan.dungeon_entry, plan.start_map_id, 300.0f)) {
      LogBot("Ravens: failed reaching reward bootstrap entry point");
      Log::Info("Ravens: failed reaching reward bootstrap entry point");
      return false;
    }
    if (!ZoneThroughPoint(plan.dungeon_entry.x, plan.dungeon_entry.y,
                          plan.dungeon_map_id)) {
      LogBot("Ravens: failed zoning into reward bootstrap dungeon instance");
      Log::Info("Ravens: failed zoning into reward bootstrap dungeon instance");
      return false;
    }
    if (!WaitForPostZoneMapReady(plan.dungeon_map_id)) {
      LogBot("Ravens: reward bootstrap dungeon map did not finish loading");
      return false;
    }
    if (!ZoneThroughPoint(plan.dungeon_exit.x, plan.dungeon_exit.y,
                          plan.start_map_id)) {
      LogBot(
          "Ravens: failed reversing out of reward bootstrap dungeon instance");
      Log::Info(
          "Ravens: failed reversing out of reward bootstrap dungeon instance");
      return false;
    }
    if (!WaitForPostZoneMapReady(plan.start_map_id)) {
      LogBot("Ravens: Varajar did not finish loading after reward bootstrap "
             "reversal");
      return false;
    }

    if (!ExecuteQuestReturn()) {
      return false;
    }
    if (!VerifyQuestState(false, "reward bootstrap reversal")) {
      LogBot("Ravens: reward bootstrap did not clear the quest log before live "
             "accept");
      return false;
    }

    LogBot("Ravens: starting live quest accept flow");
    Log::Info("Ravens: starting live quest accept flow");
    bool accepted = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
      if (!ExecuteQuestDialogs(plan, plan.accept_dialog, "quest accept")) {
        Log::Info("Ravens: quest accept dialog failed attempt=%d",
                  attempt + 1);
        return false;
      }
      if (VerifyQuestState(true, "quest accept", true)) {
        accepted = true;
        Log::Info("Ravens: quest accept verified attempt=%d", attempt + 1);
        break;
      }
      LogBot("Ravens: quest accept attempt %d did not produce an active quest "
             "state",
             attempt + 1);
    }
    if (!accepted) {
      Log::Info("Ravens: quest accept failed after retries");
      return false;
    }
  }

  Log::Info("Ravens: starting live quest approach to dungeon entry");
  if (!ExecuteQuestApproach(plan)) {
    LogBot("Ravens: failed on quest approach before live entry");
    Log::Info("Ravens: failed on quest approach before live entry");
    return false;
  }
  if (!MoveToTravelPoint(plan.dungeon_entry, plan.start_map_id, 300.0f)) {
    LogBot("Ravens: failed reaching live entry point");
    Log::Info("Ravens: failed reaching live entry point");
    return false;
  }
  if (!ZoneThroughPoint(plan.dungeon_entry.x, plan.dungeon_entry.y,
                        plan.dungeon_map_id)) {
    LogBot("Ravens: failed zoning into Raven's Point level 1");
    Log::Info("Ravens: failed zoning into Raven's Point level 1");
    return false;
  }
  const bool ready = WaitForPostZoneMapReady(plan.dungeon_map_id);
  Log::Info("Ravens: live dungeon entry ready=%d map=%u", ready ? 1 : 0,
            MapMgr::GetMapId());
  return ready;
}

bool ExecuteVarajarBootstrap() {
  const RouteDefinition &route =
      GetRouteDefinition(RouteId::RunVarajarToBlessing);
  const BlessingAnchor *blessing =
      FindBlessingAnchor(RouteId::RunVarajarToBlessing, 0);
  if (blessing == nullptr) {
    LogBot("Ravens: missing blessing anchor for Varajar bootstrap");
    return false;
  }

  LogBot("Ravens: running explicit Varajar blessing bootstrap");
  if (!FollowTravelPath(kVarajarBlessingPrepPath,
                        static_cast<int>(sizeof(kVarajarBlessingPrepPath) /
                                         sizeof(kVarajarBlessingPrepPath[0])),
                        route.map_id, 300.0f)) {
    return false;
  }
  if (!MoveToTravelPoint({blessing->x, blessing->y}, route.map_id, 300.0f)) {
    LogBot("Ravens: failed reaching Norn blessing");
    return false;
  }
  WaitMs(500u);

  DungeonNavigation::RouteFollowOptions options;
  options.waypoint_timeout_ms = 120000u;
  options.max_backtrack_retries = 3;
  return FollowRouteWithRetries(RouteId::RunVarajarToBlessing,
                                "explicit Varajar bootstrap", options);
}

bool ExecuteDoorObjective(RouteId routeId, const RouteDefinition &route) {
  const DoorObjective *door = FindDoorObjective(routeId);
  if (door == nullptr) {
    return true;
  }

  LogBot("Ravens: opening dungeon lock for %s at (%.0f, %.0f)", route.name,
         door->interact_point.x, door->interact_point.y);
  if (!MoveToTravelPoint(door->interact_point, route.map_id, 200.0f)) {
    LogBot("Ravens: failed moving to dungeon lock for %s", route.name);
    return false;
  }
  if (!DungeonBundle::InteractSignpostNearPoint(
          door->interact_point.x, door->interact_point.y, 1500.0f,
          door->interact_repeats, 1000u)) {
    LogBot("Ravens: failed interacting with dungeon lock for %s", route.name);
    return false;
  }
  WaitMs(routeId == RouteId::Level2Door ? 1500u : 500u);
  if (routeId == RouteId::Level2Door) {
    const bool resumed = DungeonBuiltinCombat::MoveToPointWithAggro(
        door->resume_point.x, door->resume_point.y, route.map_id, 350.0f,
        1600.0f, 90000u);
    auto *me = AgentMgr::GetMyAgent();
    Log::Info("Ravens: Level2Door post-lock aggro resume result=%d player=(%.0f, %.0f) "
              "target=(%.0f, %.0f) map=%u",
              resumed ? 1 : 0, me ? me->x : 0.0f, me ? me->y : 0.0f,
              door->resume_point.x, door->resume_point.y, MapMgr::GetMapId());
    if (!resumed) {
      LogBot("Ravens: failed moving past dungeon lock for %s", route.name);
      return false;
    }
    return true;
  }
  if (!MoveToTravelPoint(door->resume_point, route.map_id)) {
    LogBot("Ravens: failed moving past dungeon lock for %s", route.name);
    return false;
  }
  return true;
}

bool ExecutePostTorchDropResume(RouteId routeId, const TorchObjective &torch,
                                uint32_t mapId, const char *routeName) {
  if (routeId == RouteId::Level1Torch1) {
    constexpr DungeonQuest::TravelPoint kLevel1Torch1PostDropPath[] = {
        {-7609.0f, -14853.0f},
        {-5086.0f, -8969.0f},
    };
    for (int i = 0; i < static_cast<int>(sizeof(kLevel1Torch1PostDropPath) /
                                         sizeof(kLevel1Torch1PostDropPath[0]));
         ++i) {
      const auto &point = kLevel1Torch1PostDropPath[i];
      if (!DungeonBuiltinCombat::MoveToPointWithAggro(
              point.x, point.y, mapId, 250.0f, 1600.0f, 120000u)) {
        LogBot("Ravens: failed post-torch aggro resume %d for %s", i,
               routeName);
        Log::Info("Ravens: failed post-torch aggro resume %d for %s", i,
                  routeName);
        return false;
      }
      WaitMs(500u);
    }
  }

  if (routeId == RouteId::Level1Torch1) {
    return AcquireDwarvenBlessingAt(torch.resume_point.x, torch.resume_point.y,
                                    mapId, routeName);
  }

  if (routeId == RouteId::Level2Torch3) {
    Log::Info("Ravens: waiting for Level2Torch3 bridge before resume");
    WaitMs(4000u);
    if (!MoveToTravelPoint(torch.resume_point, mapId, 300.0f, 90000u)) {
      LogBot("Ravens: failed moving to torch resume point for %s", routeName);
      Log::Info("Ravens: failed moving to torch resume point for %s", routeName);
      return false;
    }
    return true;
  }

  if (!MoveToTravelPoint(torch.resume_point, mapId)) {
    LogBot("Ravens: failed moving to torch resume point for %s", routeName);
    Log::Info("Ravens: failed moving to torch resume point for %s", routeName);
    return false;
  }
  return true;
}

bool MoveToTorchObjectivePoint(RouteId routeId, const char *routeName,
                               const char *phase, int index,
                               const DungeonQuest::TravelPoint &point,
                               uint32_t mapId) {
  constexpr float kTorchMoveTolerance = 300.0f;
  constexpr uint32_t kTorchMoveTimeoutMs = 60000u;

  const bool arrived =
      MoveToTravelPoint(point, mapId, kTorchMoveTolerance, kTorchMoveTimeoutMs);
  if (arrived) {
    Log::Info("Ravens: torch move arrived route=%s routeId=%u phase=%s index=%d "
              "target=(%.0f, %.0f)",
              routeName ? routeName : "<unknown>",
              static_cast<unsigned>(routeId), phase ? phase : "<unknown>",
              index, point.x, point.y);
    return true;
  }

  auto *me = AgentMgr::GetMyAgent();
  LogBot("Ravens: failed torch move route=%s phase=%s index=%d target=(%.0f, %.0f)",
         routeName ? routeName : "<unknown>", phase ? phase : "<unknown>",
         index, point.x, point.y);
  Log::Info("Ravens: failed torch move route=%s routeId=%u phase=%s index=%d "
            "target=(%.0f, %.0f) map=%u hp=%.2f player=(%.0f, %.0f) dead=%d",
            routeName ? routeName : "<unknown>",
            static_cast<unsigned>(routeId), phase ? phase : "<unknown>",
            index, point.x, point.y, MapMgr::GetMapId(), me ? me->hp : 0.0f,
            me ? me->x : 0.0f, me ? me->y : 0.0f,
            DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0);
  return false;
}

bool LightTorchBrazier(RouteId routeId, const char *routeName, int index,
                       const DungeonQuest::TravelPoint &point,
                       uint32_t mapId) {
  if (!MoveToTorchObjectivePoint(routeId, routeName, "brazier", index, point,
                                 mapId)) {
    return false;
  }

  const bool interacted = DungeonBundle::InteractSignpostNearPoint(
      point.x, point.y, 1500.0f, 1, 500u);
  Log::Info("Ravens: torch brazier interact route=%s routeId=%u index=%d "
            "target=(%.0f, %.0f) interacted=%d torchItem=%u heldBundle=%u",
            routeName ? routeName : "<unknown>", static_cast<unsigned>(routeId),
            index, point.x, point.y, interacted ? 1 : 0,
            DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
                kUnlitTorchModelId),
            DungeonInteractions::GetHeldBundleItemId());
  return true;
}

bool ExecuteTorchLightingPath(RouteId routeId, const TorchObjective &torch,
                              uint32_t mapId, const char *routeName) {
  int nextBrazierIndex = 0;
  if (routeId == RouteId::Level2Torch3 && torch.brazier_count > 0) {
    if (!LightTorchBrazier(routeId, routeName, 0, torch.brazier_points[0],
                           mapId)) {
      return false;
    }
    nextBrazierIndex = 1;
  }

  for (int i = 0; i < torch.transit_point_count; ++i) {
    if (!MoveToTorchObjectivePoint(routeId, routeName, "transit", i,
                                   torch.transit_points[i], mapId)) {
      return false;
    }
  }

  for (int i = nextBrazierIndex; i < torch.brazier_count; ++i) {
    if (routeId == RouteId::Level1Torch2 && i == 1 &&
        torch.transit_point_count > 0) {
      if (!MoveToTorchObjectivePoint(routeId, routeName, "transit", 2,
                                     torch.transit_points[0], mapId)) {
        return false;
      }
    }
    if (!LightTorchBrazier(routeId, routeName, i, torch.brazier_points[i],
                           mapId)) {
      return false;
    }
  }

  return true;
}

bool AcquireTorchBundleForRoute(RouteId routeId, const TorchObjective &torch,
                                uint32_t mapId, const char *routeName) {
  if (DungeonBundle::OpenChestAndAcquireHeldBundleByModelChestPreferred(
          torch.chest.x, torch.chest.y, kUnlitTorchModelId, 1500.0f, 18000.0f,
          2, 3, 500u, 500u)) {
    return true;
  }

  if (routeId != RouteId::Level2Torch3) {
    return false;
  }

  for (int retry = 1; retry <= 2; ++retry) {
    LogBot("Ravens: retrying torch chest acquire on %s after settle pass %d",
           routeName, retry);
    Log::Info(
        "Ravens: retrying torch chest acquire on %s after settle pass %d",
        routeName ? routeName : "<unknown>", retry);

    (void)MoveToTravelPoint(torch.chest, mapId, 250.0f, 20000u);
    WaitMs(2500u);

    if (DungeonBundle::OpenChestAndAcquireHeldBundleByModelChestPreferred(
            torch.chest.x, torch.chest.y, kUnlitTorchModelId, 1500.0f,
            18000.0f, 3, 5, 1000u, 1500u)) {
      Log::Info("Ravens: Level2Torch3 chest-preferred retry acquired torch "
                "on pass %d",
                retry);
      return true;
    }

    if (DungeonBundle::OpenChestAndAcquireHeldBundleByModelActionInteract(
            torch.chest.x, torch.chest.y, kUnlitTorchModelId, 18000.0f, 3, 5,
            1000u, 1500u)) {
      Log::Info("Ravens: Level2Torch3 action-interact retry acquired torch "
                "on pass %d",
                retry);
      return true;
    }
  }

  return false;
}

bool ExecuteLevel1DoorKeyRoute() {
  const RouteId routeId = RouteId::Level1DoorKey;
  const RouteDefinition &route = GetRouteDefinition(routeId);
  DungeonNavigation::RouteFollowOptions routeOptions;
  routeOptions.waypoint_timeout_ms = 120000u;
  routeOptions.max_backtrack_retries = 3;

  LogBot("Ravens: starting Level1DoorKey key approach");
  auto keyApproach = FollowWaypointSliceWithRetries(
      routeId, route, 0, 3, "Level1DoorKey key approach", routeOptions, true);
  if (!keyApproach.completed && !keyApproach.map_changed &&
      keyApproach.failed_index != 2) {
    return false;
  }
  if (!keyApproach.completed) {
    LogBot("Ravens: continuing Level1DoorKey after key waypoint timeout");
  }

  const LootObjective *loot = FindLootObjective(routeId);
  if (loot == nullptr || !AcquireDungeonKeyAtLootObjective(*loot, route.name)) {
    return false;
  }

  LogBot("Ravens: continuing Level1DoorKey door approach");
  auto doorApproach = FollowWaypointSliceWithRetries(
      routeId, route, 3, route.waypoint_count - 3,
      "Level1DoorKey door approach", routeOptions, true);
  if (!doorApproach.completed && !doorApproach.map_changed) {
    return false;
  }

  return ExecuteDoorObjective(routeId, route);
}

bool FollowLevel1Torch2Route(
    const RouteDefinition &route,
    const DungeonNavigation::RouteFollowOptions &routeOptions) {
  auto earlyRoute = FollowWaypointSliceWithRetries(
      RouteId::Level1Torch2, route, 0, 4, "Level1Torch2 combat approach",
      routeOptions, false);
  if (!earlyRoute.completed && !earlyRoute.map_changed) {
    return false;
  }

  DungeonNavigation::RouteFollowOptions chestOptions = routeOptions;
  chestOptions.default_tolerance = 650.0f;
  chestOptions.waypoint_timeout_ms = 180000u;
  chestOptions.max_backtrack_retries = 5;

  auto chestRoute = FollowWaypointSliceWithRetries(
      RouteId::Level1Torch2, route, 4, route.waypoint_count - 4,
      "Level1Torch2 chest approach", chestOptions, false);
  if (chestRoute.completed || chestRoute.map_changed) {
    return true;
  }

  if (TryRecoverTorchChestApproach(RouteId::Level1Torch2, route)) {
    LogBot("Ravens: recovered %s chest approach after route failure",
           route.name);
    Log::Info("Ravens: recovered %s chest approach after route failure",
              route.name);
    return true;
  }
  return false;
}

bool ExecuteRoute(RouteId routeId, bool waitForTransition) {
  if (routeId == RouteId::Level1DoorKey) {
    return ExecuteLevel1DoorKeyRoute();
  }

  const RouteDefinition &route = GetRouteDefinition(routeId);
  LogBot("Ravens: starting route %s waitForTransition=%d map=%u",
         route.name, waitForTransition ? 1 : 0, MapMgr::GetMapId());
  Log::Info("Ravens: starting route %s waitForTransition=%d map=%u",
            route.name, waitForTransition ? 1 : 0, MapMgr::GetMapId());
  auto *me = AgentMgr::GetMyAgent();
  if (!me) {
    LogBot("Ravens: no player agent available for route %s", route.name);
    return false;
  }

  const int startIndex = DungeonRoute::FindNearestWaypointIndex(
      route.waypoints, route.waypoint_count, me->x, me->y);

  if (const BlessingAnchor *blessing =
          FindBlessingAnchor(routeId, startIndex)) {
    if (!AcquireDwarvenBlessingAt(blessing->x, blessing->y, route.map_id,
                                  route.name)) {
      return false;
    }
  }

  DungeonNavigation::RouteFollowOptions routeOptions;
  routeOptions.waypoint_timeout_ms =
      UsesAggroTraversal(routeId) ? 120000u : 60000u;
  routeOptions.max_backtrack_retries = 3;
  if (routeId == RouteId::Level2Torch2 || routeId == RouteId::Level2Torch3) {
    // These final torch-route waypoints are chest approaches. Requiring exact
    // 250-unit arrival can strand the run after the combat work is done.
    routeOptions.default_tolerance = 650.0f;
    routeOptions.waypoint_timeout_ms = 180000u;
    routeOptions.max_backtrack_retries = 5;
  }
  const bool followedRoute =
      routeId == RouteId::Level1Torch2
          ? FollowLevel1Torch2Route(route, routeOptions)
          : FollowRouteWithRetries(routeId, route.name, routeOptions);
  if (!followedRoute) {
    if (routeId == RouteId::Level3BossLoop && IsRavensPointQuestComplete()) {
      LogBot("Ravens: treating Level3BossLoop route failure as success because "
             "Raven quest is complete (logState=%u)",
             QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
                 ? QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)->log_state
                 : 0u);
      Log::Info("Ravens: Level3BossLoop tolerated route failure after quest "
                "completion");
    } else if ((routeId == RouteId::Level2Torch2 ||
                routeId == RouteId::Level2Torch3) &&
               TryRecoverTorchChestApproach(routeId, route)) {
      LogBot("Ravens: recovered %s chest approach after route failure",
             route.name);
      Log::Info("Ravens: recovered %s chest approach after route failure",
                route.name);
    } else {
      LogBot("Ravens: route %s failed before objectives", route.name);
      Log::Info("Ravens: route %s failed before objectives", route.name);
      return false;
    }
  }

  if (routeId == RouteId::Level3BossLoop && IsRavensPointQuestComplete()) {
    LogBot("Ravens: Level3BossLoop complete; quest logState=%u",
           QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
               ? QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)->log_state
               : 0u);
  } else if (routeId == RouteId::Level3BossLoop) {
    LogBot("Ravens: Level3BossLoop finished without quest completion yet "
           "(logState=%u)",
           QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
               ? QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)->log_state
               : 0u);
  }

  if (const LootObjective *loot = FindLootObjective(routeId)) {
    DungeonBundle::PickUpNearestItemNearPoint(loot->pickup_point.x,
                                              loot->pickup_point.y, 1800.0f,
                                              loot->pickup_retries, 500u);
  }

  if (const TorchObjective *torch = FindTorchObjective(routeId)) {
    LogBot("Ravens: opening torch chest on %s", route.name);
    const uint32_t freeSlotsBeforeTorch = AdvancedInventory::CountFreeSlots();
    LogBot("Ravens: inventory free slots before torch chest on %s = %u",
           route.name, freeSlotsBeforeTorch);
    Log::Info("Ravens: inventory free slots before torch chest on %s = %u",
              route.name, freeSlotsBeforeTorch);
    if (freeSlotsBeforeTorch == 0u) {
      AdvancedInventory::EmergencyFreeSlotOptions freeSlotOptions;
      freeSlotOptions.log_prefix = "Ravens";
      freeSlotOptions.wait_ms = &WaitMs;
      freeSlotOptions.post_drop_wait_ms = 1000u;
      const auto dropResult =
          AdvancedInventory::DropEmergencyInventoryItemForFreeSlot(
              freeSlotOptions);
      LogBot("Ravens: emergency free-slot result on %s dropped=%d item=%u "
             "model=%u freeSlotsNow=%u",
             route.name, dropResult.dropped ? 1 : 0, dropResult.item_id,
             dropResult.model_id, AdvancedInventory::CountFreeSlots());
      Log::Info("Ravens: emergency free-slot result on %s dropped=%d item=%u "
                "model=%u freeSlotsNow=%u",
                route.name, dropResult.dropped ? 1 : 0, dropResult.item_id,
                dropResult.model_id, AdvancedInventory::CountFreeSlots());
    }
    if (!AcquireTorchBundleForRoute(routeId, *torch, route.map_id,
                                    route.name)) {
      LogBot("Ravens: failed to acquire held torch bundle on %s", route.name);
      return false;
    }
    const uint32_t acquiredTorchItem =
        DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(kUnlitTorchModelId);
    LogBot("Ravens: acquired torch bundle item=%u heldBundle=%u on %s",
           acquiredTorchItem, DungeonInteractions::GetHeldBundleItemId(),
           route.name);
    if (!ExecuteTorchLightingPath(routeId, *torch, route.map_id, route.name)) {
      return false;
    }
    if (!MoveToTorchObjectivePoint(routeId, route.name, "drop", 0,
                                   torch->drop_point, route.map_id)) {
      return false;
    }
    const bool droppedTorch = acquiredTorchItem != 0u
                                  ? DungeonBundle::DropHeldOrEquippedBundleItem(
                                        acquiredTorchItem)
                                  : DungeonBundle::DropHeldOrEquippedBundleByModel(
                                        kUnlitTorchModelId);
    if (!droppedTorch) {
      LogBot("Ravens: failed to drop torch bundle after lighting on %s",
             route.name);
      return false;
    }
    WaitMs(500u);
    if (!ExecutePostTorchDropResume(routeId, *torch, route.map_id,
                                    route.name)) {
      return false;
    }
  }

  if (!ExecuteDoorObjective(routeId, route)) {
    return false;
  }

  if (!waitForTransition) {
    return true;
  }

  const auto *zonePoint = route.waypoint_count > 0
                              ? &route.waypoints[route.waypoint_count - 1]
                              : nullptr;
  if (zonePoint == nullptr) {
    return false;
  }

  const auto pushPoint = GetTransitionPushPoint(routeId, *zonePoint);
  LogBot("Ravens: pushing transition for %s via (%.0f, %.0f) to map %u",
         route.name, pushPoint.x, pushPoint.y, route.next_map_id);
  if (!ZoneThroughPoint(pushPoint.x, pushPoint.y, route.next_map_id)) {
    LogBot("Ravens: failed transition for %s via (%.0f, %.0f) to map %u",
           route.name, pushPoint.x, pushPoint.y, route.next_map_id);
    return false;
  }
  return true;
}

bool ExecuteLevel3BossLoopObjective() {
  LogBot("Ravens: starting timed Level3BossLoop objective");
  if (!ExecuteRoute(RouteId::Level3BossLoop, false)) {
    return IsRavensPointQuestComplete();
  }

  if (IsRavensPointQuestComplete()) {
    return true;
  }

  const DWORD start = GetTickCount();
  while ((GetTickCount() - start) < 45000u &&
         MapMgr::GetMapId() == GWA3::MapIds::RAVENS_POINT_LVL3 &&
         !IsRavensPointQuestComplete()) {
    LogBot("Ravens: repeating Level3BossLoop while waiting for completion "
           "(elapsed=%lu logState=%u)",
           static_cast<unsigned long>(GetTickCount() - start),
           QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
               ? QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)->log_state
               : 0u);
    if (!ExecuteRoute(RouteId::Level3BossLoop, false) &&
        !IsRavensPointQuestComplete()) {
      return false;
    }
    WaitMs(1000u);
  }

  if (!IsRavensPointQuestComplete()) {
    LogBot("Ravens: Level3BossLoop timed out before Raven quest completion "
           "(logState=%u)",
           QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)
               ? QuestMgr::GetQuestById(GWA3::QuestIds::RAVENS_POINT)->log_state
               : 0u);
    return false;
  }
  return true;
}

bool ExecuteRewardChestFlow() {
  const auto reward = GetRewardChestObjective();
  LogBot("Ravens: reward chest flow start staging=(%.0f, %.0f) chest=(%.0f, %.0f)",
         reward.staging_point.x, reward.staging_point.y, reward.search_point.x,
         reward.search_point.y);
  Log::Info("Ravens: reward chest flow start staging=(%.0f, %.0f) chest=(%.0f, %.0f)",
            reward.staging_point.x, reward.staging_point.y, reward.search_point.x,
            reward.search_point.y);
  auto *me = AgentMgr::GetMyAgent();
  const bool alreadyNearChest =
      me != nullptr &&
      DungeonNavigation::IsWithinDistance(me->x, me->y, reward.search_point.x,
                                          reward.search_point.y, 3500.0f);
  if (alreadyNearChest) {
    Log::Info("Ravens: skipping reward staging; already near chest "
              "player=(%.0f, %.0f)",
              me->x, me->y);
  } else if (!MoveToTravelPoint(reward.staging_point,
                                GWA3::MapIds::RAVENS_POINT_LVL3)) {
    me = AgentMgr::GetMyAgent();
    const bool nearChestAfterFailedStaging =
        me != nullptr &&
        DungeonNavigation::IsWithinDistance(me->x, me->y,
                                            reward.search_point.x,
                                            reward.search_point.y, 3500.0f);
    LogBot("Ravens: failed moving to reward chest staging player=(%.0f, %.0f) map=%u",
           me ? me->x : 0.0f, me ? me->y : 0.0f, MapMgr::GetMapId());
    Log::Info("Ravens: failed moving to reward chest staging player=(%.0f, %.0f) map=%u nearChest=%d",
              me ? me->x : 0.0f, me ? me->y : 0.0f, MapMgr::GetMapId(),
              nearChestAfterFailedStaging ? 1 : 0);
    if (!nearChestAfterFailedStaging) {
      return false;
    }
  }

  bool interacted = false;
  for (int attempt = 0; attempt < reward.pickup_attempts; ++attempt) {
    const bool attemptInteracted =
        DungeonBundle::InteractSignpostAndPickUpLootNearPoint(
            reward.search_point.x, reward.search_point.y, 1500.0f,
            reward.interact_repeats, 5000u, 18000.0f, 1000u);
    interacted = interacted || attemptInteracted;
    Log::Info("Ravens: reward chest attempt=%d interactedOrLooted=%d map=%u",
              attempt + 1, attemptInteracted ? 1 : 0, MapMgr::GetMapId());
    WaitMs(1000u);
    if (MapMgr::GetMapId() == GWA3::MapIds::VARAJAR_FELLS_1) {
      return true;
    }
  }

  if (!interacted) {
    LogBot("Ravens: failed interacting with reward chest");
    Log::Info("Ravens: failed interacting with reward chest");
    return false;
  }

  const bool returned =
      DungeonNavigation::WaitForMapId(GWA3::MapIds::VARAJAR_FELLS_1, 180000u);
  LogBot("Ravens: reward chest return wait result=%d map=%u",
         returned ? 1 : 0, MapMgr::GetMapId());
  Log::Info("Ravens: reward chest return wait result=%d map=%u",
            returned ? 1 : 0, MapMgr::GetMapId());
  return returned;
}

BotState HandleCharSelect(BotConfig &) {
  return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

BotState HandleTownSetup(BotConfig &cfg) {
  const uint32_t mapId = MapMgr::GetMapId();
  if (mapId == GWA3::MapIds::VARAJAR_FELLS_1) {
    return BotState::Traveling;
  }
  if (mapId == GWA3::MapIds::RAVENS_POINT_LVL1 || mapId == GWA3::MapIds::RAVENS_POINT_LVL2 ||
      mapId == GWA3::MapIds::RAVENS_POINT_LVL3) {
    return BotState::InDungeon;
  }

  if (mapId != GWA3::MapIds::OLAFSTEAD) {
    LogBot("Ravens: traveling to Olafstead from map %u", mapId);
    MapMgr::Travel(GWA3::MapIds::OLAFSTEAD);
    if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::OLAFSTEAD, 60000u)) {
      return BotState::Error;
    }
    if (!WaitForMapReady(GWA3::MapIds::OLAFSTEAD)) {
      LogBot("Ravens: Olafstead did not finish loading after travel");
      return BotState::Error;
    }
    return BotState::InTown;
  }

  if (!WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 5000u)) {
    LogBot("Ravens: Olafstead not ready for outpost setup yet");
    return BotState::Error;
  }

  if (PartyMgr::CountPartyHeroes() >= 7u) {
    LogBot("Ravens: preserving existing full hero party for Olafstead setup");
    MapMgr::SetHardMode(true);
    WaitMs(1000u);
    return BotState::Traveling;
  }

  LogBot("Ravens: applying Olafstead outpost setup");
  if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg)) {
    LogBot("Ravens: preferred outpost setup failed; retrying with Standard.txt");
    cfg.hero_config_file = "Standard.txt";
    if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg)) {
      LogBot("Ravens: Olafstead outpost setup failed");
      return BotState::Error;
    }
  }
  return BotState::Traveling;
}

BotState HandleTravel(BotConfig &) {
  switch (MapMgr::GetMapId()) {
  case GWA3::MapIds::OLAFSTEAD:
    if (!WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 5000u)) {
      LogBot("Ravens: Olafstead not ready for travel route");
      return BotState::Error;
    }
    return ExecuteRoute(RouteId::RunOlafsteadToVarajarFells, true)
               ? BotState::Traveling
               : BotState::Error;
  case GWA3::MapIds::VARAJAR_FELLS_1:
    if (!WaitForMapReady(GWA3::MapIds::VARAJAR_FELLS_1, 10000u)) {
      LogBot("Ravens: Varajar not ready for blessing route");
      return BotState::Error;
    }
    {
      const RouteDefinition &dispatch =
          GetDispatchRouteDefinition(StageId::VarajarFells);
      const int nearestIndex = GetNearestWaypointIndexForRoute(dispatch);
      if (nearestIndex < 0) {
        LogBot("Ravens: no player agent available for Varajar dispatch");
        return BotState::Error;
      }
      LogBot("Ravens: Varajar dispatch nearest index=%d", nearestIndex);
      if (nearestIndex < 2) {
        if (!ExecuteVarajarBootstrap()) {
          return BotState::Error;
        }
      }
    }
    if (!ExecuteQuestCycle()) {
      return BotState::Error;
    }
    return BotState::InDungeon;
  case GWA3::MapIds::RAVENS_POINT_LVL1:
  case GWA3::MapIds::RAVENS_POINT_LVL2:
  case GWA3::MapIds::RAVENS_POINT_LVL3:
    return BotState::InDungeon;
  default:
    LogBot("Ravens: unsupported travel map %u", MapMgr::GetMapId());
    return BotState::Error;
  }
}

BotState HandleDungeon(BotConfig &) {
  const uint32_t mapId = MapMgr::GetMapId();
  if (mapId == GWA3::MapIds::RAVENS_POINT_LVL1 || mapId == GWA3::MapIds::RAVENS_POINT_LVL2 ||
      mapId == GWA3::MapIds::RAVENS_POINT_LVL3) {
    if (!WaitForPostZoneMapReady(mapId, 30000u)) {
      LogBot("Ravens: dungeon map %u not ready for route execution", mapId);
      return BotState::Error;
    }
  }

  switch (mapId) {
  case GWA3::MapIds::RAVENS_POINT_LVL1:
    if (!ExecuteRoute(RouteId::Level1Torch1, false) ||
        !ExecuteRoute(RouteId::Level1Torch2, false) ||
        !ExecuteRoute(RouteId::Level1DoorKey, false) ||
        !ExecuteRoute(RouteId::Level1Exit, true)) {
      return BotState::Error;
    }
    return BotState::InDungeon;
  case GWA3::MapIds::RAVENS_POINT_LVL2:
    if (!ExecuteRoute(RouteId::Level2Torch1, false) ||
        !ExecuteRoute(RouteId::Level2Torch2, false) ||
        !ExecuteRoute(RouteId::Level2Torch3, false) ||
        !ExecuteRoute(RouteId::Level2BossKey, false) ||
        !ExecuteRoute(RouteId::Level2Door, false) ||
        !ExecuteRoute(RouteId::Level2Exit, true)) {
      return BotState::Error;
    }
    return BotState::InDungeon;
  case GWA3::MapIds::RAVENS_POINT_LVL3:
    if (!ExecuteRoute(RouteId::Level3Approach, false) ||
        !ExecuteLevel3BossLoopObjective()) {
      return BotState::Error;
    }
    return ExecuteRewardChestFlow() ? BotState::InTown : BotState::Error;
  default:
    return BotState::InTown;
  }
}

BotState HandleError(BotConfig &) {
  LogBot("Ravens: ERROR state - waiting before retry");
  WaitMs(5000u);
  return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

} // namespace

void Register() {
  Bot::RegisterStateHandler(BotState::CharSelect, HandleCharSelect);
  Bot::RegisterStateHandler(BotState::InTown, HandleTownSetup);
  Bot::RegisterStateHandler(BotState::Traveling, HandleTravel);
  Bot::RegisterStateHandler(BotState::InDungeon, HandleDungeon);
  Bot::RegisterStateHandler(BotState::Error, HandleError);

  auto &cfg = Bot::GetConfig();
  for (uint32_t &hero_id : cfg.hero_ids) {
    hero_id = 0u;
  }
  cfg.hero_config_file.clear();
  cfg.hard_mode = true;
  cfg.target_map_id = GWA3::MapIds::RAVENS_POINT_LVL1;
  cfg.outpost_map_id = GWA3::MapIds::OLAFSTEAD;
  cfg.bot_module_name = "RavensPoint";

  LogBot("Ravens Point module registered (runtime in-progress)");
}

} // namespace GWA3::Bot::RavensPointBot
