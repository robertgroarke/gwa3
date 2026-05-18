static void RunCastingFoeTargetSelectionBranch(uint32_t foeId) {
    uint32_t targetId = 0;
    if (!ResolveSyntheticCombatTarget(TEST_ROLE_INTERRUPT_HARD | TEST_ROLE_INTERRUPT_SOFT, foeId, targetId,
                                      "Combat target selection - casting foe branch")) {
        return;
    }

    const uint32_t castingFoe = DungeonSkill::GetCastingBalledEnemy();
    if (!castingFoe) {
        FroggyFeatureSkip("Combat target selection - casting foe branch",
                "No casting foe present in the current encounter");
        return;
    }

    auto* a = AgentMgr::GetAgentByID(castingFoe);
    auto* living = (a && a->type == 0xDB) ? static_cast<AgentLiving*>(a) : nullptr;
    FroggyFeatureCheck("Casting-foe branch resolved current casting foe", targetId == castingFoe);
    FroggyFeatureCheck("Casting-foe branch target is live foe", targetId != 0 && IsLiveEnemyAgent(targetId));
    FroggyFeatureCheck("Casting-foe branch target observed non-zero skill", living && living->skill != 0);
}
