static void ReportBuiltinCombatDistance(uint32_t foeId, const char* label) {
    auto* me = GetAgentLivingRaw(AgentMgr::GetMyId());
    auto* foe = GetAgentLivingRaw(foeId);
    if (!me || !foe) return;

    const float distance = AgentMgr::GetDistance(me->x, me->y, foe->x, foe->y);
    FroggyFeatureReport("  Builtin combat %s distance: %.0f", label, distance);
}

static bool PrepareBuiltinCombatFoeEngagement(uint32_t foeId) {
    GameThread::Enqueue([foeId]() {
        AgentMgr::ChangeTarget(foeId);
    });
    const bool targetChanged = WaitFor("Builtin combat proof target foe", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    FroggyFeatureCheck("Builtin combat proof target set to foe", targetChanged);

    ReportBuiltinCombatDistance(foeId, "pre-engage");

    const bool inCombatRange = MoveNearFoeForCombat(foeId, 1200.0f, 12000);
    if (!inCombatRange) {
        FroggyFeatureSkip("Builtin combat single-step proof", "Could not move into engagement range for builtin combat");
        return false;
    }
    FroggyFeatureCheck("Builtin combat proof moved into engagement range", true);
    const bool targetReadyAfterMove = WaitForCombatTargetAcquire(foeId, 1500, 1500);
    FroggyFeatureCheck("Builtin combat proof target stable after move", targetReadyAfterMove);
    if (!targetReadyAfterMove) {
        FroggyFeatureSkip("Builtin combat single-step proof", "Lost foe target while moving into engagement range");
        return false;
    }

    ReportBuiltinCombatDistance(foeId, "engaged");
    return true;
}
