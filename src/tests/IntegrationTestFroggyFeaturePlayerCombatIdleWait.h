static bool WaitForPlayerCombatIdle(DWORD timeoutMs, const char* label = "Player combat idle") {
    return WaitFor(label, timeoutMs, []() {
        auto* me = AgentMgr::GetMyAgent();
        return me && !AgentMgr::IsCasting(me) && me->skill == 0;
    });
}
