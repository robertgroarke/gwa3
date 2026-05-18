static void RunExplorableSkillbarRefreshProof() {
    FroggyFeatureReport("=== PHASE 5A: Explorable Skillbar Refresh ===");

    SkillbarSnapshot before = {};
    const bool beforeCaptured = CapturePlayerSkillbarSnapshot(before);
    FroggyFeatureCheck("Explorable skillbar readable before Froggy refresh", beforeCaptured);
    if (!beforeCaptured) {
        FroggyFeatureSkip("Explorable skillbar refresh proof", "Could not read player skillbar before refresh");
        return;
    }

    FroggyFeatureReport("  Skillbar before refresh: agent=%u nonZero=%d ids=[%u %u %u %u %u %u %u %u]",
              before.agentId,
              before.nonZeroSkills,
              before.skillIds[0], before.skillIds[1], before.skillIds[2], before.skillIds[3],
              before.skillIds[4], before.skillIds[5], before.skillIds[6], before.skillIds[7]);
    FroggyFeatureCheck("Explorable skillbar has non-zero skills before Froggy refresh", before.nonZeroSkills > 0);

    const bool refreshed = DungeonCombatRoutine::RefreshCombatSkillbarForDebug(Bot::Froggy::g_combatSession, "Froggy");
    FroggyFeatureCheck("Froggy combat skillbar refresh succeeds in explorable", refreshed);

    SkillbarSnapshot after = {};
    const bool afterCaptured = CapturePlayerSkillbarSnapshot(after);
    FroggyFeatureCheck("Explorable skillbar readable after Froggy refresh", afterCaptured);
    if (!afterCaptured) {
        FroggyFeatureSkip("Explorable skillbar refresh proof after refresh", "Could not read player skillbar after refresh");
        return;
    }

    FroggyFeatureReport("  Skillbar after refresh: agent=%u nonZero=%d ids=[%u %u %u %u %u %u %u %u]",
              after.agentId,
              after.nonZeroSkills,
              after.skillIds[0], after.skillIds[1], after.skillIds[2], after.skillIds[3],
              after.skillIds[4], after.skillIds[5], after.skillIds[6], after.skillIds[7]);

    FroggyFeatureCheck("Explorable skillbar agent id stable across refresh", before.agentId == after.agentId);
    FroggyFeatureCheck("Explorable skillbar remains populated after refresh", after.nonZeroSkills > 0);
    FroggyFeatureCheck("Explorable skillbar IDs stable across refresh",
             memcmp(before.skillIds, after.skillIds, sizeof(before.skillIds)) == 0);
}
