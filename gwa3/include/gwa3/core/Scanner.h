#pragma once

#include <Windows.h>
#include <cstdint>

namespace GWA3::Scanner {

    struct Section {
        uintptr_t start;
        uintptr_t size;
    };

    // Initialize scanner: parse PE headers of GW.exe, locate .text/.rdata/.data sections.
    // Call with GetModuleHandleA(nullptr) from within the injected process.
    bool Initialize(HMODULE gwModule);

    // Byte-pattern scan with '?' wildcards.
    // pattern: raw bytes to match. mask: 'x' = match, '?' = wildcard. offset: added to result.
    // Scans .text section by default. Returns 0 on failure.
    uintptr_t Find(const char* pattern, const char* mask, int offset = 0);

    // Scan within a specific address range.
    uintptr_t FindInRange(const char* pattern, const char* mask, int offset,
                          uintptr_t rangeStart, uintptr_t rangeSize);

    // Assertion-based scan: find a string literal in .rdata, then locate the xref
    // instruction pair (MOV EDX, fileAddr / MOV ECX, msgAddr) in .text.
    // Returns address of the MOV EDX instruction + offset. Returns 0 on failure.
    uintptr_t FindAssertion(const char* sourceFile, const char* message, int offset = 0);

    // Resolve an E8 rel32 (CALL) instruction at 'address' to its target.
    uintptr_t FunctionFromNearCall(uintptr_t address);

    // Resolve E8/E9/EB branch chains up to maxDepth hops.
    uintptr_t ResolveBranchChain(uintptr_t address, int maxDepth = 8);

    // Section accessors (valid after Initialize).
    Section GetTextSection();
    Section GetRdataSection();
    Section GetDataSection();

    // Check if scanner is initialized.
    bool IsInitialized();

} // namespace GWA3::Scanner
