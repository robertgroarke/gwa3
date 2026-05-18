static void PrepareCombatCastTelemetryTarget(uint32_t foeId) {
    GameThread::Enqueue([foeId]() {
        AgentMgr::ChangeTarget(foeId);
    });
    const bool targetChanged = WaitFor("Combat target telemetry set foe target", 3000, [foeId]() {
        return AgentMgr::GetTargetId() == foeId;
    });
    FroggyFeatureCheck("Combat target telemetry change target to foe", targetChanged);
}

static bool ChooseCombatCastTelemetryCandidate(SkillTestCandidate& candidate) {
    if (TryChooseSkillTestCandidate(candidate)) return true;

    DumpSkillbarForSkillTest();
    FroggyFeatureSkip("Combat target/cast telemetry validation", "No suitable recharged player skill candidate found");
    return false;
}
