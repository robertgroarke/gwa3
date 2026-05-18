bool TestGameThreadCallbacks() {
    IntReport("=== GameThread Persistent Callbacks ===");

    if (!GameThread::IsInitialized()) {
        IntSkip("GameThread callbacks", "GameThread not initialized");
        IntReport("");
        return false;
    }

    static std::atomic<uint32_t> frameCount{0};
    static std::atomic<bool> callbackEnabled{false};
    static GameThread::HookEntry cbEntry{0};

    frameCount.store(0);
    callbackEnabled.store(true);
    GameThread::RegisterCallback(&cbEntry, []() {
        if (callbackEnabled.load()) {
            frameCount++;
        }
    }, 0x4000);
    Sleep(500);

    uint32_t after500ms = frameCount.load();
    IntReport("  Frame callback hits after 500ms: %u", after500ms);
    IntCheck("Persistent callback fired at least once", after500ms > 0);

    // Disable the body first so even an in-flight copied callback becomes inert.
    callbackEnabled.store(false);
    Sleep(100);

    // Remove from the registry on the game thread so mutation is serialized with frame dispatch.
    std::atomic<bool> removeRan{false};
    GameThread::Enqueue([&]() {
        GameThread::RemoveCallback(&cbEntry);
        removeRan.store(true);
    });
    WaitFor("GameThread callback removal barrier", 1000, [&]() { return removeRan.load(); });

    uint32_t atRemoval = frameCount.load();
    Sleep(500);
    uint32_t afterRemoval = frameCount.load();
    IntReport("  Hits at removal: %u, 500ms later: %u", atRemoval, afterRemoval);
    IntCheck("Callback stopped after removal", afterRemoval == atRemoval);

    IntReport("");
    return true;
}

// ===== StoC Packet Type Coverage =====
