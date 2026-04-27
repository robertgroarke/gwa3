#include <bots/kathandrax/Kathandrax.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/MapIds.h>

namespace GWA3::Bot::Kathandrax {

using DungeonRoute::Waypoint;
using DungeonQuest::BootstrapPlan;
using DungeonQuest::QuestNpcAnchor;
using DungeonQuest::TravelPoint;

namespace {

constexpr Waypoint kRunToDaladaWaypoints[] = {
    {-15024.0f, 16571.0f, 1200.0f, "1"},
    {-15968.0f, 14434.0f, 1200.0f, "2"},
    {-15366.0f, 13553.0f, 1200.0f, "3"},
};

constexpr Waypoint kRunToSacnothWaypoints[] = {
    {-13641.0f, 10164.0f, 1300.0f, "1"},
    {-12536.0f, 5741.0f, 1300.0f, "2"},
    {-14285.0f, -26.0f, 1300.0f, "3"},
    {-17673.0f, -3956.0f, 600.0f, "4"},
    {-14504.0f, -6379.0f, 600.0f, "5"},
    {-10694.0f, -10550.0f, 1300.0f, "6"},
    {-1782.0f, -14112.0f, 900.0f, "7"},
    {8055.0f, -13311.0f, 1300.0f, "8"},
    {14323.0f, -19865.0f, 1300.0f, "9"},
};

constexpr Waypoint kRunToDungeonWaypoints[] = {
    {10061.0f, 18371.0f, 1300.0f, "1"},
    {7563.0f, 12582.0f, 1300.0f, "2"},
    {8144.0f, 7969.0f, 1300.0f, "3"},
    {10846.0f, 2839.0f, 1300.0f, "4"},
    {14130.0f, -3499.0f, 1300.0f, "5"},
    {16459.0f, -9344.0f, 1300.0f, "6"},
    {17494.0f, -15161.0f, 1300.0f, "7"},
    {13096.0f, -13647.0f, 1300.0f, "8"},
    {14225.0f, -14446.0f, 1300.0f, "9"},
    {14025.0f, -16530.0f, 1300.0f, "10"},
    {17751.0f, -17964.0f, 1300.0f, "11"},
};

constexpr Waypoint kLevel1Waypoints[] = {
    {17853.0f, -18048.0f, 1200.0f, "1"},
    {16734.0f, -10111.0f, 1200.0f, "2"},
    {16941.0f, -2965.0f, 1200.0f, "3"},
    {15628.0f, 439.0f, 1200.0f, "4"},
    {12852.0f, -2162.0f, 1200.0f, "5"},
    {10288.0f, -5612.0f, 1200.0f, "6"},
    {5938.0f, -6211.0f, 1200.0f, "7"},
    {-1724.0f, -5183.0f, 1200.0f, "8"},
    {-2068.0f, -2802.0f, 1200.0f, "9"},
    {1950.0f, 213.0f, 1200.0f, "10"},
    {5145.0f, 1364.0f, 1200.0f, "11"},
    {4465.0f, 6236.0f, 1200.0f, "12"},
    {3684.0f, 2790.0f, 1200.0f, "Dungeon Key"},
    {1241.0f, 6668.0f, 1200.0f, "14"},
    {90.0f, -1122.0f, 1200.0f, "15"},
    {-6547.0f, -1781.0f, 1200.0f, "Boss lock"},
    {-6946.0f, 478.0f, 1200.0f, "Boss Lock Checkpoint"},
    {-11797.0f, 3835.0f, 1300.0f, "18"},
    {-14140.0f, 3302.0f, 1300.0f, "19"},
    {-16746.0f, -2814.0f, 1200.0f, "20"},
};

constexpr Waypoint kLevel2Waypoints[] = {
    {17201.0f, -2324.0f, 1200.0f, "1"},
    {14379.0f, 1130.0f, 1200.0f, "2"},
    {10013.0f, -1286.0f, 1200.0f, "3"},
    {10704.0f, -2756.0f, 1200.0f, "4"},
    {11237.0f, -3594.0f, 1200.0f, "5"},
    {10162.0f, -5583.0f, 1200.0f, "6"},
    {3107.0f, -11448.0f, 1200.0f, "7"},
    {-66.0f, -11246.0f, 1200.0f, "Dungeon Key"},
    {-682.0f, -13363.0f, 1200.0f, "9"},
    {-3744.0f, -11819.0f, 1200.0f, "10"},
    {-5227.0f, -9364.0f, 1200.0f, "11"},
    {-6481.0f, -9618.0f, 1200.0f, "Boss lock"},
    {-8198.0f, -10375.0f, 1200.0f, "Boss Lock Checkpoint 2"},
    {-14713.0f, -8298.0f, 1200.0f, "14"},
    {-12852.0f, -6194.0f, 450.0f, "15"},
    {-10642.0f, -5956.0f, 450.0f, "16"},
    {-12257.0f, -3054.0f, 1200.0f, "17"},
    {-15642.0f, -2485.0f, 1200.0f, "18"},
    {-16464.0f, -739.0f, 1200.0f, "19"},
};

constexpr Waypoint kLevel3Waypoints[] = {
    {-16944.0f, 10046.0f, 1200.0f, "1"},
    {-13710.0f, 13462.0f, 1200.0f, "2"},
    {-12144.0f, 15210.0f, 1200.0f, "3"},
    {-10295.0f, 15102.0f, 1200.0f, "4"},
    {-10963.0f, 12814.0f, 1200.0f, "Dungeon Key"},
    {-11305.0f, 12007.0f, 1200.0f, "Boss lock"},
    {-10070.0f, 10970.0f, 1200.0f, "Boss lock Checkpoint 3"},
    {-9080.0f, 7537.0f, 1200.0f, "8"},
    {-8597.0f, 1928.0f, 1200.0f, "9"},
    {-5929.0f, -935.0f, 1200.0f, "10"},
    {-7496.0f, -3463.0f, 1200.0f, "11"},
    {-4919.0f, -3174.0f, 1200.0f, "12"},
    {-3617.0f, -4359.0f, 1200.0f, "13"},
    {-3081.0f, -5592.0f, 1200.0f, "14"},
    {-970.0f, -5631.0f, 1200.0f, "15"},
    {-1131.0f, -4074.0f, 1200.0f, "16"},
    {706.0f, -3170.0f, 1200.0f, "17"},
    {390.0f, -769.0f, 1200.0f, "18"},
    {-18.0f, 1504.0f, 300.0f, "19"},
    {-1660.0f, 1958.0f, 300.0f, "20"},
    {-2758.0f, 1361.0f, 300.0f, "21"},
};

constexpr RouteDefinition kRoutes[] = {
    {"RunDoomloreToDalada", GWA3::MapIds::DOOMLORE_SHRINE, GWA3::MapIds::DALADA_UPLANDS, kRunToDaladaWaypoints, static_cast<int>(sizeof(kRunToDaladaWaypoints) / sizeof(kRunToDaladaWaypoints[0]))},
    {"RunDaladaToSacnoth", GWA3::MapIds::DALADA_UPLANDS, GWA3::MapIds::SACNOTH_VALLEY, kRunToSacnothWaypoints, static_cast<int>(sizeof(kRunToSacnothWaypoints) / sizeof(kRunToSacnothWaypoints[0]))},
    {"RunSacnothToDungeon", GWA3::MapIds::SACNOTH_VALLEY, GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL1, kRunToDungeonWaypoints, static_cast<int>(sizeof(kRunToDungeonWaypoints) / sizeof(kRunToDungeonWaypoints[0]))},
    {"Level1", GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL1, GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL2, kLevel1Waypoints, static_cast<int>(sizeof(kLevel1Waypoints) / sizeof(kLevel1Waypoints[0]))},
    {"Level2", GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL2, GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL3, kLevel2Waypoints, static_cast<int>(sizeof(kLevel2Waypoints) / sizeof(kLevel2Waypoints[0]))},
    {"Level3", GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL3, GWA3::MapIds::SACNOTH_VALLEY, kLevel3Waypoints, static_cast<int>(sizeof(kLevel3Waypoints) / sizeof(kLevel3Waypoints[0]))},
};

constexpr BlessingAnchor kRunToSacnothBlessings[] = {
    {0, -14971.0f, 11013.0f},
};

constexpr BlessingAnchor kRunToDungeonBlessings[] = {
    {0, -10503.0f, 19666.0f},
};

constexpr BlessingAnchor kLevel1Blessings[] = {
    {0, 18603.0f, -19307.0f},
};

constexpr BlessingAnchor kLevel2Blessings[] = {
    {0, 16796.0f, -4854.0f},
};

constexpr BlessingAnchor kLevel3Blessings[] = {
    {0, -17643.0f, 9567.0f},
};

constexpr uint32_t kQuestDialogs[] = {
    GWA3::DialogIds::GENERIC_ACCEPT,
    GWA3::DialogIds::Kathandrax::QUEST,
};

constexpr QuestNpcAnchor kQuestNpcAnchor = {
    18329.0f,
    -18134.0f,
    1500.0f,
};

constexpr TravelPoint kQuestEntryPath[] = {
    {19324.0f, -16150.0f},
};

} // namespace

const RouteDefinition& GetRouteDefinition(RouteId routeId) {
    return kRoutes[static_cast<int>(routeId)];
}

const RouteDefinition* FindRouteDefinitionByMapId(uint32_t mapId) {
    for (const auto& route : kRoutes) {
        if (route.map_id == mapId) {
            return &route;
        }
    }
    return nullptr;
}

const BlessingAnchor* GetBlessingAnchors(RouteId routeId, int& outCount) {
    switch (routeId) {
    case RouteId::RunDoomloreToDalada:
        outCount = 0;
        return nullptr;
    case RouteId::RunDaladaToSacnoth:
        outCount = static_cast<int>(sizeof(kRunToSacnothBlessings) / sizeof(kRunToSacnothBlessings[0]));
        return kRunToSacnothBlessings;
    case RouteId::RunSacnothToDungeon:
        outCount = static_cast<int>(sizeof(kRunToDungeonBlessings) / sizeof(kRunToDungeonBlessings[0]));
        return kRunToDungeonBlessings;
    case RouteId::Level1:
        outCount = static_cast<int>(sizeof(kLevel1Blessings) / sizeof(kLevel1Blessings[0]));
        return kLevel1Blessings;
    case RouteId::Level2:
        outCount = static_cast<int>(sizeof(kLevel2Blessings) / sizeof(kLevel2Blessings[0]));
        return kLevel2Blessings;
    case RouteId::Level3:
        outCount = static_cast<int>(sizeof(kLevel3Blessings) / sizeof(kLevel3Blessings[0]));
        return kLevel3Blessings;
    default:
        outCount = 0;
        return nullptr;
    }
}

const BlessingAnchor* FindBlessingAnchor(RouteId routeId, int nearestWaypointIndex) {
    int count = 0;
    const BlessingAnchor* anchors = GetBlessingAnchors(routeId, count);
    for (int i = 0; i < count; ++i) {
        if (anchors[i].trigger_index == nearestWaypointIndex) {
            return &anchors[i];
        }
    }
    return nullptr;
}

const uint32_t* GetQuestDialogSequence(int& outCount) {
    outCount = static_cast<int>(sizeof(kQuestDialogs) / sizeof(kQuestDialogs[0]));
    return kQuestDialogs;
}

QuestNpcAnchor GetQuestNpcAnchor() {
    return kQuestNpcAnchor;
}

const TravelPoint* GetQuestEntryPath(int& outCount) {
    outCount = static_cast<int>(sizeof(kQuestEntryPath) / sizeof(kQuestEntryPath[0]));
    return kQuestEntryPath;
}

BootstrapPlan GetQuestBootstrapPlan() {
    BootstrapPlan plan;
    plan.npc = GetQuestNpcAnchor();
    plan.dialog_ids = kQuestDialogs;
    plan.dialog_count = static_cast<int>(sizeof(kQuestDialogs) / sizeof(kQuestDialogs[0]));
    plan.dialog_repeats = QUEST_DIALOG_REPEAT_COUNT;
    plan.entry_path = kQuestEntryPath;
    plan.entry_path_count = static_cast<int>(sizeof(kQuestEntryPath) / sizeof(kQuestEntryPath[0]));
    plan.entry_map_id = GWA3::MapIds::SACNOTH_VALLEY;
    plan.target_map_id = GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL1;
    return plan;
}

RewardChestObjective GetRewardChestObjective() {
    return RewardChestObjective{
        {-18.0f, 1504.0f},
        {-239.0f, -583.0f},
        2,
        2,
    };
}

WaypointBehavior ResolveWaypointBehavior(const char* label) {
    switch (DungeonRoute::ClassifyWaypointLabel(label)) {
    case DungeonRoute::WaypointLabelKind::DungeonKey:
        return WaypointBehavior::PickUpDungeonKey;
    case DungeonRoute::WaypointLabelKind::BossLock:
        return WaypointBehavior::DoubleInteract;
    default:
        return WaypointBehavior::StandardMove;
    }
}

WaypointExecutionPlan BuildWaypointExecutionPlan(RouteId routeId, int waypointIndex) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    return DungeonRoute::MakeWaypointExecutionPlan<WaypointExecutionPlan>(
        route.waypoints,
        route.waypoint_count,
        waypointIndex,
        &ResolveWaypointBehavior);
}

} // namespace GWA3::Bot::Kathandrax
