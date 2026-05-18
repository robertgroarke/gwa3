// Consolidated dungeon test support runtime module.
// --- tests/DungeonInteractionsTestSupport.cpp ---
#include "DungeonInteractionsTestSupport.h"

#include <gwa3/managers/DialogMgr.h>

namespace GWA3::Tests::DungeonInteractionsSupport {

void NoInteractionWait(uint32_t) {
}

bool StopWhenNpcDialogOpen(uint32_t npcId, void*) {
    return GWA3::DialogMgr::IsDialogOpen() &&
           GWA3::DialogMgr::GetDialogSenderAgentId() == npcId;
}

} // namespace GWA3::Tests::DungeonInteractionsSupport

// --- tests/DungeonCombatRoutineTestSupport.cpp ---
#include "DungeonCombatRoutineTestSupport.h"

#include <gwa3/managers/AgentMgr.h>

namespace GWA3::Tests::DungeonCombatRoutineSupport {

void WaitNoopCombatRoutine(uint32_t) {
}

bool NotDead() {
    return false;
}

void AutoAttack(uint32_t targetId) {
    GWA3::AgentMgr::Attack(targetId);
}

GWA3::DungeonCombatRoutine::SkillExecutionContext MakeContext(
    GWA3::DungeonSkill::CachedSkill cache[8],
    bool used[8]) {
    GWA3::DungeonCombatRoutine::SkillExecutionContext context;
    context.skill_cache = cache;
    context.skill_used_this_step = used;
    context.skill_count = 8u;
    context.wait_ms = &WaitNoopCombatRoutine;
    context.is_dead = &NotDead;
    return context;
}

void ResetCombatRoutineStubs() {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    EffectStubs::Reset();
    PartyStubs::ResetFlags();
    PartyStubs::SetPartyDefeated(false);
    SkillStubs::Reset();
}

} // namespace GWA3::Tests::DungeonCombatRoutineSupport

// --- tests/DungeonLootTestSupport.cpp ---
#include "DungeonLootTestSupport.h"

namespace GWA3::Tests::DungeonLootSupport {
namespace {

uint32_t g_combatMoveCount = 0;
uint32_t g_pickupNearbyCount = 0;
uint32_t g_openDoorCount = 0;
float g_lastCombatMoveX = 0.0f;
float g_lastCombatMoveY = 0.0f;
float g_lastCombatMoveRange = 0.0f;
float g_lastPickupRange = 0.0f;
float g_lastDoorX = 0.0f;
float g_lastDoorY = 0.0f;
bool g_openDoorResult = false;
uint32_t g_bundleOpenCount = 0;
bool g_bundleOpenResult = false;

} // namespace

void MoveNear(float, float, float) {
}

void ResetBossKeyCallbacks() {
    g_combatMoveCount = 0;
    g_pickupNearbyCount = 0;
    g_openDoorCount = 0;
    g_lastCombatMoveX = 0.0f;
    g_lastCombatMoveY = 0.0f;
    g_lastCombatMoveRange = 0.0f;
    g_lastPickupRange = 0.0f;
    g_lastDoorX = 0.0f;
    g_lastDoorY = 0.0f;
    g_openDoorResult = false;
    g_bundleOpenCount = 0;
    g_bundleOpenResult = false;
}

void RecordBossKeyCombatMove(float x, float y, float fightRange) {
    ++g_combatMoveCount;
    g_lastCombatMoveX = x;
    g_lastCombatMoveY = y;
    g_lastCombatMoveRange = fightRange;
}

int RecordBossKeyPickupNearbyLoot(float maxRange) {
    ++g_pickupNearbyCount;
    g_lastPickupRange = maxRange;
    return 0;
}

bool RecordBossKeyOpenDoor(float x, float y) {
    ++g_openDoorCount;
    g_lastDoorX = x;
    g_lastDoorY = y;
    return g_openDoorResult;
}

bool RecordBundleOpen(float, float, float) {
    ++g_bundleOpenCount;
    return g_bundleOpenResult;
}

void SetBossKeyOpenDoorResult(bool result) {
    g_openDoorResult = result;
}

void SetBundleOpenResult(bool result) {
    g_bundleOpenResult = result;
}

uint32_t CombatMoveCount() {
    return g_combatMoveCount;
}

uint32_t PickupNearbyCount() {
    return g_pickupNearbyCount;
}

uint32_t OpenDoorCount() {
    return g_openDoorCount;
}

uint32_t BundleOpenCount() {
    return g_bundleOpenCount;
}

float LastCombatMoveX() {
    return g_lastCombatMoveX;
}

float LastCombatMoveY() {
    return g_lastCombatMoveY;
}

float LastCombatMoveRange() {
    return g_lastCombatMoveRange;
}

float LastPickupRange() {
    return g_lastPickupRange;
}

float LastDoorX() {
    return g_lastDoorX;
}

float LastDoorY() {
    return g_lastDoorY;
}

} // namespace GWA3::Tests::DungeonLootSupport

// --- tests/DungeonCheckpointTestSupport.cpp ---
#include "DungeonCheckpointTestSupport.h"

#include <gwa3/dungeon/DungeonRoute.h>

namespace GWA3::Tests::DungeonCheckpointSupport {
namespace {

bool g_checkpointDead = false;
uint32_t g_checkpointWaitCount = 0u;
uint32_t g_checkpointReturnToOutpostCount = 0u;
uint32_t g_checkpointDpRemovalCount = 0u;
int g_checkpointMoveCount = 0;
int g_checkpointMovedOrdinals[8] = {};
int g_checkpointMoveFailureOrdinal = -1;
int g_checkpointReplayVisitCount = 0;
int g_checkpointVisitedIndexes[8] = {};

} // namespace

bool CheckpointIsDead() {
    return g_checkpointDead;
}

void CheckpointWait(uint32_t) {
    ++g_checkpointWaitCount;
}

void CheckpointReturnToOutpost() {
    ++g_checkpointReturnToOutpostCount;
}

void CheckpointUseDpRemoval() {
    ++g_checkpointDpRemovalCount;
}

void ResetCheckpointRecoveryCallbacks() {
    g_checkpointDead = false;
    g_checkpointWaitCount = 0u;
    g_checkpointReturnToOutpostCount = 0u;
    g_checkpointDpRemovalCount = 0u;
}

void SetCheckpointDead(bool dead) {
    g_checkpointDead = dead;
}

uint32_t CheckpointReturnToOutpostCount() {
    return g_checkpointReturnToOutpostCount;
}

uint32_t CheckpointDpRemovalCount() {
    return g_checkpointDpRemovalCount;
}

void ResetCheckpointBacktrackReplay() {
    g_checkpointMoveCount = 0;
    g_checkpointMoveFailureOrdinal = -1;
    g_checkpointReplayVisitCount = 0;
    for (int& ordinal : g_checkpointMovedOrdinals) {
        ordinal = 0;
    }
    for (int& index : g_checkpointVisitedIndexes) {
        index = -1;
    }
}

void SetCheckpointMoveFailureOrdinal(int ordinal) {
    g_checkpointMoveFailureOrdinal = ordinal;
}

int CheckpointMoveCount() {
    return g_checkpointMoveCount;
}

int CheckpointMovedOrdinal(int index) {
    return g_checkpointMovedOrdinals[index];
}

int CheckpointReplayVisitCount() {
    return g_checkpointReplayVisitCount;
}

int CheckpointVisitedIndex(int index) {
    return g_checkpointVisitedIndexes[index];
}

bool MoveCheckpointWaypoint(const GWA3::DungeonRoute::Waypoint& waypoint) {
    int ordinal = 0;
    GWA3::DungeonRoute::TryParseWaypointOrdinal(waypoint.label, ordinal);
    if (g_checkpointMoveCount < static_cast<int>(sizeof(g_checkpointMovedOrdinals) / sizeof(g_checkpointMovedOrdinals[0]))) {
        g_checkpointMovedOrdinals[g_checkpointMoveCount] = ordinal;
    }
    ++g_checkpointMoveCount;
    return ordinal != g_checkpointMoveFailureOrdinal;
}

bool MoveCheckpointWaypointWithContext(
    const GWA3::DungeonRoute::Waypoint& waypoint,
    const void* context) {
    const int* blockedOrdinal = static_cast<const int*>(context);
    int ordinal = 0;
    GWA3::DungeonRoute::TryParseWaypointOrdinal(waypoint.label, ordinal);
    if (g_checkpointMoveCount < static_cast<int>(sizeof(g_checkpointMovedOrdinals) / sizeof(g_checkpointMovedOrdinals[0]))) {
        g_checkpointMovedOrdinals[g_checkpointMoveCount] = ordinal;
    }
    ++g_checkpointMoveCount;
    return blockedOrdinal == nullptr || ordinal != *blockedOrdinal;
}

void RecordCheckpointReplayVisit(
    const GWA3::DungeonRoute::Waypoint*,
    int,
    int waypointIndex) {
    if (g_checkpointReplayVisitCount < static_cast<int>(sizeof(g_checkpointVisitedIndexes) / sizeof(g_checkpointVisitedIndexes[0]))) {
        g_checkpointVisitedIndexes[g_checkpointReplayVisitCount] = waypointIndex;
    }
    ++g_checkpointReplayVisitCount;
}

} // namespace GWA3::Tests::DungeonCheckpointSupport
