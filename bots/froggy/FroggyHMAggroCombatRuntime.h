// Froggy aggro-combat policy. Included by FroggyHM.cpp.

static void RecordFroggyAggroTargetStats(void* userData, uint32_t bestTarget) {
    auto* stats = static_cast<SparkflyTraversalCombatStats*>(userData);
    if (!stats) {
        return;
    }
    ++stats->quick_step_attempts;
    stats->last_target_id = bestTarget;
}

static void RecordFroggyAggroActionStats(
    void* userData,
    const DungeonCombatRoutine::SkillActionResult& action,
    uint32_t actionStart) {
    auto* stats = static_cast<SparkflyTraversalCombatStats*>(userData);
    if (!stats || !action.valid || action.started_at_ms < actionStart) {
        return;
    }
    if (action.used_skill) {
        ++stats->skill_steps;
    } else if (action.auto_attack) {
        ++stats->auto_attack_steps;
    }
}

static float ResolveFroggyAggroMaxAftercast(uint32_t mapId, void*) {
    return IsBogrootMapId(mapId) ? 1.5f : 3.0f;
}

static void FroggyAggroPostLoot(void* userData, float aggroRange, const char* reason) {
    LootAfterCombatSweep(aggroRange, userData ? "combat-step-stats" : (reason ? reason : "combat-step"));
}

static void FightEnemiesInAggro(float aggroRange, bool careful = false,
                                SparkflyTraversalCombatStats* stats = nullptr,
                                bool waitForSkillCompletion = true,
                                DWORD maxFightMs = 240000u) {
    DungeonCombat::SessionAggroFightProfile profile;
    profile.session = &s_combatSession;
    profile.wait_ms = &DungeonRuntime::WaitMs;
    profile.is_dead = &IsDead;
    profile.post_loot = &FroggyAggroPostLoot;
    profile.on_target = &RecordFroggyAggroTargetStats;
    profile.on_action = &RecordFroggyAggroActionStats;
    profile.resolve_max_aftercast = &ResolveFroggyAggroMaxAftercast;
    profile.user_data = stats;
    profile.log_prefix = "Froggy";

    DungeonCombat::AggroFightOptions options;
    options.careful = careful;
    options.wait_for_skill_completion = waitForSkillCompletion;
    options.max_fight_ms = maxFightMs;
    options.log_prefix = "Froggy";
    options.loot_reason = stats ? "combat-step-stats" : "combat-step";
    (void)DungeonCombat::FightEnemiesInAggroWithSession(aggroRange, profile, options);
}
