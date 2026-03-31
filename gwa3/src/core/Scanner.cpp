#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>

#include <cstring>

namespace GWA3::Scanner {

static bool s_initialized = false;
static uintptr_t s_baseAddress = 0;
static Section s_text{};
static Section s_rdata{};
static Section s_data{};

// --- PE Parsing ---

static bool ParsePESections(HMODULE gwModule) {
    auto base = reinterpret_cast<uintptr_t>(gwModule);
    s_baseAddress = base;

    auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        Log::Error("Scanner: Invalid DOS signature at 0x%08X", base);
        return false;
    }

    auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        Log::Error("Scanner: Invalid PE signature");
        return false;
    }

    WORD sectionCount = ntHeaders->FileHeader.NumberOfSections;
    auto* section = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < sectionCount; i++, section++) {
        const char* name = reinterpret_cast<const char*>(section->Name);
        uintptr_t start = base + section->VirtualAddress;
        uintptr_t size = section->Misc.VirtualSize > section->SizeOfRawData
                             ? section->Misc.VirtualSize
                             : section->SizeOfRawData;

        if (strncmp(name, ".text", 5) == 0) {
            s_text = {start, size};
        } else if (strncmp(name, ".rdata", 6) == 0) {
            s_rdata = {start, size};
        } else if (strncmp(name, ".data", 5) == 0) {
            s_data = {start, size};
        }
    }

    return s_text.size > 0 && s_rdata.size > 0;
}

// --- Core Scan ---

static uintptr_t ScanRegion(const char* pattern, const char* mask, uintptr_t start, uintptr_t size) {
    size_t patLen = strlen(mask);
    if (patLen == 0 || size < patLen) return 0;

    const auto* mem = reinterpret_cast<const uint8_t*>(start);
    const auto* pat = reinterpret_cast<const uint8_t*>(pattern);
    size_t scanEnd = size - patLen;

    for (size_t i = 0; i <= scanEnd; i++) {
        bool match = true;
        for (size_t j = 0; j < patLen; j++) {
            if (mask[j] == 'x' && mem[i + j] != pat[j]) {
                match = false;
                break;
            }
        }
        if (match) return start + i;
    }
    return 0;
}

// --- String search in section (for assertion patterns) ---

static uintptr_t FindStringInSection(const char* str, const Section& section) {
    size_t len = strlen(str);
    if (len == 0 || section.size < len) return 0;

    const auto* mem = reinterpret_cast<const uint8_t*>(section.start);
    size_t scanEnd = section.size - len;

    for (size_t i = 0; i <= scanEnd; i++) {
        if (memcmp(mem + i, str, len) == 0) {
            // Verify null-terminated (C string literal in .rdata)
            if (mem[i + len] == 0) {
                return section.start + i;
            }
        }
    }
    return 0;
}

// Find a 32-bit immediate value reference in .text (push/mov with the address)
static uintptr_t FindDwordXref(uint32_t value, const Section& section) {
    if (section.size < 4) return 0;

    const auto* mem = reinterpret_cast<const uint8_t*>(section.start);
    size_t scanEnd = section.size - 4;

    for (size_t i = 0; i <= scanEnd; i++) {
        uint32_t candidate;
        memcpy(&candidate, mem + i, 4);
        if (candidate == value) {
            return section.start + i;
        }
    }
    return 0;
}

// --- Public API ---

bool Initialize(HMODULE gwModule) {
    if (s_initialized) return true;
    if (!gwModule) {
        Log::Error("Scanner: null module handle");
        return false;
    }

    if (!ParsePESections(gwModule)) {
        Log::Error("Scanner: Failed to parse PE sections");
        return false;
    }

    Log::Info("Scanner: base=0x%08X .text=0x%08X(%u) .rdata=0x%08X(%u) .data=0x%08X(%u)",
              s_baseAddress,
              s_text.start, s_text.size,
              s_rdata.start, s_rdata.size,
              s_data.start, s_data.size);

    s_initialized = true;
    return true;
}

uintptr_t Find(const char* pattern, const char* mask, int offset) {
    if (!s_initialized) return 0;
    uintptr_t result = ScanRegion(pattern, mask, s_text.start, s_text.size);
    return result ? result + offset : 0;
}

uintptr_t FindInRange(const char* pattern, const char* mask, int offset,
                      uintptr_t rangeStart, uintptr_t rangeSize) {
    uintptr_t result = ScanRegion(pattern, mask, rangeStart, rangeSize);
    return result ? result + offset : 0;
}

uintptr_t FindAssertion(const char* sourceFile, const char* message, int offset) {
    if (!s_initialized) return 0;

    // Phase 1: locate both strings in .rdata
    uintptr_t fileAddr = FindStringInSection(sourceFile, s_rdata);
    if (!fileAddr) {
        Log::Warn("Scanner: FindAssertion — string not found in .rdata: \"%s\"", sourceFile);
        return 0;
    }

    uintptr_t msgAddr = FindStringInSection(message, s_rdata);
    if (!msgAddr) {
        Log::Warn("Scanner: FindAssertion — string not found in .rdata: \"%s\"", message);
        return 0;
    }

    // Phase 2: build pattern for MOV EDX, fileAddr / MOV ECX, msgAddr
    // This is the assertion pattern used by GW's debug strings:
    //   BA <fileAddr_le>  B9 <msgAddr_le>
    uint8_t assertPattern[10];
    assertPattern[0] = 0xBA; // MOV EDX, imm32
    memcpy(assertPattern + 1, &fileAddr, 4);
    assertPattern[5] = 0xB9; // MOV ECX, imm32
    memcpy(assertPattern + 6, &msgAddr, 4);

    uintptr_t result = ScanRegion(
        reinterpret_cast<const char*>(assertPattern),
        "xxxxxxxxxx",
        s_text.start, s_text.size
    );

    if (!result) {
        // Try alternative: PUSH fileAddr / PUSH msgAddr (some compilers)
        // or the addresses might be referenced via MOV with different register combos.
        // Fall back to finding any xref to the message string address near a file string xref.
        uintptr_t msgXref = FindDwordXref(static_cast<uint32_t>(msgAddr), s_text);
        if (msgXref) {
            // Scan nearby (within 32 bytes before) for the file address
            uintptr_t searchStart = (msgXref > s_text.start + 32) ? msgXref - 32 : s_text.start;
            uintptr_t searchSize = msgXref - searchStart + 8;
            uint8_t addrBytes[4];
            memcpy(addrBytes, &fileAddr, 4);
            uintptr_t fileXref = ScanRegion(
                reinterpret_cast<const char*>(addrBytes), "xxxx",
                searchStart, searchSize
            );
            if (fileXref) {
                // Back up to the instruction that loads the file address (1 byte before for MOV/PUSH opcode)
                result = fileXref - 1;
            }
        }
    }

    if (!result) {
        Log::Warn("Scanner: FindAssertion — xref not found for \"%s\" / \"%s\"", sourceFile, message);
        return 0;
    }

    return result + offset;
}

uintptr_t FunctionFromNearCall(uintptr_t address) {
    if (!address) return 0;
    uint8_t opcode = *reinterpret_cast<const uint8_t*>(address);
    if (opcode != 0xE8) return 0;

    int32_t rel;
    memcpy(&rel, reinterpret_cast<const void*>(address + 1), 4);
    return address + 5 + rel;
}

uintptr_t ResolveBranchChain(uintptr_t address, int maxDepth) {
    for (int i = 0; i < maxDepth && address != 0; i++) {
        uint8_t opcode = *reinterpret_cast<const uint8_t*>(address);

        if (opcode == 0xE8 || opcode == 0xE9) {
            // CALL rel32 or JMP rel32
            int32_t rel;
            memcpy(&rel, reinterpret_cast<const void*>(address + 1), 4);
            address = address + 5 + rel;
        } else if (opcode == 0xEB) {
            // JMP rel8 (short)
            int8_t rel = *reinterpret_cast<const int8_t*>(address + 1);
            address = address + 2 + rel;
        } else {
            return address; // Non-branch instruction — we've arrived
        }
    }
    return 0; // Max depth exceeded
}

Section GetTextSection()  { return s_text; }
Section GetRdataSection() { return s_rdata; }
Section GetDataSection()  { return s_data; }
bool IsInitialized()      { return s_initialized; }

} // namespace GWA3::Scanner
