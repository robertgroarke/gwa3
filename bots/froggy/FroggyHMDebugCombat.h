// Froggy combat debug adapters. Shared combat behavior lives in DungeonCombatRoutine.

bool ExecuteBuiltinCombatStep(uint32_t targetId, bool quickStep) {
    auto* target = AgentMgr::GetAgentByID(targetId);
    if (!target || target->type != 0xDBu) {
        return false;
    }

    auto& cfg = Bot::GetConfig();
    const CombatMode originalMode = cfg.combat_mode;
    cfg.combat_mode = CombatMode::Builtin;

    DungeonCombatRoutine::DebugCombatStepOptions options = {};
    options.quick_step = quickStep;
    options.prefer_first_foe_target_skill = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    options.aggro_range = DungeonCombat::LONG_BOW_RANGE;
    options.max_aftercast = IsBogrootMapId(MapMgr::GetMapId()) ? 1.5f : 3.0f;
    options.auto_attack = &AgentMgr::Attack;
    options.restricted_skill_override_map_id = MapIds::SPARKFLY_SWAMP;
    options.log_prefix = "Froggy";
    const bool executed = DungeonCombatRoutine::ExecuteBuiltinDebugCombatStep(
        s_combatSession,
        targetId,
        &DungeonRuntime::WaitMs,
        &IsDead,
        options);

    cfg.combat_mode = originalMode;
    return executed;
}

const char* GetLastCombatStepDescription() {
    return s_combatSession.last_step;
}

LastCombatStepInfo GetLastCombatStepInfo() {
    return s_combatSession.last_action;
}

void ResetSparkflyTraversalCombatStats() {
    s_sparkflyTraversalCombatStats = {};
}

SparkflyTraversalCombatStats GetSparkflyTraversalCombatStats() {
    return s_sparkflyTraversalCombatStats;
}

bool DebugResolveSyntheticSkillTarget(uint32_t roleMask, uint8_t targetType,
                                      uint32_t defaultFoeId, uint32_t& outTargetId) {
    return DungeonCombatRoutine::ResolveSyntheticSkillTarget(
        roleMask,
        targetType,
        defaultFoeId,
        outTargetId);
}

bool DebugResolveUsableSkillTargetForSlot(uint32_t slot, uint32_t defaultFoeId,
                                          uint32_t& outSkillId, uint32_t& outTargetId, uint8_t& outTargetType) {
    return DungeonCombatRoutine::ResolveUsableSkillTargetForSlot(
        s_combatSession,
        slot,
        defaultFoeId,
        outSkillId,
        outTargetId,
        outTargetType,
        "Froggy");
}

uint32_t DebugGetCastingEnemy() {
    return DungeonSkill::GetCastingBalledEnemy();
}

uint32_t DebugGetEnchantedEnemy() {
    return DungeonSkill::GetEnchantedBalledEnemy();
}

uint32_t DebugGetMeleeRangeEnemy() {
    return DungeonSkill::GetMeleeBalledEnemy();
}

bool RefreshCombatSkillbar() {
    return DungeonCombatRoutine::RefreshCombatSkillbarForDebug(s_combatSession, "Froggy");
}

void DebugDumpBuiltinCombatDecision(uint32_t targetId) {
    DungeonCombatRoutine::DumpBuiltinCombatDecision(s_combatSession, targetId, "Froggy");
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
