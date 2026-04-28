#pragma once

#include <gwa3/dungeon/DungeonRoute.h>

#include <cstdint>

namespace GWA3::DungeonNavigation {

inline constexpr float MOVE_TO_DEFAULT_THRESHOLD = 250.0f;
inline constexpr uint32_t MOVE_TO_TIMEOUT_MS = 30000u;
inline constexpr uint32_t MOVE_TO_POLL_MS = 1000u;

using MoveIssuerFn = void(*)(float x, float y);
using WaypointMoveFn = void(*)(float x, float y, float value);
using RouteWaypointMoveFn = void(*)(const DungeonRoute::Waypoint& waypoint);
using DoorOpenAtFn = bool(*)(float x, float y);
using BlessingGrabFn = void(*)(float x, float y);
using LootAfterCombatFn = int(*)(float aggroRange, const char* reason);
using PickupNearbyLootFn = int(*)(float maxRange);
using AggroFightFn = void(*)(float aggroRange,
                             bool careful,
                             void* stats,
                             bool waitForSkillCompletion,
                             uint32_t maxFightMs);
using HoldLocalClearFn = void(*)(const char* label,
                                 float x,
                                 float y,
                                 float fightRange,
                                 uint32_t targetId,
                                 void* stats);
using HoldSpecialLocalClearFn = void(*)(float x,
                                        float y,
                                        float fightRange,
                                        uint32_t targetId,
                                        void* stats);
using AgentResolverFn = uint32_t(*)(float x, float y, float radius);
using IsMapLoadedFn = bool(*)();
using BoolFn = bool(*)();
using WaitFn = void(*)(uint32_t ms);

struct MoveToResult {
    bool arrived = false;
    bool map_changed = false;
    bool timed_out = false;
};

struct LoggedMoveOptions {
    const char* log_prefix = "Dungeon";
    BoolFn is_dead = nullptr;
    uint32_t timeout_ms = MOVE_TO_TIMEOUT_MS;
    uint32_t poll_ms = MOVE_TO_POLL_MS;
};

struct AgentApproachResult {
    MoveToResult anchor_move;
    uint32_t agent_id = 0u;
    bool agent_found = false;
    MoveToResult agent_move;
};

struct RouteFollowOptions {
    float default_tolerance = 250.0f;
    bool use_waypoint_fight_range_as_tolerance = false;
    uint32_t waypoint_timeout_ms = 30000u;
    uint32_t reissue_ms = 1000u;
    int max_backtrack_retries = 0;
    int backtrack_count = 1;
};

struct RouteFollowResult {
    bool completed = false;
    bool map_changed = false;
    int failed_index = -1;
    int retries_used = 0;
};

struct StuckMonitor {
    float last_x = 0.0f;
    float last_y = 0.0f;
    int low_movement_count = 0;
    bool initialized = false;
};

struct StuckResolution {
    bool issue_recovery_move = false;
    bool abort_move = false;
    float recovery_x = 0.0f;
    float recovery_y = 0.0f;
};

struct WaypointTelemetryOptions {
    const char* log_prefix = "Dungeon";
    const char* route_name = "route";
    float nearest_enemy_range = 5000.0f;
    float nearby_enemy_range = 1800.0f;
};

bool IsWithinDistance(float currentX, float currentY, float targetX, float targetY, float threshold);
float RandomizedCoordinate(float center, float radius);
StuckMonitor MakeStuckMonitor(float startX, float startY);
StuckResolution EvaluateStuckMonitor(
    float currentX,
    float currentY,
    float targetX,
    float targetY,
    StuckMonitor& monitor,
    uint32_t randomSeed,
    float minimumProgress = 10.0f,
    int recoveryThreshold = 15,
    int abortThreshold = 30,
    float recoveryRadius = 500.0f);
MoveToResult MoveToAndWait(
    float x,
    float y,
    float threshold = 250.0f,
    uint32_t timeoutMs = 30000u,
    uint32_t reissueMs = 1000u,
    uint32_t expectedMapId = 0u);
MoveToResult MoveToAndWait(
    float x,
    float y,
    float threshold,
    uint32_t timeoutMs,
    uint32_t reissueMs,
    uint32_t expectedMapId,
    MoveIssuerFn moveIssuer);
void MoveToPoint(float x, float y, float threshold);
bool MoveToAndWaitLogged(
    float x,
    float y,
    float threshold = MOVE_TO_DEFAULT_THRESHOLD,
    const LoggedMoveOptions& options = {});
void MoveRouteWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    WaypointMoveFn moveToPoint,
    WaypointMoveFn aggroMoveToPoint,
    IsMapLoadedFn isMapLoaded,
    float moveThreshold = 250.0f);
void LogWaypointState(
    const char* stage,
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int waypointIndex,
    const WaypointTelemetryOptions& options = {});
void MoveRouteWaypointWithCombatLoot(
    const DungeonRoute::Waypoint& waypoint,
    int waypointIndex,
    RouteWaypointMoveFn moveRouteWaypoint,
    IsMapLoadedFn isMapLoaded,
    LootAfterCombatFn lootAfterCombat,
    const char* logPrefix = "Dungeon");
bool HandleBlessingWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    RouteWaypointMoveFn moveRouteWaypoint,
    BlessingGrabFn grabBlessing);
bool HandleOpenDungeonDoorWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    WaypointMoveFn aggroMoveToPoint,
    DoorOpenAtFn openDoorAt);
bool WaitForLocalPositionSettle(
    uint32_t timeoutMs,
    float maxDeltaPerSample = 20.0f,
    uint32_t sampleMs = 150u,
    int settledSamplesRequired = 3);
MoveToResult MoveToAgent(
    uint32_t agentId,
    float threshold = 120.0f,
    uint32_t timeoutMs = 30000u,
    uint32_t reissueMs = 1000u,
    uint32_t expectedMapId = 0u,
    MoveIssuerFn moveIssuer = nullptr);
AgentApproachResult MoveToNearestResolvedAgent(
    float anchorX,
    float anchorY,
    float anchorThreshold,
    float searchRadius,
    float agentThreshold,
    AgentResolverFn agentResolver,
    MoveIssuerFn moveIssuer = nullptr,
    uint32_t timeoutMs = 30000u,
    uint32_t reissueMs = 1000u,
    uint32_t expectedMapId = 0u);
RouteFollowResult FollowWaypoints(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    uint32_t mapId,
    const RouteFollowOptions& options = {},
    MoveIssuerFn moveIssuer = nullptr);
int GetNearestWaypointIndex(const DungeonRoute::Waypoint* waypoints, int count);
bool WaitForMapId(uint32_t targetMapId, uint32_t timeoutMs = 60000u);

// ===== Aggro-move support =====

struct AggroMoveState {
    float moveTargetX = 0.0f;
    float moveTargetY = 0.0f;
    bool moveTargetInitialized = false;
    uint32_t lastMoveIssuedAt = 0;
    uint32_t localClearCooldownUntil = 0;
    float localClearCooldownX = 0.0f;
    float localClearCooldownY = 0.0f;
    bool localClearCooldownActive = false;
    int blockedCount = 0;
};

enum class AggroMoveProfile : uint8_t {
    Standard,
    Opportunistic,
};

struct AggroMoveCallbacks {
    BoolFn is_dead = nullptr;
    BoolFn is_map_loaded = nullptr;
    WaitFn wait_ms = nullptr;
    AggroFightFn fight_in_aggro = nullptr;
    HoldLocalClearFn hold_local_clear = nullptr;
    HoldSpecialLocalClearFn hold_special_local_clear = nullptr;
    PickupNearbyLootFn pickup_nearby_loot = nullptr;
    void* special_stats = nullptr;
};

struct AggroMoveOptions {
    AggroMoveProfile profile = AggroMoveProfile::Standard;
    bool exact_move_target = false;
    bool use_local_clear_cooldown = false;
    float arrival_threshold = 250.0f;
    float move_random_radius = 100.0f;
    float sidestep_random_radius = 500.0f;
    float opportunistic_loot_radius = 3000.0f;
    uint32_t opportunistic_fight_budget_ms = 4000u;
    uint32_t move_budget_ms = 240000u;
    uint32_t force_move_after_ms = 60000u;
    uint32_t loop_poll_ms = 100u;
    uint32_t blocked_sidestep_wait_ms = 350u;
    int blocked_limit = 30;
    const char* log_prefix = "Dungeon";
};

bool IssueAggroMove(
    AggroMoveState& state,
    float x,
    float y,
    bool sparkflyMap,
    bool force = false,
    uint32_t movingReissueMs = 1800u,
    uint32_t reissueMs = 1200u,
    float randomRadius = 100.0f);

bool ShouldContinueLocalClearCooldown(
    AggroMoveState& state,
    float cooldownDistance = 450.0f);

void ArmLocalClearCooldown(
    AggroMoveState& state,
    float fallbackX,
    float fallbackY,
    uint32_t cooldownMs = 2500u);

void HandleBlockedMoveProgress(
    AggroMoveState& state,
    float x,
    float y,
    float oldX,
    float oldY,
    bool sparkflyMap,
    uint32_t sidestepWaitMs = 350u,
    float blockedProgressDistance = 10.0f,
    int sidestepModulus = 1000,
    int sidestepHalfRange = 500);

void AggroMoveTo(
    float x,
    float y,
    float fightRange,
    const AggroMoveCallbacks& callbacks,
    const AggroMoveOptions& options = {});

} // namespace GWA3::DungeonNavigation
