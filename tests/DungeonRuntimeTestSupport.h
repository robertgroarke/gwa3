#pragma once

#include <gwa3/dungeon/DungeonRuntime.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::TestStubs::AgentMgr {
void ResetAgents();
void SetPlayerAgent(float x, float y, float hp);
uint32_t MoveCount();
float LastMoveX();
float LastMoveY();
} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::MapMgr {
void Reset();
void SetMapId(uint32_t mapId);
void SetMoveSetsMapIdOnPoint(float x, float y, uint32_t mapId);
} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::Tests::DungeonRuntime {

inline uint32_t g_stageMoveCount = 0;
inline float g_lastStageX = 0.0f;
inline float g_lastStageY = 0.0f;
inline float g_lastStageThreshold = 0.0f;

inline bool RecordStageMove(float x, float y, float threshold) {
    ++g_stageMoveCount;
    g_lastStageX = x;
    g_lastStageY = y;
    g_lastStageThreshold = threshold;
    GWA3::AgentMgr::Move(x, y);
    return true;
}

inline void AgentMoveToPoint(float x, float y, float) {
    GWA3::AgentMgr::Move(x, y);
}

inline void AgentQueueMove(float x, float y) {
    GWA3::AgentMgr::Move(x, y);
}

inline void NoWait(uint32_t) {
}

inline void ResetStageMoveRecorder() {
    g_stageMoveCount = 0;
    g_lastStageX = 0.0f;
    g_lastStageY = 0.0f;
    g_lastStageThreshold = 0.0f;
}

inline void ArrangeTransitionFromBogrootToLevel2() {
    GWA3::TestStubs::MapMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::MapMgr::SetMapId(615u);
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::MapMgr::SetMoveSetsMapIdOnPoint(13097.0f, 26393.0f, 616u);
}

} // namespace GWA3::Tests::DungeonRuntime
