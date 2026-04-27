// Builtin combat decision dump helpers for Froggy debug probes.

static void DumpUnavailableBuiltinCombatContext(AgentLiving* me, Agent* target, Skillbar* bar) {
    AddBuiltinCombatDumpLine("unavailable me=%d target=%d bar=%d",
                             me ? 1 : 0, target ? 1 : 0, bar ? 1 : 0);
    LogBot("BuiltinCombatDump: unavailable me=%d target=%d bar=%d",
           me ? 1 : 0, target ? 1 : 0, bar ? 1 : 0);
}

static void DumpBuiltinCombatTargetHeader(uint32_t targetId, AgentLiving* me, AgentLiving* target) {
    const float myEnergy = me->energy * me->max_energy;
    const float distance = AgentMgr::GetDistance(me->x, me->y, target->x, target->y);
    AddBuiltinCombatDumpLine("target=%u distance=%.0f hp=%.3f energy=%.1f activeSkill=%u modelState=0x%X",
                             targetId, distance, target->hp, myEnergy, me->skill, me->model_state);
    LogBot("BuiltinCombatDump: target=%u distance=%.0f hp=%.3f energy=%.1f activeSkill=%u modelState=0x%X",
           targetId, distance, target->hp, myEnergy, me->skill, me->model_state);
}

static void DumpBuiltinCombatSkillSlot(int slotIndex, const CachedSkill& skill, uint32_t targetId) {
    const int displaySlot = slotIndex + 1;
    if (skill.skill_id == 0) {
        AddBuiltinCombatDumpLine("slot=%d empty", displaySlot);
        LogBot("BuiltinCombatDump: slot=%d empty", displaySlot);
        return;
    }

    const auto inspection = DungeonCombatRoutine::InspectSkillCandidate(
        skill,
        slotIndex,
        targetId,
        DungeonSkill::ROLE_OFFENSIVE | DungeonSkill::ROLE_ATTACK);
    const char* canUseReason = ExplainCanUseSkillFailure(skill, targetId);
    AddBuiltinCombatDumpLine("slot=%d skill=%u roles=0x%X match=%d recharge=%u ready=%d e=%u/%0.1f adren=%u/%u canCast=%d canUse=%d castReason=%s useReason=%s target=%u tgtType=%u type=%u",
                             displaySlot, skill.skill_id, skill.roles, inspection.role_match ? 1 : 0,
                             inspection.recharge, inspection.recharge_ready ? 1 : 0,
                             skill.energy_cost, inspection.current_energy,
                             inspection.adrenaline_current, inspection.adrenaline_required,
                             inspection.can_cast ? 1 : 0, inspection.can_use ? 1 : 0,
                             inspection.can_cast_reason ? inspection.can_cast_reason : "ok",
                             canUseReason ? canUseReason : "ok",
                             inspection.resolved_target, skill.target_type, skill.skill_type);
    LogBot("BuiltinCombatDump: slot=%d skill=%u roles=0x%X roleMatch=%d recharge=%u ready=%d energyCost=%u energyReady=%d canUse=%d resolvedTarget=%u targetType=%u type=%u",
           displaySlot, skill.skill_id, skill.roles, inspection.role_match ? 1 : 0,
           inspection.recharge, inspection.recharge_ready ? 1 : 0,
           skill.energy_cost, inspection.energy_ready ? 1 : 0, inspection.can_use ? 1 : 0,
           inspection.resolved_target, skill.target_type, skill.skill_type);
}

void DebugDumpBuiltinCombatDecision(uint32_t targetId) {
    ResetBuiltinCombatDump();
    auto* me = AgentMgr::GetMyAgent();
    auto* target = AgentMgr::GetAgentByID(targetId);
    auto* bar = SkillMgr::GetPlayerSkillbar();
    if (!me || !target || target->type != 0xDB || !bar) {
        DumpUnavailableBuiltinCombatContext(me, target, bar);
        return;
    }

    if (!s_combatSession.skills_cached) {
        RefreshFroggySkillCache();
    }

    DumpBuiltinCombatTargetHeader(targetId, me, static_cast<AgentLiving*>(target));

    for (int i = 0; i < 8; ++i) {
        DumpBuiltinCombatSkillSlot(i, s_combatSession.skill_cache[i], targetId);
    }
}

int GetBuiltinCombatDecisionDumpCount() {
    return DungeonCombatRoutine::GetDecisionDumpCount(s_combatSession);
}

const char* GetBuiltinCombatDecisionDumpLine(int index) {
    return DungeonCombatRoutine::GetDecisionDumpLine(s_combatSession, index);
}

int GetCombatDebugTraceCount() {
    return DungeonCombatRoutine::GetDebugTraceCount(s_combatSession);
}

const char* GetCombatDebugTraceLine(int index) {
    return DungeonCombatRoutine::GetDebugTraceLine(s_combatSession, index);
}
