static void RunCombatProofsForFoe(uint32_t foeId, const char* label) {
    RunCombatObservabilityHarness(foeId, label);
    RunCombatAgentReadValidation(foeId, label);
    RunCombatEffectReadValidation(foeId, label);
    if (s_enableInvasiveSparkflyCombatProofs) {
        RunCombatTargetAndCastTelemetryValidation(foeId, label);
    } else {
        FroggyFeatureSkip("Combat target/cast telemetry validation", "Deferred in full Froggy loop to preserve Sparkfly route stability");
    }
    RunReadOnlyCombatPreconditions(foeId, label);
    if (s_enableInvasiveSparkflyCombatProofs) {
        RunBuiltinCombatProofSuite(foeId);
    } else {
        SkipInvasiveCombatProofSuite("Deferred in full Froggy loop to preserve Sparkfly route stability");
    }
}

static bool RunIsolatedExplorableFlaggingProof(const char* checkLabel, const char* skipLabel) {
    if (!s_isolatedExplorableFlaggingMode) {
        FroggyFeatureSkip(skipLabel, "Covered by isolated explorable flagging test mode");
        return false;
    }

    auto* meExplorable = AgentMgr::GetMyAgent();
    if (!meExplorable || meExplorable->hp <= 0.0f) {
        FroggyFeatureSkip(skipLabel, "Player agent unavailable");
        return false;
    }

    PartyMgr::FlagAll(meExplorable->x + 100.0f, meExplorable->y + 100.0f);
    Sleep(200);
    PartyMgr::UnflagAll();
    Sleep(500);
    AgentMgr::CancelAction();
    Sleep(250);
    FroggyFeatureCheck(checkLabel, AgentMgr::GetMyAgent() != nullptr);
    FroggyFeatureReport("  Isolated explorable flagging mode: stopping after validation");
    return true;
}
