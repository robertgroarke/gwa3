static void RunReadOnlyCombatPreconditions(uint32_t foeId, const char* label) {
    FroggyFeatureReport("=== PHASE 5F: Read-only Combat Preconditions (%s) ===", label ? label : "default");

    CombatObservabilitySnapshot before = {};
    if (!CaptureReadOnlyCombatSnapshot(
            foeId,
            "Combat preconditions snapshot captured before dwell",
            "Read-only combat preconditions",
            "Could not capture initial combat snapshot",
            before)) {
        return;
    }

    AssertReadOnlyCombatSnapshotBeforeDwell(before);

    PartyInfo* playerParty = ResolveTestPlayerParty();
    AssertReadOnlyCombatHeroesAliveBeforeDwell(playerParty);

    SetReadOnlyCombatPreconditionTarget(foeId);
    Sleep(1000);

    CombatObservabilitySnapshot after = {};
    if (!CaptureReadOnlyCombatSnapshot(
            foeId,
            "Combat preconditions snapshot captured after dwell",
            "Read-only combat preconditions after dwell",
            "Could not capture follow-up combat snapshot",
            after)) {
        return;
    }

    AssertReadOnlyCombatSnapshotAfterDwell(after, foeId);
    AssertReadOnlyCombatHeroesAliveAfterDwell(playerParty);
}
