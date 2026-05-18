static bool BootstrapSparkflyRouteTestToGadds() {
    FroggyFeatureReport("=== SPARKFLY ROUTE TEST: Bootstrap ===");
    WaitForPlayerWorldReady(15000);
    CtoS::Initialize();

    if (MapMgr::GetMapId() != MapIds::GADDS_ENCAMPMENT) {
        FroggyFeatureReport("  Traveling to Gadd's from map %u...", MapMgr::GetMapId());
        MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);
        const bool arrived = WaitFor("MapID == Gadd's", 60000, []() {
            return MapMgr::GetMapId() == MapIds::GADDS_ENCAMPMENT;
        });
        FroggyFeatureCheck("Travel to Gadd's", arrived);
        if (!arrived) return false;
    }

    const bool agentReady = WaitFor("MyID > 0", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    FroggyFeatureCheck("Agent ready", agentReady);
    if (!agentReady) return false;

    WaitForStablePlayerState(10000);
    Sleep(3000);
    return true;
}

static bool SetupSparkflyRouteOutpost() {
    FroggyFeatureReport("=== SPARKFLY ROUTE TEST: Outpost Hero Setup ===");
    const bool standardHeroesReady = SetupHeroesFromTemplateForOutpost("Standard.txt", "Standard");
    FroggyFeatureCheck("Configured standard heroes in Gadd's", standardHeroesReady);
    if (!standardHeroesReady) return false;

    return EnsureOutpostHardModeEnabled("Hard mode enabled before leaving Gadd's (route test)");
}
