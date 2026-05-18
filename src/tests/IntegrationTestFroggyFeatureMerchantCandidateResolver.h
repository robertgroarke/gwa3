static uint32_t ResolveMerchantCandidateAgentId(const NpcCandidate& candidate) {
    if (candidate.playerNumber) {
        const uint32_t byPlayerNumber = FindNearestNpcByPlayerNumber(
            candidate.x, candidate.y, 350.0f, candidate.playerNumber);
        if (byPlayerNumber) return byPlayerNumber;
    }
    LivingAgentSnapshot byId;
    if (TrySnapshotLivingAgent(candidate.agentId, byId)) return candidate.agentId;
    return 0;
}
