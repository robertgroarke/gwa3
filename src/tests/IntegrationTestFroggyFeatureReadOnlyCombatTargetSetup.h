static void SetReadOnlyCombatPreconditionTarget(uint32_t foeId) {
    GameThread::Enqueue([foeId]() {
        AgentMgr::ChangeTarget(foeId);
    });
    const bool targetChanged = WaitFor("Combat preconditions target foe", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    FroggyFeatureCheck("Combat preconditions current target set to foe", targetChanged);
}
