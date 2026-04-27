#pragma once

// GWA3 MaintenanceMgr — inventory maintenance, gold management, kit restocking.
// Mirrors AutoIt Utils-Maintenance.au3 PerformMaintenance() flow.
// Called between dungeon runs to keep inventory clear and kits stocked.

#include <cstdint>

namespace GWA3 {
    struct Item;
}

namespace GWA3::MaintenanceMgr {

    // ===== Configuration =====
    struct Config {
        uint32_t minFreeSlots                = 7;
        uint32_t minIdKits                   = 1;
        uint32_t minSalvageKits              = 1;
        uint32_t maxCharacterGold            = 95000;
        uint32_t targetIdKits                = 3;
        uint32_t targetSalvageKits           = 10;
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
        uint32_t consetBatchSets             = 10;
        uint32_t consetWithdrawGoldTarget    = 100000;
        uint32_t consetMaterialStackTrigger  = 10;
        uint32_t consetMaterialPressureFreeSlots = 10;
        bool enableConsetRestock             = true;
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

    // ===== Gold Management =====

    // Deposit gold to storage. Keeps `keepOnChar` gold on character.
    void DepositGold(uint32_t keepOnChar = 10000);

    // Withdraw gold from storage up to `amount`.
    void WithdrawGold(uint32_t amount);

    // ===== Storage Deposit =====

    // Deposit basic materials from backpack (bags 1-4) to material storage (bag 6).
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

    // Check if item should be sold as junk (filter function).
    bool ShouldSellItem(const Item* item);

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

    // Run one native salvage command for a specific item/kit pair.
    // Returns true when the command was queued and survived long enough to
    // observe completion or post-command stabilization.
    bool SalvageItemNative(uint32_t kitId, uint32_t itemId, bool confirmByEnter = false);

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
