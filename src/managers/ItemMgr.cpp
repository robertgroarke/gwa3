#include <gwa3/managers/ItemMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

namespace GWA3::ItemMgr {

static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("ItemMgr: Initialized");
    return true;
}

void UseItem(uint32_t itemId)       { CtoS::UseItem(itemId); }
void EquipItem(uint32_t itemId)     { CtoS::EquipItem(itemId); }
void DropItem(uint32_t itemId)      { CtoS::DropItem(itemId); }
void PickUpItem(uint32_t agentId)   { CtoS::PickUpItem(agentId); }

void DestroyItem(uint32_t itemId) {
    CtoS::SendPacket(2, Packets::ITEM_DESTROY, itemId);
}

void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot) {
    CtoS::MoveItem(itemId, bagId, slot);
}

void IdentifyItem(uint32_t itemId, uint32_t kitId) {
    CtoS::SendPacket(3, Packets::ITEM_IDENTIFY, kitId, itemId);
}

void SplitStack(uint32_t itemId, uint32_t quantity) {
    CtoS::SendPacket(3, Packets::ITEM_SPLIT_STACK, itemId, quantity);
}

void SalvageSessionOpen(uint32_t kitId, uint32_t itemId) {
    CtoS::SendPacket(3, Packets::SALVAGE_SESSION_OPEN, kitId, itemId);
}

void SalvageMaterials() {
    CtoS::SendPacket(1, Packets::SALVAGE_MATERIALS);
}

void SalvageUpgrade() {
    CtoS::SendPacket(1, Packets::SALVAGE_UPGRADE);
}

void SalvageSessionCancel() {
    CtoS::SendPacket(1, Packets::SALVAGE_SESSION_CANCEL);
}

void SalvageSessionDone() {
    CtoS::SendPacket(1, Packets::SALVAGE_SESSION_DONE);
}

void BuyMaterials(uint32_t itemModelId, uint32_t quantity) {
    CtoS::SendPacket(3, Packets::BUY_MATERIALS, itemModelId, quantity);
}

void RequestQuote(uint32_t itemId) {
    CtoS::SendPacket(2, Packets::REQUEST_QUOTE, itemId);
}

void ChangeGold(uint32_t charGold, uint32_t storageGold) {
    CtoS::SendPacket(3, Packets::CHANGE_GOLD, charGold, storageGold);
}

void DropGold(uint32_t amount) {
    CtoS::SendPacket(2, Packets::DROP_GOLD, amount);
}

Item* GetItemById(uint32_t itemId) {
    Inventory* inv = GetInventory();
    if (!inv) return nullptr;
    for (int b = 0; b < 23; b++) {
        Bag* bag = inv->bags[b];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (item && item->item_id == itemId) return item;
        }
    }
    return nullptr;
}

Bag* GetBag(uint32_t bagIndex) {
    Inventory* inv = GetInventory();
    if (!inv || bagIndex >= 23) return nullptr;
    return inv->bags[bagIndex];
}

Inventory* GetInventory() {
    if (!Offsets::BasePointer) return nullptr;
    // BasePointer → game context → inventory at a known offset
    auto* ctx = *reinterpret_cast<uintptr_t**>(Offsets::BasePointer);
    if (!ctx) return nullptr;
    // Inventory pointer is at context + 0x2C (from GWA2.au3 GetInventoryPtr)
    auto* invPtr = reinterpret_cast<Inventory**>(reinterpret_cast<uintptr_t>(ctx) + 0x2C);
    return invPtr ? *invPtr : nullptr;
}

uint32_t GetGoldCharacter() {
    Inventory* inv = GetInventory();
    return inv ? inv->gold_character : 0;
}

uint32_t GetGoldStorage() {
    Inventory* inv = GetInventory();
    return inv ? inv->gold_storage : 0;
}

} // namespace GWA3::ItemMgr
