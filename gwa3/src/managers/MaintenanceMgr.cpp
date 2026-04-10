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
            // AutoIt: IsWeapon → GetRarity → check identified → sell
            if (!shouldSell && (IsWeapon(item) || IsArmor(item))) {
                uint16_t rarity = GetRarity(item);
                if (rarity == RARITY_WHITE || rarity == RARITY_BLUE ||
                    rarity == RARITY_PURPLE || rarity == RARITY_GOLD) {
                    if (IsIdentified(item)) {
                        shouldSell = true;
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
