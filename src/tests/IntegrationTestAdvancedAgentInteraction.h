bool TestAgentInteraction() {
    IntReport("=== Agent Interaction ===");

    if (ReadMyId() == 0) { IntSkip("AgentInteract", "Not in game"); IntReport(""); return false; }

    // AgentExists
    bool selfExists = AgentMgr::GetAgentExists(ReadMyId());
    IntReport("  AgentExists(self): %d", selfExists);
    IntCheck("Self agent exists", selfExists);

    AgentLiving* selfAgent = AgentMgr::GetMyAgent();
    IntReport("  GetMyAgent(): %p", selfAgent);
    IntCheck("GetMyAgent returned non-null", selfAgent != nullptr);

    bool bogusExists = AgentMgr::GetAgentExists(99999);
    IntReport("  AgentExists(99999): %d", bogusExists);
    IntCheck("Bogus agent does not exist", !bogusExists);

    // CallTarget is tested in explorable via TestExplorableCallTarget (086b).
    // It can't work in outpost — no valid targets to call.

    IntReport("");
    return true;
}

// ===== Camera FOV =====
