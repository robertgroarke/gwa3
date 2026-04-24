#include <gwa3/bot/DungeonCheckpoint.h>
#include <gwa3/testing/TestFramework.h>

using namespace GWA3::Bot::DungeonCheckpoint;

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

GWA3_TEST(dungeon_checkpoint_abort_policy_detection, {
    const CheckpointRetryPolicy policy = {
        "Quest Checkpoint",
        CheckpointFailureAction::AbortRun,
        0,
        0,
    };

    GWA3_ASSERT(IsAbortPolicy(policy));
})

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
})

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
})

GWA3_TEST(dungeon_checkpoint_resolution_reports_unhandled_failure_without_policy, {
    const auto resolution = EvaluateCheckpointResolution(9, 8, 34, nullptr);

    GWA3_ASSERT(!resolution.passed);
    GWA3_ASSERT_EQ(static_cast<int>(resolution.action), static_cast<int>(CheckpointFailureAction::None));
    GWA3_ASSERT_EQ(resolution.retry_index, 9);
})
