// Consolidated test module generated from small test files.
#include "DungeonNavigationTestSupport.h"
#include "DungeonRuntimeTestSupport.h"

// --- tests/test_dungeon_navigation_follow_waypoints_backtrack.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_follow_waypoints_backtrack {

using namespace GWA3::DungeonNavigation;
namespace NavSupport = GWA3::Tests::DungeonNavigationSupport;

GWA3_TEST(dungeon_navigation_follow_waypoints_backtracks_after_timeout, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    NavSupport::ResetNavigationCallback();
    NavSupport::ResetRouteRetryMoveScript();

    const GWA3::DungeonRoute::Waypoint waypoints[] = {
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
        &NavSupport::QueueMoveWithBacktrackRetry);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT(result.failed_index == -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_follow_waypoints_backtrack

// --- tests/test_dungeon_navigation_follow_waypoints_progress_floor.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_follow_waypoints_progress_floor {

using namespace GWA3::DungeonNavigation;
namespace NavSupport = GWA3::Tests::DungeonNavigationSupport;

GWA3_TEST(dungeon_navigation_follow_waypoints_preserves_progress_floor_when_player_drifts_backward, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    NavSupport::ResetNavigationCallback();
    NavSupport::ResetRouteRetryMoveScript();

    const GWA3::DungeonRoute::Waypoint waypoints[] = {
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
        &NavSupport::QueueMoveWithProgressFloorRetry);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT(!NavSupport::ProgressFloorWrongBacktrack());
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_follow_waypoints_progress_floor

// --- tests/test_dungeon_navigation_move_route_waypoint.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_move_route_waypoint {

using namespace GWA3::DungeonNavigation;
namespace NavSupport = GWA3::Tests::DungeonNavigationSupport;

GWA3_TEST(dungeon_navigation_move_route_waypoint_uses_aggro_when_fight_range_is_available, {
    AgentStubs::ResetAgents();
    NavSupport::ResetNavigationCallback();
    const GWA3::DungeonRoute::Waypoint waypoint = {100.0f, 50.0f, 900.0f, "Aggro"};

    MoveRouteWaypoint(
        waypoint,
        &NavSupport::MoveWaypointForTest,
        &NavSupport::MoveWaypointForTest,
        &NavSupport::NavigationMapLoadedTrue,
        250.0f);

    GWA3_ASSERT_EQ(NavSupport::NavigationCallbackMoves(), 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 100);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 50);
    GWA3_ASSERT_EQ(static_cast<int>(NavSupport::LastWaypointMoveValue()), 900);

    NavSupport::ResetNavigationCallback();
    MoveRouteWaypoint(
        waypoint,
        &NavSupport::MoveWaypointForTest,
        &NavSupport::MoveWaypointForTest,
        &NavSupport::NavigationMapLoadedFalse,
        250.0f);
    GWA3_ASSERT_EQ(static_cast<int>(NavSupport::LastWaypointMoveValue()), 250);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_move_route_waypoint

// --- tests/test_dungeon_navigation_move_to_and_wait.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_move_to_and_wait {

using namespace GWA3::DungeonNavigation;
namespace NavSupport = GWA3::Tests::DungeonNavigationSupport;

GWA3_TEST(dungeon_navigation_move_to_and_wait_uses_custom_move_issuer, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    NavSupport::ResetNavigationCallback();

    const auto result = MoveToAndWait(
        150.0f,
        75.0f,
        250.0f,
        1000u,
        100u,
        615u,
        &NavSupport::QueueMoveForTest);

    GWA3_ASSERT(result.arrived);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT(!result.timed_out);
    GWA3_ASSERT_EQ(NavSupport::NavigationCallbackMoves(), 1u);
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 150);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 75);
    NavSupport::AssertStuckMonitorBehavior();
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_move_to_and_wait

// --- tests/test_dungeon_navigation_move_to_nearest_agent.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_move_to_nearest_agent {

using namespace GWA3::DungeonNavigation;
namespace NavSupport = GWA3::Tests::DungeonNavigationSupport;

GWA3_TEST(dungeon_navigation_move_to_nearest_resolved_agent_approaches_anchor_and_agent, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    AgentStubs::AddAgent(7u, 90.0f, 40.0f, 0x200u);
    AgentStubs::AddAgent(8u, 300.0f, 0.0f, 0x200u);
    NavSupport::ResetNavigationCallback();

    const auto result = MoveToNearestResolvedAgent(
        100.0f,
        0.0f,
        250.0f,
        150.0f,
        120.0f,
        &NavSupport::ResolveNearestNpcForTest,
        &NavSupport::QueueMoveForTest);

    GWA3_ASSERT(result.anchor_move.arrived);
    GWA3_ASSERT(result.agent_found);
    GWA3_ASSERT_EQ(result.agent_id, 7u);
    GWA3_ASSERT(result.agent_move.arrived);
    GWA3_ASSERT_EQ(NavSupport::NavigationCallbackMoves(), 2u);
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 2u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 90);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 40);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_move_to_nearest_agent

// --- tests/test_dungeon_navigation_move_to_point.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_move_to_point {

using namespace GWA3::DungeonNavigation;

GWA3_TEST(dungeon_navigation_move_to_point_adapts_loot_callback_signature, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);

    MoveToPoint(100.0f, 50.0f, 250.0f);

    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 100);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 50);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_move_to_point

// --- tests/test_dungeon_navigation_position_settle.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_position_settle {

using namespace GWA3::DungeonNavigation;

GWA3_TEST(dungeon_navigation_wait_for_local_position_settle_requires_stable_player, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(10.0f, 20.0f, 1.0f);

    GWA3_ASSERT(WaitForLocalPositionSettle(100u, 1.0f, 1u, 2));

    AgentStubs::ResetAgents();
    AgentStubs::ClearPlayerAgent();
    GWA3_ASSERT(!WaitForLocalPositionSettle(10u, 1.0f, 1u, 1));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_position_settle

// --- tests/test_dungeon_navigation_randomized_coordinate.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_randomized_coordinate {

using namespace GWA3::DungeonNavigation;

GWA3_TEST(dungeon_navigation_randomized_coordinate_stays_inside_radius, {
    for (int i = 0; i < 16; ++i) {
        const float value = RandomizedCoordinate(1000.0f, 125.0f);
        GWA3_ASSERT(value >= 875.0f);
        GWA3_ASSERT(value <= 1125.0f);
    }
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_randomized_coordinate

// --- tests/test_dungeon_navigation_within_distance.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_navigation_within_distance {

using namespace GWA3::DungeonNavigation;

GWA3_TEST(dungeon_navigation_within_distance, {
    GWA3_ASSERT(IsWithinDistance(0.0f, 0.0f, 3.0f, 4.0f, 5.0f));
    GWA3_ASSERT(!IsWithinDistance(0.0f, 0.0f, 3.0f, 4.0f, 4.9f));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_navigation_within_distance

// --- tests/test_dungeon_runtime_execute_map_transition_move.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_runtime_execute_map_transition_move {

using namespace GWA3::Tests::DungeonRuntime;

GWA3_TEST(dungeon_runtime_execute_map_transition_move, {
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::MapMgr::SetMapId(615u);
    GWA3::TestStubs::MapMgr::SetMoveSetsMapIdOnPoint(14747.0f, 480.0f, 558u);

    GWA3_ASSERT(GWA3::DungeonRuntime::ExecuteMapTransitionMove(
        14747.0f,
        480.0f,
        558u,
        &AgentMoveToPoint,
        &AgentQueueMove,
        &GWA3::MapMgr::GetMapId,
        &NoWait,
        100u,
        0u,
        250.0f));
    GWA3_ASSERT_EQ(GWA3::MapMgr::GetMapId(), 558u);
    GWA3_ASSERT(GWA3::TestStubs::AgentMgr::MoveCount() >= 1u);

    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::MapMgr::SetMapId(615u);
    GWA3_ASSERT(!GWA3::DungeonRuntime::ExecuteMapTransitionMove(
        14747.0f,
        480.0f,
        558u,
        &AgentMoveToPoint,
        &AgentQueueMove,
        &GWA3::MapMgr::GetMapId,
        &NoWait,
        0u,
        0u,
        250.0f));
    GWA3_ASSERT_EQ(GWA3::MapMgr::GetMapId(), 615u);
})

} // namespace GWA3::Tests::Consolidated::test_dungeon_runtime_execute_map_transition_move

// --- tests/test_dungeon_runtime_push_until_map_ready.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_runtime_push_until_map_ready {

using namespace GWA3::Tests::DungeonRuntime;

GWA3_TEST(dungeon_runtime_push_until_map_ready_reissues_move_until_transition, {
    ArrangeTransitionFromBogrootToLevel2();

    const bool ready = GWA3::DungeonRuntime::PushUntilMapReady(
        616u,
        13097.0f,
        26393.0f,
        1000u,
        1000u,
        0u,
        "runtime-test");

    GWA3_ASSERT(ready);
    GWA3_ASSERT_EQ(GWA3::MapMgr::GetMapId(), 616u);
    GWA3_ASSERT(GWA3::TestStubs::AgentMgr::MoveCount() > 0u);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::TestStubs::AgentMgr::LastMoveX()), 13097);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::TestStubs::AgentMgr::LastMoveY()), 26393);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_runtime_push_until_map_ready

// --- tests/test_dungeon_runtime_stage_and_push.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_runtime_stage_and_push {

using namespace GWA3::Tests::DungeonRuntime;

GWA3_TEST(dungeon_runtime_stage_and_push_runs_stage_before_transition_push, {
    ArrangeTransitionFromBogrootToLevel2();
    ResetStageMoveRecorder();

    const bool ready = GWA3::DungeonRuntime::StageAndPushUntilMapReady(
        616u,
        14876.0f,
        632.0f,
        300.0f,
        13097.0f,
        26393.0f,
        &RecordStageMove,
        0u,
        1000u,
        1000u,
        0u,
        "runtime-test");

    GWA3_ASSERT(ready);
    GWA3_ASSERT_EQ(g_stageMoveCount, 1u);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastStageX), 14876);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastStageY), 632);
    GWA3_ASSERT_EQ(static_cast<int>(g_lastStageThreshold), 300);
    GWA3_ASSERT_EQ(GWA3::MapMgr::GetMapId(), 616u);
    GWA3_ASSERT(GWA3::TestStubs::AgentMgr::MoveCount() >= 2u);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::TestStubs::AgentMgr::LastMoveX()), 13097);
    GWA3_ASSERT_EQ(static_cast<int>(GWA3::TestStubs::AgentMgr::LastMoveY()), 26393);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_runtime_stage_and_push

// --- tests/test_dungeon_runtime_post_reward_return.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_runtime_post_reward_return {

namespace {
uint32_t g_returnToOutpostCount = 0u;

void RecordReturnToOutpost() {
    ++g_returnToOutpostCount;
}
} // namespace

GWA3_TEST(dungeon_runtime_post_reward_requests_explicit_return_when_reward_leaves_player_in_dungeon, {
    static constexpr uint32_t kDungeonMaps[] = {615u, 616u};

    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::MapMgr::SetMapId(616u);
    g_returnToOutpostCount = 0u;

    GWA3::DungeonRuntime::PostRewardReturnOptions options = {};
    options.expected_return_map_id = 558u;
    options.dungeon_map_ids = kDungeonMaps;
    options.dungeon_map_count = 2;
    options.reward_claimed = true;
    options.return_to_outpost = &RecordReturnToOutpost;
    options.explicit_return_delay_ms = 0u;
    options.long_transition_timeout_ms = 0u;
    options.long_load_timeout_ms = 0u;
    options.fallback_recovery_map_id = 0u;

    const auto result = GWA3::DungeonRuntime::HandlePostRewardReturn(options);

    GWA3_ASSERT(result.explicit_return_attempted);
    GWA3_ASSERT_EQ(g_returnToOutpostCount, 1u);
    GWA3_ASSERT(!result.returned_expected_map);
    GWA3_ASSERT_EQ(result.final_map_id, 616u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_runtime_post_reward_return
