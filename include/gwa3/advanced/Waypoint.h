#pragma once

#include <cstdint>

namespace GWA3::AdvancedWaypoint {

inline constexpr float MOVE_TO_DEFAULT_THRESHOLD = 250.0f;
inline constexpr uint32_t MOVE_TO_TIMEOUT_MS = 30000u;
inline constexpr uint32_t MOVE_TO_POLL_MS = 1000u;

using MoveIssuerFn = void(*)(float x, float y);
using AgentResolverFn = uint32_t(*)(float x, float y, float radius);
using BoolFn = bool(*)();

struct MoveToResult {
    bool arrived = false;
    bool map_changed = false;
    bool timed_out = false;
};

struct LoggedMoveOptions {
    const char* log_prefix = "Waypoint";
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

bool IsWithinDistance(float currentX, float currentY, float targetX, float targetY, float threshold);
float DistanceToPoint(float x, float y);
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
bool WaitForMapId(uint32_t targetMapId, uint32_t timeoutMs = 60000u);

} // namespace GWA3::AdvancedWaypoint
