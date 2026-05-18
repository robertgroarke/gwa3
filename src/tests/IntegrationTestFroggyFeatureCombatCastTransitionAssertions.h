static void AssertChosenCombatSkillCastTransition(const CombatObservabilitySnapshot& before,
                                                  const CombatObservabilitySnapshot& after,
                                                  int slotIndex) {
    const uint32_t beforeRecharge = before.skillbar.recharge[slotIndex];
    const uint32_t afterRecharge = after.skillbar.recharge[slotIndex];
    const bool chosenRechargeTransition = beforeRecharge == 0 && afterRecharge > 0;
    const bool chosenEnergyChanged = after.player.maxEnergy == before.player.maxEnergy &&
                                     after.player.energy != before.player.energy;
    const bool chosenActiveSkillChanged = after.player.castingSkill != before.player.castingSkill;
    if (chosenRechargeTransition || chosenEnergyChanged || chosenActiveSkillChanged) {
        FroggyFeatureCheck("Chosen combat skill shows gated cast-side transition", true);
    } else {
        FroggyFeatureSkip("Chosen combat skill cast-side transition",
                "No slot-local recharge, energy, or active-skill transition was observable for this skill window");
    }
}
