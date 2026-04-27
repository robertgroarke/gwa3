#pragma once

#include <gwa3/dungeon/DungeonQuest.h>
#include <gwa3/dungeon/DungeonRoute.h>

#include <cstdint>

namespace GWA3::Bot::Kathandrax {

enum class RouteId : uint8_t {
    RunDoomloreToDalada,
    RunDaladaToSacnoth,
    RunSacnothToDungeon,
    Level1,
    Level2,
    Level3,
};

enum class WaypointBehavior : uint8_t {
    StandardMove,
    PickUpDungeonKey,
    DoubleInteract,
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

struct WaypointExecutionPlan {
    const DungeonRoute::Waypoint* waypoint = nullptr;
    WaypointBehavior behavior = WaypointBehavior::StandardMove;
};

struct RewardChestObjective {
    DungeonQuest::TravelPoint staging_point = {};
    DungeonQuest::TravelPoint search_point = {};
    int interact_repeats = 0;
    int pickup_attempts = 0;
};

constexpr int QUEST_DIALOG_REPEAT_COUNT = 1;

const RouteDefinition& GetRouteDefinition(RouteId routeId);
const RouteDefinition* FindRouteDefinitionByMapId(uint32_t mapId);
const BlessingAnchor* GetBlessingAnchors(RouteId routeId, int& outCount);
const BlessingAnchor* FindBlessingAnchor(RouteId routeId, int nearestWaypointIndex);
const uint32_t* GetQuestDialogSequence(int& outCount);
DungeonQuest::QuestNpcAnchor GetQuestNpcAnchor();
const DungeonQuest::TravelPoint* GetQuestEntryPath(int& outCount);
DungeonQuest::BootstrapPlan GetQuestBootstrapPlan();
RewardChestObjective GetRewardChestObjective();
WaypointBehavior ResolveWaypointBehavior(const char* label);
WaypointExecutionPlan BuildWaypointExecutionPlan(RouteId routeId, int waypointIndex);

} // namespace GWA3::Bot::Kathandrax
