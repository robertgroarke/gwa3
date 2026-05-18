static size_t CollectMerchantNpcCandidates(float x, float y, float maxDist, uint16_t preferredPlayerNumber, NpcCandidate* out, size_t maxOut) {
    if (!out || maxOut == 0) return 0;
    ResetMerchantNpcCandidateBuffer(out, maxOut);

    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    const float maxDistSq = maxDist * maxDist;
    size_t count = 0;

    for (uint32_t i = 1; i < maxAgents; ++i) {
        LivingAgentSnapshot living;
        if (!TrySnapshotLivingAgent(i, living)) continue;
        if (living.type != 0xDB) continue;
        if (living.allegiance != 6) continue;
        if (living.hp <= 0.0f) continue;
        if ((living.effects & 0x0010u) != 0) continue;

        const float distSq = AgentMgr::GetSquaredDistance(x, y, living.x, living.y);
        if (distSq > maxDistSq) continue;

        NpcCandidate candidate;
        candidate.agentId = living.agentId;
        candidate.playerNumber = living.playerNumber;
        candidate.npcId = living.npcId;
        candidate.effects = living.effects;
        candidate.x = living.x;
        candidate.y = living.y;
        candidate.distance = sqrtf(distSq);
        candidate.score = static_cast<uint32_t>(candidate.distance) + (candidate.playerNumber == preferredPlayerNumber ? 0u : 100000u);

        if (InsertRankedMerchantNpcCandidate(candidate, out, maxOut) && count < maxOut) {
            count++;
        }
    }

    return count;
}
