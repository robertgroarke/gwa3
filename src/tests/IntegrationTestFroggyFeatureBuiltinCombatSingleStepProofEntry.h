static void RunBuiltinCombatSingleStepProof(uint32_t foeId, const char* label) {
    FroggyFeatureReport("=== PHASE 5G: Builtin Combat Single-Step Proof (%s) ===", label ? label : "default");
    s_lastBuiltinCombatSnapshotsValid = false;

    CombatObservabilitySnapshot before = {};
    if (!CaptureBuiltinCombatPreStepSnapshot(foeId, before)) {
        return;
    }

    if (!PrepareBuiltinCombatFoeEngagement(foeId)) return;

    ReportBuiltinCombatDecisionDump(foeId);

    BuiltinCombatStepAction action = {};
    if (!ExecuteBuiltinCombatProofStep(foeId, action)) {
        return;
    }

    CombatObservabilitySnapshot after = {};
    const bool observedSignal = ObserveBuiltinCombatSignal(foeId, before, action.autoAttack, after);

    if (!ValidateBuiltinCombatPostStepSnapshot(after)) {
        return;
    }

    if (!ValidateBuiltinCombatStepSignal(foeId, before, after, action.autoAttack, observedSignal)) {
        return;
    }

    PublishBuiltinCombatStepSnapshots(before, after);
}
