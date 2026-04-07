// Shared integration-test support helpers used by multiple extracted modules.

#include "IntegrationTestInternal.h"

#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>

#include <Windows.h>

namespace GWA3::SmokeTest {

bool WaitFor(const char* desc, int timeoutMs, const std::function<bool()>& predicate) {
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < (DWORD)timeoutMs) {
        if (predicate()) return true;
        Sleep(500);
    }
    IntReport("  Timeout waiting for: %s (%d ms)", desc, timeoutMs);
    return false;
}

uint32_t ReadMapId() {
    return MapMgr::GetMapId();
}

uint32_t ReadMyId() {
    if (Offsets::MyID < 0x10000) return 0;
    return *reinterpret_cast<uint32_t*>(Offsets::MyID);
}

bool TryReadAgentPosition(uint32_t agentId, float& x, float& y) {
    x = 0.0f;
    y = 0.0f;
    if (Offsets::AgentBase <= 0x10000 || agentId == 0 || agentId >= 5000) return false;

    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return false;

    uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + agentId * 4);
    if (agentPtr <= 0x10000) return false;

    x = *reinterpret_cast<float*>(agentPtr + 0x74);
    y = *reinterpret_cast<float*>(agentPtr + 0x78);
    return true;
}

AgentLiving* GetAgentLivingRaw(uint32_t agentId) {
    if (Offsets::AgentBase <= 0x10000 || agentId == 0 || agentId >= 5000) return nullptr;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        if (agentArr <= 0x10000) return nullptr;

        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + agentId * 4);
        if (agentPtr <= 0x10000) return nullptr;

        return reinterpret_cast<AgentLiving*>(agentPtr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

uint32_t GetPlayerTypeMap() {
    auto* me = GetAgentLivingRaw(ReadMyId());
    return me ? me->type_map : 0;
}

uint32_t GetPlayerModelState() {
    auto* me = GetAgentLivingRaw(ReadMyId());
    return me ? me->model_state : 0;
}

bool IsPlayerRuntimeReady(bool requireSkillbar) {
    const uint32_t myId = ReadMyId();
    if (!myId) return false;

    auto* me = GetAgentLivingRaw(myId);
    if (!me || me->hp <= 0.0f) return false;

    float x = 0.0f;
    float y = 0.0f;
    if (!TryReadAgentPosition(myId, x, y)) return false;
    if (x == 0.0f && y == 0.0f) return false;

    if ((me->type_map & 0x400000) == 0) return false;

    if (requireSkillbar && !SkillMgr::GetPlayerSkillbar()) return false;
    return true;
}

void DumpCurrentTargetCandidates(const char* phase, uint32_t requestedTarget) {
    static constexpr uint8_t kTailPattern[] = {0x83, 0xC4, 0x08, 0x5F, 0x8B, 0xE5, 0x5D, 0xC3, 0xCC};
    uintptr_t matches[8] = {};
    size_t matchCount = 0;

    const auto text = Scanner::GetTextSection();
    const auto* mem = reinterpret_cast<const uint8_t*>(text.start);
    const size_t patLen = sizeof(kTailPattern);
    for (size_t i = 0; i + patLen <= text.size && matchCount < 8; ++i) {
        if (memcmp(mem + i, kTailPattern, patLen) == 0) {
            matches[matchCount++] = text.start + i;
        }
    }

    IntReport("  Candidate probe (%s, requested=%u): live tail matches=%u",
              phase, requestedTarget, static_cast<unsigned>(matchCount));

    for (size_t matchIndex = 0; matchIndex < matchCount; ++matchIndex) {
        const uintptr_t tailAddr = matches[matchIndex];
        for (int delta = -16; delta <= 0; ++delta) {
            const uintptr_t probeAddr = tailAddr + delta;
            if (probeAddr < 0x10000) continue;

            __try {
                const uint32_t raw = *reinterpret_cast<uint32_t*>(probeAddr);
                uint32_t deref = 0;
                if (raw > 0x10000 && raw < 0x80000000) {
                    __try {
                        deref = *reinterpret_cast<uint32_t*>(raw);
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        deref = 0xFFFFFFFFu;
                    }
                }

                const bool interesting =
                    raw == requestedTarget ||
                    deref == requestedTarget ||
                    (raw > 0x10000 && raw < 0x80000000) ||
                    (deref > 0 && deref < 5000) ||
                    delta == -8 || delta == -9 || delta == -10 || delta == -14;
                if (!interesting) continue;

                IntReport("    tail[%u] delta=%d probe=0x%08X raw=0x%08X deref=0x%08X",
                          static_cast<unsigned>(matchIndex),
                          delta,
                          static_cast<unsigned>(probeAddr),
                          raw,
                          deref);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                IntReport("    tail[%u] delta=%d probe=0x%08X fault",
                          static_cast<unsigned>(matchIndex),
                          delta,
                          static_cast<unsigned>(probeAddr));
            }
        }
    }
}

uint32_t FindNearbyNpcLikeAgent(float maxDistance) {
    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return 0;

    if (Offsets::AgentBase <= 0x10000) return 0;
    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return 0;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    if (maxAgents == 0) return 0;

    const float maxDistSq = maxDistance * maxDistance;
    float bestNpcLikeDistSq = maxDistSq;
    uint32_t bestNpcLikeId = 0;
    float bestFallbackDistSq = maxDistSq;
    uint32_t bestFallbackId = 0;

    for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000 || i == myId) continue;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if (base->type != 0xDB) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->hp <= 0.0f) continue;

        const float distSq = AgentMgr::GetSquaredDistance(myX, myY, living->x, living->y);
        if (distSq < bestFallbackDistSq) {
            bestFallbackDistSq = distSq;
            bestFallbackId = i;
        }

        if (living->allegiance == 3) continue;

        if (distSq < bestNpcLikeDistSq) {
            bestNpcLikeDistSq = distSq;
            bestNpcLikeId = i;
        }
    }

    return bestNpcLikeId ? bestNpcLikeId : bestFallbackId;
}

} // namespace GWA3::SmokeTest
