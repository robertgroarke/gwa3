#pragma once

#include <gwa3/dungeon/DungeonQuest.h>
#include <gwa3/dungeon/DungeonRoute.h>

#include <cstdint>

namespace GWA3::Bot::RavensPoint {

enum class StageId : uint8_t {
    RunOlafsteadToVarajarFells,
    VarajarFells,
    Level1,
    Level2,
    Level3,
};

enum class RouteId : uint8_t {
    RunOlafsteadToVarajarFells,
    RunVarajarToBlessing,
    QuestApproach,
    QuestReturn,
    Level1Dispatch,
    Level1Torch1,
    Level1Torch2,
    Level1DoorKey,
    Level1DoorBacktrack,
    Level1Exit,
    Level2Dispatch,
    Level2Torch1,
    Level2Torch2,
    Level2Torch3,
    Level2BossKey,
    Level2BossKeyBacktrack,
    Level2Door,
    Level2Exit,
    Level3Approach,
    Level3BossLoop,
};

struct RouteDefinition {
    const char* name = "";
    uint32_t map_id = 0;
    uint32_t next_map_id = 0;
    const DungeonRoute::Waypoint* waypoints = nullptr;
    int waypoint_count = 0;
};

struct BlessingAnchor {
    int trigger_index = 0;
    float x = 0.0f;
    float y = 0.0f;
};

struct TorchObjective {
    RouteId route_id = RouteId::Level1Torch1;
    DungeonQuest::TravelPoint chest = {};
    const DungeonQuest::TravelPoint* transit_points = nullptr;
    int transit_point_count = 0;
    const DungeonQuest::TravelPoint* brazier_points = nullptr;
    int brazier_count = 0;
    DungeonQuest::TravelPoint drop_point = {};
    DungeonQuest::TravelPoint resume_point = {};
};

struct DoorObjective {
    RouteId route_id = RouteId::Level1DoorKey;
    DungeonQuest::TravelPoint interact_point = {};
    int interact_repeats = 0;
    DungeonQuest::TravelPoint resume_point = {};
};

struct LootObjective {
    RouteId route_id = RouteId::Level1DoorKey;
    DungeonQuest::TravelPoint pickup_point = {};
    int pickup_retries = 0;
};

struct RewardChestObjective {
    DungeonQuest::TravelPoint staging_point = {};
    DungeonQuest::TravelPoint search_point = {};
    int interact_repeats = 0;
    int pickup_attempts = 0;
};

constexpr int REWARD_DIALOG_REPEAT_COUNT = 2;
constexpr int QUEST_DIALOG_REPEAT_COUNT = 3;

const RouteDefinition& GetDispatchRouteDefinition(StageId stageId);
const RouteDefinition* FindDispatchRouteDefinitionByMapId(uint32_t mapId);
const RouteDefinition& GetRouteDefinition(RouteId routeId);
bool UsesAggroTraversal(RouteId routeId);
const BlessingAnchor* GetBlessingAnchors(RouteId routeId, int& outCount);
const BlessingAnchor* FindBlessingAnchor(RouteId routeId, int nearestWaypointIndex);
DungeonQuest::QuestCyclePlan GetQuestCyclePlan();
const TorchObjective* GetTorchObjectives(int& outCount);
const TorchObjective* FindTorchObjective(RouteId routeId);
const DoorObjective* GetDoorObjectives(int& outCount);
const DoorObjective* FindDoorObjective(RouteId routeId);
const LootObjective* GetLootObjectives(int& outCount);
const LootObjective* FindLootObjective(RouteId routeId);
RewardChestObjective GetRewardChestObjective();

} // namespace GWA3::Bot::RavensPoint
