#pragma once

#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonRoute.h>
#include <bots/froggy/FroggyHMConfig.h>

#include <cstdint>

namespace GWA3::Bot::Froggy {

enum class WaypointBehavior : uint8_t {
    StandardMove,
    GrabBlessing,
    LevelTransition,
    AcquireDungeonKey,
    OpenDungeonDoor,
    ValidateDungeonDoorCheckpoint,
    ValidateQuestDoorCheckpoint,
    BossRewardSequence,
};

struct WaypointExecutionPlan {
    const DungeonRoute::Waypoint* waypoint = nullptr;
    WaypointBehavior behavior = WaypointBehavior::StandardMove;
};

using CheckpointDecision = DungeonCheckpoint::AdvanceCheckpointDecision;

namespace Detail {

inline DungeonCheckpoint::CheckpointRetryPolicy BuildDungeonDoorCheckpointPolicy(int currentIndex) {
    return {
        "Dungeon Door Checkpoint",
        DungeonCheckpoint::CheckpointFailureAction::BacktrackRetry,
        3,
        currentIndex,
    };
}

inline constexpr DungeonCheckpoint::CheckpointRetryPolicy QUEST_DOOR_CHECKPOINT_POLICY = {
    "Quest Door Checkpoint",
    DungeonCheckpoint::CheckpointFailureAction::AbortRun,
    0,
    0,
};

} // namespace Detail

inline WaypointBehavior ResolveWaypointBehavior(const char* label) {
    switch (DungeonRoute::ClassifyWaypointLabel(label)) {
    case DungeonRoute::WaypointLabelKind::Blessing:
        return WaypointBehavior::GrabBlessing;
    case DungeonRoute::WaypointLabelKind::LevelTransition:
        return WaypointBehavior::LevelTransition;
    case DungeonRoute::WaypointLabelKind::DungeonKey:
        return WaypointBehavior::AcquireDungeonKey;
    case DungeonRoute::WaypointLabelKind::DungeonDoor:
        return WaypointBehavior::OpenDungeonDoor;
    case DungeonRoute::WaypointLabelKind::DungeonDoorCheckpoint:
        return WaypointBehavior::ValidateDungeonDoorCheckpoint;
    case DungeonRoute::WaypointLabelKind::QuestDoorCheckpoint:
        return WaypointBehavior::ValidateQuestDoorCheckpoint;
    case DungeonRoute::WaypointLabelKind::Boss:
        return WaypointBehavior::BossRewardSequence;
    default:
        return WaypointBehavior::StandardMove;
    }
}

inline int ResolveRouteStartIndex(
    uint32_t mapId,
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int nearestIndex,
    float distanceFromLvl1Portal = -1.0f) {
    if (waypoints == nullptr || count <= 0 || nearestIndex < 0) {
        return 0;
    }

    if (mapId == MapIds::BOGROOT_GROWTHS_LVL1 &&
        count > 1 &&
        nearestIndex == 1 &&
        ResolveWaypointBehavior(waypoints[1].label) == WaypointBehavior::GrabBlessing) {
        return 0;
    }

    if (mapId == MapIds::BOGROOT_GROWTHS_LVL2 &&
        distanceFromLvl1Portal >= 0.0f &&
        distanceFromLvl1Portal < BOGROOT_LVL1_TO_LVL2_HANDOFF_CLEARANCE &&
        nearestIndex >= 20) {
        return 0;
    }

    return nearestIndex < count ? nearestIndex : count - 1;
}

inline WaypointExecutionPlan BuildWaypointExecutionPlan(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int waypointIndex) {
    return DungeonRoute::MakeWaypointExecutionPlan<WaypointExecutionPlan>(
        waypoints,
        count,
        waypointIndex,
        &ResolveWaypointBehavior);
}

inline int ComputeWipeRestartWaypoint(int nearestIndex) {
    return DungeonRoute::ComputeGenericRestartIndex(nearestIndex, 2);
}

inline int ComputeStuckBacktrackWaypoint(int nearestIndex) {
    return DungeonRoute::ComputeStuckBacktrackIndex(nearestIndex, 1);
}

inline bool ShouldBacktrackForStuck(int unchangedNearestCount) {
    return DungeonRoute::ShouldTriggerStuckBacktrack(unchangedNearestCount, 5);
}

inline CheckpointDecision EvaluateCheckpointProgress(
    const char* label,
    int currentIndex,
    int nearestIndex,
    int waypointCount) {
    const DungeonCheckpoint::CheckpointRetryPolicy policies[] = {
        Detail::BuildDungeonDoorCheckpointPolicy(currentIndex),
        Detail::QUEST_DOOR_CHECKPOINT_POLICY,
    };
    return DungeonCheckpoint::EvaluateLabeledAdvanceCheckpointProgress(
        label,
        currentIndex,
        nearestIndex,
        waypointCount,
        policies,
        static_cast<int>(sizeof(policies) / sizeof(policies[0])));
}

inline int ResolveCheckpointRetryIndex(
    const CheckpointDecision& decision,
    int nearestIndex) {
    return DungeonCheckpoint::ResolveAdvanceCheckpointRetryIndex(decision, nearestIndex);
}

} // namespace GWA3::Bot::Froggy
