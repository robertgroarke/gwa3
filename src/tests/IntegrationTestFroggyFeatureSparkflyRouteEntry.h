static bool EnterSparkflyForRouteTest() {
    if (MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP) {
        FroggyFeatureCheck("Already in Sparkfly", true);
        return true;
    }

    FroggyFeatureReport("=== SPARKFLY ROUTE TEST: Enter Sparkfly ===");
    MovePlayerNear(-10018.0f, -21892.0f, 350.0f, 20000);
    MovePlayerNear(-9550.0f, -20400.0f, 350.0f, 20000);

    const DWORD zoneStart = GetTickCount();
    bool leftOutpost = false;
    while ((GetTickCount() - zoneStart) < 45000) {
        if (MapMgr::GetMapId() != MapIds::GADDS_ENCAMPMENT) {
            leftOutpost = true;
            break;
        }
        GameThread::EnqueuePost([]() {
            AgentMgr::Move(-9451.0f, -19766.0f);
        });
        Sleep(500);
    }
    FroggyFeatureCheck("Left Gadd's for Sparkfly", leftOutpost);
    if (!leftOutpost) return false;

    const bool inSparkfly = WaitFor("MapID == Sparkfly", 30000, []() {
        return MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    });
    FroggyFeatureCheck("Arrived in Sparkfly", inSparkfly);
    if (!inSparkfly) return false;

    const bool sparkflyAgentReady = WaitFor("Sparkfly MyID > 0", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    FroggyFeatureCheck("Sparkfly agent ready", sparkflyAgentReady);
    if (!sparkflyAgentReady) return false;

    WaitForStablePlayerState(8000);
    Sleep(2000);
    return true;
}
