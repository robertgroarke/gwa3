#include <bots/ravens_point/RavensPointBot.h>

#include <gwa3/advanced/Effects.h>
#include <gwa3/advanced/Diagnostics.h>
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
#include <gwa3/dungeon/DungeonVendor.h>
#include <bots/ravens_point/RavensPoint.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Memory.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>

#include <cstring>

namespace GWA3::Bot::RavensPointBot {

using namespace GWA3::Bot;
using namespace GWA3::Bot::RavensPoint;

namespace {

constexpr DungeonQuest::TravelPoint kVarajarBlessingPrepPath[] = {
    {-3393.0f, -1985.0f},
    {-2545.0f, -3501.0f},
    {-3926.0f, -4650.0f},
};
constexpr uint32_t kActionInteractCode = 0x80u;
constexpr uint32_t kUnlitTorchModelId = 22342u;
constexpr float kLegacyWaypointSignpostSearchRadius = 1500.0f;
constexpr float kLegacyWaypointLootSearchRadius = 18000.0f;
constexpr uint32_t kLegacyWaypointInteractDelayMs = 100u;
constexpr uint32_t kLegacyWaypointLootDelayMs = 500u;
constexpr uint32_t kDeldrimorTitleId = 0x27u;
constexpr uint32_t kDungeonBlessingDialogId = 0x84u;
constexpr uint32_t kTorchBrazierGadgetId = 8177u;
constexpr uint16_t kOlafsteadMerchantPlayerNumber = 6438u;
constexpr int kRouteWipeRecoveryMaxAttempts = 3;
constexpr uint32_t kQuietAsiaJapanRegion = 4u;
constexpr uint32_t kQuietAsiaJapanPreferredDistrict = 99u;
constexpr uint32_t kQuietAsiaJapanFallbackDistrict = 1u;
constexpr uint32_t kQuietAsiaJapanLanguage = 0u;
constexpr uint32_t kAmericaRegion = 0u;
constexpr uint32_t kAmericaDistrict = 0u;
constexpr uint32_t kEnglishLanguage = 0u;

uint32_t s_wipeCount = 0u;

struct RavensAggroWaypointContext {
  const char *route_name = "";
  bool enable_legacy_waypoint_hooks = true;
};

void WaitMs(uint32_t ms) { Sleep(ms); }

uint32_t GetSignpostGadgetId(uint32_t signpostId);
uint32_t FindSignpostGadgetNearPoint(float x, float y, float radius,
                                     uint32_t gadgetId);

bool HasDungeonBlessing() { return GWA3::AdvancedEffects::HasAnyDungeonBlessing(); }

void ClearInteractionStateAfterBlessing(const char *context) {
  AgentMgr::CancelAction();
  AgentMgr::ChangeTarget(0u);
  WaitMs(500u);
  Log::Info("Ravens: cleared interaction state after blessing for %s target=%u",
            context ? context : "<unknown>", AgentMgr::GetTargetId());
}

void LogRouteFailureSnapshot(const RouteDefinition &route, int absoluteIndex,
                             const char *context,
                             const DungeonNavigation::RouteFollowResult &result) {
  const auto *waypoint =
      absoluteIndex >= 0 && absoluteIndex < route.waypoint_count
          ? &route.waypoints[absoluteIndex]
          : nullptr;
  auto *me = AgentMgr::GetMyAgent();
  float nearestEnemyDist = 999999.0f;
  const uint32_t nearestEnemy =
      GWA3::AdvancedCombat::FindNearestLivingEnemy(4000.0f, &nearestEnemyDist);
  Log::Warn("Ravens: route failure snapshot context=%s route=%s wp=%d label=%s "
            "map=%u load=%u player=(%.0f, %.0f) hp=%.2f target=(%.0f, %.0f) "
            "dist=%.0f retries=%d partyDead=%d currentTarget=%u "
            "nearestEnemy=%u enemyDist=%.0f nearby1600=%u weaponType=%u "
            "weaponItemType=%u weaponItemId=%u",
            context ? context : "<unknown>", route.name ? route.name : "",
            absoluteIndex, waypoint && waypoint->label ? waypoint->label : "",
            MapMgr::GetMapId(), MapMgr::GetLoadingState(),
            me ? me->x : 0.0f, me ? me->y : 0.0f, me ? me->hp : 0.0f,
            waypoint ? waypoint->x : 0.0f, waypoint ? waypoint->y : 0.0f,
            me && waypoint ? AgentMgr::GetDistance(me->x, me->y, waypoint->x,
                                                   waypoint->y)
                            : 999999.0f,
            result.retries_used, PartyMgr::GetIsPartyDefeated() ? 1 : 0,
            AgentMgr::GetTargetId(), nearestEnemy, nearestEnemyDist,
            GWA3::AdvancedCombat::CountLivingEnemiesInRange(1600.0f),
            me ? static_cast<uint32_t>(me->weapon_type) : 0u,
            me ? static_cast<uint32_t>(me->weapon_item_type) : 0u,
            me ? static_cast<uint32_t>(me->weapon_item_id) : 0u);
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

bool PulseMoveToTravelPoint(const DungeonQuest::TravelPoint &point,
                            uint32_t mapId, float tolerance,
                            uint32_t timeoutMs,
                            uint32_t reissueMs = 250u) {
  const DWORD start = GetTickCount();
  while ((GetTickCount() - start) < timeoutMs) {
    if (MapMgr::GetMapId() != mapId) {
      return false;
    }

    auto *me = AgentMgr::GetMyAgent();
    if (me != nullptr &&
        AgentMgr::GetDistance(me->x, me->y, point.x, point.y) <= tolerance) {
      return true;
    }

    AgentMgr::Move(point.x, point.y);
    WaitMs(reissueMs);
  }

  auto *me = AgentMgr::GetMyAgent();
  return me != nullptr &&
         AgentMgr::GetDistance(me->x, me->y, point.x, point.y) <= tolerance;
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

bool WaitForCurrentMapTravelSettle(const char *context,
                                   uint32_t settleMs = 10000u) {
  const uint32_t mapId = MapMgr::GetMapId();
  if (mapId == 0u) {
    return false;
  }
  if (!WaitForMapReady(mapId, 15000u)) {
    Log::Warn("Ravens: current map %u was not ready before %s travel",
              mapId, context ? context : "outpost");
    return false;
  }

  Log::Info("Ravens: settling current map %u for %u ms before %s travel",
            mapId, settleMs, context ? context : "outpost");
  const DWORD start = GetTickCount();
  while ((GetTickCount() - start) < settleMs) {
    if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded() ||
        AgentMgr::GetMyId() == 0u) {
      return false;
    }
    WaitMs(250u);
  }
  return true;
}

void EnableRavensMapTravelBypasses() {
  static bool enabled = false;
  if (enabled) {
    return;
  }

  bool levelDataEnabled = false;
  bool mapPortEnabled = false;
  auto &levelDataPatch = GWA3::Memory::GetLevelDataBypassPatch();
  if (levelDataPatch.staged) {
    levelDataEnabled = levelDataPatch.Enable();
  }
  auto &mapPortPatch = GWA3::Memory::GetMapPortBypassPatch();
  if (mapPortPatch.staged) {
    mapPortEnabled = mapPortPatch.Enable();
  }
  Log::Info("Ravens: map travel bypass enable levelData=%d mapPort=%d",
            levelDataEnabled ? 1 : 0, mapPortEnabled ? 1 : 0);
  enabled = levelDataEnabled || mapPortEnabled;
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
  const uint32_t mapId = MapMgr::GetMapId();
  if (mapId != GWA3::MapIds::RAVENS_POINT_LVL1 &&
      mapId != GWA3::MapIds::RAVENS_POINT_LVL2 &&
      mapId != GWA3::MapIds::RAVENS_POINT_LVL3) {
    return;
  }

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

DungeonVendor::MaintenanceLocation MakeRavensMaintenanceLocation() {
  DungeonVendor::MaintenanceLocation location = {};
  location.outpost_map_id = GWA3::MapIds::OLAFSTEAD;
  location.merchant_x = 1582.0f;
  location.merchant_y = -1025.0f;
  location.merchant_move_threshold = 600.0f;
  location.merchant_search_radius = 2500.0f;
  // The nearby service cluster includes Brynn [Rare Scroll Trader]. Prefer
  // Galmann's live standard merchant player number and validate stock before
  // selling.
  location.merchant_player_number = kOlafsteadMerchantPlayerNumber;
  return location;
}

MaintenanceMgr::Config MakeRavensMaintenanceConfig() {
  auto config = DungeonVendor::BuildMaintenanceConfig(
      GWA3::MapIds::OLAFSTEAD, MakeRavensMaintenanceLocation());
  // Raven long-runs need inventory/kit/gold upkeep first. Conset conversion
  // requires additional town-service coordinates that Olafstead does not own.
  config.enableConsetRestock = false;
  return config;
}

bool NeedsRavensMaintenance() {
  return MaintenanceMgr::NeedsMaintenance(MakeRavensMaintenanceConfig());
}

bool TravelToOlafsteadForRavens(const char *context) {
  if (MapMgr::GetMapId() == GWA3::MapIds::OLAFSTEAD &&
      MapMgr::GetIsMapLoaded()) {
    return WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 15000u);
  }

  if (!WaitForCurrentMapTravelSettle(context)) {
    return false;
  }
  EnableRavensMapTravelBypasses();

  if (MapMgr::GetMapId() == GWA3::MapIds::VARAJAR_FELLS_1) {
    LogBot("Ravens: returning to Olafstead for %s from Varajar via ReturnToOutpost",
           context ? context : "startup");
    Log::Info(
        "Ravens: returning to Olafstead for %s from Varajar via ReturnToOutpost",
        context ? context : "startup");
    MapMgr::ReturnToOutpost();
    if (DungeonNavigation::WaitForMapId(GWA3::MapIds::OLAFSTEAD, 45000u) &&
        WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 15000u)) {
      return true;
    }
    Log::Warn("Ravens: ReturnToOutpost did not reach Olafstead for %s; "
              "falling back to direct map travel",
              context ? context : "startup");
  }

  LogBot("Ravens: traveling to Olafstead for %s via Asia/Japan district %u",
         context ? context : "startup", kQuietAsiaJapanPreferredDistrict);
  Log::Info("Ravens: traveling to Olafstead for %s via Asia/Japan district %u",
            context ? context : "startup", kQuietAsiaJapanPreferredDistrict);
  MapMgr::Travel(GWA3::MapIds::OLAFSTEAD, kQuietAsiaJapanRegion,
                 kQuietAsiaJapanPreferredDistrict, kQuietAsiaJapanLanguage);
  if (DungeonNavigation::WaitForMapId(GWA3::MapIds::OLAFSTEAD, 45000u) &&
      WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 15000u)) {
    return true;
  }

  LogBot("Ravens: preferred Olafstead district did not load for %s; "
         "falling back to Asia/Japan district %u",
         context ? context : "startup", kQuietAsiaJapanFallbackDistrict);
  Log::Warn("Ravens: preferred Olafstead district did not load for %s; "
            "falling back to Asia/Japan district %u",
            context ? context : "startup", kQuietAsiaJapanFallbackDistrict);
  MapMgr::Travel(GWA3::MapIds::OLAFSTEAD, kQuietAsiaJapanRegion,
                 kQuietAsiaJapanFallbackDistrict, kQuietAsiaJapanLanguage);
  if (DungeonNavigation::WaitForMapId(GWA3::MapIds::OLAFSTEAD, 45000u) &&
      WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 15000u)) {
    return true;
  }

  LogBot("Ravens: Asia/Japan Olafstead travel failed for %s; falling back to "
         "America English district 0",
         context ? context : "startup");
  Log::Warn("Ravens: Asia/Japan Olafstead travel failed for %s; falling back "
            "to America English district 0",
            context ? context : "startup");
  MapMgr::Travel(GWA3::MapIds::OLAFSTEAD, kAmericaRegion, kAmericaDistrict,
                 kEnglishLanguage);
  return DungeonNavigation::WaitForMapId(GWA3::MapIds::OLAFSTEAD, 60000u) &&
         WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 15000u);
}

bool RunRavensTownMaintenanceIfNeeded() {
  auto config = MakeRavensMaintenanceConfig();
  if (!MaintenanceMgr::NeedsMaintenance(config)) {
    return true;
  }

  const auto location = MakeRavensMaintenanceLocation();
  LogBot("Ravens: maintenance needed in Olafstead");
  Log::Info("Ravens: maintenance needed in Olafstead");

  (void)MoveToTravelPoint({location.merchant_x, location.merchant_y},
                          GWA3::MapIds::OLAFSTEAD,
                          location.merchant_move_threshold, 20000u);

  const bool opened = DungeonVendor::OpenMaintenanceMerchantContext(
      location, &MoveToPointForEffect, &WaitMs, "Ravens");
  if (opened) {
    MaintenanceMgr::PerformMaintenance(config);
    WaitMs(3000u);
  } else {
    LogBot("Ravens: maintenance merchant failed to open; depositing gold only");
    Log::Info("Ravens: maintenance merchant failed to open; depositing gold only");
    MaintenanceMgr::DepositGold(10000u);
  }

  uint32_t freeSlots = MaintenanceMgr::CountFreeSlots();
  bool stillNeedsMaintenance = MaintenanceMgr::NeedsMaintenance(config);
  if (freeSlots == 0u) {
    LogBot("Ravens: maintenance still has zero free slots in town; "
           "continuing so dungeon-side emergency drop can free space");
    Log::Warn("Ravens: maintenance still has zero free slots in town; "
              "continuing so dungeon-side emergency drop can free space");
  }

  if (stillNeedsMaintenance) {
    LogBot("Ravens: maintenance still reports low inventory after town pass; "
           "continuing with freeSlots=%u",
           freeSlots);
    Log::Warn("Ravens: maintenance still reports low inventory after town pass; "
              "continuing with freeSlots=%u",
              freeSlots);
  }

  LogBot("Ravens: maintenance complete freeSlots=%u", freeSlots);
  Log::Info("Ravens: maintenance complete freeSlots=%u", freeSlots);
  return true;
}

bool AcquireBlessingAt(const BlessingAnchor &blessing, uint32_t mapId,
                       const char *context) {
  const char *blessingName =
      blessing.log_name != nullptr ? blessing.log_name : "Dungeon blessing";
  const uint32_t requiredTitleId =
      blessing.required_title_id != 0u ? blessing.required_title_id
                                       : kDeldrimorTitleId;
  if (GWA3::AdvancedEffects::HasDungeonBlessingForTitle(requiredTitleId)) {
    LogBot("Ravens: %s already active for %s", blessingName,
           context ? context : "<unknown>");
    return true;
  }

  if (!MoveToTravelPoint({blessing.x, blessing.y}, mapId, 300.0f, 20000u)) {
    LogBot("Ravens: failed reaching %s for %s at (%.0f, %.0f)",
           blessingName, context ? context : "<unknown>", blessing.x,
           blessing.y);
    Log::Info("Ravens: failed reaching %s for %s at (%.0f, %.0f)",
              blessingName, context ? context : "<unknown>", blessing.x,
              blessing.y);
    return false;
  }

  GWA3::AdvancedEffects::BlessingInteractionOptions options;
  options.required_title_id = requiredTitleId;
  options.accept_dialog_id = kDungeonBlessingDialogId;
  options.log_prefix = blessingName;
  options.move_to_point = &MoveToPointForEffect;
  options.wait_ms = &WaitMs;
  options.signpost_scan_log = &GWA3::AdvancedDiagnostics::LogNearbySignposts;
  options.agent_log = &GWA3::AdvancedDiagnostics::LogAgentIdentity;
  options.require_specific_blessing = true;
  const auto result =
      GWA3::AdvancedEffects::AcquireDungeonBlessingAt(blessing.x, blessing.y,
                                                      options);

  const bool confirmed =
      result.confirmed ||
      GWA3::AdvancedEffects::HasDungeonBlessingForTitle(requiredTitleId);
  LogBot("Ravens: %s result for %s confirmed=%d title=0x%X", blessingName,
         context ? context : "<unknown>", confirmed ? 1 : 0,
         options.required_title_id);
  Log::Info("Ravens: %s result for %s confirmed=%d title=0x%X", blessingName,
            context ? context : "<unknown>", confirmed ? 1 : 0,
            options.required_title_id);
  if (confirmed) {
    ClearInteractionStateAfterBlessing(context);
  }
  return confirmed;
}

bool AcquireDwarvenBlessingAt(float x, float y, uint32_t mapId,
                              const char *context) {
  BlessingAnchor blessing;
  blessing.x = x;
  blessing.y = y;
  blessing.required_title_id = kDeldrimorTitleId;
  blessing.log_name = "Dwarven blessing";
  return AcquireBlessingAt(blessing, mapId, context);
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

bool TryRecoverLevel2Torch3WipeReturnPath(const char *context) {
  constexpr DungeonQuest::TravelPoint kTorch3Start = {580.0f, 6100.0f};
  constexpr DungeonQuest::TravelPoint kReturnPath[] = {
      {11547.0f, 5440.0f},
      {9334.0f, 6554.0f},
      {5050.0f, 8296.0f},
      {1667.0f, 6582.0f},
      kTorch3Start,
  };

  if (MapMgr::GetMapId() != GWA3::MapIds::RAVENS_POINT_LVL2) {
    return false;
  }

  auto *me = AgentMgr::GetMyAgent();
  if (!me) {
    return false;
  }

  float torch3StartDist =
      AgentMgr::GetDistance(me->x, me->y, kTorch3Start.x, kTorch3Start.y);
  if (torch3StartDist <= 2200.0f) {
    Log::Info("Ravens: Level2Torch3 wipe return already near route start "
              "context=%s dist=%.0f",
              context ? context : "<unknown>", torch3StartDist);
    return true;
  }

  int startIndex = 0;
  float nearestPathDist = 999999.0f;
  for (int i = 0;
       i < static_cast<int>(sizeof(kReturnPath) / sizeof(kReturnPath[0]));
       ++i) {
    const float dist =
        AgentMgr::GetDistance(me->x, me->y, kReturnPath[i].x,
                              kReturnPath[i].y);
    if (dist < nearestPathDist) {
      nearestPathDist = dist;
      startIndex = i;
    }
  }

  LogBot("Ravens: Level2Torch3 wipe return path start context=%s "
         "player=(%.0f, %.0f) torch3StartDist=%.0f nearestStep=%d dist=%.0f",
         context ? context : "<unknown>", me->x, me->y, torch3StartDist,
         startIndex, nearestPathDist);
  Log::Warn("Ravens: Level2Torch3 wipe return path start context=%s "
            "player=(%.0f, %.0f) torch3StartDist=%.0f nearestStep=%d dist=%.0f",
            context ? context : "<unknown>", me->x, me->y, torch3StartDist,
            startIndex, nearestPathDist);

  for (int i = startIndex;
       i < static_cast<int>(sizeof(kReturnPath) / sizeof(kReturnPath[0]));
       ++i) {
    if (MapMgr::GetMapId() != GWA3::MapIds::RAVENS_POINT_LVL2 ||
        DungeonBuiltinCombat::IsPlayerOrPartyDead()) {
      return false;
    }

    const auto &point = kReturnPath[i];
    const bool moved = DungeonBuiltinCombat::MoveToPointWithAggro(
        point.x, point.y, GWA3::MapIds::RAVENS_POINT_LVL2, 900.0f, 1300.0f,
        120000u);

    me = AgentMgr::GetMyAgent();
    const float pointDist =
        me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y)
           : 999999.0f;
    torch3StartDist =
        me ? AgentMgr::GetDistance(me->x, me->y, kTorch3Start.x,
                                   kTorch3Start.y)
           : 999999.0f;
    Log::Info("Ravens: Level2Torch3 wipe return step=%d moved=%d "
              "player=(%.0f, %.0f) point=(%.0f, %.0f) pointDist=%.0f "
              "torch3StartDist=%.0f dead=%d map=%u",
              i, moved ? 1 : 0, me ? me->x : 0.0f, me ? me->y : 0.0f,
              point.x, point.y, pointDist, torch3StartDist,
              DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0,
              MapMgr::GetMapId());

    if (torch3StartDist <= 2200.0f) {
      return true;
    }
    if (!moved && pointDist > 1800.0f) {
      Log::Warn("Ravens: Level2Torch3 wipe return aborting at step=%d "
                "pointDist=%.0f player=(%.0f, %.0f)",
                i, pointDist, me ? me->x : 0.0f, me ? me->y : 0.0f);
      return false;
    }
    WaitMs(500u);
  }

  return torch3StartDist <= 2400.0f;
}

bool TryRecoverRouteWipe(RouteId routeId, const DungeonRoute::Waypoint *routeSegment,
                         int segmentCount, int failedIndex, const char *context,
                         int absoluteStartIndex, int *nextStartIndex) {
  if (!MapMgr::GetIsMapLoaded() || AgentMgr::GetMyId() == 0u ||
      AgentMgr::GetMyAgent() == nullptr) {
    const uint32_t mapId = MapMgr::GetMapId();
    Log::Warn("Ravens: route recovery saw transient map/player not ready for %s "
              "(map=%u loaded=%d myId=%u); waiting instead of counting a wipe",
              context ? context : "<unknown>", mapId,
              MapMgr::GetIsMapLoaded() ? 1 : 0, AgentMgr::GetMyId());
    if (mapId != 0u && WaitForPostZoneMapReady(mapId, 15000u)) {
      if (nextStartIndex != nullptr) {
        *nextStartIndex =
            absoluteStartIndex + (failedIndex >= 0 ? failedIndex : 0);
      }
      return true;
    }
  }

  if (!DungeonBuiltinCombat::IsPlayerOrPartyDead()) {
    return false;
  }

  DungeonCheckpoint::RouteWipeRecoveryOptions recoveryOptions;
  recoveryOptions.waypoints = routeSegment;
  recoveryOptions.waypoint_count = segmentCount;
  recoveryOptions.current_index = failedIndex >= 0 ? failedIndex : 0;
  recoveryOptions.log_prefix = "Ravens";
  recoveryOptions.recovery.waypoints = routeSegment;
  recoveryOptions.recovery.waypoint_count = segmentCount;
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

  int restartIndex = recovery.restart_index;
  if (routeId == RouteId::Level2Torch3 && !recovery.returned_to_outpost) {
    if (!TryRecoverLevel2Torch3WipeReturnPath(context)) {
      LogBot("Ravens: Level2Torch3 wipe return path failed for %s",
             context ? context : "<unknown>");
      Log::Warn("Ravens: Level2Torch3 wipe return path failed for %s",
                context ? context : "<unknown>");
      return false;
    }
    restartIndex = 0;
  }

  if (nextStartIndex != nullptr) {
    *nextStartIndex = absoluteStartIndex + restartIndex;
  }
  LogBot("Ravens: recovered wipe for %s; restarting at waypoint %d",
         context ? context : "<unknown>",
         absoluteStartIndex + restartIndex);
  return true;
}

DungeonNavigation::RouteFollowResult FollowWaypointSegmentWithRetries(
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
    const auto *routeSegment = route.waypoints + currentStartIndex;
    const int currentCount = endIndex - currentStartIndex;

    if (UsesAggroTraversal(routeId)) {
      auto aggroRouteOptions = options;
      aggroRouteOptions.reissue_ms = 100u;

      DungeonCombat::AggroAdvanceOptions aggroOptions;
      DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(
          aggroOptions, aggroRouteOptions.waypoint_timeout_ms, true);
      aggroOptions.move_wait_ms = 100u;
      aggroOptions.timeout_ms = aggroRouteOptions.waypoint_timeout_ms;
      if (routeId == RouteId::Level1Torch1) {
        // AutoIt's MoveandAggro gated on each waypoint's fight range here.
        // The Torch #1 approach can see side mobs through terrain just beyond
        // fight range and stall forever if the shared 1600 floor is required.
        aggroOptions.clear_options.minimum_local_clear_range = 0.0f;
        aggroOptions.clear_options.extra_clear_range = 0.0f;
        aggroOptions.stuck_recovery_threshold = 30;
        aggroOptions.stuck_abort_threshold = 240;
        aggroOptions.stuck_recovery_radius = 900.0f;
      }
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
        // The last Level1Torch2 route segment is the chest approach. Keep the early
        // route on Froggy-style clearing, then avoid timing out on edge foes
        // once the run is already committed to the torch chest.
        aggroOptions.clear_options.minimum_local_clear_range = 0.0f;
        aggroOptions.clear_options.extra_clear_range = 0.0f;
        aggroOptions.stuck_recovery_threshold = 30;
        aggroOptions.stuck_abort_threshold = 240;
        aggroOptions.stuck_recovery_radius = 900.0f;
      }
      if (routeId == RouteId::Level1DoorKey) {
        // AutoIt's door-key route only gated on each waypoint's fight range.
        // The shared 1600 local-clear floor can catch edge enemies through
        // terrain around the brazier/lock approach and strand the run.
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
          routeSegment, currentCount, route.map_id,
          DungeonBuiltinCombat::MakeCombatCallbacks(), aggroRouteOptions,
          aggroOptions, waypointCallbacks);
    } else {
      followResult =
          DungeonNavigation::FollowWaypoints(routeSegment, currentCount, route.map_id,
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
        if (TryRecoverRouteWipe(routeId, routeSegment, currentCount,
                                followResult.failed_index, context,
                                currentStartIndex,
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
      LogRouteFailureSnapshot(route, absoluteIndex, context, followResult);
      LogBot("Ravens: failed %s at waypoint %d (%s) after %d retries", context,
             absoluteIndex, route.waypoints[absoluteIndex].label,
             followResult.retries_used);
      Log::Info("Ravens: failed %s at waypoint %d (%s) after %d retries",
                context, absoluteIndex, route.waypoints[absoluteIndex].label,
                followResult.retries_used);
    } else {
      LogRouteFailureSnapshot(route, -1, context, followResult);
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
  auto followResult = FollowWaypointSegmentWithRetries(
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
  const float acceptanceDistance =
      routeId == RouteId::Level1Torch1
          ? 650.0f
          : (routeId == RouteId::Level2Torch3 ? 2200.0f : 1600.0f);
  LogBot("Ravens: %s chest approach recovery start player=(%.0f, %.0f) "
         "chestDist=%.0f",
         routeName, me ? me->x : 0.0f, me ? me->y : 0.0f, chestDistance);
  Log::Info("Ravens: %s chest approach recovery start player=(%.0f, %.0f) "
            "chestDist=%.0f",
            routeName, me ? me->x : 0.0f, me ? me->y : 0.0f, chestDistance);

  if (chestDistance <= acceptanceDistance) {
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
  if (routeId == RouteId::Level1Torch1) {
    // Do not consume the Level1 torch chest while edge enemies are still
    // active. The chest interaction is flaky if combat steals focus here.
    aggroOptions.clear_options.minimum_local_clear_range = 1600.0f;
    aggroOptions.clear_options.change_target = true;
    aggroOptions.clear_options.call_target = true;
    aggroOptions.clear_options.chase_during_clear = true;
    aggroOptions.clear_options.chase_distance = 950.0f;
  }

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

  return advanced || finalChestDistance <= acceptanceDistance;
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

  if (!AcquireBlessingAt(*blessing, route.map_id, route.name)) {
    LogBot("Ravens: failed acquiring Varajar blessing before quest run");
    Log::Info("Ravens: failed acquiring Varajar blessing before quest run");
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
    const float resumeDistance =
        me ? AgentMgr::GetDistance(me->x, me->y, door->resume_point.x,
                                   door->resume_point.y)
           : 999999.0f;
    const bool nearResume = resumeDistance <= 650.0f;
    Log::Info("Ravens: Level2Door post-lock aggro resume result=%d player=(%.0f, %.0f) "
              "target=(%.0f, %.0f) dist=%.0f near=%d map=%u",
              resumed ? 1 : 0, me ? me->x : 0.0f, me ? me->y : 0.0f,
              door->resume_point.x, door->resume_point.y, resumeDistance,
              nearResume ? 1 : 0, MapMgr::GetMapId());
    if (!resumed && !nearResume) {
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

bool RecoverLevel2BossKeyApproachFromTorch3Area(const char *context) {
  const TorchObjective *torch3 = FindTorchObjective(RouteId::Level2Torch3);
  if (torch3 == nullptr) {
    return false;
  }

  auto *me = AgentMgr::GetMyAgent();
  if (!me) {
    return false;
  }

  const float resumeDist = AgentMgr::GetDistance(
      me->x, me->y, torch3->resume_point.x, torch3->resume_point.y);
  if (resumeDist <= 1500.0f) {
    Log::Info("Ravens: Level2BossKey bridge recovery already near resume "
              "context=%s dist=%.0f",
              context ? context : "<unknown>", resumeDist);
    return true;
  }

  const float secondBrazierDist =
      torch3->brazier_count > 1
          ? AgentMgr::GetDistance(me->x, me->y, torch3->brazier_points[1].x,
                                  torch3->brazier_points[1].y)
          : 999999.0f;
  const float thirdBrazierDist =
      torch3->brazier_count > 2
          ? AgentMgr::GetDistance(me->x, me->y, torch3->brazier_points[2].x,
                                  torch3->brazier_points[2].y)
          : 999999.0f;
  const bool nearTorch3ExitArea =
      secondBrazierDist <= 1800.0f || thirdBrazierDist <= 1800.0f ||
      (me->x <= -4500.0f && me->x >= -7200.0f && me->y >= 11800.0f &&
       me->y <= 13200.0f);
  if (!nearTorch3ExitArea) {
    return false;
  }

  constexpr DungeonQuest::TravelPoint kTorch3BridgeResumePath[] = {
      {-5600.0f, 12650.0f},
      {-6300.0f, 13250.0f},
      {-7050.0f, 14050.0f},
      {-7800.0f, 14650.0f},
      {-8521.0f, 14992.0f},
  };

  Log::Info("Ravens: Level2BossKey bridge recovery start context=%s "
            "player=(%.0f, %.0f) resumeDist=%.0f brazier2Dist=%.0f "
            "brazier3Dist=%.0f",
            context ? context : "<unknown>", me->x, me->y, resumeDist,
            secondBrazierDist, thirdBrazierDist);

  const bool directBridgeMove = PulseMoveToTravelPoint(
      torch3->resume_point, GWA3::MapIds::RAVENS_POINT_LVL2, 1200.0f, 45000u);
  me = AgentMgr::GetMyAgent();
  const float directDist =
      me ? AgentMgr::GetDistance(me->x, me->y, torch3->resume_point.x,
                                 torch3->resume_point.y)
         : 999999.0f;
  Log::Info("Ravens: Level2BossKey direct bridge move result=%d "
            "player=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f",
            directBridgeMove ? 1 : 0, me ? me->x : 0.0f,
            me ? me->y : 0.0f, torch3->resume_point.x,
            torch3->resume_point.y, directDist);
  if (directBridgeMove || directDist <= 1800.0f) {
    return true;
  }

  for (int i = 0; i < static_cast<int>(sizeof(kTorch3BridgeResumePath) /
                                       sizeof(kTorch3BridgeResumePath[0]));
       ++i) {
    const auto &point = kTorch3BridgeResumePath[i];
    const bool moved = DungeonBuiltinCombat::MoveToPointWithAggro(
        point.x, point.y, GWA3::MapIds::RAVENS_POINT_LVL2, 650.0f, 1600.0f,
        120000u);
    me = AgentMgr::GetMyAgent();
    const float pointDist =
        me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y) : 999999.0f;
    const float targetDist =
        me ? AgentMgr::GetDistance(me->x, me->y, torch3->resume_point.x,
                                   torch3->resume_point.y)
           : 999999.0f;
    Log::Info("Ravens: Level2BossKey bridge recovery step=%d moved=%d "
              "player=(%.0f, %.0f) pointDist=%.0f targetDist=%.0f",
              i, moved ? 1 : 0, me ? me->x : 0.0f, me ? me->y : 0.0f,
              pointDist, targetDist);
    if (targetDist <= 1500.0f) {
      return true;
    }
    if (!moved && pointDist > 1600.0f) {
      return false;
    }
    WaitMs(500u);
  }

  me = AgentMgr::GetMyAgent();
  const float finalDist =
      me ? AgentMgr::GetDistance(me->x, me->y, torch3->resume_point.x,
                                 torch3->resume_point.y)
         : 999999.0f;
  return finalDist <= 1800.0f;
}

float DistanceToTravelPoint(const DungeonQuest::TravelPoint &point) {
  auto *me = AgentMgr::GetMyAgent();
  return me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y)
            : 999999.0f;
}

bool TryAutoItStyleAggroMoveToTravelPoint(
    const DungeonQuest::TravelPoint &point, uint32_t mapId, float fightRange,
    float tolerance, uint32_t timeoutMs, const char *label, int stepIndex) {
  if (MapMgr::GetMapId() != mapId ||
      DungeonBuiltinCombat::IsPlayerOrPartyDead()) {
    return false;
  }

  auto *me = AgentMgr::GetMyAgent();
  const float startDist =
      me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y) : 999999.0f;
  if (startDist <= tolerance) {
    return true;
  }

  const DWORD start = GetTickCount();
  DungeonCombat::AggroAdvanceOptions options;
  DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(options, timeoutMs,
                                                            true);
  options.arrival_threshold = tolerance;
  options.move_wait_ms = 100u;
  options.stuck_recovery_threshold = 30;
  options.stuck_abort_threshold = 90;
  options.stuck_recovery_radius = 900.0f;
  options.clear_options.minimum_local_clear_range = 1600.0f;
  options.clear_options.extra_clear_range = 0.0f;
  options.clear_options.change_target = true;
  options.clear_options.call_target = true;
  options.clear_options.chase_during_clear = true;
  options.clear_options.chase_distance = 950.0f;

  const bool advanced = DungeonCombat::AdvanceWithAggro(
      point.x, point.y, fightRange, DungeonBuiltinCombat::MakeCombatCallbacks(),
      options);

  me = AgentMgr::GetMyAgent();
  const float finalDist =
      me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y) : 999999.0f;
  Log::Info("Ravens: %s AutoIt-style combat aggro move step=%d advanced=%d "
            "player=(%.0f, %.0f) "
            "target=(%.0f, %.0f) startDist=%.0f finalDist=%.0f tolerance=%.0f "
            "elapsed=%lums map=%u dead=%d",
            label ? label : "move", stepIndex, advanced ? 1 : 0,
            me ? me->x : 0.0f,
            me ? me->y : 0.0f, point.x, point.y, startDist, finalDist,
            tolerance, static_cast<unsigned long>(GetTickCount() - start),
            MapMgr::GetMapId(),
            DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0);

  return MapMgr::GetMapId() == mapId &&
         !DungeonBuiltinCombat::IsPlayerOrPartyDead() &&
         finalDist <= tolerance;
}

bool TryRecoverLevel1Torch1PostDropPath(const TorchObjective &torch,
                                        uint32_t mapId,
                                        const char *routeName) {
  constexpr DungeonQuest::TravelPoint kLevel1Torch2Anchor = {-7113.0f,
                                                             -7617.0f};
  constexpr DungeonQuest::TravelPoint kRecoveryPath[] = {
      {-7609.0f, -14853.0f},
      {-5286.0f, -15742.0f},
      {-6014.0f, -14887.0f},
      {-5086.0f, -8969.0f},
      {-4650.0f, -8350.0f},
      {-4045.0f, -7944.0f},
      kLevel1Torch2Anchor,
  };

  Log::Warn("Ravens: Level1Torch1 post-drop recovery starting for %s "
            "resumeDist=%.0f torch2Dist=%.0f",
            routeName, DistanceToTravelPoint(torch.resume_point),
            DistanceToTravelPoint(kLevel1Torch2Anchor));

  for (int i = 0; i < static_cast<int>(sizeof(kRecoveryPath) /
                                       sizeof(kRecoveryPath[0]));
       ++i) {
    const auto &point = kRecoveryPath[i];
    bool moved = TryAutoItStyleAggroMoveToTravelPoint(
        point, mapId, 1600.0f, 900.0f, 30000u,
        "Level1Torch1 post-drop recovery", i);
    if (!moved && MapMgr::GetMapId() == mapId &&
        !DungeonBuiltinCombat::IsPlayerOrPartyDead()) {
      moved = PulseMoveToTravelPoint(point, mapId, 900.0f, 10000u);
    }

    auto *me = AgentMgr::GetMyAgent();
    const float pointDist =
        me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y)
           : 999999.0f;
    const float resumeDist =
        me ? AgentMgr::GetDistance(me->x, me->y, torch.resume_point.x,
                                   torch.resume_point.y)
           : 999999.0f;
    const float torch2Dist =
        me ? AgentMgr::GetDistance(me->x, me->y, kLevel1Torch2Anchor.x,
                                   kLevel1Torch2Anchor.y)
           : 999999.0f;
    Log::Info("Ravens: Level1Torch1 post-drop recovery step=%d moved=%d "
              "player=(%.0f, %.0f) point=(%.0f, %.0f) pointDist=%.0f "
              "resumeDist=%.0f torch2Dist=%.0f map=%u dead=%d",
              i, moved ? 1 : 0, me ? me->x : 0.0f, me ? me->y : 0.0f,
              point.x, point.y, pointDist, resumeDist, torch2Dist,
              MapMgr::GetMapId(),
              DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0);

    if (MapMgr::GetMapId() != mapId ||
        DungeonBuiltinCombat::IsPlayerOrPartyDead()) {
      return false;
    }
    if (resumeDist <= 2200.0f || torch2Dist <= 1800.0f) {
      return true;
    }
    if (!moved && i >= 2 && pointDist > 1400.0f) {
      Log::Warn("Ravens: Level1Torch1 post-drop recovery aborting stuck "
                "pocket at step=%d pointDist=%.0f player=(%.0f, %.0f)",
                i, pointDist, me ? me->x : 0.0f, me ? me->y : 0.0f);
      return false;
    }
    WaitMs(500u);
  }

  const float finalResumeDist = DistanceToTravelPoint(torch.resume_point);
  const float finalTorch2Dist = DistanceToTravelPoint(kLevel1Torch2Anchor);
  Log::Warn("Ravens: Level1Torch1 post-drop recovery finished far for %s "
            "resumeDist=%.0f torch2Dist=%.0f",
            routeName, finalResumeDist, finalTorch2Dist);
  return finalResumeDist <= 2600.0f || finalTorch2Dist <= 2200.0f;
}

bool ExecutePostTorchDropResume(RouteId routeId, const TorchObjective &torch,
                                uint32_t mapId, const char *routeName) {
  if (routeId == RouteId::Level1Torch1) {
    AgentMgr::ResetMoveState("Ravens Level1Torch1 post-drop resume");
    constexpr DungeonQuest::TravelPoint kLevel1Torch1PostDropPath[] = {
        {-7609.0f, -14853.0f},
        {-5286.0f, -15742.0f},
        {-6014.0f, -14887.0f},
        {-5086.0f, -8969.0f},
    };
    constexpr float kPostDropAggroTolerance = 650.0f;
    constexpr float kPostDropFallbackTolerance = 1100.0f;
    constexpr float kPostDropResumeTolerance = 1200.0f;
    for (int i = 0; i < static_cast<int>(sizeof(kLevel1Torch1PostDropPath) /
                                         sizeof(kLevel1Torch1PostDropPath[0]));
         ++i) {
      const auto &point = kLevel1Torch1PostDropPath[i];
      const bool moved = TryAutoItStyleAggroMoveToTravelPoint(
          point, mapId, 1600.0f, kPostDropAggroTolerance, 30000u,
          "Level1Torch1 post-drop", i);
      auto *me = AgentMgr::GetMyAgent();
      float pointDist =
          me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y)
             : 999999.0f;
      float resumeDist =
          me ? AgentMgr::GetDistance(me->x, me->y, torch.resume_point.x,
                                     torch.resume_point.y)
             : 999999.0f;
      Log::Info("Ravens: Level1Torch1 post-drop aggro step=%d moved=%d "
                "player=(%.0f, %.0f) point=(%.0f, %.0f) pointDist=%.0f "
                "resumeDist=%.0f dead=%d map=%u loaded=%d",
                i, moved ? 1 : 0, me ? me->x : 0.0f, me ? me->y : 0.0f,
                point.x, point.y, pointDist, resumeDist,
                DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0,
                MapMgr::GetMapId(), MapMgr::GetIsMapLoaded() ? 1 : 0);

      if (!moved && pointDist > kPostDropFallbackTolerance) {
        if (MapMgr::GetMapId() != mapId ||
            DungeonBuiltinCombat::IsPlayerOrPartyDead()) {
          LogBot("Ravens: failed post-torch aggro resume %d for %s "
                 "hard state map=%u dead=%d",
                 i, routeName, MapMgr::GetMapId(),
                 DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0);
          Log::Warn("Ravens: failed post-torch aggro resume %d for %s "
                    "hard state map=%u dead=%d",
                    i, routeName, MapMgr::GetMapId(),
                    DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0);
          return false;
        }

        const bool fallbackMoved =
            PulseMoveToTravelPoint(point, mapId, kPostDropFallbackTolerance,
                                   15000u);
        me = AgentMgr::GetMyAgent();
        pointDist =
            me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y)
               : 999999.0f;
        resumeDist =
            me ? AgentMgr::GetDistance(me->x, me->y, torch.resume_point.x,
                                       torch.resume_point.y)
               : 999999.0f;
        Log::Info("Ravens: Level1Torch1 post-drop fallback step=%d moved=%d "
                  "player=(%.0f, %.0f) pointDist=%.0f resumeDist=%.0f",
                  i, fallbackMoved ? 1 : 0, me ? me->x : 0.0f,
                  me ? me->y : 0.0f, pointDist, resumeDist);

        if (!fallbackMoved && pointDist > 1800.0f) {
          Log::Warn("Ravens: Level1Torch1 post-drop step=%d still far "
                    "pointDist=%.0f; continuing toward resume instead",
                    i, pointDist);
        }
      }
      WaitMs(500u);
    }

    auto *me = AgentMgr::GetMyAgent();
    float resumeDist =
        me ? AgentMgr::GetDistance(me->x, me->y, torch.resume_point.x,
                                   torch.resume_point.y)
           : 999999.0f;
    if (resumeDist > kPostDropResumeTolerance) {
      const bool movedToResume = TryAutoItStyleAggroMoveToTravelPoint(
          torch.resume_point, mapId, 1600.0f, 900.0f, 60000u,
          "Level1Torch1 post-drop resume", 0);
      me = AgentMgr::GetMyAgent();
      resumeDist =
          me ? AgentMgr::GetDistance(me->x, me->y, torch.resume_point.x,
                                     torch.resume_point.y)
             : 999999.0f;
      Log::Info("Ravens: Level1Torch1 post-drop resume move moved=%d "
                "player=(%.0f, %.0f) resume=(%.0f, %.0f) resumeDist=%.0f",
                movedToResume ? 1 : 0, me ? me->x : 0.0f,
                me ? me->y : 0.0f, torch.resume_point.x, torch.resume_point.y,
                resumeDist);

      if (!movedToResume && resumeDist > 1800.0f) {
        if (MapMgr::GetMapId() != mapId ||
            DungeonBuiltinCombat::IsPlayerOrPartyDead()) {
          LogBot("Ravens: failed Level1Torch1 post-drop resume for %s "
                 "hard state map=%u dead=%d",
                 routeName, MapMgr::GetMapId(),
                 DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0);
          Log::Warn("Ravens: failed Level1Torch1 post-drop resume for %s "
                    "hard state map=%u dead=%d",
                    routeName, MapMgr::GetMapId(),
                    DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0);
          return false;
        }

        const bool fallbackResume =
            PulseMoveToTravelPoint(torch.resume_point, mapId, 1500.0f,
                                   20000u);
        me = AgentMgr::GetMyAgent();
        resumeDist =
            me ? AgentMgr::GetDistance(me->x, me->y, torch.resume_point.x,
                                       torch.resume_point.y)
               : 999999.0f;
        Log::Info("Ravens: Level1Torch1 post-drop resume fallback moved=%d "
                  "player=(%.0f, %.0f) resumeDist=%.0f",
                  fallbackResume ? 1 : 0, me ? me->x : 0.0f,
                  me ? me->y : 0.0f, resumeDist);

        if (!fallbackResume && resumeDist > 2600.0f) {
          LogBot("Ravens: Level1Torch1 post-drop resume still far for %s "
                 "dist=%.0f; running recovery path",
                 routeName, resumeDist);
          Log::Warn("Ravens: Level1Torch1 post-drop resume still far for %s "
                    "dist=%.0f; running recovery path",
                    routeName, resumeDist);
          if (!TryRecoverLevel1Torch1PostDropPath(torch, mapId, routeName)) {
            LogBot("Ravens: Level1Torch1 post-drop recovery failed for %s",
                   routeName);
            Log::Warn("Ravens: Level1Torch1 post-drop recovery failed for %s",
                      routeName);
            return false;
          }
        }
      }
    }
  }

  if (routeId == RouteId::Level1Torch1) {
    return AcquireDwarvenBlessingAt(torch.resume_point.x, torch.resume_point.y,
                                    mapId, routeName);
  }

  if (routeId == RouteId::Level2Torch3) {
    Log::Info("Ravens: waiting for Level2Torch3 bridge before resume");
    WaitMs(8000u);
    if (!PulseMoveToTravelPoint(torch.resume_point, mapId, 1200.0f, 45000u) &&
        !MoveToTravelPoint(torch.resume_point, mapId, 300.0f, 90000u)) {
      if (!RecoverLevel2BossKeyApproachFromTorch3Area(routeName)) {
        LogBot("Ravens: failed moving to torch resume point for %s", routeName);
        Log::Info("Ravens: failed moving to torch resume point for %s",
                  routeName);
        return false;
      }
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

bool TryRecoverLevel2ExitTransition(const RouteDefinition &route,
                                    const char *context) {
  if (route.waypoint_count <= 0 ||
      MapMgr::GetMapId() != GWA3::MapIds::RAVENS_POINT_LVL2) {
    return false;
  }

  auto *me = AgentMgr::GetMyAgent();
  const DoorObjective *door = FindDoorObjective(RouteId::Level2Door);
  const float doorResumeDist =
      me != nullptr && door != nullptr
          ? AgentMgr::GetDistance(me->x, me->y, door->resume_point.x,
                                  door->resume_point.y)
          : 999999.0f;
  const float firstExitDist =
      me != nullptr ? AgentMgr::GetDistance(me->x, me->y, route.waypoints[0].x,
                                            route.waypoints[0].y)
                    : 999999.0f;
  if (doorResumeDist > 2600.0f && firstExitDist > 2600.0f) {
    Log::Info("Ravens: Level2Exit recovery skipped context=%s "
              "doorResumeDist=%.0f firstExitDist=%.0f",
              context ? context : "<unknown>", doorResumeDist, firstExitDist);
    return false;
  }

  LogBot("Ravens: Level2Exit recovery start context=%s player=(%.0f, %.0f) "
         "doorResumeDist=%.0f firstExitDist=%.0f",
         context ? context : "<unknown>", me ? me->x : 0.0f,
         me ? me->y : 0.0f, doorResumeDist, firstExitDist);
  Log::Info("Ravens: Level2Exit recovery start context=%s player=(%.0f, %.0f) "
            "doorResumeDist=%.0f firstExitDist=%.0f",
            context ? context : "<unknown>", me ? me->x : 0.0f,
            me ? me->y : 0.0f, doorResumeDist, firstExitDist);

  if (door != nullptr && doorResumeDist <= 2200.0f) {
    Log::Info("Ravens: Level2Exit recovery reopening/pushing door context=%s",
              context ? context : "<unknown>");
    (void)MoveToTravelPoint(door->interact_point, route.map_id, 450.0f,
                            20000u);
    (void)DungeonBundle::InteractSignpostNearPoint(
        door->interact_point.x, door->interact_point.y, 1800.0f,
        door->interact_repeats + 1, 1000u);
    WaitMs(1500u);
    constexpr DungeonQuest::TravelPoint kPostDoorPushPath[] = {
        {4910.0f, 13055.0f},
        {5600.0f, 13750.0f},
        {6390.0f, 14419.0f},
        {5060.0f, 16381.0f},
    };
    for (int i = 0; i < static_cast<int>(sizeof(kPostDoorPushPath) /
                                         sizeof(kPostDoorPushPath[0]));
         ++i) {
      const auto &point = kPostDoorPushPath[i];
      const bool moved = PulseMoveToTravelPoint(point, route.map_id, 650.0f,
                                                i == 0 ? 45000u : 60000u);
      me = AgentMgr::GetMyAgent();
      const float dist =
          me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y) : 999999.0f;
      Log::Info("Ravens: Level2Exit recovery post-door push step=%d moved=%d "
                "player=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f map=%u",
                i + 1, moved ? 1 : 0, me ? me->x : 0.0f,
                me ? me->y : 0.0f, point.x, point.y, dist, MapMgr::GetMapId());
      if (MapMgr::GetMapId() == route.next_map_id) {
        return true;
      }
      if (!moved && i == 0 && dist > 1400.0f) {
        break;
      }
    }
  }

  for (int i = 0; i < route.waypoint_count; ++i) {
    const auto &wp = route.waypoints[i];
    const bool moved = PulseMoveToTravelPoint(
        {wp.x, wp.y}, route.map_id, i == 0 ? 650.0f : 1200.0f, 30000u);
    me = AgentMgr::GetMyAgent();
    const float dist =
        me ? AgentMgr::GetDistance(me->x, me->y, wp.x, wp.y) : 999999.0f;
    Log::Info("Ravens: Level2Exit recovery step=%d moved=%d "
              "player=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f",
              i + 1, moved ? 1 : 0, me ? me->x : 0.0f,
              me ? me->y : 0.0f, wp.x, wp.y, dist);
    if (MapMgr::GetMapId() == route.next_map_id) {
      return true;
    }
  }

  const auto pushPoint =
      GetTransitionPushPoint(RouteId::Level2Exit,
                             route.waypoints[route.waypoint_count - 1]);
  const bool transitioned =
      ZoneThroughPoint(pushPoint.x, pushPoint.y, route.next_map_id, 90000u);
  LogBot("Ravens: Level2Exit recovery transition result=%d map=%u",
         transitioned ? 1 : 0, MapMgr::GetMapId());
  Log::Info("Ravens: Level2Exit recovery transition result=%d map=%u",
            transitioned ? 1 : 0, MapMgr::GetMapId());
  return transitioned;
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
  const float distance =
      me ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y) : 999999.0f;
  if (routeId == RouteId::Level2Torch3 && phase != nullptr &&
      strcmp(phase, "transit") == 0 && distance <= 1000.0f) {
    Log::Info("Ravens: accepting near Level2Torch3 torch transit route=%s "
              "index=%d target=(%.0f, %.0f) player=(%.0f, %.0f) dist=%.0f",
              routeName ? routeName : "<unknown>", index, point.x, point.y,
              me ? me->x : 0.0f, me ? me->y : 0.0f, distance);
    return true;
  }

  LogBot("Ravens: failed torch move route=%s phase=%s index=%d target=(%.0f, %.0f)",
         routeName ? routeName : "<unknown>", phase ? phase : "<unknown>",
         index, point.x, point.y);
  Log::Info("Ravens: failed torch move route=%s routeId=%u phase=%s index=%d "
            "target=(%.0f, %.0f) map=%u hp=%.2f player=(%.0f, %.0f) "
            "dist=%.0f dead=%d",
            routeName ? routeName : "<unknown>",
            static_cast<unsigned>(routeId), phase ? phase : "<unknown>",
            index, point.x, point.y, MapMgr::GetMapId(), me ? me->hp : 0.0f,
            me ? me->x : 0.0f, me ? me->y : 0.0f,
            distance,
            DungeonBuiltinCombat::IsPlayerOrPartyDead() ? 1 : 0);
  return false;
}

void ClearTargetForAutoItAction(const char *context) {
  AgentMgr::CancelAction();
  WaitMs(100u);
  AgentMgr::ForceChangeTarget(0u);
  WaitMs(150u);
  Log::Info("Ravens: cleared target for %s target=%u",
            context ? context : "action", AgentMgr::GetTargetId());
}

bool PressTorchBrazierSignpostBurst(const char *routeName, int brazierIndex,
                                    int burstIndex,
                                    const DungeonQuest::TravelPoint &point,
                                    int presses, uint32_t delayMs) {
  const bool interacted =
      DungeonBundle::InteractSignpostNearPoint(point.x, point.y, 1500.0f,
                                               presses, delayMs);
  Log::Info("Ravens: torch brazier signpost burst route=%s index=%d "
            "burst=%d presses=%d interacted=%d target=%u",
            routeName ? routeName : "<unknown>", brazierIndex, burstIndex,
            presses, interacted ? 1 : 0, AgentMgr::GetTargetId());
  return interacted;
}

bool PressTorchBrazierActionInteractBurst(const char *routeName,
                                          int brazierIndex,
                                          const char *phase, int presses,
                                          uint32_t delayMs) {
  if (presses <= 0) {
    return false;
  }

  bool queuedAny = false;
  for (int i = 0; i < presses; ++i) {
    const bool queued = UIMgr::ActionKeyPress(kActionInteractCode);
    queuedAny = queued || queuedAny;
    WaitMs(delayMs);
  }

  Log::Info("Ravens: torch brazier action-interact burst route=%s index=%d "
            "phase=%s presses=%d queued=%d target=%u",
            routeName ? routeName : "<unknown>", brazierIndex,
            phase ? phase : "<unknown>", presses, queuedAny ? 1 : 0,
            AgentMgr::GetTargetId());
  return queuedAny;
}

bool AutoItStyleLightBrazier(RouteId routeId, const char *routeName, int index,
                             const DungeonQuest::TravelPoint &point,
                             uint32_t mapId) {
  bool interacted = false;

  ClearTargetForAutoItAction("torch brazier signpost pass");
  interacted = DungeonBundle::InteractSignpostNearPoint(point.x, point.y,
                                                        1500.0f, 2, 250u) ||
               interacted;
  interacted =
      PressTorchBrazierActionInteractBurst(routeName, index, "initial", 2,
                                           250u) ||
      interacted;
  WaitMs(500u);

  ClearTargetForAutoItAction("torch brazier first signpost burst");
  interacted = PressTorchBrazierSignpostBurst(routeName, index, 1, point, 2,
                                              100u) ||
               interacted;
  interacted =
      PressTorchBrazierActionInteractBurst(routeName, index, "first", 2,
                                           100u) ||
      interacted;
  WaitMs(500u);

  (void)MoveToTravelPoint(point, mapId, 300.0f, 15000u);
  WaitMs(1000u);

  ClearTargetForAutoItAction("torch brazier second signpost burst");
  interacted = PressTorchBrazierSignpostBurst(routeName, index, 2, point, 2,
                                              100u) ||
               interacted;
  interacted =
      PressTorchBrazierActionInteractBurst(routeName, index, "second", 2,
                                           100u) ||
      interacted;
  WaitMs(1000u);

  interacted = PressTorchBrazierSignpostBurst(routeName, index, 3, point, 2,
                                              100u) ||
               interacted;
  interacted =
      PressTorchBrazierActionInteractBurst(routeName, index, "third", 2,
                                           100u) ||
      interacted;

  Log::Info("Ravens: torch brazier AutoIt-style interact route=%s routeId=%u "
            "index=%d target=(%.0f, %.0f) interacted=%d torchItem=%u "
            "heldBundle=%u",
            routeName ? routeName : "<unknown>", static_cast<unsigned>(routeId),
            index, point.x, point.y, interacted ? 1 : 0,
            DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
                kUnlitTorchModelId),
            DungeonInteractions::GetHeldBundleItemId());
  return interacted;
}

bool Level1Torch1ActionOnlyLightBrazier(
    const char *routeName, int index, const DungeonQuest::TravelPoint &point,
    uint32_t mapId) {
  bool interacted = false;

  // Match the original AutoIt LightBrazier cadence closely. The torch timeout
  // is tight enough that the slower signpost-heavy helper can miss the passage
  // open window even when every interaction is queued.
  WaitMs(1000u);
  ClearTargetForAutoItAction("Level1Torch1 legacy brazier first action pass");
  interacted =
      PressTorchBrazierActionInteractBurst(routeName, index, "legacy-first", 2,
                                           100u) ||
      interacted;
  WaitMs(500u);

  (void)MoveToTravelPoint(point, mapId, 300.0f, 15000u);
  WaitMs(1000u);
  ClearTargetForAutoItAction("Level1Torch1 legacy brazier second action pass");
  interacted =
      PressTorchBrazierActionInteractBurst(routeName, index, "legacy-second", 2,
                                           100u) ||
      interacted;
  WaitMs(1000u);

  interacted =
      PressTorchBrazierActionInteractBurst(routeName, index, "legacy-third", 2,
                                           100u) ||
      interacted;

  Log::Info("Ravens: Level1Torch1 legacy ActionInteract brazier route=%s "
            "index=%d target=(%.0f, %.0f) interacted=%d torchItem=%u "
            "heldBundle=%u",
            routeName ? routeName : "<unknown>", index, point.x, point.y,
            interacted ? 1 : 0,
            DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
                kUnlitTorchModelId),
            DungeonInteractions::GetHeldBundleItemId());
  return interacted;
}

uint32_t ResolveLevel1Torch1BrazierSignpost(
    const DungeonQuest::TravelPoint &point) {
  uint32_t signpostId =
      FindSignpostGadgetNearPoint(point.x, point.y, 900.0f,
                                  kTorchBrazierGadgetId);
  if (signpostId != 0u) {
    return signpostId;
  }
  return DungeonInteractions::FindNearestSignpost(point.x, point.y, 700.0f);
}

bool PressLevel1Torch1BrazierFastInteract(
    const char *routeName, int index, const DungeonQuest::TravelPoint &point) {
  bool interacted = false;
  const uint32_t signpostId = ResolveLevel1Torch1BrazierSignpost(point);
  if (signpostId != 0u) {
    ClearTargetForAutoItAction("Level1Torch1 fast brazier target");
    AgentMgr::ForceChangeTarget(signpostId);
    WaitMs(75u);
    interacted =
        AgentMgr::InteractAgentWorldAction(signpostId, true) || interacted;
    WaitMs(75u);
    AgentMgr::InteractSignpost(signpostId);
    WaitMs(75u);
    AgentMgr::InteractSignpostLegacy(signpostId);
    WaitMs(75u);
  } else {
    ClearTargetForAutoItAction("Level1Torch1 fast brazier fallback");
  }

  interacted = PressTorchBrazierActionInteractBurst(
                   routeName, index, "level1-fast-targeted", 2, 75u) ||
               interacted;

  auto *me = AgentMgr::GetMyAgent();
  auto *signpost = signpostId != 0u ? AgentMgr::GetAgentByID(signpostId)
                                    : nullptr;
  const float distPlayer =
      (me && signpost)
          ? AgentMgr::GetDistance(me->x, me->y, signpost->x, signpost->y)
          : 999999.0f;
  Log::Info("Ravens: Level1Torch1 fast brazier target route=%s index=%d "
            "signpost=%u gadget=%u target=(%.0f, %.0f) signpostPos=(%.0f, %.0f) "
            "distPlayer=%.0f interacted=%d currentTarget=%u",
            routeName ? routeName : "<unknown>", index, signpostId,
            GetSignpostGadgetId(signpostId), point.x, point.y,
            signpost ? signpost->x : 0.0f, signpost ? signpost->y : 0.0f,
            distPlayer, interacted ? 1 : 0, AgentMgr::GetTargetId());
  return interacted;
}

bool LightTorchBrazier(RouteId routeId, const char *routeName, int index,
                       const DungeonQuest::TravelPoint &point,
                       uint32_t mapId) {
  if (!MoveToTorchObjectivePoint(routeId, routeName, "brazier", index, point,
                                 mapId)) {
    return false;
  }

  const bool interacted =
      AutoItStyleLightBrazier(routeId, routeName, index, point, mapId);
  if (!interacted) {
    LogBot("Ravens: failed to interact with torch brazier %d on %s", index,
           routeName ? routeName : "<unknown>");
    return false;
  }
  return true;
}

bool FastLightLevel1Torch1Brazier(const char *routeName, int index,
                                  const DungeonQuest::TravelPoint &point,
                                  uint32_t mapId, DWORD firstLightTick) {
  if (!MoveToTorchObjectivePoint(RouteId::Level1Torch1, routeName, "brazier",
                                 index, point, mapId)) {
    return false;
  }

  const DWORD start = GetTickCount();
  const bool interacted =
      PressLevel1Torch1BrazierFastInteract(routeName, index, point);
  WaitMs(250u);

  const DWORD now = GetTickCount();
  const DWORD litElapsed =
      firstLightTick == 0u ? 0u : static_cast<DWORD>(now - firstLightTick);
  Log::Info("Ravens: Level1Torch1 fast brazier route=%s index=%d "
            "target=(%.0f, %.0f) interacted=%d elapsed=%lums "
            "sinceFirstLight=%lums torchItem=%u heldBundle=%u",
            routeName ? routeName : "<unknown>", index, point.x, point.y,
            interacted ? 1 : 0, static_cast<unsigned long>(now - start),
            static_cast<unsigned long>(litElapsed),
            DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
                kUnlitTorchModelId),
            DungeonInteractions::GetHeldBundleItemId());
  if (!interacted) {
    LogBot("Ravens: failed fast action-interact with Level1Torch1 brazier %d",
           index);
  }
  return interacted;
}

bool ExecuteTorchLightingPath(RouteId routeId, const TorchObjective &torch,
                              uint32_t mapId, const char *routeName) {
  if (routeId == RouteId::Level1Torch1) {
    DWORD firstLightTick = 0u;
    for (int i = 0; i < torch.brazier_count; ++i) {
      if (!FastLightLevel1Torch1Brazier(routeName, i, torch.brazier_points[i],
                                        mapId, firstLightTick)) {
        LogBot("Ravens: failed fast targeted Level1Torch1 brazier %d", i);
        return false;
      }
      if (i == 0) {
        firstLightTick = GetTickCount();
      }
      const DWORD now = GetTickCount();
      Log::Info("Ravens: Level1Torch1 AutoIt torch sequence route=%s index=%d "
                "target=(%.0f, %.0f) sinceFirstLight=%lums torchItem=%u "
                "heldBundle=%u",
                routeName ? routeName : "<unknown>", i,
                torch.brazier_points[i].x, torch.brazier_points[i].y,
                firstLightTick == 0u
                    ? 0u
                    : static_cast<unsigned long>(now - firstLightTick),
                DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
                    kUnlitTorchModelId),
                DungeonInteractions::GetHeldBundleItemId());
    }
    Log::Info("Ravens: Level1Torch1 AutoIt torch sequence complete "
              "elapsedSinceFirstLight=%lums; dropping promptly",
              static_cast<unsigned long>(GetTickCount() - firstLightTick));
    WaitMs(250u);
    return true;
  }

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

uint32_t GetSignpostGadgetId(uint32_t signpostId) {
  auto *agent = AgentMgr::GetAgentByID(signpostId);
  if (agent == nullptr || agent->type != 0x200u) {
    return 0u;
  }
  return static_cast<const AgentGadget *>(agent)->gadget_id;
}

uint32_t FindSignpostGadgetNearPoint(float x, float y, float radius,
                                     uint32_t gadgetId) {
  const uint32_t maxAgents = AgentMgr::GetMaxAgents();
  float bestDistSq = radius * radius;
  uint32_t bestId = 0u;
  for (uint32_t agentId = 1; agentId < maxAgents; ++agentId) {
    auto *agent = AgentMgr::GetAgentByID(agentId);
    if (agent == nullptr || agent->type != 0x200u) {
      continue;
    }
    auto *gadget = static_cast<const AgentGadget *>(agent);
    if (gadget->gadget_id != gadgetId) {
      continue;
    }
    const float distSq = AgentMgr::GetSquaredDistance(x, y, agent->x, agent->y);
    if (distSq < bestDistSq) {
      bestDistSq = distSq;
      bestId = agentId;
    }
  }
  return bestId;
}

bool PollAcquireTorchByModel(float x, float y, uint32_t timeoutMs,
                             uint32_t pollMs, const char *context) {
  const DWORD start = GetTickCount();
  int pickupAttempts = 0;
  uint32_t lastItemAgent = 0u;
  while ((GetTickCount() - start) < timeoutMs) {
    const uint32_t equippedTorch =
        DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(kUnlitTorchModelId);
    if (equippedTorch != 0u) {
      Log::Info("Ravens: %s acquired torch item=%u elapsed=%lums "
                "pickupAttempts=%d",
                context ? context : "torch", equippedTorch,
                static_cast<unsigned long>(GetTickCount() - start),
                pickupAttempts);
      return true;
    }

    const uint32_t itemAgent = DungeonInteractions::FindNearestItemByModel(
        x, y, 18000.0f, kUnlitTorchModelId);
    if (itemAgent != 0u) {
      auto *agent = AgentMgr::GetAgentByID(itemAgent);
      auto *itemAgentData = agent && agent->type == 0x400u
                                ? static_cast<const AgentItem *>(agent)
                                : nullptr;
      if (itemAgent != lastItemAgent) {
        auto *me = AgentMgr::GetMyAgent();
        Log::Info("Ravens: %s found torch ground item agent=%u item=%u "
                  "pos=(%.0f, %.0f) distCenter=%.0f distPlayer=%.0f",
                  context ? context : "torch", itemAgent,
                  itemAgentData ? itemAgentData->item_id : 0u,
                  agent ? agent->x : 0.0f, agent ? agent->y : 0.0f,
                  agent ? AgentMgr::GetDistance(agent->x, agent->y, x, y)
                        : 999999.0f,
                  (agent && me) ? AgentMgr::GetDistance(agent->x, agent->y,
                                                        me->x, me->y)
                                : 999999.0f);
        lastItemAgent = itemAgent;
      }
      AgentMgr::CancelAction();
      AgentMgr::ForceChangeTarget(0u);
      WaitMs(50u);
      ItemMgr::PickUpItem(itemAgent);
      ++pickupAttempts;
    }

    WaitMs(pollMs);
  }

  const uint32_t equippedTorch =
      DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(kUnlitTorchModelId);
  Log::Warn("Ravens: %s failed to acquire torch elapsed=%lums item=%u "
            "heldBundle=%u pickupAttempts=%d",
            context ? context : "torch",
            static_cast<unsigned long>(GetTickCount() - start), equippedTorch,
            DungeonInteractions::GetHeldBundleItemId(), pickupAttempts);
  return equippedTorch != 0u;
}

bool AcquireLevel1Torch1AutoItActionOnly(const TorchObjective &torch,
                                         uint32_t mapId,
                                         const char *routeName) {
  const char *context = "Level1Torch1 AutoIt action-only torch chest";
  Log::Info("Ravens: %s begin route=%s chest=(%.0f, %.0f)", context,
            routeName ? routeName : "<unknown>", torch.chest.x,
            torch.chest.y);

  (void)PulseMoveToTravelPoint(torch.chest, mapId, 250.0f, 7000u, 150u);
  WaitMs(1000u);
  ClearTargetForAutoItAction(context);
  const bool action1 = UIMgr::ActionKeyPress(kActionInteractCode);
  WaitMs(125u);
  const bool action2 = UIMgr::ActionKeyPress(kActionInteractCode);

  auto *me = AgentMgr::GetMyAgent();
  Log::Info("Ravens: %s action-only open action1=%d action2=%d target=%u "
            "player=(%.0f, %.0f)",
            context, action1 ? 1 : 0, action2 ? 1 : 0,
            AgentMgr::GetTargetId(), me ? me->x : 0.0f, me ? me->y : 0.0f);

  if (PollAcquireTorchByModel(torch.chest.x, torch.chest.y, 2000u, 250u,
                              context)) {
    return true;
  }

  for (int sweep = 1; sweep <= 3; ++sweep) {
    (void)MoveToTravelPoint(torch.chest, mapId, 250.0f, 7000u);
    Log::Info("Ravens: %s pickup sweep=%d", context, sweep);

    if (DungeonBundle::PickUpHeldBundleByModelNearPoint(
            torch.chest.x, torch.chest.y, kUnlitTorchModelId, 18000.0f, 1,
            500u)) {
      Log::Info("Ravens: %s pickup sweep=%d acquired torch item=%u",
                context, sweep,
                DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
                    kUnlitTorchModelId));
      return true;
    }

    if (DungeonBundle::PickUpNearestItemByModelNearPoint(
            torch.chest.x, torch.chest.y, kUnlitTorchModelId, 18000.0f, 1,
            500u) &&
        DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
            kUnlitTorchModelId) != 0u) {
      Log::Info("Ravens: %s model pickup sweep=%d acquired torch item=%u",
                context, sweep,
                DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
                    kUnlitTorchModelId));
      return true;
    }

    if (PollAcquireTorchByModel(torch.chest.x, torch.chest.y, 1000u, 250u,
                                context)) {
      return true;
    }
  }

  Log::Warn("Ravens: %s failed item=%u heldBundle=%u",
            context,
            DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
                kUnlitTorchModelId),
            DungeonInteractions::GetHeldBundleItemId());
  return false;
}

bool AcquireLevel1Torch1AutoItStyle(const TorchObjective &torch,
                                    uint32_t mapId,
                                    const char *routeName) {
  const char *context = "Level1Torch1 safe signpost torch chest";
  Log::Info("Ravens: %s begin route=%s chest=(%.0f, %.0f)",
            context, routeName ? routeName : "<unknown>", torch.chest.x,
            torch.chest.y);

  (void)PulseMoveToTravelPoint(torch.chest, mapId, 250.0f, 7000u, 150u);
  WaitMs(500u);

  uint32_t signpostId =
      FindSignpostGadgetNearPoint(torch.chest.x, torch.chest.y, 900.0f, 8469u);
  if (signpostId == 0u) {
    signpostId = DungeonInteractions::FindNearestSignpost(torch.chest.x,
                                                          torch.chest.y,
                                                          700.0f);
  }
  if (signpostId == 0u) {
    Log::Warn("Ravens: %s no signpost near chest", context);
    return false;
  }

  auto *signpost = AgentMgr::GetAgentByID(signpostId);
  auto *me = AgentMgr::GetMyAgent();
  Log::Info("Ravens: %s resolved signpost=%u gadget=%u pos=(%.0f, %.0f) "
            "distCenter=%.0f distPlayer=%.0f player=(%.0f, %.0f)",
            context, signpostId, GetSignpostGadgetId(signpostId),
            signpost ? signpost->x : 0.0f, signpost ? signpost->y : 0.0f,
            signpost ? AgentMgr::GetDistance(signpost->x, signpost->y,
                                             torch.chest.x, torch.chest.y)
                     : 999999.0f,
            (signpost && me) ? AgentMgr::GetDistance(signpost->x, signpost->y,
                                                     me->x, me->y)
                             : 999999.0f,
            me ? me->x : 0.0f, me ? me->y : 0.0f);

  for (int pass = 1; pass <= 4; ++pass) {
    if (DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(
            kUnlitTorchModelId) != 0u) {
      return true;
    }

    (void)PulseMoveToTravelPoint(torch.chest, mapId, 180.0f, 5000u, 150u);

    ClearTargetForAutoItAction(context);
    const bool action1 = UIMgr::ActionKeyPress(kActionInteractCode);
    WaitMs(150u);
    const bool action2 = UIMgr::ActionKeyPress(kActionInteractCode);
    WaitMs(350u);

    AgentMgr::InteractSignpostLegacy(signpostId);
    WaitMs(250u);
    if (PollAcquireTorchByModel(torch.chest.x, torch.chest.y, 1000u, 200u,
                                context)) {
      return true;
    }

    AgentMgr::InteractSignpost(signpostId);
    WaitMs(350u);
    AgentMgr::InteractSignpostLegacy(signpostId);
    WaitMs(350u);
    AgentMgr::ForceChangeTarget(0u);
    WaitMs(100u);

    Log::Info("Ravens: %s open pass=%d signpost=%u gadget=%u target=%u "
              "action1=%d action2=%d path=chest-action-key-signpost-fallback",
              context, pass, signpostId, GetSignpostGadgetId(signpostId),
              AgentMgr::GetTargetId(), action1 ? 1 : 0, action2 ? 1 : 0);

    if (PollAcquireTorchByModel(torch.chest.x, torch.chest.y, 3500u, 250u,
                                context)) {
      return true;
    }

    signpostId = FindSignpostGadgetNearPoint(torch.chest.x, torch.chest.y,
                                             900.0f, 8469u);
    if (signpostId == 0u) {
      signpostId = DungeonInteractions::FindNearestSignpost(
          torch.chest.x, torch.chest.y, 700.0f);
    }
    if (signpostId == 0u) {
      Log::Warn("Ravens: %s signpost disappeared after pass=%d", context,
                pass);
      return false;
    }
  }

  return PollAcquireTorchByModel(torch.chest.x, torch.chest.y, 2000u, 250u,
                                 context);
}

bool PrepareLevel1Torch1ChestForPickup(const TorchObjective &torch,
                                       uint32_t mapId,
                                       const char *routeName) {
  const char *name = routeName ? routeName : "<unknown>";
  (void)MoveToTravelPoint(torch.chest, mapId, 450.0f, 30000u);

  GWA3::AdvancedCombat::ClearEnemiesOptions clearOptions;
  clearOptions.minimum_local_clear_range = 1600.0f;
  clearOptions.extra_clear_range = 0.0f;
  clearOptions.timeout_ms = 60000u;
  clearOptions.target_timeout_ms = 30000u;
  clearOptions.quiet_confirmation_ms = 1000u;
  clearOptions.pickup_after_clear = false;
  clearOptions.change_target = true;
  clearOptions.call_target = true;
  clearOptions.chase_during_clear = true;
  clearOptions.chase_distance = 950.0f;
  clearOptions.flag_heroes = false;

  const uint32_t nearbyBefore =
      GWA3::AdvancedCombat::CountLivingEnemiesInRange(1600.0f);
  const bool cleared = GWA3::AdvancedCombat::ClearEnemiesInArea(
      1300.0f, DungeonBuiltinCombat::MakeCombatCallbacks(), clearOptions);
  const uint32_t nearbyAfter =
      GWA3::AdvancedCombat::CountLivingEnemiesInRange(1600.0f);
  auto *me = AgentMgr::GetMyAgent();
  const float chestDist =
      me ? AgentMgr::GetDistance(me->x, me->y, torch.chest.x, torch.chest.y)
         : 999999.0f;

  Log::Info("Ravens: Level1Torch1 pre-chest clear route=%s cleared=%d "
            "nearbyBefore=%u nearbyAfter=%u player=(%.0f, %.0f) "
            "chestDist=%.0f",
            name, cleared ? 1 : 0, nearbyBefore, nearbyAfter,
            me ? me->x : 0.0f, me ? me->y : 0.0f, chestDist);
  if (!cleared && nearbyAfter > 0u) {
    LogBot("Ravens: Level1Torch1 chest still has %u nearby enemies; "
           "not opening torch chest yet",
           nearbyAfter);
    return false;
  }
  return chestDist <= 650.0f || MoveToTravelPoint(torch.chest, mapId, 650.0f,
                                                  15000u);
}

bool AcquireTorchBundleForRoute(RouteId routeId, const TorchObjective &torch,
                                uint32_t mapId, const char *routeName) {
  if (routeId == RouteId::Level1Torch1 &&
      !PrepareLevel1Torch1ChestForPickup(torch, mapId, routeName)) {
    return false;
  }

  if (routeId == RouteId::Level1Torch1) {
    Log::Info("Ravens: using Level1Torch1 safe signpost torch acquire route=%s",
              routeName ? routeName : "<unknown>");
    if (AcquireLevel1Torch1AutoItStyle(torch, mapId, routeName)) {
      return true;
    }

    for (int retry = 1; retry <= 2; ++retry) {
      LogBot("Ravens: retrying Level1Torch1 safe torch acquire pass %d",
             retry);
      Log::Info("Ravens: retrying Level1Torch1 safe torch acquire pass=%d "
                "route=%s",
                retry, routeName ? routeName : "<unknown>");
      (void)MoveToTravelPoint(torch.chest, mapId, 250.0f, 15000u);
      WaitMs(1000u);

      if (AcquireLevel1Torch1AutoItStyle(torch, mapId, routeName)) {
        Log::Info("Ravens: Level1Torch1 safe retry acquired torch on pass %d",
                  retry);
        return true;
      }
    }
    return false;
  }

  const float signpostSearchRadius = 1500.0f;

  if (DungeonBundle::OpenChestAndAcquireHeldBundleByModelChestPreferred(
          torch.chest.x, torch.chest.y, kUnlitTorchModelId,
          signpostSearchRadius, 18000.0f, 2, 3, 500u, 500u)) {
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

  auto *me = AgentMgr::GetMyAgent();
  const int nearestIndex =
      me ? DungeonRoute::FindNearestWaypointIndex(route.waypoints,
                                                  route.waypoint_count, me->x,
                                                  me->y)
         : -1;
  if (nearestIndex >= 3) {
    const int resumeIndex = nearestIndex > 3 ? nearestIndex - 1 : 3;
    LogBot("Ravens: resuming Level1DoorKey door approach from waypoint %d",
           resumeIndex);
    Log::Info("Ravens: resuming Level1DoorKey door approach from waypoint %d "
              "nearest=%d player=(%.0f, %.0f)",
              resumeIndex, nearestIndex, me ? me->x : 0.0f,
              me ? me->y : 0.0f);
    auto doorApproach = FollowWaypointSegmentWithRetries(
        routeId, route, resumeIndex, route.waypoint_count - resumeIndex,
        "Level1DoorKey resumed door approach", routeOptions, true);
    if (!doorApproach.completed && !doorApproach.map_changed) {
      return false;
    }
    return ExecuteDoorObjective(routeId, route);
  }

  LogBot("Ravens: starting Level1DoorKey key approach");
  auto keyApproach = FollowWaypointSegmentWithRetries(
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
  auto doorApproach = FollowWaypointSegmentWithRetries(
      routeId, route, 3, route.waypoint_count - 3,
      "Level1DoorKey door approach", routeOptions, true);
  if (!doorApproach.completed && !doorApproach.map_changed) {
    return false;
  }

  return ExecuteDoorObjective(routeId, route);
}

bool FollowLevel1Torch1Route(
    const RouteDefinition &route,
    const DungeonNavigation::RouteFollowOptions &routeOptions) {
  auto earlyRoute = FollowWaypointSegmentWithRetries(
      RouteId::Level1Torch1, route, 0, 4, "Level1Torch1 combat approach",
      routeOptions, false);
  if (!earlyRoute.completed && !earlyRoute.map_changed) {
    return false;
  }

  if (TryRecoverTorchChestApproach(RouteId::Level1Torch1, route)) {
    LogBot("Ravens: Level1Torch1 chest proximity accepted after combat "
           "approach");
    Log::Info("Ravens: Level1Torch1 chest proximity accepted after combat "
              "approach");
    return true;
  }

  DungeonNavigation::RouteFollowOptions chestOptions = routeOptions;
  chestOptions.default_tolerance = 650.0f;
  chestOptions.waypoint_timeout_ms = 45000u;
  chestOptions.max_backtrack_retries = 1;

  auto chestRoute = FollowWaypointSegmentWithRetries(
      RouteId::Level1Torch1, route, 4, route.waypoint_count - 4,
      "Level1Torch1 chest approach", chestOptions, false);
  if (chestRoute.completed || chestRoute.map_changed) {
    return true;
  }

  if (TryRecoverTorchChestApproach(RouteId::Level1Torch1, route)) {
    LogBot("Ravens: recovered %s chest approach after route failure",
           route.name);
    Log::Info("Ravens: recovered %s chest approach after route failure",
              route.name);
    return true;
  }
  return false;
}

bool FollowLevel1Torch2Route(
    const RouteDefinition &route,
    const DungeonNavigation::RouteFollowOptions &routeOptions) {
  auto earlyRoute = FollowWaypointSegmentWithRetries(
      RouteId::Level1Torch2, route, 0, 4, "Level1Torch2 combat approach",
      routeOptions, false);
  if (!earlyRoute.completed && !earlyRoute.map_changed) {
    return false;
  }

  DungeonNavigation::RouteFollowOptions chestOptions = routeOptions;
  chestOptions.default_tolerance = 650.0f;
  chestOptions.waypoint_timeout_ms = 180000u;
  chestOptions.max_backtrack_retries = 5;

  auto chestRoute = FollowWaypointSegmentWithRetries(
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
    if (!AcquireBlessingAt(*blessing, route.map_id, route.name)) {
      return false;
    }
  }

  if (routeId == RouteId::Level2BossKey) {
    (void)RecoverLevel2BossKeyApproachFromTorch3Area(route.name);
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
  if (routeId == RouteId::Level2Exit) {
    // Do not accept the pre-door choke as arrival; the bot must push through
    // the opened lock before following the exit path.
    routeOptions.default_tolerance = 650.0f;
    routeOptions.waypoint_timeout_ms = 180000u;
    routeOptions.max_backtrack_retries = 5;
  }
  const bool followedRoute =
      routeId == RouteId::Level1Torch1
          ? FollowLevel1Torch1Route(route, routeOptions)
          : routeId == RouteId::Level1Torch2
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
    } else if (routeId == RouteId::Level2Exit &&
               TryRecoverLevel2ExitTransition(route, route.name)) {
      LogBot("Ravens: recovered %s transition after route failure", route.name);
      Log::Info("Ravens: recovered %s transition after route failure",
                route.name);
      return true;
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
    if (!AcquireDungeonKeyAtLootObjective(*loot, route.name)) {
      LogBot("Ravens: failed acquiring dungeon key for %s", route.name);
      Log::Info("Ravens: failed acquiring dungeon key for %s", route.name);
      return false;
    }
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
      freeSlotOptions.allow_green_items = true;
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
    if (!DungeonBundle::RestoreCombatWeaponAfterBundleDrop(500u)) {
      LogBot("Ravens: failed to restore weapon after torch drop on %s",
             route.name);
      return false;
    }
    if (!ExecutePostTorchDropResume(routeId, *torch, route.map_id,
                                    route.name)) {
      if (routeId == RouteId::Level1Torch1) {
        LogBot("Ravens: Level1Torch1 post-drop recovery failed; "
               "aborting route for clean outpost recovery");
        Log::Warn("Ravens: Level1Torch1 post-drop recovery failed; "
                  "aborting route for clean outpost recovery");
      }
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
      s_wipeCount = 0u;
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
  if (returned) {
    s_wipeCount = 0u;
  }
  return returned;
}

BotState HandleCharSelect(BotConfig &) {
  return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

BotState HandleTownSetup(BotConfig &cfg) {
  const uint32_t mapId = MapMgr::GetMapId();
  if (mapId == GWA3::MapIds::VARAJAR_FELLS_1) {
    if (NeedsRavensMaintenance()) {
      LogBot("Ravens: maintenance needed after Varajar return; traveling to Olafstead");
      Log::Info("Ravens: maintenance needed after Varajar return; traveling to Olafstead");
      if (!TravelToOlafsteadForRavens("maintenance")) {
        LogBot("Ravens: failed traveling to Olafstead for maintenance");
        return BotState::Error;
      }
      return BotState::InTown;
    }
    return BotState::Traveling;
  }
  if (mapId == GWA3::MapIds::RAVENS_POINT_LVL1 || mapId == GWA3::MapIds::RAVENS_POINT_LVL2 ||
      mapId == GWA3::MapIds::RAVENS_POINT_LVL3) {
    return BotState::InDungeon;
  }

  if (mapId != GWA3::MapIds::OLAFSTEAD) {
    LogBot("Ravens: traveling to Olafstead from map %u", mapId);
    if (!TravelToOlafsteadForRavens("startup")) {
      LogBot("Ravens: Olafstead did not finish loading after travel");
      return BotState::Error;
    }
    return BotState::InTown;
  }

  if (!WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 5000u)) {
    LogBot("Ravens: Olafstead not ready for outpost setup yet");
    return BotState::Error;
  }

  if (!RunRavensTownMaintenanceIfNeeded()) {
    return BotState::Stopping;
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
      const BlessingAnchor *varajarBlessing =
          FindBlessingAnchor(RouteId::RunVarajarToBlessing, 0);
      const bool needsBlessing =
          varajarBlessing != nullptr
              ? !GWA3::AdvancedEffects::HasDungeonBlessingForTitle(
                    varajarBlessing->required_title_id)
              : !HasDungeonBlessing();
      if (needsBlessing && nearestIndex >= 2) {
        LogBot("Ravens: Varajar has no dungeon blessing at nearest index=%d; "
               "forcing blessing bootstrap",
               nearestIndex);
        Log::Warn("Ravens: Varajar has no dungeon blessing at nearest index=%d; "
                  "forcing blessing bootstrap",
                  nearestIndex);
      }
      if (nearestIndex < 2 || needsBlessing) {
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

RouteId GetLevel1ResumeStartRoute() {
  auto *me = AgentMgr::GetMyAgent();
  if (!me) {
    return RouteId::Level1Torch1;
  }

  if (me->x <= -23000.0f && me->y >= 14000.0f) {
    Log::Info("Ravens: Level1 resume selected Level1Torch1 from entry door "
              "player=(%.0f, %.0f)",
              me->x, me->y);
    return RouteId::Level1Torch1;
  }

  const DoorObjective *door = FindDoorObjective(RouteId::Level1DoorKey);
  if (door != nullptr) {
    const float lockDist = AgentMgr::GetDistance(
        me->x, me->y, door->interact_point.x, door->interact_point.y);
    const float resumeDist = AgentMgr::GetDistance(
        me->x, me->y, door->resume_point.x, door->resume_point.y);
    if (resumeDist <= 2500.0f || me->y >= 7000.0f) {
      Log::Info("Ravens: Level1 resume selected Level1Exit from "
                "player=(%.0f, %.0f) doorResumeDist=%.0f",
                me->x, me->y, resumeDist);
      return RouteId::Level1Exit;
    }
    if (lockDist <= 5000.0f ||
        (me->x <= -5500.0f && me->x >= -17000.0f && me->y >= -500.0f &&
         me->y <= 7000.0f)) {
      Log::Info("Ravens: Level1 resume selected Level1DoorKey from "
                "player=(%.0f, %.0f) lockDist=%.0f resumeDist=%.0f",
                me->x, me->y, lockDist, resumeDist);
      return RouteId::Level1DoorKey;
    }
  }

  const TorchObjective *torch2 = FindTorchObjective(RouteId::Level1Torch2);
  if (torch2 != nullptr) {
    const float resumeDist = AgentMgr::GetDistance(
        me->x, me->y, torch2->resume_point.x, torch2->resume_point.y);
    if (resumeDist <= 3500.0f || (me->y >= 3500.0f && me->x > -9000.0f)) {
      Log::Info("Ravens: Level1 resume selected Level1DoorKey after torch2 "
                "from player=(%.0f, %.0f) torch2ResumeDist=%.0f",
                me->x, me->y, resumeDist);
      return RouteId::Level1DoorKey;
    }
  }

  const bool inTorch2ApproachBand =
      me->x >= -10500.0f && me->x <= -3500.0f && me->y >= -13000.0f &&
      me->y <= 3500.0f;
  if (inTorch2ApproachBand) {
    Log::Info("Ravens: Level1 resume selected Level1Torch2 from "
              "player=(%.0f, %.0f)",
              me->x, me->y);
    return RouteId::Level1Torch2;
  }

  return RouteId::Level1Torch1;
}

bool ExecuteLevel1Routes(RouteId startRoute) {
  switch (startRoute) {
  case RouteId::Level1Torch1:
    return ExecuteRoute(RouteId::Level1Torch1, false) &&
           ExecuteRoute(RouteId::Level1Torch2, false) &&
           ExecuteRoute(RouteId::Level1DoorKey, false) &&
           ExecuteRoute(RouteId::Level1Exit, true);
  case RouteId::Level1Torch2:
    return ExecuteRoute(RouteId::Level1Torch2, false) &&
           ExecuteRoute(RouteId::Level1DoorKey, false) &&
           ExecuteRoute(RouteId::Level1Exit, true);
  case RouteId::Level1DoorKey:
    return ExecuteRoute(RouteId::Level1DoorKey, false) &&
           ExecuteRoute(RouteId::Level1Exit, true);
  case RouteId::Level1Exit:
    return ExecuteRoute(RouteId::Level1Exit, true);
  default:
    return false;
  }
}

RouteId GetLevel2ResumeStartRoute() {
  auto *me = AgentMgr::GetMyAgent();
  if (!me) {
    return RouteId::Level2Torch1;
  }

  const TorchObjective *torch3 = FindTorchObjective(RouteId::Level2Torch3);
  if (torch3 != nullptr) {
    const float chestDist =
        AgentMgr::GetDistance(me->x, me->y, torch3->chest.x, torch3->chest.y);
    const float firstBrazierDist =
        torch3->brazier_count > 0
            ? AgentMgr::GetDistance(me->x, me->y, torch3->brazier_points[0].x,
                                    torch3->brazier_points[0].y)
            : 999999.0f;
    float nearestTransitDist = 999999.0f;
    for (int i = 0; i < torch3->transit_point_count; ++i) {
      const float transitDist =
          AgentMgr::GetDistance(me->x, me->y, torch3->transit_points[i].x,
                                torch3->transit_points[i].y);
      if (transitDist < nearestTransitDist) {
        nearestTransitDist = transitDist;
      }
    }

    const bool inTorch3TransitArea =
        chestDist <= 3500.0f || firstBrazierDist <= 2200.0f ||
        nearestTransitDist <= 2200.0f ||
        (me->x <= -2500.0f && me->x >= -7000.0f && me->y >= 4500.0f &&
         me->y <= 8500.0f);
    if (inTorch3TransitArea) {
      Log::Info("Ravens: Level2 resume selected Level2Torch3 from "
                "player=(%.0f, %.0f) chestDist=%.0f firstBrazierDist=%.0f "
                "nearestTransitDist=%.0f",
                me->x, me->y, chestDist, firstBrazierDist, nearestTransitDist);
      return RouteId::Level2Torch3;
    }

    const float resumeDist = AgentMgr::GetDistance(
        me->x, me->y, torch3->resume_point.x, torch3->resume_point.y);
    const float secondBrazierDist =
        torch3->brazier_count > 1
            ? AgentMgr::GetDistance(me->x, me->y, torch3->brazier_points[1].x,
                                    torch3->brazier_points[1].y)
            : 999999.0f;
    const float thirdBrazierDist =
        torch3->brazier_count > 2
            ? AgentMgr::GetDistance(me->x, me->y, torch3->brazier_points[2].x,
                                    torch3->brazier_points[2].y)
            : 999999.0f;
    if (resumeDist <= 3500.0f || secondBrazierDist <= 3000.0f ||
        thirdBrazierDist <= 3000.0f ||
        (me->x <= -2500.0f && me->y >= 10000.0f)) {
      Log::Info("Ravens: Level2 resume selected Level2BossKey from "
                "player=(%.0f, %.0f) resumeDist=%.0f brazier2Dist=%.0f "
                "brazier3Dist=%.0f",
                me->x, me->y, resumeDist, secondBrazierDist, thirdBrazierDist);
      return RouteId::Level2BossKey;
    }
  }

  const DoorObjective *door = FindDoorObjective(RouteId::Level2Door);
  if (door != nullptr) {
    const float lockDist = AgentMgr::GetDistance(
        me->x, me->y, door->interact_point.x, door->interact_point.y);
    const float resumeDist = AgentMgr::GetDistance(
        me->x, me->y, door->resume_point.x, door->resume_point.y);
    if (resumeDist <= 2200.0f) {
      Log::Info("Ravens: Level2 resume selected Level2Exit from "
                "player=(%.0f, %.0f) doorResumeDist=%.0f",
                me->x, me->y, resumeDist);
      return RouteId::Level2Exit;
    }
    if (lockDist <= 3000.0f) {
      Log::Info("Ravens: Level2 resume selected Level2Door from "
                "player=(%.0f, %.0f) lockDist=%.0f",
                me->x, me->y, lockDist);
      return RouteId::Level2Door;
    }
  }

  return RouteId::Level2Torch1;
}

bool ExecuteLevel2Routes(RouteId startRoute) {
  switch (startRoute) {
  case RouteId::Level2Torch1:
    return ExecuteRoute(RouteId::Level2Torch1, false) &&
           ExecuteRoute(RouteId::Level2Torch2, false) &&
           ExecuteRoute(RouteId::Level2Torch3, false) &&
           ExecuteRoute(RouteId::Level2BossKey, false) &&
           ExecuteRoute(RouteId::Level2Door, false) &&
           ExecuteRoute(RouteId::Level2Exit, true);
  case RouteId::Level2Torch2:
    return ExecuteRoute(RouteId::Level2Torch2, false) &&
           ExecuteRoute(RouteId::Level2Torch3, false) &&
           ExecuteRoute(RouteId::Level2BossKey, false) &&
           ExecuteRoute(RouteId::Level2Door, false) &&
           ExecuteRoute(RouteId::Level2Exit, true);
  case RouteId::Level2Torch3:
    return ExecuteRoute(RouteId::Level2Torch3, false) &&
           ExecuteRoute(RouteId::Level2BossKey, false) &&
           ExecuteRoute(RouteId::Level2Door, false) &&
           ExecuteRoute(RouteId::Level2Exit, true);
  case RouteId::Level2BossKey:
    return ExecuteRoute(RouteId::Level2BossKey, false) &&
           ExecuteRoute(RouteId::Level2Door, false) &&
           ExecuteRoute(RouteId::Level2Exit, true);
  case RouteId::Level2Door:
    return ExecuteRoute(RouteId::Level2Door, false) &&
           ExecuteRoute(RouteId::Level2Exit, true);
  case RouteId::Level2Exit:
    return ExecuteRoute(RouteId::Level2Exit, true);
  default:
    return false;
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
    if (!ExecuteLevel1Routes(GetLevel1ResumeStartRoute())) {
      return BotState::Error;
    }
    return BotState::InDungeon;
  case GWA3::MapIds::RAVENS_POINT_LVL2:
    if (!ExecuteLevel2Routes(GetLevel2ResumeStartRoute())) {
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
  const uint32_t mapId = MapMgr::GetMapId();
  if (mapId == GWA3::MapIds::RAVENS_POINT_LVL1 ||
      mapId == GWA3::MapIds::RAVENS_POINT_LVL2 ||
      mapId == GWA3::MapIds::RAVENS_POINT_LVL3) {
    LogBot("Ravens: dungeon error on map %u; returning to outpost", mapId);
    Log::Warn("Ravens: dungeon error on map %u; returning to outpost",
              mapId);

    MapMgr::ReturnToOutpost();
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < 90000u) {
      const uint32_t currentMapId = MapMgr::GetMapId();
      if (currentMapId == GWA3::MapIds::OLAFSTEAD &&
          WaitForMapReady(GWA3::MapIds::OLAFSTEAD, 15000u)) {
        return BotState::InTown;
      }
      if (currentMapId == GWA3::MapIds::VARAJAR_FELLS_1 &&
          WaitForMapReady(GWA3::MapIds::VARAJAR_FELLS_1, 15000u)) {
        return BotState::Traveling;
      }
      if (currentMapId != mapId && currentMapId != 0u &&
          MapMgr::GetIsMapLoaded()) {
        LogBot("Ravens: dungeon error returned to unexpected map %u",
               currentMapId);
        Log::Warn("Ravens: dungeon error returned to unexpected map %u",
                  currentMapId);
        return BotState::InTown;
      }
      WaitMs(500u);
    }

    LogBot("Ravens: failed returning to outpost after dungeon error "
           "(map=%u loaded=%d)",
           MapMgr::GetMapId(), MapMgr::GetIsMapLoaded() ? 1 : 0);
    Log::Warn("Ravens: failed returning to outpost after dungeon error "
              "(map=%u loaded=%d)",
              MapMgr::GetMapId(), MapMgr::GetIsMapLoaded() ? 1 : 0);
    return BotState::Error;
  }
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
