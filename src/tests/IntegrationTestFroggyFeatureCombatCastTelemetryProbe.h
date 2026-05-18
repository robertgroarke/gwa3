static bool TriggerCombatCastTelemetryProbe(const SkillTestCandidate& candidate,
                                            const CombatCastTelemetrySnapshot& before) {
    SkillMgr::ResetRestrictedMapPlayerUseSkillCount();
    SkillMgr::SetRestrictedMapPlayerUseSkillOverride(true);
    SkillMgr::UseSkill(candidate.slot, candidate.targetId, 0);

    const bool telemetryChanged = WaitFor("Combat cast telemetry changes after UseSkill", 5000, [candidate, before]() {
        CombatCastTelemetrySnapshot after = {};
        if (!CaptureCombatCastTelemetrySnapshot(candidate, after)) return false;
        return CombatCastTelemetryChanged(candidate, before, after);
    });
    SkillMgr::SetRestrictedMapPlayerUseSkillOverride(false);
    return telemetryChanged;
}
