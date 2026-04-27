// Froggy loot, merchant, and maintenance state handlers. Included by FroggyHM.cpp.

BotState HandleLoot(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Loot collection");
    // Loot is handled inline during waypoint traversal
    // This state handles post-boss cleanup
    WaitMs(2000);
    return BotState::Merchant;
}

BotState HandleMerchant(BotConfig& cfg) {
    LogBot("State: Merchant (return to outpost)");
    LogBot("Merchant lane: shared maintenance path");

    const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
    uint32_t mapId = MapMgr::GetMapId();

    // Return to the configured outpost if not already there.
    if (mapId != outpostMapId) {
        LogBot("Merchant: traveling to outpost map %u from map %u", outpostMapId, mapId);
    if (!DungeonRuntime::EnsureOutpostReady(outpostMapId, 60000u, "Merchant")) {
            LogBot("Failed to travel to outpost map %u", outpostMapId);
            return BotState::Stopping;
        }
    }
    if (!DungeonRuntime::WaitForTownRuntimeReady(outpostMapId, 10000u)) {
        LogBot("Merchant: outpost runtime not ready after travel; waiting for next tick");
        WaitMs(500);
        return BotState::Merchant;
    }

    MoveToAndWait(GADDS_MERCHANT.x, GADDS_MERCHANT.y, 350.0f);
    MaintenanceMgr::Config maintenanceCfg = MakeFroggyMaintenanceConfig(outpostMapId);
    if (OpenMerchantContextNearCoords(GADDS_MERCHANT.x, GADDS_MERCHANT.y, 2500.0f)) {
        MaintenanceMgr::PerformMaintenance(maintenanceCfg);
        WaitMs(500);
    } else {
        LogBot("Merchant maintenance: merchant window failed to open, skipping sell/buy");
        MaintenanceMgr::DepositGold(10000);
    }

    if (MaintenanceMgr::NeedsMaintenance(maintenanceCfg)) {
        return BotState::Maintenance;
    }

    return BotState::InTown;
}

BotState HandleMaintenance(BotConfig& cfg) {
    LogBot("State: Maintenance");

    const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
    // Ensure we're in an outpost
    uint32_t mapId = MapMgr::GetMapId();
    if (mapId != outpostMapId) {
        LogBot("Maintenance: traveling to outpost map %u from map %u", outpostMapId, mapId);
    if (!DungeonRuntime::EnsureOutpostReady(outpostMapId, 60000u, "Maintenance")) {
            LogBot("Maintenance: failed to reach outpost map %u; stopping bot", outpostMapId);
            return BotState::Stopping;
        }
    }
    if (!DungeonRuntime::WaitForTownRuntimeReady(outpostMapId, 10000u)) {
        LogBot("Maintenance: outpost runtime not ready after travel; waiting for next tick");
        WaitMs(500);
        return BotState::Maintenance;
    }

    // Use DP removal sweets if we had wipes
    UseDpRemovalIfNeeded();

    // Report inventory and buff status
    uint32_t freeSlots = DungeonInventory::CountFreeSlots();
    uint32_t idKits = DungeonInventory::CountItemByModel(ItemModelIds::IDENTIFICATION_KIT) +
                      DungeonInventory::CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT);
    uint32_t salvKits = DungeonInventory::CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT) +
                        DungeonInventory::CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT);
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t effectCount = DungeonEffects::GetPlayerEffectCount();
    bool hasBless = DungeonEffects::HasAnyDungeonBlessing();
    bool hasCon = DungeonEffects::HasFullConset();
    LogBot("Inventory: %u free slots, %u ID kits, %u salvage kits, %u gold",
           freeSlots, idKits, salvKits, charGold);
    LogBot("Buffs: %u effects, blessing=%s, conset=%s",
           effectCount, hasBless ? "yes" : "no", hasCon ? "yes" : "no");

    // Legacy Froggy maintenance helpers below predate the shared
    // MaintenanceMgr flow. They duplicate Xunlai/conset behavior and use a
    // separate Embark coordinate path, which causes erratic movement after
    // MaintenanceMgr has already returned the bot to Gadd's. Keep this state
    // on the single shared maintenance path instead.
    MaintenanceMgr::Config maintenanceCfg = MakeFroggyMaintenanceConfig(outpostMapId);
    MoveToAndWait(GADDS_MERCHANT.x, GADDS_MERCHANT.y, 350.0f);
    if (OpenMerchantContextNearCoords(GADDS_MERCHANT.x, GADDS_MERCHANT.y, 2500.0f)) {
        MaintenanceMgr::PerformMaintenance(maintenanceCfg);
        WaitMs(500);
    } else {
        LogBot("Maintenance: merchant window failed to open, skipping shared maintenance");
    }

    // If still critically low on slots after selling + depositing, log warning
    freeSlots = DungeonInventory::CountFreeSlots();
    const bool stillNeedsMaintenance = MaintenanceMgr::NeedsMaintenance(maintenanceCfg);
    if (freeSlots < 3) {
        LogBot("WARNING: Critically low inventory space (%u slots). Consider manual cleanup.", freeSlots);
    }
    if (freeSlots < maintenanceCfg.minFreeSlots || stillNeedsMaintenance) {
        LogBot("Maintenance: inventory not cleared enough to continue (freeSlots=%u min=%u needsMaintenance=%s); stopping bot",
               freeSlots,
               maintenanceCfg.minFreeSlots,
               stillNeedsMaintenance ? "yes" : "no");
        return BotState::Stopping;
    }

    return BotState::Traveling;
}
