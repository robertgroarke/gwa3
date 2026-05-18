static void ReportCombatObservabilitySnapshot(const char* label, const CombatObservabilitySnapshot& snap) {
    FroggyFeatureReport("  %s:", label);
    FroggyFeatureReport("    player valid=%d id=%u hp=%.3f energy=%.3f/%u cast=%u pos=(%.0f, %.0f)",
              snap.player.valid ? 1 : 0,
              snap.player.agentId,
              snap.player.hp,
              snap.player.energy,
              snap.player.maxEnergy,
              snap.player.castingSkill,
              snap.player.x,
              snap.player.y);
    FroggyFeatureReport("    foe valid=%d id=%u allegiance=%u hp=%.3f cast=%u pos=(%.0f, %.0f)",
              snap.foe.valid ? 1 : 0,
              snap.foe.agentId,
              snap.foe.allegiance,
              snap.foe.hp,
              snap.foe.castingSkill,
              snap.foe.x,
              snap.foe.y);
    FroggyFeatureReport("    target=%u heroes=%u heroAgentsReadable=%d",
              snap.targetId,
              snap.heroCount,
              snap.heroAgentsReadable ? 1 : 0);
    FroggyFeatureReport("    skillbar valid=%d agent=%u nonZero=%d ids=[%u %u %u %u %u %u %u %u] recharge=[%u %u %u %u %u %u %u %u]",
              snap.skillbar.valid ? 1 : 0,
              snap.skillbar.agentId,
              snap.skillbar.nonZeroSkills,
              snap.skillbar.skillIds[0], snap.skillbar.skillIds[1], snap.skillbar.skillIds[2], snap.skillbar.skillIds[3],
              snap.skillbar.skillIds[4], snap.skillbar.skillIds[5], snap.skillbar.skillIds[6], snap.skillbar.skillIds[7],
              snap.skillbar.recharge[0], snap.skillbar.recharge[1], snap.skillbar.recharge[2], snap.skillbar.recharge[3],
              snap.skillbar.recharge[4], snap.skillbar.recharge[5], snap.skillbar.recharge[6], snap.skillbar.recharge[7]);
}
