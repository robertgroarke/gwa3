#pragma once

#include <gwa3/dungeon/DungeonQuest.h>
#include <gwa3/dungeon/DungeonRoute.h>

#include <cstdint>

namespace GWA3::Bot::ArachnisHaunt {

enum class StageId : uint8_t {
    RunRataSumToMagusStones,
    RunMagusToDungeon,
    Level1,
    Level2,
};

enum class RouteId : uint8_t {
    RunRataSumToMagusStones,
    RunMagusToDungeon,
    Level1Phase1,
    Level1Phase2,
    Level1Phase3,
    Level1Phase4,
    Level1Exit,
    Level2Phase1Approach,
    Level2Phase1Transit,
    Level2Phase2,
    Level2Phase3,
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
    uint32_t required_title_id = 0x27u;
    uint32_t accept_dialog_id = 0x84u;
};

struct FlameStaffObjective {
    const char* name = "";
    DungeonQuest::TravelPoint pickup_point = {};
    const DungeonQuest::TravelPoint* web_clear_path = nullptr;
    int web_clear_path_count = 0;
    DungeonQuest::TravelPoint drop_point = {};
};

struct SpiderEggCluster {
    const char* name = "";
    const DungeonQuest::TravelPoint* egg_points = nullptr;
    int egg_count = 0;
    const DungeonQuest::TravelPoint* post_cluster_path = nullptr;
    int post_cluster_path_count = 0;
};

struct KeyPickupObjective {
    const char* name = "";
    const DungeonQuest::TravelPoint* approach_path = nullptr;
    int approach_path_count = 0;
    DungeonQuest::TravelPoint pickup_point = {};
    int pickup_attempts = 0;
    const DungeonQuest::TravelPoint* web_clear_path = nullptr;
    int web_clear_path_count = 0;
    DungeonQuest::TravelPoint drop_point = {};
};

struct DoorOpenObjective {
    const char* name = "";
    DungeonQuest::TravelPoint interact_point = {};
    int interact_repeats = 0;
    DungeonQuest::TravelPoint zone_point = {};
};

struct RewardChestObjective {
    DungeonQuest::TravelPoint chest_point = {};
    int chest_interact_passes = 0;
    int chest_interact_count = 0;
    uint32_t chest_interact_delay_ms = 0u;
    int chest_loot_passes = 0;
    int chest_loot_attempts = 0;
    uint32_t chest_loot_delay_ms = 0u;
    DungeonQuest::QuestNpcAnchor completion_npc = {};
    DungeonQuest::DialogPlan reward_dialog = {};
    DungeonQuest::TravelPoint post_completion_point = {};
};

constexpr int REWARD_DIALOG_REPEAT_COUNT = 2;
constexpr int QUEST_DIALOG_REPEAT_COUNT = 3;
constexpr int COMPLETION_DIALOG_REPEAT_COUNT = 2;

const RouteDefinition& GetStageDefinition(StageId stageId);
const RouteDefinition* FindStageDefinitionByMapId(uint32_t mapId);
const RouteDefinition& GetRouteDefinition(RouteId routeId);
bool UsesAggroTraversal(RouteId routeId);
const BlessingAnchor* GetBlessingAnchors(StageId stageId, int& outCount);
const BlessingAnchor* FindBlessingAnchor(StageId stageId, int nearestWaypointIndex);
const BlessingAnchor* GetBlessingAnchors(RouteId routeId, int& outCount);
const BlessingAnchor* FindBlessingAnchor(RouteId routeId, int nearestWaypointIndex);
DungeonQuest::QuestCyclePlan GetQuestCyclePlan();
const DungeonQuest::TravelPoint* GetQuestReturnPath(int& outCount);
DungeonQuest::DialogPlan GetCompletionDialogPlan();
const FlameStaffObjective* GetFlameStaffObjectives(int& outCount);
const SpiderEggCluster* GetSpiderEggClusters(int& outCount);
KeyPickupObjective GetLevel1KeyObjective();
DoorOpenObjective GetLevel1ExitDoorObjective();
RewardChestObjective GetRewardChestObjective();

} // namespace GWA3::Bot::ArachnisHaunt
