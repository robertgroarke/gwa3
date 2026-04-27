// Froggy local-clear runtime helpers. Included by FroggyHM.cpp.

static void HoldForLocalClear(const char* label,
                              float waypointX,
                              float waypointY,
                              float fightRange,
                              uint32_t bestId,
                              SparkflyTraversalCombatStats* stats) {
    const auto policy = BuildFroggyLocalClearPolicy(label, fightRange, stats);
    const DWORD localClearStart = GetTickCount();
    int clearPasses = 0;

    while ((GetTickCount() - localClearStart) < policy.local_clear_budget_ms) {
        if (IsDead() || !MapMgr::GetIsMapLoaded() || PartyMgr::GetIsPartyDefeated()) {
            return;
        }

        const uint32_t nearbyBefore = DungeonCombat::CountLivingEnemiesInRange(policy.clear_range);
        if (nearbyBefore == 0) {
            const auto dwellCallbacks = MakeFroggyLocalClearDwellCallbacks();
            if (DungeonCombat::WaitForEnemyClearDwell(
                    policy.clear_range,
                    policy.quiet_dwell_ms,
                    policy.initial_dwell_timeout_ms,
                    dwellCallbacks,
                    DungeonCombat::LOCAL_CLEAR_DWELL_POLL_MS)) {
                LootAfterCombatSweep(policy.clear_range, policy.loot_reason);
                return;
            }
        }

        ++clearPasses;
        if (stats) {
            ++stats->settle_requests;
            stats->last_target_id = bestId;
        }

        Log::Info("Froggy: %s local clear pass=%d target=%u waypoint=(%.0f, %.0f) clearRange=%.0f nearbyBefore=%u",
                  policy.clear_label,
                  clearPasses,
                  bestId,
                  waypointX,
                  waypointY,
                  policy.clear_range,
                  nearbyBefore);

        AgentMgr::CancelAction();
        WaitMs(DungeonCombat::LOCAL_CLEAR_PRE_FIGHT_CANCEL_DWELL_MS);
        FightEnemiesInAggro(policy.clear_range, false, stats, true, policy.fight_budget_ms);
        AgentMgr::CancelAction();
        WaitMs(DungeonCombat::LOCAL_CLEAR_POST_FIGHT_CANCEL_DWELL_MS);

        const uint32_t nearbyAfter = DungeonCombat::CountLivingEnemiesInRange(policy.clear_range);
        const float nearestAfter =
            DungeonCombat::GetNearestLivingEnemyDistance(
                policy.clear_range + DungeonCombat::LOCAL_CLEAR_NEAREST_ENEMY_SCAN_PADDING);
        Log::Info("Froggy: %s local clear result pass=%d target=%u nearbyAfter=%u nearestAfter=%.0f",
                  policy.clear_label,
                  clearPasses,
                  bestId,
                  nearbyAfter,
                  nearestAfter);

        if (nearbyAfter == 0 &&
            nearestAfter > (policy.clear_range + DungeonCombat::LOCAL_CLEAR_EXIT_DISTANCE_PADDING)) {
            LootAfterCombatSweep(policy.clear_range, policy.loot_reason);
            return;
        }

        const auto dwellCallbacks = MakeFroggyLocalClearDwellCallbacks();
        if (DungeonCombat::WaitForEnemyClearDwell(
                policy.clear_range,
                policy.quiet_dwell_ms,
                policy.settle_dwell_timeout_ms,
                dwellCallbacks,
                DungeonCombat::LOCAL_CLEAR_DWELL_POLL_MS)) {
            LootAfterCombatSweep(policy.clear_range, policy.loot_reason);
            return;
        }

        if (policy.single_pass && clearPasses >= policy.max_clear_passes) {
            Log::Info("Froggy: %s local clear early-exit target=%u waypoint=(%.0f, %.0f) nearby=%u nearest=%.0f budget=%lums",
                      policy.clear_label,
                      bestId,
                      waypointX,
                      waypointY,
                      DungeonCombat::CountLivingEnemiesInRange(policy.clear_range),
                      DungeonCombat::GetNearestLivingEnemyDistance(
                          policy.clear_range + DungeonCombat::LOCAL_CLEAR_NEAREST_ENEMY_SCAN_PADDING),
                      static_cast<unsigned long>(GetTickCount() - localClearStart));
            LootAfterCombatSweep(policy.clear_range, policy.loot_reason);
            return;
        }
    }

    Log::Warn("Froggy: %s local clear timeout target=%u waypoint=(%.0f, %.0f) nearby=%u",
              policy.clear_label,
              bestId,
              waypointX,
              waypointY,
              DungeonCombat::CountLivingEnemiesInRange(DungeonCombat::ComputeLocalClearRange(fightRange)));
}

static void HoldSparkflyForLocalClear(float waypointX,
                                      float waypointY,
                                      float fightRange,
                                      uint32_t bestId,
                                      SparkflyTraversalCombatStats* stats) {
    HoldForLocalClear("Sparkfly", waypointX, waypointY, fightRange, bestId, stats);
}

