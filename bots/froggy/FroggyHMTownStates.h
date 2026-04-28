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
        return OpenMerchantContextNearCoords(x, y, searchRadius);
    };
    options.refresh_skill_cache = []() {
        (void)RefreshFroggySkillCache();
    };
    options.use_consumables = [](const BotConfig& botCfg) {
        UseConsumables(botCfg);
    };
    return DungeonStates::HandleTownSetup(cfg, options);
}

BotState HandleTravel(BotConfig& cfg) {
    LogBot("State: Travel to Sparkfly Swamp");

    const uint32_t outpostMapId = cfg.outpost_map_id ? cfg.outpost_map_id : MapIds::GADDS_ENCAMPMENT;
    uint32_t mapId = MapMgr::GetMapId();

    if (mapId == outpostMapId) {
        (void)DungeonQuestRuntime::FollowTravelPath(
            GADDS_TO_SPARKFLY_PATH,
            GADDS_TO_SPARKFLY_PATH_COUNT,
            outpostMapId);
        (void)DungeonQuestRuntime::ZoneThroughPoint(
            GADDS_TO_SPARKFLY_ZONE.x,
            GADDS_TO_SPARKFLY_ZONE.y,
            MapIds::SPARKFLY_SWAMP,
            10000u);
    }

    mapId = MapMgr::GetMapId();
    if (mapId == MapIds::SPARKFLY_SWAMP) {
        return BotState::InDungeon;
    }

    // Fallback
    return BotState::InTown;
}
