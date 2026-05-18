static bool RunBogrootDungeonLoopProof() {
    FroggyFeatureReport("=== PHASE 6C: Complete Bogroot Dungeon Loop ===");

    if (!PrepareBogrootDungeonLoopProof()) {
        return false;
    }

    const bool returnedToSparkfly = Bot::Froggy::RunDungeonLoopFromCurrentMap();
    const Bot::Froggy::DungeonLoopTelemetry telemetry = Bot::Froggy::g_dungeonLoopTelemetry;

    ReportBogrootDungeonLoopTelemetry(telemetry);
    AssertBogrootDungeonLoopTelemetry(telemetry, returnedToSparkfly);
    StabilizeAfterBogrootDungeonLoopReturn(telemetry, returnedToSparkfly);

    return returnedToSparkfly && telemetry.final_map_id == MapIds::SPARKFLY_SWAMP;
}
