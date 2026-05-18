static void RunDefaultFoeTargetSelectionBranch(uint32_t foeId) {
    uint32_t targetId = 0;
    if (!ResolveSyntheticCombatTarget(TEST_ROLE_HEX | TEST_ROLE_PRESSURE, foeId, targetId,
                                      "Combat target selection - unhexed foe branch")) {
        return;
    }
    if (targetId == 0) {
        FroggyFeatureSkip("Combat target selection - unhexed foe branch",
                "No valid foe target resolved for the synthetic hex/pressure branch");
        return;
    }

    FroggyFeatureCheck("Unhexed/default foe branch resolved live foe target", IsLiveEnemyAgent(targetId));
}
