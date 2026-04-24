#pragma once

namespace GWA3::Bot::DungeonCheckpoint {

enum class CheckpointFailureAction {
    None,
    AbortRun,
    BacktrackRetry,
};

struct CheckpointRetryPolicy {
    const char* label = "";
    CheckpointFailureAction action = CheckpointFailureAction::None;
    int backtrack_steps = 0;
    int retry_index = 0;
};

struct CheckpointResolution {
    bool passed = true;
    CheckpointFailureAction action = CheckpointFailureAction::None;
    int backtrack_start = 0;
    int retry_index = 0;
};

const CheckpointRetryPolicy* FindCheckpointPolicy(
    const CheckpointRetryPolicy* policies,
    int count,
    const char* label);
int ComputeCheckpointBacktrackStart(int currentIndex, const CheckpointRetryPolicy& policy);
bool IsAbortPolicy(const CheckpointRetryPolicy& policy);
CheckpointResolution EvaluateCheckpointResolution(
    int currentIndex,
    int nearestIndex,
    int waypointCount,
    const CheckpointRetryPolicy* policy);
CheckpointResolution EvaluateAdvanceCheckpointResolution(
    int currentIndex,
    int nearestIndex,
    int waypointCount,
    const CheckpointRetryPolicy* policy);

} // namespace GWA3::Bot::DungeonCheckpoint
