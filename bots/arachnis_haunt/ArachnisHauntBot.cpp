#include <bots/arachnis_haunt/ArachnisHauntBot.h>

#include <bots/arachnis_haunt/ArachnisHaunt.h>
#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonEffects.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>

#include <cstdarg>
#include <cstdio>

namespace GWA3::Bot::ArachnisHauntBot {

using namespace GWA3::Bot;
using namespace GWA3::Bot::ArachnisHaunt;

namespace {

constexpr uint32_t kAsuraFlameStaffModelId = 24350u;
constexpr uint32_t kAsuraFlameStaffEffectSkillId = 2429u;

void LogArachnisMain(const char* fmt, ...) {
    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    Log::Info("ArachnisDbg: %s", message);
}

bool HasHeldBundle();
bool EnsureHeldBundleNearPlayer();
bool WaitForHeldBundleState(bool expectedHeld, uint32_t timeoutMs, const char* context);
bool WaitForMovementToSettle(uint32_t timeoutMs, const char* context);
bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs, const char* context = nullptr);
bool FinalizeZoneTransition(
    uint32_t targetMapId,
    const char* context,
    uint32_t settleMs = 3000u,
    uint32_t readyTimeoutMs = 30000u);
bool HandleArachnisAggroWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    int waypointIndex,
    DungeonRoute::WaypointLabelKind labelKind,
    DungeonCombat::AggroWaypointPhase phase,
    void* userData);
bool TryDropFlameStaffBundle(const char* context);
void LogHeldBundleState(
    const char* context,
    const DungeonRoute::Waypoint* waypoint = nullptr,
    int waypointIndex = -1);
void LogBundleCarryTarget(
    const char* context,
    int legIndex,
    int legCount,
    const DungeonQuest::TravelPoint& point);
void SuspendTransitionSensitiveHooks(const char* context);
void ResumeTransitionSensitiveHooks(const char* context);
DungeonQuestRuntime::DialogExecutionOptions BuildQuestDialogOptions();

struct ZoneTransitionAttemptResult {
    bool zoned = false;
    bool ready = false;
};

struct AggroWaypointTraceContext {
    const char* route_name = nullptr;
    const char* context = nullptr;
};

ZoneTransitionAttemptResult ZoneThroughPointWithTransitionHooks(
    float x,
    float y,
    uint32_t targetMapId,
    const char* context,
    uint32_t timeoutMs = 60000u,
    uint32_t settleMs = 3000u,
    uint32_t readyTimeoutMs = 30000u);

void WaitMs(DWORD ms) {
    Sleep(ms);
}

const char* GetMapReadyStateLabel(uint32_t mapId, float& hp, float& x, float& y) {
    hp = 0.0f;
    x = 0.0f;
    y = 0.0f;

    if (MapMgr::GetMapId() != mapId) {
        return "wrong-map";
    }
    if (MapMgr::GetLoadingState() != 1u) {
        return "loading";
    }
    if (AgentMgr::GetMyId() == 0u) {
        return "missing-my-id";
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return "missing-agent";
    }

    hp = me->hp;
    x = me->x;
    y = me->y;
    if (me->hp <= 0.0f) {
        return "dead-agent";
    }
    if (me->x == 0.0f && me->y == 0.0f) {
        return "zero-position";
    }

    return "ready";
}

bool MoveToWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    uint32_t mapId,
    float tolerance = 250.0f,
    uint32_t timeoutMs = 30000u) {
    return DungeonNavigation::MoveToAndWait(
        waypoint.x,
        waypoint.y,
        tolerance,
        timeoutMs,
        1000u,
        mapId).arrived;
}

bool MoveToTravelPoint(
    const DungeonQuest::TravelPoint& point,
    uint32_t mapId,
    float tolerance = 250.0f,
    uint32_t timeoutMs = 30000u) {
    return DungeonNavigation::MoveToAndWait(
        point.x,
        point.y,
        tolerance,
        timeoutMs,
        1000u,
        mapId).arrived;
}

void MoveToPointForDoor(float x, float y, float threshold) {
    (void)DungeonNavigation::MoveToAndWait(
        x,
        y,
        threshold,
        30000u,
        1000u,
        MapMgr::GetMapId());
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

void SuspendTransitionSensitiveHooks(const char* context) {
    LogArachnisMain("transition hooks suspend context=%s map=%u loading=%u myId=%u",
                    context != nullptr ? context : "",
                    MapMgr::GetMapId(),
                    MapMgr::GetLoadingState(),
                    AgentMgr::GetMyId());
    AgentMgr::ResetMoveState("Arachnis transition suspend");
    CtoS::SuspendEngineHook();
    DialogMgr::Shutdown();
}

void ResumeTransitionSensitiveHooks(const char* context) {
    AgentMgr::ResetMoveState("Arachnis transition resume");
    CtoS::ResumeEngineHook();
    const bool dialogReady = DialogMgr::Initialize();
    LogArachnisMain("transition hooks resume context=%s map=%u loading=%u myId=%u dialogReady=%d",
                    context != nullptr ? context : "",
                    MapMgr::GetMapId(),
                    MapMgr::GetLoadingState(),
                    AgentMgr::GetMyId(),
                    dialogReady ? 1 : 0);
}

ZoneTransitionAttemptResult ZoneThroughPointWithTransitionHooks(
    float x,
    float y,
    uint32_t targetMapId,
    const char* context,
    uint32_t timeoutMs,
    uint32_t settleMs,
    uint32_t readyTimeoutMs) {
    ZoneTransitionAttemptResult result{};
    SuspendTransitionSensitiveHooks(context);
    result.zoned = ZoneThroughPoint(x, y, targetMapId, timeoutMs);
    if (result.zoned) {
        result.ready = FinalizeZoneTransition(targetMapId, context, settleMs, readyTimeoutMs);
    }
    ResumeTransitionSensitiveHooks(context);
    return result;
}

DungeonQuestRuntime::DialogExecutionOptions BuildQuestDialogOptions() {
    DungeonQuestRuntime::DialogExecutionOptions options;
    options.move_to_actual_npc = true;
    options.move_to_npc_tolerance = 120.0f;
    options.move_to_npc_timeout_ms = 20000u;
    options.cancel_action_before_interact = true;
    options.clear_dialog_state_before_interact = true;
    options.require_dialog_before_send = true;
    options.pre_interact_settle_ms = 500u;
    options.change_target_delay_ms = 250u;
    options.interact_count = 3;
    options.interact_delay_ms = 1500u;
    options.post_interact_delay_ms = 1000u;
    options.dialog_wait_timeout_ms = 2500u;
    options.repeat_delay_ms = 750u;
    options.max_retries_per_dialog = 2;
    options.use_direct_npc_interact = true;
    return options;
}

bool ZoneAtTravelPointWithRetries(
    const DungeonQuest::TravelPoint& point,
    uint32_t currentMapId,
    uint32_t targetMapId,
    const char* label,
    int attempts = 3) {
    for (int attempt = 0; attempt < attempts; ++attempt) {
        LogArachnisMain("zone attempt start label=%s attempt=%d/%d currentMap=%u targetMap=%u point=(%.0f, %.0f)",
                        label != nullptr ? label : "",
                        attempt + 1,
                        attempts,
                        currentMapId,
                        targetMapId,
                        point.x,
                        point.y);
        if (!MoveToTravelPoint(point, currentMapId, 300.0f, 15000u)) {
            LogBot("Arachnis: failed reaching %s point on attempt %d/%d",
                   label,
                   attempt + 1,
                   attempts);
            LogArachnisMain("zone attempt reach failed label=%s attempt=%d/%d currentMap=%u targetMap=%u",
                            label != nullptr ? label : "",
                            attempt + 1,
                            attempts,
                            MapMgr::GetMapId(),
                            targetMapId);
            WaitMs(500u);
            continue;
        }

        AgentMgr::CancelAction();
        WaitMs(100u);
        AgentMgr::ChangeTarget(0u);
        WaitMs(150u);

        const ZoneTransitionAttemptResult transition =
            ZoneThroughPointWithTransitionHooks(point.x, point.y, targetMapId, label, 15000u);
        LogArachnisMain("zone attempt observed label=%s attempt=%d/%d zoned=%d currentMap=%u loading=%u myId=%u",
                        label != nullptr ? label : "",
                        attempt + 1,
                        attempts,
                        transition.zoned ? 1 : 0,
                        MapMgr::GetMapId(),
                        MapMgr::GetLoadingState(),
                        AgentMgr::GetMyId());
        if (transition.ready) {
            return true;
        }

        LogBot("Arachnis: %s zone attempt %d/%d failed",
               label,
               attempt + 1,
               attempts);
        LogArachnisMain("zone attempt failed label=%s attempt=%d/%d currentMap=%u loading=%u myId=%u",
                        label != nullptr ? label : "",
                        attempt + 1,
                        attempts,
                        MapMgr::GetMapId(),
                        MapMgr::GetLoadingState(),
                        AgentMgr::GetMyId());
        WaitMs(500u);
    }

    return false;
}

bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs, const char* context) {
    const DWORD start = GetTickCount();
    bool observedTargetMap = false;
    DWORD observedTargetElapsedMs = 0u;
    const char* lastState = "timeout";
    uint32_t lastMapId = MapMgr::GetMapId();
    uint32_t lastLoadingState = MapMgr::GetLoadingState();
    uint32_t lastMyId = AgentMgr::GetMyId();
    float lastHp = 0.0f;
    float lastX = 0.0f;
    float lastY = 0.0f;
    while ((GetTickCount() - start) < timeoutMs) {
        lastMapId = MapMgr::GetMapId();
        lastLoadingState = MapMgr::GetLoadingState();
        lastMyId = AgentMgr::GetMyId();
        if (!observedTargetMap && lastMapId == mapId) {
            observedTargetMap = true;
            observedTargetElapsedMs = GetTickCount() - start;
            LogArachnisMain("map ready wait observed context=%s targetMap=%u elapsedMs=%lu loading=%u myId=%u",
                            context != nullptr ? context : "",
                            mapId,
                            static_cast<unsigned long>(observedTargetElapsedMs),
                            lastLoadingState,
                            lastMyId);
        }

        lastState = GetMapReadyStateLabel(mapId, lastHp, lastX, lastY);
        if (lastState[0] == 'r' && lastState[1] == 'e') {
            if (observedTargetMap) {
                LogArachnisMain("map ready wait success context=%s targetMap=%u elapsedMs=%lu observedMs=%lu player=(%.0f, %.0f) hp=%.2f",
                                context != nullptr ? context : "",
                                mapId,
                                static_cast<unsigned long>(GetTickCount() - start),
                                static_cast<unsigned long>(observedTargetElapsedMs),
                                lastX,
                                lastY,
                                lastHp);
            }
            return true;
        }

        WaitMs(250u);
    }

    LogBot("Arachnis: map %u ready wait timed out after %lu ms context=%s state=%s currentMap=%u loading=%u myId=%u player=(%.0f, %.0f) hp=%.2f",
           mapId,
           static_cast<unsigned long>(timeoutMs),
           context != nullptr ? context : "",
           lastState,
           lastMapId,
           lastLoadingState,
           lastMyId,
           lastX,
           lastY,
           lastHp);
    LogArachnisMain("map ready wait timeout context=%s targetMap=%u elapsedMs=%lu state=%s currentMap=%u loading=%u myId=%u player=(%.0f, %.0f) hp=%.2f",
                    context != nullptr ? context : "",
                    mapId,
                    static_cast<unsigned long>(timeoutMs),
                    lastState,
                    lastMapId,
                    lastLoadingState,
                    lastMyId,
                    lastX,
                    lastY,
                    lastHp);
    return false;
}

bool FinalizeZoneTransition(
    uint32_t targetMapId,
    const char* context,
    uint32_t settleMs,
    uint32_t readyTimeoutMs) {
    LogArachnisMain("zone finalize start context=%s targetMap=%u currentMap=%u loading=%u myId=%u settleMs=%u readyTimeoutMs=%u",
                    context != nullptr ? context : "",
                    targetMapId,
                    MapMgr::GetMapId(),
                    MapMgr::GetLoadingState(),
                    AgentMgr::GetMyId(),
                    settleMs,
                    readyTimeoutMs);
    if (MapMgr::GetMapId() != targetMapId) {
        LogArachnisMain("zone finalize skipped context=%s targetMap=%u currentMap=%u loading=%u myId=%u",
                        context != nullptr ? context : "",
                        targetMapId,
                        MapMgr::GetMapId(),
                        MapMgr::GetLoadingState(),
                        AgentMgr::GetMyId());
        return false;
    }

    if (settleMs > 0u) {
        WaitMs(settleMs);
    }

    const bool ready = WaitForMapReady(targetMapId, readyTimeoutMs, context);
    LogArachnisMain("zone finalize end context=%s targetMap=%u ready=%d currentMap=%u loading=%u myId=%u",
                    context != nullptr ? context : "",
                    targetMapId,
                    ready ? 1 : 0,
                    MapMgr::GetMapId(),
                    MapMgr::GetLoadingState(),
                    AgentMgr::GetMyId());
    return ready;
}

bool WaitForPartyRecovery(uint32_t mapId, uint32_t minHeroes = 1u, uint32_t timeoutMs = 8000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
            WaitMs(250u);
            continue;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (me == nullptr || me->hp <= 0.0f) {
            WaitMs(250u);
            continue;
        }

        if (PartyMgr::CountPartyHeroes() >= minHeroes) {
            return true;
        }

        WaitMs(250u);
    }

    return false;
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

bool FollowTravelPath(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t mapId,
    float tolerance = 250.0f) {
    if (points == nullptr || count <= 0) {
        return false;
    }

    return DungeonQuestRuntime::FollowTravelPath(
        points,
        count,
        mapId,
        tolerance,
        30000u,
        1000u);
}

bool MoveToTravelPointWithAggro(
    const DungeonQuest::TravelPoint& point,
    uint32_t mapId,
    float tolerance = 250.0f,
    float fightRange = 1200.0f,
    uint32_t timeoutMs = 30000u) {
    return DungeonBuiltinCombat::MoveToPointWithAggro(
        point.x,
        point.y,
        mapId,
        tolerance,
        fightRange,
        timeoutMs);
}

bool FollowTravelPathWithAggro(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t mapId,
    float tolerance = 250.0f,
    float fightRange = 1200.0f,
    uint32_t timeoutMs = 30000u) {
    return DungeonBuiltinCombat::FollowTravelPathWithAggro(
        points,
        count,
        mapId,
        tolerance,
        fightRange,
        timeoutMs);
}

bool FollowRouteWithRetries(
    RouteId routeId,
    const char* context,
    const DungeonNavigation::RouteFollowOptions& options) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    const DWORD routeStart = GetTickCount();
    auto* me = AgentMgr::GetMyAgent();
    const int startIndex = GetNearestWaypointIndexForRoute(route);
    LogArachnisMain("route start route=%s context=%s map=%u startIndex=%d player=(%.0f, %.0f) aggro=%d",
                    route.name,
                    context != nullptr ? context : "",
                    route.map_id,
                    startIndex,
                    me ? me->x : 0.0f,
                    me ? me->y : 0.0f,
                    UsesAggroTraversal(routeId) ? 1 : 0);

    DungeonNavigation::RouteFollowResult followResult;
    if (UsesAggroTraversal(routeId)) {
        DungeonCombat::AggroAdvanceOptions aggroOptions;
        DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(
            aggroOptions,
            options.waypoint_timeout_ms,
            false);

        AggroWaypointTraceContext waypointTrace;
        waypointTrace.route_name = route.name;
        waypointTrace.context = context;
        DungeonCombat::AggroWaypointCallbacks waypointCallbacks;
        waypointCallbacks.on_waypoint = &HandleArachnisAggroWaypoint;
        waypointCallbacks.user_data = &waypointTrace;
        followResult = DungeonCombat::FollowWaypointsWithAggro(
            route.waypoints,
            route.waypoint_count,
            route.map_id,
            DungeonBuiltinCombat::MakeCombatCallbacks(),
            options,
            aggroOptions,
            waypointCallbacks);
    } else {
        followResult = DungeonNavigation::FollowWaypoints(
            route.waypoints,
            route.waypoint_count,
            route.map_id,
            options);
    }
    LogArachnisMain("route result route=%s context=%s completed=%d mapChanged=%d failedIndex=%d retries=%d durationMs=%lu",
                    route.name,
                    context != nullptr ? context : "",
                    followResult.completed ? 1 : 0,
                    followResult.map_changed ? 1 : 0,
                    followResult.failed_index,
                    followResult.retries_used,
                    static_cast<unsigned long>(GetTickCount() - routeStart));
    if (followResult.completed || followResult.map_changed) {
        return true;
    }

    if (followResult.failed_index >= 0 && followResult.failed_index < route.waypoint_count) {
        LogBot("Arachnis: failed %s at waypoint %d (%s) after %d retries",
               context,
               followResult.failed_index,
               route.waypoints[followResult.failed_index].label,
               followResult.retries_used);
    } else {
        LogBot("Arachnis: failed %s after %d retries",
               context,
               followResult.retries_used);
    }
    return false;
}

bool MaybeAcquireBlessing(RouteId routeId) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    const int startIndex = GetNearestWaypointIndexForRoute(route);
    if (startIndex < 0) {
        return false;
    }

    const BlessingAnchor* blessing = FindBlessingAnchor(routeId, startIndex);
    if (blessing == nullptr) {
        return true;
    }

    if (!MoveToTravelPoint({blessing->x, blessing->y}, route.map_id, 300.0f, 15000u)) {
        LogBot("Arachnis: failed reaching blessing anchor on %s", route.name);
        return false;
    }

    GWA3::DungeonEffects::BlessingAcquireOptions options;
    options.required_title_id = blessing->required_title_id;
    options.accept_dialog_id = blessing->accept_dialog_id;
    options.dialog_retries = 3;
    options.dialog_delay_ms = 2500u;
    const auto result = GWA3::DungeonEffects::TryAcquireBlessingAt(blessing->x, blessing->y, options);
    if (result.confirmed) {
        return true;
    }

    if (result.dialog_sent) {
        WaitMs(1500u);
        if (GWA3::DungeonEffects::HasBlessing()) {
            return true;
        }
        LogBot("Arachnis: blessing not yet confirmed on %s, continuing after dialog send", route.name);
        return true;
    }

    if (!result.confirmed) {
        LogBot("Arachnis: blessing acquisition failed on %s (npc=%u dialogSent=%d)",
               route.name,
               result.npc_id,
               result.dialog_sent ? 1 : 0);
        return false;
    }

    return true;
}

bool ExecuteRoute(RouteId routeId, const char* context, bool waitForTransition = false) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    LogArachnisMain("execute route route=%s context=%s waitForTransition=%d",
                    route.name,
                    context != nullptr ? context : "",
                    waitForTransition ? 1 : 0);
    if (!MaybeAcquireBlessing(routeId)) {
        LogArachnisMain("execute route blessing failed route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        return false;
    }

    DungeonNavigation::RouteFollowOptions options;
    options.use_waypoint_fight_range_as_tolerance = true;
    options.waypoint_timeout_ms = 60000u;
    options.max_backtrack_retries = 3;
    if (!FollowRouteWithRetries(routeId, context, options)) {
        return false;
    }

    if (!waitForTransition) {
        LogArachnisMain("execute route finished route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        return true;
    }

    if (route.next_map_id == 0u || route.waypoint_count <= 0) {
        LogArachnisMain("execute route no transition route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        return true;
    }

    const auto& zonePoint = route.waypoints[route.waypoint_count - 1];
    LogArachnisMain("execute route transition route=%s context=%s zoneTarget=(%.0f, %.0f) nextMap=%u",
                    route.name,
                    context != nullptr ? context : "",
                    zonePoint.x,
                    zonePoint.y,
                    route.next_map_id);
    const ZoneTransitionAttemptResult transition = ZoneThroughPointWithTransitionHooks(
        zonePoint.x,
        zonePoint.y,
        route.next_map_id,
        context);
    if (!transition.zoned) {
        LogBot("Arachnis: failed zoning after %s", route.name);
        LogArachnisMain("execute route transition failed route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        return false;
    }
    LogArachnisMain("execute route transition ready route=%s context=%s nextMap=%u ready=%d",
                    route.name,
                    context != nullptr ? context : "",
                    route.next_map_id,
                    transition.ready ? 1 : 0);
    return transition.ready;
}

bool HasHeldBundle() {
    if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
        return true;
    }

    const uint32_t myId = AgentMgr::GetMyId();
    return myId != 0u && EffectMgr::HasEffect(myId, kAsuraFlameStaffEffectSkillId);
}

bool WaitForHeldBundleState(bool expectedHeld, uint32_t timeoutMs, const char* context) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (HasHeldBundle() == expectedHeld) {
            LogHeldBundleState(context);
            return true;
        }
        WaitMs(100u);
    }

    LogHeldBundleState(context);
    return false;
}

bool WaitForMovementToSettle(uint32_t timeoutMs, const char* context) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = AgentMgr::GetMyAgent();
        if (me != nullptr && me->move_x == 0.0f && me->move_y == 0.0f) {
            LogHeldBundleState(context);
            return true;
        }
        WaitMs(100u);
    }

    LogHeldBundleState(context);
    return false;
}

bool TryDropFlameStaffBundle(const char* context) {
    const uint32_t myId = AgentMgr::GetMyId();
    const auto* effect = myId != 0u
                             ? EffectMgr::GetEffectBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                             : nullptr;
    const auto* buff = myId != 0u
                           ? EffectMgr::GetBuffBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                           : nullptr;

    LogArachnisMain(
        "flame-staff native drop probe context=%s heldItem=%u effectSkill=%u effectId=%u buffSkill=%u buffId=%u target=%u",
        context != nullptr ? context : "",
        DungeonInteractions::GetHeldBundleItemId(),
        effect != nullptr ? effect->skill_id : 0u,
        effect != nullptr ? effect->effect_id : 0u,
        buff != nullptr ? buff->skill_id : 0u,
        buff != nullptr ? buff->buff_id : 0u,
        buff != nullptr ? buff->target_agent_id : 0u);
    if (buff == nullptr || buff->buff_id == 0u) {
        return false;
    }

    const bool dropped = EffectMgr::DropBuff(buff->buff_id);
    LogBot("Arachnis: native flame staff drop context=%s buffId=%u result=%d",
           context != nullptr ? context : "",
           buff->buff_id,
           dropped ? 1 : 0);
    LogArachnisMain("flame-staff native drop context=%s buffId=%u result=%d",
                    context != nullptr ? context : "",
                    buff->buff_id,
                    dropped ? 1 : 0);
    return dropped;
}

void LogHeldBundleState(
    const char* context,
    const DungeonRoute::Waypoint* waypoint,
    int waypointIndex) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t heldItemId = DungeonInteractions::GetHeldBundleItemId();
    const uint32_t myId = AgentMgr::GetMyId();
    const auto* effect = myId != 0u
                             ? EffectMgr::GetEffectBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                             : nullptr;
    const auto* buff = myId != 0u
                           ? EffectMgr::GetBuffBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                           : nullptr;
    const bool hasEffect = effect != nullptr;

    if (waypoint != nullptr) {
        LogBot("Arachnis: %s waypoint %d (%s) player=(%.0f, %.0f) heldItem=%u effect=%d effectId=%u buffId=%u",
               context,
               waypointIndex,
               waypoint->label != nullptr ? waypoint->label : "",
               me ? me->x : 0.0f,
               me ? me->y : 0.0f,
               heldItemId,
               hasEffect ? 1 : 0,
               effect != nullptr ? effect->effect_id : 0u,
               buff != nullptr ? buff->buff_id : 0u);
        LogArachnisMain("%s waypoint=%d label=%s player=(%.0f, %.0f) heldItem=%u effect=%d effectId=%u buffId=%u",
                        context,
                        waypointIndex,
                        waypoint->label != nullptr ? waypoint->label : "",
                        me ? me->x : 0.0f,
                        me ? me->y : 0.0f,
                        heldItemId,
                        hasEffect ? 1 : 0,
                        effect != nullptr ? effect->effect_id : 0u,
                        buff != nullptr ? buff->buff_id : 0u);
        return;
    }

    LogBot("Arachnis: %s player=(%.0f, %.0f) heldItem=%u effect=%d effectId=%u buffId=%u",
           context,
           me ? me->x : 0.0f,
           me ? me->y : 0.0f,
           heldItemId,
           hasEffect ? 1 : 0,
           effect != nullptr ? effect->effect_id : 0u,
           buff != nullptr ? buff->buff_id : 0u);
    LogArachnisMain("%s player=(%.0f, %.0f) heldItem=%u effect=%d effectId=%u buffId=%u",
                    context,
                    me ? me->x : 0.0f,
                    me ? me->y : 0.0f,
                    heldItemId,
                    hasEffect ? 1 : 0,
                    effect != nullptr ? effect->effect_id : 0u,
                    buff != nullptr ? buff->buff_id : 0u);
}

void LogBundleCarryTarget(
    const char* context,
    int legIndex,
    int legCount,
    const DungeonQuest::TravelPoint& point) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t heldItemId = DungeonInteractions::GetHeldBundleItemId();
    const uint32_t myId = AgentMgr::GetMyId();
    const auto* effect = myId != 0u
                             ? EffectMgr::GetEffectBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                             : nullptr;
    const auto* buff = myId != 0u
                           ? EffectMgr::GetBuffBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                           : nullptr;
    const bool hasEffect = effect != nullptr;

    LogBot("Arachnis: %s leg %d/%d target=(%.0f, %.0f) player=(%.0f, %.0f) heldItem=%u effect=%d effectId=%u buffId=%u",
           context,
           legIndex,
           legCount,
           point.x,
           point.y,
           me ? me->x : 0.0f,
           me ? me->y : 0.0f,
           heldItemId,
           hasEffect ? 1 : 0,
           effect != nullptr ? effect->effect_id : 0u,
           buff != nullptr ? buff->buff_id : 0u);
}

bool EnsureHeldBundleNearPlayer() {
    if (HasHeldBundle()) {
        LogHeldBundleState("ensure-held-bundle already holding");
        return true;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return false;
    }

    LogHeldBundleState("ensure-held-bundle begin");
    const bool picked = DungeonBundle::PickUpNearestItemByModelNearPoint(
        me->x,
        me->y,
        kAsuraFlameStaffModelId,
        18000.0f,
        3,
        500u);
    if (!picked) {
        LogHeldBundleState("ensure-held-bundle no pickup");
        LogBot("Arachnis: expected flame staff bundle near player at (%.0f, %.0f)", me->x, me->y);
        return false;
    }
    WaitMs(500u);
    LogHeldBundleState("ensure-held-bundle end");
    if (!HasHeldBundle()) {
        LogBot("Arachnis: interacted with flame staff near player but no bundle is held");
        return false;
    }
    return true;
}

bool HandleArachnisAggroWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    int waypointIndex,
    DungeonRoute::WaypointLabelKind labelKind,
    DungeonCombat::AggroWaypointPhase phase,
    void* userData) {
    const auto* trace = static_cast<const AggroWaypointTraceContext*>(userData);
    auto* me = AgentMgr::GetMyAgent();
    if (phase == DungeonCombat::AggroWaypointPhase::BeforeAdvance ||
        phase == DungeonCombat::AggroWaypointPhase::AfterAdvance) {
        LogArachnisMain(
            "aggro waypoint route=%s context=%s index=%d label=%s phase=%s target=(%.0f, %.0f) player=(%.0f, %.0f)",
            trace && trace->route_name ? trace->route_name : "",
            trace && trace->context ? trace->context : "",
            waypointIndex,
            waypoint.label != nullptr ? waypoint.label : "",
            phase == DungeonCombat::AggroWaypointPhase::BeforeAdvance ? "before-advance"
                                                                      : "after-advance",
            waypoint.x,
            waypoint.y,
            me ? me->x : 0.0f,
            me ? me->y : 0.0f);
    }

    switch (labelKind) {
    case DungeonRoute::WaypointLabelKind::AsuraFlameStaff: {
        if (phase != DungeonCombat::AggroWaypointPhase::AfterAdvance) {
            return true;
        }

        LogArachnisMain("aggro waypoint flame-staff index=%d label=%s phase=after-advance",
                        waypointIndex,
                        waypoint.label != nullptr ? waypoint.label : "");
        LogHeldBundleState("flame-staff hook begin", &waypoint, waypointIndex);
        if (HasHeldBundle()) {
            LogHeldBundleState("flame-staff hook already holding", &waypoint, waypointIndex);
            return true;
        }

        const bool pickedNearby = DungeonBundle::PickUpNearestItemByModelNearPoint(
            waypoint.x,
            waypoint.y,
            kAsuraFlameStaffModelId,
            600.0f,
            3,
            500u);
        const bool pickedFallback = !pickedNearby && DungeonBundle::PickUpNearestItemByModelNearPoint(
                waypoint.x,
                waypoint.y,
                kAsuraFlameStaffModelId,
                18000.0f,
                3,
                500u);
        LogBot("Arachnis: flame-staff hook waypoint %d pickup nearby=%d fallback=%d",
               waypointIndex,
               pickedNearby ? 1 : 0,
               pickedFallback ? 1 : 0);
        if (!pickedNearby && !pickedFallback) {
            LogHeldBundleState("flame-staff hook no pickup", &waypoint, waypointIndex);
            LogBot("Arachnis: no flame staff bundle found at waypoint %d (%.0f, %.0f)",
                   waypointIndex,
                   waypoint.x,
                   waypoint.y);
            return false;
        }

        WaitMs(500u);
        LogHeldBundleState("flame-staff hook end", &waypoint, waypointIndex);
        if (!HasHeldBundle()) {
            LogBot("Arachnis: flame staff pickup at waypoint %d did not result in a held bundle",
                   waypointIndex);
            return false;
        }
        return true;
    }

    case DungeonRoute::WaypointLabelKind::StaffCheck: {
        if (phase != DungeonCombat::AggroWaypointPhase::BeforeAdvance) {
            return true;
        }

        LogArachnisMain("aggro waypoint staff-check index=%d label=%s phase=before-advance",
                        waypointIndex,
                        waypoint.label != nullptr ? waypoint.label : "");
        LogHeldBundleState("staff-check hook begin", &waypoint, waypointIndex);
        if (HasHeldBundle()) {
            LogHeldBundleState("staff-check hook pass", &waypoint, waypointIndex);
            return true;
        }

        LogBot("Arachnis: staff check at waypoint %d missing bundle; attempting reacquire",
               waypointIndex);
        const bool reacquired = EnsureHeldBundleNearPlayer();
        LogHeldBundleState(
            reacquired ? "staff-check hook reacquired" : "staff-check hook failed",
            &waypoint,
            waypointIndex);
        return reacquired;
    }

    default:
        return true;
    }
}

bool PickUpObjectiveBundle(const FlameStaffObjective& objective, uint32_t mapId) {
    if (!MoveToTravelPointWithAggro(objective.pickup_point, mapId, 250.0f, 1200.0f, 60000u)) {
        return false;
    }
    if (HasHeldBundle()) {
        return true;
    }
    const bool pickedNearby = DungeonBundle::PickUpNearestItemByModelNearPoint(
        objective.pickup_point.x,
        objective.pickup_point.y,
        kAsuraFlameStaffModelId,
        600.0f,
        3,
        500u);
    if (pickedNearby && HasHeldBundle()) {
        return true;
    }

    const bool pickedFallback = DungeonBundle::PickUpNearestItemByModelNearPoint(
        objective.pickup_point.x,
        objective.pickup_point.y,
        kAsuraFlameStaffModelId,
        18000.0f,
        3,
        500u);
    if (!pickedFallback) {
        LogBot("Arachnis: no flame staff bundle found near objective pickup at %.0f, %.0f",
               objective.pickup_point.x,
               objective.pickup_point.y);
        return false;
    }
    WaitMs(500u);
    if (!HasHeldBundle()) {
        LogBot("Arachnis: interacted with flame staff at %.0f, %.0f but did not pick it up",
               objective.pickup_point.x,
               objective.pickup_point.y);
        return false;
    }
    return true;
}

bool ExecuteBundleClearPath(const FlameStaffObjective& objective, uint32_t mapId, bool dropBundle) {
    if (!EnsureHeldBundleNearPlayer()) {
        return false;
    }

    const bool hasDropPoint = objective.drop_point.x != 0.0f || objective.drop_point.y != 0.0f;
    const int totalLegCount = objective.web_clear_path_count + ((dropBundle && hasDropPoint) ? 1 : 0);
    LogHeldBundleState("bundle-clear begin");

    for (int i = 0; i < objective.web_clear_path_count; ++i) {
        LogBundleCarryTarget("bundle-clear moving", i + 1, totalLegCount, objective.web_clear_path[i]);
        if (!MoveToTravelPoint(objective.web_clear_path[i], mapId, 250.0f, 30000u)) {
            LogBundleCarryTarget("bundle-clear move failed", i + 1, totalLegCount, objective.web_clear_path[i]);
            return false;
        }
        LogBundleCarryTarget("bundle-clear arrived", i + 1, totalLegCount, objective.web_clear_path[i]);
    }

    if (!dropBundle) {
        return true;
    }

    if (hasDropPoint) {
        LogBundleCarryTarget(
            "bundle-clear moving",
            objective.web_clear_path_count + 1,
            totalLegCount,
            objective.drop_point);
        if (!MoveToTravelPoint(objective.drop_point, mapId, 250.0f, 30000u)) {
            LogBundleCarryTarget(
                "bundle-clear move failed",
                objective.web_clear_path_count + 1,
                totalLegCount,
                objective.drop_point);
            return false;
        }
        LogBundleCarryTarget(
            "bundle-clear arrived",
            objective.web_clear_path_count + 1,
            totalLegCount,
            objective.drop_point);
    }

    LogHeldBundleState("bundle-clear before drop");
    AgentMgr::CancelAction();
    WaitMs(100u);
    AgentMgr::ChangeTarget(0u);
    WaitMs(150u);
    if (!WaitForMovementToSettle(2000u, "bundle-clear drop settle")) {
        LogBot("Arachnis: movement did not settle before drop at %.0f, %.0f",
               objective.drop_point.x,
               objective.drop_point.y);
        return false;
    }
    bool dropped = TryDropFlameStaffBundle("bundle-clear");
    if (!dropped) {
        dropped = DungeonInteractions::DropHeldBundle(true);
    }
    LogBot("Arachnis: bundle-clear drop call target=(%.0f, %.0f) result=%d",
           objective.drop_point.x,
           objective.drop_point.y,
           dropped ? 1 : 0);
    LogHeldBundleState("bundle-clear after drop");
    if (!dropped) {
        LogBot("Arachnis: expected to drop flame staff bundle at %.0f, %.0f",
               objective.drop_point.x,
               objective.drop_point.y);
        return false;
    }

    if (!WaitForHeldBundleState(false, 3000u, "bundle-clear drop settled")) {
        LogBot("Arachnis: flame staff bundle still held after drop at %.0f, %.0f",
               objective.drop_point.x,
               objective.drop_point.y);
        return false;
    }
    return true;
}

bool ExecuteSpiderEggCluster(const SpiderEggCluster& cluster, uint32_t mapId) {
    if (!EnsureHeldBundleNearPlayer()) {
        return false;
    }

    for (int i = 0; i < cluster.egg_count; ++i) {
        if (!MoveToTravelPoint(cluster.egg_points[i], mapId, 250.0f, 30000u)) {
            return false;
        }
        if (!DungeonBundle::InteractSignpostNearPoint(
                cluster.egg_points[i].x,
                cluster.egg_points[i].y,
                1500.0f,
                1,
                500u)) {
            return false;
        }
    }

    if (cluster.post_cluster_path != nullptr && cluster.post_cluster_path_count > 0) {
        return FollowTravelPath(
            cluster.post_cluster_path,
            cluster.post_cluster_path_count,
            mapId,
            250.0f);
    }

    return true;
}

bool ExecuteLevel1KeyObjective() {
    const auto objective = GetLevel1KeyObjective();
    if (!FollowTravelPathWithAggro(
            objective.approach_path,
            objective.approach_path_count,
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            250.0f,
            1200.0f,
            60000u)) {
        return false;
    }

    for (int attempt = 0; attempt < objective.pickup_attempts; ++attempt) {
        if (!MoveToTravelPointWithAggro(
                objective.pickup_point,
                GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
                300.0f,
                1200.0f,
                15000u)) {
            return false;
        }
        (void)DungeonBundle::PickUpNearestItemNearPoint(
            objective.pickup_point.x,
            objective.pickup_point.y,
            18000.0f,
            1,
            500u);
    }

    if (!EnsureHeldBundleNearPlayer()) {
        return false;
    }

    for (int i = 0; i < objective.web_clear_path_count; ++i) {
        if (!MoveToTravelPointWithAggro(
                objective.web_clear_path[i],
                GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
                250.0f,
                1200.0f,
                60000u)) {
            return false;
        }
    }

    if (!MoveToTravelPointWithAggro(
            objective.drop_point,
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            250.0f,
            1200.0f,
            60000u)) {
        return false;
    }
    if (!TryDropFlameStaffBundle("level1-key-objective")
        && !DungeonInteractions::DropHeldBundle(true)) {
        LogBot("Arachnis: expected held bundle before level 1 door drop");
        return false;
    }
    WaitMs(500u);
    return true;
}

void ClearTargetAndStopForDoor() {
    AgentMgr::CancelAction();
    WaitMs(100u);
    AgentMgr::ChangeTarget(0u);
    WaitMs(150u);
}

bool SendActionInteractBurst(int interactCount, uint32_t delayMs) {
    if (interactCount <= 0) {
        return false;
    }

    for (int i = 0; i < interactCount; ++i) {
        if (!AgentMgr::ActionInteract()) {
            return false;
        }
        if (delayMs != 0u && i + 1 < interactCount) {
            WaitMs(delayMs);
        }
    }
    return true;
}

bool ExecuteLevel1ExitDoorInteractSequence(const DoorOpenObjective& door) {
    if (!MoveToTravelPoint(door.interact_point, GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 200.0f, 15000u)) {
        return false;
    }
    WaitMs(1000u);

    ClearTargetAndStopForDoor();
    if (!SendActionInteractBurst(2, 100u)) {
        return false;
    }

    WaitMs(500u);
    if (!MoveToTravelPoint(door.interact_point, GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 200.0f, 15000u)) {
        return false;
    }

    WaitMs(1000u);
    ClearTargetAndStopForDoor();
    if (!SendActionInteractBurst(2, 100u)) {
        return false;
    }

    WaitMs(1000u);
    if (!SendActionInteractBurst(2, 100u)) {
        return false;
    }

    return true;
}

bool ExecuteLevel1ExitDoor() {
    if (!ExecuteRoute(RouteId::Level1Exit, "level 1 exit")) {
        return false;
    }

    const auto door = GetLevel1ExitDoorObjective();
    if (!MoveToTravelPointWithAggro(
            door.interact_point,
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            200.0f,
            1200.0f,
            15000u)) {
        return false;
    }

    if (!ExecuteLevel1ExitDoorInteractSequence(door)) {
        LogBot("Arachnis: level 1 exit door interaction failed");
        return false;
    }

    const ZoneTransitionAttemptResult transition = ZoneThroughPointWithTransitionHooks(
        door.zone_point.x,
        door.zone_point.y,
        GWA3::MapIds::ARACHNIS_HAUNT_LVL2,
        "level 1 exit to level 2");
    if (!transition.zoned) {
        LogBot("Arachnis: failed zoning into level 2");
        return false;
    }
    return transition.ready;
}

bool ExecuteRewardChestFlow() {
    const auto reward = GetRewardChestObjective();
    if (!MoveToTravelPointWithAggro(reward.chest_point, GWA3::MapIds::ARACHNIS_HAUNT_LVL2, 250.0f, 1200.0f, 60000u)) {
        return false;
    }

    for (int pass = 0; pass < reward.chest_interact_passes; ++pass) {
        if (!DungeonBundle::InteractSignpostNearPoint(
                reward.chest_point.x,
                reward.chest_point.y,
                1500.0f,
                reward.chest_interact_count,
                reward.chest_interact_delay_ms)) {
            return false;
        }
        (void)DungeonBundle::PickUpNearestItemNearPoint(
            reward.chest_point.x,
            reward.chest_point.y,
            18000.0f,
            reward.chest_loot_attempts,
            reward.chest_loot_delay_ms);
    }

    bool completionSent = DungeonQuestRuntime::InteractNearestNpcAndSendDialogPlan(
        reward.completion_npc,
        reward.reward_dialog);
    if (!completionSent) {
        completionSent = DungeonQuestRuntime::SendDialogPlan(reward.reward_dialog);
    }
    if (!completionSent) {
        LogBot("Arachnis: completion dialog hand-in failed after chest");
        return false;
    }

    return DungeonNavigation::WaitForMapId(GWA3::MapIds::MAGUS_STONES, 180000u);
}

bool MoveToQuestNpc(const DungeonQuest::QuestCyclePlan& plan, const char* context) {
    const DungeonQuest::TravelPoint npcPoint = {plan.npc.x, plan.npc.y};
    if (!MoveToTravelPointWithAggro(
            npcPoint,
            plan.start_map_id,
            300.0f,
            plan.npc.search_radius,
            120000u)) {
        LogBot("Arachnis: failed moving to quest NPC for %s", context);
        return false;
    }
    return true;
}

bool ExecuteQuestDialogs(
    const DungeonQuest::QuestCyclePlan& plan,
    const DungeonQuest::DialogPlan& dialogPlan,
    const char* context) {
    const DungeonQuestRuntime::DialogExecutionOptions dialogOptions = BuildQuestDialogOptions();
    if (!MoveToQuestNpc(plan, context)) {
        return false;
    }
    if (!DungeonQuestRuntime::InteractNearestNpcAndSendDialogPlan(plan.npc, dialogPlan, dialogOptions)) {
        LogBot("Arachnis: dialog plan failed during %s", context);
        return false;
    }
    AgentMgr::CancelAction();
    WaitMs(100u);
    AgentMgr::ChangeTarget(0u);
    WaitMs(150u);
    DialogMgr::ClearDialog();
    DialogMgr::ResetHookState();
    DialogMgr::ResetRecentUITrace();
    if (!WaitForMovementToSettle(2000u, context)) {
        LogBot("Arachnis: player state did not settle after %s dialogs", context);
        return false;
    }
    return true;
}

bool ExecuteQuestApproach(const DungeonQuest::QuestCyclePlan& plan) {
    if (FollowTravelPathWithAggro(
            plan.approach_path,
            plan.approach_path_count,
            plan.start_map_id,
            300.0f,
            1200.0f,
            60000u)) {
        return true;
    }

    LogBot("Arachnis: quest approach aggro path failed, retrying simple travel");
    LogArachnisMain("quest approach fallback simple-travel map=%u pointCount=%d",
                    plan.start_map_id,
                    plan.approach_path_count);
    return FollowTravelPath(
        plan.approach_path,
        plan.approach_path_count,
        plan.start_map_id,
        300.0f);
}

bool ExecuteQuestReturn() {
    int count = 0;
    const auto* points = GetQuestReturnPath(count);
    if (FollowTravelPathWithAggro(
            points,
            count,
            GWA3::MapIds::MAGUS_STONES,
            300.0f,
            1200.0f,
            60000u)) {
        return true;
    }

    LogBot("Arachnis: quest return aggro path failed, retrying simple travel");
    return FollowTravelPath(
        points,
        count,
        GWA3::MapIds::MAGUS_STONES,
        300.0f);
}

bool ExecuteQuestCycle() {
    const auto plan = GetQuestCyclePlan();
    if (!DungeonQuest::IsValidQuestCyclePlan(plan)) {
        return false;
    }

    LogBot("Arachnis: starting reward/quest bootstrap");
    LogArachnisMain("quest cycle start bootstrap");
    if (!ExecuteQuestDialogs(plan, plan.reward_dialog, "reward bootstrap")) {
        LogArachnisMain("quest cycle bootstrap dialogs failed");
        return false;
    }

    if (!ExecuteQuestApproach(plan)) {
        LogBot("Arachnis: failed on quest approach before reward bounce");
        LogArachnisMain("quest cycle bootstrap approach failed");
        return false;
    }
    LogArachnisMain("quest cycle bootstrap zoning into level1");
    if (!ZoneAtTravelPointWithRetries(
            plan.dungeon_entry,
            plan.start_map_id,
            plan.dungeon_map_id,
            "reward bootstrap entry")) {
        LogBot("Arachnis: failed zoning into reward bootstrap dungeon instance");
        LogArachnisMain("quest cycle bootstrap entry failed");
        return false;
    }
    const ZoneTransitionAttemptResult reverseTransition = ZoneThroughPointWithTransitionHooks(
        plan.dungeon_exit.x,
        plan.dungeon_exit.y,
        plan.start_map_id,
        "reward bootstrap reverse");
    if (!reverseTransition.zoned) {
        LogBot("Arachnis: failed reversing out of reward bootstrap dungeon instance");
        LogArachnisMain("quest cycle bootstrap reverse failed");
        return false;
    }
    if (!reverseTransition.ready) {
        LogBot("Arachnis: Magus Stones did not finish loading after reward bootstrap reversal");
        LogArachnisMain("quest cycle bootstrap reverse ready failed");
        return false;
    }
    const bool heroesRecovered = WaitForPartyRecovery(plan.start_map_id, 1u, 8000u);
    LogBot("Arachnis: reward bootstrap returned to Magus; heroes=%u recovered=%d",
           PartyMgr::CountPartyHeroes(),
           heroesRecovered ? 1 : 0);
    LogArachnisMain("quest cycle bootstrap returned heroes=%u recovered=%d",
                    PartyMgr::CountPartyHeroes(),
                    heroesRecovered ? 1 : 0);

    if (!ExecuteQuestReturn()) {
        LogBot("Arachnis: failed on quest return after reward bootstrap");
        LogArachnisMain("quest cycle return path failed");
        return false;
    }

    LogBot("Arachnis: starting live quest accept flow");
    LogArachnisMain("quest cycle live accept start");
    if (!ExecuteQuestDialogs(plan, plan.accept_dialog, "quest accept")) {
        LogArachnisMain("quest cycle live accept dialogs failed");
        return false;
    }
    if (!ExecuteQuestApproach(plan)) {
        LogBot("Arachnis: failed on quest approach before live entry");
        LogArachnisMain("quest cycle live approach failed");
        return false;
    }
    LogArachnisMain("quest cycle live entry zoning into level1");
    if (!ZoneAtTravelPointWithRetries(
            plan.dungeon_entry,
            plan.start_map_id,
            plan.dungeon_map_id,
            "level 1 entry")) {
        LogBot("Arachnis: failed zoning into level 1");
        LogArachnisMain("quest cycle live entry failed");
        return false;
    }
    LogArachnisMain("quest cycle live entry succeeded");
    return true;
}

bool ExecuteLevel1Phase2();
bool ExecuteLevel1Phase2();
bool ExecuteLevel1Phase3();
bool ExecuteLevel1Phase4();
bool ExecuteLevel1Exit();
bool ExecuteLevel2Phase2();
bool ExecuteLevel2Phase3();

bool ExecuteLevel1Phase1() {
    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    if (objectiveCount < 2) {
        return false;
    }

    return ExecuteRoute(RouteId::Level1Phase1, "level 1 phase 1") &&
           PickUpObjectiveBundle(objectives[0], GWA3::MapIds::ARACHNIS_HAUNT_LVL1) &&
           ExecuteBundleClearPath(objectives[0], GWA3::MapIds::ARACHNIS_HAUNT_LVL1, true) &&
           ExecuteLevel1Phase2();
}

bool ExecuteLevel1Phase2() {
    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    if (objectiveCount < 2) {
        return false;
    }

    return ExecuteRoute(RouteId::Level1Phase2, "level 1 phase 2") &&
           PickUpObjectiveBundle(objectives[1], GWA3::MapIds::ARACHNIS_HAUNT_LVL1) &&
           ExecuteBundleClearPath(objectives[1], GWA3::MapIds::ARACHNIS_HAUNT_LVL1, false) &&
           ExecuteLevel1Phase3();
}

bool ExecuteLevel1Phase3() {
    int eggCount = 0;
    const auto* eggs = GetSpiderEggClusters(eggCount);
    if (eggCount < 1) {
        return false;
    }

    return ExecuteRoute(RouteId::Level1Phase3, "level 1 phase 3") &&
           EnsureHeldBundleNearPlayer() &&
           ExecuteSpiderEggCluster(eggs[0], GWA3::MapIds::ARACHNIS_HAUNT_LVL1) &&
           ExecuteLevel1Phase4();
}

bool ExecuteLevel1Phase4() {
    int eggCount = 0;
    const auto* eggs = GetSpiderEggClusters(eggCount);
    if (eggCount < 2) {
        return false;
    }

    return ExecuteRoute(RouteId::Level1Phase4, "level 1 phase 4") &&
           EnsureHeldBundleNearPlayer() &&
           ExecuteSpiderEggCluster(eggs[1], GWA3::MapIds::ARACHNIS_HAUNT_LVL1) &&
           ExecuteLevel1KeyObjective() &&
           ExecuteLevel1Exit();
}

bool ExecuteLevel1Exit() {
    return ExecuteLevel1ExitDoor();
}

bool ExecuteLevel2Phase1() {
    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    if (objectiveCount < 4) {
        return false;
    }

    return ExecuteRoute(RouteId::Level2Phase1Approach, "level 2 phase 1 pickup") &&
           PickUpObjectiveBundle(objectives[2], GWA3::MapIds::ARACHNIS_HAUNT_LVL2) &&
           ExecuteRoute(RouteId::Level2Phase1Transit, "level 2 phase 1 transit") &&
           ExecuteBundleClearPath(objectives[2], GWA3::MapIds::ARACHNIS_HAUNT_LVL2, true) &&
           ExecuteLevel2Phase2();
}

bool ExecuteLevel2Phase2() {
    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    if (objectiveCount < 4) {
        return false;
    }

    return ExecuteRoute(RouteId::Level2Phase2, "level 2 phase 2") &&
           PickUpObjectiveBundle(objectives[3], GWA3::MapIds::ARACHNIS_HAUNT_LVL2) &&
           ExecuteBundleClearPath(objectives[3], GWA3::MapIds::ARACHNIS_HAUNT_LVL2, false) &&
           ExecuteLevel2Phase3();
}

bool ExecuteLevel2Phase3() {
    int eggCount = 0;
    const auto* eggs = GetSpiderEggClusters(eggCount);
    if (eggCount < 6) {
        return false;
    }

    return ExecuteRoute(RouteId::Level2Phase3, "level 2 phase 3") &&
           EnsureHeldBundleNearPlayer() &&
           ExecuteSpiderEggCluster(eggs[2], GWA3::MapIds::ARACHNIS_HAUNT_LVL2) &&
           ExecuteSpiderEggCluster(eggs[3], GWA3::MapIds::ARACHNIS_HAUNT_LVL2) &&
           ExecuteSpiderEggCluster(eggs[4], GWA3::MapIds::ARACHNIS_HAUNT_LVL2) &&
           ExecuteSpiderEggCluster(eggs[5], GWA3::MapIds::ARACHNIS_HAUNT_LVL2) &&
           ExecuteRewardChestFlow();
}

bool ExecuteLevel1FromCurrentPosition() {
    const auto& dispatch = GetStageDefinition(StageId::Level1);
    const int nearestIndex = GetNearestWaypointIndexForRoute(dispatch);
    if (nearestIndex < 0) {
        return false;
    }

    LogArachnisMain("level1 dispatch nearestIndex=%d", nearestIndex);

    if (nearestIndex < 2) {
        LogArachnisMain("level1 dispatch selecting phase1");
        return ExecuteLevel1Phase1();
    }
    if (nearestIndex < 10) {
        LogArachnisMain("level1 dispatch selecting phase2");
        return ExecuteLevel1Phase2();
    }
    if (nearestIndex < 12) {
        LogArachnisMain("level1 dispatch selecting phase3");
        return ExecuteLevel1Phase3();
    }
    if (nearestIndex < 14) {
        LogArachnisMain("level1 dispatch selecting phase4");
        return ExecuteLevel1Phase4();
    }
    LogArachnisMain("level1 dispatch selecting exit");
    return ExecuteLevel1Exit();
}

bool ExecuteLevel2FromCurrentPosition() {
    const auto& dispatch = GetStageDefinition(StageId::Level2);
    const int nearestIndex = GetNearestWaypointIndexForRoute(dispatch);
    if (nearestIndex < 0) {
        return false;
    }

    LogArachnisMain("level2 dispatch nearestIndex=%d", nearestIndex);

    if (nearestIndex < 2) {
        LogArachnisMain("level2 dispatch selecting phase1");
        return ExecuteLevel2Phase1();
    }
    if (nearestIndex < 14) {
        LogArachnisMain("level2 dispatch selecting phase2");
        return ExecuteLevel2Phase2();
    }
    LogArachnisMain("level2 dispatch selecting phase3");
    return ExecuteLevel2Phase3();
}

BotState HandleCharSelect(BotConfig&) {
    return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

BotState HandleTownSetup(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::RATA_SUM || mapId == GWA3::MapIds::MAGUS_STONES) {
        return BotState::Traveling;
    }
    if (mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1 || mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL2) {
        return BotState::InDungeon;
    }

    LogBot("Arachnis: traveling to Rata Sum from map %u", mapId);
    MapMgr::Travel(GWA3::MapIds::RATA_SUM);
    if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::RATA_SUM, 60000u)) {
        return BotState::Error;
    }
    if (!WaitForMapReady(GWA3::MapIds::RATA_SUM, 10000u)) {
        return BotState::Error;
    }
    return BotState::Traveling;
}

BotState HandleTravel(BotConfig&) {
    switch (MapMgr::GetMapId()) {
    case GWA3::MapIds::RATA_SUM:
        if (!WaitForMapReady(GWA3::MapIds::RATA_SUM, 10000u)) {
            return BotState::Error;
        }
        return ExecuteRoute(RouteId::RunRataSumToMagusStones, "Rata Sum to Magus Stones", true)
                   ? BotState::Traveling
                   : BotState::Error;
    case GWA3::MapIds::MAGUS_STONES:
        if (!WaitForMapReady(GWA3::MapIds::MAGUS_STONES, 10000u)) {
            return BotState::Error;
        }
        if (!ExecuteRoute(RouteId::RunMagusToDungeon, "Magus Stones dungeon approach")) {
            return BotState::Error;
        }
        return ExecuteQuestCycle() ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::ARACHNIS_HAUNT_LVL1:
    case GWA3::MapIds::ARACHNIS_HAUNT_LVL2:
        return BotState::InDungeon;
    default:
        LogBot("Arachnis: unsupported travel map %u", MapMgr::GetMapId());
        return BotState::Error;
    }
}

BotState HandleDungeon(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1 || mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL2) {
        if (!WaitForMapReady(mapId, 10000u)) {
            LogBot("Arachnis: dungeon map %u not ready for route execution", mapId);
            return BotState::Error;
        }
    }

    switch (mapId) {
    case GWA3::MapIds::ARACHNIS_HAUNT_LVL1:
        return ExecuteLevel1FromCurrentPosition() ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::ARACHNIS_HAUNT_LVL2:
        return ExecuteLevel2FromCurrentPosition() ? BotState::InTown : BotState::Error;
    default:
        return BotState::InTown;
    }
}

BotState HandleError(BotConfig&) {
    LogBot("Arachnis: ERROR state - waiting before retry");
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
    cfg.target_map_id = GWA3::MapIds::ARACHNIS_HAUNT_LVL1;
    cfg.outpost_map_id = GWA3::MapIds::RATA_SUM;
    cfg.bot_module_name = "ArachnisHaunt";

    LogBot("Arachnis Haunt module registered (phase runtime enabled)");
}

} // namespace GWA3::Bot::ArachnisHauntBot
