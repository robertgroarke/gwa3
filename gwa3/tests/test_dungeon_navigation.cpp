#include <gwa3/bot/DungeonNavigation.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/testing/TestFramework.h>

#include <cmath>

using namespace GWA3::Bot::DungeonNavigation;

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddAgent(uint32_t agentId, float x, float y, uint32_t type);
void SetPlayerAgent(float x, float y, float hp);
uint32_t MoveCount();
float LastMoveX();
float LastMoveY();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::MapMgr {

void Reset();
void SetMapId(uint32_t mapId);

} // namespace GWA3::TestStubs::MapMgr

namespace {

uint32_t g_navigation_callback_moves = 0u;
bool g_route_retry_blocked_second_waypoint = false;
bool g_route_retry_unlocked_second_waypoint = false;
bool g_progress_floor_failure_armed = false;
bool g_progress_floor_third_waypoint_unlocked = false;
bool g_progress_floor_wrong_backtrack = false;

void ResetNavigationCallback() {
    g_navigation_callback_moves = 0u;
}

void ResetRouteRetryMoveScript() {
    g_route_retry_blocked_second_waypoint = false;
    g_route_retry_unlocked_second_waypoint = false;
    g_progress_floor_failure_armed = false;
    g_progress_floor_third_waypoint_unlocked = false;
    g_progress_floor_wrong_backtrack = false;
}

void QueueMoveForTest(float x, float y) {
    ++g_navigation_callback_moves;
    GWA3::AgentMgr::Move(x, y);
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

} // namespace

GWA3_TEST(dungeon_navigation_within_distance, {
    GWA3_ASSERT(IsWithinDistance(0.0f, 0.0f, 3.0f, 4.0f, 5.0f));
    GWA3_ASSERT(!IsWithinDistance(0.0f, 0.0f, 3.0f, 4.0f, 4.9f));
})

GWA3_TEST(dungeon_navigation_move_to_and_wait_uses_custom_move_issuer, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ResetNavigationCallback();

    const auto result = MoveToAndWait(
        150.0f,
        75.0f,
        250.0f,
        1000u,
        100u,
        615u,
        &QueueMoveForTest);

    GWA3_ASSERT(result.arrived);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT(!result.timed_out);
    GWA3_ASSERT_EQ(g_navigation_callback_moves, 1u);
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 150);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 75);
    AssertStuckMonitorBehavior();
})

GWA3_TEST(dungeon_navigation_move_to_nearest_resolved_agent_approaches_anchor_and_agent, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddAgent(7u, 90.0f, 40.0f, 0x200u);
    AgentStubs::AddAgent(8u, 300.0f, 0.0f, 0x200u);
    ResetNavigationCallback();

    const auto result = MoveToNearestResolvedAgent(
        100.0f,
        0.0f,
        250.0f,
        150.0f,
        120.0f,
        &ResolveNearestNpcForTest,
        &QueueMoveForTest);

    GWA3_ASSERT(result.anchor_move.arrived);
    GWA3_ASSERT(result.agent_found);
    GWA3_ASSERT_EQ(result.agent_id, 7u);
    GWA3_ASSERT(result.agent_move.arrived);
    GWA3_ASSERT_EQ(g_navigation_callback_moves, 2u);
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 2u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 90);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 40);
})

GWA3_TEST(dungeon_navigation_follow_waypoints_backtracks_after_timeout, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ResetNavigationCallback();
    ResetRouteRetryMoveScript();

    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {100.0f, 0.0f, 0.0f, "1"},
        {200.0f, 0.0f, 0.0f, "2"},
        {300.0f, 0.0f, 0.0f, "3"},
    };
    RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 100u;
    options.max_backtrack_retries = 1;
    options.backtrack_count = 1;

    const auto result = FollowWaypoints(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        options,
        &QueueMoveWithBacktrackRetry);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT(result.failed_index == -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})

GWA3_TEST(dungeon_navigation_follow_waypoints_preserves_progress_floor_when_player_drifts_backward, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    ResetNavigationCallback();
    ResetRouteRetryMoveScript();

    const GWA3::Bot::DungeonRoute::Waypoint waypoints[] = {
        {100.0f, 0.0f, 0.0f, "1"},
        {200.0f, 0.0f, 0.0f, "2"},
        {300.0f, 0.0f, 0.0f, "3"},
    };
    RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 100u;
    options.max_backtrack_retries = 1;
    options.backtrack_count = 1;

    const auto result = FollowWaypoints(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        options,
        &QueueMoveWithProgressFloorRetry);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT(!g_progress_floor_wrong_backtrack);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
