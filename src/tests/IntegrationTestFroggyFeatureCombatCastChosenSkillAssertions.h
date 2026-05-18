static void ReportChosenCombatSkillCastWindow(const Bot::Froggy::LastCombatStepInfo& info,
                                              const CombatObservabilitySnapshot& before,
                                              const CombatObservabilitySnapshot& after,
                                              int slotIndex) {
    const uint32_t beforeRecharge = before.skillbar.recharge[slotIndex];
    const uint32_t afterRecharge = after.skillbar.recharge[slotIndex];
    FroggyFeatureReport("  Chosen skill slot %d recharge: before=%u after=%u expectedAftercastMs=%u observedDurationMs=%u",
              info.slot, beforeRecharge, afterRecharge, info.expected_aftercast_ms,
              info.finished_at_ms >= info.started_at_ms ? (info.finished_at_ms - info.started_at_ms) : 0);
}

static void AssertChosenCombatSkillTarget(const Bot::Froggy::LastCombatStepInfo& info) {
    FroggyFeatureCheck("Chosen combat skill target is non-zero", info.target_id != 0);
    if (info.target_type == 5) {
        FroggyFeatureCheck("Chosen combat skill target is live foe", IsLiveEnemyAgent(info.target_id));
    }
}
