static void WaitBetweenCombatTargetAcquireAttempts() {
    WaitForPlayerPositionSettle(750, 15.0f);
    WaitFor("Botshub queue idle between ChangeTarget attempts", 750, []() {
        return CtoS::IsBotshubQueueIdle();
    });
}

static bool WaitForCombatTargetAcquire(uint32_t foeId,
                                       DWORD settleTimeoutMs = 2000,
                                       DWORD targetTimeoutMs = 2000) {
    if (foeId == 0) return false;
    if (AgentMgr::GetTargetId() == foeId) return true;

    WaitForPlayerPositionSettle(settleTimeoutMs, 15.0f);
    const bool queueIdle = WaitFor("Botshub queue idle before ChangeTarget", settleTimeoutMs, []() {
        return CtoS::IsBotshubQueueIdle();
    });
    FroggyFeatureReport("  Combat target acquire: foe=%u queueIdle=%d preTarget=%u",
              foeId,
              queueIdle ? 1 : 0,
              AgentMgr::GetTargetId());

    for (int attempt = 1; attempt <= 3; ++attempt) {
        AgentMgr::ChangeTarget(foeId);
        const bool targetChanged = WaitFor("Combat target acquired", targetTimeoutMs, [foeId]() {
            return AgentMgr::GetTargetId() == foeId;
        });
        FroggyFeatureReport("  Combat target acquire attempt %d/3: target=%u success=%d",
                  attempt,
                  AgentMgr::GetTargetId(),
                  targetChanged ? 1 : 0);
        if (targetChanged) {
            return true;
        }
        WaitBetweenCombatTargetAcquireAttempts();
    }

    return AgentMgr::GetTargetId() == foeId;
}
