static void ReportCombatAgentReadPair(const char* phaseLabel, const CombatAgentReadPair& pair) {
    const float foeDistance = AgentMgr::GetDistance(pair.player.x, pair.player.y, pair.foe.x, pair.foe.y);
    FroggyFeatureReport("  Combat agent read %s: player=%u foe=%u distance=%.0f hp=(%.3f, %.3f) cast=(%u, %u)",
              phaseLabel ? phaseLabel : "snapshot",
              pair.player.agentId,
              pair.foe.agentId,
              foeDistance,
              pair.player.hp,
              pair.foe.hp,
              pair.player.castingSkill,
              pair.foe.castingSkill);
}
