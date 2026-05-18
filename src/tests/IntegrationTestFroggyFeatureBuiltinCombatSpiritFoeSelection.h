static bool SelectSpiritChainFoeAfterBuiltinCombatProof(uint32_t proofFoeId, uint32_t& outSpiritFoeId) {
    outSpiritFoeId = proofFoeId;
    auto* spiritFoe = GetAgentLivingRaw(outSpiritFoeId);
    if (!spiritFoe || spiritFoe->hp <= 0.0f) {
        outSpiritFoeId = FindNearestFoe(5000.0f);
        FroggyFeatureReport("  Spirit chain selecting fresh live foe after builtin combat proof: %u", outSpiritFoeId);
    }
    if (!outSpiritFoeId) {
        FroggyFeatureSkip("Spirit chain regression", "No live foe available after builtin combat proof");
        return false;
    }

    WaitForPlayerPositionSettle(1000, 20.0f);
    if (!WaitForCombatTargetAcquire(outSpiritFoeId, 1500, 1500)) {
        FroggyFeatureSkip("Spirit chain regression", "Could not retarget a live foe after builtin combat proof");
        return false;
    }
    return true;
}
