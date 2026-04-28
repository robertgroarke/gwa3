#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonLoot.h>
#include <gwa3/dungeon/DungeonSkill.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <Windows.h>
#include <cstdlib>

namespace GWA3::DungeonNavigation {

namespace {

void IssueMoveDirect(float x, float y) {
    AgentMgr::Move(x, y);
}

MoveIssuerFn ResolveMoveIssuer(MoveIssuerFn moveIssuer) {
    return moveIssuer ? moveIssuer : &IssueMoveDirect;
}

} // namespace

bool IsWithinDistance(float currentX, float currentY, float targetX, float targetY, float threshold) {
    return AgentMgr::GetDistance(currentX, currentY, targetX, targetY) <= threshold;
}

float RandomizedCoordinate(float center, float radius) {
    static bool seeded = false;
    if (!seeded) {
        srand(GetTickCount());
        seeded = true;
    }
    const float unit = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return center + ((unit * 2.0f) - 1.0f) * radius;
}

StuckMonitor MakeStuckMonitor(float startX, float startY) {
    StuckMonitor monitor;
    monitor.last_x = startX;
    monitor.last_y = startY;
    return monitor;
}

StuckResolution EvaluateStuckMonitor(
    float currentX,
    float currentY,
    float /*targetX*/,
    float /*targetY*/,
    StuckMonitor& monitor,
    uint32_t randomSeed,
    float minimumProgress,
    int recoveryThreshold,
    int abortThreshold,
    float recoveryRadius) {
    StuckResolution resolution;
    if (!monitor.initialized) {
        monitor.last_x = currentX;
        monitor.last_y = currentY;
        monitor.low_movement_count = 1;
        monitor.initialized = true;
        return resolution;
    }

    const float moved = AgentMgr::GetDistance(monitor.last_x, monitor.last_y, currentX, currentY);
    if (moved < minimumProgress) {
        ++monitor.low_movement_count;
        if (monitor.low_movement_count == recoveryThreshold) {
            const float offsetX = static_cast<float>(static_cast<int>(randomSeed % 600u) - 300);
            const float offsetY = static_cast<float>(static_cast<int>((randomSeed / 7u) % 600u) - 300);
            resolution.issue_recovery_move = true;
            // Recover by sidestepping from the current position instead of
            // orbiting the blocked target point.
            resolution.recovery_x = currentX + (offsetX / 300.0f) * recoveryRadius;
            resolution.recovery_y = currentY + (offsetY / 300.0f) * recoveryRadius;
        } else if (monitor.low_movement_count >= abortThreshold) {
            resolution.abort_move = true;
        }
    } else {
        monitor.low_movement_count = 0;
    }

    monitor.last_x = currentX;
    monitor.last_y = currentY;
    return resolution;
}

MoveToResult MoveToAndWait(
    float x,
    float y,
    float threshold,
    uint32_t timeoutMs,
    uint32_t reissueMs,
    uint32_t expectedMapId) {
    return MoveToAndWait(
        x,
        y,
        threshold,
        timeoutMs,
        reissueMs,
        expectedMapId,
        &IssueMoveDirect);
}

MoveToResult MoveToAndWait(
    float x,
    float y,
    float threshold,
    uint32_t timeoutMs,
    uint32_t reissueMs,
    uint32_t expectedMapId,
    MoveIssuerFn moveIssuer) {
    MoveToResult result;
    moveIssuer = ResolveMoveIssuer(moveIssuer);

    moveIssuer(x, y);
    const DWORD start = GetTickCount();
    DWORD lastMove = start;
    auto* me = AgentMgr::GetMyAgent();
    auto stuckMonitor = MakeStuckMonitor(
        me ? me->x : 0.0f,
        me ? me->y : 0.0f);

    while ((GetTickCount() - start) < timeoutMs) {
        const uint32_t currentMapId = MapMgr::GetMapId();
        if (expectedMapId != 0u && currentMapId != expectedMapId) {
            result.map_changed = true;
            return result;
        }
        if (MapMgr::GetLoadingState() != 1u) {
            Sleep(100);
            continue;
        }

        me = AgentMgr::GetMyAgent();
        if (me && me->hp > 0.0f && IsWithinDistance(me->x, me->y, x, y, threshold)) {
            result.arrived = true;
            return result;
        }

        const DWORD now = GetTickCount();
        if (me && me->hp > 0.0f) {
            const auto stuckResolution = EvaluateStuckMonitor(
                me->x,
                me->y,
                x,
                y,
                stuckMonitor,
                now);
            if (stuckResolution.issue_recovery_move) {
                moveIssuer(stuckResolution.recovery_x, stuckResolution.recovery_y);
                lastMove = now;
                Sleep(250);
                continue;
            }
            if (stuckResolution.abort_move) {
                result.timed_out = true;
                return result;
            }
        }

        if (now - lastMove >= reissueMs) {
            moveIssuer(x, y);
            lastMove = now;
        }
        Sleep(250);
    }

    result.timed_out = true;
    return result;
}

void MoveToPoint(float x, float y, float threshold) {
    (void)MoveToAndWait(x, y, threshold);
}

bool MoveToAndWaitLogged(
    float x,
    float y,
    float threshold,
    const LoggedMoveOptions& options) {
    if (options.is_dead != nullptr && options.is_dead()) {
        auto* me = AgentMgr::GetMyAgent();
        Log::Warn("%s: MoveToAndWait abort dead target=(%.0f, %.0f) threshold=%.0f map=%u loaded=%d hp=%.3f",
                  options.log_prefix ? options.log_prefix : "Dungeon",
                  x,
                  y,
                  threshold,
                  MapMgr::GetMapId(),
                  MapMgr::GetIsMapLoaded() ? 1 : 0,
                  me ? me->hp : 0.0f);
        return false;
    }

    const auto result = MoveToAndWait(
        x,
        y,
        threshold,
        options.timeout_ms,
        options.poll_ms);
    if (!result.arrived) {
        Log::Warn("%s: MoveToAndWait timeout target=(%.0f, %.0f) dist=%.0f threshold=%.0f map=%u loaded=%d",
                  options.log_prefix ? options.log_prefix : "Dungeon",
                  x,
                  y,
                  DungeonCombat::DistanceToPoint(x, y),
                  threshold,
                  MapMgr::GetMapId(),
                  MapMgr::GetIsMapLoaded() ? 1 : 0);
    }
    return result.arrived;
}

void MoveRouteWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    WaypointMoveFn moveToPoint,
    WaypointMoveFn aggroMoveToPoint,
    IsMapLoadedFn isMapLoaded,
    float moveThreshold) {
    if (moveToPoint == nullptr || aggroMoveToPoint == nullptr || isMapLoaded == nullptr) {
        return;
    }

    if (waypoint.fight_range > 0.0f && isMapLoaded()) {
        aggroMoveToPoint(waypoint.x, waypoint.y, waypoint.fight_range);
        return;
    }

    moveToPoint(waypoint.x, waypoint.y, moveThreshold);
}

void LogWaypointState(
    const char* stage,
    const DungeonRoute::Waypoint* waypoints,
    int count,
    int waypointIndex,
    const WaypointTelemetryOptions& options) {
    if (waypoints == nullptr || count <= 0 || waypointIndex < 0 || waypointIndex >= count) {
        Log::Info("%s: %s waypoint state unavailable index=%d count=%d",
                  options.log_prefix ? options.log_prefix : "Dungeon",
                  stage ? stage : "unknown",
                  waypointIndex,
                  count);
        return;
    }

    auto* me = AgentMgr::GetMyAgent();
    const float myX = me ? me->x : 0.0f;
    const float myY = me ? me->y : 0.0f;
    const float hp = me ? me->hp : 0.0f;
    const float distToWaypoint = me
        ? AgentMgr::GetDistance(myX, myY, waypoints[waypointIndex].x, waypoints[waypointIndex].y)
        : -1.0f;
    const int nearest = me
        ? DungeonRoute::FindNearestWaypointIndex(waypoints, count, myX, myY)
        : 0;
    const float nearestEnemy = me
        ? DungeonCombat::GetNearestLivingEnemyDistance(options.nearest_enemy_range)
        : -1.0f;
    const uint32_t nearbyEnemies = me
        ? DungeonCombat::CountLivingEnemiesInRange(options.nearby_enemy_range)
        : 0u;
    Log::Info("%s: %s %s wp=%d(%s) map=%u loaded=%d alive=%d hp=%.3f pos=(%.0f, %.0f) distToWp=%.0f nearest=%d target=%u nearestEnemy=%.0f nearbyEnemies=%u",
              options.log_prefix ? options.log_prefix : "Dungeon",
              options.route_name ? options.route_name : "route",
              stage ? stage : "unknown",
              waypointIndex,
              waypoints[waypointIndex].label ? waypoints[waypointIndex].label : "",
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              me && me->hp > 0.0f ? 1 : 0,
              hp,
              myX,
              myY,
              distToWaypoint,
              nearest,
              AgentMgr::GetTargetId(),
              nearestEnemy,
              nearbyEnemies);
}

void MoveRouteWaypointWithCombatLoot(
    const DungeonRoute::Waypoint& waypoint,
    int waypointIndex,
    RouteWaypointMoveFn moveRouteWaypoint,
    IsMapLoadedFn isMapLoaded,
    LootAfterCombatFn lootAfterCombat,
    const char* logPrefix) {
    if (moveRouteWaypoint == nullptr) {
        return;
    }
    if (waypoint.fight_range <= 0.0f || isMapLoaded == nullptr || !isMapLoaded()) {
        moveRouteWaypoint(waypoint);
        return;
    }

    moveRouteWaypoint(waypoint);
    if (lootAfterCombat == nullptr) {
        return;
    }
    const int picked = lootAfterCombat(
        waypoint.fight_range,
        waypoint.label ? waypoint.label : "waypoint");
    Log::Info("%s: Post-pack loot sweep wp=%d(%s) range=%.0f picked=%d",
              logPrefix ? logPrefix : "Dungeon",
              waypointIndex,
              waypoint.label ? waypoint.label : "",
              DungeonLoot::ComputePostCombatLootRange(waypoint.fight_range),
              picked);
}

bool HandleBlessingWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    RouteWaypointMoveFn moveRouteWaypoint,
    BlessingGrabFn grabBlessing) {
    if (moveRouteWaypoint == nullptr || grabBlessing == nullptr) {
        return false;
    }
    moveRouteWaypoint(waypoint);
    grabBlessing(waypoint.x, waypoint.y);
    return true;
}

bool HandleOpenDungeonDoorWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    WaypointMoveFn aggroMoveToPoint,
    DoorOpenAtFn openDoorAt) {
    if (aggroMoveToPoint == nullptr || openDoorAt == nullptr) {
        return false;
    }
    aggroMoveToPoint(waypoint.x, waypoint.y, waypoint.fight_range);
    return openDoorAt(waypoint.x, waypoint.y);
}

bool WaitForLocalPositionSettle(
    uint32_t timeoutMs,
    float maxDeltaPerSample,
    uint32_t sampleMs,
    int settledSamplesRequired) {
    float lastX = 0.0f;
    float lastY = 0.0f;
    bool haveLast = false;
    int settledSamples = 0;
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = AgentMgr::GetMyAgent();
        if (!me) {
            Sleep(sampleMs);
            continue;
        }

        const float x = me->x;
        const float y = me->y;
        if (haveLast) {
            const float delta = AgentMgr::GetDistance(lastX, lastY, x, y);
            if (delta <= maxDeltaPerSample) {
                if (++settledSamples >= settledSamplesRequired) {
                    return true;
                }
            } else {
                settledSamples = 0;
            }
        }
        lastX = x;
        lastY = y;
        haveLast = true;
        Sleep(sampleMs);
    }
    return false;
}

MoveToResult MoveToAgent(
    uint32_t agentId,
    float threshold,
    uint32_t timeoutMs,
    uint32_t reissueMs,
    uint32_t expectedMapId,
    MoveIssuerFn moveIssuer) {
    auto* agent = AgentMgr::GetAgentByID(agentId);
    if (agent == nullptr) {
        MoveToResult result;
        result.timed_out = true;
        return result;
    }

    return MoveToAndWait(
        agent->x,
        agent->y,
        threshold,
        timeoutMs,
        reissueMs,
        expectedMapId,
        ResolveMoveIssuer(moveIssuer));
}

AgentApproachResult MoveToNearestResolvedAgent(
    float anchorX,
    float anchorY,
    float anchorThreshold,
    float searchRadius,
    float agentThreshold,
    AgentResolverFn agentResolver,
    MoveIssuerFn moveIssuer,
    uint32_t timeoutMs,
    uint32_t reissueMs,
    uint32_t expectedMapId) {
    AgentApproachResult result;
    result.anchor_move = MoveToAndWait(
        anchorX,
        anchorY,
        anchorThreshold,
        timeoutMs,
        reissueMs,
        expectedMapId,
        ResolveMoveIssuer(moveIssuer));
    if (agentResolver == nullptr) {
        return result;
    }

    result.agent_id = agentResolver(anchorX, anchorY, searchRadius);
    result.agent_found = (result.agent_id != 0u);
    if (!result.agent_found) {
        return result;
    }

    result.agent_move = MoveToAgent(
        result.agent_id,
        agentThreshold,
        timeoutMs,
        reissueMs,
        expectedMapId,
        ResolveMoveIssuer(moveIssuer));
    return result;
}

RouteFollowResult FollowWaypoints(
    const DungeonRoute::Waypoint* waypoints,
    int count,
    uint32_t mapId,
    const RouteFollowOptions& options,
    MoveIssuerFn moveIssuer) {
    RouteFollowResult result;
    if (waypoints == nullptr || count <= 0) {
        result.failed_index = 0;
        return result;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (me == nullptr) {
        result.failed_index = 0;
        return result;
    }

    int i = DungeonRoute::FindNearestWaypointIndex(waypoints, count, me->x, me->y);
    int retriesUsed = 0;
    while (i < count) {
        float tolerance = options.default_tolerance;
        if (options.use_waypoint_fight_range_as_tolerance && waypoints[i].fight_range > 0.0f) {
            tolerance = waypoints[i].fight_range;
        }

        const auto moveResult = MoveToAndWait(
            waypoints[i].x,
            waypoints[i].y,
            tolerance,
            options.waypoint_timeout_ms,
            options.reissue_ms,
            mapId,
            ResolveMoveIssuer(moveIssuer));
        if (moveResult.arrived) {
            ++i;
            continue;
        }

        if (moveResult.map_changed) {
            result.map_changed = true;
            result.retries_used = retriesUsed;
            return result;
        }

        if (retriesUsed >= options.max_backtrack_retries) {
            result.failed_index = i;
            result.retries_used = retriesUsed;
            return result;
        }

        me = AgentMgr::GetMyAgent();
        int nearestIndex = i;
        if (me != nullptr) {
            nearestIndex = DungeonRoute::FindNearestWaypointIndex(waypoints, count, me->x, me->y);
        }
        if (nearestIndex < i) {
            // Preserve confirmed forward progress: route retries should backtrack
            // from the failed waypoint, not from whichever earlier waypoint the
            // player drifted closest to during combat or knockback.
            nearestIndex = i;
        }

        int backtrackIndex = DungeonRoute::ComputeStuckBacktrackIndex(
            nearestIndex,
            options.backtrack_count);
        if (backtrackIndex >= i && i > 0) {
            backtrackIndex = i - 1;
        }

        i = backtrackIndex;
        ++retriesUsed;
    }

    result.completed = true;
    result.retries_used = retriesUsed;
    return result;
}

int GetNearestWaypointIndex(const DungeonRoute::Waypoint* waypoints, int count) {
    auto* me = AgentMgr::GetMyAgent();
    if (me == nullptr) {
        return 0;
    }
    return DungeonRoute::FindNearestWaypointIndex(waypoints, count, me->x, me->y);
}

bool WaitForMapId(uint32_t targetMapId, uint32_t timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() == targetMapId && MapMgr::GetLoadingState() == 1u) {
            return true;
        }
        Sleep(250);
    }
    return false;
}

// ===== Aggro-move support =====

bool IssueAggroMove(
    AggroMoveState& state,
    float x,
    float y,
    bool sparkflyMap,
    bool force,
    uint32_t movingReissueMs,
    uint32_t reissueMs,
    float randomRadius) {
    auto* me = AgentMgr::GetMyAgent();
    const bool isMoving = me && (me->move_x != 0.0f || me->move_y != 0.0f);
    const DWORD now = GetTickCount();
    if (!force && isMoving &&
        (now - state.lastMoveIssuedAt) < movingReissueMs) {
        return false;
    }
    if (!force && (now - state.lastMoveIssuedAt) < reissueMs) {
        return false;
    }
    if (sparkflyMap) {
        state.moveTargetX = x;
        state.moveTargetY = y;
    } else if (force || !state.moveTargetInitialized) {
        state.moveTargetX = RandomizedCoordinate(x, randomRadius);
        state.moveTargetY = RandomizedCoordinate(y, randomRadius);
    }
    state.moveTargetInitialized = true;
    AgentMgr::Move(state.moveTargetX, state.moveTargetY);
    state.lastMoveIssuedAt = now;
    return true;
}

bool ShouldContinueLocalClearCooldown(
    AggroMoveState& state,
    float cooldownDistance) {
    if (!state.localClearCooldownActive) {
        return false;
    }

    const DWORD now = GetTickCount();
    auto* me = AgentMgr::GetMyAgent();
    const float movedSinceCooldown = me
        ? AgentMgr::GetDistance(state.localClearCooldownX, state.localClearCooldownY, me->x, me->y)
        : 0.0f;
    if (now < state.localClearCooldownUntil &&
        movedSinceCooldown < cooldownDistance) {
        return true;
    }

    state.localClearCooldownActive = false;
    return false;
}

void ArmLocalClearCooldown(
    AggroMoveState& state,
    float fallbackX,
    float fallbackY,
    uint32_t cooldownMs) {
    auto* me = AgentMgr::GetMyAgent();
    state.localClearCooldownX = me ? me->x : fallbackX;
    state.localClearCooldownY = me ? me->y : fallbackY;
    state.localClearCooldownUntil = GetTickCount() + cooldownMs;
    state.localClearCooldownActive = true;
}

void HandleBlockedMoveProgress(
    AggroMoveState& state,
    float x,
    float y,
    float oldX,
    float oldY,
    bool sparkflyMap,
    uint32_t sidestepWaitMs,
    float blockedProgressDistance,
    int sidestepModulus,
    int sidestepHalfRange) {
    auto* meAfterMove = AgentMgr::GetMyAgent();
    if (!meAfterMove) {
        return;
    }

    const float moved = AgentMgr::GetDistance(oldX, oldY, meAfterMove->x, meAfterMove->y);
    const bool isMoving = meAfterMove->move_x != 0.0f || meAfterMove->move_y != 0.0f;
    if (isMoving || moved >= blockedProgressDistance) {
        state.blockedCount = 0;
        return;
    }

    ++state.blockedCount;
    const int sidestepOffsetX =
        static_cast<int>(GetTickCount() % static_cast<DWORD>(sidestepModulus)) -
        sidestepHalfRange;
    const int sidestepOffsetY =
        static_cast<int>((GetTickCount() / 7u) %
                         static_cast<DWORD>(sidestepModulus)) -
        sidestepHalfRange;
    const float sidestepX = meAfterMove->x + static_cast<float>(sidestepOffsetX);
    const float sidestepY = meAfterMove->y + static_cast<float>(sidestepOffsetY);
    AgentMgr::Move(sidestepX, sidestepY);
    Sleep(sidestepWaitMs);
    state.moveTargetInitialized = false;
    IssueAggroMove(state, x, y, sparkflyMap, true);
}

namespace {

bool CallBool(BoolFn fn, bool fallback) {
    return fn ? fn() : fallback;
}

void CallWait(WaitFn fn, uint32_t ms) {
    if (fn) {
        fn(ms);
    } else {
        Sleep(ms);
    }
}

bool IsAggroMoveWorldReady(const AggroMoveCallbacks& callbacks) {
    return !CallBool(callbacks.is_dead, false) &&
           CallBool(callbacks.is_map_loaded, MapMgr::GetIsMapLoaded());
}

void AggroMoveToOpportunistic(
    float x,
    float y,
    float fightRange,
    const AggroMoveCallbacks& callbacks,
    const AggroMoveOptions& options) {
    DWORD start = GetTickCount();
    int blockedCount = 0;
    float moveTargetX = x;
    float moveTargetY = y;

    auto issueMove = [&]() {
        moveTargetX = RandomizedCoordinate(x, options.move_random_radius);
        moveTargetY = RandomizedCoordinate(y, options.move_random_radius);
        AgentMgr::Move(moveTargetX, moveTargetY);
    };
    auto issueSidestep = [&]() {
        auto* me = AgentMgr::GetMyAgent();
        if (!me) return;
        AgentMgr::Move(RandomizedCoordinate(me->x, options.sidestep_random_radius),
                       RandomizedCoordinate(me->y, options.sidestep_random_radius));
    };

    if (DungeonCombat::CanMoveWithEnemyRangeGate(fightRange, DungeonCombat::LONG_BOW_RANGE)) {
        issueMove();
    }

    while (DungeonCombat::DistanceToPoint(x, y) > options.arrival_threshold &&
           (GetTickCount() - start) < options.move_budget_ms) {
        if (!IsAggroMoveWorldReady(callbacks)) {
            return;
        }

        auto* meBefore = AgentMgr::GetMyAgent();
        const float oldX = meBefore ? meBefore->x : 0.0f;
        const float oldY = meBefore ? meBefore->y : 0.0f;

        if (DungeonCombat::GetNearestLivingEnemyDistance() < fightRange &&
            callbacks.fight_in_aggro != nullptr) {
            callbacks.fight_in_aggro(
                fightRange,
                false,
                nullptr,
                true,
                options.opportunistic_fight_budget_ms);
        }

        if (DungeonCombat::CanMoveWithEnemyRangeGate(fightRange, DungeonCombat::LONG_BOW_RANGE) ||
            (GetTickCount() - start) > options.force_move_after_ms) {
            issueMove();
            if (callbacks.pickup_nearby_loot != nullptr) {
                (void)callbacks.pickup_nearby_loot(options.opportunistic_loot_radius);
                if (!IsAggroMoveWorldReady(callbacks)) {
                    return;
                }
            }
            CallWait(callbacks.wait_ms, options.loop_poll_ms);
            if (!IsAggroMoveWorldReady(callbacks)) {
                return;
            }

            auto* meAfter = AgentMgr::GetMyAgent();
            if (meAfter && meAfter->x == oldX && meAfter->y == oldY) {
                ++blockedCount;
                issueSidestep();
                CallWait(callbacks.wait_ms, options.blocked_sidestep_wait_ms);
                issueMove();
            } else {
                blockedCount = 0;
            }
        }

        if (blockedCount > options.blocked_limit) {
            Log::Warn("%s: AggroMove opportunistic blocked limit reached (%d) target=(%.0f, %.0f) remaining=%.0f",
                      options.log_prefix ? options.log_prefix : "Dungeon",
                      blockedCount,
                      x,
                      y,
                      DungeonCombat::DistanceToPoint(x, y));
            return;
        }

        CallWait(callbacks.wait_ms, options.loop_poll_ms);
    }

    Log::Info("%s: AggroMove opportunistic end target=(%.0f, %.0f) remaining=%.0f threshold=%.0f",
              options.log_prefix ? options.log_prefix : "Dungeon",
              x,
              y,
              DungeonCombat::DistanceToPoint(x, y),
              options.arrival_threshold);
}

void AggroMoveToStandard(
    float x,
    float y,
    float fightRange,
    const AggroMoveCallbacks& callbacks,
    const AggroMoveOptions& options) {
    const float localClearRange = DungeonCombat::ComputeLocalClearRange(fightRange);
    AggroMoveState moveState;
    moveState.moveTargetX = x;
    moveState.moveTargetY = y;
    if (DungeonCombat::CanMoveWithEnemyRangeGate(fightRange, DungeonCombat::LONG_BOW_RANGE)) {
        IssueAggroMove(moveState, x, y, options.exact_move_target, true);
    }
    DWORD start = GetTickCount();
    while (DungeonCombat::DistanceToPoint(x, y) > options.arrival_threshold &&
           (GetTickCount() - start) < options.move_budget_ms) {
        if (!IsAggroMoveWorldReady(callbacks)) {
            return;
        }

        auto* meLoop = AgentMgr::GetMyAgent();
        const float oldX = meLoop ? meLoop->x : 0.0f;
        const float oldY = meLoop ? meLoop->y : 0.0f;
        const float nearestDistance = DungeonCombat::GetNearestLivingEnemyDistance();

        if (nearestDistance < localClearRange) {
            if (options.use_local_clear_cooldown && ShouldContinueLocalClearCooldown(moveState)) {
                IssueAggroMove(moveState, x, y, options.exact_move_target, true);
                CallWait(callbacks.wait_ms, DungeonCombat::AGGRO_STANDARD_COOLDOWN_MOVE_DELAY_MS);
                continue;
            }

            moveState.blockedCount = 0;
            const uint32_t bestId = DungeonSkill::GetBestBalledEnemy(localClearRange);
            if (!bestId) {
                CallWait(callbacks.wait_ms, DungeonCombat::AGGRO_STANDARD_NO_BALL_DELAY_MS);
                continue;
            }

            if (callbacks.hold_special_local_clear != nullptr) {
                Log::Info("%s: AggroMove holding special local clear foe=%u waypoint=(%.0f, %.0f) dist=%.0f",
                          options.log_prefix ? options.log_prefix : "Dungeon",
                          bestId,
                          x,
                          y,
                          nearestDistance);
                callbacks.hold_special_local_clear(
                    x,
                    y,
                    fightRange,
                    bestId,
                    callbacks.special_stats);
            } else if (callbacks.hold_local_clear != nullptr) {
                Log::Info("%s: AggroMove holding local clear foe=%u waypoint=(%.0f, %.0f) dist=%.0f clearRange=%.0f",
                          options.log_prefix ? options.log_prefix : "Dungeon",
                          bestId,
                          x,
                          y,
                          nearestDistance,
                          localClearRange);
                callbacks.hold_local_clear("Route", x, y, fightRange, bestId, nullptr);
                if (options.use_local_clear_cooldown) {
                    ArmLocalClearCooldown(moveState, x, y);
                }
            }

            if (!IsAggroMoveWorldReady(callbacks)) {
                return;
            }
            WaitForLocalPositionSettle(
                DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_SETTLE_TIMEOUT_MS,
                DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_SETTLE_DISTANCE);
            if (!IsAggroMoveWorldReady(callbacks)) {
                return;
            }
            IssueAggroMove(moveState, x, y, options.exact_move_target, true);
            CallWait(callbacks.wait_ms, DungeonCombat::AGGRO_STANDARD_LOCAL_CLEAR_RESUME_DELAY_MS);
            if (!IsAggroMoveWorldReady(callbacks)) {
                return;
            }
            continue;
        }

        if (DungeonCombat::CanMoveWithEnemyRangeGate(fightRange, DungeonCombat::LONG_BOW_RANGE) ||
            (GetTickCount() - start) > options.force_move_after_ms) {
            IssueAggroMove(
                moveState,
                x,
                y,
                options.exact_move_target,
                (GetTickCount() - start) > options.force_move_after_ms);
            CallWait(callbacks.wait_ms, options.loop_poll_ms);
            if (!IsAggroMoveWorldReady(callbacks)) {
                return;
            }
            HandleBlockedMoveProgress(
                moveState,
                x,
                y,
                oldX,
                oldY,
                options.exact_move_target);
        }

        if (moveState.blockedCount > options.blocked_limit) {
            Log::Warn("%s: AggroMove blocked limit reached (%d) target=(%.0f, %.0f) remaining=%.0f",
                      options.log_prefix ? options.log_prefix : "Dungeon",
                      moveState.blockedCount,
                      x,
                      y,
                      DungeonCombat::DistanceToPoint(x, y));
            return;
        }
        CallWait(callbacks.wait_ms, options.loop_poll_ms);
    }
    Log::Info("%s: AggroMove end target=(%.0f, %.0f) remaining=%.0f threshold=%.0f",
              options.log_prefix ? options.log_prefix : "Dungeon",
              x,
              y,
              DungeonCombat::DistanceToPoint(x, y),
              options.arrival_threshold);
}

} // namespace

void AggroMoveTo(
    float x,
    float y,
    float fightRange,
    const AggroMoveCallbacks& callbacks,
    const AggroMoveOptions& options) {
    if (options.profile == AggroMoveProfile::Opportunistic) {
        AggroMoveToOpportunistic(x, y, fightRange, callbacks, options);
        return;
    }
    AggroMoveToStandard(x, y, fightRange, callbacks, options);
}

} // namespace GWA3::DungeonNavigation
