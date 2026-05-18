static void DumpMerchantNpcCandidates(float x, float y, float maxDist, uint16_t preferredPlayerNumber) {
    NpcCandidate candidates[8];
    const size_t count = CollectMerchantNpcCandidates(x, y, maxDist, preferredPlayerNumber, candidates, _countof(candidates));
    FroggyFeatureReport("  NPC candidates near merchant coords (preferred player_number=%u): %zu", preferredPlayerNumber, count);
    for (size_t i = 0; i < count; ++i) {
        const auto& c = candidates[i];
        FroggyFeatureReport("    cand[%zu]: agent=%u player=%u npc_id=%u effects=0x%08X dist=%.0f pos=(%.0f, %.0f)%s",
                  i,
                  c.agentId,
                  c.playerNumber,
                  c.npcId,
                  c.effects,
                  c.distance,
                  c.x,
                  c.y,
                  c.playerNumber == preferredPlayerNumber ? " [preferred]" : "");
    }
}
