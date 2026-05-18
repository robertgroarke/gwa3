static void ReportBuiltinCombatDecisionDump(uint32_t foeId) {
    DungeonCombatRoutine::DumpBuiltinCombatDecision(Bot::Froggy::g_combatSession, foeId, "Froggy");
    const int dumpCount = DungeonCombatRoutine::GetDecisionDumpCount(Bot::Froggy::g_combatSession);
    for (int i = 0; i < dumpCount; ++i) {
        FroggyFeatureReport("  Builtin combat dump[%d]: %s",
                     i,
                     DungeonCombatRoutine::GetDecisionDumpLine(Bot::Froggy::g_combatSession, i));
    }
}

static void ReportBuiltinCombatTrace() {
    const int traceCount = DungeonCombatRoutine::GetDebugTraceCount(Bot::Froggy::g_combatSession);
    for (int i = 0; i < traceCount; ++i) {
        FroggyFeatureReport("  Builtin combat trace[%d]: %s",
                     i,
                     DungeonCombatRoutine::GetDebugTraceLine(Bot::Froggy::g_combatSession, i));
    }
}
