bool TestPartyManagement() {
    IntReport("=== Party Management ===");

    if (ReadMyId() == 0) { IntSkip("PartyMgmt", "Not in game"); IntReport(""); return false; }

    const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
    if (!area || IsSkillCastMapType(area->type)) {
        IntSkip("PartyMgmt", "Not in outpost");
        IntReport("");
        return false;
    }

    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes before kick: %u", heroesBefore);
    if (heroesBefore == 0) {
        IntSkip("Party hero assertions", "No heroes present in player party");
        IntReport("");
        return true;
    }

    // KickAllHeroes then re-add
    IntReport("  Kicking all heroes...");
    PartyMgr::KickAllHeroes();
    Sleep(2000);
    const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after kick: %u", heroesAfterKick);
    IntCheck("KickAllHeroes removed party heroes", heroesAfterKick == 0);

    // Re-add standard heroes
    // SAMPLE ONE uses the "Mercs" hero profile from config/hero_configs/Mercs.txt.
    uint32_t heroIds[] = {30, 14, 21, 4, 24, 15, 29};
    IntReport("  Re-adding 7 heroes...");
    for (int i = 0; i < 7; i++) {
        const uint32_t beforeAddCount = PartyMgr::CountPartyHeroes();
        PartyMgr::AddHero(heroIds[i]);
        Sleep(500);
        uint32_t afterAddCount = PartyMgr::CountPartyHeroes();
        for (int retry = 0; retry < 4 && afterAddCount == beforeAddCount; ++retry) {
            Sleep(400);
            afterAddCount = PartyMgr::CountPartyHeroes();
        }
        IntReport("    Hero %u add: before=%u after=%u", heroIds[i], beforeAddCount, afterAddCount);
    }
    const uint32_t heroesAfterAdd = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after re-add: %u", heroesAfterAdd);
    IntCheck("Heroes re-added to party", heroesAfterAdd >= heroesBefore);

    IntSkip("Tick(true/false)", "No readable ready-state flag exposed yet");
    IntSkip("LockHeroTarget", "No readable hero target-lock state exposed yet");

    IntReport("");
    return true;
}

// ===== Title Management =====
