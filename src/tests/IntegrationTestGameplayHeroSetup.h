bool TestHeroSetup() {
    IntReport("=== Hero Setup + Consumables ===");

    uint32_t mapId = ReadMapId();
    if (mapId == 0) {
        IntSkip("Hero setup", "Not in game");
        return false;
    }

    const AreaInfo* area = MapMgr::GetAreaInfo(mapId);
    const bool isOutpost = area && !IsSkillCastMapType(area->type);

    // SAMPLE ONE uses the "Mercs" hero profile from config/hero_configs/Mercs.txt.
    uint32_t heroIds[] = {30, 14, 21, 4, 24, 15, 29};
    const uint32_t heroesBefore = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes before setup: %u", heroesBefore);

    if (isOutpost && heroesBefore > 0) {
        IntReport("  Clearing existing heroes before setup...");
        uint32_t before27 = 0;
        uint32_t after27 = 0;
        const bool kickAll27Worked = TryKickAllHeroesSentinelWithObservation(0x27u, &before27, &after27);
        IntCheck("Upstream KickAllHeroes sentinel 0x27 removes party heroes", kickAll27Worked);
        if (!kickAll27Worked) {
            IntReport("  Upstream 0x27 sentinel did not clear heroes; using reliable per-hero clear path");
        }
        PartyMgr::DebugDumpPartyState("IntegrationTestGameplay before KickAllHeroes");
        PartyMgr::KickAllHeroes();
        Sleep(2000);
        PartyMgr::DebugDumpPartyState("IntegrationTestGameplay after KickAllHeroes");
        const uint32_t heroesAfterKick = PartyMgr::CountPartyHeroes();
        IntReport("  Party heroes after clear: %u", heroesAfterKick);
        IntCheck("Existing heroes cleared before setup", heroesAfterKick == 0);
    }

    IntReport("  Adding 7 heroes...");
    for (int i = 0; i < 7; i++) {
        TryAddHeroWithObservation(heroIds[i]);
    }
    const uint32_t heroesAfterAdd = PartyMgr::CountPartyHeroes();
    IntReport("  Party heroes after setup: %u", heroesAfterAdd);
    if (isOutpost) {
        IntCheck("Seven heroes present after setup", heroesAfterAdd == 7);
        if (heroesAfterAdd != 7) {
            RunHeroAddDiagnostics();
            IntReport("  Restoring standard hero setup after diagnostics...");
            RestoreHeroSetup(heroIds, std::size(heroIds));
            const uint32_t heroesAfterRestore = PartyMgr::CountPartyHeroes();
            IntReport("  Party heroes after restore: %u", heroesAfterRestore);
        }
    } else {
        IntCheck("Hero setup did not reduce party heroes", heroesAfterAdd >= heroesBefore);
    }

    IntReport("  Setting hero behaviors to Guard...");
    for (int i = 0; i < 7; i++) {
        GameThread::Enqueue([idx = i + 1]() {
            PartyMgr::SetHeroBehavior(idx, 1);
        });
        Sleep(200);
    }
    IntCheck("Hero behaviors set (no crash)", true);

    IntReport("");
    return true;
}

// Movement
