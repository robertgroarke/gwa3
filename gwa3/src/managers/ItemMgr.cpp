#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/TradeMgr.h>
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
void PickUpItem(uint32_t agentId)   { AgentMgr::InteractItem(agentId, false); }

void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot) {
    uint32_t packetBagId = bagId;
    Bag* bag = GetBag(bagId);
    if (bag) {
        // GW packets use the bag's runtime ID at +0x08, not the bag-array index.
        packetBagId = bag->h0008;
    }
    CtoS::MoveItem(itemId, packetBagId, slot);
}

void IdentifyItem(uint32_t itemId, uint32_t kitId) {
    CtoS::SendPacket(3, Packets::ITEM_IDENTIFY, kitId, itemId);
}

void SalvageSessionOpen(uint32_t kitId, uint32_t itemId) {
    CtoS::SendPacket(3, Packets::SALVAGE_SESSION_OPEN, kitId, itemId);
}

void SalvageMaterials() {
    CtoS::SendPacket(1, Packets::SALVAGE_MATERIALS);
}

void SalvageSessionDone() {
    CtoS::SendPacket(1, Packets::SALVAGE_SESSION_DONE);
}

void BuyMaterials(uint32_t itemModelId, uint32_t quantity) {
    TradeMgr::BuyMaterials(itemModelId, quantity);
}

void RequestQuote(uint32_t itemId) {
    CtoS::SendPacket(2, Packets::REQUEST_QUOTE, itemId);
}

// Xunlai-open timestamp — see MarkXunlaiOpened / IsXunlaiRecentlyOpened.
// Written from the bridge's open_xunlai handler AFTER the GoNPC packet
// lands; read by ChangeGold-facing handlers to refuse stale-state calls.
static DWORD s_xunlaiOpenedTick = 0;
static constexpr DWORD kXunlaiFreshTtlMs = 15000;

void MarkXunlaiOpened() {
    s_xunlaiOpenedTick = GetTickCount();
}

bool IsXunlaiRecentlyOpened() {
    if (s_xunlaiOpenedTick == 0) return false;
    return (GetTickCount() - s_xunlaiOpenedTick) <= kXunlaiFreshTtlMs;
}

void ChangeGold(uint32_t charGold, uint32_t storageGold) {
    CtoS::SendPacket(3, Packets::CHANGE_GOLD, charGold, storageGold);
}

void DropGold(uint32_t amount) {
    CtoS::SendPacket(2, Packets::DROP_GOLD, amount);
}

// ItemClick: __fastcall(uint32_t* bag_id_ptr, void* edx, ItemClickParam* param)
// ItemClickParam layout: { uint32_t call_id, uint32_t item_id, ... }
struct ItemClickParam {
    uint32_t call_id;
    uint32_t item_id;
    uint32_t h0008;
};

typedef void(__fastcall* ItemClickFn)(uint32_t*, void*, ItemClickParam*);

static bool ReadPtr(uintptr_t address, uintptr_t& value);
static bool GetGlobalItemArray(uintptr_t& itemsBase, uint32_t& arraySize);

static uintptr_t s_lastInventoryRootLog = 0;
static uintptr_t s_lastInventoryStructLog = 0;
static uintptr_t s_lastInventoryProbeDumpP1 = 0;

static void DumpInventoryCandidate(uintptr_t p1, uintptr_t rootOffset, uintptr_t p2, uintptr_t invStruct) {
    __try {
        Log::Warn("ItemMgr: Candidate +0x%X p2=0x%08X inv=0x%08X p2.B8=0x%08X p2.C0=%u",
                  static_cast<unsigned>(rootOffset),
                  static_cast<unsigned>(p2),
                  static_cast<unsigned>(invStruct),
                  p2 > 0x10000 ? *reinterpret_cast<uint32_t*>(p2 + 0xB8) : 0u,
                  p2 > 0x10000 ? *reinterpret_cast<uint32_t*>(p2 + 0xC0) : 0u);
        for (uint32_t idx = 0; idx < 6; ++idx) {
            uintptr_t bagPtr = 0;
            if (!ReadPtr(invStruct + 4u * idx, bagPtr)) {
                Log::Warn("ItemMgr: Candidate +0x%X bag[%u]=0x00000000", static_cast<unsigned>(rootOffset), idx);
                continue;
            }
            auto* bag = reinterpret_cast<Bag*>(bagPtr);
            Log::Warn("ItemMgr: Candidate +0x%X bag[%u]=0x%08X type=%u index=%u slots=%u items.size=%u",
                      static_cast<unsigned>(rootOffset),
                      idx,
                      static_cast<unsigned>(bagPtr),
                      bag->bag_type,
                      bag->index,
                      bag->items_count,
                      bag->items.size);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("ItemMgr: Candidate +0x%X raw dump faulted p2=0x%08X inv=0x%08X",
                  static_cast<unsigned>(rootOffset),
                  static_cast<unsigned>(p2),
                  static_cast<unsigned>(invStruct));
    }
}

static bool ProbeInventoryRoot(uintptr_t p1, uintptr_t rootOffset, uintptr_t& p2, uintptr_t& invStruct, uint32_t& bagHits) {
    p2 = invStruct = 0;
    bagHits = 0;
    if (!ReadPtr(p1 + rootOffset, p2)) return false;
    __try {
        invStruct = *reinterpret_cast<uintptr_t*>(p2 + 0xF8);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        invStruct = 0;
    }
    if (invStruct <= 0x10000) {
        invStruct = 0;
        return false;
    }

    for (uint32_t idx = 1; idx <= 4; ++idx) {
        uintptr_t bagPtr = 0;
        if (!ReadPtr(invStruct + 4u * idx, bagPtr)) continue;
        __try {
            auto* bag = reinterpret_cast<Bag*>(bagPtr);
            const bool indexMatches = (bag->index == idx) || (bag->index + 1u == idx);
            if (!indexMatches) continue;
            if (bag->items.size > 512u) continue;
            ++bagHits;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
    }

    return true;
}

static bool ReadInventoryStruct(uintptr_t& p0, uintptr_t& p1, uintptr_t& p2, uintptr_t& invStruct) {
    p0 = p1 = p2 = invStruct = 0;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (Offsets::BasePointer &&
            ReadPtr(Offsets::BasePointer, p0) &&
            ReadPtr(p0 + 0x18, p1)) {
            uintptr_t primaryRoot = 0, primaryInv = 0;
            uintptr_t siblingRoot = 0, siblingInv = 0;
            uint32_t primaryHits = 0, siblingHits = 0;

            const bool havePrimary = ProbeInventoryRoot(p1, 0x40, primaryRoot, primaryInv, primaryHits);
            const bool haveSibling = ProbeInventoryRoot(p1, 0x44, siblingRoot, siblingInv, siblingHits);
            if (havePrimary || (haveSibling && siblingHits > 0u)) {
                const bool chooseSibling = !havePrimary && siblingHits > 0u;
                p2 = chooseSibling ? siblingRoot : primaryRoot;
                invStruct = chooseSibling ? siblingInv : primaryInv;
                if (p2 != s_lastInventoryRootLog || invStruct != s_lastInventoryStructLog) {
                    s_lastInventoryRootLog = p2;
                    s_lastInventoryStructLog = invStruct;
                    Log::Info("ItemMgr: Inventory root selected from +0x%X p2=0x%08X inv=0x%08X primaryHits=%u siblingHits=%u",
                              chooseSibling ? 0x44u : 0x40u,
                              static_cast<unsigned>(p2),
                              static_cast<unsigned>(invStruct),
                              primaryHits,
                              siblingHits);
                }
                return true;
            }
            Log::Warn("ItemMgr: Inventory root probe failed p1=0x%08X primaryRoot=0x%08X primaryInv=0x%08X primaryHits=%u siblingRoot=0x%08X siblingInv=0x%08X siblingHits=%u",
                      static_cast<unsigned>(p1),
                      static_cast<unsigned>(primaryRoot),
                      static_cast<unsigned>(primaryInv),
                      primaryHits,
                      static_cast<unsigned>(siblingRoot),
                      static_cast<unsigned>(siblingInv),
                      siblingHits);
            if (p1 != s_lastInventoryProbeDumpP1) {
                s_lastInventoryProbeDumpP1 = p1;
                if (primaryInv > 0x10000) DumpInventoryCandidate(p1, 0x40, primaryRoot, primaryInv);
                if (siblingInv > 0x10000) DumpInventoryCandidate(p1, 0x44, siblingRoot, siblingInv);
            }
        }
        if (!Offsets::RefreshBasePointer()) break;
    }
    return false;
}

static bool ReadInventoryChain(uintptr_t& p0, uintptr_t& p1, uintptr_t& p2) {
    uintptr_t invStruct = 0;
    return ReadInventoryStruct(p0, p1, p2, invStruct);
}

bool ClickItem(uint32_t itemId) {
    if (!Offsets::ItemClick || Offsets::ItemClick <= 0x10000) return false;

    Item* item = GetItemById(itemId);
    if (!item || !item->bag) return false;

    auto fn = reinterpret_cast<ItemClickFn>(Offsets::ItemClick);
    uint32_t bagIndex = item->bag->index;
    ItemClickParam param = {};
    param.call_id = 15; // standard item click call ID
    param.item_id = itemId;

    __try {
        fn(&bagIndex, nullptr, &param);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
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

    uintptr_t itemsBase = 0;
    uint32_t arraySize = 0;
    if (!GetGlobalItemArray(itemsBase, arraySize) || itemId >= arraySize) return nullptr;

    uintptr_t itemPtr = 0;
    if (!ReadPtr(itemsBase + static_cast<uintptr_t>(itemId) * 4u, itemPtr)) return nullptr;

    auto* item = reinterpret_cast<Item*>(itemPtr);
    __try {
        return item->item_id == itemId ? item : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

Item* FindItemByModelId(uint32_t modelId) {
    if (!modelId) return nullptr;

    __try {
        Inventory* inv = GetInventory();
        if (inv) {
            for (int b = 0; b < 23; ++b) {
                Bag* bag = inv->bags[b];
                if (!bag || !bag->items.buffer || bag->items.size == 0 || bag->items.size > 256) continue;
                for (uint32_t i = 0; i < bag->items.size; ++i) {
                    Item* item = bag->items.buffer[i];
                    if (item && item->model_id == modelId) return item;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    uintptr_t itemsBase = 0;
    uint32_t arraySize = 0;
    if (!GetGlobalItemArray(itemsBase, arraySize)) return nullptr;

    for (uint32_t i = 0; i < arraySize; ++i) {
        uintptr_t itemPtr = 0;
        if (!ReadPtr(itemsBase + static_cast<uintptr_t>(i) * 4u, itemPtr)) continue;

        auto* item = reinterpret_cast<Item*>(itemPtr);
        __try {
            if (item && item->item_id != 0 && item->model_id == modelId) return item;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
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
    uintptr_t ctx = 0, p1 = 0, p2 = 0, invStruct = 0;
    return ReadInventoryStruct(ctx, p1, p2, invStruct) ? invStruct : 0;
}

static void ClearCachedInventory(Inventory& inv) {
    for (auto& bag : inv.bags) bag = nullptr;
}

// AutoIt global item array:
// *BasePointer -> +0x18 -> +0x40 -> +0xB8 = Item** array
// *BasePointer -> +0x18 -> +0x40 -> +0xC0 = item array size
static bool GetGlobalItemArray(uintptr_t& itemsBase, uint32_t& arraySize) {
    itemsBase = 0;
    arraySize = 0;
    uintptr_t p0 = 0, p1 = 0, p2 = 0;
    if (!ReadInventoryChain(p0, p1, p2)) return false;
    if (!ReadPtr(p2 + 0xB8, itemsBase)) {
        if (!Offsets::RefreshBasePointer() || !ReadInventoryChain(p0, p1, p2) || !ReadPtr(p2 + 0xB8, itemsBase)) {
            return false;
        }
    }

    __try {
        arraySize = *reinterpret_cast<uint32_t*>(p2 + 0xC0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        itemsBase = 0;
        arraySize = 0;
        return false;
    }

    return itemsBase > 0x10000 && arraySize > 0 && arraySize <= 8192;
}

static bool PopulateInventoryFromBagRoot(Inventory& inv) {
    uintptr_t bagsBase = GetBagsArrayBase();
    if (!bagsBase) return false;

    bool foundAny = false;
    __try {
        for (int b = 0; b < 23; b++) {
            uintptr_t bagPtr = 0;
            if (ReadPtr(bagsBase + 4 * b, bagPtr)) {
                inv.bags[b] = reinterpret_cast<Bag*>(bagPtr);
                foundAny = true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return foundAny;
}

static bool PopulateInventoryFromGlobalItems(Inventory& inv) {
    uintptr_t itemsBase = 0;
    uint32_t arraySize = 0;
    if (!GetGlobalItemArray(itemsBase, arraySize)) return false;

    bool foundAny = false;
    for (uint32_t i = 0; i < arraySize; ++i) {
        uintptr_t itemPtr = 0;
        if (!ReadPtr(itemsBase + static_cast<uintptr_t>(i) * 4u, itemPtr)) continue;

        auto* item = reinterpret_cast<Item*>(itemPtr);
        __try {
            if (!item || item->item_id == 0) continue;
            Bag* bag = item->bag;
            if (!bag) continue;

            const uint32_t bagIndex = bag->index;
            if (bagIndex >= 23) continue;

            inv.bags[bagIndex] = bag;
            foundAny = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
    }

    return foundAny;
}

static void UpdateCachedGold(Inventory& inv) {
    uintptr_t ctx = 0, p1 = 0, p2 = 0, invStruct = 0;
    if (!ReadInventoryStruct(ctx, p1, p2, invStruct)) return;

    __try {
        inv.gold_character = *reinterpret_cast<uint32_t*>(invStruct + 0x90);
        inv.gold_storage = *reinterpret_cast<uint32_t*>(invStruct + 0x94);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
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
    ClearCachedInventory(s_cachedInventory);

    bool foundAny = PopulateInventoryFromBagRoot(s_cachedInventory);
    if (!foundAny) {
        foundAny = PopulateInventoryFromGlobalItems(s_cachedInventory);
    }
    if (foundAny) {
        UpdateCachedGold(s_cachedInventory);
        return &s_cachedInventory;
    }

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
    uintptr_t ctx = 0, p1 = 0, p2 = 0, invStruct = 0;
    if (ReadInventoryStruct(ctx, p1, p2, invStruct)) {
        __try {
            s_cachedInventory.gold_character = *reinterpret_cast<uint32_t*>(invStruct + 0x90);
            s_cachedInventory.gold_storage = *reinterpret_cast<uint32_t*>(invStruct + 0x94);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
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
