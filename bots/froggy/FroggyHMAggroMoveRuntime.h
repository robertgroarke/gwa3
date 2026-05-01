// Froggy movement and aggro traversal runtime helpers. Included by FroggyHM.cpp.

static bool MoveToAndWait(float x, float y, float threshold = DungeonNavigation::MOVE_TO_DEFAULT_THRESHOLD) {
    DungeonNavigation::LoggedMoveOptions options;
    options.log_prefix = "Froggy";
    options.is_dead = &IsDead;
    return DungeonNavigation::MoveToAndWaitLogged(x, y, threshold, options);
}

static void FroggyFightEnemiesInAggroForMove(
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

static void FroggyHoldSpecialLocalClear(
    float x,
    float y,
    float fightRange,
    uint32_t targetId,
    void* stats) {
    HoldSparkflyForLocalClear(
        x,
        y,
        fightRange,
        targetId,
        static_cast<SparkflyTraversalCombatStats*>(stats));
}

static void FroggyHoldLocalClear(
    const char* label,
    float x,
    float y,
    float fightRange,
    uint32_t targetId,
    void* stats) {
    HoldForLocalClear(
        label,
        x,
        y,
        fightRange,
        targetId,
        static_cast<SparkflyTraversalCombatStats*>(stats));
}

static void AggroMoveToEx(float x, float y, float fightRange = DungeonCombat::AGGRO_DEFAULT_FIGHT_RANGE) {
    const bool sparkflyMap = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    const bool bogrootMap = IsBogrootMapId(MapMgr::GetMapId());

    DungeonNavigation::AggroMoveProfileConfig config;
    config.is_dead = &IsDead;
    config.is_map_loaded = &IsMapLoaded;
    config.wait_ms = &DungeonRuntime::WaitMs;
    config.fight_in_aggro = &FroggyFightEnemiesInAggroForMove;
    config.hold_local_clear = &FroggyHoldLocalClear;
    config.hold_special_local_clear = &FroggyHoldSpecialLocalClear;
    config.pickup_nearby_loot = &PickupNearbyLoot;
    config.special_stats = &s_sparkflyTraversalCombatStats;
    config.profile = bogrootMap
        ? DungeonNavigation::AggroMoveProfile::Opportunistic
        : DungeonNavigation::AggroMoveProfile::Standard;
    config.exact_move_target = sparkflyMap;
    config.use_special_local_clear = sparkflyMap;
    config.use_local_clear_cooldown = false;
    config.log_prefix = "Froggy";
    config.sidestep_random_radius = AGGRO_BOGROOT_SIDESTEP_RANDOM_RADIUS;
    config.opportunistic_fight_budget_ms = AGGRO_BOGROOT_FIGHT_BUDGET_MS;
    config.opportunistic_loot_radius = AGGRO_BOGROOT_LOOT_RADIUS;
    DungeonNavigation::AggroMoveToConfigured(x, y, fightRange, config);
}
