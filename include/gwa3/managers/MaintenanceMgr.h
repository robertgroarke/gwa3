#pragma once

// GWA3 MaintenanceMgr — inventory maintenance, gold management, kit restocking.
// Mirrors AutoIt Utils-Maintenance.au3 PerformMaintenance() flow.
// Called between dungeon runs to keep inventory clear and kits stocked.

#include <cstdint>

namespace GWA3 {
    struct Item;
}

namespace GWA3::MaintenanceMgr {

    // Rarity IDs from the encoded item name prefix. Kept public so profile
    // handoff code can build native maintenance policies without duplicating
    // magic values.
    static constexpr uint16_t kItemRarityAny    = 0;
    static constexpr uint16_t kItemRarityWhite  = 2621;
    static constexpr uint16_t kItemRarityGray   = 2622;
    static constexpr uint16_t kItemRarityBlue   = 2623;
    static constexpr uint16_t kItemRarityGold   = 2624;
    static constexpr uint16_t kItemRarityPurple = 2626;
    static constexpr uint16_t kItemRarityGreen  = 2627;

    constexpr uint64_t UpgradeSalvageItemTypeMask(uint8_t itemType) {
        return itemType < 64 ? (uint64_t{1} << itemType) : uint64_t{0};
    }

    struct UpgradeSalvageRule {
        bool enabled = true;
        const char* name = nullptr;
        const char* modPatternHex = nullptr;
        uint8_t salvageIndex = 2;  // 0=prefix, 1=suffix/rune, 2=inscription.
        uint64_t itemTypeMask = 0; // 0 means any item type.
        uint16_t minimumRarity = kItemRarityGold;
    };

    struct UpgradeSalvageMatch {
        bool matched = false;
        uint8_t salvageIndex = 2;
        const char* ruleName = nullptr;
    };

    // ===== Configuration =====
    struct Config {
        uint32_t minFreeSlots                = 5;
        uint32_t minIdKits                   = 1;
        uint32_t minSalvageKits              = 1;
        uint32_t maxCharacterGold            = 95000;
        uint32_t targetIdKits                = 3;
        uint32_t targetSalvageKits           = 8;  // Regular/basic salvage kits.
        uint32_t targetExpertSalvageKits     = 1;  // Superior/expert salvage kit for gold salvage.
        // Owned by the dungeon module. When 0, conversion paths return to the
        // map they started from instead of assuming a shared maintenance town.
        uint32_t maintenanceTown             = 0;
        // Optional town service coordinates owned by the dungeon module. These
        // disable town-specific storage/material-trader steps when left unset.
        float xunlaiChestX                   = 0.0f;
        float xunlaiChestY                   = 0.0f;
        float materialTraderX                = 0.0f;
        float materialTraderY                = 0.0f;
        uint16_t materialTraderPlayerNumber  = 0;
        uint32_t depositKeepOnChar           = 5000;
        uint32_t depositWhenCharacterGoldAtLeast = 80000;
        uint32_t consetStorageGoldTrigger    = 800000;
        uint32_t consetStorageGoldFloor      = 600000;
        uint32_t targetStoredConsetsEach     = 25;
        uint32_t targetCharacterConsetsEach  = 1;
        uint32_t consetBatchSets             = 10;
        uint32_t consetWithdrawGoldTarget    = 100000;
        // Count occupied inventory slots containing conset materials. When this
        // threshold is reached, craft carried materials into consets to clear bag
        // pressure even if stored consets are already above target.
        uint32_t consetMaterialStackTrigger  = 10;
        // Kept as a UI/status hint and emergency free-slot fallback. The primary
        // inventory-material crafting trigger is consetMaterialStackTrigger.
        uint32_t consetMaterialPressureFreeSlots = 10;
        bool enableConsetRestock             = true;
        bool salvageMatchedUpgradesBeforeMaterials = true;
        bool salvageUnmatchedGoldWeaponsForMaterials = false;
        const UpgradeSalvageRule* upgradeSalvageRules = nullptr;
        uint32_t upgradeSalvageRuleCount      = 0;
    };

    // ===== Rare Skin Detection =====

    // Check if a weapon/armor model ID is a rare skin (should not be sold or salvaged).
    // Ported from AutoIt RareSkins.au3 — ~200 known rare weapon skins.
    bool IsRareSkin(uint32_t modelId);

    // ===== Diagnostics =====

    // Check if maintenance is needed (free slots, kit counts, gold).
    bool NeedsMaintenance(const Config& cfg = {});

    // Get current free inventory slots across bags 1-4.
    uint32_t CountFreeSlots();

    // Count items matching a model ID across bags 1-4.
    uint32_t CountItemByModel(uint32_t modelId);

    // Count items matching a model ID across Xunlai storage panes (bags 8-16).
    uint32_t CountItemByModelInStorage(uint32_t modelId);

    bool HasCharacterConsetSet(uint32_t targetEach = 1);
    bool NeedsCharacterConsetRestock(const Config& cfg = {});
    uint32_t WithdrawMissingConsetsFromStorage(uint32_t targetEach = 1);

    // ===== Gold Management =====

    // Deposit gold to storage. Keeps `keepOnChar` gold on character.
    void DepositGold(uint32_t keepOnChar = 10000);

    // Withdraw gold from storage up to `amount`.
    void WithdrawGold(uint32_t amount);

    // ===== Storage Deposit =====

    // Deposit known basic and rare materials from backpack (bags 1-4) to material storage (bag 6).
    // Requires Xunlai chest to be open. Returns number of stacks deposited.
    uint32_t DepositMaterialsToStorage();

    // Deposit matching item models from backpack (bags 1-4) into Xunlai storage bags 8-16.
    // Returns number of stacks moved.
    uint32_t DepositItemModelsToStorage(const uint32_t* modelIds, uint32_t modelCount);

    // Open the Xunlai chest NPC at given coordinates (move to, interact).
    void OpenXunlaiChest(float chestX, float chestY);

    // ===== Sell Items =====

    // Sell all junk items to the currently-open merchant.
    // Returns number of items sold.
    // Junk = identified whites/blues/purples, excluding kits and rare skins.
    uint32_t SellJunkItems();

    // Sell safe emergency cleanup items at an open merchant until at least
    // minFreeSlots are available. Prefers white weapons, then low-grade
    // salvage kits. Returns number of items sold.
    uint32_t SellEmergencyItemsForFreeSlots(uint32_t minFreeSlots);

    // Check if item should be sold as junk (filter function).
    bool ShouldSellItem(const Item* item);

    // Check whether an identified item matches the configured useful-upgrade
    // salvage matrix. Uses GWA2-style raw mod-struct hex patterns.
    UpgradeSalvageMatch FindUpgradeSalvageMatch(const Item* item, const Config& cfg = {});

    // ===== Kit Management =====

    // Buy ID and salvage kits from the currently-open merchant to reach targets.
    void BuyKitsToTarget(const Config& cfg = {});

    // Convert excess Xunlai gold into consets via Embark Beach material trader/crafters.
    // Returns true when the conversion path ran successfully enough to continue.
    bool ConvertExcessStorageGoldToConsets(const Config& cfg = {});

    // ===== Item Identification =====

    // Identify all unidentified items in bags 1-4.
    // Skips rare skins (never identify those — preserves value).
    // Requires an ID kit in inventory. Returns number of items identified.
    uint32_t IdentifyAllItems();

    // ===== Salvage =====

    // Salvage all non-rare, identified white/blue items in bags 1-4.
    // Uses the native salvage command path that mirrors AutoIt's
    // CommandSalvage shellcode.
    // Requires a salvage kit in inventory. Returns number of items salvaged.
    uint32_t SalvageJunkItems();

    // Post-run gold cleanup flow:
    // 1. identify all non-rare items in bags 1-4
    // 2. salvage eligible identified gold weapon/offhand/shield items
    // Mirrors the legacy dungeon Boss()->SalvageItems() pattern.
    // Returns number of gold items salvaged.
    uint32_t IdentifyAndSalvageGoldItems();
    uint32_t IdentifyAndSalvageGoldItems(const Config& cfg);

    // Run one native salvage command for a specific item/kit pair.
    // Returns true when the command was queued and survived long enough to
    // observe completion or post-command stabilization.
    bool SalvageItemNative(uint32_t kitId, uint32_t itemId, bool confirmByEnter = false);

    // Run one native upgrade salvage command for a specific item/kit/mod slot.
    // modIndex mirrors GWA2 SalvageMod: 0=prefix, 1=suffix/rune, 2=inscription.
    bool SalvageUpgradeNative(uint32_t kitId, uint32_t itemId, uint8_t modIndex, bool confirmByEnter = false);

    // Run one legacy AutoIt-like botshub salvage command for a specific
    // item/kit pair. This uses the naked salvage command stub/return path
    // rather than the C++ game-command callback path.
bool SalvageItemLegacyBotshub(uint32_t kitId, uint32_t itemId, uint32_t followupHeader = 0u, bool followupAfterConsume = false, bool sendMaterials = true, bool sendEnterAfterConsume = false);

    // Harness/debug helper that mirrors AutoIt's item-pointer driven wait
    // semantics more closely: the caller provides the original captured
    // ground-truth Item* and success is determined from that pointer going to
    // item_id==0, without falling back to inventory lookups mid-chain.
bool SalvageItemLegacyBotshubTracked(uint32_t kitId, uint32_t itemId, Item* trackedItem, bool waitForInventoryRestoreAfterConsume = false, uint32_t forcedSessionId = 0u, bool allowZeroSession = false);

    // ===== Full Maintenance =====

    // Run the full maintenance sequence:
    // 1. Deposit excess gold
    // 2. Sell junk items to merchant
    // 3. Buy kits to target
    // Assumes player is in maintenance town with merchant accessible.
    void PerformMaintenance(const Config& cfg = {});

} // namespace GWA3::MaintenanceMgr
