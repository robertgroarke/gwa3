// Consolidated test module generated from small test files.
#include "DungeonCombatTestSupport.h"
#include <gwa3/testing/TestFramework.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/game/Agent.h>

// --- tests/test_dungeon_combat_follow_waypoints_backtrack.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_backtrack {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_backtracks_after_timeout, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveWithAggroBacktrack;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {100.0f, 0.0f, 0.0f, "1"},
        {200.0f, 0.0f, 0.0f, "2"},
        {300.0f, 0.0f, 0.0f, "3"},
    };

    GWA3::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;
    options.max_backtrack_retries = 1;
    options.backtrack_count = 1;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.stuck_recovery_threshold = 1000;
    aggroOptions.stuck_abort_threshold = 1000;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_backtrack


// --- tests/test_dungeon_combat_follow_waypoints_hook_failure.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_hook_failure {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_retries_after_waypoint_hook_failure, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {100.0f, 0.0f, 0.0f, "1"},
        {200.0f, 0.0f, 0.0f, "Staff Check"},
        {300.0f, 0.0f, 0.0f, "3"},
    };

    GWA3::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;
    options.max_backtrack_retries = 1;
    options.backtrack_count = 1;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    AggroWaypointCallbacks waypointCallbacks;
    waypointCallbacks.on_waypoint = &FailSecondWaypointBeforeOnce;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions,
        waypointCallbacks);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT(WaypointHookFailedSecondBefore());
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_hook_failure


// --- tests/test_dungeon_combat_follow_waypoints_hooks.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_hooks {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_invokes_waypoint_hooks_before_and_after_arrival, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {500.0f, 0.0f, 0.0f, "Asura Flame Staff"},
    };

    GWA3::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    AggroWaypointCallbacks waypointCallbacks;
    waypointCallbacks.on_waypoint = &RecordWaypointHook;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions,
        waypointCallbacks);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(WaypointHookCallCount(), 2u);
    GWA3_ASSERT_EQ(WaypointHookBeforeCount(), 1u);
    GWA3_ASSERT_EQ(WaypointHookAfterCount(), 1u);
    GWA3_ASSERT_EQ(WaypointHookLastIndex(), 0);
    GWA3_ASSERT_EQ(
        WaypointHookLastLabelKind(),
        static_cast<int>(GWA3::DungeonRoute::WaypointLabelKind::AsuraFlameStaff));
    GWA3_ASSERT_EQ(static_cast<int>(WaypointHookLastPhase()), static_cast<int>(AggroWaypointPhase::AfterAdvance));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_hooks


// --- tests/test_dungeon_combat_follow_waypoints_progress_floor.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_progress_floor {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_preserves_progress_floor_when_player_drifts_backward, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveWithAggroProgressFloorRetry;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {100.0f, 0.0f, 0.0f, "1"},
        {200.0f, 0.0f, 0.0f, "2"},
        {300.0f, 0.0f, 0.0f, "3"},
    };

    GWA3::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 20.0f;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;
    options.max_backtrack_retries = 1;
    options.backtrack_count = 1;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.stuck_recovery_threshold = 1000;
    aggroOptions.stuck_abort_threshold = 1000;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.retries_used, 1);
    GWA3_ASSERT(!CombatProgressFloorWrongBacktrack());
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 300);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_progress_floor


// --- tests/test_dungeon_combat_follow_waypoints_tolerance.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_tolerance {


namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;
namespace PartyStubs = GWA3::TestStubs::PartyMgr;
using namespace GWA3::DungeonCombat;
using namespace GWA3::Tests::DungeonCombatSupport;

GWA3_TEST(dungeon_combat_follow_waypoints_with_aggro_keeps_arrival_tolerance_separate_from_fight_range, {
    AgentStubs::ResetAgents();
    MapStubs::Reset();
    MapStubs::SetMapId(615u);
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    PartyStubs::ResetFlags();
    ResetCombatRecorder();

    CombatCallbacks callbacks;
    callbacks.is_dead = &AliveFalse;
    callbacks.is_map_loaded = &AliveAndLoaded;
    callbacks.wait_ms = &WaitNoop;
    callbacks.queue_move = &QueueMoveForCombat;
    callbacks.fight_target = &KillTargetOnFight;

    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {500.0f, 0.0f, 1000.0f, "1"},
    };

    GWA3::DungeonNavigation::RouteFollowOptions options;
    options.default_tolerance = 250.0f;
    options.use_waypoint_fight_range_as_tolerance = false;
    options.waypoint_timeout_ms = 1000u;
    options.reissue_ms = 0u;

    AggroAdvanceOptions aggroOptions;
    aggroOptions.move_wait_ms = 0u;
    aggroOptions.clear_options.quiet_confirmation_ms = 0u;
    aggroOptions.clear_options.loop_wait_ms = 0u;
    aggroOptions.clear_options.idle_wait_ms = 0u;
    aggroOptions.clear_options.chase_wait_ms = 0u;
    aggroOptions.clear_options.pickup_after_clear = false;

    const auto result = FollowWaypointsWithAggro(
        waypoints,
        static_cast<int>(sizeof(waypoints) / sizeof(waypoints[0])),
        615u,
        callbacks,
        options,
        aggroOptions);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT(!result.map_changed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(AgentStubs::MoveCount(), 1u);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveX()), 500);
    GWA3_ASSERT_EQ(static_cast<int>(AgentStubs::LastMoveY()), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_combat_follow_waypoints_tolerance
