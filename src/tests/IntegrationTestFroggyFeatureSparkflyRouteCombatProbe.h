static void RunSparkflyRouteCombatProbe() {
    FroggyFeatureReport("=== SPARKFLY ROUTE TEST: Combat Probe ===");
    FroggyFeatureCheck("Combat cache refresh",
                DungeonCombatRoutine::RefreshCombatSkillbarForDebug(Bot::Froggy::g_combatSession, "Froggy"));
    if (s_preferDirectTekksStagingForDebug) {
        FroggyFeatureSkip("Found Sparkfly foe", "Direct Tekks debug staging skips the route-side combat probe");
        FroggyFeatureSkip("Combat observability harness", "Direct Tekks debug staging skips the route-side combat probe");
        return;
    }

    uint32_t foeId = FindNearestFoe(5000.0f);
    if (!foeId) {
        FroggyFeatureReport("  No nearby foe at spawn; moving toward first route leg...");
        MovePlayerNear(-4559.0f, -14406.0f, 500.0f, 25000);
        foeId = FindNearestFoe(5000.0f);
    }

    if (foeId) {
        FroggyFeatureCheck("Found Sparkfly foe", true);
        AgentMgr::ChangeTarget(foeId);
        Sleep(750);
        RunCombatObservabilityHarness(foeId, "sparkfly route foe");
    } else {
        FroggyFeatureSkip("Found Sparkfly foe", "No nearby foe before route start");
        FroggyFeatureSkip("Combat observability harness", "Skipped because no Sparkfly foe was available at route start");
    }
}

static void ReportSparkflyTraversalStats() {
    const auto stats = Bot::Froggy::g_sparkflyTraversalCombatStats;
    FroggyFeatureReport("  Sparkfly traversal combat stats: attempts=%u skill_steps=%u auto_attacks=%u settle_requests=%u unsettled=%u last_target=%u",
              stats.quick_step_attempts,
              stats.skill_steps,
              stats.auto_attack_steps,
              stats.settle_requests,
              stats.unsettled_skips,
              stats.last_target_id);
    if (s_preferDirectTekksStagingForDebug) {
        FroggyFeatureSkip("Sparkfly route exercised aggro combat",
                "Direct Tekks debug staging bypasses Froggy's aggro traversal combat");
        FroggyFeatureSkip("Sparkfly route recorded player skill or attack steps",
                "Direct Tekks debug staging bypasses Froggy's aggro traversal combat");
    } else {
        FroggyFeatureCheck("Sparkfly route exercised aggro combat",
                 stats.quick_step_attempts > 0 || stats.skill_steps > 0 || stats.auto_attack_steps > 0);
        FroggyFeatureCheck("Sparkfly route recorded player skill or attack steps",
                 (stats.skill_steps + stats.auto_attack_steps) > 0);
    }
}
