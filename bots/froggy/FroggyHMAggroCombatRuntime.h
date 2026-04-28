// Froggy aggro-combat adapters. Included by FroggyHM.cpp.

static void RecordFroggyAggroTargetStats(
    void* userData,
    uint32_t bestTarget) {
    auto* stats = static_cast<SparkflyTraversalCombatStats*>(userData);
    DungeonCombatRoutine::ResetUsedSkills(s_combatSession);
    if (!stats) {
        return;
    }
    ++stats->quick_step_attempts;
    stats->last_target_id = bestTarget;
}

static void RecordFallbackAutoAttackStep(void*, uint32_t targetId, uint32_t actionStart) {
    DungeonCombatRoutine::SetLastActionDescription(s_combatSession, "auto_attack target=%u", targetId);
    DungeonCombatRoutine::ResetLastAction(s_combatSession);
    DungeonCombatRoutine::ApplyLastAction(s_combatSession, DungeonCombatRoutine::MakeAutoAttackActionResult(
        targetId,
        DungeonSkill::ROLE_ATTACK | DungeonSkill::ROLE_OFFENSIVE,
        actionStart));
    DungeonCombatRoutine::FinishLastAction(s_combatSession);
}

static void RecordFroggyAggroActionStats(void* userData, uint32_t actionStart) {
    auto* stats = static_cast<SparkflyTraversalCombatStats*>(userData);
    const auto stepInfo = GetLastCombatStepInfo();
    if (!stats || !stepInfo.valid || stepInfo.started_at_ms < actionStart) {
        return;
    }
    if (stepInfo.used_skill) {
        ++stats->skill_steps;
    } else if (stepInfo.auto_attack) {
        ++stats->auto_attack_steps;
    }
}

static int UseFroggySkillsInAggro(uint32_t targetId, float aggroRange, bool waitForSkillCompletion) {
    DungeonCombatRoutine::SlotOrderUseOptions options;
    options.wait_for_completion = waitForSkillCompletion;
    options.aggro_range = aggroRange;
    options.skill_use.timing = DungeonCombatRoutine::MakeSkillCastTimingOptions(
        "Froggy",
        IsBogrootMapId(MapMgr::GetMapId()) ? 1.5f : 3.0f);
    options.skill_use.log_prefix = "Froggy";
    return DungeonCombatRoutine::UseSkillsInSlotOrderTracked(
        s_combatSession,
        targetId,
        &DungeonRuntime::WaitMs,
        &IsDead,
        options);
}

static void FroggyAggroPostLoot(void* userData, float aggroRange, const char* reason) {
    LootAfterCombatSweep(aggroRange, userData ? "combat-step-stats" : (reason ? reason : "combat-step"));
}

static void FightEnemiesInAggro(float aggroRange, bool careful = false,
                                SparkflyTraversalCombatStats* stats = nullptr,
                                bool waitForSkillCompletion = true,
                                DWORD maxFightMs = 240000u) {
    DungeonCombat::AggroFightCallbacks callbacks;
    callbacks.is_dead = &IsDead;
    callbacks.wait_ms = &DungeonRuntime::WaitMs;
    callbacks.use_skills = &UseFroggySkillsInAggro;
    callbacks.record_target = &RecordFroggyAggroTargetStats;
    callbacks.record_auto_attack = &RecordFallbackAutoAttackStep;
    callbacks.record_action = &RecordFroggyAggroActionStats;
    callbacks.post_loot = &FroggyAggroPostLoot;
    callbacks.user_data = stats;

    DungeonCombat::AggroFightOptions options;
    options.careful = careful;
    options.wait_for_skill_completion = waitForSkillCompletion;
    options.max_fight_ms = maxFightMs;
    options.restricted_skill_override_map_id = MapIds::SPARKFLY_SWAMP;
    options.log_prefix = "Froggy";
    options.loot_reason = stats ? "combat-step-stats" : "combat-step";
    (void)DungeonCombat::FightEnemiesInAggro(aggroRange, callbacks, options);
}
