static bool KickAllHeroesWithObservation(DWORD timeoutMs) {
    PartyMgr::DebugDumpPartyState("Froggy before KickAllHeroes");
    PartyMgr::KickAllHeroes();
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (PartyMgr::CountPartyHeroes() == 0) {
            PartyMgr::DebugDumpPartyState("Froggy after KickAllHeroes success");
            return true;
        }
        if ((GetTickCount() - start) > 2000 && (GetTickCount() - start) < 2250) {
            FroggyFeatureReport("  KickAllHeroes still pending after 2s, reissuing reliable per-hero clear...");
            PartyMgr::DebugDumpPartyState("Froggy before KickAllHeroes reissue");
            PartyMgr::KickAllHeroes();
        }
        Sleep(250);
    }
    PartyMgr::DebugDumpPartyState("Froggy after KickAllHeroes timeout");
    return PartyMgr::CountPartyHeroes() == 0;
}
