#pragma once

#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/testing/TestFramework.h>

#include <cstdint>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace MapStubs = GWA3::TestStubs::MapMgr;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddAgent(uint32_t agentId, float x, float y, uint32_t type);
void SetPlayerAgent(float x, float y, float hp);
void ClearPlayerAgent();
uint32_t MoveCount();
float LastMoveX();
float LastMoveY();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::MapMgr {

void Reset();
void SetMapId(uint32_t mapId);

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::Tests::DungeonNavigationSupport {

void ResetNavigationCallback();
void ResetRouteRetryMoveScript();
uint32_t NavigationCallbackMoves();
float LastWaypointMoveValue();
bool ProgressFloorWrongBacktrack();

void QueueMoveForTest(float x, float y);
void MoveWaypointForTest(float x, float y, float value);
bool NavigationMapLoadedTrue();
bool NavigationMapLoadedFalse();
void QueueMoveWithBacktrackRetry(float x, float y);
void QueueMoveWithProgressFloorRetry(float x, float y);
uint32_t ResolveNearestNpcForTest(float x, float y, float radius);
void AssertStuckMonitorBehavior();

} // namespace GWA3::Tests::DungeonNavigationSupport
