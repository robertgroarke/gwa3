#pragma once

// GWA3 MaintenanceMgr — inventory maintenance, gold management, kit restocking.
// Mirrors AutoIt Utils-Maintenance.au3 PerformMaintenance() flow.
// Called between dungeon runs to keep inventory clear and kits stocked.

#include <cstdint>

namespace GWA3::MaintenanceMgr {

    // ===== Configuration =====
    struct Config {
        uint32_t minFreeSlots       = 7;
        uint32_t minIdKits          = 1;
        uint32_t minSalvageKits     = 1;
        uint32_t maxCharacterGold   = 95000;
        uint32_t targetIdKits       = 3;
        uint32_t targetSalvageKits  = 8;
        uint32_t maintenanceTown    = 638; // Gadd's Encampment
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

    // ===== Gold Management =====

    // Deposit gold to storage. Keeps `keepOnChar` gold on character.
    void DepositGold(uint32_t keepOnChar = 10000);

    // Withdraw gold from storage up to `amount`.
    void WithdrawGold(uint32_t amount);

    // ===== Storage Deposit =====

    // Deposit basic materials from backpack (bags 1-4) to material storage (bag 6).
    // Requires Xunlai chest to be open. Returns number of stacks deposited.
    uint32_t DepositMaterialsToStorage();

    // Open the Xunlai chest NPC at given coordinates (move to, interact).
    void OpenXunlaiChest(float chestX, float chestY);

    // ===== Sell Items =====

    // Sell all junk items to the currently-open merchant.
    // Returns number of items sold.
    // Junk = identified whites/blues/purples, excluding kits and rare skins.
    uint32_t SellJunkItems();

    // Check if item should be sold as junk (filter function).
    bool ShouldSellItem(uint32_t modelId, uint16_t value, uint8_t type);

    // ===== Kit Management =====

    // Buy ID and salvage kits from the currently-open merchant to reach targets.
    void BuyKitsToTarget(const Config& cfg = {});

    // ===== Item Identification =====

    // Identify all unidentified items in bags 1-4.
    // Skips rare skins (never identify those — preserves value).
    // Requires an ID kit in inventory. Returns number of items identified.
    uint32_t IdentifyAllItems();

    // ===== Salvage =====

    // Salvage all non-rare, identified white/blue items in bags 1-4.
    // Uses SalvageSessionOpen + SalvageMaterials flow.
    // Requires a salvage kit in inventory. Returns number of items salvaged.
    uint32_t SalvageJunkItems();

    // ===== Full Maintenance =====

    // Run the full maintenance sequence:
    // 1. Deposit excess gold
    // 2. Sell junk items to merchant
    // 3. Buy kits to target
    // Assumes player is in maintenance town with merchant accessible.
    void PerformMaintenance(const Config& cfg = {});

} // namespace GWA3::MaintenanceMgr
