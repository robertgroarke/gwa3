#include <bots/froggy/FroggyHM.h>
#include <bots/froggy/FroggyHMConfig.h>
#include <bots/froggy/FroggyHMRoutePolicy.h>
#include <bots/froggy/FroggyHMRoutes.h>
#include <bots/common/BotFramework.h>
#include <bots/common/DungeonBotStates.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/dungeon/DungeonDiagnostics.h>
#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/dungeon/DungeonEffects.h>
#include <gwa3/dungeon/DungeonEntryRecovery.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/dungeon/DungeonItemActions.h>
#include <gwa3/dungeon/DungeonItemPolicy.h>
#include <gwa3/dungeon/DungeonLoot.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <gwa3/dungeon/DungeonRoute.h>
#include <gwa3/dungeon/DungeonRouteRunner.h>
#include <gwa3/dungeon/DungeonRuntime.h>
#include <gwa3/dungeon/DungeonSkill.h>
#include <gwa3/dungeon/DungeonVendor.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/game/Quest.h>

#include <Windows.h>
#include <cmath>
#include <cstdlib>
#include <cstdarg>
#include <cstddef>
#include <cstdio>

namespace GWA3::Bot::Froggy {

using DungeonDiagnostics::CollectNearbyNpcCandidates;
using DungeonDiagnostics::LogAgentIdentity;
using DungeonDiagnostics::LogNearbyNpcCandidates;
using DungeonDiagnostics::LogNearbySignposts;
using DungeonDiagnostics::NearbyNpcCandidate;

using Waypoint = DungeonRoute::Waypoint;
using CachedSkill = DungeonSkill::CachedSkill;
using DungeonSkill::CanUseSkill;
using DungeonSkill::ExplainCanUseSkillFailure;
using DungeonSkill::ResolveSkillTarget;

// ===== Run Statistics =====
static uint32_t s_runCount = 0;
static uint32_t s_failCount = 0;
static uint32_t s_wipeCount = 0;
static DWORD s_runStartTime = 0;
static DWORD s_totalStartTime = 0;
static DWORD s_bestRunTime = 0xFFFFFFFF;
static DungeonEntryRecovery::EntryFailureTracker s_tekksQuestEntryFailures = {
    0,
    TEKKS_DIALOG_RESET_FAILURE_THRESHOLD,
};
static DungeonCombatRoutine::CombatSessionState s_combatSession = {};
static SparkflyTraversalCombatStats s_sparkflyTraversalCombatStats = {};
static DungeonLoopTelemetry s_dungeonLoopTelemetry = {};

using DungeonRuntime::IsDead;
using DungeonRuntime::IsMapLoaded;
using DungeonRuntime::WaitMs;
using DungeonNavigation::WaitForLocalPositionSettle;

// ===== Forward declarations =====
static int PickupNearbyLoot(float maxRange = DEFAULT_LOOT_PICKUP_RADIUS);
static bool AcquireBogrootBossKey();
static void UseDpRemovalIfNeeded();
static bool OpenChestAt(float chestX, float chestY, float searchRadius = DEFAULT_CHEST_OPEN_RADIUS);
static void FollowWaypoints(const Waypoint* wps, int count, bool ignoreBotRunning);

#include "FroggyHMMovementCombatRuntime.h"
#include "FroggyHMAggroCombatRuntime.h"
#include "FroggyHMTransitionRuntime.h"

static DungeonCombat::LocalClearPolicy BuildFroggyLocalClearPolicy(
    const char* label,
    float fightRange,
    SparkflyTraversalCombatStats* stats) {
    const auto profile = IsBogrootMapId(MapMgr::GetMapId())
        ? DungeonCombat::LocalClearProfile::ShortTraversal
        : DungeonCombat::LocalClearProfile::StandardTraversal;
    return DungeonCombat::BuildLocalClearPolicy(profile, label, fightRange, stats != nullptr);
}

static void RecordFroggyLocalClearPass(void* userData, int, uint32_t targetId) {
    auto* stats = static_cast<SparkflyTraversalCombatStats*>(userData);
    if (!stats) return;
    ++stats->settle_requests;
    stats->last_target_id = targetId;
}

static void FroggyLocalClearPostLoot(void*, float aggroRange, const char* reason) {
    LootAfterCombatSweep(aggroRange, reason);
}

static void FroggyLocalClearFightInAggro(
    float aggroRange,
    bool careful,
    void* stats,
    bool waitForSkillCompletion,
    uint32_t maxFightMs) {
    FightEnemiesInAggro(
        aggroRange,
        careful,
        static_cast<SparkflyTraversalCombatStats*>(stats),
        waitForSkillCompletion,
        maxFightMs);
}

static DungeonCombat::HoldLocalClearCallbacks MakeFroggyLocalClearCallbacks(
    SparkflyTraversalCombatStats* stats) {
    DungeonCombat::HoldLocalClearCallbacks callbacks = {};
    callbacks.is_dead = &IsDead;
    callbacks.is_map_loaded = &IsMapLoaded;
    callbacks.wait_ms = &DungeonRuntime::WaitMs;
    callbacks.fight_in_aggro = &FroggyLocalClearFightInAggro;
    callbacks.post_loot = &FroggyLocalClearPostLoot;
    callbacks.on_clear_pass = &RecordFroggyLocalClearPass;
    callbacks.user_data = stats;
    return callbacks;
}

static void HoldForLocalClear(const char* label,
                              float waypointX,
                              float waypointY,
                              float fightRange,
                              uint32_t bestId,
                              SparkflyTraversalCombatStats* stats) {
    const auto policy = BuildFroggyLocalClearPolicy(label, fightRange, stats);
    DungeonCombat::HoldLocalClearOptions options = {};
    options.target_id = bestId;
    options.log_prefix = "Froggy";
    (void)DungeonCombat::HoldForLocalClear(
        waypointX,
        waypointY,
        fightRange,
        policy,
        MakeFroggyLocalClearCallbacks(stats),
        options);
}

static void HoldSparkflyForLocalClear(float waypointX,
                                      float waypointY,
                                      float fightRange,
                                      uint32_t bestId,
                                      SparkflyTraversalCombatStats* stats) {
    HoldForLocalClear("Sparkfly", waypointX, waypointY, fightRange, bestId, stats);
}

#include "FroggyHMAggroMoveRuntime.h"

#include "FroggyHMTransitions.h"

static void GrabDungeonBlessing(float shrineX, float shrineY); // forward decl
static bool OpenDungeonDoorAt(float doorX, float doorY);       // forward decl
#include "FroggyHMBossWaypointHandler.h"
#include "FroggyHMWaypointSupport.h"
#include "FroggyHMCheckpointReplay.h"
#include "FroggyHMLvl2TransitionWaypoint.h"

static bool RecoverFromStandardWaypointWipe(const Waypoint* wps, int count, int currentIndex, int& outRestartIndex) {
    return RecoverFromWaypointWipe(wps, count, currentIndex, WipeRecoveryContext::Standard, outRestartIndex);
}

static bool RecoverFroggyLabelWaypointWipe(
    const Waypoint* wps,
    int count,
    int currentIndex,
    DungeonCheckpoint::WaypointWipeRecoveryContext context,
    int& outRestartIndex) {
    return RecoverFromWaypointWipe(wps, count, currentIndex, context, outRestartIndex);
}

static void UpdateFroggyQuestMapReturnTelemetry(uint32_t finalMapId, bool returnedToQuestMap) {
    s_dungeonLoopTelemetry.final_map_id = finalMapId;
    s_dungeonLoopTelemetry.returned_to_sparkfly =
        returnedToQuestMap && finalMapId == MapIds::SPARKFLY_SWAMP;
}

static void HandleFroggyLevelTransitionWaypoint(const Waypoint&) {
    HandleFroggyLvl1ToLvl2Waypoint();
}

static void HandleFroggyBossRewardWaypoint(const Waypoint& waypoint) {
    HandleBossWaypoint(waypoint);
}

static DungeonRouteRunner::RouteLabelExecutorOptions MakeFroggyRouteLabelOptions() {
    DungeonRouteRunner::RouteLabelExecutorOptions options;
    options.move_route_waypoint = &MoveFroggyRouteWaypointDefault;
    options.move_key_waypoint = &MoveFroggyRouteWaypointDefault;
    options.move_checkpoint_waypoint = &MoveFroggyWaypointForCheckpointReplay;
    options.aggro_move_to = &AggroMoveToEx;
    options.open_dungeon_door_at = &OpenDungeonDoorAt;
    options.grab_blessing = &GrabDungeonBlessing;
    options.acquire_dungeon_key = &AcquireBogrootBossKey;
    options.handle_level_transition = &HandleFroggyLevelTransitionWaypoint;
    options.handle_boss_reward = &HandleFroggyBossRewardWaypoint;
    options.recover_wipe = &RecoverFroggyLabelWaypointWipe;
    options.is_dead = &IsDead;
    options.return_to_outpost = &MapMgr::ReturnToOutpost;
    options.wait_for_map_ready = &DungeonRuntime::WaitForMapReady;
    options.get_nearest_waypoint = &DungeonNavigation::GetNearestWaypointIndex;
    options.log_waypoint_state = &LogFroggyWaypointState;
    options.after_quest_door_backtrack_waypoint = &LogQuestDoorCheckpointReplayWaypoint;
    options.quest_door_diagnostic = &LogQuestDoorCheckpointDiagnostic;
    options.update_return_to_quest_map = &UpdateFroggyQuestMapReturnTelemetry;
    options.return_to_quest_map = MakeFroggyReturnToSparkflyPlan("Froggy quest-door refresh");
    options.quest_id = GWA3::QuestIds::TEKKS_WAR;
    options.quest_ready.refresh_delay_ms = TEKKS_QUEST_REFRESH_DELAY_MS;
    options.quest_ready.post_set_active_delay_ms = TEKKS_SET_ACTIVE_DWELL_MS;
    options.recovery_outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    options.log_prefix = "Froggy";
    return options;
}

static DungeonRouteRunner::WaypointHandlerResult HandleFroggySpecialWaypointForRouteRunner(
    const Waypoint* wps,
    int count,
    int& waypointIndex) {
    return DungeonRouteRunner::ExecuteRouteLabelWaypoint(
        wps,
        count,
        waypointIndex,
        MakeFroggyRouteLabelOptions());
}

static void UpdateFroggyRouteTelemetry(int waypointIndex, const Waypoint& waypoint) {
    s_dungeonLoopTelemetry.last_waypoint_index = static_cast<uint32_t>(waypointIndex);
    s_dungeonLoopTelemetry.waypoint_iterations++;
    strncpy_s(s_dungeonLoopTelemetry.last_waypoint_label,
              waypoint.label ? waypoint.label : "",
              _TRUNCATE);
}

static void LogFroggyRouteWaypoint(int waypointIndex, const Waypoint& waypoint) {
    LogBot("Moving to waypoint %d: %s (%.0f, %.0f)",
           waypointIndex,
           waypoint.label ? waypoint.label : "",
           waypoint.x,
           waypoint.y);
}

static void FollowWaypoints(const Waypoint* wps, int count, bool ignoreBotRunning = false) {
    DungeonRouteRunner::RouteRunCallbacks callbacks;
    callbacks.get_map_id = &MapMgr::GetMapId;
    callbacks.is_bot_running = &Bot::IsRunning;
    callbacks.is_dead = &IsDead;
    callbacks.resolve_start_index = &GetFroggyRouteStartIndex;
    callbacks.is_route_map = &IsBogrootRouteMap;
    callbacks.log_waypoint_state = &LogFroggyWaypointState;
    callbacks.recover_wipe = &RecoverFromStandardWaypointWipe;
    callbacks.handle_special_waypoint = &HandleFroggySpecialWaypointForRouteRunner;
    callbacks.move_standard_waypoint = &MoveFroggyRouteWaypointWithCombatLoot;
    callbacks.update_telemetry = &UpdateFroggyRouteTelemetry;
    callbacks.log_waypoint = &LogFroggyRouteWaypoint;

    DungeonRouteRunner::RouteRunOptions options;
    options.ignore_bot_running = ignoreBotRunning;
    options.log_prefix = "Froggy";
    options.route_name = "Bogroot";
    (void)DungeonRouteRunner::RunWaypointRoute(wps, count, callbacks, options);
}

#include "FroggyHMTekksEntry.h"

#include "FroggyHMLootAndDoors.h"

#include "FroggyHMBlessing.h"

static DungeonVendor::MaintenanceLocation MakeFroggyMaintenanceLocation() {
    DungeonVendor::MaintenanceLocation location = {};
    location.outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    location.merchant_x = GADDS_MERCHANT.x;
    location.merchant_y = GADDS_MERCHANT.y;
    location.merchant_move_threshold = 350.0f;
    location.merchant_search_radius = 2500.0f;
    location.merchant_player_number = GADDS_MERCHANT_PLAYER_NUMBER;
    location.xunlai_chest_x = GADDS_XUNLAI.x;
    location.xunlai_chest_y = GADDS_XUNLAI.y;
    location.material_trader_x = GADDS_MATERIAL_TRADER.x;
    location.material_trader_y = GADDS_MATERIAL_TRADER.y;
    location.material_trader_player_number = GADDS_MATERIAL_TRADER_PLAYER_NUMBER;
    return location;
}

static MaintenanceMgr::Config MakeFroggyMaintenanceConfig(uint32_t outpostMapId) {
    return DungeonVendor::BuildMaintenanceConfig(outpostMapId, MakeFroggyMaintenanceLocation());
}

static DungeonVendor::MaintenanceStateOptions MakeFroggyMaintenanceStateOptions() {
    DungeonVendor::MaintenanceStateOptions options = {};
    options.fallback_outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    options.log_prefix = "Froggy";
    options.move_to_point = &MoveToAndWait;
    options.wait_ms = &DungeonRuntime::WaitMs;
    return options;
}

static void UseConsumables(const BotConfig& cfg) {
    (void)DungeonItemActions::UseConsetsForCurrentPlayerIfEnabled(
        cfg.use_consets,
        &DungeonRuntime::WaitMs,
        {},
        "Froggy");
}

static void UseDpRemovalIfNeeded() {
    DungeonItemActions::UseItemOptions options;
    options.delay_ms = 5000u;
    const auto result =
        DungeonItemActions::UseDpRemovalSweetIfNeeded(&s_wipeCount, &DungeonRuntime::WaitMs, options);
    if (result.used_model_id != 0u) {
        Log::Info("Froggy: Using DP removal sweet (model=%u) after %u wipes",
                  result.used_model_id,
                  result.previous_wipe_count);
    }
}

#include "FroggyHMDungeonLoopRuntime.h"

#include "FroggyHMTownStates.h"
#include "FroggyHMDungeonState.h"
#include "FroggyHMErrorState.h"

BotState HandleLoot(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Loot collection");
    WaitMs(2000);
    return BotState::Merchant;
}

BotState HandleMerchant(BotConfig& cfg) {
    const auto result = DungeonVendor::RunMerchantMaintenanceState(
        cfg.outpost_map_id,
        MakeFroggyMaintenanceLocation(),
        MakeFroggyMaintenanceStateOptions());
    switch (result) {
    case DungeonVendor::MaintenanceStateResult::Done:
        return BotState::InTown;
    case DungeonVendor::MaintenanceStateResult::NeedsMaintenance:
        return BotState::Maintenance;
    case DungeonVendor::MaintenanceStateResult::Retry:
        return BotState::Merchant;
    case DungeonVendor::MaintenanceStateResult::Stop:
    default:
        return BotState::Stopping;
    }
}

BotState HandleMaintenance(BotConfig& cfg) {
    const auto result = DungeonVendor::RunFullMaintenanceState(
        cfg.outpost_map_id,
        &s_wipeCount,
        MakeFroggyMaintenanceLocation(),
        MakeFroggyMaintenanceStateOptions());
    switch (result) {
    case DungeonVendor::MaintenanceStateResult::Done:
        return BotState::Traveling;
    case DungeonVendor::MaintenanceStateResult::Retry:
        return BotState::Maintenance;
    case DungeonVendor::MaintenanceStateResult::NeedsMaintenance:
    case DungeonVendor::MaintenanceStateResult::Stop:
    default:
        return BotState::Stopping;
    }
}

// ===== Registration =====

void Register() {
    Bot::RegisterStateHandler(BotState::CharSelect, HandleCharSelect);
    Bot::RegisterStateHandler(BotState::InTown, HandleTownSetup);
    Bot::RegisterStateHandler(BotState::Traveling, HandleTravel);
    Bot::RegisterStateHandler(BotState::InDungeon, HandleDungeon);
    Bot::RegisterStateHandler(BotState::Looting, HandleLoot);
    Bot::RegisterStateHandler(BotState::Merchant, HandleMerchant);
    Bot::RegisterStateHandler(BotState::Maintenance, HandleMaintenance);
    Bot::RegisterStateHandler(BotState::Error, HandleError);

    // Default route config. Hero templates are resolved from config/gwa3.ini.
    auto& cfg = Bot::GetConfig();
    cfg.hero_config_file.clear();
    for (uint32_t& hero_id : cfg.hero_ids) {
        hero_id = 0u;
    }
    cfg.hard_mode = true;
    cfg.target_map_id = MapIds::BOGROOT_GROWTHS_LVL1;
    cfg.outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    cfg.bot_module_name = "FroggyHM";

    s_runCount = 0;
    s_failCount = 0;
    s_wipeCount = 0;
    s_totalStartTime = GetTickCount();

    LogBot("Froggy HM module registered (hard mode)");
}

#include "FroggyHMDebugMovement.h"
#include "FroggyHMDebugCombat.h"
#include "FroggyHMDebugCombatDump.h"
#include "FroggyHMDebugDungeonLoop.h"

} // namespace GWA3::Bot::Froggy
