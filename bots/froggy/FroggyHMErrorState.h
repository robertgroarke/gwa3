// Froggy error state handler. Included by FroggyHM.cpp.

BotState HandleError(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: ERROR - waiting 10s before retry");
    WaitMs(10000);

    uint32_t mapId = MapMgr::GetMapId();
    if (mapId == 0) {
        return BotState::CharSelect;
    }
    if (mapId == MapIds::SPARKFLY_SWAMP ||
        mapId == MapIds::BOGROOT_GROWTHS_LVL1 ||
        mapId == MapIds::BOGROOT_GROWTHS_LVL2) {
        LogBot("Error recovery staying in explorable map %u", mapId);
        return BotState::InDungeon;
    }
    return BotState::InTown;
}
