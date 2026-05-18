#include "DungeonNavigationTestSupport.h"

#include <gwa3/managers/AgentMgr.h>

#include <cmath>

using namespace GWA3::DungeonNavigation;

namespace GWA3::Tests::DungeonNavigationSupport {
namespace {

uint32_t g_navigation_callback_moves = 0u;
float g_last_waypoint_move_value = 0.0f;
bool g_route_retry_blocked_second_waypoint = false;
bool g_route_retry_unlocked_second_waypoint = false;
bool g_progress_floor_failure_armed = false;
bool g_progress_floor_third_waypoint_unlocked = false;
bool g_progress_floor_wrong_backtrack = false;

} // namespace

void ResetNavigationCallback() {
    g_navigation_callback_moves = 0u;
    g_last_waypoint_move_value = 0.0f;
}

void ResetRouteRetryMoveScript() {
    g_route_retry_blocked_second_waypoint = false;
    g_route_retry_unlocked_second_waypoint = false;
    g_progress_floor_failure_armed = false;
    g_progress_floor_third_waypoint_unlocked = false;
    g_progress_floor_wrong_backtrack = false;
}

uint32_t NavigationCallbackMoves() {
    return g_navigation_callback_moves;
}

float LastWaypointMoveValue() {
    return g_last_waypoint_move_value;
}

bool ProgressFloorWrongBacktrack() {
    return g_progress_floor_wrong_backtrack;
}

void QueueMoveForTest(float x, float y) {
    ++g_navigation_callback_moves;
    GWA3::AgentMgr::Move(x, y);
}

void MoveWaypointForTest(float x, float y, float value) {
    ++g_navigation_callback_moves;
    g_last_waypoint_move_value = value;
    GWA3::AgentMgr::Move(x, y);
}

bool NavigationMapLoadedTrue() {
    return true;
}

bool NavigationMapLoadedFalse() {
    return false;
}

void QueueMoveWithBacktrackRetry(float x, float y) {
    ++g_navigation_callback_moves;

    const int targetX = static_cast<int>(std::round(x));
    const int targetY = static_cast<int>(std::round(y));
    if (targetX == 100 && targetY == 0) {
        if (g_route_retry_blocked_second_waypoint) {
            g_route_retry_unlocked_second_waypoint = true;
        }
        GWA3::AgentMgr::Move(100.0f, 0.0f);
        return;
    }

    if (targetX == 200 && targetY == 0 && !g_route_retry_unlocked_second_waypoint) {
        g_route_retry_blocked_second_waypoint = true;
        return;
    }

    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveWithProgressFloorRetry(float x, float y) {
    ++g_navigation_callback_moves;

    const int targetX = static_cast<int>(std::round(x));
    const int targetY = static_cast<int>(std::round(y));
    if (targetY != 0) {
        GWA3::AgentMgr::Move(x, y);
        return;
    }

    if (targetX == 100) {
        if (g_progress_floor_failure_armed && !g_progress_floor_third_waypoint_unlocked) {
            g_progress_floor_wrong_backtrack = true;
        }
        GWA3::AgentMgr::Move(100.0f, 0.0f);
        return;
    }

    if (targetX == 200) {
        if (g_progress_floor_failure_armed) {
            g_progress_floor_third_waypoint_unlocked = true;
        }
        GWA3::AgentMgr::Move(200.0f, 0.0f);
        return;
    }

    if (targetX == 300 && !g_progress_floor_third_waypoint_unlocked) {
        g_progress_floor_failure_armed = true;
        GWA3::AgentMgr::Move(110.0f, 0.0f);
        return;
    }

    GWA3::AgentMgr::Move(x, y);
}

uint32_t ResolveNearestNpcForTest(float x, float y, float radius) {
    uint32_t nearestId = 0u;
    float nearestDistance = radius;
    for (uint32_t agentId = 1u; agentId < 64u; ++agentId) {
        auto* agent = GWA3::AgentMgr::GetAgentByID(agentId);
        if (agent == nullptr || agent->type != 0x200u) {
            continue;
        }

        const float distance = GWA3::AgentMgr::GetDistance(x, y, agent->x, agent->y);
        if (distance <= nearestDistance) {
            nearestDistance = distance;
            nearestId = agentId;
        }
    }
    return nearestId;
}

void AssertStuckMonitorBehavior() {
    auto monitor = MakeStuckMonitor(0.0f, 0.0f);
    constexpr float currentX = 250.0f;
    constexpr float currentY = -125.0f;
    constexpr float targetX = 1000.0f;
    constexpr float targetY = 2000.0f;
    constexpr float expectedRecoveryX = -180.0f;
    constexpr float expectedRecoveryY = -615.0f;

    for (int i = 0; i < 14; ++i) {
        const auto resolution = EvaluateStuckMonitor(
            currentX,
            currentY,
            targetX,
            targetY,
            monitor,
            42u);
        GWA3_ASSERT(!resolution.issue_recovery_move);
        GWA3_ASSERT(!resolution.abort_move);
    }

    const auto recovery = EvaluateStuckMonitor(
        currentX,
        currentY,
        targetX,
        targetY,
        monitor,
        42u);
    GWA3_ASSERT(recovery.issue_recovery_move);
    GWA3_ASSERT(!recovery.abort_move);
    GWA3_ASSERT(std::fabs(recovery.recovery_x - expectedRecoveryX) < 0.5f);
    GWA3_ASSERT(std::fabs(recovery.recovery_y - expectedRecoveryY) < 0.5f);

    auto resetMonitor = monitor;
    const auto reset = EvaluateStuckMonitor(
        currentX + 50.0f,
        currentY,
        targetX,
        targetY,
        resetMonitor,
        42u);
    GWA3_ASSERT(!reset.issue_recovery_move);
    GWA3_ASSERT(!reset.abort_move);
    GWA3_ASSERT_EQ(resetMonitor.low_movement_count, 0);

    auto abortMonitor = MakeStuckMonitor(0.0f, 0.0f);
    for (int i = 0; i < 29; ++i) {
        const auto resolution = EvaluateStuckMonitor(
            0.0f,
            0.0f,
            1000.0f,
            2000.0f,
            abortMonitor,
            7u);
        if (i < 14) {
            GWA3_ASSERT(!resolution.issue_recovery_move);
            GWA3_ASSERT(!resolution.abort_move);
        }
    }
    const auto abortResolution = EvaluateStuckMonitor(
        0.0f,
        0.0f,
        1000.0f,
        2000.0f,
        abortMonitor,
        7u);
    GWA3_ASSERT(abortResolution.abort_move);
}

} // namespace GWA3::Tests::DungeonNavigationSupport
