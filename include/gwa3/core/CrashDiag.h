#pragma once

#include <cstdint>

namespace GWA3::CrashDiag {

    bool Initialize();
    void CaptureProcessState(const char* tag);
    void Shutdown();

    // Resolve a DLL-relative RVA to a function name using the MAP file.
    // Returns nullptr if no MAP file loaded or RVA not found.
    const char* ResolveRvaToFunctionName(uintptr_t rva);

    // Get the number of loaded MAP file entries (for diagnostics).
    unsigned GetMapEntryCount();

} // namespace GWA3::CrashDiag
