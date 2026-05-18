bool TestPingStability() {
    IntReport("=== Ping Stability ===");

    if (ReadMyId() == 0) {
        IntSkip("Ping stability", "Not in game");
        IntReport("");
        return false;
    }

    uint32_t samples[5] = {};
    for (int i = 0; i < 5; ++i) {
        samples[i] = ChatMgr::GetPing();
        if (i < 4) Sleep(250);
    }

    IntReport("  Ping samples: %u %u %u %u %u",
              samples[0], samples[1], samples[2], samples[3], samples[4]);

    uint32_t minPing = samples[0];
    uint32_t maxPing = samples[0];
    for (int i = 1; i < 5; ++i) {
        if (samples[i] < minPing) minPing = samples[i];
        if (samples[i] > maxPing) maxPing = samples[i];
    }

    IntReport("  Ping range: %u - %u ms (spread=%u)", minPing, maxPing, maxPing - minPing);
    IntCheck("All pings > 0", minPing > 0);
    IntCheck("All pings < 5000ms", maxPing < 5000);
    IntCheck("Ping spread < 2000ms (not wildly unstable)", (maxPing - minPing) < 2000);

    IntReport("");
    return true;
}
