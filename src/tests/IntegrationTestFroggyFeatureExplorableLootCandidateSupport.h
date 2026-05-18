static bool FindExplorableLootCandidate(ExplorableLootCandidate& out) {
    AgentItem* item = FindNearbyGroundItem(5000.0f);
    if (!item) {
        const bool createdOpportunity = TryForceNearbyLootDrop();
        FroggyFeatureCheck("Loot pickup can create a nearby opportunity if needed", createdOpportunity);
        if (createdOpportunity) {
            Sleep(1000);
            item = FindNearbyGroundItem(12000.0f);
        }
    }
    if (!item) {
        FroggyFeatureSkip("Explorable loot pickup", "No nearby ground item found after loot probe");
        return false;
    }

    out.agentId = item->agent_id;
    out.itemId = item->item_id;
    out.x = item->x;
    out.y = item->y;
    return true;
}

static float GetDistanceToExplorableLootCandidate(const ExplorableLootCandidate& candidate) {
    float myX = 0.0f;
    float myY = 0.0f;
    const bool havePlayerPos = TryReadAgentPosition(ReadMyId(), myX, myY);
    return havePlayerPos ? AgentMgr::GetDistance(myX, myY, candidate.x, candidate.y) : -1.0f;
}

static void ReportExplorableLootCandidate(const ExplorableLootCandidate& candidate, const InventorySnapshot& inventoryBefore) {
    const float itemDistance = GetDistanceToExplorableLootCandidate(candidate);
    FroggyFeatureReport("  Loot candidate: agent=%u item=%u pos=(%.0f, %.0f) dist=%.0f inventoryCount=%u gold=%u/%u",
              candidate.agentId,
              candidate.itemId,
              candidate.x,
              candidate.y,
              itemDistance,
              inventoryBefore.count,
              inventoryBefore.goldCharacter,
              inventoryBefore.goldStorage);
}

static void MoveNearExplorableLootCandidate(const ExplorableLootCandidate& candidate) {
    const float itemDistance = GetDistanceToExplorableLootCandidate(candidate);
    if (itemDistance >= 0.0f && itemDistance <= 180.0f) return;

    FroggyFeatureReport("  Moving closer to loot before pickup...");
    MovePlayerNear(candidate.x, candidate.y, 120.0f, 12000);
}
