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

static int LootAfterCombatSweep(float aggroRange, const char* reason) {
    DungeonLoot::PostCombatLootSweepOptions options = {};
    options.log_prefix = "Froggy";
    options.reason = reason;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.is_world_ready = &DungeonLoot::IsWorldReadyForLootWithPlayerAgent;
    options.pickup_nearby_loot = &PickupNearbyLoot;
    return DungeonLoot::SweepPostCombatLoot(aggroRange, options);
}

static void RecordFroggyAggroTargetStats(void* userData, uint32_t bestTarget) {
    auto* stats = static_cast<SparkflyTraversalCombatStats*>(userData);
    if (!stats) {
        return;
    }
    ++stats->quick_step_attempts;
    stats->last_target_id = bestTarget;
}

static void RecordFroggyAggroActionStats(
    void* userData,
    const DungeonCombatRoutine::SkillActionResult& action,
    uint32_t actionStart) {
    auto* stats = static_cast<SparkflyTraversalCombatStats*>(userData);
    if (!stats || !action.valid || action.started_at_ms < actionStart) {
        return;
    }
    if (action.used_skill) {
        ++stats->skill_steps;
    } else if (action.auto_attack) {
        ++stats->auto_attack_steps;
    }
}

static float ResolveFroggyAggroMaxAftercast(uint32_t mapId, void*) {
    return IsBogrootMapId(mapId) ? 1.5f : 3.0f;
}

static void FroggyAggroPostLoot(void* userData, float aggroRange, const char* reason) {
    LootAfterCombatSweep(aggroRange, userData ? "combat-step-stats" : (reason ? reason : "combat-step"));
}

static void FightEnemiesInAggro(float aggroRange, bool careful = false,
                                SparkflyTraversalCombatStats* stats = nullptr,
                                bool waitForSkillCompletion = true,
                                DWORD maxFightMs = 240000u) {
    DungeonCombat::SessionAggroFightProfile profile;
    profile.session = &s_combatSession;
    profile.wait_ms = &DungeonRuntime::WaitMs;
    profile.is_dead = &IsDead;
    profile.post_loot = &FroggyAggroPostLoot;
    profile.on_target = &RecordFroggyAggroTargetStats;
    profile.on_action = &RecordFroggyAggroActionStats;
    profile.resolve_max_aftercast = &ResolveFroggyAggroMaxAftercast;
    profile.user_data = stats;
    profile.log_prefix = "Froggy";

    DungeonCombat::AggroFightOptions options;
    options.careful = careful;
    options.wait_for_skill_completion = waitForSkillCompletion;
    options.max_fight_ms = maxFightMs;
    options.log_prefix = "Froggy";
    options.loot_reason = stats ? "combat-step-stats" : "combat-step";
    (void)DungeonCombat::FightEnemiesInAggroWithSession(aggroRange, profile, options);
}

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

static bool MoveToAndWait(float x, float y, float threshold = DungeonNavigation::MOVE_TO_DEFAULT_THRESHOLD) {
    DungeonNavigation::LoggedMoveOptions options;
    options.log_prefix = "Froggy";
    options.is_dead = &IsDead;
    return DungeonNavigation::MoveToAndWaitLogged(x, y, threshold, options);
}

static void FroggyFightEnemiesInAggroForMove(
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

static void FroggyHoldSpecialLocalClear(
    float x,
    float y,
    float fightRange,
    uint32_t targetId,
    void* stats) {
    HoldSparkflyForLocalClear(
        x,
        y,
        fightRange,
        targetId,
        static_cast<SparkflyTraversalCombatStats*>(stats));
}

static void FroggyHoldLocalClear(
    const char* label,
    float x,
    float y,
    float fightRange,
    uint32_t targetId,
    void* stats) {
    HoldForLocalClear(
        label,
        x,
        y,
        fightRange,
        targetId,
        static_cast<SparkflyTraversalCombatStats*>(stats));
}

static void AggroMoveToEx(float x, float y, float fightRange = DungeonCombat::AGGRO_DEFAULT_FIGHT_RANGE) {
    const bool sparkflyMap = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    const bool bogrootMap = IsBogrootMapId(MapMgr::GetMapId());

    DungeonNavigation::AggroMoveProfileConfig config;
    config.is_dead = &IsDead;
    config.is_map_loaded = &IsMapLoaded;
    config.wait_ms = &DungeonRuntime::WaitMs;
    config.fight_in_aggro = &FroggyFightEnemiesInAggroForMove;
    config.hold_local_clear = &FroggyHoldLocalClear;
    config.hold_special_local_clear = &FroggyHoldSpecialLocalClear;
    config.pickup_nearby_loot = &PickupNearbyLoot;
    config.special_stats = &s_sparkflyTraversalCombatStats;
    config.profile = bogrootMap
        ? DungeonNavigation::AggroMoveProfile::Opportunistic
        : DungeonNavigation::AggroMoveProfile::Standard;
    config.exact_move_target = sparkflyMap;
    config.use_special_local_clear = sparkflyMap;
    config.use_local_clear_cooldown = false;
    config.log_prefix = "Froggy";
    config.sidestep_random_radius = AGGRO_BOGROOT_SIDESTEP_RANDOM_RADIUS;
    config.opportunistic_fight_budget_ms = AGGRO_BOGROOT_FIGHT_BUDGET_MS;
    config.opportunistic_loot_radius = AGGRO_BOGROOT_LOOT_RADIUS;
    DungeonNavigation::AggroMoveToConfigured(x, y, fightRange, config);
}

#include "FroggyHMTransitions.h"

static void GrabDungeonBlessing(float shrineX, float shrineY); // forward decl
static bool OpenDungeonDoorAt(float doorX, float doorY);       // forward decl
#include "FroggyHMBossWaypointHandler.h"

static bool IsFroggyRouteMap(uint32_t mapId) {
    return mapId == MapIds::SPARKFLY_SWAMP || IsBogrootMapId(mapId);
}

static int GetFroggyRouteStartIndex(const Waypoint* wps, int count, uint32_t mapId) {
    const int nearestIdx = DungeonNavigation::GetNearestWaypointIndex(wps, count);
    float distanceFromLvl1Portal = -1.0f;
    if (mapId == MapIds::BOGROOT_GROWTHS_LVL2) {
        if (auto* me = AgentMgr::GetMyAgent()) {
            distanceFromLvl1Portal = AgentMgr::GetDistance(me->x, me->y,
                                                           BOGROOT_LVL1_TO_LVL2_PORTAL.x,
                                                           BOGROOT_LVL1_TO_LVL2_PORTAL.y);
        }
    }

    const int startIdx = ResolveRouteStartIndex(mapId, wps, count, nearestIdx, distanceFromLvl1Portal);
    if (mapId == MapIds::BOGROOT_GROWTHS_LVL1 && nearestIdx == 1 && startIdx == 0) {
        Log::Info("Froggy: Bogroot lvl1 forcing startIdx from blessing back to opening aggro leg");
    }
    if (mapId == MapIds::BOGROOT_GROWTHS_LVL2 && nearestIdx >= 20 && startIdx == 0) {
        Log::Info("Froggy: Bogroot lvl2 suppressing stale startIdx=%d while spawn handoff is unresolved (distFromLvl1Portal=%.0f)",
                  nearestIdx,
                  distanceFromLvl1Portal);
    }
    return startIdx;
}

static void LogFroggyWaypointState(const char* stage, const Waypoint* wps, int count, int waypointIndex) {
    DungeonNavigation::WaypointTelemetryOptions options;
    options.log_prefix = "Froggy";
    options.route_name = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP ? "Sparkfly" : "Bogroot";
    options.nearest_enemy_range = TELEMETRY_NEAREST_ENEMY_RANGE;
    options.nearby_enemy_range = TELEMETRY_NEARBY_ENEMY_RANGE;
    DungeonNavigation::LogWaypointState(stage, wps, count, waypointIndex, options);
}

static void MoveFroggyWaypoint(float x, float y, float threshold) {
    (void)MoveToAndWait(x, y, threshold);
}

static void AggroMoveFroggyWaypoint(float x, float y, float fightRange) {
    AggroMoveToEx(x, y, fightRange);
}

static DungeonNavigation::WaypointMoveResult MoveFroggyRouteWaypoint(
    const Waypoint& waypoint,
    float moveThreshold = 250.0f) {
    return DungeonNavigation::MoveRouteWaypoint(
        waypoint,
        &MoveFroggyWaypoint,
        &AggroMoveFroggyWaypoint,
        &IsMapLoaded,
        moveThreshold);
}

static DungeonNavigation::WaypointMoveResult MoveFroggyRouteWaypointDefault(const Waypoint& waypoint) {
    return MoveFroggyRouteWaypoint(waypoint);
}

static bool MoveFroggyWaypointForCheckpointReplay(const Waypoint& waypoint) {
    MoveFroggyRouteWaypoint(waypoint);
    return true;
}

static void MarkFroggyLevelTransitionStarted() {
    s_dungeonLoopTelemetry.lvl1_to_lvl2_started = true;
}

static void RecordFroggyLevelTransitionAttempt(uint32_t attempt) {
    s_dungeonLoopTelemetry.lvl1_to_lvl2_attempts = attempt;
}

static void MarkFroggyLevelTransitionEntered(uint32_t) {
    s_dungeonLoopTelemetry.entered_lvl2 = true;
}

static void RecordFroggyLevelTransitionTelemetry(const DungeonRuntime::TransitionTelemetry& telemetry) {
    s_dungeonLoopTelemetry.map_loaded = telemetry.map_loaded;
    s_dungeonLoopTelemetry.player_alive = telemetry.player_alive;
    s_dungeonLoopTelemetry.player_hp = telemetry.player_hp;
    s_dungeonLoopTelemetry.player_x = telemetry.player_x;
    s_dungeonLoopTelemetry.player_y = telemetry.player_y;
    s_dungeonLoopTelemetry.target_id = telemetry.target_id;
    s_dungeonLoopTelemetry.dist_to_exit = telemetry.dist_to_exit;
    s_dungeonLoopTelemetry.nearest_enemy_dist = telemetry.nearest_enemy_dist;
    s_dungeonLoopTelemetry.nearby_enemy_count = telemetry.nearby_enemy_count;
    s_dungeonLoopTelemetry.lvl1_portal_id = telemetry.portal_id;
    s_dungeonLoopTelemetry.lvl1_portal_x = telemetry.portal_x;
    s_dungeonLoopTelemetry.lvl1_portal_y = telemetry.portal_y;
    s_dungeonLoopTelemetry.lvl1_portal_dist = telemetry.portal_dist;
}

static DungeonCheckpoint::WaypointWipeRecoveryOptions MakeFroggyWipeRecoveryOptions() {
    DungeonCheckpoint::WaypointWipeRecoveryOptions options;
    options.backtrack_steps = 2;
    options.wipe_count = &s_wipeCount;
    options.is_dead = &IsDead;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.return_to_outpost = &MapMgr::ReturnToOutpost;
    options.use_dp_removal = &UseDpRemovalIfNeeded;
    options.get_nearest_waypoint = &DungeonNavigation::GetNearestWaypointIndex;
    return options;
}

static void UpdateFroggyQuestMapReturnTelemetry(uint32_t finalMapId, bool returnedToQuestMap) {
    s_dungeonLoopTelemetry.final_map_id = finalMapId;
    s_dungeonLoopTelemetry.returned_to_sparkfly =
        returnedToQuestMap && finalMapId == MapIds::SPARKFLY_SWAMP;
}

static void HandleFroggyLevelTransitionWaypoint(const Waypoint&) {
    DungeonRuntime::LevelTransitionOptions options;
    options.log_prefix = "Froggy";
    options.transition_name = "Lvl1 to Lvl2";
    options.exit_anchor = {BOGROOT_LVL1_TO_LVL2_PORTAL.x, BOGROOT_LVL1_TO_LVL2_PORTAL.y};
    options.target_map_id = MapIds::BOGROOT_GROWTHS_LVL2;
    options.portal_search_radius = BOGROOT_LVL1_TO_LVL2_PORTAL_SEARCH_RADIUS;
    options.timeout_ms = BOGROOT_LVL1_TO_LVL2_TIMEOUT_MS;
    options.log_interval_ms = BOGROOT_LVL1_TO_LVL2_LOG_INTERVAL_MS;
    options.move_poll_ms = BOGROOT_LVL1_TO_LVL2_MOVE_POLL_MS;
    options.spawn_ready_timeout_ms = BOGROOT_LVL2_SPAWN_READY_TIMEOUT_MS;
    options.spawn_stale_anchor = options.exit_anchor;
    options.spawn_stale_anchor_clearance = BOGROOT_LVL1_TO_LVL2_HANDOFF_CLEARANCE;
    options.spawn_settle_timeout_ms = BOGROOT_LVL2_SPAWN_SETTLE_TIMEOUT_MS;
    options.spawn_settle_distance = BOGROOT_LVL2_SPAWN_SETTLE_DISTANCE;
    options.nearest_enemy_range = TELEMETRY_NEAREST_ENEMY_RANGE;
    options.nearby_enemy_range = TELEMETRY_NEARBY_ENEMY_RANGE;
    options.queue_move = &AgentMgr::Move;
    options.get_map_id = &MapMgr::GetMapId;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.find_portal = &DungeonInteractions::FindNearestSignpost;
    options.on_started = &MarkFroggyLevelTransitionStarted;
    options.on_attempt = &RecordFroggyLevelTransitionAttempt;
    options.on_entered = &MarkFroggyLevelTransitionEntered;
    options.on_telemetry = &RecordFroggyLevelTransitionTelemetry;

    const auto result = DungeonRuntime::ExecuteLevelTransition(options);
    if (result.entered_target_map) {
        auto* me = AgentMgr::GetMyAgent();
        Log::Info("Froggy: Lvl1 to Lvl2 nearestLvl2Wp=%d player=(%.0f, %.0f)",
                  DungeonNavigation::GetNearestWaypointIndex(BOGROOT_LVL2, BOGROOT_LVL2_COUNT),
                  me ? me->x : 0.0f,
                  me ? me->y : 0.0f);
    }
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
    options.wipe_recovery = MakeFroggyWipeRecoveryOptions();
    options.is_dead = &IsDead;
    options.return_to_outpost = &MapMgr::ReturnToOutpost;
    options.wait_for_map_ready = &DungeonRuntime::WaitForMapReady;
    options.get_nearest_waypoint = &DungeonNavigation::GetNearestWaypointIndex;
    options.log_waypoint_state = &LogFroggyWaypointState;
    options.update_return_to_quest_map = &UpdateFroggyQuestMapReturnTelemetry;
    options.return_to_quest_map = MakeFroggyReturnToSparkflyPlan("Froggy quest-door refresh");
    options.quest_id = GWA3::QuestIds::TEKKS_WAR;
    options.quest_ready.refresh_delay_ms = TEKKS_QUEST_REFRESH_DELAY_MS;
    options.quest_ready.post_set_active_delay_ms = TEKKS_SET_ACTIVE_DWELL_MS;
    options.recovery_outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    options.log_prefix = "Froggy";
    return options;
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
    callbacks.is_route_map = &IsFroggyRouteMap;
    callbacks.log_waypoint_state = &LogFroggyWaypointState;
    callbacks.update_telemetry = &UpdateFroggyRouteTelemetry;
    callbacks.log_waypoint = &LogFroggyRouteWaypoint;

    DungeonRouteRunner::RouteRunOptions options;
    options.ignore_bot_running = ignoreBotRunning;
    options.execute_route_label_waypoints = true;
    options.route_label_options = MakeFroggyRouteLabelOptions();
    options.wipe_recovery = MakeFroggyWipeRecoveryOptions();
    options.log_prefix = "Froggy";
    options.route_name = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP ? "Sparkfly" : "Bogroot";
    options.standard_waypoint_movement.move_to_point = &MoveFroggyWaypoint;
    options.standard_waypoint_movement.aggro_move_to_point = &AggroMoveFroggyWaypoint;
    options.standard_waypoint_movement.is_map_loaded = &IsMapLoaded;
    options.standard_waypoint_movement.loot_after_combat = &LootAfterCombatSweep;
    options.standard_waypoint_movement.log_prefix = "Froggy";
    (void)DungeonRouteRunner::RunWaypointRoute(wps, count, callbacks, options);
}

#include "FroggyHMTekksEntry.h"

static DungeonLoot::BossKeyModelSet MakeBogrootBossKeyModelSet() {
    DungeonLoot::BossKeyModelSet modelSet;
    modelSet.model_ids = BOGROOT_BOSS_KEY_MODELS;
    modelSet.model_count = BOGROOT_BOSS_KEY_MODEL_COUNT;
    modelSet.accept_type_key = true;
    return modelSet;
}

static int PickupNearbyLoot(float maxRange) {
    return DungeonLoot::PickUpNearbyLoot(
        maxRange,
        &DungeonRuntime::WaitMs,
        &IsDead,
        DungeonLoot::MakeLootPickupOptions("Froggy", &DungeonLoot::IsWorldReadyForLoot));
}

static void LogNearbyBogrootBossKeyCandidates(const char* label, float x, float y, float maxRange) {
    DungeonLoot::LogNearbyBossKeyCandidates(
        label,
        x,
        y,
        maxRange,
        "Froggy",
        {},
        nullptr,
        MakeBogrootBossKeyModelSet());
}

static void MoveToBogrootBossKeyWithAggro(float x, float y, float fightRange) {
    AggroMoveToEx(x, y, fightRange);
}

static bool AcquireBogrootBossKey() {
    DungeonLoot::BossKeyAcquireOptions options;
    options.key_x = BOGROOT_BOSS_KEY_X;
    options.key_y = BOGROOT_BOSS_KEY_Y;
    options.key_scan_range = BOGROOT_BOSS_KEY_SCAN_RANGE;
    options.log_prefix = "Froggy";
    options.combat_move_to = &MoveToBogrootBossKeyWithAggro;
    options.pickup_nearby_loot = &PickupNearbyLoot;
    options.move_to_point = &DungeonNavigation::MoveToPoint;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.is_dead = &IsDead;
    options.boss_key_models = MakeBogrootBossKeyModelSet();
    options.loot = DungeonLoot::MakeLootPickupOptions("Froggy", &DungeonLoot::IsWorldReadyForLoot);
    options.force_pickup.log_prefix = "Froggy";
    return DungeonLoot::AcquireBossKey(options);
}

static DungeonInteractions::OpenedChestTracker s_openedChestTracker;

static void ResetOpenedChestTracker(const char* reason) {
    DungeonInteractions::ResetOpenedChestTrackerForCurrentMap(s_openedChestTracker, reason, "Froggy");
}

static DungeonLoot::ChestBundleFallbackOptions MakeFroggyChestBundleFallbackOptions() {
    DungeonLoot::ChestBundleFallbackOptions options;
    options.min_signpost_radius = CHEST_BUNDLE_MIN_SIGNPOST_RADIUS;
    options.min_loot_radius = CHEST_BUNDLE_MIN_LOOT_RADIUS;
    options.loot_radius_multiplier = CHEST_BUNDLE_LOOT_RADIUS_MULTIPLIER;
    options.open_attempts = CHEST_BUNDLE_OPEN_ATTEMPTS;
    options.pickup_attempts = CHEST_BUNDLE_PICKUP_ATTEMPTS;
    options.open_retry_delay_ms = CHEST_BUNDLE_OPEN_RETRY_DELAY_MS;
    options.pickup_retry_delay_ms = CHEST_BUNDLE_PICKUP_RETRY_DELAY_MS;
    options.verify_delay_ms = CHEST_BUNDLE_VERIFY_DELAY_MS;
    options.log_prefix = "Froggy";
    return options;
}

static bool OpenChestAt(float chestX, float chestY, float searchRadius) {
    const auto options = DungeonLoot::MakeChestAtOpenOptions(
        "Froggy",
        true,
        MakeFroggyChestBundleFallbackOptions(),
        &LogNearbySignposts,
        &DungeonLoot::IsWorldReadyForLoot);
    return DungeonLoot::OpenChestAt(
        chestX,
        chestY,
        searchRadius,
        s_openedChestTracker,
        &DungeonNavigation::MoveToPoint,
        &DungeonRuntime::WaitMs,
        &IsDead,
        options);
}

static bool OpenDungeonDoorAt(float doorX, float doorY) {
    const auto options = DungeonInteractions::MakeDoorOpenOptions(
        "Froggy",
        &LogNearbySignposts,
        &LogAgentIdentity,
        &LogNearbyBogrootBossKeyCandidates);
    return DungeonInteractions::OpenDoorAtWithProbe(
        doorX,
        doorY,
        BOGROOT_BOSS_KEY_DOOR_PROBE_X,
        BOGROOT_BOSS_KEY_DOOR_PROBE_Y,
        &MoveToAndWait,
        &DungeonRuntime::WaitMs,
        options);
}

static void GrabDungeonBlessing(float shrineX, float shrineY) {
    DungeonEffects::DungeonBlessingAcquireOptions options;
    options.primary_search_radius = BLESSING_PRIMARY_SEARCH_RADIUS;
    options.fallback_search_radius = BLESSING_FALLBACK_SEARCH_RADIUS;
    options.signpost_move_threshold = BLESSING_SIGNPOST_MOVE_THRESHOLD;
    options.npc_move_threshold = BLESSING_NPC_MOVE_THRESHOLD;
    options.required_title_id = BLESSING_TITLE_ID;
    options.title_settle_delay_ms = BLESSING_TITLE_SETTLE_MS;
    options.accept_dialog_id = BLESSING_ACCEPT_DIALOG_ID;
    options.settle_timeout_ms = BLESSING_SETTLE_TIMEOUT_MS;
    options.settle_distance = BLESSING_SETTLE_DISTANCE;
    options.log_prefix = "Froggy: Blessing";
    options.move_to_point = &MoveToAndWait;
    options.wait_ms = &DungeonRuntime::WaitMs;
    options.signpost_scan_log = &LogNearbySignposts;
    options.agent_log = &LogAgentIdentity;
    (void)DungeonEffects::AcquireDungeonBlessingAt(shrineX, shrineY, options);
}

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

void ResetDungeonLoopTelemetry() {
    s_dungeonLoopTelemetry = {};
    ResetOpenedChestTracker("dungeon-loop-start");
}

DungeonLoopTelemetry GetDungeonLoopTelemetry() {
    s_dungeonLoopTelemetry.final_map_id = MapMgr::GetMapId();
    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    return s_dungeonLoopTelemetry;
}

static bool PrepareTekksDungeonEntryForLoop(const char*) {
    return PrepareTekksDungeonEntry();
}

static bool EnterBogrootFromSparkflyForLoop(const char*) {
    return EnterBogrootFromSparkfly();
}

static bool RecordTekksQuestEntryFailureForLoop(const char* context) {
    return RecordTekksQuestEntryFailureAndMaybeResetDialog(context);
}

static bool ResetTekksQuestEntryFailuresForLoop(const char* context) {
    ResetTekksQuestEntryFailures(context);
    return true;
}

static bool HasReachedBogrootLvl2ForLoop() {
    return s_dungeonLoopTelemetry.entered_lvl2;
}

static bool HasCompletedBogrootObjectiveForLoop() {
    return s_dungeonLoopTelemetry.boss_completed;
}

static void MarkBogrootLevelStartedForLoop(int levelIndex) {
    if (levelIndex == 0) {
        s_dungeonLoopTelemetry.started_in_lvl1 = true;
    } else if (levelIndex == 1) {
        s_dungeonLoopTelemetry.started_in_lvl2 = true;
    }
}

static void MarkBogrootProgressLevelReachedForLoop(int levelIndex) {
    if (levelIndex >= 1) {
        s_dungeonLoopTelemetry.entered_lvl2 = true;
    }
}

static void LogBogrootLevelRouteResultForLoop(
    int levelIndex,
    const DungeonRouteRunner::RouteRunResult&) {
    if (levelIndex == 0) {
        Log::Info("Froggy: Bogroot loop after lvl1 map=%u lastWp=%u(%s) returnedToSparkfly=%d",
                  MapMgr::GetMapId(),
                  s_dungeonLoopTelemetry.last_waypoint_index,
                  s_dungeonLoopTelemetry.last_waypoint_label,
                  s_dungeonLoopTelemetry.returned_to_sparkfly ? 1 : 0);
    } else if (levelIndex == 1) {
        Log::Info("Froggy: Bogroot loop after lvl2 map=%u bossStarted=%d bossCompleted=%d",
                  MapMgr::GetMapId(),
                  s_dungeonLoopTelemetry.boss_started ? 1 : 0,
                  s_dungeonLoopTelemetry.boss_completed ? 1 : 0);
    }
}

static bool RunDungeonLoopFromCurrentMap() {
    DungeonRouteRunner::DungeonLoopLevel levels[2] = {};
    levels[0].map_id = MapIds::BOGROOT_GROWTHS_LVL1;
    levels[0].waypoints = BOGROOT_LVL1;
    levels[0].waypoint_count = BOGROOT_LVL1_COUNT;
    levels[0].name = "Bogroot Level 1";
    levels[1].map_id = MapIds::BOGROOT_GROWTHS_LVL2;
    levels[1].waypoints = BOGROOT_LVL2;
    levels[1].waypoint_count = BOGROOT_LVL2_COUNT;
    levels[1].name = "Bogroot Level 2";
    levels[1].spawn_stale_anchor = {BOGROOT_LVL1_TO_LVL2_PORTAL.x, BOGROOT_LVL1_TO_LVL2_PORTAL.y};
    levels[1].spawn_stale_anchor_clearance = BOGROOT_LVL1_TO_LVL2_HANDOFF_CLEARANCE;
    levels[1].spawn_ready_timeout_ms = 10000u;
    levels[1].spawn_ready_poll_ms = 200u;
    levels[1].spawn_settle_timeout_ms = 1500u;
    levels[1].spawn_settle_distance = 24.0f;

    DungeonRouteRunner::DungeonLoopCallbacks callbacks = {};
    callbacks.get_map_id = &MapMgr::GetMapId;
    callbacks.follow_waypoints = &FollowWaypoints;
    callbacks.is_loop_objective_completed = &HasCompletedBogrootObjectiveForLoop;
    callbacks.is_progress_level_reached = &HasReachedBogrootLvl2ForLoop;
    callbacks.reset_loop_state = &ResetDungeonLoopTelemetry;
    callbacks.prepare_entry = &PrepareTekksDungeonEntryForLoop;
    callbacks.enter_dungeon = &EnterBogrootFromSparkflyForLoop;
    callbacks.record_entry_failure = &RecordTekksQuestEntryFailureForLoop;
    callbacks.reset_entry_failures = &ResetTekksQuestEntryFailuresForLoop;
    callbacks.on_level_started = &MarkBogrootLevelStartedForLoop;
    callbacks.on_progress_level_reached = &MarkBogrootProgressLevelReachedForLoop;
    callbacks.after_level_route = &LogBogrootLevelRouteResultForLoop;

    DungeonRouteRunner::DungeonLoopOptions options = {};
    options.levels = levels;
    options.level_count = static_cast<int>(sizeof(levels) / sizeof(levels[0]));
    options.progress_level_index = 1;
    options.entry_map_id = MapIds::SPARKFLY_SWAMP;
    options.fallback_completion_map_id = MapIds::GADDS_ENCAMPMENT;
    options.max_entry_refresh_retries_before_progress = 3;
    options.ignore_bot_running_for_routes = true;
    options.log_prefix = "Froggy";
    options.loop_name = "Bogroot";
    options.entry_refresh_context = "bogroot-loop-refresh";

    const auto result = DungeonRouteRunner::RunDungeonLoop(callbacks, options);
    s_dungeonLoopTelemetry.final_map_id = result.final_map_id;
    s_dungeonLoopTelemetry.last_dialog_id = DialogMgr::GetLastDialogId();
    s_dungeonLoopTelemetry.returned_to_sparkfly =
        result.returned_to_entry_map &&
        s_dungeonLoopTelemetry.final_map_id == MapIds::SPARKFLY_SWAMP;
    return result.completed;
}

BotState HandleCharSelect(BotConfig& cfg) {
    return DungeonStates::HandleCharSelect(cfg);
}

BotState HandleTownSetup(BotConfig& cfg) {
    const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
    DungeonStates::TownSetupOptions options = {};
    options.default_outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    options.run_number = s_runCount + 1;
    options.log_prefix = "Froggy";
    options.maintenance = MakeFroggyMaintenanceConfig(outpostMapId);
    options.merchant_x = GADDS_MERCHANT.x;
    options.merchant_y = GADDS_MERCHANT.y;
    options.outpost.default_hero_config_file = "Standard.txt";
    options.move_to_point = [](float x, float y, float threshold) {
        return MoveToAndWait(x, y, threshold);
    };
    options.open_merchant = [](float x, float y, float searchRadius) {
        DungeonVendor::MerchantContextNearCoordsOptions vendorOptions;
        vendorOptions.preferred_player_number = GADDS_MERCHANT_PLAYER_NUMBER;
        vendorOptions.log_prefix = "Froggy";
        return DungeonVendor::OpenMerchantContextNearCoords(
            x,
            y,
            searchRadius,
            &MoveToAndWait,
            &DungeonRuntime::WaitMs,
            vendorOptions);
    };
    options.refresh_skill_cache = []() {
        (void)DungeonCombatRoutine::RefreshSkillCacheWithDebugLog(s_combatSession, "Froggy");
    };
    options.use_consumables = [](const BotConfig& botCfg) {
        UseConsumables(botCfg);
    };
    return DungeonStates::HandleTownSetup(cfg, options);
}

BotState HandleTravel(BotConfig& cfg) {
    DungeonStates::TravelStateOptions options = {};
    options.default_source_map_id = MapIds::GADDS_ENCAMPMENT;
    options.entry_map_id = MapIds::SPARKFLY_SWAMP;
    options.travel_path = GADDS_TO_SPARKFLY_PATH;
    options.travel_path_count = GADDS_TO_SPARKFLY_PATH_COUNT;
    options.zone_point = GADDS_TO_SPARKFLY_ZONE;
    options.zone_timeout_ms = 10000u;
    options.log_prefix = "Froggy";
    options.label = "Travel to Sparkfly Swamp";
    return DungeonStates::HandleTravelToEntryMap(cfg, options);
}

BotState HandleDungeon(BotConfig& cfg) {
    DungeonStates::DungeonProgressionOptions options = {};
    options.default_outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    options.entry_map_id = MapIds::SPARKFLY_SWAMP;
    options.dungeon_map_ids = BOGROOT_DUNGEON_MAPS;
    options.dungeon_map_count = BOGROOT_DUNGEON_MAP_COUNT;
    options.log_prefix = "Froggy";
    options.entry_map_name = "Sparkfly Swamp";
    options.dungeon_name = "Bogroot";
    options.refresh_skill_cache = []() {
        RefreshCombatSkillbar();
    };
    options.use_consumables = [](const BotConfig& botCfg) {
        UseConsumables(botCfg);
    };
    options.move_to_entry_npc = []() {
        return MoveToTekksFromSparkflyCurrentSide();
    };
    options.prepare_entry = []() {
        return PrepareTekksDungeonEntry();
    };
    options.enter_dungeon = []() {
        return EnterBogrootFromSparkfly();
    };
    options.record_entry_failure = [](const char* context) {
        (void)RecordTekksQuestEntryFailureAndMaybeResetDialog(context);
    };
    options.reset_entry_failures = [](const char* context) {
        ResetTekksQuestEntryFailures(context);
    };
    options.run_dungeon_loop = []() {
        const bool completed = RunDungeonLoopFromCurrentMap();
        const DungeonLoopTelemetry telemetry = GetDungeonLoopTelemetry();
        return DungeonStates::DungeonLoopStateResult{
            completed,
            telemetry.final_map_id,
        };
    };
    options.needs_maintenance = [&cfg]() {
        MaintenanceMgr::Config maintenanceCfg = MakeFroggyMaintenanceConfig(
            cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT);
        return MaintenanceMgr::NeedsMaintenance(maintenanceCfg);
    };
    options.resolve_post_entry_map_run_decision = [](bool maintenanceNeeded) {
        const auto decision = ResolvePostSparkflyRunDecision(maintenanceNeeded);
        return DungeonStates::PostEntryMapRunDecision{
            decision.next_state,
            decision.maintenance_deferred,
        };
    };
    options.mark_run_started = []() {
        s_runCount++;
        s_runStartTime = GetTickCount();
        return s_runCount;
    };
    options.mark_run_completed = [](uint32_t runNumber, uint32_t finalMapId) {
        DWORD runTime = GetTickCount() - s_runStartTime;
        if (runTime < s_bestRunTime) s_bestRunTime = runTime;
        LogBot("Run #%u complete in %u ms (best: %u ms finalMap=%u)",
               runNumber,
               runTime,
               s_bestRunTime,
               finalMapId);
    };
    options.mark_run_failed = [](uint32_t) {
        s_failCount++;
    };
    return DungeonStates::HandleDungeonProgression(cfg, options);
}

BotState HandleError(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: ERROR - waiting 10s before retry");
    WaitMs(10000);

    uint32_t mapId = MapMgr::GetMapId();
    if (mapId == 0) {
        return BotState::CharSelect;
    }
    if (mapId == MapIds::SPARKFLY_SWAMP ||
        mapId == MapIds::BOGROOT_GROWTHS_LVL1 ||
        mapId == MapIds::BOGROOT_GROWTHS_LVL2) {
        LogBot("Error recovery staying in explorable map %u", mapId);
        return BotState::InDungeon;
    }
    return BotState::InTown;
}

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

bool DebugRunDungeonLoopFromCurrentMap() {
    return RunDungeonLoopFromCurrentMap();
}

} // namespace GWA3::Bot::Froggy
