static bool ValidateCombatAgentRawPointers(const CombatAgentReadPair& pair) {
    auto* rawPlayer = GetAgentLivingRaw(pair.player.agentId);
    auto* rawFoe = GetAgentLivingRaw(pair.foe.agentId);
    FroggyFeatureCheck("Combat agent read raw player pointer available", rawPlayer != nullptr);
    FroggyFeatureCheck("Combat agent read raw foe pointer available", rawFoe != nullptr);
    if (!rawPlayer || !rawFoe) {
        FroggyFeatureSkip("Combat agent read validation", "Raw player or foe pointer unavailable");
        return false;
    }

    FroggyFeatureCheck("Combat agent read player id matches raw pointer", rawPlayer->agent_id == pair.player.agentId);
    FroggyFeatureCheck("Combat agent read foe id matches raw pointer", rawFoe->agent_id == pair.foe.agentId);
    FroggyFeatureCheck("Combat agent read raw foe allegiance matches snapshot", rawFoe->allegiance == pair.foe.allegiance);
    return true;
}

static void ValidateCombatAgentSnapshotSanity(const CombatAgentReadPair& pair, bool afterDwell) {
    FroggyFeatureCheck(afterDwell ? "Combat agent read player hp sane after dwell" : "Combat agent read player hp sane",
                pair.player.hp >= 0.0f && pair.player.hp <= 2.0f);
    FroggyFeatureCheck(afterDwell ? "Combat agent read foe hp sane after dwell" : "Combat agent read foe hp sane",
                pair.foe.hp >= 0.0f && pair.foe.hp <= 2.0f);
    FroggyFeatureCheck(afterDwell ? "Combat agent read player position sane after dwell" : "Combat agent read player position sane",
                IsSaneCombatPosition(pair.player.x, pair.player.y));
    FroggyFeatureCheck(afterDwell ? "Combat agent read foe position sane after dwell" : "Combat agent read foe position sane",
                IsSaneCombatPosition(pair.foe.x, pair.foe.y));
    if (!afterDwell) {
        FroggyFeatureCheck("Combat agent read foe allegiance is enemy", pair.foe.allegiance == 3);
    }
}

static void ValidateCombatAgentReadStability(const CombatAgentReadPair& before, const CombatAgentReadPair& after) {
    const float foeDistanceAfter = AgentMgr::GetDistance(after.player.x, after.player.y, after.foe.x, after.foe.y);
    FroggyFeatureCheck("Combat agent read player id stable", after.player.agentId == before.player.agentId);
    FroggyFeatureCheck("Combat agent read foe id stable", after.foe.agentId == before.foe.agentId);
    FroggyFeatureCheck("Combat agent read foe allegiance stable", after.foe.allegiance == before.foe.allegiance);
    ValidateCombatAgentSnapshotSanity(after, true);
    FroggyFeatureCheck("Combat agent read foe distance remains plausible", foeDistanceAfter >= 0.0f && foeDistanceAfter < 5000.0f);
}
