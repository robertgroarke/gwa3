struct SpiritChainObservation {
    bool sawSlot2 = false;
    bool sawFollowUpSpirit = false;
    int slot2Step = -1;
};

static bool IsSpiritTraceUseLine(const char* line) {
    if (!line || !line[0] || strstr(line, " USE ") == nullptr) return false;
    return strstr(line, "slot=2 ") != nullptr ||
           strstr(line, "slot=3 ") != nullptr ||
           strstr(line, "slot=4 ") != nullptr ||
           strstr(line, "slot=5 ") != nullptr;
}

static void RecordSpiritChainSlotUse(int step, int slot, SpiritChainObservation& observation) {
    if (slot == 2) {
        observation.sawSlot2 = true;
        if (observation.slot2Step < 0) observation.slot2Step = step;
    }
    if ((slot == 3 || slot == 4 || slot == 5) &&
        observation.slot2Step > 0 &&
        step >= observation.slot2Step) {
        observation.sawFollowUpSpirit = true;
    }
}

static void ObserveSpiritChainStep(int step, SpiritChainObservation& observation) {
    const auto info = Bot::Froggy::g_combatSession.last_action;
    if (info.valid && info.used_skill) {
        RecordSpiritChainSlotUse(step, info.slot, observation);
    }

    const int traceCount = DungeonCombatRoutine::GetDebugTraceCount(Bot::Froggy::g_combatSession);
    for (int i = 0; i < traceCount; ++i) {
        const char* line = DungeonCombatRoutine::GetDebugTraceLine(Bot::Froggy::g_combatSession, i);
        if (IsSpiritTraceUseLine(line)) {
            FroggyFeatureReport("  Spirit chain trace[%d.%d]: %s", step, i, line);
        }
        if (!line || strstr(line, " USE ") == nullptr) continue;
        if (strstr(line, "slot=2 ")) RecordSpiritChainSlotUse(step, 2, observation);
        if (strstr(line, "slot=3 ")) RecordSpiritChainSlotUse(step, 3, observation);
        if (strstr(line, "slot=4 ")) RecordSpiritChainSlotUse(step, 4, observation);
        if (strstr(line, "slot=5 ")) RecordSpiritChainSlotUse(step, 5, observation);
    }
}
