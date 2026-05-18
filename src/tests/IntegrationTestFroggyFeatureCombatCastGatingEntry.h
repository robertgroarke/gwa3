static void RunCombatCastGatingAndSafetyAssertions(const CombatObservabilitySnapshot& before,
                                                   const CombatObservabilitySnapshot& after,
                                                   const char* label) {
    FroggyFeatureReport("=== PHASE 5I: Cast Gating and Safety Assertions (%s) ===", label ? label : "default");

    const Bot::Froggy::LastCombatStepInfo info = Bot::Froggy::g_combatSession.last_action;
    if (!ValidateCombatCastGatingStepInfo(info)) {
        return;
    }

    const int slotIndex = info.slot - 1;
    ReportChosenCombatSkillCastWindow(info, before, after, slotIndex);
    AssertChosenCombatSkillTarget(info);
    AssertChosenCombatSkillCastTransition(before, after, slotIndex);
    AssertCombatCastAftercastPacing(info);
    AssertRechargeBlockedCandidateStaysBlocked(before, after, slotIndex);

    FroggyFeatureCheck("Combat cast left player snapshot valid after step", after.player.valid);
    FroggyFeatureCheck("Combat cast left foe snapshot valid after step", after.foe.valid);
}
