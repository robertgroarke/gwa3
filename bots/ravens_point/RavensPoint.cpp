#include <bots/ravens_point/RavensPoint.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/MapIds.h>

namespace GWA3::Bot::RavensPoint {

using DungeonQuest::DialogPlan;
using DungeonQuest::QuestCyclePlan;
using DungeonQuest::QuestNpcAnchor;
using DungeonQuest::TravelPoint;
using DungeonRoute::Waypoint;

namespace {

constexpr Waypoint kDispatchRunOlafstead[] = {
    {273.0f, 1242.0f, 0.0f, "1"},
    {-1420.0f, 1400.0f, 0.0f, "2"},
};

constexpr Waypoint kDispatchVarajar[] = {
    {-3393.0f, -1985.0f, 1200.0f, "1"},
    {-3926.0f, -4650.0f, 1300.0f, "2"},
    {15526.0f, 8811.0f, 1300.0f, "3"},
    {-24788.0f, 15648.0f, 1300.0f, "4"},
};

constexpr Waypoint kDispatchLevel1[] = {
    {-14780.0f, -14951.0f, 1200.0f, "1"},
    {-5286.0f, -15742.0f, 1300.0f, "2"},
    {-7113.0f, -7617.0f, 1300.0f, "3"},
    {-6203.0f, 1452.0f, 1300.0f, "4"},
    {-6302.0f, 6194.0f, 1300.0f, "5"},
};

constexpr Waypoint kDispatchLevel2[] = {
    {19657.0f, 2913.0f, 1200.0f, "1"},
    {11613.0f, 3915.0f, 1300.0f, "2"},
    {1667.0f, 6582.0f, 1300.0f, "3"},
    {11547.0f, 5440.0f, 1300.0f, "4"},
    {580.0f, 6100.0f, 1300.0f, "5"},
    {-5086.0f, 12664.0f, 1300.0f, "6"},
    {-8521.0f, 14992.0f, 1300.0f, "7"},
    {-10913.0f, 16217.0f, 1300.0f, "8"},
    {-12198.0f, 15231.0f, 1300.0f, "9"},
    {4172.0f, 11634.0f, 1300.0f, "10"},
    {4910.0f, 13055.0f, 1300.0f, "11"},
    {5060.0f, 16381.0f, 1300.0f, "12"},
};

constexpr Waypoint kDispatchLevel3[] = {
    {5542.0f, 19196.0f, 1200.0f, "1"},
    {5681.0f, 17359.0f, 1300.0f, "2"},
    {4357.0f, 13443.0f, 1300.0f, "3"},
    {6302.0f, 11501.0f, 1300.0f, "4"},
    {9289.0f, 9126.0f, 1300.0f, "5"},
    {11022.0f, 9589.0f, 1300.0f, "6"},
    {12684.0f, 9621.0f, 1300.0f, "7"},
    {11770.0f, 10091.0f, 1300.0f, "8"},
};

constexpr Waypoint kRunVarajarToBlessing[] = {
    {-4167.0f, -3617.0f, 1300.0f, "1"},
    {-4423.0f, -296.0f, 1500.0f, "2"},
    {-4393.0f, 557.0f, 1300.0f, "3"},
    {-5329.0f, 3548.0f, 1300.0f, "4"},
    {-6191.0f, 4359.0f, 1200.0f, "5"},
    {-10426.0f, 4791.0f, 1200.0f, "6"},
    {-13696.0f, 7770.0f, 1200.0f, "7"},
};

constexpr Waypoint kQuestApproach[] = {
    {-16687.0f, 10730.0f, 900.0f, "1"},
    {-19787.0f, 10647.0f, 900.0f, "2"},
    {-21885.0f, 14297.0f, 900.0f, "3"},
    {-24788.0f, 15648.0f, 900.0f, "4"},
};

constexpr Waypoint kQuestReturn[] = {
    {-24788.0f, 15648.0f, 900.0f, "4"},
    {-21885.0f, 14297.0f, 900.0f, "3"},
    {-19787.0f, 10647.0f, 900.0f, "2"},
    {-16687.0f, 10730.0f, 900.0f, "1"},
};

constexpr Waypoint kLevel1Torch1[] = {
    {-14780.0f, -14951.0f, 1200.0f, "1"},
    {-11514.0f, -16219.0f, 1300.0f, "2"},
    {-7286.0f, -16119.0f, 1300.0f, "3"},
    {-5286.0f, -15742.0f, 1300.0f, "4"},
    {-6014.0f, -14887.0f, 1300.0f, "Chest"},
};

constexpr Waypoint kLevel1Torch2[] = {
    {-7113.0f, -7617.0f, 1200.0f, "1"},
    {-8618.0f, -5788.0f, 1300.0f, "2"},
    {-7875.0f, -3215.0f, 1300.0f, "3"},
    {-4161.0f, -2357.0f, 1300.0f, "4"},
    {-4460.0f, 1022.0f, 1300.0f, "5"},
    {-6203.0f, 1452.0f, 1300.0f, "Chest"},
};

constexpr Waypoint kLevel1DoorKey[] = {
    {-6302.0f, 6194.0f, 1300.0f, "1"},
    {-6043.0f, 7606.0f, 1300.0f, "2"},
    {-6218.0f, 6530.0f, 1300.0f, "Dungeon Key"},
    {-6301.0f, 1193.0f, 1300.0f, "4"},
    {-9412.0f, 732.0f, 1300.0f, "5"},
    {-10249.0f, 768.0f, 1300.0f, "6"},
    {-13614.0f, 2204.0f, 1300.0f, "7"},
    {-15090.0f, 3390.0f, 1300.0f, "8"},
    {-15151.0f, 5989.0f, 600.0f, "Boss lock"},
};

constexpr Waypoint kLevel1DoorBacktrack[] = {
    {-15151.0f, 5989.0f, 600.0f, "6"},
    {-15090.0f, 3390.0f, 1300.0f, "5"},
    {-13614.0f, 2204.0f, 1300.0f, "4"},
    {-10249.0f, 768.0f, 1300.0f, "3"},
    {-9412.0f, 732.0f, 1300.0f, "2"},
    {-6301.0f, 1193.0f, 1300.0f, "1"},
    {-6218.0f, 6530.0f, 1300.0f, "3"},
    {-6043.0f, 7606.0f, 1300.0f, "2"},
    {-6302.0f, 6194.0f, 1300.0f, "1"},
};

constexpr Waypoint kLevel1Exit[] = {
    {-15871.0f, 8184.0f, 300.0f, "1"},
    {-18067.0f, 8976.0f, 300.0f, "2"},
    {-18894.0f, 9596.0f, 1300.0f, "3"},
};

constexpr Waypoint kLevel2Torch1[] = {
    {19657.0f, 2913.0f, 1200.0f, "1"},
    {15881.0f, 908.0f, 1300.0f, "2"},
    {13075.0f, 942.0f, 1300.0f, "Chest"},
};

constexpr Waypoint kLevel2Torch2[] = {
    {11547.0f, 5440.0f, 1200.0f, "1"},
    {9334.0f, 6554.0f, 1300.0f, "2"},
    {5050.0f, 8296.0f, 1300.0f, "3"},
    {7959.0f, 5444.0f, 1300.0f, "4"},
    {7690.0f, 4440.0f, 1300.0f, "Chest"},
};

constexpr Waypoint kLevel2Torch3[] = {
    {580.0f, 6100.0f, 1200.0f, "1"},
    {-2556.0f, 8065.0f, 1300.0f, "2"},
    {-4063.0f, 9879.0f, 1300.0f, "3"},
    {-4574.0f, 6048.0f, 1300.0f, "Chest"},
};

constexpr Waypoint kLevel2BossKey[] = {
    {-8521.0f, 14992.0f, 1200.0f, "1"},
    {-10913.0f, 16217.0f, 1300.0f, "2"},
    {-12434.0f, 15356.0f, 1300.0f, "Dungeon Key"},
};

constexpr Waypoint kLevel2BossKeyBacktrack[] = {
    {3983.0f, 11605.0f, 1300.0f, "8"},
    {3753.0f, 8461.0f, 1300.0f, "7"},
    {1598.0f, 6798.0f, 1300.0f, "6"},
    {-901.0f, 7190.0f, 1300.0f, "5"},
    {-3701.0f, 9734.0f, 1300.0f, "4"},
    {-5179.0f, 12349.0f, 1300.0f, "3"},
    {-10223.0f, 16032.0f, 1300.0f, "2"},
    {-129581.0f, 15239.0f, 1200.0f, "Dungeon Key"},
};

constexpr Waypoint kLevel2Door[] = {
    {-129581.0f, 15239.0f, 1200.0f, "1"},
    {-10223.0f, 16032.0f, 1300.0f, "2"},
    {-5179.0f, 12349.0f, 1300.0f, "3"},
    {-3701.0f, 9734.0f, 1300.0f, "4"},
    {-901.0f, 7190.0f, 1300.0f, "5"},
    {1598.0f, 6798.0f, 1300.0f, "6"},
    {3753.0f, 8461.0f, 1300.0f, "7"},
    {3983.0f, 11605.0f, 1300.0f, "Boss lock"},
};

constexpr Waypoint kLevel2Exit[] = {
    {4910.0f, 13055.0f, 1200.0f, "1"},
    {6390.0f, 14419.0f, 1300.0f, "2"},
    {5060.0f, 16381.0f, 1300.0f, "3"},
};

constexpr Waypoint kLevel3Approach[] = {
    {5542.0f, 19196.0f, 1200.0f, "1"},
    {5681.0f, 17359.0f, 1300.0f, "2"},
    {4357.0f, 13443.0f, 1300.0f, "3"},
    {6302.0f, 11501.0f, 1300.0f, "4"},
    {9289.0f, 9126.0f, 1300.0f, "5"},
    {11022.0f, 9589.0f, 1300.0f, "6"},
    {12684.0f, 9621.0f, 1300.0f, "7"},
    {11770.0f, 10091.0f, 1300.0f, "8"},
};

constexpr Waypoint kLevel3BossLoop[] = {
    {11022.0f, 9589.0f, 2500.0f, "6"},
    {12684.0f, 9621.0f, 2500.0f, "7"},
    {11770.0f, 10091.0f, 2500.0f, "8"},
    {10866.0f, 12209.0f, 2500.0f, "9"},
    {10036.0f, 9067.0f, 2500.0f, "10"},
    {11770.0f, 10091.0f, 2500.0f, "11"},
};

constexpr RouteDefinition kDispatchRoutes[] = {
    {"RunOlafsteadToVarajarFells", GWA3::MapIds::OLAFSTEAD, GWA3::MapIds::VARAJAR_FELLS_1, kDispatchRunOlafstead, static_cast<int>(sizeof(kDispatchRunOlafstead) / sizeof(kDispatchRunOlafstead[0]))},
    {"VarajarFells", GWA3::MapIds::VARAJAR_FELLS_1, GWA3::MapIds::RAVENS_POINT_LVL1, kDispatchVarajar, static_cast<int>(sizeof(kDispatchVarajar) / sizeof(kDispatchVarajar[0]))},
    {"Level1", GWA3::MapIds::RAVENS_POINT_LVL1, GWA3::MapIds::RAVENS_POINT_LVL2, kDispatchLevel1, static_cast<int>(sizeof(kDispatchLevel1) / sizeof(kDispatchLevel1[0]))},
    {"Level2", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL3, kDispatchLevel2, static_cast<int>(sizeof(kDispatchLevel2) / sizeof(kDispatchLevel2[0]))},
    {"Level3", GWA3::MapIds::RAVENS_POINT_LVL3, GWA3::MapIds::VARAJAR_FELLS_1, kDispatchLevel3, static_cast<int>(sizeof(kDispatchLevel3) / sizeof(kDispatchLevel3[0]))},
};

constexpr RouteDefinition kRoutes[] = {
    {"RunOlafsteadToVarajarFells", GWA3::MapIds::OLAFSTEAD, GWA3::MapIds::VARAJAR_FELLS_1, kDispatchRunOlafstead, static_cast<int>(sizeof(kDispatchRunOlafstead) / sizeof(kDispatchRunOlafstead[0]))},
    {"RunVarajarToBlessing", GWA3::MapIds::VARAJAR_FELLS_1, GWA3::MapIds::VARAJAR_FELLS_1, kRunVarajarToBlessing, static_cast<int>(sizeof(kRunVarajarToBlessing) / sizeof(kRunVarajarToBlessing[0]))},
    {"QuestApproach", GWA3::MapIds::VARAJAR_FELLS_1, GWA3::MapIds::RAVENS_POINT_LVL1, kQuestApproach, static_cast<int>(sizeof(kQuestApproach) / sizeof(kQuestApproach[0]))},
    {"QuestReturn", GWA3::MapIds::VARAJAR_FELLS_1, GWA3::MapIds::VARAJAR_FELLS_1, kQuestReturn, static_cast<int>(sizeof(kQuestReturn) / sizeof(kQuestReturn[0]))},
    {"Level1Dispatch", GWA3::MapIds::RAVENS_POINT_LVL1, GWA3::MapIds::RAVENS_POINT_LVL2, kDispatchLevel1, static_cast<int>(sizeof(kDispatchLevel1) / sizeof(kDispatchLevel1[0]))},
    {"Level1Torch1", GWA3::MapIds::RAVENS_POINT_LVL1, GWA3::MapIds::RAVENS_POINT_LVL1, kLevel1Torch1, static_cast<int>(sizeof(kLevel1Torch1) / sizeof(kLevel1Torch1[0]))},
    {"Level1Torch2", GWA3::MapIds::RAVENS_POINT_LVL1, GWA3::MapIds::RAVENS_POINT_LVL1, kLevel1Torch2, static_cast<int>(sizeof(kLevel1Torch2) / sizeof(kLevel1Torch2[0]))},
    {"Level1DoorKey", GWA3::MapIds::RAVENS_POINT_LVL1, GWA3::MapIds::RAVENS_POINT_LVL1, kLevel1DoorKey, static_cast<int>(sizeof(kLevel1DoorKey) / sizeof(kLevel1DoorKey[0]))},
    {"Level1DoorBacktrack", GWA3::MapIds::RAVENS_POINT_LVL1, GWA3::MapIds::RAVENS_POINT_LVL1, kLevel1DoorBacktrack, static_cast<int>(sizeof(kLevel1DoorBacktrack) / sizeof(kLevel1DoorBacktrack[0]))},
    {"Level1Exit", GWA3::MapIds::RAVENS_POINT_LVL1, GWA3::MapIds::RAVENS_POINT_LVL2, kLevel1Exit, static_cast<int>(sizeof(kLevel1Exit) / sizeof(kLevel1Exit[0]))},
    {"Level2Dispatch", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL3, kDispatchLevel2, static_cast<int>(sizeof(kDispatchLevel2) / sizeof(kDispatchLevel2[0]))},
    {"Level2Torch1", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL2, kLevel2Torch1, static_cast<int>(sizeof(kLevel2Torch1) / sizeof(kLevel2Torch1[0]))},
    {"Level2Torch2", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL2, kLevel2Torch2, static_cast<int>(sizeof(kLevel2Torch2) / sizeof(kLevel2Torch2[0]))},
    {"Level2Torch3", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL2, kLevel2Torch3, static_cast<int>(sizeof(kLevel2Torch3) / sizeof(kLevel2Torch3[0]))},
    {"Level2BossKey", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL2, kLevel2BossKey, static_cast<int>(sizeof(kLevel2BossKey) / sizeof(kLevel2BossKey[0]))},
    {"Level2BossKeyBacktrack", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL2, kLevel2BossKeyBacktrack, static_cast<int>(sizeof(kLevel2BossKeyBacktrack) / sizeof(kLevel2BossKeyBacktrack[0]))},
    {"Level2Door", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL2, kLevel2Door, static_cast<int>(sizeof(kLevel2Door) / sizeof(kLevel2Door[0]))},
    {"Level2Exit", GWA3::MapIds::RAVENS_POINT_LVL2, GWA3::MapIds::RAVENS_POINT_LVL3, kLevel2Exit, static_cast<int>(sizeof(kLevel2Exit) / sizeof(kLevel2Exit[0]))},
    {"Level3Approach", GWA3::MapIds::RAVENS_POINT_LVL3, GWA3::MapIds::RAVENS_POINT_LVL3, kLevel3Approach, static_cast<int>(sizeof(kLevel3Approach) / sizeof(kLevel3Approach[0]))},
    {"Level3BossLoop", GWA3::MapIds::RAVENS_POINT_LVL3, GWA3::MapIds::VARAJAR_FELLS_1, kLevel3BossLoop, static_cast<int>(sizeof(kLevel3BossLoop) / sizeof(kLevel3BossLoop[0]))},
};

constexpr BlessingAnchor kRunVarajarBlessings[] = {
    {0, -2034.0f, -4512.0f},
};

constexpr BlessingAnchor kLevel1Torch1Blessings[] = {
    {0, -17536.0f, -14257.0f},
};

constexpr BlessingAnchor kLevel2Torch1Blessings[] = {
    {0, 19866.0f, 3238.0f},
};

constexpr BlessingAnchor kLevel2DoorBlessings[] = {
    {7, 4172.0f, 11634.0f},
};

constexpr uint32_t kRewardDialogs[] = {
    GWA3::DialogIds::GENERIC_ACCEPT,
    GWA3::DialogIds::RavensPoint::REWARD,
};

constexpr uint32_t kAcceptDialogs[] = {
    GWA3::DialogIds::GENERIC_ACCEPT,
    GWA3::DialogIds::RavensPoint::QUEST,
};

constexpr TravelPoint kQuestApproachPath[] = {
    {-16687.0f, 10730.0f},
    {-19787.0f, 10647.0f},
    {-21885.0f, 14297.0f},
    {-24788.0f, 15648.0f},
};

constexpr TravelPoint kLevel1Torch2Transit[] = {
    {-6862.0f, 1046.0f},
    {-6862.0f, 1046.0f},
};

constexpr TravelPoint kLevel2Torch2Transit[] = {
    {8166.0f, 5694.0f},
};

constexpr TravelPoint kLevel2Torch3Transit[] = {
    {-5648.0f, 6199.0f},
    {-4744.0f, 6290.0f},
};

constexpr TravelPoint kLevel1Torch1Braziers[] = {
    {-8508.0f, -17412.0f},
    {-5286.0f, -15742.0f},
    {-7609.0f, -14853.0f},
};

constexpr TravelPoint kLevel1Torch2Braziers[] = {
    {-7923.0f, 2013.0f},
    {-6561.0f, 2899.0f},
    {-6035.0f, 2957.0f},
};

constexpr TravelPoint kLevel2Torch1Braziers[] = {
    {14561.0f, 1046.0f},
    {11744.0f, 2699.0f},
    {12250.0f, 2805.0f},
};

constexpr TravelPoint kLevel2Torch2Braziers[] = {
    {7358.0f, 6100.0f},
    {2074.0f, 7441.0f},
    {1667.0f, 6582.0f},
};

constexpr TravelPoint kLevel2Torch3Braziers[] = {
    {-5649.0f, 5788.0f},
    {-5444.0f, 12113.0f},
    {-5086.0f, 12664.0f},
};

constexpr TorchObjective kTorchObjectives[] = {
    {RouteId::Level1Torch1, {-6462.0f, -15819.0f}, nullptr, 0, kLevel1Torch1Braziers, 3, {-7609.0f, -14853.0f}, {-4045.0f, -7944.0f}},
    {RouteId::Level1Torch2, {-6255.0f, 2870.0f}, kLevel1Torch2Transit, 2, kLevel1Torch2Braziers, 3, {-6035.0f, 2957.0f}, {-6302.0f, 6194.0f}},
    {RouteId::Level2Torch1, {12475.0f, 1741.0f}, nullptr, 0, kLevel2Torch1Braziers, 3, {12250.0f, 2805.0f}, {11541.0f, 5486.0f}},
    {RouteId::Level2Torch2, {7087.0f, 5675.0f}, kLevel2Torch2Transit, 1, kLevel2Torch2Braziers, 3, {1667.0f, 6582.0f}, {580.0f, 6100.0f}},
    {RouteId::Level2Torch3, {-5643.0f, 6112.0f}, kLevel2Torch3Transit, 2, kLevel2Torch3Braziers, 3, {-5086.0f, 12664.0f}, {-8521.0f, 14992.0f}},
};

constexpr DoorObjective kDoorObjectives[] = {
    {RouteId::Level1DoorKey, {-15612.0f, 6094.0f}, 6, {-15871.0f, 8184.0f}},
    {RouteId::Level2Door, {3686.0f, 12128.0f}, 1, {4910.0f, 13055.0f}},
};

constexpr LootObjective kLootObjectives[] = {
    {RouteId::Level1DoorKey, {-6174.0f, 6594.0f}, 3},
    {RouteId::Level2BossKey, {-129581.0f, 15239.0f}, 1},
    {RouteId::Level2BossKeyBacktrack, {-129581.0f, 15239.0f}, 1},
};

constexpr RewardChestObjective kRewardChest = {
    {14876.0f, -19033.0f},
    {12111.0f, 9604.0f},
    2,
    2,
};

} // namespace

const RouteDefinition& GetDispatchRouteDefinition(StageId stageId) {
    return kDispatchRoutes[static_cast<int>(stageId)];
}

const RouteDefinition* FindDispatchRouteDefinitionByMapId(uint32_t mapId) {
    for (const auto& route : kDispatchRoutes) {
        if (route.map_id == mapId) {
            return &route;
        }
    }
    return nullptr;
}

const RouteDefinition& GetRouteDefinition(RouteId routeId) {
    return kRoutes[static_cast<int>(routeId)];
}

bool UsesAggroTraversal(RouteId routeId) {
    if (routeId == RouteId::RunVarajarToBlessing ||
        routeId == RouteId::QuestApproach ||
        routeId == RouteId::QuestReturn) {
        return true;
    }
    const auto& route = GetRouteDefinition(routeId);
    return route.map_id == GWA3::MapIds::RAVENS_POINT_LVL1 ||
           route.map_id == GWA3::MapIds::RAVENS_POINT_LVL2 ||
           route.map_id == GWA3::MapIds::RAVENS_POINT_LVL3;
}

const BlessingAnchor* GetBlessingAnchors(RouteId routeId, int& outCount) {
    switch (routeId) {
    case RouteId::RunVarajarToBlessing:
        outCount = static_cast<int>(sizeof(kRunVarajarBlessings) / sizeof(kRunVarajarBlessings[0]));
        return kRunVarajarBlessings;
    case RouteId::Level1Torch1:
        outCount = static_cast<int>(sizeof(kLevel1Torch1Blessings) / sizeof(kLevel1Torch1Blessings[0]));
        return kLevel1Torch1Blessings;
    case RouteId::Level2Torch1:
        outCount = static_cast<int>(sizeof(kLevel2Torch1Blessings) / sizeof(kLevel2Torch1Blessings[0]));
        return kLevel2Torch1Blessings;
    case RouteId::Level2Door:
        outCount = static_cast<int>(sizeof(kLevel2DoorBlessings) / sizeof(kLevel2DoorBlessings[0]));
        return kLevel2DoorBlessings;
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

QuestCyclePlan GetQuestCyclePlan() {
    QuestCyclePlan plan;
    plan.npc = QuestNpcAnchor{-15526.0f, 8811.0f, 1500.0f};
    plan.reward_dialog = DialogPlan{kRewardDialogs, static_cast<int>(sizeof(kRewardDialogs) / sizeof(kRewardDialogs[0])), REWARD_DIALOG_REPEAT_COUNT};
    plan.accept_dialog = DialogPlan{kAcceptDialogs, static_cast<int>(sizeof(kAcceptDialogs) / sizeof(kAcceptDialogs[0])), QUEST_DIALOG_REPEAT_COUNT};
    plan.approach_path = kQuestApproachPath;
    plan.approach_path_count = static_cast<int>(sizeof(kQuestApproachPath) / sizeof(kQuestApproachPath[0]));
    plan.dungeon_entry = TravelPoint{-26100.0f, 16100.0f};
    plan.dungeon_exit = TravelPoint{-18700.0f, -14350.0f};
    plan.start_map_id = GWA3::MapIds::VARAJAR_FELLS_1;
    plan.dungeon_map_id = GWA3::MapIds::RAVENS_POINT_LVL1;
    return plan;
}

const TorchObjective* GetTorchObjectives(int& outCount) {
    outCount = static_cast<int>(sizeof(kTorchObjectives) / sizeof(kTorchObjectives[0]));
    return kTorchObjectives;
}

const TorchObjective* FindTorchObjective(RouteId routeId) {
    int count = 0;
    const TorchObjective* objectives = GetTorchObjectives(count);
    for (int i = 0; i < count; ++i) {
        if (objectives[i].route_id == routeId) {
            return &objectives[i];
        }
    }
    return nullptr;
}

const DoorObjective* GetDoorObjectives(int& outCount) {
    outCount = static_cast<int>(sizeof(kDoorObjectives) / sizeof(kDoorObjectives[0]));
    return kDoorObjectives;
}

const DoorObjective* FindDoorObjective(RouteId routeId) {
    int count = 0;
    const DoorObjective* objectives = GetDoorObjectives(count);
    for (int i = 0; i < count; ++i) {
        if (objectives[i].route_id == routeId) {
            return &objectives[i];
        }
    }
    return nullptr;
}

const LootObjective* GetLootObjectives(int& outCount) {
    outCount = static_cast<int>(sizeof(kLootObjectives) / sizeof(kLootObjectives[0]));
    return kLootObjectives;
}

const LootObjective* FindLootObjective(RouteId routeId) {
    int count = 0;
    const LootObjective* objectives = GetLootObjectives(count);
    for (int i = 0; i < count; ++i) {
        if (objectives[i].route_id == routeId) {
            return &objectives[i];
        }
    }
    return nullptr;
}

RewardChestObjective GetRewardChestObjective() {
    return kRewardChest;
}

} // namespace GWA3::Bot::RavensPoint
