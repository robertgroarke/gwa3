#include <bots/arachnis_haunt/ArachnisHaunt.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/MapIds.h>

namespace GWA3::Bot::ArachnisHaunt {

using DungeonQuest::DialogPlan;
using DungeonQuest::QuestCyclePlan;
using DungeonQuest::QuestNpcAnchor;
using DungeonQuest::TravelPoint;
using DungeonRoute::Waypoint;

namespace {

constexpr Waypoint kRunRataSumToMagusStones[] = {
    {16806.0f, 15141.0f, 0.0f, "1"},
    {16464.0f, 13997.0f, 0.0f, "2"},
    {16400.0f, 13600.0f, 0.0f, "3"},
};

constexpr Waypoint kRunMagusToDungeon[] = {
    {14084.0f, 12017.0f, 1300.0f, "1"},
    {13662.0f, 8876.0f, 1300.0f, "2"},
    {12255.0f, 7668.0f, 1300.0f, "3"},
    {11600.0f, 8161.0f, 1300.0f, "4"},
    {10019.0f, 7670.0f, 1300.0f, "5"},
    {8864.0f, 7022.0f, 1300.0f, "6"},
    {8198.0f, 5650.0f, 1500.0f, "7"},
    {7802.0f, 2329.0f, 1300.0f, "8"},
    {4892.0f, -166.0f, 1300.0f, "9"},
    {1139.0f, -1685.0f, 1200.0f, "10"},
    {-3414.0f, -1505.0f, 1300.0f, "11"},
    {-8499.0f, -2519.0f, 1300.0f, "12"},
    {-8739.0f, -8641.0f, 1300.0f, "13"},
    {-10580.0f, -11560.0f, 1300.0f, "14"},
    {-11391.0f, -11516.0f, 1300.0f, "15"},
    {-9683.0f, -17090.0f, 1300.0f, "16"},
};

constexpr Waypoint kLevel1Dispatch[] = {
    {16892.0f, 18903.0f, 1200.0f, "1"},
    {12197.0f, 19538.0f, 1200.0f, "2"},
    {11511.0f, 19382.0f, 1200.0f, "3"},
    {8688.0f, 17062.0f, 1300.0f, "4"},
    {5794.0f, 10708.0f, 1200.0f, "5"},
    {1246.0f, 6427.0f, 1200.0f, "6"},
    {-2618.0f, 7722.0f, 1200.0f, "7"},
    {-7510.0f, 4036.0f, 1200.0f, "8"},
    {-13519.0f, 3069.0f, 1200.0f, "9"},
    {-14551.0f, 2302.0f, 1200.0f, "10"},
    {-15443.0f, 2121.0f, 1200.0f, "11"},
    {-18741.0f, 8193.0f, 1200.0f, "12"},
    {-17242.0f, 11529.0f, 1200.0f, "13"},
    {353.0f, 16889.0f, 1200.0f, "14"},
    {1802.0f, 16439.0f, 1200.0f, "15"},
    {2125.0f, 19555.0f, 1200.0f, "16"},
};

constexpr Waypoint kLevel2Dispatch[] = {
    {9265.0f, 18798.0f, 1200.0f, "1"},
    {-140.0f, 11883.0f, 1200.0f, "2"},
    {-930.0f, 12615.0f, 1200.0f, "3"},
    {-1782.0f, 12685.0f, 400.0f, "4"},
    {-3071.0f, 12532.0f, 1200.0f, "5"},
    {-4827.0f, 11955.0f, 1200.0f, "6"},
    {-4020.0f, 13218.0f, 1200.0f, "Asura Flame Staff"},
    {-7030.0f, 11813.0f, 1200.0f, "8"},
    {-7028.0f, 15477.0f, 400.0f, "9"},
    {-10383.0f, 17987.0f, 300.0f, "10"},
    {-12274.0f, 18366.0f, 1200.0f, "11"},
    {-13923.0f, 18399.0f, 1200.0f, "Staff Check"},
    {-15512.0f, 17956.0f, 1200.0f, "13"},
    {-15361.0f, 16256.0f, 1200.0f, "14"},
    {-15361.0f, 15527.0f, 1200.0f, "15"},
    {-17131.0f, 11601.0f, 1200.0f, "Chest"},
};

constexpr Waypoint kLevel1Phase1[] = {
    {16892.0f, 18903.0f, 1200.0f, "1"},
    {16209.0f, 18758.0f, 1300.0f, "2"},
    {15170.0f, 18781.0f, 1300.0f, "3"},
    {12938.0f, 19815.0f, 1200.0f, "Asura Flame Staff"},
};

constexpr Waypoint kLevel1Phase2[] = {
    {11511.0f, 19382.0f, 1200.0f, "1"},
    {10662.0f, 19179.0f, 1300.0f, "2"},
    {8688.0f, 17062.0f, 1300.0f, "3"},
    {7268.0f, 13949.0f, 1200.0f, "4"},
    {6206.0f, 12645.0f, 1200.0f, "5"},
    {5794.0f, 10708.0f, 1200.0f, "6"},
    {4568.0f, 9745.0f, 1200.0f, "7"},
    {3033.0f, 8438.0f, 1200.0f, "8"},
    {1246.0f, 6427.0f, 1200.0f, "9"},
    {208.0f, 7354.0f, 1200.0f, "10"},
    {-1406.0f, 8499.0f, 1200.0f, "11"},
    {-2618.0f, 7722.0f, 1200.0f, "12"},
    {-4263.0f, 6631.0f, 1200.0f, "13"},
    {-3362.0f, 4833.0f, 1200.0f, "Asura Flame Staff"},
    {-6177.0f, 5097.0f, 1200.0f, "14"},
    {-7510.0f, 4036.0f, 1200.0f, "15"},
    {-8739.0f, 2919.0f, 1200.0f, "Staff Check"},
    {-11151.0f, 1929.0f, 1399.0f, "17"},
    {-13519.0f, 3069.0f, 1200.0f, "Staff Check"},
};

constexpr Waypoint kLevel1Phase3[] = {
    {-15443.0f, 2121.0f, 1200.0f, "1"},
    {-17861.0f, 3169.0f, 1300.0f, "2"},
    {-18048.0f, 4155.0f, 1300.0f, "3"},
    {-17113.0f, 4400.0f, 1200.0f, "Staff Check"},
    {-15467.0f, 5074.0f, 1200.0f, "5"},
    {-14999.0f, 6304.0f, 1200.0f, "6"},
    {-15328.0f, 8054.0f, 1200.0f, "Staff Check"},
    {-16242.0f, 8332.0f, 1200.0f, "8"},
    {-17283.0f, 9277.0f, 1200.0f, "9"},
    {-18659.0f, 8761.0f, 1200.0f, "Staff Check"},
};

constexpr Waypoint kLevel1Phase4[] = {
    {-17242.0f, 11529.0f, 1200.0f, "1"},
    {-13898.0f, 12112.0f, 1300.0f, "2"},
    {-11784.0f, 13143.0f, 1300.0f, "3"},
    {-11016.0f, 16147.0f, 1200.0f, "Staff Check"},
    {-8412.0f, 18665.0f, 1200.0f, "5"},
    {-6319.0f, 20076.0f, 1200.0f, "6"},
    {-4299.0f, 19848.0f, 1200.0f, "7"},
    {-2685.0f, 19234.0f, 1200.0f, "Staff Check"},
    {-1861.0f, 19363.0f, 1200.0f, "9"},
    {-689.0f, 18084.0f, 1200.0f, "Staff Check"},
    {-3311.0f, 17200.0f, 1200.0f, "11"},
};

constexpr Waypoint kLevel1Exit[] = {
    {1802.0f, 16439.0f, 1200.0f, "1"},
    {3353.0f, 17107.0f, 1300.0f, "2"},
    {3482.0f, 18460.0f, 1300.0f, "3"},
    {2125.0f, 19555.0f, 1200.0f, "4"},
};

constexpr Waypoint kLevel2Phase1Approach[] = {
    {9265.0f, 18798.0f, 400.0f, "1"},
    {9441.0f, 17306.0f, 400.0f, "2"},
    {12038.0f, 16658.0f, 1200.0f, "3"},
    {12939.0f, 17075.0f, 1200.0f, "Asura Flame Staff"},
};

constexpr Waypoint kLevel2Phase1Transit[] = {
    {9204.0f, 19087.0f, 1200.0f, "5"},
    {9441.0f, 17306.0f, 400.0f, "6"},
    {8200.0f, 16947.0f, 400.0f, "7"},
    {6439.0f, 15282.0f, 300.0f, "Staff Check"},
    {8119.0f, 12951.0f, 1200.0f, "9"},
    {8043.0f, 11673.0f, 1200.0f, "10"},
    {4068.0f, 9826.0f, 1000.0f, "Staff Check"},
    {2377.0f, 10341.0f, 1100.0f, "12"},
    {596.0f, 11403.0f, 1000.0f, "Staff Check"},
};

constexpr Waypoint kLevel2Phase2[] = {
    {-930.0f, 12615.0f, 400.0f, "1"},
    {-1782.0f, 12685.0f, 400.0f, "2"},
    {-3071.0f, 12532.0f, 1200.0f, "3"},
    {-4827.0f, 11955.0f, 1200.0f, "4"},
    {-4020.0f, 13218.0f, 1200.0f, "Asura Flame Staff"},
    {-7030.0f, 11813.0f, 1200.0f, "5"},
    {-7028.0f, 15477.0f, 400.0f, "6"},
    {-8987.0f, 15655.0f, 400.0f, "Staff Check"},
    {-10383.0f, 17987.0f, 300.0f, "9"},
    {-12274.0f, 18366.0f, 1200.0f, "10"},
    {-13923.0f, 18399.0f, 1200.0f, "Staff Check"},
    {-15512.0f, 17956.0f, 1200.0f, "12"},
    {-15381.0f, 16860.0f, 1200.0f, "Staff Check"},
};

constexpr Waypoint kLevel2Phase3[] = {
    {-7028.0f, 15477.0f, 400.0f, "1"},
    {-8987.0f, 15655.0f, 400.0f, "Staff Check"},
    {-10383.0f, 17987.0f, 300.0f, "3"},
    {-12274.0f, 18366.0f, 1200.0f, "4"},
    {-13923.0f, 18399.0f, 1200.0f, "Staff Check"},
    {-15512.0f, 17956.0f, 1200.0f, "5"},
    {-15381.0f, 16860.0f, 1200.0f, "Staff Check"},
    {-15361.0f, 15527.0f, 1200.0f, "7"},
    {-15109.0f, 13907.0f, 1200.0f, "Staff Check"},
    {-15575.0f, 12427.0f, 1200.0f, "9"},
    {-13940.0f, 11822.0f, 1200.0f, "Staff Check"},
    {-17023.0f, 12060.0f, 1200.0f, "11"},
    {-14021.0f, 12216.0f, 400.0f, "Staff Check"},
    {-14176.0f, 11556.0f, 1200.0f, "Spider Eggs Lvl3 1-2"},
    {-14313.0f, 12197.0f, 1200.0f, "Staff Check"},
    {-14119.0f, 11789.0f, 1200.0f, "15"},
    {-13530.0f, 12012.0f, 1200.0f, "Spider Eggs Lvl3 3-5"},
    {-14119.0f, 11789.0f, 1200.0f, "Staff Check"},
    {-13555.0f, 11806.0f, 1200.0f, "18"},
    {-16796.0f, 11509.0f, 1200.0f, "Spider Eggs Lvl3 6-7"},
    {-17134.0f, 11339.0f, 1200.0f, "Staff Check"},
    {-17241.0f, 11388.0f, 1200.0f, "21"},
    {-17113.0f, 11286.0f, 1200.0f, "Spider Eggs Lvl3 8-10"},
    {-17335.0f, 11790.0f, 1200.0f, "Boss 1"},
    {-17269.0f, 11593.0f, 1200.0f, "Boss 2"},
    {-16722.0f, 11558.0f, 1300.0f, "Boss 3"},
    {-15019.0f, 13840.0f, 1200.0f, "Boss 4"},
    {-14176.0f, 11556.0f, 1200.0f, "Boss 5"},
    {-17246.0f, 12100.0f, 1200.0f, "Boss 6"},
};

constexpr RouteDefinition kStages[] = {
    {"RunRataSumToMagusStones", GWA3::MapIds::RATA_SUM, GWA3::MapIds::MAGUS_STONES, kRunRataSumToMagusStones, static_cast<int>(sizeof(kRunRataSumToMagusStones) / sizeof(kRunRataSumToMagusStones[0]))},
    {"RunMagusToDungeon", GWA3::MapIds::MAGUS_STONES, GWA3::MapIds::ARACHNIS_HAUNT_LVL1, kRunMagusToDungeon, static_cast<int>(sizeof(kRunMagusToDungeon) / sizeof(kRunMagusToDungeon[0]))},
    {"Level1", GWA3::MapIds::ARACHNIS_HAUNT_LVL1, GWA3::MapIds::ARACHNIS_HAUNT_LVL2, kLevel1Dispatch, static_cast<int>(sizeof(kLevel1Dispatch) / sizeof(kLevel1Dispatch[0]))},
    {"Level2", GWA3::MapIds::ARACHNIS_HAUNT_LVL2, GWA3::MapIds::MAGUS_STONES, kLevel2Dispatch, static_cast<int>(sizeof(kLevel2Dispatch) / sizeof(kLevel2Dispatch[0]))},
};

constexpr RouteDefinition kRoutes[] = {
    {"RunRataSumToMagusStones", GWA3::MapIds::RATA_SUM, GWA3::MapIds::MAGUS_STONES, kRunRataSumToMagusStones, static_cast<int>(sizeof(kRunRataSumToMagusStones) / sizeof(kRunRataSumToMagusStones[0]))},
    {"RunMagusToDungeon", GWA3::MapIds::MAGUS_STONES, 0u, kRunMagusToDungeon, static_cast<int>(sizeof(kRunMagusToDungeon) / sizeof(kRunMagusToDungeon[0]))},
    {"Level1Phase1", GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 0u, kLevel1Phase1, static_cast<int>(sizeof(kLevel1Phase1) / sizeof(kLevel1Phase1[0]))},
    {"Level1Phase2", GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 0u, kLevel1Phase2, static_cast<int>(sizeof(kLevel1Phase2) / sizeof(kLevel1Phase2[0]))},
    {"Level1Phase3", GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 0u, kLevel1Phase3, static_cast<int>(sizeof(kLevel1Phase3) / sizeof(kLevel1Phase3[0]))},
    {"Level1Phase4", GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 0u, kLevel1Phase4, static_cast<int>(sizeof(kLevel1Phase4) / sizeof(kLevel1Phase4[0]))},
    {"Level1Exit", GWA3::MapIds::ARACHNIS_HAUNT_LVL1, GWA3::MapIds::ARACHNIS_HAUNT_LVL2, kLevel1Exit, static_cast<int>(sizeof(kLevel1Exit) / sizeof(kLevel1Exit[0]))},
    {"Level2Phase1Approach", GWA3::MapIds::ARACHNIS_HAUNT_LVL2, 0u, kLevel2Phase1Approach, static_cast<int>(sizeof(kLevel2Phase1Approach) / sizeof(kLevel2Phase1Approach[0]))},
    {"Level2Phase1Transit", GWA3::MapIds::ARACHNIS_HAUNT_LVL2, 0u, kLevel2Phase1Transit, static_cast<int>(sizeof(kLevel2Phase1Transit) / sizeof(kLevel2Phase1Transit[0]))},
    {"Level2Phase2", GWA3::MapIds::ARACHNIS_HAUNT_LVL2, 0u, kLevel2Phase2, static_cast<int>(sizeof(kLevel2Phase2) / sizeof(kLevel2Phase2[0]))},
    {"Level2Phase3", GWA3::MapIds::ARACHNIS_HAUNT_LVL2, 0u, kLevel2Phase3, static_cast<int>(sizeof(kLevel2Phase3) / sizeof(kLevel2Phase3[0]))},
};

constexpr BlessingAnchor kRunBlessings[] = {
    {0, 14862.0f, 13173.0f},
};

constexpr BlessingAnchor kLevel1Blessings[] = {
    {0, 17507.0f, 18900.0f},
    {6, 4943.0f, 9667.0f},
};

constexpr BlessingAnchor kLevel2Blessings[] = {
    {0, 9506.0f, 17713.0f},
    {8, -6755.0f, 15568.0f},
};

constexpr BlessingAnchor kLevel1Phase1Blessings[] = {
    {0, 17507.0f, 18900.0f},
};

constexpr BlessingAnchor kLevel1Phase2Blessings[] = {
    {6, 4943.0f, 9667.0f},
};

constexpr BlessingAnchor kLevel1Phase3Blessings[] = {
    {0, -16066.0f, 865.0f},
};

constexpr BlessingAnchor kLevel2Phase1Blessings[] = {
    {0, 9506.0f, 17713.0f},
};

constexpr BlessingAnchor kLevel2Phase2Blessings[] = {
    {5, -6755.0f, 15568.0f},
};

constexpr BlessingAnchor kLevel2Phase3Blessings[] = {
    {12, -6755.0f, 15568.0f},
};

constexpr uint32_t kRewardDialogs[] = {
    GWA3::DialogIds::GENERIC_ACCEPT,
    GWA3::DialogIds::ArachnisHaunt::REWARD,
};

constexpr uint32_t kAcceptDialogs[] = {
    GWA3::DialogIds::SHORT_ACCEPT,
    GWA3::DialogIds::ArachnisHaunt::QUEST,
};

constexpr uint32_t kCompletionDialogs[] = {
    GWA3::DialogIds::SHORT_ACCEPT,
    GWA3::DialogIds::ArachnisHaunt::COMPLETION,
};

constexpr TravelPoint kQuestApproachPath[] = {
    {-9735.0f, -17557.0f},
    {-11392.0f, -18547.0f},
};

constexpr TravelPoint kQuestReturnPath[] = {
    {-11392.0f, -18547.0f},
    {-9735.0f, -17557.0f},
};

constexpr TravelPoint kLvl1FirstStaffPath[] = {
    {12197.0f, 19538.0f},
    {12197.0f, 19538.0f},
    {11511.0f, 19382.0f},
};

constexpr TravelPoint kLvl1SecondStaffPath[] = {
    {-14551.0f, 2302.0f},
    {-14551.0f, 2302.0f},
    {-15443.0f, 2121.0f},
};

constexpr TravelPoint kLvl2FirstStaffPath[] = {
    {-140.0f, 11883.0f},
    {-140.0f, 11883.0f},
    {-930.0f, 12615.0f},
};

constexpr TravelPoint kLvl2SecondStaffPath[] = {
    {-15361.0f, 16256.0f},
    {-15361.0f, 16256.0f},
    {-15361.0f, 15527.0f},
};

constexpr FlameStaffObjective kFlameStaffObjectives[] = {
    {"Level1FirstStaff", {12938.0f, 19815.0f}, kLvl1FirstStaffPath, 3, {11511.0f, 19382.0f}},
    {"Level1SecondStaff", {-3362.0f, 4833.0f}, kLvl1SecondStaffPath, 3, {0.0f, 0.0f}},
    {"Level2FirstStaff", {12939.0f, 17075.0f}, kLvl2FirstStaffPath, 3, {-930.0f, 12615.0f}},
    {"Level2SecondStaff", {-4020.0f, 13218.0f}, kLvl2SecondStaffPath, 3, {0.0f, 0.0f}},
};

constexpr TravelPoint kLvl1EggsA[] = {
    {-18759.0f, 9024.0f},
    {-18875.0f, 9057.0f},
    {-18545.0f, 8256.0f},
    {-18662.0f, 8323.0f},
    {-18741.0f, 8193.0f},
};

constexpr TravelPoint kLvl1EggsAPostPath[] = {
    {-17512.0f, 9502.0f},
    {-17242.0f, 11529.0f},
};

constexpr TravelPoint kLvl1EggsB[] = {
    {-3096.0f, 16689.0f},
    {-2950.0f, 16678.0f},
    {-2840.0f, 16942.0f},
    {-2778.0f, 17032.0f},
    {-2651.0f, 17091.0f},
};

constexpr TravelPoint kLvl2Eggs12[] = {
    {-14209.0f, 11671.0f},
    {-14119.0f, 11789.0f},
};

constexpr TravelPoint kLvl2Eggs35[] = {
    {-13563.0f, 11989.0f},
    {-13547.0f, 11871.0f},
    {-13555.0f, 11806.0f},
};

constexpr TravelPoint kLvl2Eggs67[] = {
    {-17134.0f, 11339.0f},
    {-17241.0f, 11388.0f},
};

constexpr TravelPoint kLvl2Eggs810[] = {
    {-17269.0f, 11593.0f},
    {-17343.0f, 11680.0f},
    {-17335.0f, 11790.0f},
};

constexpr SpiderEggCluster kSpiderEggClusters[] = {
    {"Level1Eggs1To5", kLvl1EggsA, static_cast<int>(sizeof(kLvl1EggsA) / sizeof(kLvl1EggsA[0])), kLvl1EggsAPostPath, static_cast<int>(sizeof(kLvl1EggsAPostPath) / sizeof(kLvl1EggsAPostPath[0]))},
    {"Level1Eggs6To10", kLvl1EggsB, static_cast<int>(sizeof(kLvl1EggsB) / sizeof(kLvl1EggsB[0])), nullptr, 0},
    {"Level2Eggs1To2", kLvl2Eggs12, static_cast<int>(sizeof(kLvl2Eggs12) / sizeof(kLvl2Eggs12[0])), nullptr, 0},
    {"Level2Eggs3To5", kLvl2Eggs35, static_cast<int>(sizeof(kLvl2Eggs35) / sizeof(kLvl2Eggs35[0])), nullptr, 0},
    {"Level2Eggs6To7", kLvl2Eggs67, static_cast<int>(sizeof(kLvl2Eggs67) / sizeof(kLvl2Eggs67[0])), nullptr, 0},
    {"Level2Eggs8To10", kLvl2Eggs810, static_cast<int>(sizeof(kLvl2Eggs810) / sizeof(kLvl2Eggs810[0])), nullptr, 0},
};

constexpr TravelPoint kLevel1KeyApproach[] = {
    {-3435.0f, 17080.0f},
    {-1570.0f, 17647.0f},
    {6.0f, 16960.0f},
};

constexpr TravelPoint kLevel1KeyWebPath[] = {
    {353.0f, 16889.0f},
    {353.0f, 16889.0f},
    {1802.0f, 16439.0f},
};

constexpr KeyPickupObjective kLevel1KeyObjective = {
    "Level1DungeonKey",
    kLevel1KeyApproach,
    static_cast<int>(sizeof(kLevel1KeyApproach) / sizeof(kLevel1KeyApproach[0])),
    {6.0f, 16960.0f},
    4,
    kLevel1KeyWebPath,
    static_cast<int>(sizeof(kLevel1KeyWebPath) / sizeof(kLevel1KeyWebPath[0])),
    {1802.0f, 16439.0f},
};

constexpr DoorOpenObjective kLevel1ExitDoor = {
    "Level1ExitDoor",
    {2065.0f, 19738.0f},
    6,
    {1480.0f, 20200.0f},
};

constexpr RewardChestObjective kRewardChest = {
    {-17131.0f, 11601.0f},
    2,
    2,
    1000u,
    2,
    1,
    1000u,
    {-17131.0f, 11601.0f, 5000.0f},
    {kCompletionDialogs, static_cast<int>(sizeof(kCompletionDialogs) / sizeof(kCompletionDialogs[0])), COMPLETION_DIALOG_REPEAT_COUNT},
    {-14021.0f, 12216.0f},
};

} // namespace

const RouteDefinition& GetStageDefinition(StageId stageId) {
    return kStages[static_cast<int>(stageId)];
}

const RouteDefinition* FindStageDefinitionByMapId(uint32_t mapId) {
    for (const auto& route : kStages) {
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
    switch (routeId) {
    case RouteId::RunRataSumToMagusStones:
    case RouteId::RunMagusToDungeon:
    case RouteId::Level1Phase1:
    case RouteId::Level1Phase2:
    case RouteId::Level1Phase3:
    case RouteId::Level1Phase4:
    case RouteId::Level1Exit:
    case RouteId::Level2Phase1Approach:
    case RouteId::Level2Phase1Transit:
    case RouteId::Level2Phase2:
    case RouteId::Level2Phase3:
        return true;
    }
    return false;
}

const BlessingAnchor* GetBlessingAnchors(StageId stageId, int& outCount) {
    switch (stageId) {
    case StageId::RunMagusToDungeon:
        outCount = static_cast<int>(sizeof(kRunBlessings) / sizeof(kRunBlessings[0]));
        return kRunBlessings;
    case StageId::Level1:
        outCount = static_cast<int>(sizeof(kLevel1Blessings) / sizeof(kLevel1Blessings[0]));
        return kLevel1Blessings;
    case StageId::Level2:
        outCount = static_cast<int>(sizeof(kLevel2Blessings) / sizeof(kLevel2Blessings[0]));
        return kLevel2Blessings;
    default:
        outCount = 0;
        return nullptr;
    }
}

const BlessingAnchor* FindBlessingAnchor(StageId stageId, int nearestWaypointIndex) {
    int count = 0;
    const BlessingAnchor* anchors = GetBlessingAnchors(stageId, count);
    for (int i = 0; i < count; ++i) {
        if (anchors[i].trigger_index == nearestWaypointIndex) {
            return &anchors[i];
        }
    }
    return nullptr;
}

const BlessingAnchor* GetBlessingAnchors(RouteId routeId, int& outCount) {
    switch (routeId) {
    case RouteId::RunMagusToDungeon:
        outCount = static_cast<int>(sizeof(kRunBlessings) / sizeof(kRunBlessings[0]));
        return kRunBlessings;
    case RouteId::Level1Phase1:
        outCount = static_cast<int>(sizeof(kLevel1Phase1Blessings) / sizeof(kLevel1Phase1Blessings[0]));
        return kLevel1Phase1Blessings;
    case RouteId::Level1Phase2:
        outCount = static_cast<int>(sizeof(kLevel1Phase2Blessings) / sizeof(kLevel1Phase2Blessings[0]));
        return kLevel1Phase2Blessings;
    case RouteId::Level1Phase3:
        outCount = static_cast<int>(sizeof(kLevel1Phase3Blessings) / sizeof(kLevel1Phase3Blessings[0]));
        return kLevel1Phase3Blessings;
    case RouteId::Level2Phase1Approach:
        outCount = static_cast<int>(sizeof(kLevel2Phase1Blessings) / sizeof(kLevel2Phase1Blessings[0]));
        return kLevel2Phase1Blessings;
    case RouteId::Level2Phase2:
        outCount = static_cast<int>(sizeof(kLevel2Phase2Blessings) / sizeof(kLevel2Phase2Blessings[0]));
        return kLevel2Phase2Blessings;
    case RouteId::Level2Phase3:
        outCount = static_cast<int>(sizeof(kLevel2Phase3Blessings) / sizeof(kLevel2Phase3Blessings[0]));
        return kLevel2Phase3Blessings;
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
    plan.npc = QuestNpcAnchor{-10150.0f, -17087.0f, 1500.0f};
    plan.reward_dialog = DialogPlan{kRewardDialogs, static_cast<int>(sizeof(kRewardDialogs) / sizeof(kRewardDialogs[0])), REWARD_DIALOG_REPEAT_COUNT};
    plan.accept_dialog = DialogPlan{kAcceptDialogs, static_cast<int>(sizeof(kAcceptDialogs) / sizeof(kAcceptDialogs[0])), QUEST_DIALOG_REPEAT_COUNT};
    plan.approach_path = kQuestApproachPath;
    plan.approach_path_count = static_cast<int>(sizeof(kQuestApproachPath) / sizeof(kQuestApproachPath[0]));
    plan.dungeon_entry = TravelPoint{-11570.0f, -19200.0f};
    plan.dungeon_exit = TravelPoint{17450.0f, 20000.0f};
    plan.start_map_id = GWA3::MapIds::MAGUS_STONES;
    plan.dungeon_map_id = GWA3::MapIds::ARACHNIS_HAUNT_LVL1;
    return plan;
}

const TravelPoint* GetQuestReturnPath(int& outCount) {
    outCount = static_cast<int>(sizeof(kQuestReturnPath) / sizeof(kQuestReturnPath[0]));
    return kQuestReturnPath;
}

DialogPlan GetCompletionDialogPlan() {
    return kRewardChest.reward_dialog;
}

const FlameStaffObjective* GetFlameStaffObjectives(int& outCount) {
    outCount = static_cast<int>(sizeof(kFlameStaffObjectives) / sizeof(kFlameStaffObjectives[0]));
    return kFlameStaffObjectives;
}

const SpiderEggCluster* GetSpiderEggClusters(int& outCount) {
    outCount = static_cast<int>(sizeof(kSpiderEggClusters) / sizeof(kSpiderEggClusters[0]));
    return kSpiderEggClusters;
}

KeyPickupObjective GetLevel1KeyObjective() {
    return kLevel1KeyObjective;
}

DoorOpenObjective GetLevel1ExitDoorObjective() {
    return kLevel1ExitDoor;
}

RewardChestObjective GetRewardChestObjective() {
    return kRewardChest;
}

} // namespace GWA3::Bot::ArachnisHaunt
