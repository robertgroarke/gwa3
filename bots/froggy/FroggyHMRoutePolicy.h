#pragma once

#include <gwa3/dungeon/DungeonRoute.h>
#include <bots/froggy/FroggyHMConfig.h>

namespace GWA3::Bot::Froggy {

inline int ResolveRouteStartIndex(
    uint32_t mapId,
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int nearestIndex,
    float distanceFromLvl1Portal = -1.0f) {
    if (waypoints == nullptr || count <= 0 || nearestIndex < 0) {
        return 0;
    }

    if (mapId == MapIds::BOGROOT_GROWTHS_LVL1 &&
        count > 1 &&
        nearestIndex == 1 &&
        DungeonRoute::ClassifyWaypointLabel(waypoints[1].label) == DungeonRoute::WaypointLabelKind::Blessing) {
        return 0;
    }

    if (mapId == MapIds::BOGROOT_GROWTHS_LVL2 &&
        distanceFromLvl1Portal >= 0.0f &&
        distanceFromLvl1Portal < BOGROOT_LVL1_TO_LVL2_HANDOFF_CLEARANCE &&
        nearestIndex >= 20) {
        return 0;
    }

    return nearestIndex < count ? nearestIndex : count - 1;
}

} // namespace GWA3::Bot::Froggy
