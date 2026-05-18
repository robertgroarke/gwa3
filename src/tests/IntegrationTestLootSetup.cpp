// Shared integration-test helper for forcing nearby loot opportunities.

#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include "IntegrationTestInternal.h"

#include <Windows.h>

namespace GWA3::SmokeTest {

static size_t CollectNearbyFoeAgents(float maxDistance, uint32_t* outIds, size_t capacity) {
    if (!outIds || capacity == 0) return 0;

    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return 0;

    if (Offsets::AgentBase <= 0x10000) return 0;

    struct Candidate {
        uint32_t id;
        float distSq;
    };

    Candidate best[8] = {};
    size_t bestCount = 0;

    static bool s_loggedAgentScan = false;
    uint32_t totalAgents = 0, livingCount = 0, foeCount = 0;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr <= 0x10000 || maxAgents == 0) return 0;

        const float maxDistSq = maxDistance * maxDistance;

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            if (i == myId) continue;

            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;
            totalAgents++;

            auto* base = reinterpret_cast<Agent*>(agentPtr);
            if (base->type != 0xDB) continue;
            livingCount++;

            auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
            if (living->allegiance != 3) continue;
            if (living->hp <= 0.0f) continue;
            foeCount++;

            const float distSq = AgentMgr::GetSquaredDistance(myX, myY, living->x, living->y);
            if (distSq > maxDistSq) continue;

            const size_t limit = (capacity < 8) ? capacity : 8;
            size_t insertAt = bestCount;
            if (bestCount < limit) {
                best[bestCount++] = {i, distSq};
            } else if (distSq >= best[bestCount - 1].distSq) {
                continue;
            } else {
                insertAt = bestCount - 1;
                best[insertAt] = {i, distSq};
            }

            while (insertAt > 0 && best[insertAt].distSq < best[insertAt - 1].distSq) {
                Candidate tmp = best[insertAt - 1];
                best[insertAt - 1] = best[insertAt];
                best[insertAt] = tmp;
                --insertAt;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }

    if (!s_loggedAgentScan) {
        Log::Info("[INTG] Agent scan: total=%u living=%u foes=%u nearby=%u (maxAgents=%u)",
                  totalAgents, livingCount, foeCount, static_cast<uint32_t>(bestCount),
                  *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8));
        s_loggedAgentScan = true;
    }

    for (size_t i = 0; i < bestCount; ++i) {
        outIds[i] = best[i].id;
    }
    return bestCount;
}

static bool FindNearestFoeAgent(float maxDistance, uint32_t& outId, float& outX, float& outY, float& outDistance) {
    outId = 0;
    outX = 0.0f;
    outY = 0.0f;
    outDistance = 0.0f;

    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return false;
    if (Offsets::AgentBase <= 0x10000) return false;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr <= 0x10000 || maxAgents == 0) return false;

        const float maxDistSq = maxDistance * maxDistance;
        float bestDistSq = maxDistSq;

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            if (i == myId) continue;

            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;

            auto* base = reinterpret_cast<Agent*>(agentPtr);
            if (base->type != 0xDB) continue;

            auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
            if (living->allegiance != 3) continue;
            if (living->hp <= 0.0f) continue;

            const float distSq = AgentMgr::GetSquaredDistance(myX, myY, living->x, living->y);
            if (distSq >= bestDistSq) continue;

            bestDistSq = distSq;
            outId = i;
            outX = living->x;
            outY = living->y;
        }

        if (outId == 0) return false;
        outDistance = sqrtf(bestDistSq);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool EngageFoeForLootDrop(
    uint32_t foeId,
    int& foesDefeated,
    int& foesDamaged,
    const std::function<bool()>& waitForDropAfterCombat) {
    AgentLiving* foe = GetAgentLivingRaw(foeId);
    if (!foe || foe->hp <= 0.0f) return false;

    const float hpBefore = foe->hp;
    IntReport("  Engaging nearby foe %u to create a loot drop opportunity (hp=%.2f)...", foeId, hpBefore);

    SkillTestCandidate offensiveSkill{};
    const bool haveOffensiveSkill = TryChooseOffensiveSkillCandidate(foeId, offensiveSkill);
    if (haveOffensiveSkill) {
        IntReport("  Offensive skill available for loot setup: slot %u skill %u", offensiveSkill.slot, offensiveSkill.skillId);
    }

    bool foeDefeated = false;
    bool foeDamaged = false;
    const DWORD fightStart = GetTickCount();
    while ((GetTickCount() - fightStart) < 30000) {
        foe = GetAgentLivingRaw(foeId);
        if (!foe || foe->hp <= 0.0f) {
            foeDefeated = true;
            ++foesDefeated;
            break;
        }

        float px = 0.0f;
        float py = 0.0f;
        if (TryReadAgentPosition(ReadMyId(), px, py)) {
            const float dist = AgentMgr::GetDistance(px, py, foe->x, foe->y);
            if (dist > 250.0f) {
                float fx = foe->x, fy = foe->y;
                GameThread::EnqueuePost([fx, fy]() { AgentMgr::Move(fx, fy); });
                Sleep(dist > 1200.0f ? 750 : 350);
            }
        }

        AgentMgr::ChangeTarget(foeId);

        // Keep loot combat stable: combat-time hero flagging can
        // crash Sparkfly in this client build.

        if (haveOffensiveSkill) {
            Skillbar* liveBar = SkillMgr::GetPlayerSkillbar();
            if (liveBar && liveBar->skills[offensiveSkill.slot - 1].recharge == 0) {
                SkillMgr::UseSkill(offensiveSkill.slot, foeId, 0);
            }
        }
        Sleep(1500);

        foe = GetAgentLivingRaw(foeId);
        if (foe) {
            if (foe->hp < hpBefore && foe->hp > 0.0f) {
                IntReport("  Foe %u HP: %.2f (taking damage)", foeId, foe->hp);
            }
        }

        if (FindNearbyGroundItem(5000.0f)) return true;

        foe = GetAgentLivingRaw(foeId);
        if (foe && foe->hp < hpBefore) {
            foeDamaged = true;
        }
        if (!foe || foe->hp <= 0.0f) {
            foeDefeated = true;
            ++foesDefeated;
            break;
        }
    }

    AgentMgr::CancelAction();
    Sleep(250);

    if (foeDamaged) {
        ++foesDamaged;
    } else {
        IntReport("  Loot setup combat did not reduce foe %u HP", foeId);
    }

    if (foeDefeated && waitForDropAfterCombat()) {
        return true;
    }

    return FindNearbyGroundItem(5000.0f) != nullptr;
}

static bool EngageNearbyFoesForLootDrop(
    const uint32_t* foeIds,
    size_t foeCount,
    int& foesDefeated,
    int& foesDamaged,
    const std::function<bool()>& waitForDropAfterCombat) {
    IntReport("  Found %u nearby foe(s) for loot setup", static_cast<unsigned>(foeCount));
    for (size_t i = 0; i < foeCount; ++i) {
        if (EngageFoeForLootDrop(foeIds[i], foesDefeated, foesDamaged, waitForDropAfterCombat)) {
            return true;
        }
        if (foesDefeated >= 8) break;
    }
    return FindNearbyGroundItem(5000.0f) != nullptr;
}

bool TryForceNearbyLootDrop() {
    const struct SparkflyProbeStep {
        float x;
        float y;
    } kProbeSteps[] = {
        {-4559.0f, -14406.0f},
        {-5204.0f,  -9831.0f},
        {-7520.0f, -14166.0f},
        { -928.0f,  -8699.0f},
        {-6374.0f, -13639.0f},
    };

    auto waitForDropAfterCombat = []() {
        return WaitFor("ground item appears after nearby combat", 5000, []() {
            return FindNearbyGroundItem(5000.0f) != nullptr;
        });
    };

    int foesDefeated = 0;
    int foesDamaged = 0;

    for (const auto& step : kProbeSteps) {
        if (FindNearbyGroundItem(5000.0f)) return true;

        IntReport("  Probing Sparkfly combat route at (%.0f, %.0f)...", step.x, step.y);
        MovePlayerNear(step.x, step.y, 350.0f, 15000);
        Sleep(1000);

        uint32_t foeIds[8] = {};
        const size_t foeCount = CollectNearbyFoeAgents(5000.0f, foeIds, 8);
        if (foeCount == 0) {
            uint32_t nearestFoeId = 0;
            float nearestFoeX = 0.0f;
            float nearestFoeY = 0.0f;
            float nearestFoeDist = 0.0f;
            if (FindNearestFoeAgent(25000.0f, nearestFoeId, nearestFoeX, nearestFoeY, nearestFoeDist)) {
                IntReport("  No nearby foes at probe point; moving toward nearest foe %u at (%.0f, %.0f), dist=%.0f...",
                          nearestFoeId, nearestFoeX, nearestFoeY, nearestFoeDist);
                MovePlayerNear(nearestFoeX, nearestFoeY, 1200.0f, 25000);
                Sleep(1000);
                const size_t retriedFoeCount = CollectNearbyFoeAgents(5000.0f, foeIds, 8);
                if (retriedFoeCount == 0) continue;
                for (size_t i = retriedFoeCount; i < 8; ++i) foeIds[i] = 0;
                IntReport("  Found %u nearby foe(s) after nearest-foe chase", static_cast<unsigned>(retriedFoeCount));
                if (EngageNearbyFoesForLootDrop(foeIds, retriedFoeCount, foesDefeated, foesDamaged, waitForDropAfterCombat)) {
                    return true;
                }
                if (foesDefeated >= 8) break;
                continue;
            }
            continue;
        }

        if (EngageNearbyFoesForLootDrop(foeIds, foeCount, foesDefeated, foesDamaged, waitForDropAfterCombat)) {
            return true;
        }
        if (foesDefeated >= 8) break;
    }

    IntReport("  Loot setup summary: foesDamaged=%d foesDefeated=%d",
              foesDamaged,
              foesDefeated);
    return FindNearbyGroundItem(5000.0f) != nullptr;
}

// Gameplay coverage lives in IntegrationTestGameplay.cpp.

// Session and trader coverage lives in IntegrationTestSession.cpp.

// World-action and session-state coverage lives in IntegrationTestWorld.cpp.

// System, state, offset, and patch coverage lives in IntegrationTestSystems.cpp and IntegrationTestOffsets.cpp.
// Dungeon feature runners live in their own IntegrationTest*.cpp files.
} // namespace GWA3::SmokeTest
