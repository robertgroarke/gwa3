static void StabilizeAfterBogrootDungeonLoopReturn(
    const Bot::Froggy::DungeonLoopTelemetry& telemetry,
    bool returnedToSparkfly) {
    if (!returnedToSparkfly || telemetry.final_map_id != MapIds::SPARKFLY_SWAMP) {
        return;
    }

    const bool agentReady = WaitFor("MyID after Bogroot return", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    FroggyFeatureCheck("Sparkfly agent ready after Bogroot return", agentReady);
    if (agentReady) {
        WaitForStablePlayerState(10000);
        Sleep(2000);
    }
}
