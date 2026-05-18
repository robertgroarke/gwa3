static bool CaptureBuiltinCombatPreStepSnapshot(uint32_t foeId, CombatObservabilitySnapshot& outBefore) {
    const bool beforeCaptured = CaptureCombatObservabilitySnapshot(foeId, outBefore);
    FroggyFeatureCheck("Builtin combat proof snapshot captured before step", beforeCaptured);
    if (!beforeCaptured) {
        FroggyFeatureSkip("Builtin combat single-step proof", "Could not capture pre-step combat snapshot");
        return false;
    }
    return true;
}

static bool ValidateBuiltinCombatPostStepSnapshot(const CombatObservabilitySnapshot& after) {
    FroggyFeatureCheck("Builtin combat proof snapshot captured after step", after.player.valid || after.foe.valid || after.skillbar.valid);
    if (!after.player.valid || !after.foe.valid) {
        FroggyFeatureSkip("Builtin combat single-step proof after step", "Could not capture post-step player/foe snapshot");
        return false;
    }
    return true;
}

static void PublishBuiltinCombatStepSnapshots(const CombatObservabilitySnapshot& before,
                                              const CombatObservabilitySnapshot& after) {
    s_lastBuiltinCombatBefore = before;
    s_lastBuiltinCombatAfter = after;
    s_lastBuiltinCombatSnapshotsValid = true;
}
