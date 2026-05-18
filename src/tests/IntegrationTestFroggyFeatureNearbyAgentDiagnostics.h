// Safe single-agent read for signpost/generic agent scan.
static bool TrySnapshotAgent(uint32_t agentId, uint32_t& outType, float& outX, float& outY) {
    auto* a = AgentMgr::GetAgentByID(agentId);
    if (!a) return false;
    __try {
        outType = a->type;
        outX = a->x;
        outY = a->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

struct NearbyAgentInfo {
    uint32_t id;
    uint32_t type;
    float x;
    float y;
    float dist;
};

static void SortNearbyAgentsByDistance(NearbyAgentInfo* agents, size_t count) {
    for (size_t i = 1; i < count; i++) {
        NearbyAgentInfo key = agents[i];
        size_t j = i;
        while (j > 0 && agents[j - 1].dist > key.dist) {
            agents[j] = agents[j - 1];
            j--;
        }
        agents[j] = key;
    }
}

// Dump nearby agents of all types for diagnostic purposes.
static void DumpNearbyAgents(float cx, float cy, float maxDist, size_t limit) {
    NearbyAgentInfo agents[32];
    size_t count = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float maxDistSq = maxDist * maxDist;

    for (uint32_t i = 1; i < maxAgents && count < 32; i++) {
        uint32_t aType = 0; float aX = 0, aY = 0;
        if (!TrySnapshotAgent(i, aType, aX, aY)) continue;
        float d = AgentMgr::GetSquaredDistance(cx, cy, aX, aY);
        if (d < maxDistSq) {
            agents[count++] = { i, aType, aX, aY, sqrtf(d) };
        }
    }

    SortNearbyAgentsByDistance(agents, count);

    size_t show = count < limit ? count : limit;
    FroggyFeatureReport("  Nearby agents within %.0f of (%.0f, %.0f): %zu found", maxDist, cx, cy, count);
    for (size_t i = 0; i < show; i++) {
        FroggyFeatureReport("    agent=%u type=0x%X pos=(%.0f, %.0f) dist=%.0f",
                  agents[i].id, agents[i].type, agents[i].x, agents[i].y, agents[i].dist);
    }
}
