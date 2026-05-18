enum class SparkflyEntryResult {
    InSparkfly,
    FailedSparkflyLoad,
    DidNotLeaveOutpost,
};

static void ReportCurrentPlayerPosition(const char* label) {
    float px = 0.0f;
    float py = 0.0f;
    TryReadAgentPosition(ReadMyId(), px, py);
    FroggyFeatureReport("  %s: (%.0f, %.0f)", label, px, py);
}

static SparkflyEntryResult RunFroggySparkflyEntryPhase() {
    FroggyFeatureReport("=== PHASE 4: Enter Sparkfly Swamp ===");
    {
        float px = 0.0f;
        float py = 0.0f;
        TryReadAgentPosition(ReadMyId(), px, py);
        FroggyFeatureReport("  Current position: (%.0f, %.0f) MapID=%u", px, py, ReadMapId());
    }

    FroggyFeatureReport("  Walking to exit waypoint 1 (-10018, -21892)...");
    MovePlayerNear(-10018.0f, -21892.0f, 350.0f, 20000);
    ReportCurrentPlayerPosition("After wp1");

    FroggyFeatureReport("  Walking to exit waypoint 2 (-9550, -20400)...");
    MovePlayerNear(-9550.0f, -20400.0f, 350.0f, 20000);
    ReportCurrentPlayerPosition("After wp2");

    FroggyFeatureReport("  Pushing toward Sparkfly...");
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

    if (!leftOutpost) {
        FroggyFeatureSkip("Explorable entry", "Failed to leave Gadd's within 30s");
        FroggyFeatureSkip("Explorable tests", "Never entered explorable");
        FroggyFeatureSkip("Return to outpost", "Never left outpost");
        return SparkflyEntryResult::DidNotLeaveOutpost;
    }

    const bool inSparkfly = WaitFor("MapID == Sparkfly", 30000, []() {
        return MapMgr::GetMapId() == MapIds::SPARKFLY_SWAMP;
    });
    if (!inSparkfly) {
        FroggyFeatureSkip("Explorable tests", "Failed to enter Sparkfly Swamp");
        FroggyFeatureSkip("Return to outpost", "Never left outpost");
        return SparkflyEntryResult::FailedSparkflyLoad;
    }

    const bool agentOk = WaitFor("MyID in explorable", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    Sleep(5000);
    FroggyFeatureCheck("Phase 4: Entered Sparkfly Swamp", agentOk);
    return SparkflyEntryResult::InSparkfly;
}
