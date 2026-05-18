bool TestMemAllocFree() {
    IntReport("=== MemAlloc/MemFree ===");

    void* ptr = MemoryMgr::MemAlloc(4096);
    IntReport("  MemAlloc(4096) = %p", ptr);
    IntCheck("MemAlloc returned non-null", ptr != nullptr);

    if (ptr) {
        // Write pattern
        memset(ptr, 0xAA, 4096);
        uint8_t* bytes = static_cast<uint8_t*>(ptr);
        IntCheck("Written pattern reads back correctly", bytes[0] == 0xAA && bytes[4095] == 0xAA);

        MemoryMgr::MemFree(ptr);
        IntSkip("MemFree semantics", "No reliable post-free validity probe in current harness");
    }

    IntReport("");
    return true;
}

// ===== Skillbar Management =====
