#pragma once

#include <cstddef>
#include <cstdint>

namespace GWA3::DungeonDiagnostics {

struct NearbyNpcCandidate {
    uint32_t agentId = 0;
    uint16_t playerNumber = 0;
    uint32_t npcId = 0;
    uint32_t effects = 0;
    float x = 0.0f;
    float y = 0.0f;
    float distanceToSearch = 0.0f;
    float distanceToPlayer = 0.0f;
    uint32_t score = 0xFFFFFFFFu;
};

std::size_t CollectNearbyNpcCandidates(
    float x,
    float y,
    float maxDist,
    NearbyNpcCandidate* out,
    std::size_t maxOut);
void LogNearbyNpcCandidates(
    const char* label,
    float x,
    float y,
    float maxDist,
    NearbyNpcCandidate* candidates,
    std::size_t count);
void LogAgentIdentity(const char* label, uint32_t agentId);
void LogNearbySignposts(float x, float y, float maxDist, const char* label, bool chestOnly);

} // namespace GWA3::DungeonDiagnostics
