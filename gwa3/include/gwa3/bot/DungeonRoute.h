#pragma once

#include <cstdint>

namespace GWA3::Bot::DungeonRoute {

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

WaypointLabelKind ClassifyWaypointLabel(const char* label);
bool TryParseWaypointOrdinal(const char* label, int& outOrdinal);
bool IsCheckpointWaypointLabel(WaypointLabelKind kind);
int ClampWaypointIndex(int index, int count);
int FindNearestWaypointIndex(const Waypoint* waypoints, int count, float x, float y);
int ComputeGenericRestartIndex(int nearestIndex, int backtrackCount = 2);
int ComputeStuckBacktrackIndex(int nearestIndex, int backtrackCount = 1);
bool ShouldTriggerStuckBacktrack(int unchangedNearestCount, int threshold = 5);
int ComputeRestartIndexFromRules(
    const Waypoint* waypoints,
    int count,
    int nearestIndex,
    const RestartRule* rules,
    int ruleCount,
    int defaultBacktrackCount = 2);

} // namespace GWA3::Bot::DungeonRoute
