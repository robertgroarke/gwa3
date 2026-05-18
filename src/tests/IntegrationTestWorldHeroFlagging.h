bool TestHeroFlagging() {
    IntReport("=== Hero Flagging ===");

    if (ReadMyId() == 0 || ReadMapId() == 0) {
        IntSkip("Hero flagging", "Not in game");
        IntReport("");
        return false;
    }

    const AreaInfo* flagArea = MapMgr::GetAreaInfo(ReadMapId());
    if (flagArea && !IsSkillCastMapType(flagArea->type)) {
        IntSkip("Hero flagging", "Not in explorable (heroes not spawned in outpost)");
        IntReport("");
        return false;
    }

    float myX = 0.0f;
    float myY = 0.0f;
    if (!TryReadAgentPosition(ReadMyId(), myX, myY)) {
        IntSkip("Hero flagging", "Cannot read player position");
        IntReport("");
        return false;
    }

    const float flagX = myX + 300.0f;
    const float flagY = myY + 200.0f;
    IntReport("  Flagging hero 1 to (%.0f, %.0f)...", flagX, flagY);
    GameThread::EnqueuePost([flagX, flagY]() {
        PartyMgr::FlagHero(1, flagX, flagY);
    });
    Sleep(1000);
    IntCheck("FlagHero(1) sent (no crash)", true);

    const float flagAllX = myX - 300.0f;
    const float flagAllY = myY - 200.0f;
    IntReport("  Flagging all heroes to (%.0f, %.0f)...", flagAllX, flagAllY);
    GameThread::EnqueuePost([flagAllX, flagAllY]() {
        PartyMgr::FlagAll(flagAllX, flagAllY);
    });
    Sleep(1000);
    IntCheck("FlagAll sent (no crash)", true);

    IntReport("  Unflagging hero 1...");
    GameThread::EnqueuePost([]() {
        PartyMgr::UnflagHero(1);
    });
    Sleep(500);
    IntCheck("UnflagHero(1) sent (no crash)", true);

    IntReport("  Unflagging all...");
    GameThread::EnqueuePost([]() {
        PartyMgr::UnflagAll();
    });
    Sleep(500);
    IntCheck("UnflagAll sent (no crash)", true);

    IntReport("");
    return true;
}
