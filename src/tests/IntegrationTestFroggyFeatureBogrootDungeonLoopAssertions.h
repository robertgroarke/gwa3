static void AssertBogrootDungeonLoopTelemetry(
    const Bot::Froggy::DungeonLoopTelemetry& telemetry,
    bool returnedToSparkfly) {
    FroggyFeatureCheck("Bogroot loop reached level 2", telemetry.entered_lvl2 || telemetry.started_in_lvl2);
    FroggyFeatureCheck("Bogroot boss sequence started", telemetry.boss_started);
    FroggyFeatureCheck("Bogroot boss sequence completed", telemetry.boss_completed);
    FroggyFeatureCheck("Bogroot chest interaction attempted", telemetry.chest_attempts > 0);
    FroggyFeatureCheck("Bogroot chest opened", telemetry.chest_successes > 0);
    FroggyFeatureCheck("Bogroot reward dialog attempted", telemetry.reward_attempted);
    FroggyFeatureCheck("Bogroot reward dialog latched", telemetry.reward_dialog_latched || telemetry.last_dialog_id == GWA3::DialogIds::TekksWar::QUEST_REWARD);
    FroggyFeatureCheck("Bogroot loop returned to Sparkfly", returnedToSparkfly && telemetry.returned_to_sparkfly && telemetry.final_map_id == MapIds::SPARKFLY_SWAMP);
}
