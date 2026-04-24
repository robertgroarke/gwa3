#include <gwa3/bot/DungeonCheckpoint.h>

#include <cstring>

namespace GWA3::Bot::DungeonCheckpoint {

namespace {

int ClampWaypointIndex(int index, int count) {
    if (count <= 0) {
        return 0;
    }
    if (index < 0) {
        return 0;
    }
    if (index >= count) {
        return count - 1;
    }
    return index;
}

} // namespace

const CheckpointRetryPolicy* FindCheckpointPolicy(
    const CheckpointRetryPolicy* policies,
    int count,
    const char* label) {
    if (policies == nullptr || count <= 0 || label == nullptr || label[0] == '\0') {
        return nullptr;
    }

    for (int i = 0; i < count; ++i) {
        if (std::strcmp(policies[i].label, label) == 0) {
            return &policies[i];
        }
    }

    return nullptr;
}

int ComputeCheckpointBacktrackStart(int currentIndex, const CheckpointRetryPolicy& policy) {
    int start = currentIndex - policy.backtrack_steps;
    if (start < 0) {
        start = 0;
    }
    return start;
}

bool IsAbortPolicy(const CheckpointRetryPolicy& policy) {
    return policy.action == CheckpointFailureAction::AbortRun;
}

CheckpointResolution EvaluateCheckpointResolution(
    int currentIndex,
    int nearestIndex,
    int waypointCount,
    const CheckpointRetryPolicy* policy) {
    CheckpointResolution resolution;
    const int clampedCurrent = ClampWaypointIndex(currentIndex, waypointCount);
    const int clampedNearest = ClampWaypointIndex(nearestIndex, waypointCount);
    resolution.retry_index = clampedCurrent;

    if (clampedCurrent == clampedNearest) {
        return resolution;
    }

    resolution.passed = false;
    if (policy == nullptr) {
        return resolution;
    }

    resolution.action = policy->action;
    resolution.backtrack_start = ComputeCheckpointBacktrackStart(clampedCurrent, *policy);
    resolution.retry_index = ClampWaypointIndex(policy->retry_index, waypointCount);
    return resolution;
}

CheckpointResolution EvaluateAdvanceCheckpointResolution(
    int currentIndex,
    int nearestIndex,
    int waypointCount,
    const CheckpointRetryPolicy* policy) {
    CheckpointResolution resolution;
    const int clampedCurrent = ClampWaypointIndex(currentIndex, waypointCount);
    const int clampedNearest = ClampWaypointIndex(nearestIndex, waypointCount);
    resolution.retry_index = clampedCurrent;

    if (clampedNearest > clampedCurrent) {
        return resolution;
    }

    resolution.passed = false;
    if (policy == nullptr) {
        return resolution;
    }

    resolution.action = policy->action;
    resolution.backtrack_start = ComputeCheckpointBacktrackStart(clampedCurrent, *policy);
    resolution.retry_index = ClampWaypointIndex(policy->retry_index, waypointCount);
    return resolution;
}

} // namespace GWA3::Bot::DungeonCheckpoint
