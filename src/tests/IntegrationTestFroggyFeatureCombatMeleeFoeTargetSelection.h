static void RunMeleeFoeTargetSelectionBranch(uint32_t foeId) {
    uint32_t targetId = 0;
    if (!ResolveSyntheticCombatTarget(TEST_ROLE_ATTACK, foeId, targetId,
                                      "Combat target selection - melee branch")) {
        return;
    }

    const uint32_t meleeFoe = DungeonSkill::GetMeleeBalledEnemy();
    if (!meleeFoe) {
        FroggyFeatureSkip("Combat target selection - melee branch",
                "No melee-range foe present in the current encounter");
        return;
    }

    auto* me = AgentMgr::GetMyAgent();
    auto* foe = AgentMgr::GetAgentByID(targetId);
    auto* living = (foe && foe->type == 0xDB) ? static_cast<AgentLiving*>(foe) : nullptr;
    const float dist = (me && living) ? AgentMgr::GetDistance(me->x, me->y, living->x, living->y) : 99999.0f;
    FroggyFeatureCheck("Melee branch resolved melee-range foe", targetId == meleeFoe);
    FroggyFeatureCheck("Melee branch target within melee threshold", dist <= 1320.0f);
}
