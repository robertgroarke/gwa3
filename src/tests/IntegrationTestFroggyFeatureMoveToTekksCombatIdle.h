static void ResetCombatStateBeforeTekksPath() {
    FroggyFeatureReport("  Resetting player combat state before Tekks path...");
    AgentMgr::CancelAction();
    Sleep(100);
    const bool idleBeforeRoute = WaitFor("Player combat idle before Tekks path", 3000, []() {
        auto* me = AgentMgr::GetMyAgent();
        return me && !AgentMgr::IsCasting(me) && me->skill == 0;
    });
    FroggyFeatureCheck("Player combat idle before Tekks path", idleBeforeRoute);
}
