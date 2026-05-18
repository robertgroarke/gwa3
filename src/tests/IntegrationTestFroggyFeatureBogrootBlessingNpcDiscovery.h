static uint32_t FindBogrootBlessingNpc() {
    const float kSearchRadius = 2000.0f;
    uint32_t npcId = FindNearestNpc(kBlessingX, kBlessingY, kSearchRadius);
    if (npcId == 0) {
        FroggyFeatureReport("  No NPC found within %.0f of blessing coords", kSearchRadius);
        DumpNearbyAgents(kBlessingX, kBlessingY, kSearchRadius, 10);
    }
    return npcId;
}
