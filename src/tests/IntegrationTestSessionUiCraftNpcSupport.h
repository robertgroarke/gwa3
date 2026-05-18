size_t CollectNearestNpcLikeAgentsToCoords(float targetX, float targetY, float maxDistance, uint32_t* outIds, size_t capacity) {
    if (!outIds || capacity == 0 || Offsets::AgentBase <= 0x10000) return 0;

    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return 0;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    if (maxAgents == 0) return 0;

    struct Candidate {
        uint32_t id = 0;
        float distSq = 0.0f;
    };
    Candidate candidates[32]{};
    size_t count = 0;
    const float maxDistSq = maxDistance * maxDistance;
    const uint32_t myId = ReadMyId();

    for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
        if (i == myId) continue;
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000) continue;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if (base->type != 0xDB) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->hp <= 0.0f) continue;
        if (living->allegiance != 6) continue;

        const float distSq = AgentMgr::GetSquaredDistance(targetX, targetY, living->x, living->y);
        if (distSq > maxDistSq) continue;
        if (count >= _countof(candidates)) break;

        candidates[count].id = i;
        candidates[count].distSq = distSq;
        count++;
    }

    for (size_t i = 0; i < count; ++i) {
        size_t best = i;
        for (size_t j = i + 1; j < count; ++j) {
            if (candidates[j].distSq < candidates[best].distSq) best = j;
        }
        if (best != i) {
            Candidate tmp = candidates[i];
            candidates[i] = candidates[best];
            candidates[best] = tmp;
        }
    }

    const size_t emit = count < capacity ? count : capacity;
    for (size_t i = 0; i < emit; ++i) {
        outIds[i] = candidates[i].id;
    }
    return emit;
}

void DumpNpcLikeAgentsNearCoords(float targetX, float targetY, float maxDistance, size_t limit) {
    if (Offsets::AgentBase <= 0x10000) return;

    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    const float maxDistSq = maxDistance * maxDistance;
    size_t emitted = 0;

    for (uint32_t i = 1; i < maxAgents && i < 4096 && emitted < limit; ++i) {
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000) continue;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if (base->type != 0xDB) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->hp <= 0.0f) continue;
        if (i == ReadMyId()) continue;
        if (living->allegiance != 6) continue;

        const float distSq = AgentMgr::GetSquaredDistance(targetX, targetY, living->x, living->y);
        if (distSq > maxDistSq) continue;

        IntReport("    NPC cand id=%u dist=%.0f allegiance=%u player=%u npc_id=%u pos=(%.0f, %.0f)",
                  i,
                  sqrtf(distSq),
                  living->allegiance,
                  living->player_number,
                  living->transmog_npc_id,
                  living->x,
                  living->y);
        emitted++;
    }
}
