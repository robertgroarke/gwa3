static bool CaptureCombatCastTelemetrySnapshot(const SkillTestCandidate& candidate, CombatCastTelemetrySnapshot& out) {
    out = {};
    Skillbar* bar = SkillMgr::GetPlayerSkillbar();
    AgentLiving* me = GetAgentLivingRaw(ReadMyId());
    if (!bar || !me || candidate.slot == 0 || candidate.slot > 8) return false;
    __try {
        const SkillbarSkill& sb = bar->skills[candidate.slot - 1];
        out.valid = true;
        out.candidate = candidate;
        out.targetId = AgentMgr::GetTargetId();
        out.energy = GetCurrentEnergyPoints();
        out.activeSkill = me->skill;
        out.recharge = sb.recharge;
        out.event = sb.event;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}

static bool CombatCastTelemetryChanged(const SkillTestCandidate& candidate,
                                       const CombatCastTelemetrySnapshot& before,
                                       const CombatCastTelemetrySnapshot& after) {
    return after.recharge != before.recharge ||
           after.event != before.event ||
           after.activeSkill != before.activeSkill ||
           after.activeSkill == candidate.skillId ||
           after.energy < before.energy;
}
