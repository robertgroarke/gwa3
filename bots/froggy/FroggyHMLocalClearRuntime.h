// Froggy local-clear runtime helpers. Included by FroggyHM.cpp.

static void HoldForLocalClear(const char* label,
                              float waypointX,
                              float waypointY,
                              float fightRange,
                              uint32_t bestId,
                              SparkflyTraversalCombatStats* stats) {
    const auto policy = BuildFroggyLocalClearPolicy(label, fightRange, stats);
    DungeonCombat::HoldLocalClearOptions options = {};
    options.target_id = bestId;
    options.log_prefix = "Froggy";
    (void)DungeonCombat::HoldForLocalClear(
        waypointX,
        waypointY,
        fightRange,
        policy,
        MakeFroggyLocalClearCallbacks(stats),
        options);
}

static void HoldSparkflyForLocalClear(float waypointX,
                                      float waypointY,
                                      float fightRange,
                                      uint32_t bestId,
                                      SparkflyTraversalCombatStats* stats) {
    HoldForLocalClear("Sparkfly", waypointX, waypointY, fightRange, bestId, stats);
}

