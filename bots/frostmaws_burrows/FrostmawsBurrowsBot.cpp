#include <bots/frostmaws_burrows/FrostmawsBurrowsBot.h>

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <bots/frostmaws_burrows/FrostmawsBurrows.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <Windows.h>

namespace GWA3::Bot::FrostmawsBurrowsBot {

using namespace GWA3::Bot;
using namespace GWA3::Bot::FrostmawsBurrows;

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
        LogBot("Frostmaw: no player agent available for route %s", route.name);
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
        const auto plan = BuildWaypointExecutionPlan(routeId, i);
        if (!plan.waypoint || !MoveToWaypoint(*plan.waypoint, route.map_id)) {
            return false;
        }
    }

    if (!waitForTransition) {
        return true;
    }

    switch (routeId) {
    case RouteId::RunSifhallaToJagaMoraine:
        return ZoneThroughPoint(16788.0f, 22797.0f, GWA3::MapIds::JAGA_MORAINE);
    case RouteId::Level1:
        return ZoneThroughPoint(-10760.0f, 10900.0f, GWA3::MapIds::FROSTMAWS_BURROWS_LVL2);
    case RouteId::Level2:
        return ZoneThroughPoint(13875.0f, -19445.0f, GWA3::MapIds::FROSTMAWS_BURROWS_LVL3);
    case RouteId::Level3:
        return ZoneThroughPoint(17887.0f, 15830.0f, GWA3::MapIds::FROSTMAWS_BURROWS_LVL4);
    default:
        return true;
    }
}

bool ExecuteQuestBootstrap() {
    const auto bootstrap = GetQuestBootstrapPlan();
    return DungeonQuestRuntime::ExecuteBootstrapPlan(bootstrap);
}

BotState HandleCharSelect(BotConfig&) {
    return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

BotState HandleTownSetup(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::SIFHALLA || mapId == GWA3::MapIds::JAGA_MORAINE) {
        return BotState::Traveling;
    }
    if (mapId == GWA3::MapIds::FROSTMAWS_BURROWS_LVL1 || mapId == GWA3::MapIds::FROSTMAWS_BURROWS_LVL2 ||
        mapId == GWA3::MapIds::FROSTMAWS_BURROWS_LVL3 || mapId == GWA3::MapIds::FROSTMAWS_BURROWS_LVL4) {
        return BotState::InDungeon;
    }

    LogBot("Frostmaw: traveling to Sifhalla from map %u", mapId);
    MapMgr::Travel(GWA3::MapIds::SIFHALLA);
    if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::SIFHALLA, 60000u)) {
        return BotState::Error;
    }
    return BotState::Traveling;
}

BotState HandleTravel(BotConfig&) {
    switch (MapMgr::GetMapId()) {
    case GWA3::MapIds::SIFHALLA:
        return ExecuteRoute(RouteId::RunSifhallaToJagaMoraine, true) ? BotState::Traveling : BotState::Error;
    case GWA3::MapIds::JAGA_MORAINE:
        if (!ExecuteRoute(RouteId::RunJagaMoraineToDungeon, false)) {
            return BotState::Error;
        }
        return ExecuteQuestBootstrap() ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::FROSTMAWS_BURROWS_LVL1:
    case GWA3::MapIds::FROSTMAWS_BURROWS_LVL2:
    case GWA3::MapIds::FROSTMAWS_BURROWS_LVL3:
    case GWA3::MapIds::FROSTMAWS_BURROWS_LVL4:
        return BotState::InDungeon;
    default:
        LogBot("Frostmaw: unsupported travel map %u", MapMgr::GetMapId());
        return BotState::Error;
    }
}

BotState HandleDungeon(BotConfig&) {
    switch (MapMgr::GetMapId()) {
    case GWA3::MapIds::FROSTMAWS_BURROWS_LVL1:
        return ExecuteRoute(RouteId::Level1, true) ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::FROSTMAWS_BURROWS_LVL2:
        return ExecuteRoute(RouteId::Level2, true) ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::FROSTMAWS_BURROWS_LVL3:
        return ExecuteRoute(RouteId::Level3, true) ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::FROSTMAWS_BURROWS_LVL4:
        LogBot("Frostmaw: level 5 source is missing; stopping after level 4 route");
        return ExecuteRoute(RouteId::Level4, false) ? BotState::Error : BotState::Error;
    default:
        return BotState::InTown;
    }
}

BotState HandleError(BotConfig&) {
    LogBot("Frostmaw: ERROR state - waiting before retry");
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
    cfg.target_map_id = GWA3::MapIds::FROSTMAWS_BURROWS_LVL1;
    cfg.outpost_map_id = GWA3::MapIds::SIFHALLA;
    cfg.bot_module_name = "FrostmawsBurrows";

    LogBot("Frostmaws Burrows module registered (runtime in-progress)");
}

} // namespace GWA3::Bot::FrostmawsBurrowsBot
