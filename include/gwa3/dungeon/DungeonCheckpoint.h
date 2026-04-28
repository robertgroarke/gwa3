#pragma once

#include <gwa3/dungeon/DungeonRoute.h>

#include <cstdint>

namespace GWA3::DungeonCheckpoint {

using WaitFn = void(*)(uint32_t ms);
using BoolFn = bool(*)();
using VoidFn = void(*)();
using NearestWaypointIndexFn = int(*)(const DungeonRoute::Waypoint* waypoints, int waypoint_count);
using WaypointMoveFn = bool(*)(const DungeonRoute::Waypoint& waypoint);
using WaypointMoveWithContextFn = bool(*)(
    const DungeonRoute::Waypoint& waypoint,
    const void* context);
using WaypointReplayVisitFn = void(*)(
    const DungeonRoute::Waypoint* waypoints,
    int waypoint_count,
    int waypoint_index);

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

enum class WaypointWipeRecoveryContext : uint8_t {
    Standard,
    DungeonDoorCheckpoint,
    QuestDoorCheckpoint,
};

struct AdvanceCheckpointDecision {
    bool passed = true;
    CheckpointFailureAction action = CheckpointFailureAction::None;
    int backtrack_start = 0;
    int retry_index = 0;
    bool retry_from_nearest_after_backtrack = false;
};

struct WaypointWipeRecoveryOptions {
    int nearest_index = 0;
    int waypoint_count = 0;
    int backtrack_steps = 2;
    uint32_t* wipe_count = nullptr;
    uint32_t revive_timeout_ms = 120000u;
    uint32_t revive_poll_ms = 500u;
    uint32_t dp_removal_wipe_threshold = 2u;
    uint32_t post_dp_wait_ms = 500u;
    BoolFn is_dead = nullptr;
    WaitFn wait_ms = nullptr;
    VoidFn return_to_outpost = nullptr;
    VoidFn use_dp_removal = nullptr;
};

struct WaypointWipeRecoveryResult {
    bool recovered = false;
    bool returned_to_outpost = false;
    bool used_dp_removal = false;
    int restart_index = 0;
    uint32_t wipe_count = 0u;
};

struct RouteWipeRecoveryOptions {
    const DungeonRoute::Waypoint* waypoints = nullptr;
    int waypoint_count = 0;
    int current_index = 0;
    WaypointWipeRecoveryContext context = WaypointWipeRecoveryContext::Standard;
    const char* log_prefix = "Dungeon";
    WaypointWipeRecoveryOptions recovery = {};
};

struct RouteWipeRecoveryResult {
    bool recovered = false;
    bool returned_to_outpost = false;
    bool used_dp_removal = false;
    int restart_index = 0;
    uint32_t wipe_count = 0u;
};

struct LockedDoorCheckpointOptions {
    const DungeonRoute::Waypoint* waypoints = nullptr;
    int waypoint_count = 0;
    int current_index = 0;
    int backtrack_steps = 3;
    bool move_before_check = true;
    const char* log_prefix = "Dungeon";
    const char* checkpoint_name = "Dungeon Door Checkpoint";
    WaypointMoveFn move_waypoint = nullptr;
    NearestWaypointIndexFn get_nearest_waypoint = nullptr;
    WaypointReplayVisitFn after_backtrack_waypoint = nullptr;
};

struct LockedDoorCheckpointResult {
    bool completed = false;
    bool backtracked = false;
    int nearest_index = 0;
    int waypoint_index = 0;
};

struct CheckpointBacktrackReplayOptions {
    const DungeonRoute::Waypoint* waypoints = nullptr;
    int waypoint_count = 0;
    int current_index = 0;
    int backtrack_start = 0;
    WaypointMoveFn move_waypoint = nullptr;
    WaypointMoveWithContextFn move_waypoint_with_context = nullptr;
    const void* move_context = nullptr;
    WaypointReplayVisitFn after_move_waypoint = nullptr;
};

struct CheckpointBacktrackReplayResult {
    bool completed = false;
    int failed_index = -1;
    int visited_count = 0;
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
AdvanceCheckpointDecision EvaluateLabeledAdvanceCheckpointProgress(
    const char* label,
    int currentIndex,
    int nearestIndex,
    int waypointCount,
    const CheckpointRetryPolicy* policies,
    int policyCount);
int ResolveAdvanceCheckpointRetryIndex(
    const AdvanceCheckpointDecision& decision,
    int nearestIndex);
WaypointWipeRecoveryResult RecoverWaypointWipe(
    const WaypointWipeRecoveryOptions& options);
RouteWipeRecoveryResult RecoverRouteWaypointWipe(
    const RouteWipeRecoveryOptions& options);
CheckpointBacktrackReplayResult ReplayCheckpointBacktrack(
    const CheckpointBacktrackReplayOptions& options);
LockedDoorCheckpointResult HandleLockedDoorCheckpoint(
    const LockedDoorCheckpointOptions& options);

} // namespace GWA3::DungeonCheckpoint
