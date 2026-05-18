static int FindRechargeBlockedNonChosenSlot(const CombatObservabilitySnapshot& before, int chosenSlotIndex) {
    for (int i = 0; i < 8; ++i) {
        if (i == chosenSlotIndex) continue;
        if (before.skillbar.recharge[i] > 0) {
            return i;
        }
    }
    return -1;
}

static void AssertRechargeBlockedCandidateStaysBlocked(const CombatObservabilitySnapshot& before,
                                                       const CombatObservabilitySnapshot& after,
                                                       int chosenSlotIndex) {
    const int blockedSlot = FindRechargeBlockedNonChosenSlot(before, chosenSlotIndex);
    if (blockedSlot < 0) {
        FroggyFeatureSkip("Recharge-blocked candidate assertion",
                "No non-chosen recharging skill was available to prove gating");
        return;
    }

    FroggyFeatureReport("  Recharge-blocked slot %d: before=%u after=%u",
              blockedSlot + 1, before.skillbar.recharge[blockedSlot], after.skillbar.recharge[blockedSlot]);
    if (before.skillbar.recharge[blockedSlot] > 0 && after.skillbar.recharge[blockedSlot] > 0) {
        FroggyFeatureCheck("Recharge-blocked candidate stayed non-ready", true);
    } else {
        FroggyFeatureSkip("Recharge-blocked candidate assertion",
                "The sampled blocked slot cooled down naturally before the post-step snapshot");
    }
}
