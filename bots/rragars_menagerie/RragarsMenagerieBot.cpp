#include <bots/rragars_menagerie/RragarsMenagerieBot.h>

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <bots/rragars_menagerie/RragarsMenagerie.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/SkillMgr.h>

#include <Windows.h>

namespace GWA3::Bot::RragarsMenagerieBot {

using namespace GWA3::Bot;
using namespace GWA3::Bot::RragarsMenagerie;

namespace {

constexpr uint32_t LEGACY_AGGRO_MOVE_TIMEOUT_MS = 240000u;
constexpr float kQuestNpcX = -19166.0f;
constexpr float kQuestNpcY = 17980.0f;

void WaitMs(DWORD ms) {
    Sleep(ms);
}

void WaitCombatMs(uint32_t ms) {
    WaitMs(ms);
}

void LogPlayerBundleDiagnostics(const char* label) {
    const uint32_t myId = AgentMgr::GetMyId();
    auto* skillbar = SkillMgr::GetPlayerSkillbar();
    const uint32_t heldBundle = DungeonInteractions::GetHeldBundleItemId();
    const uint32_t skill1 = skillbar ? skillbar->skills[0].skill_id : 0u;
    const uint32_t skill2 = skillbar ? skillbar->skills[1].skill_id : 0u;
    const uint32_t skill3 = skillbar ? skillbar->skills[2].skill_id : 0u;
    const uint32_t skill4 = skillbar ? skillbar->skills[3].skill_id : 0u;
    const uint32_t skill5 = skillbar ? skillbar->skills[4].skill_id : 0u;
    const uint32_t skill6 = skillbar ? skillbar->skills[5].skill_id : 0u;
    const uint32_t skill7 = skillbar ? skillbar->skills[6].skill_id : 0u;
    const uint32_t skill8 = skillbar ? skillbar->skills[7].skill_id : 0u;

    auto* effectArray = myId != 0u ? EffectMgr::GetAgentEffectArray(myId) : nullptr;
    auto* buffArray = myId != 0u ? EffectMgr::GetAgentBuffArray(myId) : nullptr;
    const uint32_t effectCount = effectArray ? effectArray->size : 0u;
    const uint32_t buffCount = buffArray ? buffArray->size : 0u;

    Log::Info(
        "RragarsDbg: bundle-state label=%s myId=%u heldBundle=%u "
        "skillbar=[%u,%u,%u,%u,%u,%u,%u,%u] effectCount=%u buffCount=%u",
        label ? label : "",
        myId,
        heldBundle,
        skill1,
        skill2,
        skill3,
        skill4,
        skill5,
        skill6,
        skill7,
        skill8,
        effectCount,
        buffCount);

    if (effectArray && effectArray->buffer) {
        const uint32_t emit = effectArray->size < 8u ? effectArray->size : 8u;
        for (uint32_t i = 0u; i < emit; ++i) {
            const Effect& effect = effectArray->buffer[i];
            Log::Info(
                "RragarsDbg: bundle-effect label=%s idx=%u skill=%u effectId=%u duration=%.1f",
                label ? label : "",
                i,
                effect.skill_id,
                effect.effect_id,
                effect.duration);
        }
    }

    if (buffArray && buffArray->buffer) {
        const uint32_t emit = buffArray->size < 8u ? buffArray->size : 8u;
        for (uint32_t i = 0u; i < emit; ++i) {
            const Buff& buff = buffArray->buffer[i];
            Log::Info(
                "RragarsDbg: bundle-buff label=%s idx=%u skill=%u buffId=%u",
                label ? label : "",
                i,
                buff.skill_id,
                buff.buff_id);
        }
    }
}

bool IsPlayerOrPartyDead() {
    auto* me = AgentMgr::GetMyAgent();
    return me == nullptr || me->hp <= 0.0f || PartyMgr::GetIsPartyDefeated();
}

bool IsCurrentMapLoaded() {
    return MapMgr::GetMapId() != 0u && MapMgr::GetLoadingState() == 1u;
}

void QueueAggroMove(float x, float y) {
    AgentMgr::Move(x, y);
}

void ConfigureLegacyAggroMoveOptions(
    DungeonCombat::AggroAdvanceOptions& options,
    uint32_t timeoutMs) {
    options.timeout_ms = timeoutMs;
    // Match the AutoIt AggroMoveToEX behavior more closely: only care about
    // foes inside the actual waypoint fight range instead of the shared
    // Froggy-style local clear floor.
    options.clear_options.extra_clear_range = 0.0f;
    options.clear_options.minimum_local_clear_range = 0.0f;
    options.clear_options.timeout_ms = timeoutMs;
    options.clear_options.target_timeout_ms = timeoutMs;
}

DungeonCombat::CombatCallbacks MakeRragarsCombatCallbacks() {
    DungeonCombat::CombatCallbacks callbacks;
    callbacks.is_dead = &IsPlayerOrPartyDead;
    callbacks.is_map_loaded = &IsCurrentMapLoaded;
    callbacks.wait_ms = &WaitCombatMs;
    callbacks.queue_move = &QueueAggroMove;
    callbacks.fight_target = &DungeonBuiltinCombat::FightTargetWithPriorityBuiltinCombat;
    return callbacks;
}

bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs = 15000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
            WaitMs(250u);
            continue;
        }

        if (AgentMgr::GetMyId() == 0u) {
            WaitMs(250u);
            continue;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (!me || me->hp <= 0.0f) {
            WaitMs(250u);
            continue;
        }

        if (me->x == 0.0f && me->y == 0.0f) {
            WaitMs(250u);
            continue;
        }

        return true;
    }
    return false;
}

bool WaitForSpawnAwayFromPoint(
    uint32_t mapId,
    float staleX,
    float staleY,
    float minimumDistance = 3000.0f,
    uint32_t timeoutMs = 15000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded() || AgentMgr::GetMyId() == 0u) {
            WaitMs(250u);
            continue;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (!me || me->hp <= 0.0f) {
            WaitMs(250u);
            continue;
        }

        if (me->x == 0.0f && me->y == 0.0f) {
            WaitMs(250u);
            continue;
        }

        if (AgentMgr::GetDistance(me->x, me->y, staleX, staleY) > minimumDistance) {
            return true;
        }

        WaitMs(200u);
    }
    return false;
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

bool MoveToPointWithAggro(
    float x,
    float y,
    uint32_t mapId,
    float tolerance,
    float fightRange,
    uint32_t timeoutMs) {
    if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
        return false;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (me && AgentMgr::GetDistance(me->x, me->y, x, y) <= tolerance) {
        return true;
    }

    DungeonCombat::AggroAdvanceOptions options;
    options.arrival_threshold = tolerance;
    options.clear_options.pickup_after_clear = false;
    options.clear_options.flag_heroes = false;
    options.clear_options.change_target = false;
    options.clear_options.call_target = false;
    ConfigureLegacyAggroMoveOptions(options, timeoutMs);
    const bool arrived = DungeonCombat::AdvanceWithAggro(
        x,
        y,
        fightRange,
        MakeRragarsCombatCallbacks(),
        options);
    if (!arrived) {
        LogBot("Rragars: aggro move timed out target=(%.0f, %.0f) map=%u remaining=%.0f fight_range=%.0f timeout=%u",
               x,
               y,
               mapId,
               DungeonCombat::DistanceToPoint(x, y),
               fightRange,
               timeoutMs);
    }
    return arrived;
}

bool MoveToWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    uint32_t mapId,
    float tolerance = 250.0f,
    uint32_t timeoutMs = LEGACY_AGGRO_MOVE_TIMEOUT_MS) {
    if (mapId != GWA3::MapIds::DOOMLORE_SHRINE) {
        const float fightRange = waypoint.fight_range > 0.0f ? waypoint.fight_range : 1200.0f;
        return MoveToPointWithAggro(
            waypoint.x,
            waypoint.y,
            mapId,
            tolerance,
            fightRange,
            timeoutMs);
    }

    return DungeonNavigation::MoveToAndWait(
        waypoint.x,
        waypoint.y,
        tolerance,
        timeoutMs,
        1000u,
        mapId).arrived;
}

bool MoveToPoint(
    float x,
    float y,
    uint32_t mapId,
    float tolerance = 250.0f,
    uint32_t timeoutMs = LEGACY_AGGRO_MOVE_TIMEOUT_MS,
    float fightRange = 1200.0f) {
    if (mapId != GWA3::MapIds::DOOMLORE_SHRINE) {
        return MoveToPointWithAggro(x, y, mapId, tolerance, fightRange, timeoutMs);
    }

    return DungeonNavigation::MoveToAndWait(
        x,
        y,
        tolerance,
        timeoutMs,
        1000u,
        mapId).arrived;
}

bool MoveCheckpointWaypointForRoute(const DungeonRoute::Waypoint& waypoint, const void* context) {
    const auto* route = static_cast<const RouteDefinition*>(context);
    return route != nullptr && MoveToWaypoint(waypoint, route->map_id);
}

bool IsExplorableTravelRoute(RouteId routeId) {
    switch (routeId) {
    case RouteId::RunDaladaToGrothmar:
    case RouteId::RunGrothmarToSacnoth:
    case RouteId::RunSacnothToDungeon:
        return true;
    default:
        return false;
    }
}

bool FollowTravelRouteWithRetries(RouteId routeId) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return false;
    }

    DungeonCombat::AggroAdvanceOptions aggroOptions;
    aggroOptions.clear_options.pickup_after_clear = false;
    aggroOptions.clear_options.flag_heroes = false;
    aggroOptions.clear_options.change_target = false;
    aggroOptions.clear_options.call_target = false;
    uint32_t waypointTimeoutMs = 120000u;

    if (routeId == RouteId::RunSacnothToDungeon) {
        // Sacnoth should follow the legacy AggroMoveToEX contract closely:
        // engage only inside the actual waypoint range and avoid the shared
        // Froggy-style extra clear sweep that pulls side groups off-route.
        aggroOptions.clear_options.extra_clear_range = 0.0f;
        aggroOptions.clear_options.minimum_local_clear_range = 0.0f;
        aggroOptions.clear_options.chase_distance = 1300.0f;
        // Sacnoth has several narrow turns where the generic stuck monitor
        // aborts earlier than the original AutoIt AggroMoveToEX flow.
        // Legacy AggroMoveToEX allows up to four minutes before giving up on
        // a leg. The waypoint-7/8 approach in Sacnoth still needs that headroom.
        waypointTimeoutMs = 240000u;
        aggroOptions.stuck_recovery_threshold = 30;
        aggroOptions.stuck_abort_threshold = 240;
        aggroOptions.stuck_recovery_radius = 900.0f;
    }

    aggroOptions.timeout_ms = waypointTimeoutMs;
    aggroOptions.clear_options.timeout_ms = waypointTimeoutMs;
    aggroOptions.clear_options.target_timeout_ms = waypointTimeoutMs;

    const int startIndex = DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);
    for (int i = startIndex; i < route.waypoint_count; ++i) {
        const float fightRange = route.waypoints[i].fight_range > 0.0f
            ? route.waypoints[i].fight_range
            : aggroOptions.clear_options.minimum_engage_range;
        auto waypointOptions = aggroOptions;
        waypointOptions.arrival_threshold = 250.0f;

        const bool arrived = DungeonCombat::AdvanceWithAggro(
            route.waypoints[i].x,
            route.waypoints[i].y,
            fightRange,
            MakeRragarsCombatCallbacks(),
            waypointOptions);
        if (MapMgr::GetMapId() != route.map_id) {
            return true;
        }
        if (IsPlayerOrPartyDead() || !WaitForMapReady(route.map_id, 10000u)) {
            LogBot("Rragars: travel route %s aborted at waypoint %d (%s)",
                   route.name,
                   i,
                   route.waypoints[i].label);
            return false;
        }
        if (!arrived) {
            LogBot("Rragars: autoit-style continue after timeout on %s waypoint %d (%s) remaining=%.0f",
                   route.name,
                   i,
                   route.waypoints[i].label,
                   DungeonCombat::DistanceToPoint(route.waypoints[i].x, route.waypoints[i].y));
        }
    }

    return true;
}

int GetNearestWaypointIndexForRoute(const RouteDefinition& route) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return -1;
    }
    return DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);
}

bool ReplayCheckpointBacktrack(const RouteDefinition& route, int currentIndex, int backtrackStart) {
    DungeonCheckpoint::CheckpointBacktrackReplayOptions options;
    options.waypoints = route.waypoints;
    options.waypoint_count = route.waypoint_count;
    options.current_index = currentIndex;
    options.backtrack_start = backtrackStart;
    options.move_waypoint_with_context = &MoveCheckpointWaypointForRoute;
    options.move_context = &route;

    const auto replay = DungeonCheckpoint::ReplayCheckpointBacktrack(options);
    if (!replay.completed) {
        LogBot("Rragars: failed checkpoint backtrack at waypoint %d (%s) on %s",
               replay.failed_index,
               replay.failed_index >= 0 && replay.failed_index < route.waypoint_count
                   ? route.waypoints[replay.failed_index].label
                   : "",
               route.name);
        return false;
    }
    return true;
}

bool HasVeiledThreatQuest() {
    return QuestMgr::GetActiveQuestId() == GWA3::QuestIds::VEILED_THREAT
        || QuestMgr::GetQuestById(GWA3::QuestIds::VEILED_THREAT) != nullptr;
}

bool InteractNearestNpcAndSendDialogs(float npcX, float npcY, float searchRadius, const uint32_t* dialogs, int dialogCount) {
    const uint32_t npcId = DungeonInteractions::FindNearestNpc(npcX, npcY, searchRadius);
    if (npcId == 0u) {
        LogBot("Rragars: no NPC found near (%.0f, %.0f)", npcX, npcY);
        return false;
    }

    AgentMgr::InteractNPC(npcId);
    WaitMs(1000);
    return DungeonDialog::SendDialogSequence(dialogs, dialogCount, 500u, 1);
}

bool EnsureVeiledThreatQuest() {
    if (HasVeiledThreatQuest()) {
        return true;
    }

    static constexpr uint32_t kCompleteDialogs[] = {
        GWA3::DialogIds::GENERIC_ACCEPT,
        GWA3::DialogIds::RragarsMenagerie::COMPLETE_OLD_QUEST,
        GWA3::DialogIds::GENERIC_ACCEPT,
        GWA3::DialogIds::RragarsMenagerie::COMPLETE_OLD_QUEST,
    };
    static constexpr uint32_t kAcceptDialogs[] = {
        GWA3::DialogIds::GENERIC_ACCEPT,
        GWA3::DialogIds::RragarsMenagerie::PICK_VEILED_THREAT,
        GWA3::DialogIds::GENERIC_ACCEPT,
        GWA3::DialogIds::RragarsMenagerie::ACCEPT_VEILED_THREAT,
    };

    LogBot("Rragars: acquiring Veiled Threat in Doomlore Shrine");
    if (!DungeonNavigation::MoveToAndWait(-17583.0f, 17668.0f, 250.0f, 20000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived ||
        !DungeonNavigation::MoveToAndWait(kQuestNpcX, kQuestNpcY, 250.0f, 20000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived) {
        return false;
    }

    if (!InteractNearestNpcAndSendDialogs(kQuestNpcX, kQuestNpcY, 1500.0f, kCompleteDialogs, 4)) {
        return false;
    }

    MapMgr::Travel(GWA3::MapIds::LONGEYES_LEDGE);
    if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::LONGEYES_LEDGE, 60000u)) {
        return false;
    }
    MapMgr::Travel(GWA3::MapIds::DOOMLORE_SHRINE);
    if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::DOOMLORE_SHRINE, 60000u)) {
        return false;
    }

    if (!DungeonNavigation::MoveToAndWait(kQuestNpcX, kQuestNpcY, 250.0f, 20000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived) {
        return false;
    }
    if (!InteractNearestNpcAndSendDialogs(kQuestNpcX, kQuestNpcY, 1500.0f, kAcceptDialogs, 4)) {
        return false;
    }
    WaitMs(1000);
    if (!InteractNearestNpcAndSendDialogs(kQuestNpcX, kQuestNpcY, 1500.0f, kAcceptDialogs, 4)) {
        return false;
    }

    WaitMs(1000);
    if (!HasVeiledThreatQuest()) {
        LogBot("Rragars: Veiled Threat was not confirmed in quest log after dialog flow");
        return false;
    }
    return true;
}

bool LeaveDoomloreForDalada() {
    LogBot("Rragars: leaving Doomlore Shrine for Dalada Uplands");
    if (!WaitForMapReady(GWA3::MapIds::DOOMLORE_SHRINE, 15000u) ||
        !DungeonNavigation::MoveToAndWait(-15024.0f, 16571.0f, 250.0f, 15000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived ||
        !DungeonNavigation::MoveToAndWait(-15968.0f, 14434.0f, 250.0f, 15000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived) {
        return false;
    }

    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < 60000u) {
        AgentMgr::Move(-15366.0f, 13553.0f);
        if (MapMgr::GetMapId() == GWA3::MapIds::DALADA_UPLANDS ||
            DungeonNavigation::WaitForMapId(GWA3::MapIds::DALADA_UPLANDS, 250u)) {
            return true;
        }
        WaitMs(250u);
    }
    return false;
}

BotState HandleCharSelect(BotConfig&) {
    return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

bool ExecuteRewardChestFlow() {
    const RewardChestObjective reward = GetRewardChestObjective();
    const DWORD chestSearchStart = GetTickCount();
    while ((GetTickCount() - chestSearchStart) < 60000u) {
        if (DungeonInteractions::FindNearestSignpost(reward.chest_x, reward.chest_y, 1500.0f) != 0u) {
            break;
        }
        if (!MoveToPoint(reward.staging_x, reward.staging_y, GWA3::MapIds::RRAGARS_MENAGERIE_LVL3, 300.0f, 10000u)) {
            LogBot("Rragars: failed moving to reward staging point");
            return false;
        }
        WaitMs(500u);
    }

    const uint32_t chestId = DungeonInteractions::FindNearestSignpost(reward.chest_x, reward.chest_y, 1500.0f);
    if (chestId == 0u) {
        LogBot("Rragars: reward chest signpost never appeared near (%.0f, %.0f)",
               reward.chest_x, reward.chest_y);
        return false;
    }

    if (!MoveToPoint(reward.chest_x, reward.chest_y, GWA3::MapIds::RRAGARS_MENAGERIE_LVL3, 250.0f, 15000u)) {
        LogBot("Rragars: failed reaching reward chest");
        return false;
    }

    for (int i = 0; i < reward.interact_repeats; ++i) {
        AgentMgr::InteractSignpost(chestId);
        WaitMs(5000u);
        DungeonBundle::PickUpNearestItemNearPoint(
            reward.chest_x,
            reward.chest_y,
            5000.0f,
            reward.pickup_attempts,
            500u);
    }

    if (MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3) {
        MapMgr::Travel(GWA3::MapIds::DOOMLORE_SHRINE);
        if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::DOOMLORE_SHRINE, 180000u) ||
            !WaitForMapReady(GWA3::MapIds::DOOMLORE_SHRINE, 15000u)) {
            LogBot("Rragars: failed returning to Doomlore Shrine after reward chest");
            return false;
        }
    }

    return MapMgr::GetMapId() == GWA3::MapIds::DOOMLORE_SHRINE;
}

bool ExecuteSimpleRoute(RouteId routeId, bool waitForTransition = true) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        LogBot("Rragars: no player agent available for route %s", route.name);
        return false;
    }

    const int startIndex = DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);
    LogBot("Rragars: route %s start index %d player=(%.0f, %.0f)",
           route.name,
           startIndex,
           me->x,
           me->y);

    if (const BlessingAnchor* blessing = FindBlessingAnchor(routeId, startIndex)) {
        LogBot("Rragars: blessing anchor at route %s index %d -> (%.0f, %.0f)",
               route.name, startIndex, blessing->x, blessing->y);
        DungeonNavigation::MoveToAndWait(blessing->x, blessing->y, 300.0f, 15000u, 1000u, route.map_id);
    }

    if (IsExplorableTravelRoute(routeId)) {
        if (!FollowTravelRouteWithRetries(routeId)) {
            return false;
        }
        if (!waitForTransition) {
            return true;
        }
        const ZoneTransitionPoint* transition = FindZoneTransitionPoint(routeId);
        const auto* fallbackPoint = route.waypoint_count > 0 ? &route.waypoints[route.waypoint_count - 1] : nullptr;
        const float zoneX = transition ? transition->x : (fallbackPoint ? fallbackPoint->x : 0.0f);
        const float zoneY = transition ? transition->y : (fallbackPoint ? fallbackPoint->y : 0.0f);
        if ((!transition && !fallbackPoint) ||
            !ZoneThroughPoint(zoneX, zoneY, route.next_map_id)) {
            return false;
        }
        if (!WaitForMapReady(route.next_map_id, 10000u)) {
            return false;
        }

        if (transition &&
            !WaitForSpawnAwayFromPoint(route.next_map_id, transition->x, transition->y, 3000.0f, 10000u)) {
            LogBot("Rragars: spawn on map %u never moved away from transition point (%.0f, %.0f)",
                   route.next_map_id,
                   transition->x,
                   transition->y);
            return false;
        }
        return true;
    }

    for (int i = startIndex; i < route.waypoint_count; ++i) {
        const WaypointExecutionPlan plan = BuildWaypointExecutionPlan(routeId, i);
        if (!plan.waypoint) {
            return false;
        }

        switch (plan.behavior) {
        case WaypointBehavior::StandardMove:
            if (!MoveToWaypoint(*plan.waypoint, route.map_id)) {
                LogBot("Rragars: failed moving to waypoint %d (%s) on %s",
                       i, plan.waypoint->label, route.name);
                return false;
            }
            break;
        case WaypointBehavior::PickUpKeg: {
            Log::Info("RragarsDbg: keg step begin route=%s waypoint=%d label=%s heldBundle=%u",
                      route.name,
                      i,
                      plan.waypoint->label,
                      DungeonInteractions::GetHeldBundleItemId());
            if (!MoveToWaypoint(*plan.waypoint, route.map_id, 250.0f)) {
                return false;
            }
            Log::Info("RragarsDbg: keg step arrived route=%s waypoint=%d heldBundle=%u",
                      route.name,
                      i,
                      DungeonInteractions::GetHeldBundleItemId());
            LogPlayerBundleDiagnostics("keg-before");
            bool acquired = DungeonBundle::AutoItGoToSignpostAndAcquireHeldBundleNearPoint(
                plan.waypoint->x,
                plan.waypoint->y,
                1500.0f,
                2,
                100u,
                1000u);
            Log::Info("RragarsDbg: keg legacy interact result route=%s waypoint=%d acquired=%d heldBundle=%u",
                      route.name,
                      i,
                      acquired ? 1 : 0,
                      DungeonInteractions::GetHeldBundleItemId());
            LogPlayerBundleDiagnostics("keg-after-legacy");
            if (!acquired) {
                acquired = DungeonBundle::InteractSignpostAndAcquireHeldBundleNearPoint(
                    plan.waypoint->x,
                    plan.waypoint->y,
                    1500.0f,
                    2,
                    100u,
                    2000u);
            }
            Log::Info("RragarsDbg: keg interact result route=%s waypoint=%d acquired=%d heldBundle=%u",
                      route.name,
                      i,
                      acquired ? 1 : 0,
                      DungeonInteractions::GetHeldBundleItemId());
            LogPlayerBundleDiagnostics("keg-after-interact");
            if (!acquired) {
                const bool chestPicked = DungeonBundle::OpenChestAndPickUpBundle(
                    plan.waypoint->x,
                    plan.waypoint->y,
                    1500.0f,
                    1500.0f,
                    2,
                    2,
                    500u,
                    500u);
                acquired = DungeonInteractions::GetHeldBundleItemId() != 0u;
                Log::Info("RragarsDbg: keg chest-open fallback route=%s waypoint=%d picked=%d acquired=%d heldBundle=%u",
                          route.name,
                          i,
                          chestPicked ? 1 : 0,
                          acquired ? 1 : 0,
                          DungeonInteractions::GetHeldBundleItemId());
                LogPlayerBundleDiagnostics("keg-after-chest-fallback");
            }
            if (!acquired) {
                const bool pickedGround = DungeonBundle::PickUpNearestItemNearPoint(
                    plan.waypoint->x,
                    plan.waypoint->y,
                    1500.0f,
                    2,
                    500u);
                acquired = DungeonInteractions::GetHeldBundleItemId() != 0u;
                Log::Info("RragarsDbg: keg ground-pickup fallback route=%s waypoint=%d picked=%d acquired=%d heldBundle=%u",
                          route.name,
                          i,
                          pickedGround ? 1 : 0,
                          acquired ? 1 : 0,
                          DungeonInteractions::GetHeldBundleItemId());
                LogPlayerBundleDiagnostics("keg-after-ground-fallback");
            }
            if (!acquired) {
                LogBot("Rragars: keg pickup near waypoint %d on %s did not result in a held bundle",
                       i,
                       route.name);
            }
            WaitMs(500);
            break;
        }
        case WaypointBehavior::DropKegAtBlastDoor: {
            Log::Info("RragarsDbg: blast-door step begin route=%s waypoint=%d label=%s heldBundle=%u",
                      route.name,
                      i,
                      plan.waypoint->label,
                      DungeonInteractions::GetHeldBundleItemId());
            LogPlayerBundleDiagnostics("blast-door-before");
            if (!MoveToWaypoint(*plan.waypoint, route.map_id, 250.0f)) {
                return false;
            }
            Log::Info("RragarsDbg: blast-door arrived route=%s waypoint=%d heldBundle=%u",
                      route.name,
                      i,
                      DungeonInteractions::GetHeldBundleItemId());
            const bool droppedOnce = DungeonInteractions::DropHeldBundle(false, false);
            Log::Info("RragarsDbg: blast-door first-drop route=%s waypoint=%d dropped=%d heldBundle=%u",
                      route.name,
                      i,
                      droppedOnce ? 1 : 0,
                      DungeonInteractions::GetHeldBundleItemId());
            if (!droppedOnce) {
                LogBot("Rragars: blast door waypoint %d on %s reached without a held bundle; deferring to checkpoint retry",
                       i,
                       route.name);
            }
            WaitMs(500);
            if (droppedOnce || DungeonInteractions::GetHeldBundleItemId() != 0u) {
                const bool droppedTwice = DungeonInteractions::DropHeldBundle(true, false);
                Log::Info("RragarsDbg: blast-door second-drop route=%s waypoint=%d dropped=%d heldBundle=%u",
                          route.name,
                          i,
                          droppedTwice ? 1 : 0,
                          DungeonInteractions::GetHeldBundleItemId());
            } else {
                Log::Info("RragarsDbg: blast-door second-drop skipped route=%s waypoint=%d heldBundle=%u",
                          route.name,
                          i,
                          DungeonInteractions::GetHeldBundleItemId());
            }
            WaitMs(4000);
            break;
        }
        case WaypointBehavior::PickUpDungeonKey: {
            if (!MoveToWaypoint(*plan.waypoint, route.map_id)) {
                return false;
            }
            const LootObjective* loot = FindLootObjective(routeId);
            const float pickupX = loot ? loot->pickup_x : plan.waypoint->x;
            const float pickupY = loot ? loot->pickup_y : plan.waypoint->y;
            const int pickupRetries = loot ? loot->pickup_retries : 1;
            if (!DungeonBundle::PickUpNearestItemNearPoint(
                    pickupX,
                    pickupY,
                    1200.0f,
                    pickupRetries,
                    500u)) {
                LogBot("Rragars: no nearby dungeon key item found near waypoint %d on %s",
                       i, route.name);
            }
            break;
        }
        case WaypointBehavior::ValidateQuestCheckpoint:
        case WaypointBehavior::ValidateRetryCheckpoint: {
            Log::Info("RragarsDbg: checkpoint step begin route=%s waypoint=%d label=%s",
                      route.name,
                      i,
                      plan.waypoint->label);
            if (!MoveToWaypoint(*plan.waypoint, route.map_id)) {
                return false;
            }

            const int nearestWaypoint = GetNearestWaypointIndexForRoute(route);
            Log::Info("RragarsDbg: checkpoint nearest route=%s waypoint=%d label=%s nearest=%d",
                      route.name,
                      i,
                      plan.waypoint->label,
                      nearestWaypoint);
            if (nearestWaypoint < 0) {
                LogBot("Rragars: failed to resolve nearest waypoint after checkpoint %d on %s", i, route.name);
                return false;
            }

            const auto resolution = DungeonCheckpoint::EvaluateCheckpointResolution(
                i,
                nearestWaypoint,
                route.waypoint_count,
                plan.checkpoint_policy);
            Log::Info("RragarsDbg: checkpoint resolution route=%s waypoint=%d label=%s passed=%d action=%d backtrackStart=%d retryIndex=%d",
                      route.name,
                      i,
                      plan.waypoint->label,
                      resolution.passed ? 1 : 0,
                      static_cast<int>(resolution.action),
                      resolution.backtrack_start,
                      resolution.retry_index);
            if (resolution.passed) {
                break;
            }

            switch (resolution.action) {
            case DungeonCheckpoint::CheckpointFailureAction::AbortRun:
                LogBot("Rragars: checkpoint %s failed at index %d, returning to Doomlore Shrine",
                       plan.waypoint->label, i);
                MapMgr::Travel(GWA3::MapIds::DOOMLORE_SHRINE);
                return DungeonNavigation::WaitForMapId(GWA3::MapIds::DOOMLORE_SHRINE, 60000u);
            case DungeonCheckpoint::CheckpointFailureAction::BacktrackRetry:
                LogBot("Rragars: checkpoint %s failed at index %d, backtracking to %d and retrying from loop index %d",
                       plan.waypoint->label, i, resolution.backtrack_start, resolution.retry_index);
                if (!ReplayCheckpointBacktrack(route, i, resolution.backtrack_start)) {
                    return false;
                }
                // Match the AutoIt loop semantics: the enclosing for-loop increments after this case.
                i = resolution.retry_index;
                break;
            default:
                LogBot("Rragars: checkpoint %s failed at index %d without a retry policy",
                       plan.waypoint->label, i);
                return false;
            }
            break;
        }
        case WaypointBehavior::DoubleInteract: {
            if (!MoveToWaypoint(*plan.waypoint, route.map_id, 250.0f)) {
                return false;
            }
            const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(
                plan.waypoint->x,
                plan.waypoint->y,
                1500.0f);
            if (signpostId == 0u) {
                LogBot("Rragars: no signpost found near waypoint %d label=%s route=%s",
                       i, plan.waypoint->label, route.name);
                return false;
            }
            AgentMgr::InteractSignpost(signpostId);
            WaitMs(1000);
            AgentMgr::InteractSignpost(signpostId);
            WaitMs(1000);
            break;
        }
        default:
            return false;
        }
    }

    if (!waitForTransition) {
        return true;
    }
    return DungeonNavigation::WaitForMapId(route.next_map_id, 60000u);
}

BotState HandleTownSetup(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::DOOMLORE_SHRINE || mapId == GWA3::MapIds::DALADA_UPLANDS || mapId == GWA3::MapIds::GROTHMAR_WARDOWNS || mapId == GWA3::MapIds::SACNOTH_VALLEY) {
        return BotState::Traveling;
    }

    LogBot("Rragars: traveling to Doomlore Shrine from map %u", mapId);
    MapMgr::Travel(GWA3::MapIds::DOOMLORE_SHRINE);
    if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::DOOMLORE_SHRINE, 60000u) ||
        !WaitForMapReady(GWA3::MapIds::DOOMLORE_SHRINE, 15000u)) {
        return BotState::Error;
    }
    return BotState::Traveling;
}

BotState HandleTravel(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::DOOMLORE_SHRINE) {
        if (!WaitForMapReady(GWA3::MapIds::DOOMLORE_SHRINE, 15000u)) {
            return BotState::Error;
        }
        if (!EnsureVeiledThreatQuest() || !LeaveDoomloreForDalada()) {
            return BotState::Error;
        }
        return BotState::Traveling;
    }

    const RouteDefinition* route = FindRouteDefinitionByMapId(mapId);
    if (!route) {
        if (mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL2 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3) {
            return BotState::InDungeon;
        }
        LogBot("Rragars: unsupported travel map %u", mapId);
        return BotState::Error;
    }

    if (!WaitForMapReady(mapId, 15000u)) {
        return BotState::Error;
    }

    const RouteId routeId = static_cast<RouteId>(route - &GetRouteDefinition(RouteId::RunDaladaToGrothmar));
    LogBot("Rragars: executing travel route %s", route->name);
    if (!ExecuteSimpleRoute(routeId)) {
        return BotState::Error;
    }
    return MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1 ? BotState::InDungeon : BotState::Traveling;
}

BotState HandleDungeon(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    RouteId routeId = RouteId::Level1;
    switch (mapId) {
    case GWA3::MapIds::RRAGARS_MENAGERIE_LVL1:
        routeId = RouteId::Level1;
        break;
    case GWA3::MapIds::RRAGARS_MENAGERIE_LVL2:
        routeId = RouteId::Level2;
        break;
    case GWA3::MapIds::RRAGARS_MENAGERIE_LVL3:
        routeId = RouteId::Level3;
        break;
    default:
        return BotState::InTown;
    }

    if (!WaitForMapReady(mapId, 15000u)) {
        return BotState::Error;
    }

    LogBot("Rragars: executing dungeon route on map %u", mapId);
    if (routeId == RouteId::Level3) {
        if (!ExecuteSimpleRoute(routeId, false) || !ExecuteRewardChestFlow()) {
            return BotState::Error;
        }
        return BotState::InTown;
    }

    if (!ExecuteSimpleRoute(routeId, true)) {
        return BotState::Error;
    }
    return MapMgr::GetMapId() == GWA3::MapIds::DOOMLORE_SHRINE ? BotState::InTown : BotState::InDungeon;
}

BotState HandleError(BotConfig&) {
    LogBot("Rragars: ERROR state - waiting before retry");
    WaitMs(5000);
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
    cfg.target_map_id = GWA3::MapIds::RRAGARS_MENAGERIE_LVL1;
    cfg.outpost_map_id = GWA3::MapIds::DOOMLORE_SHRINE;
    cfg.bot_module_name = "RragarsMenagerie";

    LogBot("Rragars Menagerie module registered (in-progress runtime)");
}

} // namespace GWA3::Bot::RragarsMenagerieBot
