bool TestUIFrameInteraction() {
    IntReport("=== UI Frame Interaction ===");

    if (ReadMyId() == 0) { IntSkip("UIFrameInteract", "Not in game"); IntReport(""); return false; }

    // Debug: check FrameArray offset and state
    IntReport("  Offsets::FrameArray = 0x%08X", static_cast<unsigned>(Offsets::FrameArray));
    if (Offsets::FrameArray > 0x10000) {
        __try {
            uintptr_t* buffer = *reinterpret_cast<uintptr_t**>(Offsets::FrameArray);
            uint32_t size = *reinterpret_cast<uint32_t*>(Offsets::FrameArray + sizeof(uintptr_t));
            IntReport("  FrameArray: buffer=0x%08X size=%u",
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(buffer)), size);
            if (buffer && size > 0) {
                // Count non-null entries and sample first few
                uint32_t nonNull = 0;
                uint32_t limit = (size < 1024) ? size : 1024;
                for (uint32_t i = 0; i < limit; ++i) {
                    if (buffer[i] > 0x10000) nonNull++;
                }
                IntReport("  FrameArray non-null entries: %u / %u", nonNull, limit);

                // Sample first 5 non-null entries with hash at multiple offsets
                uint32_t sampled = 0;
                for (uint32_t i = 0; i < limit && sampled < 8; ++i) {
                    if (buffer[i] > 0x10000) {
                        uint32_t hash134 = *reinterpret_cast<uint32_t*>(buffer[i] + 0x134);
                        uint32_t hash128 = *reinterpret_cast<uint32_t*>(buffer[i] + 0x128);
                        uint32_t hash12C = *reinterpret_cast<uint32_t*>(buffer[i] + 0x12C);
                        uint32_t hash130 = *reinterpret_cast<uint32_t*>(buffer[i] + 0x130);
                        uint32_t hash138 = *reinterpret_cast<uint32_t*>(buffer[i] + 0x138);
                        uint32_t state18C = *reinterpret_cast<uint32_t*>(buffer[i] + 0x18C);
                        uint32_t state188 = *reinterpret_cast<uint32_t*>(buffer[i] + 0x188);
                        uint32_t state190 = *reinterpret_cast<uint32_t*>(buffer[i] + 0x190);
                        IntReport("    [%u] frame=0x%08X +128=%08X +12C=%08X +130=%08X +134=%08X +138=%08X state+188=%X +18C=%X +190=%X",
                                  i, static_cast<unsigned>(buffer[i]),
                                  hash128, hash12C, hash130, hash134, hash138,
                                  state188, state18C, state190);
                        sampled++;
                    }
                }

                // Search for known in-game hashes
                constexpr uint32_t kInventory = 0xAB580F41u;   // Inventory Window
                constexpr uint32_t kParty = 0xC69AAB72u;       // Party Formation
                for (uint32_t i = 1; i < limit; ++i) {
                    if (buffer[i] <= 0x10000) continue;
                    uint32_t h = *reinterpret_cast<uint32_t*>(buffer[i] + 0x134);
                    if (h == kInventory) {
                        IntReport("  *** Found Inventory Window at frame[%u] ***", i);
                    } else if (h == kParty) {
                        IntReport("  *** Found Party Formation at frame[%u] ***", i);
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            IntReport("  FrameArray read faulted");
        }
    }

    // Try known in-game hashes
    uintptr_t inventoryFrame = UIMgr::GetFrameByHash(0xAB580F41u); // Inventory Window
    uintptr_t partyFrame = UIMgr::GetFrameByHash(0xC69AAB72u);     // Party Formation
    IntReport("  Inventory frame: 0x%08X", static_cast<unsigned>(inventoryFrame));
    IntReport("  Party frame: 0x%08X", static_cast<unsigned>(partyFrame));

    // Use any available frame for testing — root (buffer[0]) is often null
    uintptr_t testFrame = inventoryFrame ? inventoryFrame : partyFrame;
    if (!testFrame) {
        // Fallback: find any non-null frame with a hash
        if (Offsets::FrameArray > 0x10000) {
            __try {
                uintptr_t* buf = *reinterpret_cast<uintptr_t**>(Offsets::FrameArray);
                uint32_t sz = *reinterpret_cast<uint32_t*>(Offsets::FrameArray + sizeof(uintptr_t));
                for (uint32_t i = 1; i < sz && i < 1024; ++i) {
                    if (buf[i] > 0x10000) {
                        uint32_t h = *reinterpret_cast<uint32_t*>(buf[i] + 0x134);
                        if (h != 0) { testFrame = buf[i]; break; }
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    IntReport("  Test frame: 0x%08X", static_cast<unsigned>(testFrame));
    if (!testFrame) { IntSkip("UIFrameInteract", "No usable frame found"); IntReport(""); return false; }

    // GetChildOffsetId
    uint32_t childOffset = UIMgr::GetChildOffsetId(testFrame);
    IntReport("  GetChildOffsetId: %u", childOffset);
    IntCheck("GetChildOffsetId returned plausible value", childOffset < 4096);

    // GetFrameContext
    uintptr_t ctx = UIMgr::GetFrameContext(testFrame);
    IntReport("  GetFrameContext: 0x%08X", static_cast<unsigned>(ctx));
    IntCheck("GetFrameContext returned non-null", ctx > 0x10000);

    // Frame state
    uint32_t frameState = UIMgr::GetFrameState(testFrame);
    uint32_t frameHash = UIMgr::GetFrameHash(testFrame);
    IntReport("  Frame state=0x%X hash=0x%08X", frameState, frameHash);
    IntCheck("Frame state has created bit", (frameState & UIMgr::FRAME_CREATED) != 0);

    IntSkip("SendUIMessage(0)", "No observable UI-side effect defined for this message");

    IntReport("");
    return true;
}

// ===== Agent Interaction =====
