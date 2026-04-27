#include <bots/frostmaws_burrows/FrostmawsBurrows.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/MapIds.h>

namespace GWA3::Bot::FrostmawsBurrows {

using DungeonRoute::Waypoint;
using DungeonQuest::BootstrapPlan;
using DungeonQuest::QuestNpcAnchor;
using DungeonQuest::TravelPoint;

namespace {

constexpr Waypoint kRunJagaWaypoints[] = {
    {14440.0f, 22851.0f, 0.0f, "1"},
    {16193.0f, 22812.0f, 0.0f, "2"},
    {16788.0f, 22797.0f, 0.0f, "3"},
};

constexpr Waypoint kRunToDungeonWaypoints[] = {
    {-8775.0f, -21631.0f, 1300.0f, "1"},
    {-3971.0f, -18365.0f, 1300.0f, "2"},
    {-2226.0f, -12049.0f, 1300.0f, "3"},
    {-5745.0f, -5340.0f, 1300.0f, "4"},
    {-5789.0f, 5475.0f, 1300.0f, "5"},
    {-3207.0f, 11162.0f, 1300.0f, "6"},
    {-715.0f, 15460.0f, 1300.0f, "7"},
    {-5463.0f, 19433.0f, 1300.0f, "8"},
    {31.0f, 24778.0f, 1300.0f, "9"},
};

constexpr Waypoint kLevel1Waypoints[] = {
    {-16298.0f, 18052.0f, 1200.0f, "1"},
    {-15077.0f, 16723.0f, 1200.0f, "2"},
    {-12677.0f, 14880.0f, 1200.0f, "3"},
    {-10971.0f, 13069.0f, 1200.0f, "4"},
    {-10884.0f, 12162.0f, 1200.0f, "5"},
};

constexpr Waypoint kLevel2Waypoints[] = {
    {19252.0f, -3513.0f, 1200.0f, "1"},
    {16947.0f, -4793.0f, 1200.0f, "2"},
    {16698.0f, -5811.0f, 1200.0f, "3"},
    {15521.0f, -6714.0f, 1200.0f, "4"},
    {12376.0f, -7127.0f, 1200.0f, "5"},
    {12390.0f, -9773.0f, 1200.0f, "6"},
    {13235.0f, -11529.0f, 1200.0f, "7"},
    {14037.0f, -14063.0f, 1200.0f, "8"},
    {14281.0f, -14820.0f, 1200.0f, "9"},
    {13735.0f, -16673.0f, 1200.0f, "10"},
    {13728.0f, -18402.0f, 1200.0f, "11"},
};

constexpr Waypoint kLevel3Waypoints[] = {
    {-18707.0f, 9639.0f, 1200.0f, "1"},
    {-16482.0f, 10973.0f, 1200.0f, "2"},
    {-14420.0f, 12256.0f, 1200.0f, "3"},
    {-13429.0f, 13201.0f, 1200.0f, "4"},
    {-10303.0f, 15506.0f, 1200.0f, "5"},
    {-8760.0f, 15389.0f, 1200.0f, "6"},
    {-7023.0f, 14764.0f, 1200.0f, "7"},
    {-4447.0f, 14961.0f, 1200.0f, "8"},
    {-2695.0f, 13392.0f, 1200.0f, "9"},
    {-1464.0f, 12410.0f, 1200.0f, "10"},
    {-300.0f, 11487.0f, 1200.0f, "11"},
    {896.0f, 14113.0f, 1200.0f, "12"},
    {2669.0f, 13776.0f, 1200.0f, "13"},
    {4062.0f, 14149.0f, 1200.0f, "14"},
    {5101.0f, 15185.0f, 1200.0f, "15"},
    {6716.0f, 16382.0f, 1200.0f, "16"},
    {8649.0f, 16900.0f, 1200.0f, "17"},
    {11915.0f, 16769.0f, 1200.0f, "18"},
    {14828.0f, 16770.0f, 1200.0f, "19"},
    {17100.0f, 15989.0f, 1200.0f, "20"},
    {17887.0f, 15750.0f, 1200.0f, "21"},
};

constexpr Waypoint kLevel4Waypoints[] = {
    {-15181.0f, 16464.0f, 1200.0f, "1"},
    {-14407.0f, 15103.0f, 1200.0f, "2"},
    {-14098.0f, 14022.0f, 1200.0f, "3"},
    {-15008.0f, 10679.0f, 1200.0f, "4"},
    {-16325.0f, 9984.0f, 1200.0f, "5"},
    {-17600.0f, 8590.0f, 1200.0f, "6"},
    {-19243.0f, 5493.0f, 1200.0f, "7"},
    {-18569.0f, 4192.0f, 1200.0f, "8"},
    {-17797.0f, 2636.0f, 1200.0f, "9"},
    {-14643.0f, 2452.0f, 1200.0f, "10"},
    {-14262.0f, 1794.0f, 1200.0f, "11"},
    {-12767.0f, -2119.0f, 1200.0f, "12"},
    {-13150.0f, -6631.0f, 1200.0f, "13"},
    {-13724.0f, -8353.0f, 1200.0f, "14"},
    {5101.0f, 15185.0f, 1200.0f, "15"},
    {6716.0f, 16382.0f, 1200.0f, "16"},
    {8649.0f, 16900.0f, 1200.0f, "17"},
    {11915.0f, 16769.0f, 1200.0f, "18"},
    {14828.0f, 16770.0f, 1200.0f, "19"},
    {17100.0f, 15989.0f, 1200.0f, "20"},
    {17887.0f, 15750.0f, 1200.0f, "21"},
};

// The supplied AutoIt source references FrostmawsBurrowsLvl5() but does not define it.
constexpr RouteDefinition kRoutes[] = {
    {"RunSifhallaToJagaMoraine", GWA3::MapIds::SIFHALLA, GWA3::MapIds::JAGA_MORAINE, kRunJagaWaypoints, static_cast<int>(sizeof(kRunJagaWaypoints) / sizeof(kRunJagaWaypoints[0]))},
    {"RunJagaMoraineToDungeon", GWA3::MapIds::JAGA_MORAINE, GWA3::MapIds::FROSTMAWS_BURROWS_LVL1, kRunToDungeonWaypoints, static_cast<int>(sizeof(kRunToDungeonWaypoints) / sizeof(kRunToDungeonWaypoints[0]))},
    {"Level1", GWA3::MapIds::FROSTMAWS_BURROWS_LVL1, GWA3::MapIds::FROSTMAWS_BURROWS_LVL2, kLevel1Waypoints, static_cast<int>(sizeof(kLevel1Waypoints) / sizeof(kLevel1Waypoints[0]))},
    {"Level2", GWA3::MapIds::FROSTMAWS_BURROWS_LVL2, GWA3::MapIds::FROSTMAWS_BURROWS_LVL3, kLevel2Waypoints, static_cast<int>(sizeof(kLevel2Waypoints) / sizeof(kLevel2Waypoints[0]))},
    {"Level3", GWA3::MapIds::FROSTMAWS_BURROWS_LVL3, GWA3::MapIds::FROSTMAWS_BURROWS_LVL4, kLevel3Waypoints, static_cast<int>(sizeof(kLevel3Waypoints) / sizeof(kLevel3Waypoints[0]))},
    {"Level4", GWA3::MapIds::FROSTMAWS_BURROWS_LVL4, GWA3::MapIds::FROSTMAWS_BURROWS_LVL5, kLevel4Waypoints, static_cast<int>(sizeof(kLevel4Waypoints) / sizeof(kLevel4Waypoints[0]))},
};

constexpr BlessingAnchor kRunToDungeonBlessings[] = {
    {0, -9068.0f, -22735.0f},
};

constexpr BlessingAnchor kLevel1Blessings[] = {
    {0, -16179.0f, 17596.0f},
};

constexpr BlessingAnchor kLevel2Blessings[] = {
    {0, 19072.0f, -3047.0f},
};

constexpr BlessingAnchor kLevel3Blessings[] = {
    {0, 18540.0f, 9962.0f},
};

constexpr BlessingAnchor kLevel4Blessings[] = {
    {0, -13752.0f, 16820.0f},
};

constexpr uint32_t kQuestDialogs[] = {
    GWA3::DialogIds::GENERIC_ACCEPT,
    GWA3::DialogIds::FrostmawsBurrows::QUEST,
};

const DungeonQuest::QuestNpcAnchor kQuestNpcAnchor = {
    1012.0f,
    25505.0f,
    1500.0f,
};

constexpr DungeonQuest::TravelPoint kQuestEntryPath[] = {
    {821.0f, 25300.0f},
    {1760.0f, 25332.0f},
    {1714.0f, 25700.0f},
};

constexpr DungeonQuest::TravelPoint kQuestResetPath[] = {
    {-16549.0f, 18104.0f},
    {-17300.0f, 18500.0f},
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
    case RouteId::RunSifhallaToJagaMoraine:
        outCount = 0;
        return nullptr;
    case RouteId::RunJagaMoraineToDungeon:
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
    case RouteId::Level4:
        outCount = static_cast<int>(sizeof(kLevel4Blessings) / sizeof(kLevel4Blessings[0]));
        return kLevel4Blessings;
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

const TravelPoint* GetQuestResetPath(int& outCount) {
    outCount = static_cast<int>(sizeof(kQuestResetPath) / sizeof(kQuestResetPath[0]));
    return kQuestResetPath;
}

BootstrapPlan GetQuestBootstrapPlan() {
    BootstrapPlan plan;
    plan.npc = GetQuestNpcAnchor();
    plan.dialog_ids = kQuestDialogs;
    plan.dialog_count = static_cast<int>(sizeof(kQuestDialogs) / sizeof(kQuestDialogs[0]));
    plan.dialog_repeats = QUEST_DIALOG_REPEAT_COUNT;
    plan.entry_path = kQuestEntryPath;
    plan.entry_path_count = static_cast<int>(sizeof(kQuestEntryPath) / sizeof(kQuestEntryPath[0]));
    plan.reset_path = kQuestResetPath;
    plan.reset_path_count = static_cast<int>(sizeof(kQuestResetPath) / sizeof(kQuestResetPath[0]));
    plan.entry_map_id = GWA3::MapIds::JAGA_MORAINE;
    plan.reset_map_id = GWA3::MapIds::SIFHALLA;
    plan.target_map_id = GWA3::MapIds::FROSTMAWS_BURROWS_LVL1;
    return plan;
}

WaypointBehavior ResolveWaypointBehavior(const char* label) {
    (void)label;
    return WaypointBehavior::StandardMove;
}

WaypointExecutionPlan BuildWaypointExecutionPlan(RouteId routeId, int waypointIndex) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    return DungeonRoute::MakeWaypointExecutionPlan<WaypointExecutionPlan>(
        route.waypoints,
        route.waypoint_count,
        waypointIndex,
        &ResolveWaypointBehavior);
}

} // namespace GWA3::Bot::FrostmawsBurrows
