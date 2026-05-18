static bool ApproachTekksQuestNpc(float& outNpcX, float& outNpcY) {
    const uint32_t tekksId = FindNearestNpc(kTekksX, kTekksY, 1800.0f);
    FroggyFeatureCheck("Tekks NPC found near expected coordinates", tekksId != 0);
    if (!tekksId) {
        return false;
    }

    TryReadAgentPosition(tekksId, outNpcX, outNpcY);
    FroggyFeatureReport("  Tekks candidate: agent=%u pos=(%.0f, %.0f)", tekksId, outNpcX, outNpcY);

    const bool reachedNpc = MovePlayerNear(outNpcX, outNpcY, 120.0f, 12000);
    float meX = 0.0f;
    float meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const float distToTekks = AgentMgr::GetDistance(meX, meY, outNpcX, outNpcY);
    FroggyFeatureReport("  Tekks pre-interact approach: reached=%d player=(%.0f, %.0f) dist=%.0f",
              reachedNpc ? 1 : 0, meX, meY, distToTekks);
    FroggyFeatureCheck("Reached Tekks staging range", reachedNpc || distToTekks <= 180.0f);
    return true;
}
