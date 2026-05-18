bool TestAgentDistanceCrossCheck() {
    IntReport("=== Agent Distance Cross-Check ===");

    const uint32_t myId = ReadMyId();
    if (myId == 0) {
        IntSkip("Agent distance", "Not in game");
        IntReport("");
        return false;
    }

    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) {
        IntSkip("Agent distance", "Cannot read player position");
        IntReport("");
        return false;
    }

    uint32_t nearbyId = FindNearbyNpcLikeAgent(5000.0f);
    if (!nearbyId) {
        IntSkip("Agent distance cross-check", "No nearby agent found");
        IntReport("");
        return true;
    }

    float npcX = 0.0f;
    float npcY = 0.0f;
    if (!TryReadAgentPosition(nearbyId, npcX, npcY)) {
        IntSkip("Agent distance cross-check", "Cannot read NPC position");
        IntReport("");
        return true;
    }

    const float dx = myX - npcX;
    const float dy = myY - npcY;
    const float manualDist = sqrtf(dx * dx + dy * dy);
    const float mgrDist = AgentMgr::GetDistance(myX, myY, npcX, npcY);

    IntReport("  Player pos: (%.0f, %.0f)", myX, myY);
    IntReport("  NPC %u pos: (%.0f, %.0f)", nearbyId, npcX, npcY);
    IntReport("  Manual distance: %.1f", manualDist);
    IntReport("  AgentMgr distance: %.1f", mgrDist);

    const float diff = (manualDist > mgrDist) ? (manualDist - mgrDist) : (mgrDist - manualDist);
    IntCheck("Distance calculations agree (within 1.0)", diff < 1.0f);
    IntCheck("Distance > 0 (different agents)", manualDist > 0.0f);
    IntCheck("Distance < 5000 (within search range)", manualDist < 5000.0f);

    IntReport("");
    return true;
}
