static void AssertCombatCastAftercastPacing(const Bot::Froggy::LastCombatStepInfo& info) {
    if (info.expected_aftercast_ms > 0 && info.finished_at_ms >= info.started_at_ms) {
        const uint32_t observedDurationMs = info.finished_at_ms - info.started_at_ms;
        FroggyFeatureCheck("Aftercast pacing observed at or beyond expected delay",
                 observedDurationMs + 50 >= info.expected_aftercast_ms);
    } else {
        FroggyFeatureSkip("Aftercast pacing assertion",
                "Chosen skill did not expose a positive expected aftercast duration");
    }
}
