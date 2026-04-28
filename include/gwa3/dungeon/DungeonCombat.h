#pragma once

#include <gwa3/dungeon/DungeonNavigation.h>

#include <cstdint>

namespace GWA3::DungeonCombat {

inline constexpr float LONG_BOW_RANGE = 1320.0f;

inline constexpr float AGGRO_DEFAULT_FIGHT_RANGE = 1350.0f;
inline constexpr float AGGRO_ARRIVAL_THRESHOLD = 250.0f;
inline constexpr uint32_t AGGRO_MOVE_BUDGET_MS = 240000u;
inline constexpr uint32_t AGGRO_FORCE_MOVE_AFTER_MS = 60000u;
inline constexpr int AGGRO_BLOCKED_LIMIT = 30;
inline constexpr uint32_t AGGRO_LOOP_POLL_MS = 100u;
inline constexpr float AGGRO_MOVE_RANDOM_RADIUS = 100.0f;
inline constexpr uint32_t AGGRO_BLOCKED_SIDESTEP_WAIT_MS = 350u;

inline constexpr uint32_t AGGRO_STANDARD_MOVING_REISSUE_MS = 1800u;
inline constexpr uint32_t AGGRO_STANDARD_REISSUE_MS = 1200u;
inline constexpr float AGGRO_STANDARD_LOCAL_CLEAR_COOLDOWN_DISTANCE = 450.0f;
inline constexpr uint32_t AGGRO_STANDARD_LOCAL_CLEAR_COOLDOWN_MS = 2500u;
inline constexpr float AGGRO_STANDARD_BLOCKED_PROGRESS_DISTANCE = 10.0f;
inline constexpr int AGGRO_STANDARD_SIDESTEP_MODULUS = 1000;
inline constexpr int AGGRO_STANDARD_SIDESTEP_HALF_RANGE = 500;
inline constexpr uint32_t AGGRO_STANDARD_LOCAL_CLEAR_SETTLE_TIMEOUT_MS = 1200u;
inline constexpr float AGGRO_STANDARD_LOCAL_CLEAR_SETTLE_DISTANCE = 18.0f;
inline constexpr uint32_t AGGRO_STANDARD_LOCAL_CLEAR_RESUME_DELAY_MS = 250u;
inline constexpr uint32_t AGGRO_STANDARD_COOLDOWN_MOVE_DELAY_MS = 150u;
inline constexpr uint32_t AGGRO_STANDARD_NO_BALL_DELAY_MS = 100u;

inline constexpr uint32_t LOCAL_CLEAR_PRE_FIGHT_CANCEL_DWELL_MS = 50u;
inline constexpr uint32_t LOCAL_CLEAR_POST_FIGHT_CANCEL_DWELL_MS = 150u;
inline constexpr uint32_t LOCAL_CLEAR_DWELL_POLL_MS = 200u;
inline constexpr float LOCAL_CLEAR_NEAREST_ENEMY_SCAN_PADDING = 250.0f;
inline constexpr float LOCAL_CLEAR_EXIT_DISTANCE_PADDING = 75.0f;
inline constexpr float LOCAL_CLEAR_RANGE_PADDING = 250.0f;
inline constexpr float LOCAL_CLEAR_MIN_RANGE = 1600.0f;

using WaitFn = void(*)(uint32_t ms);
using MoveIssuerFn = void(*)(float x, float y);
using FightTargetFn = void(*)(uint32_t targetId);
using PickupLootFn = int(*)(float maxRange);
using BoolFn = bool(*)();
using AggroUseSkillsFn = int(*)(uint32_t targetId, float aggroRange, bool waitForCompletion);
using AggroRecordTargetFn = void(*)(void* userData, uint32_t targetId);
using AggroRecordAutoAttackFn = void(*)(void* userData, uint32_t targetId, uint32_t actionStartMs);
using AggroRecordActionFn = void(*)(void* userData, uint32_t actionStartMs);
using AggroPostLootFn = void(*)(void* userData, float aggroRange, const char* reason);
enum class AggroWaypointPhase : uint8_t;
using AggroWaypointHookFn = bool(*)(const DungeonRoute::Waypoint& waypoint,
                                    int waypointIndex,
                                    DungeonRoute::WaypointLabelKind labelKind,
                                    AggroWaypointPhase phase,
                                    void* userData);

enum class LocalClearProfile : uint8_t {
    StandardTraversal,
    ShortTraversal,
};

struct LocalClearPolicy {
    LocalClearProfile profile = LocalClearProfile::StandardTraversal;
    bool single_pass = false;
    float clear_range = 0.0f;
    uint32_t local_clear_budget_ms = 0;
    uint32_t quiet_dwell_ms = 0;
    uint32_t initial_dwell_timeout_ms = 0;
    uint32_t settle_dwell_timeout_ms = 0;
    uint32_t fight_budget_ms = 0;
    int max_clear_passes = 0;
    const char* clear_label = "Route";
    const char* loot_reason = "local-clear";
};

struct CombatCallbacks {
    BoolFn is_dead = nullptr;
    BoolFn is_map_loaded = nullptr;
    WaitFn wait_ms = nullptr;
    MoveIssuerFn queue_move = nullptr;
    FightTargetFn fight_target = nullptr;
    PickupLootFn pickup_loot = nullptr;
};

struct ClearEnemiesOptions {
    float minimum_engage_range = 1200.0f;
    float extra_clear_range = 250.0f;
    float minimum_local_clear_range = 1600.0f;
    float chase_distance = 1100.0f;
    float pickup_range = 800.0f;
    uint32_t timeout_ms = 180000u;
    uint32_t quiet_confirmation_ms = 1500u;
    uint32_t target_timeout_ms = 120000u;
    uint32_t flag_reissue_ms = 3000u;
    uint32_t target_reissue_ms = 5000u;
    uint32_t fight_reissue_ms = 1250u;
    uint32_t attack_reissue_ms = 2000u;
    uint32_t chase_wait_ms = 350u;
    uint32_t pre_clear_cancel_wait_ms = 0u;
    uint32_t post_clear_cancel_wait_ms = 0u;
    uint32_t idle_wait_ms = 250u;
    uint32_t loop_wait_ms = 650u;
    bool pickup_after_clear = true;
    bool flag_heroes = true;
    bool change_target = true;
    bool call_target = true;
    bool chase_during_clear = true;
    bool hold_movement_for_local_clear = false;
};

struct AggroAdvanceOptions {
    float arrival_threshold = 250.0f;
    uint32_t timeout_ms = 240000u;
    uint32_t move_wait_ms = 500u;
    float stuck_minimum_progress = 10.0f;
    int stuck_recovery_threshold = 15;
    int stuck_abort_threshold = 30;
    float stuck_recovery_radius = 500.0f;
    ClearEnemiesOptions clear_options = {};
};

enum class AggroWaypointPhase : uint8_t {
    BeforeAdvance,
    AfterAdvance,
};

struct AggroWaypointCallbacks {
    AggroWaypointHookFn on_waypoint = nullptr;
    void* user_data = nullptr;
};

struct AggroFightCallbacks {
    BoolFn is_dead = nullptr;
    WaitFn wait_ms = nullptr;
    AggroUseSkillsFn use_skills = nullptr;
    AggroRecordTargetFn record_target = nullptr;
    AggroRecordAutoAttackFn record_auto_attack = nullptr;
    AggroRecordActionFn record_action = nullptr;
    AggroPostLootFn post_loot = nullptr;
    void* user_data = nullptr;
};

struct AggroFightOptions {
    bool careful = false;
    bool wait_for_skill_completion = true;
    uint32_t max_fight_ms = 240000u;
    uint32_t restricted_skill_override_map_id = 0u;
    const char* log_prefix = "DungeonCombat";
    const char* loot_reason = "combat-step";
};

inline float ComputeLocalClearRange(float fightRange) {
    const float clearRange = fightRange + LOCAL_CLEAR_RANGE_PADDING;
    return clearRange > LOCAL_CLEAR_MIN_RANGE ? clearRange : LOCAL_CLEAR_MIN_RANGE;
}

inline LocalClearPolicy BuildLocalClearPolicy(
    LocalClearProfile profile,
    const char* label,
    float fightRange,
    bool routeEntryTraversal) {
    LocalClearPolicy policy;
    policy.profile = profile;
    policy.single_pass = profile == LocalClearProfile::ShortTraversal;
    policy.clear_range = ComputeLocalClearRange(fightRange);
    policy.local_clear_budget_ms = policy.single_pass ? 20000u : 120000u;
    policy.quiet_dwell_ms = policy.single_pass ? 700u : 1250u;
    policy.initial_dwell_timeout_ms = policy.single_pass ? 1500u : 2500u;
    policy.settle_dwell_timeout_ms = policy.single_pass ? 2000u : 4000u;
    policy.fight_budget_ms = policy.single_pass ? 8000u : 240000u;
    policy.max_clear_passes = policy.single_pass ? 1 : 0x7FFFFFFF;
    policy.clear_label = label ? label : "Route";
    policy.loot_reason = routeEntryTraversal ? "route-entry-local-clear" : "local-clear";
    return policy;
}

float DistanceToPoint(float x, float y);
float GetNearestLivingEnemyDistance(float maxRange = 99999.0f);
bool CanMoveWithEnemyRangeGate(float range, float unrestrictedRange = LONG_BOW_RANGE);
uint32_t FindNearestLivingEnemy(float maxRange, float* outDistance = nullptr);
uint32_t CountLivingEnemiesInRange(float maxRange);
bool WaitForEnemyClearDwell(float clearRange,
                            uint32_t dwellMs,
                            uint32_t timeoutMs,
                            const CombatCallbacks& callbacks = {},
                            uint32_t pollMs = 250u);
void FlagAllHeroes(float x, float y);
void UnflagAllHeroes();
bool ClearEnemiesInArea(float fightRange, const CombatCallbacks& callbacks,
                        const ClearEnemiesOptions& options = {});
bool AdvanceWithAggro(float x, float y, float fightRange, const CombatCallbacks& callbacks,
                      const AggroAdvanceOptions& options = {});
bool FightEnemiesInAggro(float aggroRange,
                         const AggroFightCallbacks& callbacks,
                         const AggroFightOptions& options = {});
DungeonNavigation::RouteFollowResult FollowWaypointsWithAggro(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    uint32_t mapId,
    const CombatCallbacks& callbacks,
    const DungeonNavigation::RouteFollowOptions& options = {},
    const AggroAdvanceOptions& aggroOptions = {},
    const AggroWaypointCallbacks& waypointCallbacks = {});

} // namespace GWA3::DungeonCombat
