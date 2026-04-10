// MaintenanceMgr — inventory maintenance between dungeon runs.
// Mirrors AutoIt Utils-Maintenance.au3 PerformMaintenance() flow.

#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/GameThread.h>
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

// ===== Sell Items =====

// Item rarity from name_enc first ushort (matches AutoIt pattern)
static uint8_t GetItemRarity(Item* item) {
    if (!item || !item->name_enc) return 0;
    uint16_t first = item->name_enc[0];
    // GW rarity encoding: 0xA40 = white, 0xA41 = blue, 0xA42 = purple, 0xA43 = gold
    switch (first) {
        case 0x0108: return 1; // white
        case 0x010A: return 2; // blue
        case 0x010B: return 3; // purple
        case 0x010C: return 4; // gold
        default:     return 0; // unknown
    }
}

bool ShouldSellItem(uint32_t modelId, uint16_t value, uint8_t type) {
    (void)value;
    (void)type;
    // Never sell kits
    if (IsKit(modelId)) return false;
    return true;
}

uint32_t SellJunkItems() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    // Check merchant is open
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

            // Never sell kits
            if (IsKit(item->model_id)) continue;

            // Only sell items with a sell value
            if (item->value == 0) continue;

            bool shouldSell = false;

            // Sell non-keep materials (AutoIt ShouldSellMaterialForMaintenance)
            if (IsBasicMaterial(item)) {
                if (IsKeepMaterial(item->model_id)) continue; // keep for consets
                shouldSell = true;
            }

            // Sell explicitly-listed materials regardless of type
            if (IsSellMaterial(item->model_id)) {
                shouldSell = true;
            }

            // Sell identified weapons/armor: whites, blues, purples
            if (!shouldSell) {
                uint8_t rarity = GetItemRarity(item);
                if (rarity >= 1 && rarity <= 3) shouldSell = true;
            }

            if (!shouldSell) continue;

            uint32_t qty = (item->quantity > 0) ? item->quantity : 1;
            Log::Info("MaintenanceMgr: Selling item=%u model=%u value=%u qty=%u type=%u",
                      item->item_id, item->model_id, item->value, qty, item->type);
            TradeMgr::SellMerchantItem(item->item_id, qty, item->value * qty);
            WaitMs(300);
            soldCount++;
        }
    }
    Log::Info("MaintenanceMgr: Sold %u items", soldCount);
    return soldCount;
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

    // Step 2: Sell junk items (requires merchant to be open)
    uint32_t sold = SellJunkItems();
    if (sold > 0) WaitMs(500);

    // Step 3: Buy kits to target (requires merchant to be open)
    BuyKitsToTarget(cfg);

    // Step 4: Final gold deposit
    charGold = ItemMgr::GetGoldCharacter();
    if (charGold > 10000) {
        DepositGold(5000);
    }

    Log::Info("MaintenanceMgr: Maintenance complete (freeSlots=%u idKits=%u salvKits=%u gold=%u/%u)",
              CountFreeSlots(), CountAllIdKits(), CountAllSalvageKits(),
              ItemMgr::GetGoldCharacter(), ItemMgr::GetGoldStorage());
}

} // namespace GWA3::MaintenanceMgr
