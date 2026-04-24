#pragma once

#include <gwa3/bot/DungeonNavigation.h>

#include <cstdint>

namespace GWA3::Bot::DungeonCombat {

using WaitFn = void(*)(uint32_t ms);
using MoveIssuerFn = void(*)(float x, float y);
using FightTargetFn = void(*)(uint32_t targetId);
using PickupLootFn = int(*)(float maxRange);
using BoolFn = bool(*)();
enum class AggroWaypointPhase : uint8_t;
using AggroWaypointHookFn = bool(*)(const DungeonRoute::Waypoint& waypoint,
                                    int waypointIndex,
                                    DungeonRoute::WaypointLabelKind labelKind,
                                    AggroWaypointPhase phase,
                                    void* userData);

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

float DistanceToPoint(float x, float y);
uint32_t FindNearestLivingEnemy(float maxRange, float* outDistance = nullptr);
uint32_t CountLivingEnemiesInRange(float maxRange);
void FlagAllHeroes(float x, float y);
void UnflagAllHeroes();
bool ClearEnemiesInArea(float fightRange, const CombatCallbacks& callbacks,
                        const ClearEnemiesOptions& options = {});
bool AdvanceWithAggro(float x, float y, float fightRange, const CombatCallbacks& callbacks,
                      const AggroAdvanceOptions& options = {});
DungeonNavigation::RouteFollowResult FollowWaypointsWithAggro(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    uint32_t mapId,
    const CombatCallbacks& callbacks,
    const DungeonNavigation::RouteFollowOptions& options = {},
    const AggroAdvanceOptions& aggroOptions = {},
    const AggroWaypointCallbacks& waypointCallbacks = {});

} // namespace GWA3::Bot::DungeonCombat
