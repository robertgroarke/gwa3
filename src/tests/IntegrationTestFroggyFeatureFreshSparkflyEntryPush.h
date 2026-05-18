static bool PushOutOfGaddsForFreshSparkfly() {
    FroggyFeatureReport("  Re-entering Sparkfly for the real dungeon path...");
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
    FroggyFeatureCheck("Left Gadd's for fresh Sparkfly run", leftOutpost);
    return leftOutpost;
}

static bool WaitForFreshSparkflyInstance() {
    const bool inSparkfly = WaitFor("MapID == Sparkfly after reset", 30000, []() {
        return MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    });
    FroggyFeatureCheck("Arrived in fresh Sparkfly instance", inSparkfly);
    if (!inSparkfly) return false;

    const bool sparkflyAgentReady = WaitFor("MyID in fresh Sparkfly instance", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    FroggyFeatureCheck("Agent ready in fresh Sparkfly instance", sparkflyAgentReady);
    if (!sparkflyAgentReady) return false;

    WaitForStablePlayerState(8000);
    Sleep(2000);
    return true;
}

static bool EnterFreshSparkflyFromGadds() {
    return PushOutOfGaddsForFreshSparkfly() && WaitForFreshSparkflyInstance();
}
