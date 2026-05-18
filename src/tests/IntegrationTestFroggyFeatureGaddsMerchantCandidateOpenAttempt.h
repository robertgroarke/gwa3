struct GaddsMerchantOpenAttemptResult {
    bool reachedNpc = false;
    bool merchantOpen = false;
    uint32_t openedMerchantId = 0;
};

static GaddsMerchantOpenAttemptResult TryOpenGaddsMerchantCandidate(
    const NpcCandidate& candidate,
    size_t candidateIndex,
    size_t merchantCandidateCount) {
    GaddsMerchantOpenAttemptResult result = {};
    const uint32_t resolvedAgentId = ResolveMerchantCandidateAgentId(candidate);
    LivingAgentSnapshot npc;
    const bool haveNpc = resolvedAgentId && TrySnapshotLivingAgent(resolvedAgentId, npc);
    FroggyFeatureReport("  Merchant candidate %u/%u: agent=%u allegiance=%u player_number=%u npc_id=%u at (%.0f, %.0f)",
              static_cast<unsigned>(candidateIndex + 1),
              static_cast<unsigned>(merchantCandidateCount),
              resolvedAgentId ? resolvedAgentId : candidate.agentId,
              haveNpc ? npc.allegiance : 0,
              haveNpc ? npc.playerNumber : candidate.playerNumber,
              haveNpc ? npc.npcId : 0,
              haveNpc ? npc.x : 0.0f,
              haveNpc ? npc.y : 0.0f);

    if (!haveNpc) {
        FroggyFeatureReport("  WARN: Candidate %u could not be resolved to a readable living NPC", candidate.agentId);
        return result;
    }

    float postApproachDistance = 0.0f;
    result.reachedNpc = MovePlayerNearForMerchantHarnessBody(npc.x, npc.y, &postApproachDistance);

    float px = 0.0f;
    float py = 0.0f;
    TryReadAgentPosition(ReadMyId(), px, py);
    FroggyFeatureReport("  After merchant candidate approach: pos=(%.0f,%.0f) reached=%d dist=%.0f",
              px, py, result.reachedNpc ? 1 : 0, postApproachDistance);

    if (!result.reachedNpc) {
        FroggyFeatureReport("  WARN: Could not reach merchant candidate %u", resolvedAgentId);
        return result;
    }

    ReportMerchantPreInteractState("Froggy pre-interact snapshot", resolvedAgentId, npc.x, npc.y);
    ReportMerchantRuntimeContext("Froggy runtime context");
    result.merchantOpen = OpenMerchantContextWithSessionHarnessBody(resolvedAgentId, npc.x, npc.y);
    if (result.merchantOpen) {
        result.openedMerchantId = resolvedAgentId;
    } else {
        FroggyFeatureReport("  Candidate %u failed to open merchant context; trying next candidate", resolvedAgentId);
    }
    return result;
}
