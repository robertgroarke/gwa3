#pragma once

#include <gwa3/dungeon/DungeonRoute.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::Tests::DungeonRoute {

inline float g_lastMoveX = 0.0f;
inline float g_lastMoveY = 0.0f;
inline float g_lastMoveThreshold = 0.0f;
inline int g_moveCallCount = 0;
inline float g_moveHistoryX[16] = {};
inline float g_moveHistoryY[16] = {};
inline float g_moveHistoryValue[16] = {};
inline int g_moveHistoryCount = 0;

inline void ResetMoveRecorder() {
    g_lastMoveX = 0.0f;
    g_lastMoveY = 0.0f;
    g_lastMoveThreshold = 0.0f;
    g_moveCallCount = 0;
    g_moveHistoryCount = 0;
}

inline void RecordMoveToPoint(float x, float y, float threshold) {
    g_lastMoveX = x;
    g_lastMoveY = y;
    g_lastMoveThreshold = threshold;
    if (g_moveHistoryCount < 16) {
        g_moveHistoryX[g_moveHistoryCount] = x;
        g_moveHistoryY[g_moveHistoryCount] = y;
        g_moveHistoryValue[g_moveHistoryCount] = threshold;
    }
    ++g_moveHistoryCount;
    ++g_moveCallCount;
}

inline void RecordAggroMoveToPoint(float x, float y, float fightRange) {
    RecordMoveToPoint(x, y, fightRange);
}

inline bool MapLoadedTrue() {
    return true;
}

inline bool MapLoadedFalse() {
    return false;
}

} // namespace GWA3::Tests::DungeonRoute
