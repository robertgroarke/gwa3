static bool PrepareSpiritChainRegressionEngagement(uint32_t foeId) {
    SkillbarSnapshot barBefore = {};
    if (!CapturePlayerSkillbarSnapshot(barBefore)) {
        FroggyFeatureSkip("Spirit chain regression", "Could not read player skillbar before spirit chain validation");
        return false;
    }
    if (barBefore.skillIds[1] != 1239u) {
        FroggyFeatureSkip("Spirit chain regression", "Current bar does not have Signet of Spirits in slot 2");
        return false;
    }
    if (barBefore.recharge[1] > 0) {
        FroggyFeatureReport("  Spirit chain waiting for Signet of Spirits recharge: %u", barBefore.recharge[1]);
        const bool signetReady = WaitFor("Spirit chain slot 2 recharge clears", 30000, [&barBefore]() {
            SkillbarSnapshot current = {};
            if (!CapturePlayerSkillbarSnapshot(current)) return false;
            barBefore = current;
            return current.skillIds[1] == 1239u && current.recharge[1] == 0;
        });
        if (!signetReady) {
            FroggyFeatureSkip("Spirit chain regression", "Signet of Spirits did not recharge within the validation window");
            return false;
        }
        FroggyFeatureCheck("Spirit chain slot 2 became ready for validation", true);
    }

    GameThread::Enqueue([foeId]() {
        AgentMgr::ChangeTarget(foeId);
    });
    const bool targetChanged = WaitFor("Spirit chain regression target foe", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    FroggyFeatureCheck("Spirit chain regression target set to foe", targetChanged);
    if (!targetChanged) {
        FroggyFeatureSkip("Spirit chain regression", "Could not target foe");
        return false;
    }

    const bool inCombatRange = MoveNearFoeForCombat(foeId, 1200.0f, 15000);
    if (!inCombatRange) {
        FroggyFeatureSkip("Spirit chain regression", "Could not move into engagement range");
        return false;
    }
    FroggyFeatureCheck("Spirit chain regression moved into engagement range", true);
    return true;
}
