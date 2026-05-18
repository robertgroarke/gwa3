static void RunCombatObservabilityHarness(uint32_t foeId, const char* label) {
    FroggyFeatureReport("=== PHASE 5B: Combat Observability Harness (%s) ===", label ? label : "default");

    const bool targetReady = WaitForCombatTargetAcquire(foeId);
    FroggyFeatureCheck("Combat observability target set to foe", targetReady);

    CombatObservabilitySnapshot before = {};
    const bool beforeCaptured = CaptureCombatObservabilitySnapshot(foeId, before);
    FroggyFeatureCheck("Combat snapshot captured before dwell", beforeCaptured);
    if (!beforeCaptured) {
        FroggyFeatureSkip("Combat observability harness", "Could not capture initial snapshot");
        return;
    }
    ReportCombatObservabilitySnapshot("Combat snapshot before dwell", before);

    Sleep(1500);

    CombatObservabilitySnapshot after = {};
    const bool afterCaptured = CaptureCombatObservabilitySnapshot(foeId, after);
    FroggyFeatureCheck("Combat snapshot captured after dwell", afterCaptured);
    if (!afterCaptured) {
        FroggyFeatureSkip("Combat observability harness after dwell", "Could not capture follow-up snapshot");
        return;
    }
    ReportCombatObservabilitySnapshot("Combat snapshot after dwell", after);

    FroggyFeatureCheck("Combat snapshot player valid before dwell", before.player.valid);
    FroggyFeatureCheck("Combat snapshot player valid after dwell", after.player.valid);
    FroggyFeatureCheck("Combat snapshot foe valid before dwell", before.foe.valid);
    FroggyFeatureCheck("Combat snapshot foe valid after dwell", after.foe.valid);
    FroggyFeatureCheck("Combat snapshot foe is enemy allegiance", before.foe.valid && before.foe.allegiance == 3);
    FroggyFeatureCheck("Combat snapshot target stable across dwell", targetReady && before.targetId == foeId && after.targetId == foeId);
    FroggyFeatureCheck("Combat snapshot skillbar valid before dwell", before.skillbar.valid);
    FroggyFeatureCheck("Combat snapshot skillbar valid after dwell", after.skillbar.valid);
    FroggyFeatureCheck("Combat snapshot skillbar has non-zero skills before dwell", before.skillbar.nonZeroSkills > 0);
    FroggyFeatureCheck("Combat snapshot skillbar has non-zero skills after dwell", after.skillbar.nonZeroSkills > 0);
    FroggyFeatureCheck("Combat snapshot hero count available", before.heroCount > 0);
    FroggyFeatureCheck("Combat snapshot hero agents readable before dwell", before.heroAgentsReadable);
    FroggyFeatureCheck("Combat snapshot hero agents readable after dwell", after.heroAgentsReadable);
}
