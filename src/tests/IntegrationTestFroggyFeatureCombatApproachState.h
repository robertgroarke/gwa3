struct CombatApproachState {
    DWORD lastIssue = 0;
    float lastPx = 0.0f;
    float lastPy = 0.0f;
    bool haveLastPos = false;
    bool moveIssued = false;
};

static void UpdateCombatApproachPosition(CombatApproachState& state) {
    float px = 0.0f;
    float py = 0.0f;
    if (!TryReadAgentPosition(ReadMyId(), px, py)) return;
    state.lastPx = px;
    state.lastPy = py;
    state.haveLastPos = true;
}

static bool ShouldIssueCombatApproachMove(CombatApproachState& state, DWORD now) {
    if (!state.moveIssued) return true;
    if (!state.haveLastPos || (now - state.lastIssue) < 2000) return false;

    float px = 0.0f;
    float py = 0.0f;
    if (!TryReadAgentPosition(ReadMyId(), px, py)) return false;

    const float moved = AgentMgr::GetDistance(state.lastPx, state.lastPy, px, py);
    state.lastPx = px;
    state.lastPy = py;
    return moved < 40.0f;
}
