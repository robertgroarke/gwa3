#pragma once

#include <cstdint>
#include <functional>

namespace GWA3::DungeonRuntime {

using MoveToPointFn = bool(*)(float x, float y, float threshold);
using VoidMoveToPointFn = void(*)(float x, float y, float threshold);
using QueueMoveFn = void(*)(float x, float y);
using GetMapIdFn = uint32_t(*)();
using WaitMsFn = void(*)(uint32_t ms);

bool IsDead();
bool IsMapLoaded();
void WaitMs(uint32_t ms);
bool WaitForCondition(uint32_t timeoutMs, const std::function<bool()>& predicate, uint32_t pollMs = 50u);
void SuspendTransitionSensitiveHooks(const char* context = nullptr);
void ResumeTransitionSensitiveHooks(const char* context = nullptr);
bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs);
bool WaitForTownRuntimeReady(uint32_t mapId, uint32_t timeoutMs);
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
