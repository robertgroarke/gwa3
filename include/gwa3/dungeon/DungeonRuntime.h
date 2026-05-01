#pragma once

#include <cstdint>
#include <functional>

namespace GWA3::DungeonRuntime {

using MoveToPointFn = bool(*)(float x, float y, float threshold);
using VoidMoveToPointFn = void(*)(float x, float y, float threshold);
using QueueMoveFn = void(*)(float x, float y);
using GetMapIdFn = uint32_t(*)();
using WaitMsFn = void(*)(uint32_t ms);
using SalvageRewardItemsFn = uint32_t(*)();
using FindTransitionPortalFn = uint32_t(*)(float x, float y, float searchRadius);

struct TransitionAnchor {
    float x = 0.0f;
    float y = 0.0f;
};

struct TransitionTelemetry {
    bool map_loaded = false;
    bool player_alive = false;
    float player_hp = 0.0f;
    float player_x = 0.0f;
    float player_y = 0.0f;
    uint32_t target_id = 0u;
    float dist_to_exit = -1.0f;
    float nearest_enemy_dist = -1.0f;
    uint32_t nearby_enemy_count = 0u;
    uint32_t portal_id = 0u;
    float portal_x = 0.0f;
    float portal_y = 0.0f;
    float portal_dist = -1.0f;
    uint32_t portal_type = 0u;
    uint32_t portal_gadget = 0u;
};

using TransitionStartedFn = void(*)();
using TransitionAttemptFn = void(*)(uint32_t attempt);
using TransitionEnteredFn = void(*)(uint32_t attempt);
using TransitionTelemetryFn = void(*)(const TransitionTelemetry& telemetry);

struct LevelTransitionOptions {
    const char* log_prefix = nullptr;
    const char* transition_name = nullptr;
    TransitionAnchor exit_anchor = {};
    uint32_t target_map_id = 0u;
    float portal_search_radius = 0.0f;
    uint32_t timeout_ms = 60000u;
    uint32_t log_interval_ms = 2000u;
    uint32_t move_poll_ms = 250u;
    uint32_t spawn_ready_timeout_ms = 0u;
    uint32_t spawn_ready_poll_ms = 200u;
    TransitionAnchor spawn_stale_anchor = {};
    float spawn_stale_anchor_clearance = 0.0f;
    uint32_t spawn_settle_timeout_ms = 0u;
    float spawn_settle_distance = 0.0f;
    float nearest_enemy_range = 5000.0f;
    float nearby_enemy_range = 1800.0f;
    QueueMoveFn queue_move = nullptr;
    GetMapIdFn get_map_id = nullptr;
    WaitMsFn wait_ms = nullptr;
    FindTransitionPortalFn find_portal = nullptr;
    TransitionStartedFn on_started = nullptr;
    TransitionAttemptFn on_attempt = nullptr;
    TransitionEnteredFn on_entered = nullptr;
    TransitionTelemetryFn on_telemetry = nullptr;
};

struct LevelTransitionResult {
    bool entered_target_map = false;
    bool spawn_ready = false;
    uint32_t attempts = 0u;
    uint32_t portal_id = 0u;
    uint32_t elapsed_ms = 0u;
    uint32_t final_map_id = 0u;
};

struct PostRewardReturnOptions {
    uint32_t expected_return_map_id = 0u;
    uint32_t fallback_recovery_map_id = 0u;
    uint32_t fallback_recovery_timeout_ms = 120000u;
    uint32_t long_transition_timeout_ms = 210000u;
    uint32_t short_transition_timeout_ms = 45000u;
    uint32_t long_load_timeout_ms = 180000u;
    uint32_t short_load_timeout_ms = 30000u;
    const uint32_t* dungeon_map_ids = nullptr;
    int dungeon_map_count = 0;
    bool reward_claimed = false;
    bool reward_dialog_latched = false;
    SalvageRewardItemsFn salvage_reward_items = nullptr;
    const char* log_prefix = nullptr;
    const char* label = nullptr;
};

struct PostRewardReturnResult {
    bool used_long_wait = false;
    bool salvaged_reward_items = false;
    bool returned_expected_map = false;
    bool fallback_attempted = false;
    bool fallback_recovered = false;
    bool skipped_fallback_ghost_state = false;
    uint32_t salvaged_item_count = 0u;
    uint32_t final_map_id = 0u;
    bool final_map_loaded = false;
    uint32_t final_player_id = 0u;
};

bool IsDead();
bool IsMapLoaded();
void WaitMs(uint32_t ms);
bool WaitForCondition(uint32_t timeoutMs, const std::function<bool()>& predicate, uint32_t pollMs = 50u);
void SuspendTransitionSensitiveHooks(const char* context = nullptr);
void ResumeTransitionSensitiveHooks(const char* context = nullptr);
bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs);
bool WaitForTownRuntimeReady(uint32_t mapId, uint32_t timeoutMs);
bool WaitForLevelSpawnReady(
    uint32_t targetMapId,
    const TransitionAnchor& staleAnchor,
    float staleAnchorClearance,
    uint32_t timeoutMs,
    uint32_t pollMs = 200u);
TransitionTelemetry CaptureLevelTransitionTelemetry(
    const TransitionAnchor& exitAnchor,
    uint32_t portalId,
    float nearestEnemyRange = 5000.0f,
    float nearbyEnemyRange = 1800.0f);
void LogLevelTransitionTelemetry(
    const char* logPrefix,
    const char* transitionName,
    const char* stage,
    uint32_t portalId,
    uint32_t elapsedMs,
    uint32_t attempt,
    const TransitionAnchor& exitAnchor,
    float nearestEnemyRange = 5000.0f,
    float nearbyEnemyRange = 1800.0f,
    TransitionTelemetry* outTelemetry = nullptr);
LevelTransitionResult ExecuteLevelTransition(const LevelTransitionOptions& options);
bool EnsureOutpostReady(uint32_t outpostMapId, uint32_t timeoutMs, const char* context = nullptr);
bool PushUntilMapReady(
    uint32_t targetMapId,
    float pushX,
    float pushY,
    uint32_t transitionTimeoutMs = 60000u,
    uint32_t loadTimeoutMs = 30000u,
    uint32_t settleMs = 3000u,
    const char* context = nullptr);
bool StageAndPushUntilMapReady(
    uint32_t targetMapId,
    float stageX,
    float stageY,
    float stageThreshold,
    float pushX,
    float pushY,
    MoveToPointFn move_to_point,
    uint32_t prePushDelayMs = 1000u,
    uint32_t transitionTimeoutMs = 60000u,
    uint32_t loadTimeoutMs = 30000u,
    uint32_t settleMs = 3000u,
    const char* context = nullptr);
bool ExecuteMapTransitionMove(
    float x,
    float y,
    uint32_t targetMapId,
    VoidMoveToPointFn moveToPoint,
    QueueMoveFn queueMove,
    GetMapIdFn getMapId,
    WaitMsFn waitMs,
    uint32_t timeoutMs = 60000u,
    uint32_t pulseDelayMs = 250u,
    float settleThreshold = 250.0f);
bool WaitForPostDungeonReturn(
    uint32_t expectedMapId,
    uint32_t transitionTimeoutMs,
    uint32_t loadTimeoutMs,
    const uint32_t* dungeonMapIds,
    int dungeonMapCount,
    const char* logPrefix = nullptr);
PostRewardReturnResult HandlePostRewardReturn(const PostRewardReturnOptions& options);

} // namespace GWA3::DungeonRuntime
