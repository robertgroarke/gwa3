bool TestCallbackRegistry() {
    IntReport("=== CallbackRegistry ===");

    if (ReadMyId() == 0) { IntSkip("CallbackRegistry", "Not in game"); IntReport(""); return false; }

    // Test UIMessage callback via EmulatePacket-style approach
    // Register a callback for a harmless UI message and dispatch it
    static std::atomic<uint32_t> cbFireCount{0};
    GWA3::HookEntry testEntry{nullptr};

    constexpr uint32_t kTestMsgId = 0x9999u; // unlikely to collide with real traffic

    bool registered = CallbackRegistry::RegisterUIMessageCallback(
        &testEntry, kTestMsgId,
        [](GWA3::HookStatus*, uint32_t, void*, void*) { cbFireCount++; }, -1);

    IntReport("  RegisterUIMessageCallback: %s", registered ? "ok" : "failed");
    IntCheck("UIMessage callback registered", registered);

    if (registered) {
        // Dispatch manually
        CallbackRegistry::DispatchUIMessage(kTestMsgId, nullptr, nullptr);
        IntReport("  After dispatch: fires=%u", cbFireCount.load());
        IntCheck("UIMessage callback fired", cbFireCount.load() > 0);

        // Remove and verify doesn't fire
        CallbackRegistry::RemoveCallbacks(&testEntry);
        uint32_t countBefore = cbFireCount.load();
        CallbackRegistry::DispatchUIMessage(kTestMsgId, nullptr, nullptr);
        IntCheck("Callback doesn't fire after removal", cbFireCount.load() == countBefore);
    }

    IntReport("");
    return true;
}

// ===== GameThread Persistent Callbacks =====
