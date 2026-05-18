static void RunBehavioralStabilityTest(uint32_t mapId) {
    CmdReport("--- Test 11: Stability ---");
    CmdReport("Waiting 5 seconds for stability check...");
    Sleep(5000);
    auto* meAfter = AgentMgr::GetMyAgent();
    CmdCheck("Agent still readable after tests", meAfter != nullptr);
    uint32_t mapAfter = MapMgr::GetMapId();
    CmdCheck("Still on same map", mapAfter == mapId);
    CmdReport("");
}
