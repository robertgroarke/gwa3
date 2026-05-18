static bool PrepareGaddsForFreshSparkflyEntry() {
    const bool outpostAgentReady = WaitFor("MyID in Gadd's after Sparkfly reset", 30000, []() {
        return AgentMgr::GetMyId() > 0;
    });
    FroggyFeatureCheck("Agent ready in Gadd's after Sparkfly reset", outpostAgentReady);
    if (!outpostAgentReady) return false;

    WaitForStablePlayerState(8000);
    Sleep(2000);

    return EnsureOutpostHardModeEnabled("Hard mode enabled before fresh Sparkfly re-entry");
}
