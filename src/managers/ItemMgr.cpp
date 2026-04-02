#include <gwa3/managers/ItemMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>

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
    if (!itemId) return nullptr;
    __try {
        for (int b = 0; b < 23; b++) {
            Bag* bag = GetBag(b);
            if (!bag || !bag->items.buffer || bag->items.size == 0 || bag->items.size > 256) continue;
            for (uint32_t i = 0; i < bag->items.size; i++) {
                Item* item = bag->items.buffer[i];
                if (item && item->item_id == itemId) return item;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

// Read a pointer safely, returning false on null/low addresses
static bool ReadPtr(uintptr_t address, uintptr_t& value) {
    if (address <= 0x10000) { value = 0; return false; }
    __try {
        value = *reinterpret_cast<uintptr_t*>(address);
        return value > 0x10000;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = 0;
        return false;
    }
}

// AutoIt chain: *BasePointer -> +0x18 -> +0x40 -> +0xF8 = bags array base
static uintptr_t GetBagsArrayBase() {
    if (!Offsets::BasePointer) return 0;
    uintptr_t ctx = 0, p1 = 0, p2 = 0, bagsBase = 0;
    if (!ReadPtr(Offsets::BasePointer, ctx)) return 0;
    if (!ReadPtr(ctx + 0x18, p1)) return 0;
    if (!ReadPtr(p1 + 0x40, p2)) return 0;
    if (!ReadPtr(p2 + 0xF8, bagsBase)) return 0;
    return bagsBase;
}

Bag* GetBag(uint32_t bagIndex) {
    if (bagIndex >= 23) return nullptr;
    uintptr_t bagsBase = GetBagsArrayBase();
    if (!bagsBase) return nullptr;
    uintptr_t bagPtr = 0;
    if (!ReadPtr(bagsBase + 4 * bagIndex, bagPtr)) return nullptr;
    return reinterpret_cast<Bag*>(bagPtr);
}

Inventory* GetInventory() {
    // Inventory struct doesn't map directly to a single pointer.
    // Build a pseudo-inventory by reading bags individually.
    // For gold, use the BasePointer chain: *base -> +0x18 -> +0x40 -> gold offsets
    static Inventory s_cachedInventory = {};
    uintptr_t bagsBase = GetBagsArrayBase();
    if (!bagsBase) return nullptr;

    __try {
        for (int b = 0; b < 23; b++) {
            uintptr_t bagPtr = 0;
            if (ReadPtr(bagsBase + 4 * b, bagPtr)) {
                s_cachedInventory.bags[b] = reinterpret_cast<Bag*>(bagPtr);
            } else {
                s_cachedInventory.bags[b] = nullptr;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }

    // Gold: same chain as bags — bagsBase is already deref'd from +0xF8
    // Gold char at bagsBase + 0x90, gold storage at bagsBase + 0x94
    // But bagsBase was the result of 4 derefs. We need the struct BEFORE the +0xF8 deref.
    uintptr_t ctx = 0, p1 = 0, p2 = 0;
    if (ReadPtr(Offsets::BasePointer, ctx) && ReadPtr(ctx + 0x18, p1) && ReadPtr(p1 + 0x40, p2)) {
        uintptr_t invStruct = 0;
        if (ReadPtr(p2 + 0xF8, invStruct)) {
            __try {
                s_cachedInventory.gold_character = *reinterpret_cast<uint32_t*>(invStruct + 0x90);
                s_cachedInventory.gold_storage = *reinterpret_cast<uint32_t*>(invStruct + 0x94);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    return &s_cachedInventory;
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
