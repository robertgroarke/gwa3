static bool CaptureReadOnlyCombatSnapshot(
    uint32_t foeId,
    const char* checkName,
    const char* skipName,
    const char* skipReason,
    CombatObservabilitySnapshot& out) {
    const bool captured = CaptureCombatObservabilitySnapshot(foeId, out);
    FroggyFeatureCheck(checkName, captured);
    if (!captured) {
        FroggyFeatureSkip(skipName, skipReason);
        return false;
    }
    return true;
}

static void AssertReadOnlyCombatSnapshotBeforeDwell(const CombatObservabilitySnapshot& before) {
    FroggyFeatureCheck("Combat preconditions player skillbar exists", before.skillbar.valid);
    FroggyFeatureCheck("Combat preconditions player skillbar has non-zero skills", before.skillbar.nonZeroSkills > 0);
    FroggyFeatureCheck("Combat preconditions foe readable before dwell", before.foe.valid);
    FroggyFeatureCheck("Combat preconditions foe allegiance is enemy", before.foe.valid && before.foe.allegiance == 3);
    FroggyFeatureCheck("Combat preconditions player alive before dwell", before.player.valid && before.player.hp > 0.0f);
    FroggyFeatureCheck("Combat preconditions foe alive before dwell", before.foe.valid && before.foe.hp > 0.0f);
    FroggyFeatureCheck("Combat preconditions hero count available", before.heroCount > 0);
    FroggyFeatureCheck("Combat preconditions hero agents readable before dwell", before.heroAgentsReadable);
}

static void AssertReadOnlyCombatSnapshotAfterDwell(const CombatObservabilitySnapshot& after, uint32_t foeId) {
    FroggyFeatureCheck("Combat preconditions foe remains readable across dwell", after.foe.valid);
    FroggyFeatureCheck("Combat preconditions player remains alive across dwell", after.player.valid && after.player.hp > 0.0f);
    FroggyFeatureCheck("Combat preconditions foe remains alive across dwell", after.foe.valid && after.foe.hp > 0.0f);
    FroggyFeatureCheck("Combat preconditions target stays stable briefly", after.targetId == foeId);
    FroggyFeatureCheck("Combat preconditions hero agents readable after dwell", after.heroAgentsReadable);
}
