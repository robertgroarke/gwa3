static void SkipBuiltinCombatProofSuiteNoFoe() {
    FroggyFeatureSkip("Builtin combat single-step proof", "No reachable foe found for builtin combat proof suite");
    FroggyFeatureSkip("Combat target selection coverage", "No reachable foe found for builtin combat proof suite");
    FroggyFeatureSkip("Cast gating and safety assertions", "No reachable foe found for builtin combat proof suite");
    FroggyFeatureSkip("Spirit chain regression", "No reachable foe found for builtin combat proof suite");
}

static void RunBuiltinCombatProofSuite(uint32_t seedFoeId) {
    uint32_t proofFoeId = 0;
    const char* proofLabel = "combat proof foe";
    const bool haveProofFoe = SelectReachableBuiltinCombatProofFoe(seedFoeId, proofFoeId, proofLabel);
    if (!haveProofFoe) {
        SkipBuiltinCombatProofSuiteNoFoe();
        return;
    }

    RunBuiltinCombatSingleStepProof(proofFoeId, proofLabel);
    RunCombatTargetSelectionCoverage(proofFoeId, proofLabel);
    if (s_lastBuiltinCombatSnapshotsValid) {
        RunCombatCastGatingAndSafetyAssertions(s_lastBuiltinCombatBefore, s_lastBuiltinCombatAfter, proofLabel);
    } else {
        FroggyFeatureSkip("Cast gating and safety assertions", "Builtin combat proof did not capture before/after snapshots");
    }

    uint32_t spiritFoeId = 0;
    if (SelectSpiritChainFoeAfterBuiltinCombatProof(proofFoeId, spiritFoeId)) {
        RunBuiltinSpiritChainRegression(spiritFoeId, proofLabel);
    }
}
