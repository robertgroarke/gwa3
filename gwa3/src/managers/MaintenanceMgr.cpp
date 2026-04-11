// MaintenanceMgr — inventory maintenance between dungeon runs.
// Mirrors AutoIt Utils-Maintenance.au3 PerformMaintenance() flow.

#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/Item.h>

#include <Windows.h>

namespace GWA3::MaintenanceMgr {

// ===== Known Item Model IDs =====
static constexpr uint32_t MODEL_SALVAGE_KIT       = 2992; // Expert Salvage Kit
static constexpr uint32_t MODEL_BASIC_SALVAGE_KIT = 2989;
static constexpr uint32_t MODEL_SUP_ID_KIT        = 5899; // Superior Identification Kit
static constexpr uint32_t MODEL_ALT_ID_KIT        = 235;
static constexpr uint32_t MODEL_ALT_SALVAGE_KIT   = 243;
static constexpr uint32_t MODEL_ID_KIT            = 2991; // Regular ID Kit

// ===== Rare Skin Detection (GWA3-176) =====
// Ported from AutoIt RareSkins.au3 — ~200 model IDs that should never be sold or salvaged.
bool IsRareSkin(uint32_t modelId) {
    // Sorted array of rare skin model IDs for binary search
    static const uint32_t kRareSkins[] = {
        114, 117, 118, 127, 205, 332, 333, 336, 341, 342, 344, 391, 399, 528,
        773, 776, 777, 778, 789, 854, 855, 856, 858, 860, 861, 862, 874, 875,
        928, 942, 943, 944, 945, 947, 949, 951, 952, 953, 954, 955, 956, 958,
        959, 960, 985, 1022, 1052, 1195, 1271, 1315, 1316, 1320, 1321, 1350,
        1452, 1536, 1557, 1953, 1956, 1957, 1958, 1959, 1960, 1961, 1962,
        1963, 1964, 1965, 1966, 1967, 1968, 1969, 1970, 1971, 1972, 1973,
        1974, 1975, 1977, 1985, 1987, 1988, 1989, 1990, 1991, 1992, 1993,
        1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
        2005, 2006, 2007, 2008, 2009, 2039, 2058, 2062, 2129, 2236, 2237,
        2295, 2296, 2331, 2385, 2386, 2387, 2388, 2389, 2421, 2422, 2423,
        2435, 2436, 2437, 2438, 2439, 2440, 2441, 2442, 2443, 2444, 2472,
        2474, 3270, 5901, 5902, 5914, 5916, 5919, 5947, 5949, 6309, 6996,
        7995, 8003, 8084, 8085, 8257, 8431, 8509, 8515, 8547, 8554, 8555,
        8640, 8641, 8785, 11521, 15239, 19219, 19266, 19273, 19286, 19302,
        19310, 19323, 19337, 19338, 19344, 19364, 19379, 19380, 19382, 19385,
        19388, 19400, 19410, 19412, 19413, 19420, 19424, 19429, 21264, 21265,
        21267, 21271, 21272, 21273, 21280, 25918, 26901, 26902, 26910, 26925,
        26930, 26931, 26936, 26956, 26958, 26974, 26986, 26987, 26988, 27001,
        27005, 27006, 27017, 27022, 27030, 27031, 28314, 29110, 29113, 29114,
        29115, 29117, 29119, 30218, 30231, 31167, 35131, 35134, 35136, 35137,
        35139, 35141, 35142, 35143, 35145, 36676, 36985
    };
    static constexpr size_t kCount = sizeof(kRareSkins) / sizeof(kRareSkins[0]);
    // Binary search
    size_t lo = 0, hi = kCount;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (kRareSkins[mid] < modelId) lo = mid + 1;
        else if (kRareSkins[mid] > modelId) hi = mid;
        else return true;
    }
    return false;
}

// Kit and special item model IDs to never sell
static bool IsKit(uint32_t modelId) {
    return modelId == MODEL_SALVAGE_KIT ||
           modelId == MODEL_BASIC_SALVAGE_KIT ||
           modelId == MODEL_SUP_ID_KIT ||
           modelId == MODEL_ALT_ID_KIT ||
           modelId == MODEL_ALT_SALVAGE_KIT ||
           modelId == MODEL_ID_KIT;
}

// Basic material model IDs — from AutoIt GWA2_ID.au3
static constexpr uint32_t MAT_BONE              = 921;
static constexpr uint32_t MAT_IRON_INGOT        = 948;
static constexpr uint32_t MAT_TANNED_HIDE       = 940;
static constexpr uint32_t MAT_SCALE             = 953;
static constexpr uint32_t MAT_CHITIN            = 954;
static constexpr uint32_t MAT_BOLT_OF_CLOTH     = 925;
static constexpr uint32_t MAT_WOOD_PLANK        = 946;
static constexpr uint32_t MAT_GRANITE_SLAB      = 955;
static constexpr uint32_t MAT_GLITTERING_DUST   = 929;
static constexpr uint32_t MAT_PLANT_FIBER       = 934;
static constexpr uint32_t MAT_FEATHER           = 933;

// Materials to KEEP for conset crafting (AutoIt $KEEP_MATERIALS)
static bool IsKeepMaterial(uint32_t modelId) {
    return modelId == MAT_IRON_INGOT ||
           modelId == MAT_GLITTERING_DUST ||
           modelId == MAT_BONE ||
           modelId == MAT_FEATHER ||
           modelId == MAT_GRANITE_SLAB ||
           modelId == MAT_PLANT_FIBER ||
           modelId == MAT_SCALE;
}

// Materials to always SELL (AutoIt $SELL_MATERIALS)
static bool IsSellMaterial(uint32_t modelId) {
    return modelId == MAT_BOLT_OF_CLOTH ||
           modelId == MAT_TANNED_HIDE ||
           modelId == MAT_WOOD_PLANK ||
           modelId == MAT_CHITIN;
}

// Check if an item is a basic material (type 11 = materials in GW)
static bool IsBasicMaterial(Item* item) {
    if (!item) return false;
    return item->type == 11; // GW material type
}

// ===== Helpers =====

static void WaitMs(uint32_t ms) { Sleep(ms); }

uint32_t CountFreeSlots() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t freeSlots = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            if (!bag->items.buffer[i]) freeSlots++;
        }
    }
    return freeSlots;
}

uint32_t CountItemByModel(uint32_t modelId) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t total = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (item && item->model_id == modelId) {
                total += (item->quantity > 0) ? item->quantity : 1;
            }
        }
    }
    return total;
}

static uint32_t CountAllSalvageKits() {
    return CountItemByModel(MODEL_SALVAGE_KIT) +
           CountItemByModel(MODEL_BASIC_SALVAGE_KIT) +
           CountItemByModel(MODEL_ALT_SALVAGE_KIT);
}

static uint32_t CountAllIdKits() {
    return CountItemByModel(MODEL_SUP_ID_KIT) +
           CountItemByModel(MODEL_ALT_ID_KIT) +
           CountItemByModel(MODEL_ID_KIT);
}

// ===== Diagnostics =====

bool NeedsMaintenance(const Config& cfg) {
    uint32_t freeSlots = CountFreeSlots();
    if (freeSlots < cfg.minFreeSlots) {
        Log::Info("MaintenanceMgr: Needs maintenance — freeSlots=%u < %u", freeSlots, cfg.minFreeSlots);
        return true;
    }

    uint32_t idKits = CountAllIdKits();
    if (idKits < cfg.minIdKits) {
        Log::Info("MaintenanceMgr: Needs maintenance — idKits=%u < %u", idKits, cfg.minIdKits);
        return true;
    }

    uint32_t salvKits = CountAllSalvageKits();
    if (salvKits < cfg.minSalvageKits) {
        Log::Info("MaintenanceMgr: Needs maintenance — salvageKits=%u < %u", salvKits, cfg.minSalvageKits);
        return true;
    }

    uint32_t charGold = ItemMgr::GetGoldCharacter();
    if (charGold >= cfg.maxCharacterGold) {
        Log::Info("MaintenanceMgr: Needs maintenance — gold=%u >= %u", charGold, cfg.maxCharacterGold);
        return true;
    }

    return false;
}

// ===== Gold Management =====

void DepositGold(uint32_t keepOnChar) {
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t storageGold = ItemMgr::GetGoldStorage();

    if (charGold <= keepOnChar) return;

    uint32_t deposit = charGold - keepOnChar;
    // Cap at storage limit (1,000,000)
    if (storageGold + deposit > 1000000) {
        deposit = (storageGold < 1000000) ? (1000000 - storageGold) : 0;
    }
    if (deposit == 0) return;

    uint32_t newChar = charGold - deposit;
    uint32_t newStorage = storageGold + deposit;

    Log::Info("MaintenanceMgr: DepositGold %u (char: %u->%u, storage: %u->%u)",
              deposit, charGold, newChar, storageGold, newStorage);
    ItemMgr::ChangeGold(newChar, newStorage);
    WaitMs(500);
}

void WithdrawGold(uint32_t amount) {
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t storageGold = ItemMgr::GetGoldStorage();

    if (amount > storageGold) amount = storageGold;
    // Cap character gold at 100,000
    if (charGold + amount > 100000) amount = 100000 - charGold;
    if (amount == 0) return;

    uint32_t newChar = charGold + amount;
    uint32_t newStorage = storageGold - amount;

    Log::Info("MaintenanceMgr: WithdrawGold %u (char: %u->%u, storage: %u->%u)",
              amount, charGold, newChar, storageGold, newStorage);
    ItemMgr::ChangeGold(newChar, newStorage);
    WaitMs(500);
}

// ===== Xunlai Chest Interaction =====

void OpenXunlaiChest(float chestX, float chestY) {
    // Move to the Xunlai chest NPC
    GameThread::EnqueuePost([chestX, chestY]() {
        AgentMgr::Move(chestX, chestY);
    });
    for (int tick = 0; tick < 30; tick++) {
        WaitMs(500);
        auto* me = AgentMgr::GetMyAgent();
        if (me && AgentMgr::GetDistance(me->x, me->y, chestX, chestY) < 350.0f) break;
    }

    // Find the Xunlai NPC
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = 900.0f * 900.0f;
    uint32_t chestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 6 || living->hp <= 0.0f) continue;
        float d = AgentMgr::GetSquaredDistance(chestX, chestY, living->x, living->y);
        if (d < bestDist) { bestDist = d; chestId = living->agent_id; }
    }

    if (!chestId) {
        Log::Warn("MaintenanceMgr: No Xunlai chest NPC found near (%.0f, %.0f)", chestX, chestY);
        return;
    }

    // Use raw GoNPC packet (0x39) like AutoIt — matches the proven merchant interaction.
    // AgentMgr::InteractNPC uses the native function which opens a UI dialog that
    // disrupts player agent state (causes position reads to return 0,0).
    Log::Info("MaintenanceMgr: Opening Xunlai chest via GoNPC (agent=%u)", chestId);
    AgentMgr::ChangeTarget(chestId);
    WaitMs(250);
    CtoS::SendPacket(3, Packets::INTERACT_NPC, chestId, 0u);
    WaitMs(2000);

    // Close the storage dialog — we only need the chest "activated" for MoveItem packets.
    // The dialog itself blocks agent reads if left open.
    AgentMgr::CancelAction();
    WaitMs(500);
}

// ===== Material Storage Deposit =====

// Material storage slot mapping (bag 6) — from AutoIt MATERIALS_DOUBLE_ARRAY
// Maps model_id → slot index in material storage bag.
static int GetMaterialStorageSlot(uint32_t modelId) {
    switch (modelId) {
        case MAT_BONE:            return 1;
        case MAT_IRON_INGOT:      return 2;
        case MAT_TANNED_HIDE:     return 3;
        case MAT_SCALE:           return 4;
        case MAT_CHITIN:          return 5;
        case MAT_BOLT_OF_CLOTH:   return 6;
        case MAT_WOOD_PLANK:      return 7;
        // slot 8 = not used for basic materials
        case MAT_GRANITE_SLAB:    return 9;
        case MAT_GLITTERING_DUST: return 10;
        case MAT_PLANT_FIBER:     return 11;
        case MAT_FEATHER:         return 12;
        default: return -1; // not a basic material with known slot
    }
}

uint32_t DepositMaterialsToStorage() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t deposited = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;

            // Only deposit basic materials (type 11 = material)
            if (item->type != 11) continue;

            int slot = GetMaterialStorageSlot(item->model_id);
            if (slot < 0) continue; // not a known basic material

            // AutoIt uses 1-based slot, but MoveItem packet needs 0-based (slot - 1)
            int packetSlot = slot - 1;
            Log::Info("MaintenanceMgr: Depositing material=%u model=%u qty=%u to bag 6 slot %d (packet=%d)",
                      item->item_id, item->model_id, item->quantity, slot, packetSlot);
            ItemMgr::MoveItem(item->item_id, 6, packetSlot);
            WaitMs(300);
            deposited++;
        }
    }
    if (deposited > 0) {
        Log::Info("MaintenanceMgr: Deposited %u material stacks to storage", deposited);
    }
    return deposited;
}

// ===== Item Classification (matches AutoIt GWA2_ID_Items.au3) =====

// Rarity constants from name_enc first ushort (AutoIt GWA2_ID_Items.au3)
static constexpr uint16_t RARITY_WHITE  = 2621;
static constexpr uint16_t RARITY_GRAY   = 2622;
static constexpr uint16_t RARITY_BLUE   = 2623;
static constexpr uint16_t RARITY_GOLD   = 2624;
static constexpr uint16_t RARITY_PURPLE = 2626;
static constexpr uint16_t RARITY_GREEN  = 2627;

// Item type IDs (AutoIt GWA2_ID_Items.au3)
static constexpr uint8_t TYPE_AXE       = 2;
static constexpr uint8_t TYPE_FOOT      = 4;
static constexpr uint8_t TYPE_BOW       = 5;
static constexpr uint8_t TYPE_CHEST     = 7;
static constexpr uint8_t TYPE_MATERIAL  = 11;
static constexpr uint8_t TYPE_OFFHAND   = 12;
static constexpr uint8_t TYPE_HAND      = 13;
static constexpr uint8_t TYPE_HAMMER    = 15;
static constexpr uint8_t TYPE_HEAD      = 16;
static constexpr uint8_t TYPE_LEG       = 19;
static constexpr uint8_t TYPE_WAND      = 22;
static constexpr uint8_t TYPE_SHIELD    = 24;
static constexpr uint8_t TYPE_STAFF     = 26;
static constexpr uint8_t TYPE_SWORD     = 27;
static constexpr uint8_t TYPE_DAGGER    = 32;
static constexpr uint8_t TYPE_SCYTHE    = 35;
static constexpr uint8_t TYPE_SPEAR     = 36;

// GetRarity: reads name_enc first ushort (AutoIt GetRarity at offset +56)
// Our Item struct has name_enc at offset +0x34 = 52, but complete_name_enc at +0x38 = 56
// AutoIt reads ptr at offset 56 (complete_name_enc), then reads ushort at that ptr.
static uint16_t GetRarity(Item* item) {
    if (!item) return 0;
    // AutoIt: MemoryRead(GetItemPtr($aItem) + 56, "ptr") → name string → ushort
    // In our struct: +56 = 0x38 = complete_name_enc
    wchar_t* nameStr = item->complete_name_enc;
    if (!nameStr) nameStr = item->name_enc; // fallback
    if (!nameStr) return 0;
    return static_cast<uint16_t>(nameStr[0]);
}

// IsIdentified: checks interaction field bit 0 (AutoIt: Interaction & 0x1)
static bool IsIdentified(Item* item) {
    if (!item) return false;
    return (item->interaction & 0x1) != 0;
}

// IsWeapon: checks if item type is a weapon type
static bool IsWeapon(Item* item) {
    if (!item) return false;
    switch (item->type) {
        case TYPE_AXE: case TYPE_BOW: case TYPE_OFFHAND: case TYPE_HAMMER:
        case TYPE_WAND: case TYPE_SHIELD: case TYPE_STAFF: case TYPE_SWORD:
        case TYPE_DAGGER: case TYPE_SCYTHE: case TYPE_SPEAR:
            return true;
        default: return false;
    }
}

// IsArmor: checks if item type is an armor type
static bool IsArmor(Item* item) {
    if (!item) return false;
    switch (item->type) {
        case TYPE_FOOT: case TYPE_CHEST: case TYPE_HAND: case TYPE_HEAD: case TYPE_LEG:
            return true;
        default: return false;
    }
}

// ===== Sell Items =====

// ShouldSellItemForMaintenance — mirrors AutoIt Utils-Maintenance.au3 line 750
// Sells: identified weapons of white/blue/purple/gold rarity (not rare skins)
//        + materials in the SELL list
// Keeps: kits, unidentified items, green/red items, rare skins
bool ShouldSellItem(uint32_t modelId, uint16_t value, uint8_t type) {
    (void)value;
    (void)type;
    if (IsKit(modelId)) return false;
    return true;
}

uint32_t SellJunkItems() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t merchantItems = TradeMgr::GetMerchantItemCount();
    if (merchantItems == 0) {
        Log::Warn("MaintenanceMgr: SellJunkItems — merchant not open");
        return 0;
    }

    uint32_t soldCount = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;
            if (item->value == 0) continue;
            if (IsKit(item->model_id)) continue;

            bool shouldSell = false;

            // 1. Sell materials in the SELL list (cloth, hide, wood, chitin)
            if (IsSellMaterial(item->model_id)) {
                shouldSell = true;
            }

            // 2. Sell non-keep basic materials
            if (!shouldSell && IsBasicMaterial(item) && !IsKeepMaterial(item->model_id)) {
                shouldSell = true;
            }

            // 3. Sell identified weapons/armor of white/blue/purple/gold rarity
            // AutoIt: IsWeapon → GetRarity → check rare skin → check identified → sell
            if (!shouldSell && (IsWeapon(item) || IsArmor(item))) {
                if (!IsRareSkin(item->model_id)) {
                    uint16_t rarity = GetRarity(item);
                    if (rarity == RARITY_WHITE || rarity == RARITY_BLUE ||
                        rarity == RARITY_PURPLE || rarity == RARITY_GOLD) {
                        if (IsIdentified(item)) {
                            shouldSell = true;
                        }
                    }
                }
            }

            // 4. Sell trophies (type 30) — common dungeon drops
            if (!shouldSell && item->type == 30 && item->value > 0) {
                shouldSell = true;
            }

            if (!shouldSell) continue;

            uint32_t qty = (item->quantity > 0) ? item->quantity : 1;
            Log::Info("MaintenanceMgr: Selling item=%u model=%u value=%u qty=%u type=%u rarity=%u",
                      item->item_id, item->model_id, item->value, qty, item->type, GetRarity(item));
            TradeMgr::SellInventoryItem(item->item_id, qty);
            WaitMs(300);
            soldCount++;
        }
    }
    Log::Info("MaintenanceMgr: Sold %u items", soldCount);
    return soldCount;
}

// ===== Item Identification (GWA3-178) =====
// Mirrors AutoIt IdentifyUnidentifiedItemsForMaintenance():
// scan bags 1-4, skip rare skins, identify with ID kit.

static Item* FindIdKit() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return nullptr;
    for (uint32_t b = 1; b <= 4; b++) {
        Bag* bag = inv->bags[b];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            Item* item = bag->items.buffer[s];
            if (!item) continue;
            if (item->model_id == MODEL_SUP_ID_KIT || item->model_id == MODEL_ID_KIT ||
                item->model_id == MODEL_ALT_ID_KIT) return item;
        }
    }
    return nullptr;
}

static Item* FindSalvageKit() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return nullptr;
    for (uint32_t b = 1; b <= 4; b++) {
        Bag* bag = inv->bags[b];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            Item* item = bag->items.buffer[s];
            if (!item) continue;
            if (item->model_id == MODEL_SALVAGE_KIT || item->model_id == MODEL_BASIC_SALVAGE_KIT ||
                item->model_id == MODEL_ALT_SALVAGE_KIT) return item;
        }
    }
    return nullptr;
}

uint32_t IdentifyAllItems() {
    Item* kit = FindIdKit();
    if (!kit) {
        Log::Warn("MaintenanceMgr: No ID kit found in inventory");
        return 0;
    }

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t identified = 0;
    for (uint32_t b = 1; b <= 4; b++) {
        Bag* bag = inv->bags[b];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            Item* item = bag->items.buffer[s];
            if (!item || item->model_id == 0) continue;
            if (IsIdentified(item)) continue;
            if (IsKit(item->model_id)) continue;
            // Skip rare skins — don't identify, preserves value
            if (IsRareSkin(item->model_id)) continue;

            Log::Info("MaintenanceMgr: Identifying item=%u model=%u type=%u with kit=%u",
                      item->item_id, item->model_id, item->type, kit->item_id);
            ItemMgr::IdentifyItem(item->item_id, kit->item_id);
            WaitMs(1000);
            identified++;

            // Re-find kit (it may have been consumed)
            kit = FindIdKit();
            if (!kit) {
                Log::Warn("MaintenanceMgr: ID kit exhausted after %u identifications", identified);
                return identified;
            }
        }
    }
    Log::Info("MaintenanceMgr: Identified %u items", identified);
    return identified;
}

// ===== Salvage (GWA3-179) =====
// Uses the native Salvage function directly, matching AutoIt's CommandSalvage shellcode.
// AutoIt: writes item_id + kit_id to SalvageGlobal, calls Salvage(session_id, kit_id, item_id).
// This avoids the kPreStartSalvage UI message which corrupts game state.

// Read the salvage session ID from the game's pointer chain.
// AutoIt: MemoryReadPtr(base_address_ptr, [0, 0x18, 0x2C, 0x690])
static uint32_t GetSalvageSessionId() {
    if (!Offsets::BasePointer) return 0;
    __try {
        uintptr_t p = Offsets::BasePointer;
        p = *reinterpret_cast<uintptr_t*>(p + 0x18);
        if (!p) return 0;
        p = *reinterpret_cast<uintptr_t*>(p + 0x2C);
        if (!p) return 0;
        return *reinterpret_cast<uint32_t*>(p + 0x690);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// Execute a salvage command matching the AutoIt CommandSalvage shellcode exactly.
// AutoIt's command queue calls with eax = pointer to data block:
//   [eax+0] = command function ptr (cleared to 0 before call)
//   [eax+4] = item_id
//   [eax+8] = kit_id
//   [eax+C] = session_id
// CommandSalvage reads these, writes item_id/kit_id to SalvageGlobal,
// then calls Salvage(session_id, kit_id, item_id) via push/call.
static void ExecuteSalvageCommand(uint32_t itemId, uint32_t kitId, uint32_t sessionId) {
    if (!Offsets::Salvage || !Offsets::SalvageGlobal) return;

    // Write item_id and kit_id to SalvageGlobal (matches AutoIt shellcode lines 1976-1981)
    uint32_t* global = reinterpret_cast<uint32_t*>(Offsets::SalvageGlobal);
    global[0] = itemId;
    global[1] = kitId;

    // Call Salvage function with 3 args on stack (matches AutoIt lines 1982-1989)
    // push item_id; push kit_id; push session_id; call Salvage; add esp, 0xC
    uintptr_t fn = Offsets::Salvage;
    uint32_t sId = sessionId, kId = kitId, iId = itemId;
    __asm {
        push iId
        push kId
        push sId
        call fn
        add esp, 0xC
    }
}

// Wait for the bags array pointer to become non-null after salvage.
// The Salvage function temporarily zeroes p2+0xF8 during processing.
static void WaitForBagsPointerRestore() {
    __try {
        uintptr_t bp = Offsets::BasePointer;
        uintptr_t ctx = bp ? *reinterpret_cast<uintptr_t*>(bp) : 0;
        uintptr_t p1 = ctx ? *reinterpret_cast<uintptr_t*>(ctx + 0x18) : 0;
        uintptr_t p2 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x40) : 0;

        Log::Info("MaintenanceMgr: POST-SALVAGE: p2=0x%08X bags=0x%08X",
                  p2, p2 ? *reinterpret_cast<uintptr_t*>(p2 + 0xF8) : 0);

        for (int wait = 0; wait < 30; wait++) {
            uintptr_t bags = p2 ? *reinterpret_cast<uintptr_t*>(p2 + 0xF8) : 0;
            if (bags != 0) {
                Log::Info("MaintenanceMgr: Bags pointer restored after %d ms (bags=0x%08X)", wait * 100, bags);
                return;
            }
            Sleep(100);
        }
        Log::Warn("MaintenanceMgr: Bags pointer still NULL after 3s");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Error("MaintenanceMgr: Exception reading pointer chain after salvage");
    }
}

uint32_t SalvageJunkItems() {
    Item* kit = FindSalvageKit();
    if (!kit) {
        Log::Warn("MaintenanceMgr: No salvage kit found in inventory");
        return 0;
    }

    // Phase 1: Collect item IDs to salvage (DON'T modify inventory during scan)
    uint32_t toSalvage[64];
    uint32_t toSalvageCount = 0;
    {
        Inventory* inv = ItemMgr::GetInventory();
        if (!inv) return 0;

        for (uint32_t b = 1; b <= 4 && toSalvageCount < 64; b++) {
            Bag* bag = inv->bags[b];
            if (!bag || !bag->items.buffer) continue;
            for (uint32_t s = 0; s < bag->items.size && toSalvageCount < 64; s++) {
                Item* item = bag->items.buffer[s];
                if (!item || item->model_id == 0) continue;
                if (IsKit(item->model_id)) continue;
                if (IsRareSkin(item->model_id)) continue;
                if (!IsIdentified(item)) continue;
                if (!(IsWeapon(item) || IsArmor(item))) continue;

                uint16_t rarity = GetRarity(item);
                if (rarity != RARITY_WHITE && rarity != RARITY_BLUE) continue;
                if (item->is_material_salvageable == 0) continue;

                toSalvage[toSalvageCount++] = item->item_id;
            }
        }
    }

    if (toSalvageCount == 0) {
        Log::Info("MaintenanceMgr: No items to salvage");
        return 0;
    }
    if (toSalvageCount > 10) toSalvageCount = 10;
    Log::Info("MaintenanceMgr: Salvaging %u items (capped at 10)", toSalvageCount);

    // Phase 2: Salvage each item using native function call
    uint32_t salvaged = 0;
    for (uint32_t i = 0; i < toSalvageCount; i++) {
        uint32_t itemId = toSalvage[i];
        kit = FindSalvageKit();
        if (!kit) {
            Log::Warn("MaintenanceMgr: Salvage kit exhausted after %u salvages", salvaged);
            break;
        }

        Item* item = ItemMgr::GetItemById(itemId);
        if (!item || item->model_id == 0) continue;

        uint32_t sessionId = GetSalvageSessionId();
        if (sessionId == 0) {
            Log::Warn("MaintenanceMgr: Salvage session ID is 0, skipping");
            continue;
        }

        uint32_t kitId = kit->item_id;
        // PRE-SALVAGE: trace the pointer chain
        {
            uintptr_t bp = Offsets::BasePointer;
            uintptr_t ctx = bp ? *reinterpret_cast<uintptr_t*>(bp) : 0;
            uintptr_t p1 = ctx ? *reinterpret_cast<uintptr_t*>(ctx + 0x18) : 0;
            uintptr_t p2 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x40) : 0;
            uintptr_t bags = p2 ? *reinterpret_cast<uintptr_t*>(p2 + 0xF8) : 0;
            uint32_t gold = p2 ? *reinterpret_cast<uint32_t*>(p2 + 0x90) : 0;
            Log::Info("MaintenanceMgr: PRE-SALVAGE chain: BP=0x%08X ctx=0x%08X p1=0x%08X p2=0x%08X bags=0x%08X gold=%u",
                      bp, ctx, p1, p2, bags, gold);
        }
        Log::Info("MaintenanceMgr: Salvaging [%u/%u] item=%u model=%u kit=%u session=%u",
                  i + 1, toSalvageCount, itemId, item->model_id, kitId, sessionId);

        // Open the salvage session on game thread
        GameThread::EnqueuePost([itemId, kitId, sessionId]() {
            ExecuteSalvageCommand(itemId, kitId, sessionId);
        });
        // Wait for session to stabilize before sending SalvageMaterials
        WaitMs(2000);

        // Send SalvageMaterials — via CtoS ring buffer.
        // This triggers the server to process the salvage and send back
        // StoC inventory updates that restore the bags pointer.
        CtoS::SendPacket(1, Packets::SALVAGE_MATERIALS);

        // Wait for server response to rebuild bags pointer (up to 3s)
        WaitMs(1000);
        WaitForBagsPointerRestore();

        salvaged++;
    }
    if (salvaged > 0) WaitMs(1000);
    Log::Info("MaintenanceMgr: Salvaged %u items (freeSlots now=%u)", salvaged, CountFreeSlots());
    return salvaged;
}

// ===== Kit Management =====

void BuyKitsToTarget(const Config& cfg) {
    uint32_t currentIdKits = CountAllIdKits();
    uint32_t currentSalvKits = CountAllSalvageKits();

    if (currentIdKits >= cfg.targetIdKits && currentSalvKits >= cfg.targetSalvageKits) {
        Log::Info("MaintenanceMgr: Kits sufficient (id=%u/%u salv=%u/%u)",
                  currentIdKits, cfg.targetIdKits, currentSalvKits, cfg.targetSalvageKits);
        return;
    }

    // Check merchant is open
    uint32_t merchantItems = TradeMgr::GetMerchantItemCount();
    if (merchantItems == 0) {
        Log::Warn("MaintenanceMgr: BuyKitsToTarget — merchant not open");
        return;
    }

    // Buy ID kits if needed
    if (currentIdKits < cfg.targetIdKits) {
        uint32_t need = cfg.targetIdKits - currentIdKits;
        // Try Superior ID Kit first (model 5899), then regular (2991)
        bool bought = TradeMgr::BuyMerchantItemByModelId(MODEL_SUP_ID_KIT, need);
        if (!bought) bought = TradeMgr::BuyMerchantItemByModelId(MODEL_ID_KIT, need);
        if (bought) {
            Log::Info("MaintenanceMgr: Bought %u ID kits", need);
            WaitMs(1000);
        } else {
            Log::Warn("MaintenanceMgr: Could not buy ID kits");
        }
    }

    // Buy salvage kits if needed
    if (currentSalvKits < cfg.targetSalvageKits) {
        uint32_t need = cfg.targetSalvageKits - currentSalvKits;
        // Try basic kit first (2989), then expert (2992)
        bool bought = TradeMgr::BuyMerchantItemByModelId(MODEL_BASIC_SALVAGE_KIT, need);
        if (!bought) bought = TradeMgr::BuyMerchantItemByModelId(MODEL_SALVAGE_KIT, need);
        if (bought) {
            Log::Info("MaintenanceMgr: Bought %u salvage kits", need);
            WaitMs(1000);
        } else {
            Log::Warn("MaintenanceMgr: Could not buy salvage kits");
        }
    }
}

// ===== Full Maintenance =====

void PerformMaintenance(const Config& cfg) {
    Log::Info("MaintenanceMgr: Starting maintenance (freeSlots=%u idKits=%u salvKits=%u gold=%u/%u)",
              CountFreeSlots(), CountAllIdKits(), CountAllSalvageKits(),
              ItemMgr::GetGoldCharacter(), ItemMgr::GetGoldStorage());

    // Step 1: Deposit excess gold
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    if (charGold > 80000) {
        DepositGold(10000);
    }

    // Step 2: Deposit materials to material storage (bag 6)
    // Requires Xunlai chest to be open — move there and interact first.
    // Gadd's Encampment Xunlai chest coordinates.
    static constexpr float kXunlaiX = -10481.0f;
    static constexpr float kXunlaiY = -22787.0f;

    // Check if there are any materials to deposit before moving to chest
    bool hasMaterials = false;
    {
        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (uint32_t b = 1; b <= 4 && !hasMaterials; b++) {
                Bag* bag = inv->bags[b];
                if (!bag || !bag->items.buffer) continue;
                for (uint32_t s = 0; s < bag->items.size; s++) {
                    Item* item = bag->items.buffer[s];
                    if (item && item->type == 11 && GetMaterialStorageSlot(item->model_id) >= 0) {
                        hasMaterials = true;
                        break;
                    }
                }
            }
        }
    }

    // Only visit Xunlai if we're critically low on space AND have materials to deposit.
    // Visiting Xunlai moves the player away from the merchant, breaking sell/buy flow.
    uint32_t currentFreeSlots = CountFreeSlots();
    if (hasMaterials && currentFreeSlots < cfg.minFreeSlots) {
        OpenXunlaiChest(kXunlaiX, kXunlaiY);
        uint32_t deposited = DepositMaterialsToStorage();
        if (deposited > 0) {
            WaitMs(1000 + deposited * 200);
            Log::Info("MaintenanceMgr: After deposit: freeSlots=%u", CountFreeSlots());
        }

        // Verify agent state is valid after Xunlai operations
        for (int retry = 0; retry < 10; retry++) {
            auto* me = AgentMgr::GetMyAgent();
            if (me && me->x != 0.0f && me->y != 0.0f) {
                Log::Info("MaintenanceMgr: Agent valid after Xunlai (pos=%.0f, %.0f)", me->x, me->y);
                break;
            }
            Log::Warn("MaintenanceMgr: Agent invalid after Xunlai, waiting...");
            WaitMs(500);
        }
    }

    // Step 3: Identify unidentified items (needed before sell/salvage decisions)
    uint32_t identified = IdentifyAllItems();
    if (identified > 0) WaitMs(500);

    // Step 4: Sell junk items FIRST to free inventory space (requires merchant open)
    uint32_t sold = SellJunkItems();
    if (sold > 0) WaitMs(500);

    // Step 5: Salvage — temporarily disabled while investigating crash rate spike.
    // if (CountFreeSlots() >= 2) {
    //     uint32_t salvaged = SalvageJunkItems();
    //     if (salvaged > 0) WaitMs(500);
    // }

    // Step 6: Buy kits to target (requires merchant to be open)
    BuyKitsToTarget(cfg);

    // Step 7: Final gold deposit
    charGold = ItemMgr::GetGoldCharacter();
    if (charGold > 10000) {
        DepositGold(5000);
    }

    Log::Info("MaintenanceMgr: Maintenance complete (freeSlots=%u idKits=%u salvKits=%u gold=%u/%u)",
              CountFreeSlots(), CountAllIdKits(), CountAllSalvageKits(),
              ItemMgr::GetGoldCharacter(), ItemMgr::GetGoldStorage());
}

} // namespace GWA3::MaintenanceMgr
