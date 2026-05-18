static FroggyFeatureAbort RunFroggyTravelToGaddsPhase() {
    FroggyFeatureReport("=== PHASE 1: Travel to Gadd's Encampment ===");
    WaitForPlayerWorldReady(15000);
    CtoS::Initialize();

    if (MapMgr::GetMapId() != MapIds::GADDS_ENCAMPMENT) {
        FroggyFeatureReport("  Not at Gadd's (map=%u) - traveling...", MapMgr::GetMapId());
        MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);
        const bool arrived = WaitFor("MapID == 638", 60000, []() {
            return MapMgr::GetMapId() == MapIds::GADDS_ENCAMPMENT;
        });
        if (!arrived) {
            FroggyFeatureReport("  ABORT: Failed to travel to Gadd's Encampment");
            FroggyFeatureReport("=== FROGGY TESTS ABORTED (no outpost) ===");
            return {true, s_failed + 1};
        }
    }

    const bool agentReady = WaitFor("MyID > 0", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    if (!agentReady) {
        FroggyFeatureReport("  ABORT: Agent not ready after travel");
        return {true, s_failed + 1};
    }

    Sleep(3000);
    FroggyFeatureCheck("Phase 1: In Gadd's Encampment", MapMgr::GetMapId() == MapIds::GADDS_ENCAMPMENT);
    FroggyFeatureReport("  Waiting for game to stabilize...");
    WaitForStablePlayerState(10000);
    Sleep(5000);
    return {};
}
