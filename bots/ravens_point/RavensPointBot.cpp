#include <bots/ravens_point/RavensPointBot.h>

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <bots/ravens_point/RavensPoint.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
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

struct RavensAggroWaypointContext {
  const char *route_name = "";
};

void WaitMs(DWORD ms) { Sleep(ms); }

bool IsQuestReadyForEntry(const Quest *quest) {
  return quest != nullptr && (quest->log_state & 0x02u) == 0u;
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

bool FollowRouteWithRetries(
    RouteId routeId, const char *context,
    const DungeonNavigation::RouteFollowOptions &options) {
  const RouteDefinition &route = GetRouteDefinition(routeId);
  DungeonNavigation::RouteFollowResult followResult;
  if (UsesAggroTraversal(routeId)) {
    auto aggroRouteOptions = options;
    aggroRouteOptions.reissue_ms = 100u;

    DungeonCombat::AggroAdvanceOptions aggroOptions;
    aggroOptions.clear_options.pickup_after_clear = true;
    aggroOptions.clear_options.flag_heroes = false;
    aggroOptions.clear_options.change_target = false;
    aggroOptions.clear_options.call_target = false;
    aggroOptions.clear_options.quiet_confirmation_ms = 1250u;
    aggroOptions.clear_options.chase_wait_ms = 300u;
    aggroOptions.clear_options.idle_wait_ms = 100u;
    aggroOptions.clear_options.loop_wait_ms = 100u;
    aggroOptions.clear_options.fight_reissue_ms = 100u;
    aggroOptions.clear_options.attack_reissue_ms = 100u;
    aggroOptions.clear_options.timeout_ms =
        aggroRouteOptions.waypoint_timeout_ms;
    aggroOptions.clear_options.target_timeout_ms =
        aggroRouteOptions.waypoint_timeout_ms;
    aggroOptions.timeout_ms = aggroRouteOptions.waypoint_timeout_ms;
    RavensAggroWaypointContext waypointContext;
    waypointContext.route_name = route.name;
    DungeonCombat::AggroWaypointCallbacks waypointCallbacks;
    waypointCallbacks.on_waypoint = &HandleRavensAggroWaypoint;
    waypointCallbacks.user_data = &waypointContext;
    followResult = DungeonCombat::FollowWaypointsWithAggro(
        route.waypoints, route.waypoint_count, route.map_id,
        DungeonBuiltinCombat::MakeCombatCallbacks(), aggroRouteOptions,
        aggroOptions, waypointCallbacks);
  } else {
    followResult = DungeonNavigation::FollowWaypoints(
        route.waypoints, route.waypoint_count, route.map_id, options);
  }
  if (followResult.completed || followResult.map_changed) {
    return true;
  }

  if (followResult.failed_index >= 0 &&
      followResult.failed_index < route.waypoint_count) {
    LogBot("Ravens: failed %s at waypoint %d (%s) after %d retries", context,
           followResult.failed_index,
           route.waypoints[followResult.failed_index].label,
           followResult.retries_used);
  } else {
    LogBot("Ravens: failed %s after %d retries", context,
           followResult.retries_used);
  }
  return false;
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
  } else {
    LogBot("Ravens: starting reward/quest bootstrap");
    if (!ExecuteQuestDialogs(plan, plan.reward_dialog, "reward bootstrap")) {
      return false;
    }

    if (!ExecuteQuestApproach(plan)) {
      LogBot("Ravens: failed on quest approach before reward bounce");
      return false;
    }
    if (!MoveToTravelPoint(plan.dungeon_entry, plan.start_map_id, 300.0f)) {
      LogBot("Ravens: failed reaching reward bootstrap entry point");
      return false;
    }
    if (!ZoneThroughPoint(plan.dungeon_entry.x, plan.dungeon_entry.y,
                          plan.dungeon_map_id)) {
      LogBot("Ravens: failed zoning into reward bootstrap dungeon instance");
      return false;
    }
    if (!WaitForMapReady(plan.dungeon_map_id, 10000u)) {
      LogBot("Ravens: reward bootstrap dungeon map did not finish loading");
      return false;
    }
    if (!ZoneThroughPoint(plan.dungeon_exit.x, plan.dungeon_exit.y,
                          plan.start_map_id)) {
      LogBot(
          "Ravens: failed reversing out of reward bootstrap dungeon instance");
      return false;
    }
    if (!WaitForMapReady(plan.start_map_id, 10000u)) {
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
    bool accepted = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
      if (!ExecuteQuestDialogs(plan, plan.accept_dialog, "quest accept")) {
        return false;
      }
      if (VerifyQuestState(true, "quest accept", true)) {
        accepted = true;
        break;
      }
      LogBot("Ravens: quest accept attempt %d did not produce an active quest "
             "state",
             attempt + 1);
    }
    if (!accepted) {
      return false;
    }
  }

  if (!ExecuteQuestApproach(plan)) {
    LogBot("Ravens: failed on quest approach before live entry");
    return false;
  }
  if (!MoveToTravelPoint(plan.dungeon_entry, plan.start_map_id, 300.0f)) {
    LogBot("Ravens: failed reaching live entry point");
    return false;
  }
  if (!ZoneThroughPoint(plan.dungeon_entry.x, plan.dungeon_entry.y,
                        plan.dungeon_map_id)) {
    LogBot("Ravens: failed zoning into Raven's Point level 1");
    return false;
  }
  return WaitForMapReady(plan.dungeon_map_id, 10000u);
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

bool ExecuteRoute(RouteId routeId, bool waitForTransition) {
  const RouteDefinition &route = GetRouteDefinition(routeId);
  auto *me = AgentMgr::GetMyAgent();
  if (!me) {
    LogBot("Ravens: no player agent available for route %s", route.name);
    return false;
  }

  const int startIndex = DungeonRoute::FindNearestWaypointIndex(
      route.waypoints, route.waypoint_count, me->x, me->y);

  if (const BlessingAnchor *blessing =
          FindBlessingAnchor(routeId, startIndex)) {
    DungeonNavigation::MoveToAndWait(blessing->x, blessing->y, 300.0f, 15000u,
                                     1000u, route.map_id);
  }

  DungeonNavigation::RouteFollowOptions routeOptions;
  routeOptions.waypoint_timeout_ms =
      UsesAggroTraversal(routeId) ? 120000u : 60000u;
  routeOptions.max_backtrack_retries = 3;
  if (!FollowRouteWithRetries(routeId, route.name, routeOptions)) {
    return false;
  }

  if (const LootObjective *loot = FindLootObjective(routeId)) {
    DungeonBundle::PickUpNearestItemNearPoint(loot->pickup_point.x,
                                              loot->pickup_point.y, 1800.0f,
                                              loot->pickup_retries, 500u);
  }

  if (const TorchObjective *torch = FindTorchObjective(routeId)) {
    LogBot("Ravens: opening torch chest on %s", route.name);
    if (!DungeonBundle::OpenChestAndAcquireHeldBundleByModelChestPreferred(
            torch->chest.x, torch->chest.y, kUnlitTorchModelId, 1500.0f,
            18000.0f, 2, 3, 500u, 500u)) {
      LogBot("Ravens: failed to acquire held torch bundle on %s", route.name);
      return false;
    }
    LogBot("Ravens: acquired torch bundle item=%u on %s",
           DungeonInteractions::GetHeldBundleItemId(), route.name);
    for (int i = 0; i < torch->transit_point_count; ++i) {
      if (!MoveToTravelPoint(torch->transit_points[i], route.map_id)) {
        return false;
      }
    }
    for (int i = 0; i < torch->brazier_count; ++i) {
      if (!MoveToTravelPoint(torch->brazier_points[i], route.map_id)) {
        return false;
      }
      DungeonBundle::InteractSignpostNearPoint(torch->brazier_points[i].x,
                                               torch->brazier_points[i].y,
                                               1500.0f, 1, 500u);
    }
    if (!MoveToTravelPoint(torch->drop_point, route.map_id)) {
      return false;
    }
    if (!DungeonInteractions::DropHeldBundle()) {
      LogBot("Ravens: expected a held torch bundle after chest on %s",
             route.name);
      return false;
    }
    WaitMs(500u);
    if (!MoveToTravelPoint(torch->resume_point, route.map_id)) {
      return false;
    }
  }

  if (const DoorObjective *door = FindDoorObjective(routeId)) {
    if (!MoveToTravelPoint(door->interact_point, route.map_id, 200.0f)) {
      return false;
    }
    if (!DungeonBundle::InteractSignpostNearPoint(
            door->interact_point.x, door->interact_point.y, 1500.0f,
            door->interact_repeats, 1000u)) {
      return false;
    }
    if (!MoveToTravelPoint(door->resume_point, route.map_id)) {
      return false;
    }
  }

  if (!waitForTransition) {
    return true;
  }

  const auto *zonePoint = route.waypoint_count > 0
                              ? &route.waypoints[route.waypoint_count - 1]
                              : nullptr;
  return zonePoint &&
         ZoneThroughPoint(zonePoint->x, zonePoint->y, route.next_map_id);
}

bool ExecuteRewardChestFlow() {
  const auto reward = GetRewardChestObjective();
  if (!MoveToTravelPoint(reward.staging_point, GWA3::MapIds::RAVENS_POINT_LVL3)) {
    return false;
  }
  if (!DungeonBundle::OpenChestAndPickUpBundle(
          reward.search_point.x, reward.search_point.y, 1500.0f, 18000.0f,
          reward.interact_repeats, reward.pickup_attempts, 5000u, 1000u)) {
    return false;
  }
  return DungeonNavigation::WaitForMapId(GWA3::MapIds::VARAJAR_FELLS_1, 180000u);
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

  LogBot("Ravens: applying Olafstead outpost setup");
  if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg)) {
    LogBot("Ravens: Olafstead outpost setup failed");
    return BotState::Error;
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
    if (!WaitForMapReady(mapId, 10000u)) {
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
        !ExecuteRoute(RouteId::Level3BossLoop, false)) {
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
