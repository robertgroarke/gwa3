// Froggy character-select, town setup, and travel state handlers. Included by FroggyHM.cpp.

// ===== State Handlers =====

BotState HandleCharSelect(BotConfig& cfg) {
    return DungeonStates::HandleCharSelect(cfg);
}

BotState HandleTownSetup(BotConfig& cfg) {
    const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
    DungeonStates::TownSetupOptions options = {};
    options.default_outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    options.run_number = s_runCount + 1;
    options.log_prefix = "Froggy";
    options.maintenance = MakeFroggyMaintenanceConfig(outpostMapId);
    options.merchant_x = GADDS_MERCHANT.x;
    options.merchant_y = GADDS_MERCHANT.y;
    options.outpost.default_hero_config_file = "Standard.txt";
    options.move_to_point = [](float x, float y, float threshold) {
        return MoveToAndWait(x, y, threshold);
    };
    options.open_merchant = [](float x, float y, float searchRadius) {
        DungeonVendor::MerchantContextNearCoordsOptions vendorOptions;
        vendorOptions.preferred_player_number = GADDS_MERCHANT_PLAYER_NUMBER;
        vendorOptions.log_prefix = "Froggy";
        return DungeonVendor::OpenMerchantContextNearCoords(
            x,
            y,
            searchRadius,
            &MoveToAndWait,
            &DungeonRuntime::WaitMs,
            vendorOptions);
    };
    options.refresh_skill_cache = []() {
        (void)DungeonCombatRoutine::RefreshSkillCacheWithDebugLog(s_combatSession, "Froggy");
    };
    options.use_consumables = [](const BotConfig& botCfg) {
        UseConsumables(botCfg);
    };
    return DungeonStates::HandleTownSetup(cfg, options);
}

BotState HandleTravel(BotConfig& cfg) {
    const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;

    DungeonQuestRuntime::TravelToEntryMapOptions options = {};
    options.source_map_id = outpostMapId;
    options.entry_map_id = MapIds::SPARKFLY_SWAMP;
    options.travel_path = GADDS_TO_SPARKFLY_PATH;
    options.travel_path_count = GADDS_TO_SPARKFLY_PATH_COUNT;
    options.zone_point = GADDS_TO_SPARKFLY_ZONE;
    options.zone_timeout_ms = 10000u;
    options.log_prefix = "Froggy";
    options.label = "Travel to Sparkfly Swamp";

    const auto result = DungeonQuestRuntime::TravelToEntryMap(options);
    if (result == DungeonQuestRuntime::TravelToEntryMapStatus::AtEntryMap) {
        return BotState::InDungeon;
    }
    return BotState::InTown;
}
