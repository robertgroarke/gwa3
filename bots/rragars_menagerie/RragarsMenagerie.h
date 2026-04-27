#pragma once

#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonRoute.h>

#include <cstdint>

namespace GWA3::Bot::RragarsMenagerie {

enum class RouteId : uint8_t {
    RunDaladaToGrothmar,
    RunGrothmarToSacnoth,
    RunSacnothToDungeon,
    Level1,
    Level2,
    Level3,
};

enum class WaypointBehavior : uint8_t {
    StandardMove,
    PickUpKeg,
    DropKegAtBlastDoor,
    ValidateQuestCheckpoint,
    ValidateRetryCheckpoint,
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
    const DungeonCheckpoint::CheckpointRetryPolicy* checkpoint_policy = nullptr;
};

struct LootObjective {
    RouteId route_id = RouteId::Level1;
    float pickup_x = 0.0f;
    float pickup_y = 0.0f;
    int pickup_retries = 0;
};

struct RewardChestObjective {
    float staging_x = 0.0f;
    float staging_y = 0.0f;
    float chest_x = 0.0f;
    float chest_y = 0.0f;
    int interact_repeats = 0;
    int pickup_attempts = 0;
};

struct ZoneTransitionPoint {
    RouteId route_id = RouteId::RunDaladaToGrothmar;
    float x = 0.0f;
    float y = 0.0f;
};

const RouteDefinition& GetRouteDefinition(RouteId routeId);
const RouteDefinition* FindRouteDefinitionByMapId(uint32_t mapId);
const BlessingAnchor* GetBlessingAnchors(RouteId routeId, int& outCount);
const BlessingAnchor* FindBlessingAnchor(RouteId routeId, int nearestWaypointIndex);
const DungeonCheckpoint::CheckpointRetryPolicy* GetCheckpointPolicies(RouteId routeId, int& outCount);
const LootObjective* GetLootObjectives(int& outCount);
const LootObjective* FindLootObjective(RouteId routeId);
RewardChestObjective GetRewardChestObjective();
const ZoneTransitionPoint* FindZoneTransitionPoint(RouteId routeId);
WaypointBehavior ResolveWaypointBehavior(const char* label);
WaypointExecutionPlan BuildWaypointExecutionPlan(RouteId routeId, int waypointIndex);

} // namespace GWA3::Bot::RragarsMenagerie
