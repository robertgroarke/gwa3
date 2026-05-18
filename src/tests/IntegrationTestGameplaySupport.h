namespace {

bool TryAddHeroWithObservation(uint32_t heroId, uint32_t* beforeOut = nullptr, uint32_t* afterOut = nullptr) {
    const uint32_t beforeAdd = PartyMgr::CountPartyHeroes();
    PartyMgr::AddHero(heroId);
    Sleep(500);

    uint32_t afterAdd = PartyMgr::CountPartyHeroes();
    for (int retry = 0; retry < 4 && afterAdd == beforeAdd; ++retry) {
        Sleep(400);
        afterAdd = PartyMgr::CountPartyHeroes();
    }

    if (beforeOut) *beforeOut = beforeAdd;
    if (afterOut) *afterOut = afterAdd;
    IntReport("    Hero %u add: before=%u after=%u", heroId, beforeAdd, afterAdd);
    return afterAdd > beforeAdd;
}

bool TryKickAllHeroesSentinelWithObservation(uint32_t sentinel, uint32_t* beforeOut = nullptr, uint32_t* afterOut = nullptr) {
    const uint32_t before = PartyMgr::CountPartyHeroes();
    IntReport("    Trying HERO_KICK sentinel 0x%X: before=%u", sentinel, before);
    CtoS::SendPacket(2, Packets::HERO_KICK, sentinel);
    Sleep(500);

    uint32_t after = PartyMgr::CountPartyHeroes();
    for (int retry = 0; retry < 6 && after == before; ++retry) {
        Sleep(400);
        after = PartyMgr::CountPartyHeroes();
    }

    if (beforeOut) *beforeOut = before;
    if (afterOut) *afterOut = after;
    IntReport("    HERO_KICK sentinel 0x%X: after=%u", sentinel, after);
    return after < before;
}

void RunHeroAddDiagnostics() {
    IntReport("  Running hero-add diagnostics...");

    const uint32_t knownGoodHeroId = 14;
    const uint32_t suspectHeroId = 25;
    const uint32_t mercenaryHeroIds[] = {28, 29, 30, 31, 32, 33, 34, 35};

    PartyMgr::KickAllHeroes();
    Sleep(2000);
    IntReport("    Diagnostic baseline hero count: %u", PartyMgr::CountPartyHeroes());

    uint32_t before = 0;
    uint32_t after = 0;
    const bool knownGoodAdded = TryAddHeroWithObservation(knownGoodHeroId, &before, &after);
    IntReport("    Standard hero %u diagnostic: %s", knownGoodHeroId, knownGoodAdded ? "added" : "did not add");

    PartyMgr::KickAllHeroes();
    Sleep(1500);
    const bool suspectAdded = TryAddHeroWithObservation(suspectHeroId, &before, &after);
    IntReport("    Suspect hero %u diagnostic: %s", suspectHeroId, suspectAdded ? "added" : "did not add");

    PartyMgr::KickAllHeroes();
    Sleep(1500);
    bool mercAdded = false;
    for (uint32_t mercHeroId : mercenaryHeroIds) {
        if (TryAddHeroWithObservation(mercHeroId, &before, &after)) {
            IntReport("    Mercenary hero %u diagnostic: added", mercHeroId);
            mercAdded = true;
            break;
        }
        IntReport("    Mercenary hero %u diagnostic: did not add", mercHeroId);
        PartyMgr::KickAllHeroes();
        Sleep(1000);
    }
    if (!mercAdded) {
        IntReport("    No mercenary hero IDs 28-35 added during diagnostics");
    }
}

void RestoreHeroSetup(const uint32_t* heroIds, size_t heroCount) {
    PartyMgr::KickAllHeroes();
    Sleep(2000);
    for (size_t i = 0; i < heroCount; ++i) {
        TryAddHeroWithObservation(heroIds[i]);
    }
}

} // namespace
