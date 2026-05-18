#pragma once

#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/testing/TestFramework.h>

#include <cstdint>

namespace GWA3::Tests::DungeonCheckpointSupport {

bool CheckpointIsDead();
void CheckpointWait(uint32_t ms);
void CheckpointReturnToOutpost();
void CheckpointUseDpRemoval();

void ResetCheckpointRecoveryCallbacks();
void SetCheckpointDead(bool dead);
uint32_t CheckpointReturnToOutpostCount();
uint32_t CheckpointDpRemovalCount();

void ResetCheckpointBacktrackReplay();
void SetCheckpointMoveFailureOrdinal(int ordinal);
int CheckpointMoveCount();
int CheckpointMovedOrdinal(int index);
int CheckpointReplayVisitCount();
int CheckpointVisitedIndex(int index);

bool MoveCheckpointWaypoint(const GWA3::DungeonRoute::Waypoint& waypoint);
bool MoveCheckpointWaypointWithContext(
    const GWA3::DungeonRoute::Waypoint& waypoint,
    const void* context);
void RecordCheckpointReplayVisit(
    const GWA3::DungeonRoute::Waypoint* waypoint,
    int waypointCount,
    int waypointIndex);

} // namespace GWA3::Tests::DungeonCheckpointSupport
