#pragma once

#include <gwa3/bot/DungeonRoute.h>

#include <cstdint>

namespace GWA3::Bot::DungeonNavigation {

using MoveIssuerFn = void(*)(float x, float y);
using AgentResolverFn = uint32_t(*)(float x, float y, float radius);

struct MoveToResult {
    bool arrived = false;
    bool map_changed = false;
    bool timed_out = false;
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

bool IsWithinDistance(float currentX, float currentY, float targetX, float targetY, float threshold);
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
bool WaitForMapId(uint32_t targetMapId, uint32_t timeoutMs = 60000u);

} // namespace GWA3::Bot::DungeonNavigation
