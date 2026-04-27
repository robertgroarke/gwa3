// Froggy character-select, town setup, and travel state handlers. Included by FroggyHM.cpp.

// ===== State Handlers =====

BotState HandleCharSelect(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: CharSelect");

    // Click Play button
    if (UIMgr::IsFrameVisible(UIMgr::Hashes::PlayButton)) {
        UIMgr::ButtonClickByHash(UIMgr::Hashes::PlayButton);
        WaitMs(5000);
    }

    // Handle reconnect dialog
    if (UIMgr::IsFrameVisible(UIMgr::Hashes::ReconnectYes)) {
        UIMgr::ButtonClickByHash(UIMgr::Hashes::ReconnectYes);
        WaitMs(3000);
    }

    // Wait for map load
    WaitMs(5000);
    uint32_t mapId = MapMgr::GetMapId();
    if (mapId > 0) {
        return BotState::InTown;
    }

    return BotState::CharSelect;
}

BotState HandleTownSetup(BotConfig& cfg) {
    LogBot("State: TownSetup (run #%u)", s_runCount + 1);

    const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
    uint32_t mapId = MapMgr::GetMapId();

    // If not at the configured outpost, travel there first.
    if (mapId != outpostMapId) {
        LogBot("TownSetup: traveling to outpost map %u from map %u", outpostMapId, mapId);
    if (!DungeonRuntime::EnsureOutpostReady(outpostMapId, 60000u, "TownSetup")) {
            LogBot("TownSetup: outpost travel failed; stopping bot to avoid queue saturation");
            return BotState::Stopping;
        }
        return BotState::InTown;
    }
    if (!DungeonRuntime::WaitForTownRuntimeReady(outpostMapId, 10000u)) {
        LogBot("TownSetup: outpost runtime not ready after map load; waiting for next tick");
        WaitMs(500);
        return BotState::InTown;
    }

    // Run maintenance if needed (sell junk, deposit gold, buy kits)
    MaintenanceMgr::Config maintenanceCfg = MakeFroggyMaintenanceConfig(outpostMapId);
    if (MaintenanceMgr::NeedsMaintenance(maintenanceCfg)) {
        LogBot("Maintenance needed - running before dungeon entry");

        // Move to merchant and open window
        MoveToAndWait(GADDS_MERCHANT.x, GADDS_MERCHANT.y, 350.0f);
        if (OpenMerchantContextNearCoords(GADDS_MERCHANT.x, GADDS_MERCHANT.y, 2500.0f)) {
                MaintenanceMgr::PerformMaintenance(maintenanceCfg);
                // PerformMaintenance may zone to Embark and back for conset
                // conversion. Issuing ACTION_CANCEL immediately after that
                // return has been crashing the current client.
                WaitMs(500);
        } else {
            LogBot("Maintenance: merchant window failed to open, skipping sell/buy");
            // Still deposit gold even without merchant
            MaintenanceMgr::DepositGold(10000);
        }

        const uint32_t postMaintenanceFreeSlots = MaintenanceMgr::CountFreeSlots();
        const bool stillNeedsMaintenance = MaintenanceMgr::NeedsMaintenance(maintenanceCfg);
        if (postMaintenanceFreeSlots < maintenanceCfg.minFreeSlots || stillNeedsMaintenance) {
            LogBot("TownSetup: maintenance did not clear inventory enough (freeSlots=%u min=%u needsMaintenance=%s); stopping before explorable entry",
                   postMaintenanceFreeSlots,
                   maintenanceCfg.minFreeSlots,
                   stillNeedsMaintenance ? "yes" : "no");
            return BotState::Stopping;
        }
    }

    DungeonOutpostSetup::Options outpostSetupOptions = {};
    outpostSetupOptions.default_hero_config_file = "Standard.txt";
    if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg, outpostSetupOptions)) {
        LogBot("Froggy: outpost setup failed");
        return BotState::Error;
    }

    // Cache player skillbar for combat
    RefreshFroggySkillCache();

    // Use consumables before entering dungeon
    UseConsumables(cfg);

    return BotState::Traveling;
}

BotState HandleTravel(BotConfig& cfg) {
    LogBot("State: Travel to Sparkfly Swamp");

    const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
    uint32_t mapId = MapMgr::GetMapId();

    if (mapId == outpostMapId) {
        // Move to exit portal
        MoveToAndWait(-10018, -21892);
        MoveToAndWait(-9550, -20400);
        AgentMgr::Move(-9451, -19766);
        // Wait for Sparkfly Swamp load
        WaitMs(10000);
    }

    mapId = MapMgr::GetMapId();
    if (mapId == MapIds::SPARKFLY_SWAMP) {
        return BotState::InDungeon;
    }

    // Fallback
    return BotState::InTown;
}
