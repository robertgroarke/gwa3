// GameThread assertion investigation section. Included by SmokeTest.cpp inside GWA3::SmokeTest.

static void RunGameThreadAssertionSmokeSection() {
    Report("--- GameThread Assertion Investigation ---");
    Report("Searching .rdata for FrApi.cpp and renderElapsed strings...");
    {
        auto rdata = Scanner::GetRdataSection();
        auto text = Scanner::GetTextSection();
        const char* rdataPtr = reinterpret_cast<const char*>(rdata.start);
        size_t rdataSize = rdata.size;

        // Search for any string containing "FrApi" in .rdata
        int frApiCount = 0;
        for (size_t i = 0; i + 8 < rdataSize; i++) {
            if (memcmp(rdataPtr + i, "FrApi", 5) == 0) {
                const char* str = rdataPtr + i;
                size_t maxLen = rdataSize - i;
                size_t len = 0;
                while (len < maxLen && len < 200 && str[len] != '\0') len++;
                Report("  Found 'FrApi' at .rdata+0x%X (addr 0x%08X): \"%.*s\"",
                       (unsigned)i, (unsigned)(rdata.start + i), (int)len, str);
                frApiCount++;
                i += len; // skip past this string
            }
        }
        Report("  Total 'FrApi' strings found: %d", frApiCount);

        // Search for "renderElapsed" in .rdata
        int renderCount = 0;
        for (size_t i = 0; i + 15 < rdataSize; i++) {
            if (memcmp(rdataPtr + i, "renderElapsed", 13) == 0) {
                const char* str = rdataPtr + i;
                size_t maxLen = rdataSize - i;
                size_t len = 0;
                while (len < maxLen && len < 200 && str[len] != '\0') len++;
                Report("  Found 'renderElapsed' at .rdata+0x%X (addr 0x%08X): \"%.*s\"",
                       (unsigned)i, (unsigned)(rdata.start + i), (int)len, str);
                renderCount++;
                i += len;
            }
        }
        Report("  Total 'renderElapsed' strings found: %d", renderCount);

        if (frApiCount == 0) {
            Report("  WARNING: No 'FrApi' strings in .rdata!");
            // Try searching for just "Api.cpp" or "Frame"
            int apiCppCount = 0;
            for (size_t i = 0; i + 10 < rdataSize; i++) {
                if (memcmp(rdataPtr + i, "Api.cpp", 7) == 0) {
                    const char* str = rdataPtr + i;
                    // Walk back to find full path
                    const char* start = str;
                    while (start > rdataPtr && *(start-1) != '\0') start--;
                    size_t len = 0;
                    while (len < 200 && str[len] != '\0') len++;
                    size_t fullLen = (str + len) - start;
                    Report("    'Api.cpp' at 0x%08X: \"%.*s\"",
                           (unsigned)(rdata.start + (start - rdataPtr)), (int)fullLen, start);
                    apiCppCount++;
                    if (apiCppCount >= 20) break;
                    i = (str - rdataPtr) + len;
                }
            }
        }

        if (frApiCount > 0 && renderCount > 0) {
            Report("  Attempting FindAssertion with exact strings...");
            uintptr_t result = Scanner::FindAssertion("FrApi.cpp", "renderElapsed >= 0", 0);
            Report("  FindAssertion result: 0x%08X", result);
        }

        Report("  Searching .text for any reference to FrApi.cpp string addr...");
        {
            // Find the full path string "P:\Code\Engine\Frame\FrApi.cpp"
            uintptr_t frApiFullPath = 0;
            for (size_t i = 0; i + 30 < rdataSize; i++) {
                if (memcmp(rdataPtr + i, "P:\\Code\\Engine\\Frame\\FrApi.cpp", 29) == 0) {
                    frApiFullPath = rdata.start + i;
                    break;
                }
            }
            uintptr_t renderStr = 0;
            for (size_t i = 0; i + 20 < rdataSize; i++) {
                if (memcmp(rdataPtr + i, "renderElapsed >= 0", 18) == 0) {
                    renderStr = rdata.start + i;
                    break;
                }
            }
            Report("  FrApi full path addr: 0x%08X", frApiFullPath);
            Report("  renderElapsed addr: 0x%08X", renderStr);

            const uint8_t* textPtr = reinterpret_cast<const uint8_t*>(text.start);
            size_t textSize = text.size;
            int xrefCount = 0;

            if (frApiFullPath) {
                uint32_t target = static_cast<uint32_t>(frApiFullPath);
                for (size_t i = 0; i + 4 < textSize && xrefCount < 10; i++) {
                    uint32_t val;
                    memcpy(&val, textPtr + i, 4);
                    if (val == target) {
                        uintptr_t addr = text.start + i;
                        uint8_t opcode = textPtr[i - 1]; // byte before the address
                        Report("  XREF to FrApi at 0x%08X, preceding opcode: 0x%02X", addr, opcode);
                        DumpBytes("  xref context", addr, 8, 12);
                        xrefCount++;
                    }
                }
            }
            Report("  Total FrApi xrefs in .text: %d", xrefCount);

            xrefCount = 0;
            if (renderStr) {
                uint32_t target = static_cast<uint32_t>(renderStr);
                for (size_t i = 0; i + 4 < textSize && xrefCount < 10; i++) {
                    uint32_t val;
                    memcpy(&val, textPtr + i, 4);
                    if (val == target) {
                        uintptr_t addr = text.start + i;
                        uint8_t opcode = textPtr[i - 1];
                        Report("  XREF to renderElapsed at 0x%08X, preceding opcode: 0x%02X", addr, opcode);
                        DumpBytes("  xref context", addr, 8, 12);
                        xrefCount++;
                    }
                }
            }
            Report("  Total renderElapsed xrefs in .text: %d", xrefCount);
        }

        Report("  Enumerating ALL '83 C1 DC E8' (ADD ECX,-24 / CALL) sites in .text...");
        {
            const uint8_t* tp = reinterpret_cast<const uint8_t*>(text.start);
            int matchCount = 0;
            for (size_t i = 0; i + 8 < text.size && matchCount < 20; i++) {
                if (tp[i] == 0x83 && tp[i+1] == 0xC1 && tp[i+2] == 0xDC && tp[i+3] == 0xE8) {
                    uintptr_t callAddr = text.start + i + 3; // E8 instruction
                    int32_t rel;
                    memcpy(&rel, tp + i + 4, 4);
                    uintptr_t target = callAddr + 5 + rel;
                    Report("    Match[%d] at 0x%08X -> CALL target 0x%08X (RVA 0x%X)",
                           matchCount, (unsigned)(text.start + i), (unsigned)target,
                           (unsigned)(target - (text.start - 0x1000)));
                    matchCount++;
                }
            }
            Report("  Total '83 C1 DC E8' matches: %d", matchCount);
        }

        int pCodeCount = 0;
        for (size_t i = 0; i + 10 < rdataSize && pCodeCount < 5; i++) {
            if (memcmp(rdataPtr + i, "P:\\Code\\Engine\\Frame", 19) == 0) {
                const char* str = rdataPtr + i;
                size_t len = 0;
                while (len < 200 && str[len] != '\0') len++;
                Report("  Found P:\\Code\\Engine\\Frame path at 0x%08X: \"%.*s\"",
                       (unsigned)(rdata.start + i), (int)len, str);
                pCodeCount++;
                i += len;
            }
        }
    }
    Report("");
}
