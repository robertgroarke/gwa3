#include <bots/rragars_menagerie/RragarsMenagerie.h>
#include <gwa3/game/MapIds.h>

namespace GWA3::Bot::RragarsMenagerie {

using DungeonRoute::Waypoint;
using DungeonCheckpoint::CheckpointFailureAction;
using DungeonCheckpoint::CheckpointRetryPolicy;

namespace {

constexpr Waypoint kRunToDaladaWaypoints[] = {
    {-15786.0f, 6446.0f, 1300.0f, "1"},
    {-13207.0f, 2226.0f, 1300.0f, "2"},
    {-14210.0f, -32.0f, 1300.0f, "3"},
    {-17176.0f, -624.0f, 1300.0f, "4"},
    {-19946.0f, 151.0f, 1300.0f, "5"},
};

constexpr Waypoint kRunToGrothmarWaypoints[] = {
    {20885.0f, 13259.0f, 1300.0f, "1"},
    {17459.0f, 9108.0f, 1300.0f, "2"},
    {11175.0f, 2832.0f, 1300.0f, "3"},
    {15194.0f, -4046.0f, 1300.0f, "4"},
    {18718.0f, -6481.0f, 1300.0f, "5"},
    {20607.0f, -9289.0f, 1300.0f, "6"},
    {21930.0f, -13266.0f, 1300.0f, "7"},
};

constexpr Waypoint kRunToDungeonWaypoints[] = {
    {-17781.0f, 16242.0f, 1300.0f, "1"},
    {-16047.0f, 16691.0f, 1300.0f, "2"},
    {-17668.0f, 9141.0f, 1300.0f, "3"},
    {-13134.0f, 333.0f, 1300.0f, "4"},
    {-11122.0f, -2750.0f, 1300.0f, "5"},
    {-9369.0f, -5365.0f, 1300.0f, "6"},
    {-13949.0f, -11709.0f, 1300.0f, "7"},
    {-18585.0f, -15944.0f, 1300.0f, "8"},
};

constexpr Waypoint kLevel1Waypoints[] = {
    {4705.0f, -18700.0f, 1200.0f, "1"},
    {2590.0f, -17622.0f, 1200.0f, "3"},
    {-1005.0f, -14034.0f, 1200.0f, "Quest Checkpoint"},
    {2590.0f, -17622.0f, 1200.0f, "3"},
    {4705.0f, -18700.0f, 1200.0f, "1"},
    {1954.0f, -16205.0f, 1200.0f, "Keg"},
    {2590.0f, -17622.0f, 1200.0f, "3"},
    {-1005.0f, -14034.0f, 1200.0f, "4"},
    {-545.0f, -13200.0f, 1200.0f, "Blast Door"},
    {2143.0f, -11360.0f, 1200.0f, "Blast Door Checkpoint 1"},
    {3390.0f, -11635.0f, 1200.0f, "7"},
    {8056.0f, -8593.0f, 1200.0f, "8"},
    {10292.0f, -3916.0f, 1200.0f, "Dungeon Key"},
    {8056.0f, -8593.0f, 1200.0f, "9"},
    {3390.0f, -11635.0f, 1200.0f, "5"},
    {2143.0f, -11360.0f, 1200.0f, "4"},
    {-575.0f, -13303.0f, 1200.0f, "3"},
    {-1400.0f, -13607.0f, 1300.0f, "12"},
    {-3333.0f, -12512.0f, 1300.0f, "13"},
    {-6235.0f, -9584.0f, 1200.0f, "14"},
    {-12226.0f, -12296.0f, 1200.0f, "15"},
    {-14565.0f, -8800.0f, 1200.0f, "17"},
    {-13502.0f, -6860.0f, 600.0f, "18"},
    {-10570.0f, -5855.0f, 600.0f, "19"},
    {-13502.0f, -6860.0f, 600.0f, "18"},
    {-14565.0f, -8800.0f, 1200.0f, "17"},
    {-11575.0f, -12451.0f, 300.0f, "Keg"},
    {-14565.0f, -8800.0f, 1200.0f, "17"},
    {-13502.0f, -6860.0f, 600.0f, "18"},
    {-10570.0f, -5855.0f, 600.0f, "19"},
    {-11477.0f, -4091.0f, 300.0f, "Blast Door"},
    {-13427.0f, -2843.0f, 1200.0f, "Blast Door Checkpoint 2"},
    {-14312.0f, 207.0f, 1200.0f, "22"},
    {-16800.0f, -949.0f, 300.0f, "Boss lock"},
};

constexpr Waypoint kLevel2Waypoints[] = {
    {17891.0f, 15602.0f, 1200.0f, "1"},
    {17245.0f, 13626.0f, 600.0f, "2"},
    {15975.0f, 17719.0f, 600.0f, "3"},
    {12686.0f, 16182.0f, 300.0f, "4"},
    {10736.0f, 10150.0f, 300.0f, "5"},
    {8428.0f, 11205.0f, 1200.0f, "6"},
    {3765.0f, 9690.0f, 1200.0f, "15"},
    {860.0f, 5196.0f, 1200.0f, "16"},
    {-4492.0f, 1583.0f, 1200.0f, "17"},
    {-6826.0f, -2666.0f, 1200.0f, "18"},
    {-8239.0f, -5305.0f, 1200.0f, "19"},
    {-10771.0f, -4068.0f, 1200.0f, "20"},
    {-11969.0f, -2882.0f, 1200.0f, "Dungeon Key"},
    {-10771.0f, -4068.0f, 1200.0f, "20"},
    {-8239.0f, -5305.0f, 1200.0f, "19"},
    {-6826.0f, -2666.0f, 1200.0f, "18"},
    {-2302.0f, -2804.0f, 1200.0f, "25"},
    {-2258.0f, -7221.0f, 1200.0f, "26"},
    {1867.0f, -8762.0f, 1200.0f, "27"},
    {2750.0f, -14361.0f, 1200.0f, "28"},
    {7014.0f, -16040.0f, 1200.0f, "29"},
    {8846.0f, -15965.0f, 1200.0f, "30"},
    {10605.0f, -15736.0f, 1200.0f, "Boss lock"},
    {12058.0f, -15933.0f, 1200.0f, "32"},
    {11616.0f, -18108.0f, 1200.0f, "33"},
};

constexpr Waypoint kLevel3Waypoints[] = {
    {18445.0f, 15811.0f, 1200.0f, "1"},
    {18105.0f, 12676.0f, 1200.0f, "2"},
    {15893.0f, 10018.0f, 1200.0f, "3"},
    {10583.0f, 6765.0f, 1200.0f, "4"},
    {6816.0f, 5626.0f, 1200.0f, "5"},
    {3814.0f, -542.0f, 1200.0f, "6"},
    {534.0f, -375.0f, 1200.0f, "Keg"},
    {534.0f, -375.0f, 1200.0f, "Blast Door"},
    {-876.0f, -4002.0f, 1200.0f, "9"},
    {-439.0f, -7735.0f, 1200.0f, "10"},
    {1019.0f, -9599.0f, 1200.0f, "11"},
    {1294.0f, -9648.0f, 1200.0f, "Dungeon Key"},
    {-439.0f, -7735.0f, 1200.0f, "10"},
    {-876.0f, -4002.0f, 1200.0f, "9"},
    {575.0f, 1209.0f, 1200.0f, "15"},
    {-605.0f, 5108.0f, 1200.0f, "16"},
    {-1199.0f, 11547.0f, 1200.0f, "17"},
    {-4144.0f, 10220.0f, 1200.0f, "18"},
    {-6884.0f, 12451.0f, 1200.0f, "19"},
    {-6925.0f, 13984.0f, 1200.0f, "Boss lock"},
    {-6925.0f, 13984.0f, 1200.0f, "21"},
    {-2227.0f, 16031.0f, 1200.0f, "22"},
};

constexpr RouteDefinition kRoutes[] = {
    {"RunDaladaToGrothmar", GWA3::MapIds::DALADA_UPLANDS, GWA3::MapIds::GROTHMAR_WARDOWNS, kRunToDaladaWaypoints, static_cast<int>(sizeof(kRunToDaladaWaypoints) / sizeof(kRunToDaladaWaypoints[0]))},
    {"RunGrothmarToSacnoth", GWA3::MapIds::GROTHMAR_WARDOWNS, GWA3::MapIds::SACNOTH_VALLEY, kRunToGrothmarWaypoints, static_cast<int>(sizeof(kRunToGrothmarWaypoints) / sizeof(kRunToGrothmarWaypoints[0]))},
    {"RunSacnothToDungeon", GWA3::MapIds::SACNOTH_VALLEY, GWA3::MapIds::RRAGARS_MENAGERIE_LVL1, kRunToDungeonWaypoints, static_cast<int>(sizeof(kRunToDungeonWaypoints) / sizeof(kRunToDungeonWaypoints[0]))},
    {"Level1", GWA3::MapIds::RRAGARS_MENAGERIE_LVL1, GWA3::MapIds::RRAGARS_MENAGERIE_LVL2, kLevel1Waypoints, static_cast<int>(sizeof(kLevel1Waypoints) / sizeof(kLevel1Waypoints[0]))},
    {"Level2", GWA3::MapIds::RRAGARS_MENAGERIE_LVL2, GWA3::MapIds::RRAGARS_MENAGERIE_LVL3, kLevel2Waypoints, static_cast<int>(sizeof(kLevel2Waypoints) / sizeof(kLevel2Waypoints[0]))},
    {"Level3", GWA3::MapIds::RRAGARS_MENAGERIE_LVL3, GWA3::MapIds::DOOMLORE_SHRINE, kLevel3Waypoints, static_cast<int>(sizeof(kLevel3Waypoints) / sizeof(kLevel3Waypoints[0]))},
};

constexpr BlessingAnchor kRunToDaladaBlessings[] = {
    {0, -14971.0f, 11013.0f},
};

constexpr BlessingAnchor kRunToGrothmarBlessings[] = {
    {0, 22884.0f, 14606.0f},
};

constexpr BlessingAnchor kRunToDungeonBlessings[] = {
    {0, -16047.0f, 16691.0f},
    {3, -12558.0f, 221.0f},
};

constexpr BlessingAnchor kLevel1Blessings[] = {
    {0, 3975.0f, -18471.0f},
    {20, -11685.0f, -12242.0f},
};

constexpr BlessingAnchor kLevel2Blessings[] = {
    {0, 18111.0f, 16274.0f},
    {8, -6826.0f, -2666.0f},
};

constexpr BlessingAnchor kLevel3Blessings[] = {
    {0, 18871.0f, 16634.0f},
    {5, 4258.0f, -751.0f},
    {20, -7268.0f, 17252.0f},
};

constexpr CheckpointRetryPolicy kLevel1CheckpointPolicies[] = {
    {"Quest Checkpoint", CheckpointFailureAction::AbortRun, 0, 0},
    {"Blast Door Checkpoint 1", CheckpointFailureAction::BacktrackRetry, 4, 3},
    {"Blast Door Checkpoint 2", CheckpointFailureAction::BacktrackRetry, 5, 25},
};

constexpr LootObjective kLootObjectives[] = {
    {RouteId::Level1, 10292.0f, -3916.0f, 3},
    {RouteId::Level2, -11969.0f, -2882.0f, 3},
    {RouteId::Level3, 1294.0f, -9648.0f, 3},
};

constexpr RewardChestObjective kRewardChest = {
    -364.0f,
    17572.0f,
    130.0f,
    17437.0f,
    2,
    2,
};

constexpr ZoneTransitionPoint kZoneTransitionPoints[] = {
    {RouteId::RunDaladaToGrothmar, -20600.0f, 430.0f},
    {RouteId::RunGrothmarToSacnoth, 23320.0f, -13476.0f},
    {RouteId::RunSacnothToDungeon, -19600.0f, -15945.0f},
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
    case RouteId::RunDaladaToGrothmar:
        outCount = static_cast<int>(sizeof(kRunToDaladaBlessings) / sizeof(kRunToDaladaBlessings[0]));
        return kRunToDaladaBlessings;
    case RouteId::RunGrothmarToSacnoth:
        outCount = static_cast<int>(sizeof(kRunToGrothmarBlessings) / sizeof(kRunToGrothmarBlessings[0]));
        return kRunToGrothmarBlessings;
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

const CheckpointRetryPolicy* GetCheckpointPolicies(RouteId routeId, int& outCount) {
    switch (routeId) {
    case RouteId::Level1:
        outCount = static_cast<int>(sizeof(kLevel1CheckpointPolicies) / sizeof(kLevel1CheckpointPolicies[0]));
        return kLevel1CheckpointPolicies;
    default:
        outCount = 0;
        return nullptr;
    }
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

const ZoneTransitionPoint* FindZoneTransitionPoint(RouteId routeId) {
    for (const auto& point : kZoneTransitionPoints) {
        if (point.route_id == routeId) {
            return &point;
        }
    }
    return nullptr;
}

WaypointBehavior ResolveWaypointBehavior(const char* label) {
    switch (DungeonRoute::ClassifyWaypointLabel(label)) {
    case DungeonRoute::WaypointLabelKind::Keg:
        return WaypointBehavior::PickUpKeg;
    case DungeonRoute::WaypointLabelKind::BlastDoor:
        return WaypointBehavior::DropKegAtBlastDoor;
    case DungeonRoute::WaypointLabelKind::QuestCheckpoint:
        return WaypointBehavior::ValidateQuestCheckpoint;
    case DungeonRoute::WaypointLabelKind::BlastDoorCheckpoint:
        return WaypointBehavior::ValidateRetryCheckpoint;
    case DungeonRoute::WaypointLabelKind::DungeonKey:
        return WaypointBehavior::PickUpDungeonKey;
    case DungeonRoute::WaypointLabelKind::BossLock:
    case DungeonRoute::WaypointLabelKind::Chest:
    case DungeonRoute::WaypointLabelKind::Signpost:
        return WaypointBehavior::DoubleInteract;
    default:
        return WaypointBehavior::StandardMove;
    }
}

WaypointExecutionPlan BuildWaypointExecutionPlan(RouteId routeId, int waypointIndex) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    WaypointExecutionPlan plan = DungeonRoute::MakeWaypointExecutionPlan<WaypointExecutionPlan>(
        route.waypoints,
        route.waypoint_count,
        waypointIndex,
        &ResolveWaypointBehavior);
    if (plan.waypoint == nullptr) {
        return plan;
    }

    int checkpointCount = 0;
    const CheckpointRetryPolicy* policies = GetCheckpointPolicies(routeId, checkpointCount);
    plan.checkpoint_policy = DungeonCheckpoint::FindCheckpointPolicy(
        policies,
        checkpointCount,
        plan.waypoint->label);
    return plan;
}

} // namespace GWA3::Bot::RragarsMenagerie
