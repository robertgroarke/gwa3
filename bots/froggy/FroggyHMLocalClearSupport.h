// Froggy local-clear policy and dwell helpers.

static DungeonCombat::LocalClearPolicy BuildFroggyLocalClearPolicy(
    const char* label,
    float fightRange,
    SparkflyTraversalCombatStats* stats) {
    const auto profile = IsBogrootMapId(MapMgr::GetMapId())
        ? DungeonCombat::LocalClearProfile::ShortTraversal
        : DungeonCombat::LocalClearProfile::StandardTraversal;
    return DungeonCombat::BuildLocalClearPolicy(profile, label, fightRange, stats != nullptr);
}

static void RecordFroggyLocalClearPass(void* userData, int, uint32_t targetId) {
    auto* stats = static_cast<SparkflyTraversalCombatStats*>(userData);
    if (!stats) return;
    ++stats->settle_requests;
    stats->last_target_id = targetId;
}

static void FroggyLocalClearPostLoot(void*, float aggroRange, const char* reason) {
    LootAfterCombatSweep(aggroRange, reason);
}

static void FroggyLocalClearFightInAggro(
    float aggroRange,
    bool careful,
    void* stats,
    bool waitForSkillCompletion,
    uint32_t maxFightMs) {
    FightEnemiesInAggro(
        aggroRange,
        careful,
        static_cast<SparkflyTraversalCombatStats*>(stats),
        waitForSkillCompletion,
        maxFightMs);
}

static DungeonCombat::HoldLocalClearCallbacks MakeFroggyLocalClearCallbacks(
    SparkflyTraversalCombatStats* stats) {
    DungeonCombat::HoldLocalClearCallbacks callbacks = {};
    callbacks.is_dead = &IsDead;
    callbacks.is_map_loaded = &IsMapLoaded;
    callbacks.wait_ms = &DungeonRuntime::WaitMs;
    callbacks.fight_in_aggro = &FroggyLocalClearFightInAggro;
    callbacks.post_loot = &FroggyLocalClearPostLoot;
    callbacks.on_clear_pass = &RecordFroggyLocalClearPass;
    callbacks.user_data = stats;
    return callbacks;
}
