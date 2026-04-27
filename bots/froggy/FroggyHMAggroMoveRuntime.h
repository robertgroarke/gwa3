// Froggy movement and aggro traversal runtime helpers. Included by FroggyHM.cpp.

static bool MoveToAndWait(float x, float y, float threshold = DungeonNavigation::MOVE_TO_DEFAULT_THRESHOLD) {
    if (IsDead()) {
        return false;
    }

    const auto result = DungeonNavigation::MoveToAndWait(
        x,
        y,
        threshold,
        DungeonNavigation::MOVE_TO_TIMEOUT_MS,
        DungeonNavigation::MOVE_TO_POLL_MS);
    if (!result.arrived) {
        const float finalDist = DungeonCombat::DistanceToPoint(x, y);
        Log::Warn("Froggy: MoveToAndWait timeout target=(%.0f, %.0f) dist=%.0f threshold=%.0f map=%u loaded=%d",
                  x,
                  y,
                  finalDist,
                  threshold,
                  MapMgr::GetMapId(),
                  MapMgr::GetIsMapLoaded() ? 1 : 0);
    }
    return result.arrived;
}

#include "FroggyHMAggroMoveBogrootLoop.h"
#include "FroggyHMAggroMoveStandardLoop.h"

static void AggroMoveToEx(float x, float y, float fightRange = DungeonCombat::AGGRO_DEFAULT_FIGHT_RANGE) {
    LogBot("AggroMoveToEx start target=(%.0f, %.0f) fightRange=%.0f", x, y, fightRange);
    const bool sparkflyMap = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    const bool bogrootMap = IsBogrootMap();
    if (bogrootMap) {
        AggroMoveToBogroot(x, y, fightRange);
        return;
    }
    AggroMoveToStandard(x, y, fightRange, sparkflyMap, bogrootMap);
}
