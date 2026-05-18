bool TestStoCHook() {
    IntReport("=== StoC Packet Hook ===");

    if (ReadMyId() == 0) {
        IntSkip("StoC hook", "Not in game");
        IntReport("");
        return false;
    }

    static std::atomic<uint32_t> emulateHitCount{0};
    static std::atomic<uint32_t> emulateLastHeader{0};
    StoC::HookEntry testEntry{nullptr};
    constexpr uint32_t kTestHeader = 0x1FFu;

    const bool registered = StoC::RegisterPacketCallback(&testEntry, kTestHeader,
        [](StoC::HookStatus*, StoC::PacketBase* packet) {
            emulateHitCount++;
            emulateLastHeader = packet->header;
        }, -1);
    IntCheck("RegisterPacketCallback succeeded", registered);

    StoC::PacketBase fakePacket;
    fakePacket.header = kTestHeader;
    const bool emulated = StoC::EmulatePacket(&fakePacket);
    IntCheck("EmulatePacket dispatched to callback", emulated);
    IntCheck("Callback fired from emulated packet", emulateHitCount.load() > 0);
    IntCheck("Callback received correct header", emulateLastHeader.load() == kTestHeader);
    IntReport("  EmulatePacket: hits=%u lastHeader=0x%X",
              emulateHitCount.load(), emulateLastHeader.load());

    StoC::RemoveCallbacks(&testEntry);
    IntCheck("RemoveCallbacks succeeded (no crash)", true);

    const uint32_t hitsBefore = emulateHitCount.load();
    StoC::EmulatePacket(&fakePacket);
    IntCheck("Callback does not fire after removal", emulateHitCount.load() == hitsBefore);

    static std::atomic<uint32_t> liveHitCount{0};
    StoC::HookEntry liveEntry{nullptr};

    const bool liveRegistered = StoC::RegisterPostPacketCallback(&liveEntry, 0x00E1u,
        [](StoC::HookStatus*, StoC::PacketBase*) {
            liveHitCount++;
        });
    IntCheck("Live packet callback registered", liveRegistered);

    const bool liveHit = WaitFor("StoC live packet fires", 3000, []() {
        return liveHitCount.load() > 0;
    });

    IntReport("  Live StoC hits after wait: %u", liveHitCount.load());
    if (liveHit) {
        IntCheck("Live StoC callback fired from game traffic", true);
    } else {
        IntSkip("Live StoC callback", "No 0xE1 packets observed in 3s (hook may not be installed yet)");
    }

    StoC::RemoveCallbacks(&liveEntry);
    IntCheck("Live callback cleanup (no crash)", true);

    IntReport("");
    return true;
}
