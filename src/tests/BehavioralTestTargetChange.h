static void RunBehavioralTargetChangeTest(uint32_t myId) {
    CmdReport("--- Test 4: Target Change ---");
    uint32_t targetId = 0;
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    for (uint32_t i = 1; i < maxAgents && i < 200; i++) {
        if (i == myId) continue;
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->agent_id == 0) continue;
        targetId = a->agent_id;
        break;
    }

    if (targetId != 0) {
        CmdReport("Targeting agent %u", targetId);
        AgentMgr::ChangeTarget(targetId);
        Sleep(1000);

        uint32_t currentTarget = AgentMgr::GetTargetId();
        CmdReport("Current target after change: %u", currentTarget);
        CmdCheck("Target changed to requested agent", currentTarget == targetId);
    } else {
        CmdReport("[SKIP] No other agents found to target");
    }
    CmdReport("");
}
