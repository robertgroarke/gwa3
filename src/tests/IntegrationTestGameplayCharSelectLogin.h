bool TestCharSelectLogin() {
    IntReport("=== Character Select + Login ===");

    const uint32_t mapId = ReadMapId();
    const uint32_t myId = ReadMyId();
    IntReport("  Post-bootstrap state: MapID=%u, MyID=%u", mapId, myId);

    IntCheck("Bootstrap reached map", mapId > 0);
    IntCheck("Bootstrap produced MyID", myId > 0);
    IntCheck("GameThread initialized post-login", GameThread::IsInitialized());

    if (!GameThread::IsInitialized()) {
        IntReport("  Cannot continue without GameThread after login");
        return false;
    }

    std::atomic<bool> gtFlag{false};
    GameThread::Enqueue([&gtFlag]() { gtFlag = true; });
    Sleep(1000);
    IntCheck("GameThread enqueue fires", gtFlag.load());

    if (mapId == 0 || myId == 0) {
        IntReport("");
        return false;
    }

    IntCheck("MapID is valid", mapId <= 2000);
    IntCheck("MyID is valid", myId <= 10000);

    const uint32_t ping = ChatMgr::GetPing();
    IntReport("  Ping: %u ms", ping);
    IntCheck("Ping plausible", ping > 0 && ping <= 5000);

    IntReport("");
    return true;
}
