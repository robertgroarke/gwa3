static bool MovePlayerNearForMerchantHarnessBody(float npcX, float npcY, float* outDistance) {
    const bool reached = MovePlayerNear(npcX, npcY, 70.0f, 12000);
    float meX = 0.0f;
    float meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const float dist = AgentMgr::GetDistance(meX, meY, npcX, npcY);
    if (outDistance) *outDistance = dist;
    return reached || dist <= kSessionHarnessInteractionDistanceTolerance;
}

static bool MovePlayerNearMerchantIsolation(float x, float y, float threshold, int timeoutMs) {
    const DWORD start = GetTickCount();
    GameThread::EnqueuePost([x, y]() {
        AgentMgr::Move(x, y);
    });
    while ((GetTickCount() - start) < static_cast<DWORD>(timeoutMs)) {
        Sleep(500);

        float px = 0.0f;
        float py = 0.0f;
        if (!TryReadAgentPosition(ReadMyId(), px, py)) continue;

        const float dist = AgentMgr::GetDistance(px, py, x, y);
        FroggyFeatureReport("  Merchant move probe: pos=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f",
                  px, py, x, y, dist);
        if (dist <= threshold) {
            return true;
        }
    }
    return false;
}
