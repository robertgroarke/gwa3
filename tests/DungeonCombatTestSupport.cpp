#include "DungeonCombatTestSupport.h"

#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/game/Agent.h>

namespace GWA3::Tests::DungeonCombatSupport {
namespace {

uint32_t g_pickup_count = 0u;
uint32_t g_last_fight_target = 0u;
bool g_enemy_cleared = false;
bool g_combat_route_retry_blocked_second_waypoint = false;
bool g_combat_route_retry_unlocked_second_waypoint = false;
uint32_t g_delayed_move_calls = 0u;
uint32_t g_delayed_move_release_after = 0u;
bool g_combat_progress_floor_failure_armed = false;
bool g_combat_progress_floor_third_waypoint_unlocked = false;
bool g_combat_progress_floor_wrong_backtrack = false;
uint32_t g_waypoint_hook_call_count = 0u;
uint32_t g_waypoint_hook_before_count = 0u;
uint32_t g_waypoint_hook_after_count = 0u;
int g_waypoint_hook_last_index = -1;
int g_waypoint_hook_last_label_kind = -1;
GWA3::DungeonCombat::AggroWaypointPhase g_waypoint_hook_last_phase =
    GWA3::DungeonCombat::AggroWaypointPhase::BeforeAdvance;
bool g_waypoint_hook_failed_second_before = false;

} // namespace

bool AliveAndLoaded() {
    return true;
}

bool AliveFalse() {
    return false;
}

void WaitNoop(uint32_t) {
}

void QueueMoveForCombat(float x, float y) {
    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveForAggroAdvance(float x, float y) {
    if (!g_enemy_cleared && static_cast<int>(x) == 500 && static_cast<int>(y) == 0) {
        GWA3::AgentMgr::Move(40.0f, 0.0f);
        return;
    }
    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveWithAggroBacktrack(float x, float y) {
    const int targetX = static_cast<int>(x);
    const int targetY = static_cast<int>(y);
    if (targetX == 100 && targetY == 0) {
        if (g_combat_route_retry_blocked_second_waypoint) {
            g_combat_route_retry_unlocked_second_waypoint = true;
        }
        GWA3::AgentMgr::Move(100.0f, 0.0f);
        return;
    }

    if (targetX == 200 && targetY == 0 && !g_combat_route_retry_unlocked_second_waypoint) {
        g_combat_route_retry_blocked_second_waypoint = true;
        return;
    }

    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveWithDelayedRelease(float x, float y) {
    ++g_delayed_move_calls;
    if (g_delayed_move_calls <= g_delayed_move_release_after) {
        return;
    }
    GWA3::AgentMgr::Move(x, y);
}

void QueueMoveWithAggroProgressFloorRetry(float x, float y) {
    const int targetX = static_cast<int>(x);
    const int targetY = static_cast<int>(y);
    if (targetY != 0) {
        GWA3::AgentMgr::Move(x, y);
        return;
    }

    if (targetX == 100) {
        if (g_combat_progress_floor_failure_armed && !g_combat_progress_floor_third_waypoint_unlocked) {
            g_combat_progress_floor_wrong_backtrack = true;
        }
        GWA3::AgentMgr::Move(100.0f, 0.0f);
        return;
    }

    if (targetX == 200) {
        if (g_combat_progress_floor_failure_armed) {
            g_combat_progress_floor_third_waypoint_unlocked = true;
        }
        GWA3::AgentMgr::Move(200.0f, 0.0f);
        return;
    }

    if (targetX == 300 && !g_combat_progress_floor_third_waypoint_unlocked) {
        g_combat_progress_floor_failure_armed = true;
        GWA3::AgentMgr::Move(110.0f, 0.0f);
        return;
    }

    GWA3::AgentMgr::Move(x, y);
}

int RecordPickup(float) {
    ++g_pickup_count;
    return 1;
}

bool RecordWaypointHook(
    const GWA3::DungeonRoute::Waypoint&,
    int waypointIndex,
    GWA3::DungeonRoute::WaypointLabelKind labelKind,
    GWA3::DungeonCombat::AggroWaypointPhase phase,
    void*) {
    ++g_waypoint_hook_call_count;
    g_waypoint_hook_last_index = waypointIndex;
    g_waypoint_hook_last_label_kind = static_cast<int>(labelKind);
    g_waypoint_hook_last_phase = phase;
    if (phase == GWA3::DungeonCombat::AggroWaypointPhase::BeforeAdvance) {
        ++g_waypoint_hook_before_count;
    } else {
        ++g_waypoint_hook_after_count;
    }
    return true;
}

bool FailSecondWaypointBeforeOnce(
    const GWA3::DungeonRoute::Waypoint& waypoint,
    int waypointIndex,
    GWA3::DungeonRoute::WaypointLabelKind labelKind,
    GWA3::DungeonCombat::AggroWaypointPhase phase,
    void* userData) {
    if (!RecordWaypointHook(waypoint, waypointIndex, labelKind, phase, userData)) {
        return false;
    }

    if (waypointIndex == 1 &&
        phase == GWA3::DungeonCombat::AggroWaypointPhase::BeforeAdvance &&
        !g_waypoint_hook_failed_second_before) {
        g_waypoint_hook_failed_second_before = true;
        return false;
    }

    return true;
}

void KillTargetOnFight(uint32_t targetId) {
    g_last_fight_target = targetId;
    g_enemy_cleared = true;
    auto* agent = GWA3::AgentMgr::GetAgentByID(targetId);
    if (agent && agent->type == 0xDBu) {
        static_cast<GWA3::AgentLiving*>(agent)->hp = 0.0f;
    }
}

void FightTargetWithBuiltinCombatAndKill(uint32_t targetId) {
    GWA3::DungeonBuiltinCombat::FightTargetWithBuiltinCombat(targetId);
    KillTargetOnFight(targetId);
}

void ResetCombatRecorder() {
    g_pickup_count = 0u;
    g_last_fight_target = 0u;
    g_enemy_cleared = false;
    g_combat_route_retry_blocked_second_waypoint = false;
    g_combat_route_retry_unlocked_second_waypoint = false;
    g_delayed_move_calls = 0u;
    g_delayed_move_release_after = 0u;
    g_combat_progress_floor_failure_armed = false;
    g_combat_progress_floor_third_waypoint_unlocked = false;
    g_combat_progress_floor_wrong_backtrack = false;
    g_waypoint_hook_call_count = 0u;
    g_waypoint_hook_before_count = 0u;
    g_waypoint_hook_after_count = 0u;
    g_waypoint_hook_last_index = -1;
    g_waypoint_hook_last_label_kind = -1;
    g_waypoint_hook_last_phase = GWA3::DungeonCombat::AggroWaypointPhase::BeforeAdvance;
    g_waypoint_hook_failed_second_before = false;
}

void SetDelayedMoveReleaseAfter(uint32_t releaseAfter) {
    g_delayed_move_release_after = releaseAfter;
}

uint32_t PickupCount() {
    return g_pickup_count;
}

uint32_t LastFightTarget() {
    return g_last_fight_target;
}

bool EnemyCleared() {
    return g_enemy_cleared;
}

uint32_t DelayedMoveCalls() {
    return g_delayed_move_calls;
}

uint32_t DelayedMoveReleaseAfter() {
    return g_delayed_move_release_after;
}

bool CombatProgressFloorWrongBacktrack() {
    return g_combat_progress_floor_wrong_backtrack;
}

uint32_t WaypointHookCallCount() {
    return g_waypoint_hook_call_count;
}

uint32_t WaypointHookBeforeCount() {
    return g_waypoint_hook_before_count;
}

uint32_t WaypointHookAfterCount() {
    return g_waypoint_hook_after_count;
}

int WaypointHookLastIndex() {
    return g_waypoint_hook_last_index;
}

int WaypointHookLastLabelKind() {
    return g_waypoint_hook_last_label_kind;
}

GWA3::DungeonCombat::AggroWaypointPhase WaypointHookLastPhase() {
    return g_waypoint_hook_last_phase;
}

bool WaypointHookFailedSecondBefore() {
    return g_waypoint_hook_failed_second_before;
}

} // namespace GWA3::Tests::DungeonCombatSupport
