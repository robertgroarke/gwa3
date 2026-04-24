#pragma once

#include <gwa3/bot/DungeonQuest.h>
#include <gwa3/bot/DungeonRoute.h>

#include <cstdint>

namespace GWA3::Bot::FrostmawsBurrows {

enum class RouteId : uint8_t {
    RunSifhallaToJagaMoraine,
    RunJagaMoraineToDungeon,
    Level1,
    Level2,
    Level3,
    Level4,
};

enum class WaypointBehavior : uint8_t {
    StandardMove,
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

constexpr uint32_t MAP_JAGA_MORAINE = 546u;
constexpr uint32_t MAP_FROSTMAWS_BURROWS_LVL1 = 630u;
constexpr uint32_t MAP_FROSTMAWS_BURROWS_LVL2 = 631u;
constexpr uint32_t MAP_FROSTMAWS_BURROWS_LVL3 = 632u;
constexpr uint32_t MAP_FROSTMAWS_BURROWS_LVL4 = 633u;
constexpr uint32_t MAP_FROSTMAWS_BURROWS_LVL5 = 634u;
constexpr uint32_t MAP_SIFHALLA = 643u;
constexpr uint32_t MAP_DOOMLORE_SHRINE = 648u;

constexpr uint32_t DIALOG_ACCEPT = 0x8101u;
constexpr uint32_t DIALOG_FROSTMAW_QUEST = 0x832A01u;
constexpr int QUEST_DIALOG_REPEAT_COUNT = 3;
constexpr bool HAS_LEVEL5_ROUTE_SOURCE = false;

const RouteDefinition& GetRouteDefinition(RouteId routeId);
const RouteDefinition* FindRouteDefinitionByMapId(uint32_t mapId);
const BlessingAnchor* GetBlessingAnchors(RouteId routeId, int& outCount);
const BlessingAnchor* FindBlessingAnchor(RouteId routeId, int nearestWaypointIndex);
const uint32_t* GetQuestDialogSequence(int& outCount);
DungeonQuest::QuestNpcAnchor GetQuestNpcAnchor();
const DungeonQuest::TravelPoint* GetQuestEntryPath(int& outCount);
const DungeonQuest::TravelPoint* GetQuestResetPath(int& outCount);
DungeonQuest::BootstrapPlan GetQuestBootstrapPlan();
WaypointBehavior ResolveWaypointBehavior(const char* label);
WaypointExecutionPlan BuildWaypointExecutionPlan(RouteId routeId, int waypointIndex);

} // namespace GWA3::Bot::FrostmawsBurrows
