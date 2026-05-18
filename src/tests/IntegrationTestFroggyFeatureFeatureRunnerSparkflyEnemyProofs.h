static bool RunSparkflyEnemyProofs() {
    FroggyFeatureReport("=== PHASE 5: Explorable Tests ===");
    RunExplorableSkillbarRefreshProof();
    RunExplorablePlayerEffectsProof();

    uint32_t foeId = FindNearestFoe(5000.0f);
    if (foeId) {
        FroggyFeatureCheck("Found enemy in explorable", true);
        FroggyFeatureReport("  Enemy agent=%u found", foeId);
        auto* foeAgent = AgentMgr::GetAgentByID(foeId);
        FroggyFeatureCheck("Enemy agent readable", foeAgent != nullptr);
        if (foeAgent) {
            auto* foeLiving = static_cast<AgentLiving*>(foeAgent);
            FroggyFeatureCheck("Enemy is alive", foeLiving->hp > 0.0f);
            FroggyFeatureCheck("Enemy is foe allegiance", foeLiving->allegiance == 3);
        }

        AgentMgr::ChangeTarget(foeId);
        Sleep(500);
        FroggyFeatureCheck("Target changed to enemy", AgentMgr::GetTargetId() == foeId);
        RunCombatProofsForFoe(foeId, "initial foe");
        return RunIsolatedExplorableFlaggingProof(
            "Hero flagging in explorable keeps player agent valid",
            "Hero flagging in explorable");
    }

    FroggyFeatureSkip("Enemy targeting tests", "No enemies within 5000 range");
    FroggyFeatureReport("  Moving toward enemies...");
    MovePlayerNear(-4559.0f, -14406.0f, 500.0f, 25000);
    foeId = FindNearestFoe(5000.0f);
    if (!foeId) {
        FroggyFeatureSkip("Enemy found after move", "Still no enemies - area may be cleared");
        return false;
    }

    FroggyFeatureCheck("Found enemy after moving", true);
    const bool targetChangedAfterMove = WaitForCombatTargetAcquire(foeId);
    FroggyFeatureCheck("Target changed to enemy after moving", targetChangedAfterMove);
    RunCombatProofsForFoe(foeId, "foe after moving");
    return RunIsolatedExplorableFlaggingProof(
        "Hero flagging in explorable after moving keeps player agent valid",
        "Hero flagging in explorable after moving");
}
