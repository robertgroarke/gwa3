static bool WaitForPlayerPositionSettle(DWORD timeoutMs, float maxDeltaPerSample = 20.0f) {
    float lastX = 0.0f;
    float lastY = 0.0f;
    bool haveLast = false;
    int settledSamples = 0;
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        float x = 0.0f;
        float y = 0.0f;
        if (!TryReadAgentPosition(ReadMyId(), x, y)) {
            Sleep(100);
            continue;
        }
        if (haveLast) {
            const float delta = AgentMgr::GetDistance(lastX, lastY, x, y);
            if (delta <= maxDeltaPerSample) {
                if (++settledSamples >= 3) {
                    return true;
                }
            } else {
                settledSamples = 0;
            }
        }
        lastX = x;
        lastY = y;
        haveLast = true;
        Sleep(150);
    }
    return false;
}
