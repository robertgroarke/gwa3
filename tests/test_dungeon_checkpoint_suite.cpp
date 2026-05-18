// Consolidated test module generated from small test files.
#include "DungeonCheckpointTestSupport.h"

// --- tests/test_dungeon_checkpoint_abort_policy.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_abort_policy {

using namespace GWA3::DungeonCheckpoint;

GWA3_TEST(dungeon_checkpoint_abort_policy_detection, {
    const CheckpointRetryPolicy policy = {
        "Quest Checkpoint",
        CheckpointFailureAction::AbortRun,
        0,
        0,
    };

    GWA3_ASSERT(IsAbortPolicy(policy));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_abort_policy

// --- tests/test_dungeon_checkpoint_backtrack_start.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_backtrack_start {

using namespace GWA3::DungeonCheckpoint;

GWA3_TEST(dungeon_checkpoint_backtrack_start_clamps_to_zero, {
    const CheckpointRetryPolicy policy = {
        "Blast Door Checkpoint 2",
        CheckpointFailureAction::BacktrackRetry,
        5,
        25,
    };

    GWA3_ASSERT_EQ(ComputeCheckpointBacktrackStart(10, policy), 5);
    GWA3_ASSERT_EQ(ComputeCheckpointBacktrackStart(3, policy), 0);
    GWA3_ASSERT(!IsAbortPolicy(policy));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_backtrack_start

// --- tests/test_dungeon_checkpoint_find_policy.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_find_policy {

using namespace GWA3::DungeonCheckpoint;

GWA3_TEST(dungeon_checkpoint_find_policy_by_label, {
    const CheckpointRetryPolicy policies[] = {
        {"Quest Checkpoint", CheckpointFailureAction::AbortRun, 0, 0},
        {"Blast Door Checkpoint 1", CheckpointFailureAction::BacktrackRetry, 4, 3},
    };

    const auto* first = FindCheckpointPolicy(policies, 2, "Quest Checkpoint");
    const auto* second = FindCheckpointPolicy(policies, 2, "Blast Door Checkpoint 1");
    const auto* missing = FindCheckpointPolicy(policies, 2, "Missing");

    GWA3_ASSERT(first != nullptr);
    GWA3_ASSERT(second != nullptr);
    GWA3_ASSERT(missing == nullptr);
    GWA3_ASSERT_EQ(static_cast<int>(first->action), static_cast<int>(CheckpointFailureAction::AbortRun));
    GWA3_ASSERT_EQ(second->retry_index, 3);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_find_policy

// --- tests/test_dungeon_checkpoint_recover_waypoint_backtrack.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_recover_waypoint_backtrack {

using namespace GWA3::DungeonCheckpoint;
namespace CheckpointSupport = GWA3::Tests::DungeonCheckpointSupport;

GWA3_TEST(dungeon_checkpoint_recover_waypoint_wipe_backtracks_from_nearest, {
    CheckpointSupport::ResetCheckpointRecoveryCallbacks();
    uint32_t wipeCount = 0u;

    WaypointWipeRecoveryOptions options;
    options.nearest_index = 7;
    options.waypoint_count = 12;
    options.backtrack_steps = 2;
    options.wipe_count = &wipeCount;
    options.is_dead = &CheckpointSupport::CheckpointIsDead;
    options.wait_ms = &CheckpointSupport::CheckpointWait;
    options.return_to_outpost = &CheckpointSupport::CheckpointReturnToOutpost;
    options.use_dp_removal = &CheckpointSupport::CheckpointUseDpRemoval;

    const auto result = RecoverWaypointWipe(options);

    GWA3_ASSERT(result.recovered);
    GWA3_ASSERT_EQ(result.restart_index, 5);
    GWA3_ASSERT_EQ(result.wipe_count, 1u);
    GWA3_ASSERT_EQ(wipeCount, 1u);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointReturnToOutpostCount(), 0u);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointDpRemovalCount(), 0u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_recover_waypoint_backtrack

// --- tests/test_dungeon_checkpoint_recover_waypoint_dp.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_recover_waypoint_dp {

using namespace GWA3::DungeonCheckpoint;
namespace CheckpointSupport = GWA3::Tests::DungeonCheckpointSupport;

GWA3_TEST(dungeon_checkpoint_recover_waypoint_wipe_uses_dp_after_repeated_wipes, {
    CheckpointSupport::ResetCheckpointRecoveryCallbacks();
    uint32_t wipeCount = 1u;

    WaypointWipeRecoveryOptions options;
    options.nearest_index = 1;
    options.waypoint_count = 12;
    options.backtrack_steps = 3;
    options.wipe_count = &wipeCount;
    options.post_dp_wait_ms = 0u;
    options.is_dead = &CheckpointSupport::CheckpointIsDead;
    options.use_dp_removal = &CheckpointSupport::CheckpointUseDpRemoval;

    const auto result = RecoverWaypointWipe(options);

    GWA3_ASSERT(result.recovered);
    GWA3_ASSERT(result.used_dp_removal);
    GWA3_ASSERT_EQ(result.restart_index, 0);
    GWA3_ASSERT_EQ(result.wipe_count, 2u);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointDpRemovalCount(), 1u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_recover_waypoint_dp

// --- tests/test_dungeon_checkpoint_recover_waypoint_outpost.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_recover_waypoint_outpost {

using namespace GWA3::DungeonCheckpoint;
namespace CheckpointSupport = GWA3::Tests::DungeonCheckpointSupport;

GWA3_TEST(dungeon_checkpoint_recover_waypoint_wipe_returns_outpost_on_defeat_timeout, {
    CheckpointSupport::ResetCheckpointRecoveryCallbacks();
    CheckpointSupport::SetCheckpointDead(true);
    uint32_t wipeCount = 0u;

    WaypointWipeRecoveryOptions options;
    options.nearest_index = 7;
    options.waypoint_count = 12;
    options.wipe_count = &wipeCount;
    options.revive_timeout_ms = 0u;
    options.is_dead = &CheckpointSupport::CheckpointIsDead;
    options.return_to_outpost = &CheckpointSupport::CheckpointReturnToOutpost;

    const auto result = RecoverWaypointWipe(options);

    GWA3_ASSERT(!result.recovered);
    GWA3_ASSERT(result.returned_to_outpost);
    GWA3_ASSERT_EQ(result.wipe_count, 1u);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointReturnToOutpostCount(), 1u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_recover_waypoint_outpost

// --- tests/test_dungeon_checkpoint_route_wipe_forward_nearest.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_route_wipe_forward_nearest {

using namespace GWA3::DungeonCheckpoint;

static int g_nearestIndex = 0;

static int TestNearestWaypoint(
    const GWA3::DungeonRoute::Waypoint*,
    int) {
    return g_nearestIndex;
}

GWA3_TEST(dungeon_checkpoint_route_wipe_resumes_forward_when_respawn_is_ahead, {
    uint32_t wipeCount = 0u;
    g_nearestIndex = 17;
    GWA3::DungeonRoute::Waypoint waypoints[20] = {};

    RouteWipeRecoveryOptions options;
    options.waypoints = waypoints;
    options.waypoint_count = 20;
    options.current_index = 15;
    options.log_prefix = "Test";
    options.recovery.nearest_index = 17;
    options.recovery.backtrack_steps = 2;
    options.recovery.wipe_count = &wipeCount;
    options.recovery.get_nearest_waypoint = &TestNearestWaypoint;

    const auto result = RecoverRouteWaypointWipe(options);

    GWA3_ASSERT(result.recovered);
    GWA3_ASSERT_EQ(result.restart_index, 17);
    GWA3_ASSERT_EQ(result.wipe_count, 1u);
    GWA3_ASSERT_EQ(wipeCount, 1u);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_route_wipe_forward_nearest

// --- tests/test_dungeon_checkpoint_replay_backtrack_context.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_replay_backtrack_context {

using namespace GWA3::DungeonCheckpoint;
namespace CheckpointSupport = GWA3::Tests::DungeonCheckpointSupport;

GWA3_TEST(dungeon_checkpoint_replay_backtrack_supports_move_context, {
    CheckpointSupport::ResetCheckpointBacktrackReplay();
    const int blockedOrdinal = 2;
    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "2"},
        {200.0f, 0.0f, 0.0f, "3"},
    };

    CheckpointBacktrackReplayOptions options;
    options.waypoints = waypoints;
    options.waypoint_count = 3;
    options.current_index = 2;
    options.backtrack_start = 0;
    options.move_waypoint_with_context = &CheckpointSupport::MoveCheckpointWaypointWithContext;
    options.move_context = &blockedOrdinal;

    const auto result = ReplayCheckpointBacktrack(options);

    GWA3_ASSERT(!result.completed);
    GWA3_ASSERT_EQ(result.failed_index, 1);
    GWA3_ASSERT_EQ(result.visited_count, 0);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMoveCount(), 1);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMovedOrdinal(0), 2);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_replay_backtrack_context

// --- tests/test_dungeon_checkpoint_replay_backtrack_failure.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_replay_backtrack_failure {

using namespace GWA3::DungeonCheckpoint;
namespace CheckpointSupport = GWA3::Tests::DungeonCheckpointSupport;

GWA3_TEST(dungeon_checkpoint_replay_backtrack_stops_on_move_failure, {
    CheckpointSupport::ResetCheckpointBacktrackReplay();
    CheckpointSupport::SetCheckpointMoveFailureOrdinal(3);
    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "2"},
        {200.0f, 0.0f, 0.0f, "3"},
        {300.0f, 0.0f, 0.0f, "4"},
        {400.0f, 0.0f, 0.0f, "5"},
    };

    CheckpointBacktrackReplayOptions options;
    options.waypoints = waypoints;
    options.waypoint_count = 5;
    options.current_index = 4;
    options.backtrack_start = 1;
    options.move_waypoint = &CheckpointSupport::MoveCheckpointWaypoint;

    const auto result = ReplayCheckpointBacktrack(options);

    GWA3_ASSERT(!result.completed);
    GWA3_ASSERT_EQ(result.failed_index, 2);
    GWA3_ASSERT_EQ(result.visited_count, 1);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMoveCount(), 2);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMovedOrdinal(0), 4);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMovedOrdinal(1), 3);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_replay_backtrack_failure

// --- tests/test_dungeon_checkpoint_replay_backtrack_reverse.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_replay_backtrack_reverse {

using namespace GWA3::DungeonCheckpoint;
namespace CheckpointSupport = GWA3::Tests::DungeonCheckpointSupport;

GWA3_TEST(dungeon_checkpoint_replay_backtrack_visits_prior_waypoints_in_reverse, {
    CheckpointSupport::ResetCheckpointBacktrackReplay();
    const GWA3::DungeonRoute::Waypoint waypoints[] = {
        {0.0f, 0.0f, 0.0f, "1"},
        {100.0f, 0.0f, 0.0f, "2"},
        {200.0f, 0.0f, 0.0f, "3"},
        {300.0f, 0.0f, 0.0f, "4"},
        {400.0f, 0.0f, 0.0f, "5"},
    };

    CheckpointBacktrackReplayOptions options;
    options.waypoints = waypoints;
    options.waypoint_count = 5;
    options.current_index = 4;
    options.backtrack_start = 1;
    options.move_waypoint = &CheckpointSupport::MoveCheckpointWaypoint;
    options.after_move_waypoint = &CheckpointSupport::RecordCheckpointReplayVisit;

    const auto result = ReplayCheckpointBacktrack(options);

    GWA3_ASSERT(result.completed);
    GWA3_ASSERT_EQ(result.failed_index, -1);
    GWA3_ASSERT_EQ(result.visited_count, 3);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMoveCount(), 3);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMovedOrdinal(0), 4);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMovedOrdinal(1), 3);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointMovedOrdinal(2), 2);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointReplayVisitCount(), 3);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointVisitedIndex(0), 3);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointVisitedIndex(1), 2);
    GWA3_ASSERT_EQ(CheckpointSupport::CheckpointVisitedIndex(2), 1);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_replay_backtrack_reverse

// --- tests/test_dungeon_checkpoint_resolution_abort.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_resolution_abort {

using namespace GWA3::DungeonCheckpoint;

GWA3_TEST(dungeon_checkpoint_resolution_aborts_failed_quest_checkpoint, {
    const CheckpointRetryPolicy policy = {
        "Quest Checkpoint",
        CheckpointFailureAction::AbortRun,
        0,
        0,
    };

    const auto resolution = EvaluateCheckpointResolution(2, 1, 34, &policy);

    GWA3_ASSERT(!resolution.passed);
    GWA3_ASSERT_EQ(static_cast<int>(resolution.action), static_cast<int>(CheckpointFailureAction::AbortRun));
    GWA3_ASSERT_EQ(resolution.backtrack_start, 2);
    GWA3_ASSERT_EQ(resolution.retry_index, 0);

    const CheckpointRetryPolicy labeledPolicies[] = {
        policy,
    };
    const auto labeled = EvaluateLabeledAdvanceCheckpointProgress(
        "Quest Checkpoint",
        2,
        2,
        34,
        labeledPolicies,
        1);

    GWA3_ASSERT(!labeled.passed);
    GWA3_ASSERT_EQ(static_cast<int>(labeled.action), static_cast<int>(CheckpointFailureAction::AbortRun));
    GWA3_ASSERT(!labeled.retry_from_nearest_after_backtrack);
    GWA3_ASSERT_EQ(ResolveAdvanceCheckpointRetryIndex(labeled, 1), 0);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_resolution_abort

// --- tests/test_dungeon_checkpoint_resolution_match.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_resolution_match {

using namespace GWA3::DungeonCheckpoint;

GWA3_TEST(dungeon_checkpoint_resolution_passes_when_checkpoint_matches, {
    const CheckpointRetryPolicy policy = {
        "Blast Door Checkpoint 1",
        CheckpointFailureAction::BacktrackRetry,
        4,
        3,
    };

    const auto resolution = EvaluateCheckpointResolution(9, 9, 34, &policy);

    GWA3_ASSERT(resolution.passed);
    GWA3_ASSERT_EQ(static_cast<int>(resolution.action), static_cast<int>(CheckpointFailureAction::None));
    GWA3_ASSERT_EQ(resolution.retry_index, 9);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_resolution_match

// --- tests/test_dungeon_checkpoint_resolution_retry_backtrack.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_resolution_retry_backtrack {

using namespace GWA3::DungeonCheckpoint;

GWA3_TEST(dungeon_checkpoint_resolution_computes_retry_backtrack, {
    const CheckpointRetryPolicy policy = {
        "Blast Door Checkpoint 2",
        CheckpointFailureAction::BacktrackRetry,
        5,
        25,
    };

    const auto resolution = EvaluateCheckpointResolution(31, 30, 34, &policy);

    GWA3_ASSERT(!resolution.passed);
    GWA3_ASSERT_EQ(static_cast<int>(resolution.action), static_cast<int>(CheckpointFailureAction::BacktrackRetry));
    GWA3_ASSERT_EQ(resolution.backtrack_start, 26);
    GWA3_ASSERT_EQ(resolution.retry_index, 25);

    const CheckpointRetryPolicy advancePolicy = {
        "Dungeon Door Checkpoint",
        CheckpointFailureAction::BacktrackRetry,
        3,
        4,
    };

    const auto passed = EvaluateAdvanceCheckpointResolution(4, 5, 12, &advancePolicy);
    const auto failed = EvaluateAdvanceCheckpointResolution(4, 4, 12, &advancePolicy);

    GWA3_ASSERT(passed.passed);
    GWA3_ASSERT_EQ(static_cast<int>(passed.action), static_cast<int>(CheckpointFailureAction::None));

    GWA3_ASSERT(!failed.passed);
    GWA3_ASSERT_EQ(static_cast<int>(failed.action), static_cast<int>(CheckpointFailureAction::BacktrackRetry));
    GWA3_ASSERT_EQ(failed.backtrack_start, 1);
    GWA3_ASSERT_EQ(failed.retry_index, 4);

    const CheckpointRetryPolicy labeledPolicies[] = {
        advancePolicy,
    };
    const auto labeled = EvaluateLabeledAdvanceCheckpointProgress(
        "Dungeon Door Checkpoint",
        4,
        4,
        12,
        labeledPolicies,
        1);

    GWA3_ASSERT(!labeled.passed);
    GWA3_ASSERT_EQ(static_cast<int>(labeled.action), static_cast<int>(CheckpointFailureAction::BacktrackRetry));
    GWA3_ASSERT(labeled.retry_from_nearest_after_backtrack);
    GWA3_ASSERT_EQ(ResolveAdvanceCheckpointRetryIndex(labeled, 4), 3);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_resolution_retry_backtrack

// --- tests/test_dungeon_checkpoint_resolution_unhandled.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_resolution_unhandled {

using namespace GWA3::DungeonCheckpoint;

GWA3_TEST(dungeon_checkpoint_resolution_reports_unhandled_failure_without_policy, {
    const auto resolution = EvaluateCheckpointResolution(9, 8, 34, nullptr);

    GWA3_ASSERT(!resolution.passed);
    GWA3_ASSERT_EQ(static_cast<int>(resolution.action), static_cast<int>(CheckpointFailureAction::None));
    GWA3_ASSERT_EQ(resolution.retry_index, 9);
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_checkpoint_resolution_unhandled
