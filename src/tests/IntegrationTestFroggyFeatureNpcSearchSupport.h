static uint32_t FindNearestNpc(float x, float y, float maxDist) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue; // NPC
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (d < bestDist) { bestDist = d; bestId = living.agentId; }
    }
    return bestId;
}

static uint32_t FindNearestNpcByPlayerNumber(float x, float y, float maxDist, uint16_t playerNumber) {
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = maxDist * maxDist;
    uint32_t bestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue;
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;
        if (living.playerNumber != playerNumber) continue;
        float d = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (d < bestDist) { bestDist = d; bestId = living.agentId; }
    }
    return bestId;
}
