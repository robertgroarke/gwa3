#include <gwa3/dungeon/DungeonCheckpoint.h>

#include <Windows.h>

#include <cstring>

namespace GWA3::DungeonCheckpoint {

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

void WaitOrSleep(WaitFn waitFn, uint32_t ms) {
    if (waitFn) {
        waitFn(ms);
        return;
    }
    Sleep(ms);
}

bool IsDead(BoolFn isDeadFn) {
    return isDeadFn ? isDeadFn() : false;
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

AdvanceCheckpointDecision EvaluateLabeledAdvanceCheckpointProgress(
    const char* label,
    int currentIndex,
    int nearestIndex,
    int waypointCount,
    const CheckpointRetryPolicy* policies,
    int policyCount) {
    AdvanceCheckpointDecision decision;
    const auto* policy = FindCheckpointPolicy(policies, policyCount, label);
    if (policy == nullptr) {
        return decision;
    }

    const auto resolution = EvaluateAdvanceCheckpointResolution(
        currentIndex,
        nearestIndex,
        waypointCount,
        policy);
    decision.passed = resolution.passed;
    decision.action = resolution.action;
    decision.backtrack_start = resolution.backtrack_start;
    decision.retry_index = resolution.retry_index;
    decision.retry_from_nearest_after_backtrack =
        !resolution.passed && resolution.action == CheckpointFailureAction::BacktrackRetry;
    return decision;
}

int ResolveAdvanceCheckpointRetryIndex(
    const AdvanceCheckpointDecision& decision,
    int nearestIndex) {
    if (decision.retry_from_nearest_after_backtrack) {
        return nearestIndex > 0 ? nearestIndex - 1 : 0;
    }
    return decision.retry_index > 0 ? decision.retry_index : 0;
}

WaypointWipeRecoveryResult RecoverWaypointWipe(
    const WaypointWipeRecoveryOptions& options) {
    WaypointWipeRecoveryResult result = {};

    uint32_t localWipeCount = options.wipe_count ? *options.wipe_count : 0u;
    ++localWipeCount;
    if (options.wipe_count) {
        *options.wipe_count = localWipeCount;
    }
    result.wipe_count = localWipeCount;

    const DWORD reviveStart = GetTickCount();
    while (IsDead(options.is_dead) &&
           (GetTickCount() - reviveStart) < options.revive_timeout_ms) {
        WaitOrSleep(options.wait_ms, options.revive_poll_ms);
    }

    if (IsDead(options.is_dead)) {
        if (options.return_to_outpost) {
            options.return_to_outpost();
        }
        result.returned_to_outpost = true;
        return result;
    }

    if (localWipeCount >= options.dp_removal_wipe_threshold) {
        if (options.use_dp_removal) {
            options.use_dp_removal();
        }
        result.used_dp_removal = true;
        if (options.post_dp_wait_ms > 0u) {
            WaitOrSleep(options.wait_ms, options.post_dp_wait_ms);
        }
    }

    int restartIndex = options.nearest_index - options.backtrack_steps;
    if (restartIndex < 0) {
        restartIndex = 0;
    }
    result.restart_index = ClampWaypointIndex(restartIndex, options.waypoint_count);
    result.recovered = true;
    return result;
}

CheckpointBacktrackReplayResult ReplayCheckpointBacktrack(
    const CheckpointBacktrackReplayOptions& options) {
    CheckpointBacktrackReplayResult result;
    if (options.waypoints == nullptr ||
        options.waypoint_count <= 0 ||
        (options.move_waypoint == nullptr && options.move_waypoint_with_context == nullptr)) {
        result.failed_index = 0;
        return result;
    }

    const int currentIndex = ClampWaypointIndex(options.current_index, options.waypoint_count);
    const int backtrackStart = ClampWaypointIndex(options.backtrack_start, options.waypoint_count);
    for (int i = currentIndex - 1; i >= 0 && i >= backtrackStart; --i) {
        const bool moved = options.move_waypoint_with_context
            ? options.move_waypoint_with_context(options.waypoints[i], options.move_context)
            : options.move_waypoint(options.waypoints[i]);
        if (!moved) {
            result.failed_index = i;
            return result;
        }
        if (options.after_move_waypoint) {
            options.after_move_waypoint(options.waypoints, options.waypoint_count, i);
        }
        ++result.visited_count;
    }

    result.completed = true;
    return result;
}

} // namespace GWA3::DungeonCheckpoint
