#include <bots/kathandrax/KathandraxBot.h>

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <bots/kathandrax/Kathandrax.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <Windows.h>

namespace GWA3::Bot::KathandraxBot {

using namespace GWA3::Bot;
using namespace GWA3::Bot::Kathandrax;

namespace {

void WaitMs(DWORD ms) {
    Sleep(ms);
}

bool MoveToWaypoint(const DungeonRoute::Waypoint& waypoint, uint32_t mapId, float tolerance = 250.0f) {
    return DungeonNavigation::MoveToAndWait(
        waypoint.x,
        waypoint.y,
        tolerance,
        30000u,
        1000u,
        mapId).arrived;
}

bool ZoneThroughPoint(float x, float y, uint32_t targetMapId, uint32_t timeoutMs = 60000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        AgentMgr::Move(x, y);
        if (MapMgr::GetMapId() == targetMapId) {
            return true;
        }
        if (DungeonNavigation::WaitForMapId(targetMapId, 250u)) {
            return true;
        }
        WaitMs(250u);
    }
    return false;
}

bool ExecuteRoute(RouteId routeId, bool waitForTransition) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        LogBot("Kathandrax: no player agent available for route %s", route.name);
        return false;
    }

    const int startIndex = DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);

    if (const BlessingAnchor* blessing = FindBlessingAnchor(routeId, startIndex)) {
        DungeonNavigation::MoveToAndWait(blessing->x, blessing->y, 300.0f, 15000u, 1000u, route.map_id);
    }

    for (int i = startIndex; i < route.waypoint_count; ++i) {
        const WaypointExecutionPlan plan = BuildWaypointExecutionPlan(routeId, i);
        if (!plan.waypoint) {
            return false;
        }

        switch (plan.behavior) {
        case WaypointBehavior::StandardMove:
            if (!MoveToWaypoint(*plan.waypoint, route.map_id)) {
                LogBot("Kathandrax: failed moving to waypoint %d (%s) on %s",
                       i, plan.waypoint->label, route.name);
                return false;
            }
            break;
        case WaypointBehavior::PickUpDungeonKey: {
            if (!MoveToWaypoint(*plan.waypoint, route.map_id)) {
                return false;
            }
            const uint32_t itemId = DungeonInteractions::FindNearestItem(
                plan.waypoint->x,
                plan.waypoint->y,
                1200.0f);
            if (itemId != 0u) {
                ItemMgr::PickUpItem(itemId);
                WaitMs(1000u);
            }
            break;
        }
        case WaypointBehavior::DoubleInteract: {
            if (!MoveToWaypoint(*plan.waypoint, route.map_id, 200.0f)) {
                return false;
            }
            const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(
                plan.waypoint->x,
                plan.waypoint->y,
                1500.0f);
            if (signpostId == 0u) {
                LogBot("Kathandrax: no signpost found near waypoint %d (%s) on %s",
                       i, plan.waypoint->label, route.name);
                return false;
            }
            AgentMgr::InteractSignpost(signpostId);
            WaitMs(1000u);
            AgentMgr::InteractSignpost(signpostId);
            WaitMs(1000u);
            break;
        }
        default:
            return false;
        }
    }

    if (!waitForTransition) {
        return true;
    }

    switch (routeId) {
    case RouteId::RunDoomloreToDalada:
        return ZoneThroughPoint(-15366.0f, 13553.0f, GWA3::MapIds::DALADA_UPLANDS);
    case RouteId::RunDaladaToSacnoth:
        return ZoneThroughPoint(14390.0f, -20300.0f, GWA3::MapIds::SACNOTH_VALLEY);
    case RouteId::Level1:
        return ZoneThroughPoint(-16946.0f, -3014.0f, GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL2);
    case RouteId::Level2:
        return ZoneThroughPoint(-16864.0f, -539.0f, GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL3);
    default:
        return true;
    }
}

bool ExecuteQuestBootstrap() {
    const auto bootstrap = GetQuestBootstrapPlan();
    return DungeonQuestRuntime::ExecuteBootstrapPlan(bootstrap);
}

bool ExecuteRewardChestFlow() {
    const auto reward = GetRewardChestObjective();
    const RouteDefinition& route = GetRouteDefinition(RouteId::Level3);
    const int loopStart = route.waypoint_count >= 4 ? route.waypoint_count - 4 : 0;
    const DWORD searchStart = GetTickCount();
    uint32_t signpostId = 0u;

    while ((GetTickCount() - searchStart) < 60000u) {
        signpostId = DungeonInteractions::FindNearestSignpost(
            reward.search_point.x,
            reward.search_point.y,
            1500.0f);
        if (signpostId != 0u) {
            break;
        }

        for (int i = loopStart; i < route.waypoint_count; ++i) {
            if (!MoveToWaypoint(route.waypoints[i], route.map_id)) {
                return false;
            }
        }
        WaitMs(500u);
    }

    if (signpostId == 0u) {
        LogBot("Kathandrax: reward chest signpost did not appear");
        return false;
    }

    DungeonNavigation::MoveToAndWait(
        reward.staging_point.x,
        reward.staging_point.y,
        250.0f,
        15000u,
        1000u,
        GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL3);

    for (int i = 0; i < reward.interact_repeats; ++i) {
        AgentMgr::InteractSignpost(signpostId);
        WaitMs(5000u);
        for (int pickup = 0; pickup < reward.pickup_attempts; ++pickup) {
            const uint32_t itemId = DungeonInteractions::FindNearestItem(
                reward.search_point.x,
                reward.search_point.y,
                1500.0f);
            if (itemId != 0u) {
                ItemMgr::PickUpItem(itemId);
                WaitMs(1000u);
            }
        }
    }

    return DungeonNavigation::WaitForMapId(GWA3::MapIds::SACNOTH_VALLEY, 180000u);
}

BotState HandleCharSelect(BotConfig&) {
    return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

BotState HandleTownSetup(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::DOOMLORE_SHRINE || mapId == GWA3::MapIds::DALADA_UPLANDS || mapId == GWA3::MapIds::SACNOTH_VALLEY) {
        return BotState::Traveling;
    }
    if (mapId == GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL1 || mapId == GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL2 || mapId == GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL3) {
        return BotState::InDungeon;
    }

    LogBot("Kathandrax: traveling to Doomlore Shrine from map %u", mapId);
    MapMgr::Travel(GWA3::MapIds::DOOMLORE_SHRINE);
    if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::DOOMLORE_SHRINE, 60000u)) {
        return BotState::Error;
    }
    return BotState::Traveling;
}

BotState HandleTravel(BotConfig&) {
    switch (MapMgr::GetMapId()) {
    case GWA3::MapIds::DOOMLORE_SHRINE:
        return ExecuteRoute(RouteId::RunDoomloreToDalada, true) ? BotState::Traveling : BotState::Error;
    case GWA3::MapIds::DALADA_UPLANDS:
        return ExecuteRoute(RouteId::RunDaladaToSacnoth, true) ? BotState::Traveling : BotState::Error;
    case GWA3::MapIds::SACNOTH_VALLEY:
        if (!ExecuteRoute(RouteId::RunSacnothToDungeon, false)) {
            return BotState::Error;
        }
        return ExecuteQuestBootstrap() ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL1:
    case GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL2:
    case GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL3:
        return BotState::InDungeon;
    default:
        LogBot("Kathandrax: unsupported travel map %u", MapMgr::GetMapId());
        return BotState::Error;
    }
}

BotState HandleDungeon(BotConfig&) {
    switch (MapMgr::GetMapId()) {
    case GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL1:
        return ExecuteRoute(RouteId::Level1, true) ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL2:
        return ExecuteRoute(RouteId::Level2, true) ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL3:
        if (!ExecuteRoute(RouteId::Level3, false)) {
            return BotState::Error;
        }
        return ExecuteRewardChestFlow() ? BotState::InTown : BotState::Error;
    default:
        return BotState::InTown;
    }
}

BotState HandleError(BotConfig&) {
    LogBot("Kathandrax: ERROR state - waiting before retry");
    WaitMs(5000u);
    return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

} // namespace

void Register() {
    Bot::RegisterStateHandler(BotState::CharSelect, HandleCharSelect);
    Bot::RegisterStateHandler(BotState::InTown, HandleTownSetup);
    Bot::RegisterStateHandler(BotState::Traveling, HandleTravel);
    Bot::RegisterStateHandler(BotState::InDungeon, HandleDungeon);
    Bot::RegisterStateHandler(BotState::Error, HandleError);

    auto& cfg = Bot::GetConfig();
    cfg.hard_mode = true;
    cfg.target_map_id = GWA3::MapIds::CATACOMBS_OF_KATHANDRAX_LVL1;
    cfg.outpost_map_id = GWA3::MapIds::DOOMLORE_SHRINE;
    cfg.bot_module_name = "Kathandrax";

    LogBot("Kathandrax module registered (runtime in-progress)");
}

} // namespace GWA3::Bot::KathandraxBot
