#pragma once

#include <cstdint>
#include <functional>

namespace GWA3::DungeonRuntime {

using MoveToPointFn = bool(*)(float x, float y, float threshold);
using VoidMoveToPointFn = void(*)(float x, float y, float threshold);
using QueueMoveFn = void(*)(float x, float y);
using GetMapIdFn = uint32_t(*)();
using WaitMsFn = void(*)(uint32_t ms);

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

} // namespace GWA3::DungeonRuntime
