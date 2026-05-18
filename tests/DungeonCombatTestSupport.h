#pragma once

#include <gwa3/dungeon/DungeonCombat.h>

#include <cstdint>

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
void SetPlayerAgent(float x, float y, float hp);
uint32_t MoveCount();
uint32_t ChangeTargetCount();
uint32_t CallTargetCount();
uint32_t AttackCount();
uint32_t LastChangedTargetId();
uint32_t LastCalledTargetId();
uint32_t LastAttackTargetId();
float LastMoveX();
float LastMoveY();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::MapMgr {

void Reset();
void SetMapId(uint32_t mapId);

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::TestStubs::PartyMgr {

void ResetFlags();
uint32_t FlagAllCount();
uint32_t UnflagAllCount();
float LastFlagAllX();
float LastFlagAllY();

} // namespace GWA3::TestStubs::PartyMgr

namespace GWA3::Tests::DungeonCombatSupport {

bool AliveAndLoaded();
bool AliveFalse();
void WaitNoop(uint32_t ms);

void QueueMoveForCombat(float x, float y);
void QueueMoveForAggroAdvance(float x, float y);
void QueueMoveWithAggroBacktrack(float x, float y);
void QueueMoveWithDelayedRelease(float x, float y);
void QueueMoveWithAggroProgressFloorRetry(float x, float y);

int RecordPickup(float range);
bool RecordWaypointHook(const GWA3::DungeonRoute::Waypoint& waypoint,
                        int waypointIndex,
                        GWA3::DungeonRoute::WaypointLabelKind labelKind,
                        GWA3::DungeonCombat::AggroWaypointPhase phase,
                        void* userData);
bool FailSecondWaypointBeforeOnce(const GWA3::DungeonRoute::Waypoint& waypoint,
                                  int waypointIndex,
                                  GWA3::DungeonRoute::WaypointLabelKind labelKind,
                                  GWA3::DungeonCombat::AggroWaypointPhase phase,
                                  void* userData);
void KillTargetOnFight(uint32_t targetId);
void FightTargetWithBuiltinCombatAndKill(uint32_t targetId);

void ResetCombatRecorder();
void SetDelayedMoveReleaseAfter(uint32_t releaseAfter);

uint32_t PickupCount();
uint32_t LastFightTarget();
bool EnemyCleared();
uint32_t DelayedMoveCalls();
uint32_t DelayedMoveReleaseAfter();
bool CombatProgressFloorWrongBacktrack();
uint32_t WaypointHookCallCount();
uint32_t WaypointHookBeforeCount();
uint32_t WaypointHookAfterCount();
int WaypointHookLastIndex();
int WaypointHookLastLabelKind();
GWA3::DungeonCombat::AggroWaypointPhase WaypointHookLastPhase();
bool WaypointHookFailedSecondBefore();

} // namespace GWA3::Tests::DungeonCombatSupport
