#include <gwa3/dungeon/DungeonRoute.h>

#include <cmath>
#include <cstring>

namespace GWA3::DungeonRoute {

namespace {

bool IsNullOrEmpty(const char* label) {
    return label == nullptr || label[0] == '\0';
}

bool StartsWith(const char* value, const char* prefix) {
    if (IsNullOrEmpty(value) || IsNullOrEmpty(prefix)) {
        return false;
    }

    const std::size_t prefixLength = std::strlen(prefix);
    return std::strncmp(value, prefix, prefixLength) == 0;
}

float DistanceSquared(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

} // namespace

WaypointLabelKind ClassifyWaypointLabel(const char* label) {
    if (IsNullOrEmpty(label)) {
        return WaypointLabelKind::Empty;
    }

    int ordinal = 0;
    if (TryParseWaypointOrdinal(label, ordinal)) {
        (void)ordinal;
        return WaypointLabelKind::Numeric;
    }

    if (std::strcmp(label, "Blessing") == 0) {
        return WaypointLabelKind::Blessing;
    }
    if (std::strcmp(label, "Lvl1 to Lvl2") == 0) {
        return WaypointLabelKind::LevelTransition;
    }
    if (std::strcmp(label, "Dungeon Key") == 0) {
        return WaypointLabelKind::DungeonKey;
    }
    if (std::strcmp(label, "Dungeon Door") == 0) {
        return WaypointLabelKind::DungeonDoor;
    }
    if (std::strcmp(label, "Dungeon Door Checkpoint") == 0) {
        return WaypointLabelKind::DungeonDoorCheckpoint;
    }
    if (std::strcmp(label, "Quest Checkpoint") == 0) {
        return WaypointLabelKind::QuestCheckpoint;
    }
    if (std::strcmp(label, "Quest Door Checkpoint") == 0) {
        return WaypointLabelKind::QuestDoorCheckpoint;
    }
    if (std::strcmp(label, "Boss lock") == 0) {
        return WaypointLabelKind::BossLock;
    }
    if (std::strcmp(label, "Boss Lock Checkpoint") == 0) {
        return WaypointLabelKind::BossLockCheckpoint;
    }
    if (std::strcmp(label, "Chest") == 0) {
        return WaypointLabelKind::Chest;
    }
    if (std::strcmp(label, "Signpost") == 0) {
        return WaypointLabelKind::Signpost;
    }
    if (std::strcmp(label, "Keg") == 0) {
        return WaypointLabelKind::Keg;
    }
    if (std::strcmp(label, "Blast Door") == 0) {
        return WaypointLabelKind::BlastDoor;
    }
    if (std::strcmp(label, "Asura Flame Staff") == 0) {
        return WaypointLabelKind::AsuraFlameStaff;
    }
    if (std::strcmp(label, "Staff Check") == 0) {
        return WaypointLabelKind::StaffCheck;
    }
    if (StartsWith(label, "Boss Lock Checkpoint") || StartsWith(label, "Boss lock Checkpoint")) {
        return WaypointLabelKind::BossLockCheckpoint;
    }
    if (StartsWith(label, "Boss")) {
        return WaypointLabelKind::Boss;
    }
    if (StartsWith(label, "Blast Door Checkpoint")) {
        return WaypointLabelKind::BlastDoorCheckpoint;
    }
    if (StartsWith(label, "Spider Eggs")) {
        return WaypointLabelKind::SpiderEggs;
    }

    return WaypointLabelKind::Unknown;
}

bool TryParseWaypointOrdinal(const char* label, int& outOrdinal) {
    outOrdinal = 0;
    if (IsNullOrEmpty(label)) {
        return false;
    }

    int value = 0;
    for (int i = 0; label[i] != '\0'; ++i) {
        const char c = label[i];
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }

    outOrdinal = value;
    return true;
}

bool IsCheckpointWaypointLabel(WaypointLabelKind kind) {
    return kind == WaypointLabelKind::DungeonDoorCheckpoint
        || kind == WaypointLabelKind::QuestCheckpoint
        || kind == WaypointLabelKind::QuestDoorCheckpoint
        || kind == WaypointLabelKind::BossLockCheckpoint
        || kind == WaypointLabelKind::BlastDoorCheckpoint;
}

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

int FindNearestWaypointIndex(const Waypoint* waypoints, int count, float x, float y) {
    if (waypoints == nullptr || count <= 0) {
        return 0;
    }

    int bestIndex = 0;
    float bestDistanceSq = DistanceSquared(x, y, waypoints[0].x, waypoints[0].y);
    for (int i = 1; i < count; ++i) {
        const float distanceSq = DistanceSquared(x, y, waypoints[i].x, waypoints[i].y);
        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestIndex = i;
        }
    }
    return bestIndex;
}

int ComputeGenericRestartIndex(int nearestIndex, int backtrackCount) {
    return ClampWaypointIndex(nearestIndex - backtrackCount, nearestIndex + 1);
}

int ComputeStuckBacktrackIndex(int nearestIndex, int backtrackCount) {
    return ClampWaypointIndex(nearestIndex - backtrackCount, nearestIndex + 1);
}

bool ShouldTriggerStuckBacktrack(int unchangedNearestCount, int threshold) {
    return unchangedNearestCount >= threshold;
}

WaypointProgressState MakeWaypointProgressState(int startIndex) {
    WaypointProgressState state;
    state.last_nearest_index = startIndex;
    return state;
}

WaypointProgressDecision EvaluateWaypointProgress(
    int currentNearestIndex,
    WaypointProgressState& state,
    int stuckThreshold,
    int backtrackCount) {
    WaypointProgressDecision decision;
    if (currentNearestIndex == state.last_nearest_index) {
        ++state.unchanged_nearest_count;
        if (ShouldTriggerStuckBacktrack(state.unchanged_nearest_count, stuckThreshold)) {
            decision.backtrack = true;
            decision.backtrack_index = ComputeStuckBacktrackIndex(currentNearestIndex, backtrackCount);
            state.unchanged_nearest_count = 0;
        }
        return decision;
    }

    state.last_nearest_index = currentNearestIndex;
    state.unchanged_nearest_count = 0;
    return decision;
}

int ComputeRestartIndexFromRules(
    const Waypoint* waypoints,
    int count,
    int nearestIndex,
    const RestartRule* rules,
    int ruleCount,
    int defaultBacktrackCount) {
    const int clampedNearest = ClampWaypointIndex(nearestIndex, count);
    if (waypoints == nullptr || count <= 0 || rules == nullptr || ruleCount <= 0) {
        return ComputeGenericRestartIndex(clampedNearest, defaultBacktrackCount);
    }

    int ordinal = 0;
    if (!TryParseWaypointOrdinal(waypoints[clampedNearest].label, ordinal)) {
        return ComputeGenericRestartIndex(clampedNearest, defaultBacktrackCount);
    }

    for (int i = 0; i < ruleCount; ++i) {
        if (ordinal >= rules[i].first_ordinal && ordinal <= rules[i].last_ordinal) {
            return ClampWaypointIndex(rules[i].restart_index, count);
        }
    }

    return ComputeGenericRestartIndex(clampedNearest, defaultBacktrackCount);
}

void ExecuteWaypointTravel(
    const Waypoint& waypoint,
    MoveWaypointFn moveToPoint,
    MoveWaypointFn aggroMoveToPoint,
    IsMapLoadedFn isMapLoaded,
    float moveThreshold) {
    if (moveToPoint == nullptr || aggroMoveToPoint == nullptr || isMapLoaded == nullptr) {
        return;
    }

    if (waypoint.fight_range > 0.0f && isMapLoaded()) {
        aggroMoveToPoint(waypoint.x, waypoint.y, waypoint.fight_range);
        return;
    }

    moveToPoint(waypoint.x, waypoint.y, moveThreshold);
}

void ExecuteReverseWaypointTraversal(
    const Waypoint* waypoints,
    int startIndex,
    int endIndex,
    MoveWaypointFn moveToPoint,
    MoveWaypointFn aggroMoveToPoint,
    IsMapLoadedFn isMapLoaded,
    float moveThreshold) {
    if (waypoints == nullptr || startIndex < endIndex) {
        return;
    }

    for (int index = startIndex; index >= endIndex; --index) {
        ExecuteWaypointTravel(
            waypoints[index],
            moveToPoint,
            aggroMoveToPoint,
            isMapLoaded,
            moveThreshold);
    }
}

} // namespace GWA3::DungeonRoute
