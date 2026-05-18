static bool ResolveSyntheticCombatTarget(uint32_t role, uint32_t foeId, uint32_t& targetId, const char* branchLabel) {
    if (DungeonCombatRoutine::ResolveSyntheticSkillTarget(role, 5u, foeId, targetId)) return true;

    FroggyFeatureSkip(branchLabel, "Synthetic target resolver unavailable");
    return false;
}
