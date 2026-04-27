#pragma once

#include <cstdint>

namespace GWA3::DungeonRoute {

struct Waypoint {
    float x = 0.0f;
    float y = 0.0f;
    float fight_range = 0.0f;
    const char* label = "";
};

enum class WaypointLabelKind : uint8_t {
    Empty,
    Numeric,
    Blessing,
    LevelTransition,
    DungeonKey,
    DungeonDoor,
    DungeonDoorCheckpoint,
    QuestCheckpoint,
    QuestDoorCheckpoint,
    Boss,
    BossLock,
    BossLockCheckpoint,
    Chest,
    Signpost,
    Keg,
    BlastDoor,
    BlastDoorCheckpoint,
    AsuraFlameStaff,
    StaffCheck,
    SpiderEggs,
    Unknown,
};

struct RestartRule {
    int first_ordinal = 0;
    int last_ordinal = 0;
    int restart_index = 0;
};

struct WaypointProgressState {
    int last_nearest_index = 0;
    int unchanged_nearest_count = 0;
};

struct WaypointProgressDecision {
    bool backtrack = false;
    int backtrack_index = 0;
};

template <typename Plan, typename ResolveBehaviorFn>
Plan MakeWaypointExecutionPlan(
    const Waypoint* waypoints,
    int count,
    int waypointIndex,
    ResolveBehaviorFn resolveBehavior) {
    Plan plan;
    if (waypoints == nullptr || waypointIndex < 0 || waypointIndex >= count) {
        return plan;
    }

    plan.waypoint = &waypoints[waypointIndex];
    plan.behavior = resolveBehavior(plan.waypoint->label);
    return plan;
}

using MoveWaypointFn = void(*)(float x, float y, float value);
using IsMapLoadedFn = bool(*)();

WaypointLabelKind ClassifyWaypointLabel(const char* label);
bool TryParseWaypointOrdinal(const char* label, int& outOrdinal);
bool IsCheckpointWaypointLabel(WaypointLabelKind kind);
int ClampWaypointIndex(int index, int count);
int FindNearestWaypointIndex(const Waypoint* waypoints, int count, float x, float y);
int ComputeGenericRestartIndex(int nearestIndex, int backtrackCount = 2);
int ComputeStuckBacktrackIndex(int nearestIndex, int backtrackCount = 1);
bool ShouldTriggerStuckBacktrack(int unchangedNearestCount, int threshold = 5);
WaypointProgressState MakeWaypointProgressState(int startIndex);
WaypointProgressDecision EvaluateWaypointProgress(
    int currentNearestIndex,
    WaypointProgressState& state,
    int stuckThreshold = 5,
    int backtrackCount = 1);
int ComputeRestartIndexFromRules(
    const Waypoint* waypoints,
    int count,
    int nearestIndex,
    const RestartRule* rules,
    int ruleCount,
    int defaultBacktrackCount = 2);
void ExecuteWaypointTravel(
    const Waypoint& waypoint,
    MoveWaypointFn moveToPoint,
    MoveWaypointFn aggroMoveToPoint,
    IsMapLoadedFn isMapLoaded,
    float moveThreshold = 250.0f);
void ExecuteReverseWaypointTraversal(
    const Waypoint* waypoints,
    int startIndex,
    int endIndex,
    MoveWaypointFn moveToPoint,
    MoveWaypointFn aggroMoveToPoint,
    IsMapLoadedFn isMapLoaded,
    float moveThreshold = 250.0f);

} // namespace GWA3::DungeonRoute
