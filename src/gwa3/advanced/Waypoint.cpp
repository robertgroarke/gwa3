#include <gwa3/advanced/Waypoint.h>

#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <Windows.h>
#include <cstdlib>

namespace GWA3::AdvancedWaypoint {

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

float DistanceToPoint(float x, float y) {
    auto* me = AgentMgr::GetMyAgent();
    if (me == nullptr) {
        return 999999.0f;
    }
    return AgentMgr::GetDistance(me->x, me->y, x, y);
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
                  options.log_prefix ? options.log_prefix : "Waypoint",
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
                  options.log_prefix ? options.log_prefix : "Waypoint",
                  x,
                  y,
                  DistanceToPoint(x, y),
                  threshold,
                  MapMgr::GetMapId(),
                  MapMgr::GetIsMapLoaded() ? 1 : 0);
    }
    return result.arrived;
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

} // namespace GWA3::AdvancedWaypoint
