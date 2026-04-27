#pragma once

#include <gwa3/dungeon/DungeonRoute.h>

namespace GWA3::Bot::Froggy {

inline constexpr int SPARKFLY_TO_DUNGEON_COUNT = 9;
inline constexpr int BOGROOT_LVL1_COUNT = 28;
inline constexpr int BOGROOT_LVL2_COUNT = 36;

extern const DungeonRoute::Waypoint SPARKFLY_TO_DUNGEON[SPARKFLY_TO_DUNGEON_COUNT];
extern const DungeonRoute::Waypoint BOGROOT_LVL1[BOGROOT_LVL1_COUNT];
extern const DungeonRoute::Waypoint BOGROOT_LVL2[BOGROOT_LVL2_COUNT];

} // namespace GWA3::Bot::Froggy
