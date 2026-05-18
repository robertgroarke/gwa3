static Item* FindFirstBackpackItem() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return nullptr;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            if (bag->items.buffer[i]) return bag->items.buffer[i];
        }
    }
    return nullptr;
}

static uint32_t CountBackpackItems() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            if (bag->items.buffer[i]) count++;
        }
    }
    return count;
}

static uint32_t FindInventoryBagSlot(Inventory* inv, Bag* bagPtr) {
    if (!inv || !bagPtr) return UINT32_MAX;
    for (uint32_t bagIdx = 0; bagIdx < 23; ++bagIdx) {
        if (inv->bags[bagIdx] == bagPtr) return bagIdx;
    }
    return UINT32_MAX;
}

static uint32_t FindUsableTitleId(uint32_t currentTitle) {
    if (currentTitle > 0) return currentTitle;

    constexpr uint32_t kPreferredTitles[] = {
        TitleID::Sunspear,
        TitleID::Lightbringer,
        TitleID::Vanguard,
        TitleID::Norn,
        TitleID::Asura,
        TitleID::Deldrimor,
        TitleID::Kurzick,
        TitleID::Luxon,
    };

    for (uint32_t titleId : kPreferredTitles) {
        Title* track = PlayerMgr::GetTitleTrack(titleId);
        TitleClientData* clientData = PlayerMgr::GetTitleData(titleId);
        if (!track || !clientData) continue;
        if (clientData->name_id == 0) continue;
        if (track->max_title_rank == 0 || track->max_title_rank >= 100) continue;
        if (track->current_points > 0 || track->current_title_tier_index > 0 || track->next_title_tier_index > 0) {
            return titleId;
        }
    }

    return 0;
}

static uint32_t FindAlternateQuestId(uint32_t excludeQuestId) {
    const uint32_t questCount = QuestMgr::GetQuestLogSize();
    for (uint32_t i = 0; i < questCount; ++i) {
        Quest* quest = QuestMgr::GetQuestByIndex(i);
        if (!quest || quest->quest_id == 0) continue;
        if (quest->quest_id == excludeQuestId) continue;
        return quest->quest_id;
    }
    return 0;
}

static uint32_t FindNearbyFoeAgent(float maxDistance) {
    const uint32_t myId = ReadMyId();
    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(myId, myX, myY)) return 0;

    if (Offsets::AgentBase <= 0x10000) return 0;
    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return 0;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    const float maxDistSq = maxDistance * maxDistance;
    float bestDistSq = maxDistSq;
    uint32_t bestId = 0;

    for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->agent_id == myId) continue;
        if (living->type != 0xDB) continue;
        if (living->allegiance != 3) continue;
        if (living->hp <= 0.0f) continue;

        const float dx = living->x - myX;
        const float dy = living->y - myY;
        const float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = i;
        }
    }

    return bestId;
}

// ===== Item Workflow Tests =====
