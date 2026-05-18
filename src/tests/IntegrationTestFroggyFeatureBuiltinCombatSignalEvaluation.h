static bool IsBuiltinCombatActionChosen(const char* actionDesc) {
    return actionDesc && actionDesc[0] != '\0' &&
           strncmp(actionDesc, "uninitialized", 13) != 0 &&
           strncmp(actionDesc, "no_action", 9) != 0;
}

static BuiltinCombatSignalSummary BuildBuiltinCombatSignalSummary(
    const CombatObservabilitySnapshot& before,
    const CombatObservabilitySnapshot& after,
    uint32_t foeId) {
    BuiltinCombatSignalSummary summary;
    for (int i = 0; i < 8; ++i) {
        if (after.skillbar.recharge[i] != before.skillbar.recharge[i]) {
            summary.rechargeChanged = true;
            summary.rechargeSlot = i + 1;
            break;
        }
    }
    summary.activeSkillChanged = after.player.castingSkill != before.player.castingSkill;
    summary.energyChanged = after.player.maxEnergy == before.player.maxEnergy &&
                            after.player.energy != before.player.energy;
    summary.foeHpChanged = after.foe.valid && before.foe.valid && after.foe.hp != before.foe.hp;
    summary.distanceBefore = AgentMgr::GetDistance(before.player.x, before.player.y, before.foe.x, before.foe.y);
    summary.distanceAfter = AgentMgr::GetDistance(after.player.x, after.player.y, after.foe.x, after.foe.y);
    summary.distanceClosed = after.targetId == foeId && summary.distanceAfter + 100.0f < summary.distanceBefore;
    return summary;
}

static bool IsBuiltinCombatSignalValid(const BuiltinCombatSignalSummary& signal, bool isAutoAttack) {
    return isAutoAttack
        ? (signal.foeHpChanged || signal.distanceClosed)
        : (signal.rechargeChanged || signal.activeSkillChanged || signal.energyChanged || signal.foeHpChanged);
}
