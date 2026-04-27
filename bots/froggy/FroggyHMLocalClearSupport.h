// Froggy local-clear policy and dwell helpers.

static DungeonCombat::LocalClearPolicy BuildFroggyLocalClearPolicy(
    const char* label,
    float fightRange,
    SparkflyTraversalCombatStats* stats) {
    const auto profile = IsBogrootMap(MapMgr::GetMapId())
        ? DungeonCombat::LocalClearProfile::ShortTraversal
        : DungeonCombat::LocalClearProfile::StandardTraversal;
    return DungeonCombat::BuildLocalClearPolicy(profile, label, fightRange, stats != nullptr);
}

static DungeonCombat::CombatCallbacks MakeFroggyLocalClearDwellCallbacks() {
    DungeonCombat::CombatCallbacks callbacks = {};
    callbacks.is_dead = &IsDead;
    callbacks.is_map_loaded = &IsMapLoaded;
    callbacks.wait_ms = &DungeonRuntime::WaitMs;
    return callbacks;
}
