// Froggy merchant and consumable maintenance helpers. Included by FroggyHM.cpp
// while shared inventory and vendor behavior is progressively consolidated.

static bool OpenMerchantContextNearCoords(float searchX, float searchY, float searchRadius) {
    DungeonVendor::MerchantContextNearCoordsOptions options;
    options.preferred_player_number = GADDS_MERCHANT_PLAYER_NUMBER;
    options.log_prefix = "Froggy";
    return DungeonVendor::OpenMerchantContextNearCoords(
        searchX,
        searchY,
        searchRadius,
        &MoveToAndWait,
        &DungeonRuntime::WaitMs,
        options);
}

static MaintenanceMgr::Config MakeFroggyMaintenanceConfig(uint32_t outpostMapId) {
    MaintenanceMgr::Config maintenanceCfg = {};
    maintenanceCfg.maintenanceTown = outpostMapId;
    if (outpostMapId == MapIds::GADDS_ENCAMPMENT) {
        maintenanceCfg.xunlaiChestX = GADDS_XUNLAI.x;
        maintenanceCfg.xunlaiChestY = GADDS_XUNLAI.y;
        maintenanceCfg.materialTraderX = GADDS_MATERIAL_TRADER.x;
        maintenanceCfg.materialTraderY = GADDS_MATERIAL_TRADER.y;
        maintenanceCfg.materialTraderPlayerNumber = GADDS_MATERIAL_TRADER_PLAYER_NUMBER;
    }
    return maintenanceCfg;
}

// ===== Consumable Usage =====

static void UseConsumables(const BotConfig& cfg) {
    if (!cfg.use_consets) return;
    if (Offsets::MyID <= 0x10000u) return;
    const uint32_t myId = *reinterpret_cast<uint32_t*>(Offsets::MyID);

    const auto result = DungeonItemActions::UseConsetsForAgentIfEnabled(
        true,
        myId,
        &DungeonRuntime::WaitMs);
    if (!result.attempted) return;

    if (result.consets.used_armor) {
        LogBot("Using Armor of Salvation");
    }
    if (result.consets.used_essence) {
        LogBot("Using Essence of Celerity");
    }
    if (result.consets.used_grail) {
        LogBot("Using Grail of Might");
    }

    if (result.consets.full_active) {
        LogBot("All consets active");
    } else {
        LogBot("Some consets missing - may need to craft/buy");
    }
}

static void UseDpRemovalIfNeeded() {
    DungeonItemActions::UseItemOptions options;
    options.delay_ms = 5000u;
    const auto result = DungeonItemActions::UseDpRemovalSweetIfNeeded(&s_wipeCount, &DungeonRuntime::WaitMs, options);
    if (result.used_model_id != 0u) {
        LogBot("Using DP removal sweet (model=%u) after %u wipes",
               result.used_model_id,
               result.previous_wipe_count);
    }
}
