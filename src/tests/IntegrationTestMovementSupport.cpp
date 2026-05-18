#include "IntegrationTestInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>

#include <Windows.h>
#include <cmath>
#include <cstdint>

namespace GWA3::SmokeTest {

bool MovePlayerNear(float targetX, float targetY, float threshold, int timeoutMs) {
    // Stuck-aware movement with lateral avoidance for outpost NPC blocking.
    // Detects when the character hasn't made progress, scans for blocking NPCs
    // in the forward path, and sidesteps laterally to navigate around them.
    constexpr float kStuckThreshold   = 25.0f;   // moved less than this = stuck
    constexpr int   kStuckCountLimit  = 4;        // consecutive stuck checks before repath
    constexpr float kLateralOffsetBase = 350.0f;  // base sidestep distance
    constexpr float kBacktrackUnits   = 250.0f;   // how far to back away from target
    constexpr float kBlockingAgentRange = 200.0f; // scan radius for blocking NPCs

    const DWORD start = GetTickCount();
    int stuckCount = 0;
    int stuckRetries = 0;                          // how many times we've unstuck
    float prevX = 0.0f, prevY = 0.0f;
    bool havePrev = false;
    int lateralSign = 1; // alternate left/right sidesteps
    bool loggedFirstIssue = false;
    DWORD lastProgressLog = 0;
    int readFailureCount = 0;

    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        float myX = 0.0f, myY = 0.0f;
        if (!TryReadAgentPosition(ReadMyId(), myX, myY)) {
            ++readFailureCount;
            if ((readFailureCount % 5) == 1) {
                Log::Info("[MOVE] MovePlayerNear could not read player position yet target=(%.0f, %.0f) readFailures=%d",
                          targetX, targetY, readFailureCount);
            }
            Sleep(500);
            continue;
        }
        readFailureCount = 0;

        // Check if arrived
        const float distToTarget = AgentMgr::GetDistance(myX, myY, targetX, targetY);
        if (distToTarget <= threshold) {
            Log::Info("[MOVE] MovePlayerNear reached target=(%.0f, %.0f) player=(%.0f, %.0f) dist=%.0f threshold=%.0f elapsed=%lu",
                      targetX, targetY, myX, myY, distToTarget, threshold,
                      static_cast<unsigned long>(GetTickCount() - start));
            return true;
        }

        // Stuck detection: compare with previous position
        float moved = 0.0f;
        if (havePrev) {
            moved = AgentMgr::GetDistance(prevX, prevY, myX, myY);
            if (moved < kStuckThreshold) {
                ++stuckCount;
            } else {
                stuckCount = 0;
            }
        }

        const DWORD elapsed = GetTickCount() - start;
        if (lastProgressLog == 0 || (elapsed - lastProgressLog) >= 2000) {
            Log::Info("[MOVE] MovePlayerNear progress player=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f moved=%.0f stuckCount=%d elapsed=%lu/%d",
                      myX, myY, targetX, targetY, distToTarget, moved, stuckCount,
                      static_cast<unsigned long>(elapsed), timeoutMs);
            lastProgressLog = elapsed;
        }

        if (stuckCount >= kStuckCountLimit) {
            // Direction vector from us to target (normalized)
            const float dx = targetX - myX;
            const float dy = targetY - myY;
            const float len = sqrtf(dx * dx + dy * dy);
            const float ndx = (len > 0.01f) ? dx / len : 1.0f;
            const float ndy = (len > 0.01f) ? dy / len : 0.0f;

            // Scan nearby agents to find closest NPC in our forward path
            const uint32_t maxAgents = AgentMgr::GetMaxAgents();
            for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
                if (i == ReadMyId()) continue;
                auto* agent = AgentMgr::GetAgentByID(i);
                if (!agent || agent->type != 0xDB) continue;
                auto* living = static_cast<AgentLiving*>(agent);
                if (living->allegiance != 6) continue; // NPC only
                const float agentDist = AgentMgr::GetDistance(myX, myY, living->x, living->y);
                if (agentDist < kBlockingAgentRange) {
                    const float adx = living->x - myX;
                    const float ady = living->y - myY;
                    const float alen = sqrtf(adx * adx + ady * ady);
                    if (alen > 0.01f) {
                        const float dot = (adx * ndx + ady * ndy) / alen;
                        if (dot > 0.3f) { // in front of us (within ~70 degree cone)
                            Log::Info("[MOVE] Stuck: blocker agent %u at (%.0f,%.0f) dist=%.0f",
                                      i, living->x, living->y, agentDist);
                            break;
                        }
                    }
                }
            }

            // Phase 1: backtrack — move AWAY from target first to escape
            // whatever geometry is blocking us. This gives the pathfinder
            // room to plan a new route instead of re-colliding with the
            // same wall/NPC.
            ++stuckRetries;
            const float backtrackX = myX - ndx * kBacktrackUnits;
            const float backtrackY = myY - ndy * kBacktrackUnits;
            Log::Info("[MOVE] MovePlayerNear STUCK retry=%d — backtrack player=(%.0f, %.0f) -> (%.0f, %.0f)",
                      stuckRetries, myX, myY, backtrackX, backtrackY);
            if (GameThread::IsInitialized()) {
                GameThread::EnqueuePost([backtrackX, backtrackY]() {
                    AgentMgr::Move(backtrackX, backtrackY);
                });
            }
            Sleep(1500);

            // Re-read position after backtrack so the sidestep is calculated
            // from the new spot (not the wall we were wedged against)
            float postBtX = myX, postBtY = myY;
            TryReadAgentPosition(ReadMyId(), postBtX, postBtY);

            // Phase 2: sidestep perpendicular, with lateral distance growing
            // each retry so we sweep outward if the first angle still blocks.
            const float jitter = 1.0f + 0.35f * static_cast<float>(stuckRetries - 1);
            const float lateralOffset = kLateralOffsetBase * jitter;
            const float perpX = -ndy * lateralSign;
            const float perpY =  ndx * lateralSign;
            const float waypointX = postBtX + perpX * lateralOffset;
            const float waypointY = postBtY + perpY * lateralOffset;

            Log::Info("[MOVE] MovePlayerNear sidestep post-backtrack=(%.0f, %.0f) waypoint=(%.0f, %.0f) "
                      "target=(%.0f, %.0f) side=%d jitter=%.2f",
                      postBtX, postBtY, waypointX, waypointY, targetX, targetY, lateralSign, jitter);

            if (GameThread::IsInitialized()) {
                GameThread::EnqueuePost([waypointX, waypointY]() {
                    AgentMgr::Move(waypointX, waypointY);
                });
            }
            Sleep(1500);

            lateralSign = -lateralSign;                 // alternate side next time
            stuckCount = 0;
            havePrev = false;
            continue;
        }

        // Normal forward movement — only reissue when stalled or first move
        if (GameThread::IsInitialized()) {
            if (!loggedFirstIssue) {
                Log::Info("[MOVE] MovePlayerNear issuing first move player=(%.0f, %.0f) target=(%.0f, %.0f) threshold=%.0f timeout=%d",
                          myX, myY, targetX, targetY, threshold, timeoutMs);
                loggedFirstIssue = true;
            }
            GameThread::EnqueuePost([targetX, targetY]() {
                Log::Info("[MOVE] MovePlayerNear post-dispatch callback target=(%.0f, %.0f)", targetX, targetY);
                AgentMgr::Move(targetX, targetY);
            });
        }

        prevX = myX;
        prevY = myY;
        havePrev = true;
        Sleep(500);
    }
    float finalX = 0.0f;
    float finalY = 0.0f;
    if (TryReadAgentPosition(ReadMyId(), finalX, finalY)) {
        Log::Info("[MOVE] MovePlayerNear timed out player=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f threshold=%.0f timeout=%d",
                  finalX, finalY, targetX, targetY,
                  AgentMgr::GetDistance(finalX, finalY, targetX, targetY),
                  threshold, timeoutMs);
    } else {
        Log::Info("[MOVE] MovePlayerNear timed out without final player position target=(%.0f, %.0f) threshold=%.0f timeout=%d",
                  targetX, targetY, threshold, timeoutMs);
    }
    return false;
}

} // namespace GWA3::SmokeTest