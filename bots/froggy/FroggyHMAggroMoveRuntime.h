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
    LogBot("AggroMoveToEx start target=(%.0f, %.0f) fightRange=%.0f", x, y, fightRange);
    const bool sparkflyMap = MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    const bool bogrootMap = IsBogrootMapId(MapMgr::GetMapId());

    DungeonNavigation::AggroMoveCallbacks callbacks;
    callbacks.is_dead = &IsDead;
    callbacks.is_map_loaded = &IsMapLoaded;
    callbacks.wait_ms = &DungeonRuntime::WaitMs;
    callbacks.fight_in_aggro = &FroggyFightEnemiesInAggroForMove;
    callbacks.hold_local_clear = &FroggyHoldLocalClear;
    callbacks.hold_special_local_clear = sparkflyMap ? &FroggyHoldSpecialLocalClear : nullptr;
    callbacks.pickup_nearby_loot = &PickupNearbyLoot;
    callbacks.special_stats = &s_sparkflyTraversalCombatStats;

    DungeonNavigation::AggroMoveOptions options;
    options.profile = bogrootMap
        ? DungeonNavigation::AggroMoveProfile::Opportunistic
        : DungeonNavigation::AggroMoveProfile::Standard;
    options.exact_move_target = sparkflyMap;
    options.use_local_clear_cooldown = false;
    options.log_prefix = "Froggy";
    options.sidestep_random_radius = AGGRO_BOGROOT_SIDESTEP_RANDOM_RADIUS;
    options.opportunistic_fight_budget_ms = AGGRO_BOGROOT_FIGHT_BUDGET_MS;
    options.opportunistic_loot_radius = AGGRO_BOGROOT_LOOT_RADIUS;
    DungeonNavigation::AggroMoveTo(x, y, fightRange, callbacks, options);
}
