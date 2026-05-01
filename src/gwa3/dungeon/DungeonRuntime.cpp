#include <gwa3/dungeon/DungeonRuntime.h>

#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonNavigation.h>

#include <Windows.h>

namespace GWA3::DungeonRuntime {

namespace {

template <typename Predicate>
bool WaitForPredicate(uint32_t timeoutMs, Predicate&& predicate, uint32_t pollMs = 250u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (predicate()) {
            return true;
        }
        Sleep(pollMs);
    }
    return predicate();
}

const char* ContextOrDefault(const char* context) {
    return context ? context : "Outpost";
}

const char* TransitionContextOrDefault(const char* context, const char* fallback) {
    return context ? context : fallback;
}

bool IsListedMap(uint32_t mapId, const uint32_t* mapIds, int mapCount) {
    if (!mapIds || mapCount <= 0) return false;
    for (int i = 0; i < mapCount; ++i) {
        if (mapIds[i] == mapId) return true;
    }
    return false;
}

const char* LogPrefixOrDefault(const char* logPrefix) {
    return logPrefix ? logPrefix : "DungeonRuntime";
}

} // namespace

bool IsDead() {
    auto* me = AgentMgr::GetMyAgent();
    return !me || me->hp <= 0.0f;
}

bool IsMapLoaded() {
    return MapMgr::GetIsMapLoaded();
}

void WaitMs(uint32_t ms) {
    Sleep(ms);
}

bool WaitForCondition(uint32_t timeoutMs, const std::function<bool()>& predicate, uint32_t pollMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (predicate && predicate()) {
            return true;
        }
        Sleep(pollMs);
    }
    return predicate && predicate();
}

void SuspendTransitionSensitiveHooks(const char* context) {
    AgentMgr::ResetMoveState(TransitionContextOrDefault(context, "Dungeon transition suspend"));
    CtoS::SuspendEngineHook();
    DialogMgr::Shutdown();
}

void ResumeTransitionSensitiveHooks(const char* context) {
    AgentMgr::ResetMoveState(TransitionContextOrDefault(context, "Dungeon transition resume"));
    CtoS::ResumeEngineHook();
    DialogMgr::Initialize();
}

bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs) {
    return WaitForPredicate(timeoutMs, [mapId]() {
        return MapMgr::GetMapId() == mapId &&
               MapMgr::GetIsMapLoaded() &&
               AgentMgr::GetMyId() > 0u;
    });
}

bool WaitForTownRuntimeReady(uint32_t mapId, uint32_t timeoutMs) {
    return WaitForPredicate(timeoutMs, [mapId]() {
        if (MapMgr::GetMapId() != mapId ||
            !MapMgr::GetIsMapLoaded() ||
            AgentMgr::GetMyId() == 0u ||
            AgentMgr::GetMyAgent() == nullptr) {
            return false;
        }
        auto* inv = ItemMgr::GetInventory();
        if (!inv) return false;
        return inv->bags[1] != nullptr;
    });
}

bool WaitForLevelSpawnReady(
    uint32_t targetMapId,
    const TransitionAnchor& staleAnchor,
    float staleAnchorClearance,
    uint32_t timeoutMs,
    uint32_t pollMs) {
    return WaitForCondition(timeoutMs, [targetMapId, staleAnchor, staleAnchorClearance]() {
        if (MapMgr::GetMapId() != targetMapId ||
            !MapMgr::GetIsMapLoaded() ||
            AgentMgr::GetMyId() == 0) {
            return false;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (!me || me->hp <= 0.0f) {
            return false;
        }

        if (staleAnchorClearance <= 0.0f) {
            return true;
        }
        const float distFromStaleAnchor = AgentMgr::GetDistance(
            me->x,
            me->y,
            staleAnchor.x,
            staleAnchor.y);
        return distFromStaleAnchor > staleAnchorClearance;
    }, pollMs);
}

TransitionTelemetry CaptureLevelTransitionTelemetry(
    const TransitionAnchor& exitAnchor,
    uint32_t portalId,
    float nearestEnemyRange,
    float nearbyEnemyRange) {
    TransitionTelemetry telemetry;
    auto* me = AgentMgr::GetMyAgent();
    telemetry.map_loaded = MapMgr::GetIsMapLoaded();
    telemetry.player_alive = me != nullptr && me->hp > 0.0f;
    telemetry.player_hp = me ? me->hp : 0.0f;
    telemetry.player_x = me ? me->x : 0.0f;
    telemetry.player_y = me ? me->y : 0.0f;
    telemetry.target_id = AgentMgr::GetTargetId();
    telemetry.dist_to_exit = me
        ? AgentMgr::GetDistance(me->x, me->y, exitAnchor.x, exitAnchor.y)
        : -1.0f;
    telemetry.nearest_enemy_dist = me
        ? DungeonCombat::GetNearestLivingEnemyDistance(nearestEnemyRange)
        : -1.0f;
    telemetry.nearby_enemy_count = me
        ? DungeonCombat::CountLivingEnemiesInRange(nearbyEnemyRange)
        : 0u;
    telemetry.portal_id = portalId;

    if (portalId != 0u) {
        if (auto* portal = AgentMgr::GetAgentByID(portalId)) {
            telemetry.portal_x = portal->x;
            telemetry.portal_y = portal->y;
            telemetry.portal_type = portal->type;
            if (portal->type == 0x200) {
                telemetry.portal_gadget = static_cast<AgentGadget*>(portal)->gadget_id;
            }
            if (me) {
                telemetry.portal_dist = AgentMgr::GetDistance(me->x, me->y, portal->x, portal->y);
            }
        }
    }

    return telemetry;
}

void LogLevelTransitionTelemetry(
    const char* logPrefix,
    const char* transitionName,
    const char* stage,
    uint32_t portalId,
    uint32_t elapsedMs,
    uint32_t attempt,
    const TransitionAnchor& exitAnchor,
    float nearestEnemyRange,
    float nearbyEnemyRange,
    TransitionTelemetry* outTelemetry) {
    const auto telemetry = CaptureLevelTransitionTelemetry(
        exitAnchor,
        portalId,
        nearestEnemyRange,
        nearbyEnemyRange);
    if (outTelemetry) {
        *outTelemetry = telemetry;
    }
    Log::Info("%s: %s [%s] attempt=%lu elapsed=%lums map=%u loaded=%d me=(%.0f, %.0f) distToExit=%.0f target=%u portal=%u type=0x%X gadget=%u portalPos=(%.0f, %.0f) portalDist=%.0f",
              logPrefix ? logPrefix : "DungeonRuntime",
              transitionName ? transitionName : "Level transition",
              stage ? stage : "unknown",
              static_cast<unsigned long>(attempt),
              static_cast<unsigned long>(elapsedMs),
              MapMgr::GetMapId(),
              telemetry.map_loaded ? 1 : 0,
              telemetry.player_x,
              telemetry.player_y,
              telemetry.dist_to_exit,
              telemetry.target_id,
              portalId,
              telemetry.portal_type,
              telemetry.portal_gadget,
              telemetry.portal_x,
              telemetry.portal_y,
              telemetry.portal_dist);
}

LevelTransitionResult ExecuteLevelTransition(const LevelTransitionOptions& options) {
    LevelTransitionResult result;
    const char* prefix = LogPrefixOrDefault(options.log_prefix);
    const char* name = options.transition_name ? options.transition_name : "Level transition";
    QueueMoveFn queueMove = options.queue_move ? options.queue_move : &AgentMgr::Move;
    GetMapIdFn getMapId = options.get_map_id ? options.get_map_id : &MapMgr::GetMapId;
    WaitMsFn waitMs = options.wait_ms ? options.wait_ms : &WaitMs;

    if (options.target_map_id == 0u || queueMove == nullptr || getMapId == nullptr || waitMs == nullptr) {
        result.final_map_id = getMapId ? getMapId() : 0u;
        return result;
    }

    if (options.on_started) {
        options.on_started();
    }

    uint32_t portalId = 0u;
    if (options.find_portal) {
        portalId = options.find_portal(
            options.exit_anchor.x,
            options.exit_anchor.y,
            options.portal_search_radius);
    }
    result.portal_id = portalId;

    Log::Info("%s: %s move=(%.0f, %.0f) targetMap=%u portal=%u",
              prefix,
              name,
              options.exit_anchor.x,
              options.exit_anchor.y,
              options.target_map_id,
              portalId);

    auto logStage = [&](const char* stage, uint32_t elapsedMs, uint32_t attempt) {
        TransitionTelemetry telemetry;
        LogLevelTransitionTelemetry(
            prefix,
            name,
            stage,
            portalId,
            elapsedMs,
            attempt,
            options.exit_anchor,
            options.nearest_enemy_range,
            options.nearby_enemy_range,
            &telemetry);
        if (options.on_telemetry) {
            options.on_telemetry(telemetry);
        }
    };

    logStage("start", 0u, 0u);
    const DWORD start = GetTickCount();
    DWORD lastLogAt = 0u;
    while ((GetTickCount() - start) < options.timeout_ms) {
        ++result.attempts;
        if (options.on_attempt) {
            options.on_attempt(result.attempts);
        }

        const DWORD elapsed = GetTickCount() - start;
        if (elapsed - lastLogAt >= options.log_interval_ms) {
            if (options.find_portal) {
                const uint32_t refreshedPortalId = options.find_portal(
                    options.exit_anchor.x,
                    options.exit_anchor.y,
                    options.portal_search_radius);
                if (refreshedPortalId != portalId) {
                    Log::Info("%s: %s portal refresh old=%u new=%u",
                              prefix,
                              name,
                              portalId,
                              refreshedPortalId);
                    portalId = refreshedPortalId;
                    result.portal_id = portalId;
                }
            }
            logStage("loop", elapsed, result.attempts);
            lastLogAt = elapsed;
        }

        queueMove(options.exit_anchor.x, options.exit_anchor.y);
        waitMs(options.move_poll_ms);
        if (getMapId() == options.target_map_id) {
            result.entered_target_map = true;
            result.elapsed_ms = GetTickCount() - start;
            result.final_map_id = getMapId();
            logStage("entered", result.elapsed_ms, result.attempts);
            if (options.on_entered) {
                options.on_entered(result.attempts);
            }
            if (options.spawn_ready_timeout_ms > 0u) {
                result.spawn_ready = WaitForLevelSpawnReady(
                    options.target_map_id,
                    options.spawn_stale_anchor,
                    options.spawn_stale_anchor_clearance,
                    options.spawn_ready_timeout_ms,
                    options.spawn_ready_poll_ms);
            } else {
                result.spawn_ready = true;
            }
            auto* me = AgentMgr::GetMyAgent();
            Log::Info("%s: %s spawn ready=%d player=(%.0f, %.0f)",
                      prefix,
                      name,
                      result.spawn_ready ? 1 : 0,
                      me ? me->x : 0.0f,
                      me ? me->y : 0.0f);
            if (options.spawn_settle_timeout_ms > 0u && options.spawn_settle_distance > 0.0f) {
                DungeonNavigation::WaitForLocalPositionSettle(
                    options.spawn_settle_timeout_ms,
                    options.spawn_settle_distance);
            }
            return result;
        }
    }

    result.elapsed_ms = GetTickCount() - start;
    result.final_map_id = getMapId();
    logStage("timeout", result.elapsed_ms, result.attempts);
    return result;
}

bool EnsureOutpostReady(uint32_t outpostMapId, uint32_t timeoutMs, const char* context) {
    const uint32_t currentMapId = MapMgr::GetMapId();
    const uint32_t loadingState = MapMgr::GetLoadingState();
    if (currentMapId == outpostMapId) {
        const bool ready = WaitForMapReady(outpostMapId, timeoutMs);
        Log::Info("DungeonRuntime: %s outpost already selected map=%u ready=%d loading=%u",
                  ContextOrDefault(context),
                  outpostMapId,
                  ready ? 1 : 0,
                  loadingState);
        return ready;
    }

    Log::Info("DungeonRuntime: %s traveling to outpost map=%u from map=%u loading=%u",
              ContextOrDefault(context),
              outpostMapId,
              currentMapId,
              loadingState);
    if (!MapMgr::Travel(outpostMapId)) {
        Log::Warn("DungeonRuntime: %s travel request rejected target=%u currentMap=%u loading=%u",
                  ContextOrDefault(context),
                  outpostMapId,
                  currentMapId,
                  loadingState);
        return false;
    }

    const bool ready = WaitForMapReady(outpostMapId, timeoutMs);
    Log::Info("DungeonRuntime: %s outpost travel result ready=%d currentMap=%u loading=%u",
              ContextOrDefault(context),
              ready ? 1 : 0,
              MapMgr::GetMapId(),
              MapMgr::GetLoadingState());
    return ready;
}

bool PushUntilMapReady(
    uint32_t targetMapId,
    float pushX,
    float pushY,
    uint32_t transitionTimeoutMs,
    uint32_t loadTimeoutMs,
    uint32_t settleMs,
    const char* context) {
    const char* transitionContext = TransitionContextOrDefault(context, "Dungeon transition");
    SuspendTransitionSensitiveHooks(transitionContext);
    const bool transitioned = WaitForCondition(transitionTimeoutMs, [targetMapId, pushX, pushY]() {
        if (MapMgr::GetMapId() == targetMapId) {
            return true;
        }
        AgentMgr::Move(pushX, pushY);
        return false;
    }, 250u);
    ResumeTransitionSensitiveHooks(transitionContext);

    if (!transitioned) {
        return false;
    }
    if (settleMs > 0u) {
        WaitMs(settleMs);
    }
    return WaitForMapReady(targetMapId, loadTimeoutMs);
}

bool StageAndPushUntilMapReady(
    uint32_t targetMapId,
    float stageX,
    float stageY,
    float stageThreshold,
    float pushX,
    float pushY,
    MoveToPointFn move_to_point,
    uint32_t prePushDelayMs,
    uint32_t transitionTimeoutMs,
    uint32_t loadTimeoutMs,
    uint32_t settleMs,
    const char* context) {
    if (move_to_point) {
        (void)move_to_point(stageX, stageY, stageThreshold);
    } else {
        AgentMgr::Move(stageX, stageY);
    }
    AgentMgr::Move(pushX, pushY);
    if (prePushDelayMs > 0u) {
        WaitMs(prePushDelayMs);
    }
    return PushUntilMapReady(
        targetMapId,
        pushX,
        pushY,
        transitionTimeoutMs,
        loadTimeoutMs,
        settleMs,
        context);
}

bool ExecuteMapTransitionMove(
    float x,
    float y,
    uint32_t targetMapId,
    VoidMoveToPointFn moveToPoint,
    QueueMoveFn queueMove,
    GetMapIdFn getMapId,
    WaitMsFn waitMs,
    uint32_t timeoutMs,
    uint32_t pulseDelayMs,
    float settleThreshold) {
    if (moveToPoint == nullptr || queueMove == nullptr || getMapId == nullptr || waitMs == nullptr) {
        return false;
    }

    moveToPoint(x, y, settleThreshold);
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        queueMove(x, y);
        waitMs(pulseDelayMs);
        if (getMapId() == targetMapId) {
            return true;
        }
    }

    return getMapId() == targetMapId;
}

bool WaitForPostDungeonReturn(
    uint32_t expectedMapId,
    uint32_t transitionTimeoutMs,
    uint32_t loadTimeoutMs,
    const uint32_t* dungeonMapIds,
    int dungeonMapCount,
    const char* logPrefix) {
    const char* prefix = LogPrefixOrDefault(logPrefix);
    Log::Info("%s waiting for post-dungeon transition expectedMap=%u", prefix, expectedMapId);
    DWORD start = GetTickCount();
    DWORD lastLogAt = 0u;
    uint32_t lastMapId = 0xFFFFFFFFu;
    int lastLoaded = -1;
    uint32_t lastMyId = 0xFFFFFFFFu;
    bool sawUnload = false;
    bool leftDungeonState = false;

    while ((GetTickCount() - start) < transitionTimeoutMs) {
        const uint32_t mapId = MapMgr::GetMapId();
        const bool loaded = MapMgr::GetIsMapLoaded();
        const uint32_t myId = AgentMgr::GetMyId();
        const DWORD elapsed = GetTickCount() - start;

        if (mapId != lastMapId || static_cast<int>(loaded ? 1 : 0) != lastLoaded ||
            myId != lastMyId || (elapsed - lastLogAt) >= 5000u) {
            Log::Info("%s transition poll elapsed=%lu map=%u loaded=%d myId=%u sawUnload=%d",
                      prefix,
                      static_cast<unsigned long>(elapsed),
                      mapId,
                      loaded ? 1 : 0,
                      myId,
                      sawUnload ? 1 : 0);
            lastMapId = mapId;
            lastLoaded = loaded ? 1 : 0;
            lastMyId = myId;
            lastLogAt = elapsed;
        }

        if (mapId == expectedMapId && loaded && myId > 0u) {
            Log::Info("%s reached expected map=%u during transition wait", prefix, mapId);
            return true;
        }

        if (mapId == 0u || !loaded || myId == 0u) {
            sawUnload = true;
            leftDungeonState = true;
        } else if (!IsListedMap(mapId, dungeonMapIds, dungeonMapCount)) {
            leftDungeonState = true;
        }

        if (leftDungeonState) {
            break;
        }

        Sleep(250u);
    }

    Log::Info("%s transition state leftDungeonState=%d map=%u loaded=%d myId=%u",
              prefix,
              leftDungeonState ? 1 : 0,
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              AgentMgr::GetMyId());

    const DWORD loadStart = GetTickCount();
    lastLogAt = 0u;
    lastMapId = 0xFFFFFFFFu;
    lastLoaded = -1;
    lastMyId = 0xFFFFFFFFu;
    while ((GetTickCount() - loadStart) < loadTimeoutMs) {
        const uint32_t mapId = MapMgr::GetMapId();
        const bool loaded = MapMgr::GetIsMapLoaded();
        const uint32_t myId = AgentMgr::GetMyId();
        const DWORD elapsed = GetTickCount() - loadStart;

        if (mapId != lastMapId || static_cast<int>(loaded ? 1 : 0) != lastLoaded ||
            myId != lastMyId || (elapsed - lastLogAt) >= 5000u) {
            Log::Info("%s load poll elapsed=%lu map=%u loaded=%d myId=%u",
                      prefix,
                      static_cast<unsigned long>(elapsed),
                      mapId,
                      loaded ? 1 : 0,
                      myId);
            lastMapId = mapId;
            lastLoaded = loaded ? 1 : 0;
            lastMyId = myId;
            lastLogAt = elapsed;
        }

        if (mapId == expectedMapId && loaded && myId > 0u) {
            Log::Info("%s loaded=1 finalMap=%u myId=%u", prefix, mapId, myId);
            return true;
        }

        if (mapId != 0u && loaded && myId > 0u &&
            !IsListedMap(mapId, dungeonMapIds, dungeonMapCount) &&
            mapId != expectedMapId) {
            Log::Info("%s landed on unexpected loaded map=%u myId=%u", prefix, mapId, myId);
            return false;
        }

        Sleep(250u);
    }

    Log::Info("%s loaded=0 finalMap=%u myId=%u",
              prefix,
              MapMgr::GetMapId(),
              AgentMgr::GetMyId());
    return false;
}

PostRewardReturnResult HandlePostRewardReturn(const PostRewardReturnOptions& options) {
    PostRewardReturnResult result;
    const char* prefix = LogPrefixOrDefault(options.log_prefix);
    const char* label = options.label ? options.label : "Post reward";
    result.used_long_wait = options.reward_claimed || options.reward_dialog_latched;

    if (options.expected_return_map_id == 0u) {
        return result;
    }

    if (result.used_long_wait && options.salvage_reward_items != nullptr) {
        result.salvaged_item_count = options.salvage_reward_items();
        result.salvaged_reward_items = true;
        Log::Info("%s: %s reward salvage result salvaged=%u rewardClaimed=%d rewardLatched=%d",
                  prefix,
                  label,
                  result.salvaged_item_count,
                  options.reward_claimed ? 1 : 0,
                  options.reward_dialog_latched ? 1 : 0);
    } else if (!result.used_long_wait) {
        Log::Info("%s: %s reward not claimed and dialog not latched; skipping post-reward salvage",
                  prefix,
                  label);
    }

    result.returned_expected_map = WaitForPostDungeonReturn(
        options.expected_return_map_id,
        result.used_long_wait ? options.long_transition_timeout_ms : options.short_transition_timeout_ms,
        result.used_long_wait ? options.long_load_timeout_ms : options.short_load_timeout_ms,
        options.dungeon_map_ids,
        options.dungeon_map_count,
        prefix);
    if (result.returned_expected_map) {
        result.final_map_id = MapMgr::GetMapId();
        result.final_map_loaded = MapMgr::GetIsMapLoaded();
        result.final_player_id = AgentMgr::GetMyId();
        return result;
    }

    const uint32_t mapAfterReward = MapMgr::GetMapId();
    const bool mapLoadedAfterReward = MapMgr::GetIsMapLoaded();
    const uint32_t playerIdAfterReward = AgentMgr::GetMyId();
    if (IsListedMap(mapAfterReward, options.dungeon_map_ids, options.dungeon_map_count)) {
        if (!mapLoadedAfterReward || playerIdAfterReward == 0u) {
            result.skipped_fallback_ghost_state = true;
            Log::Info("%s: %s map stuck in ghost state map=%u loaded=%d playerId=%u; skipping explicit recovery travel",
                      prefix,
                      label,
                      mapAfterReward,
                      mapLoadedAfterReward ? 1 : 0,
                      playerIdAfterReward);
        } else if (options.fallback_recovery_map_id != 0u) {
            result.fallback_attempted = true;
            Log::Info("%s: %s still in dungeon after wait rewardClaimed=%d map=%u loaded=%d playerId=%u; traveling to recovery outpost map=%u",
                      prefix,
                      label,
                      options.reward_claimed ? 1 : 0,
                      mapAfterReward,
                      mapLoadedAfterReward ? 1 : 0,
                      playerIdAfterReward,
                      options.fallback_recovery_map_id);
            MapMgr::Travel(options.fallback_recovery_map_id);
            result.fallback_recovered = WaitForMapReady(
                options.fallback_recovery_map_id,
                options.fallback_recovery_timeout_ms);
            Log::Info("%s: %s recovery travel result=%d finalMap=%u loaded=%d playerId=%u",
                      prefix,
                      label,
                      result.fallback_recovered ? 1 : 0,
                      MapMgr::GetMapId(),
                      MapMgr::GetIsMapLoaded() ? 1 : 0,
                      AgentMgr::GetMyId());
        }
    } else {
        Log::Info("%s: %s wait ended off dungeon rewardClaimed=%d map=%u loaded=%d playerId=%u",
                  prefix,
                  label,
                  options.reward_claimed ? 1 : 0,
                  mapAfterReward,
                  mapLoadedAfterReward ? 1 : 0,
                  playerIdAfterReward);
    }

    result.final_map_id = MapMgr::GetMapId();
    result.final_map_loaded = MapMgr::GetIsMapLoaded();
    result.final_player_id = AgentMgr::GetMyId();
    return result;
}

} // namespace GWA3::DungeonRuntime
