#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/core/Log.h>

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

const char* ContextName(WaypointWipeRecoveryContext context) {
    switch (context) {
    case WaypointWipeRecoveryContext::DungeonDoorCheckpoint:
        return "Dungeon Door Checkpoint";
    case WaypointWipeRecoveryContext::QuestDoorCheckpoint:
        return "Quest Door Checkpoint";
    default:
        return "Waypoint";
    }
}

int ResolveNearestWaypoint(
    const DungeonRoute::Waypoint* waypoints,
    int waypointCount,
    NearestWaypointIndexFn getNearestWaypoint) {
    return getNearestWaypoint ? getNearestWaypoint(waypoints, waypointCount) : 0;
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

    int nearestIndex = options.nearest_index;
    if (options.get_nearest_waypoint && options.waypoints && options.waypoint_count > 0) {
        nearestIndex = options.get_nearest_waypoint(options.waypoints, options.waypoint_count);
    }

    int restartIndex = nearestIndex - options.backtrack_steps;
    if (restartIndex < 0) {
        restartIndex = 0;
    }
    result.restart_index = ClampWaypointIndex(restartIndex, options.waypoint_count);
    result.recovered = true;
    return result;
}

RouteWipeRecoveryResult RecoverRouteWaypointWipe(
    const RouteWipeRecoveryOptions& options) {
    RouteWipeRecoveryResult result;
    const auto contextName = ContextName(options.context);
    const auto prefix = options.log_prefix ? options.log_prefix : "Dungeon";
    const auto* currentWaypoint =
        options.waypoints != nullptr &&
        options.current_index >= 0 &&
        options.current_index < options.waypoint_count
            ? &options.waypoints[options.current_index]
            : nullptr;
    const char* label = currentWaypoint && currentWaypoint->label ? currentWaypoint->label : "";
    Log::Info("%s: %s wipe detected at wp=%d(%s) nextWipe=%u",
              prefix,
              contextName,
              options.current_index,
              label,
              options.recovery.wipe_count ? *options.recovery.wipe_count + 1u : 1u);

    auto recoveryOptions = options.recovery;
    if (!recoveryOptions.waypoints) {
        recoveryOptions.waypoints = options.waypoints;
    }
    if (recoveryOptions.waypoint_count <= 0) {
        recoveryOptions.waypoint_count = options.waypoint_count;
    }
    const auto recovery = RecoverWaypointWipe(recoveryOptions);
    result.recovered = recovery.recovered;
    result.returned_to_outpost = recovery.returned_to_outpost;
    result.used_dp_removal = recovery.used_dp_removal;
    result.restart_index = recovery.restart_index;
    result.wipe_count = recovery.wipe_count;

    if (recovery.returned_to_outpost) {
        Log::Info("%s: %s party defeated - returning to outpost", prefix, contextName);
        return result;
    }
    if (recovery.used_dp_removal) {
        Log::Info("%s: %s multiple wipes (%u) - used DP removal",
                  prefix,
                  contextName,
                  recovery.wipe_count);
    }
    Log::Info("%s: %s resuming from checkpoint %d after wipe (was at %d)",
              prefix,
              contextName,
              recovery.restart_index,
              options.current_index);
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

LockedDoorCheckpointResult HandleLockedDoorCheckpoint(
    const LockedDoorCheckpointOptions& options) {
    LockedDoorCheckpointResult result;
    result.waypoint_index = options.current_index;
    if (options.waypoints == nullptr ||
        options.waypoint_count <= 0 ||
        options.current_index < 0 ||
        options.current_index >= options.waypoint_count ||
        options.move_waypoint == nullptr) {
        return result;
    }

    if (options.move_before_check) {
        (void)options.move_waypoint(options.waypoints[options.current_index]);
    }
    int nearest = ResolveNearestWaypoint(
        options.waypoints,
        options.waypoint_count,
        options.get_nearest_waypoint);
    result.nearest_index = nearest;

    const auto prefix = options.log_prefix ? options.log_prefix : "Dungeon";
    const auto checkpointName = options.checkpoint_name ? options.checkpoint_name : "Dungeon Door Checkpoint";
    if (nearest < options.current_index) {
        Log::Info("%s: %s failed at wp=%d nearest=%d; backtracking",
                  prefix,
                  checkpointName,
                  options.current_index,
                  nearest);
        CheckpointBacktrackReplayOptions replayOptions;
        replayOptions.waypoints = options.waypoints;
        replayOptions.waypoint_count = options.waypoint_count;
        replayOptions.current_index = options.current_index;
        replayOptions.backtrack_start = options.current_index - options.backtrack_steps;
        replayOptions.move_waypoint = options.move_waypoint;
        replayOptions.after_move_waypoint = options.after_backtrack_waypoint;
        (void)ReplayCheckpointBacktrack(replayOptions);
        nearest = ResolveNearestWaypoint(
            options.waypoints,
            options.waypoint_count,
            options.get_nearest_waypoint);
        result.nearest_index = nearest;
        result.waypoint_index = nearest;
        result.backtracked = true;
    }

    Log::Info("%s: %s reached at wp=%d nearest=%d",
              prefix,
              checkpointName,
              result.waypoint_index,
              result.nearest_index);
    result.completed = true;
    return result;
}

} // namespace GWA3::DungeonCheckpoint
