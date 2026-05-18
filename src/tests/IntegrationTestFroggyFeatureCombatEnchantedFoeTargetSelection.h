static void RunEnchantedFoeTargetSelectionBranch(uint32_t foeId) {
    uint32_t targetId = 0;
    if (!ResolveSyntheticCombatTarget(TEST_ROLE_ENCHANT_REMOVE, foeId, targetId,
                                      "Combat target selection - enchanted foe branch")) {
        return;
    }

    const uint32_t enchantedFoe = DungeonSkill::GetEnchantedBalledEnemy();
    if (!enchantedFoe) {
        FroggyFeatureSkip("Combat target selection - enchanted foe branch",
                "No enchanted foe present in the current encounter");
        return;
    }

    FroggyFeatureCheck("Enchanted-foe branch resolved enchanted foe", targetId == enchantedFoe);
    FroggyFeatureCheck("Enchanted-foe branch target is live foe", targetId != 0 && IsLiveEnemyAgent(targetId));
}
