namespace GWA3::PartyMgr {

void FlagAll(float x, float y) {
    g_last_flag_all_x = x;
    g_last_flag_all_y = y;
    ++g_flag_all_count;
}

void UnflagAll() {
    ++g_unflag_all_count;
}

bool GetIsPartyDefeated() {
    return g_test_party_defeated;
}

} // namespace GWA3::PartyMgr

namespace GWA3::TestStubs::PartyMgr {

void ResetFlags() {
    g_last_flag_all_x = 0.0f;
    g_last_flag_all_y = 0.0f;
    g_flag_all_count = 0u;
    g_unflag_all_count = 0u;
    g_test_party_defeated = false;
}

uint32_t FlagAllCount() {
    return g_flag_all_count;
}

uint32_t UnflagAllCount() {
    return g_unflag_all_count;
}

float LastFlagAllX() {
    return g_last_flag_all_x;
}

float LastFlagAllY() {
    return g_last_flag_all_y;
}

void SetPartyDefeated(bool defeated) {
    g_test_party_defeated = defeated;
}

} // namespace GWA3::TestStubs::PartyMgr

namespace GWA3::TestStubs::ItemMgr {

void Reset() {
    g_test_inventory = {};
    g_test_bundle_item = {};
    for (std::size_t bagIndex = 0; bagIndex < kMaxTestBags; ++bagIndex) {
        g_test_bags[bagIndex] = {};
        for (std::size_t slotIndex = 0; slotIndex < kMaxTestItemsPerBag; ++slotIndex) {
            g_test_items[bagIndex][slotIndex] = {};
            g_test_item_ptrs[bagIndex][slotIndex] = nullptr;
            g_test_item_names[bagIndex][slotIndex][0] = 0;
            g_test_item_names[bagIndex][slotIndex][1] = 0;
        }
    }
    g_last_dropped_item_id = 0u;
    g_last_picked_item_agent_id = 0u;
    g_last_equipped_item_id = 0u;
    g_last_used_item_id = 0u;
    g_last_identified_item_id = 0u;
    g_last_identify_kit_id = 0u;
    g_last_salvage_kit_id = 0u;
    g_last_salvaged_item_id = 0u;
    g_pending_salvage_item_id = 0u;
    g_last_moved_item_id = 0u;
    g_last_move_bag_id = 0u;
    g_last_move_slot = 0u;
    g_accept_unclaimed_count = 0u;
    g_last_accept_unclaimed_bag_index = 0u;
    g_last_transact_type = 0u;
    g_last_transact_quantity = 0u;
    g_last_transact_item_id = 0u;
    g_identify_count = 0u;
    g_salvage_open_count = 0u;
    g_salvage_done_count = 0u;
    g_move_item_count = 0u;
    g_use_item_count = 0u;
    g_transact_count = 0u;
}

void SetGold(uint32_t charGold, uint32_t storageGold) {
    g_test_inventory.gold_character = charGold;
    g_test_inventory.gold_storage = storageGold;
}

void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount) {
    if (bagIndex >= kMaxTestBags) {
        return;
    }
    if (slotCount > kMaxTestItemsPerBag) {
        slotCount = static_cast<uint32_t>(kMaxTestItemsPerBag);
    }

    auto& bag = g_test_bags[bagIndex];
    bag = {};
    bag.index = bagIndex;
    bag.items.buffer = g_test_item_ptrs[bagIndex];
    bag.items.capacity = static_cast<uint32_t>(kMaxTestItemsPerBag);
    bag.items.size = slotCount;
    bag.items_count = 0u;
    g_test_inventory.bags[bagIndex] = &bag;
}

void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity) {
    if (bagIndex >= kMaxTestBags || slotIndex >= kMaxTestItemsPerBag) {
        return;
    }
    if (g_test_inventory.bags[bagIndex] == nullptr) {
        SetBagCapacity(bagIndex, static_cast<uint32_t>(slotIndex + 1u));
    }

    auto& item = g_test_items[bagIndex][slotIndex];
    item = {};
    item.item_id = itemId;
    item.model_id = modelId;
    item.type = type;
    item.quantity = quantity;
    item.value = value;
    item.interaction = interaction;
    if (rarity != 0u) {
        g_test_item_names[bagIndex][slotIndex][0] = static_cast<wchar_t>(rarity);
        item.name_enc = g_test_item_names[bagIndex][slotIndex];
    }
    g_test_item_ptrs[bagIndex][slotIndex] = &item;

    auto* bag = g_test_inventory.bags[bagIndex];
    item.bag = bag;
    if (bag && slotIndex >= bag->items.size) {
        bag->items.size = static_cast<uint32_t>(slotIndex + 1u);
    }
    if (bag) {
        uint32_t count = 0u;
        for (uint32_t i = 0u; i < bag->items.size; ++i) {
            if (g_test_item_ptrs[bagIndex][i] != nullptr) {
                ++count;
            }
        }
        bag->items_count = count;
    }
}

void ClearBagItem(uint32_t bagIndex, uint32_t slotIndex) {
    if (bagIndex >= kMaxTestBags || slotIndex >= kMaxTestItemsPerBag) {
        return;
    }
    g_test_items[bagIndex][slotIndex] = {};
    g_test_item_ptrs[bagIndex][slotIndex] = nullptr;
    g_test_item_names[bagIndex][slotIndex][0] = 0;
    g_test_item_names[bagIndex][slotIndex][1] = 0;
    auto* bag = g_test_inventory.bags[bagIndex];
    if (bag) {
        uint32_t count = 0u;
        for (uint32_t i = 0u; i < bag->items.size; ++i) {
            if (g_test_item_ptrs[bagIndex][i] != nullptr) {
                ++count;
            }
        }
        bag->items_count = count;
    }
}

void SetHeldBundleItemId(uint32_t itemId) {
    g_test_bundle_item = {};
    g_test_bundle_item.item_id = itemId;
    g_test_inventory.bundle = itemId == 0u ? nullptr : &g_test_bundle_item;
}

uint32_t LastDroppedItemId() {
    return g_last_dropped_item_id;
}

uint32_t LastEquippedItemId() {
    return g_last_equipped_item_id;
}

uint32_t LastPickedItemAgentId() {
    return g_last_picked_item_agent_id;
}

uint32_t LastUsedItemId() {
    return g_last_used_item_id;
}

uint32_t LastIdentifiedItemId() {
    return g_last_identified_item_id;
}

uint32_t LastIdentifyKitId() {
    return g_last_identify_kit_id;
}

uint32_t IdentifyCount() {
    return g_identify_count;
}

uint32_t LastSalvagedItemId() {
    return g_last_salvaged_item_id;
}

uint32_t LastSalvageKitId() {
    return g_last_salvage_kit_id;
}

uint32_t SalvageOpenCount() {
    return g_salvage_open_count;
}

uint32_t SalvageDoneCount() {
    return g_salvage_done_count;
}

uint32_t LastMovedItemId() {
    return g_last_moved_item_id;
}

uint32_t LastMoveBagId() {
    return g_last_move_bag_id;
}

uint32_t LastMoveSlot() {
    return g_last_move_slot;
}

uint32_t AcceptUnclaimedCount() {
    return g_accept_unclaimed_count;
}

uint32_t LastAcceptUnclaimedBagIndex() {
    return g_last_accept_unclaimed_bag_index;
}

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs() {
    for (std::size_t i = 0; i < kMaxDialogHistory; ++i) {
        g_dialog_history[i] = 0u;
    }
    g_dialog_count = 0;
    g_test_last_dialog_id = 0u;
}

std::size_t DialogCount() {
    return g_dialog_count;
}

uint32_t DialogAt(std::size_t index) {
    return index < g_dialog_count ? g_dialog_history[index] : 0u;
}

void ResetQuestState() {
    g_test_quest = {};
    g_test_dialog_granted_quest = {};
    g_test_active_quest_id = 0u;
    g_test_dialog_grants_quest_dialog_id = 0u;
    g_test_request_quest_info_count = 0u;
}

void SetQuest(uint32_t questId, uint32_t logState) {
    g_test_quest = {};
    g_test_quest.quest_id = questId;
    g_test_quest.log_state = logState;
}

void SetQuestGrantedOnDialog(uint32_t dialogId, uint32_t questId, uint32_t logState) {
    g_test_dialog_grants_quest_dialog_id = dialogId;
    g_test_dialog_granted_quest = {};
    g_test_dialog_granted_quest.quest_id = questId;
    g_test_dialog_granted_quest.log_state = logState;
}

uint32_t RequestQuestInfoCount() {
    return g_test_request_quest_info_count;
}

void SetBlessingDialogEffect(uint32_t dialogId, uint32_t skillId) {
    g_test_dialog_applies_effect_dialog_id = dialogId;
    g_test_dialog_applies_effect_skill_id = skillId;
}

} // namespace GWA3::TestStubs::QuestMgr

namespace GWA3::ItemMgr {

namespace {

void RecountBag(Bag* bag) {
    if (!bag || !bag->items.buffer) return;
    uint32_t count = 0u;
    for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
        if (bag->items.buffer[slot] != nullptr) {
            ++count;
        }
    }
    bag->items_count = count;
}

Item* DetachItem(uint32_t itemId) {
    for (std::size_t bagIndex = 0; bagIndex < kMaxTestBags; ++bagIndex) {
        auto* bag = g_test_inventory.bags[bagIndex];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (item && item->item_id == itemId) {
                bag->items.buffer[slot] = nullptr;
                item->bag = nullptr;
                RecountBag(bag);
                return item;
            }
        }
    }
    return nullptr;
}

} // namespace

Inventory* GetInventory() {
    return &g_test_inventory;
}

Bag* GetBag(uint32_t bagIndex) {
    return bagIndex < kMaxTestBags ? g_test_inventory.bags[bagIndex] : nullptr;
}

Item* GetItemById(uint32_t itemId) {
    if (g_test_inventory.bundle && g_test_inventory.bundle->item_id == itemId) {
        return g_test_inventory.bundle;
    }
    for (std::size_t bagIndex = 0; bagIndex < kMaxTestBags; ++bagIndex) {
        auto* bag = g_test_inventory.bags[bagIndex];
        if (!bag || !bag->items.buffer) {
            continue;
        }
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (item && item->item_id == itemId) {
                return item;
            }
        }
    }
    return nullptr;
}

uint32_t GetGoldCharacter() {
    return g_test_inventory.gold_character;
}

uint32_t GetGoldStorage() {
    return g_test_inventory.gold_storage;
}

void ChangeGold(uint32_t charGold, uint32_t storageGold) {
    g_test_inventory.gold_character = charGold;
    g_test_inventory.gold_storage = storageGold;
}

void DropItem(uint32_t itemId) {
    g_last_dropped_item_id = itemId;
    if (g_test_inventory.bundle && g_test_inventory.bundle->item_id == itemId) {
        g_test_inventory.bundle = nullptr;
        return;
    }
    (void)DetachItem(itemId);
}

void EquipItem(uint32_t itemId) {
    g_last_equipped_item_id = itemId;
    auto* item = GetItemById(itemId);
    if (!item) {
        return;
    }

    if (!g_test_has_player_agent) {
        g_test_player_agent = {};
        g_test_player_agent.agent_id = g_test_my_agent_id;
        g_test_player_agent.type = 0xDBu;
        g_test_player_agent.allegiance = 1u;
        g_test_player_agent.hp = 1.0f;
        g_test_player_agent.energy = 1.0f;
        g_test_has_player_agent = true;
    }
    g_test_player_agent.weapon_item_id = static_cast<uint16_t>(itemId);
    g_test_player_agent.weapon_item_type = item->type;
    item->equipped = 1u;
}

void PickUpItem(uint32_t itemAgentId) {
    g_last_picked_item_agent_id = itemAgentId;
    auto* agent = GWA3::AgentMgr::GetAgentByID(itemAgentId);
    if (!agent || agent->type != 0x400u) {
        return;
    }

    auto* itemAgent = static_cast<GWA3::AgentItem*>(agent);
    auto* item = GetItemById(itemAgent->item_id);
    if (item && item->type == 6u) {
        GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(item->item_id);
    }
}

void UseItem(uint32_t itemId) {
    g_last_used_item_id = itemId;
    ++g_use_item_count;
}

void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot) {
    g_last_moved_item_id = itemId;
    g_last_move_bag_id = bagId;
    g_last_move_slot = slot;
    ++g_move_item_count;

    auto* item = DetachItem(itemId);
    auto* bag = GetBag(bagId);
    if (!item || !bag || !bag->items.buffer || slot >= bag->items.capacity) {
        return;
    }
    if (slot >= bag->items.size) {
        bag->items.size = slot + 1u;
    }
    bag->items.buffer[slot] = item;
    RecountBag(bag);
}

void AcceptAllUnclaimedItems(uint32_t unclaimedBagIndex) {
    g_last_accept_unclaimed_bag_index = unclaimedBagIndex;
    ++g_accept_unclaimed_count;
}

void IdentifyItem(uint32_t itemId, uint32_t kitId) {
    g_last_identified_item_id = itemId;
    g_last_identify_kit_id = kitId;
    ++g_identify_count;
    auto* item = GetItemById(itemId);
    if (item) {
        item->interaction |= 0x1u;
    }
}

void SalvageSessionOpen(uint32_t kitId, uint32_t itemId) {
    g_last_salvage_kit_id = kitId;
    g_last_salvaged_item_id = itemId;
    g_pending_salvage_item_id = itemId;
    ++g_salvage_open_count;
}

void SalvageMaterials() {
    if (g_pending_salvage_item_id != 0u) {
        (void)DetachItem(g_pending_salvage_item_id);
    }
}

void SalvageSessionDone() {
    g_pending_salvage_item_id = 0u;
    ++g_salvage_done_count;
}

} // namespace GWA3::ItemMgr

namespace GWA3::QuestMgr {

void Dialog(uint32_t dialogId) {
    if (g_dialog_count >= kMaxDialogHistory) {
        return;
    }
    g_dialog_history[g_dialog_count++] = dialogId;
    g_test_last_dialog_id = dialogId;
    if (dialogId == g_test_dialog_applies_effect_dialog_id &&
        g_test_dialog_applies_effect_skill_id != 0u &&
        g_test_effect_skill_count < kMaxEffectSkills) {
        g_test_effect_skill_ids[g_test_effect_skill_count++] = g_test_dialog_applies_effect_skill_id;
        GWA3::TestStubs::EffectMgr::AddEffectForAgent(
            g_test_my_agent_id,
            g_test_dialog_applies_effect_skill_id,
            10.0f);
    }
    if (dialogId == g_test_dialog_grants_quest_dialog_id &&
        g_test_dialog_granted_quest.quest_id != 0u) {
        g_test_quest = g_test_dialog_granted_quest;
    }
}

void SetActiveQuest(uint32_t questId) {
    g_test_active_quest_id = questId;
}

void RequestQuestInfo(uint32_t) {
    ++g_test_request_quest_info_count;
}

uint32_t GetActiveQuestId() {
    return g_test_active_quest_id;
}

uint32_t GetQuestLogSize() {
    return g_test_quest.quest_id == 0u ? 0u : 1u;
}

Quest* GetQuestById(uint32_t questId) {
    return g_test_quest.quest_id == questId ? &g_test_quest : nullptr;
}

} // namespace GWA3::QuestMgr
