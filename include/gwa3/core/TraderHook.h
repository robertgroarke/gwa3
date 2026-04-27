#pragma once

#include <cstdint>

namespace GWA3::TraderHook {

    bool Initialize();
    void Shutdown();
    bool IsInitialized();

    void Reset();
    uint32_t GetQuoteId();
    uint32_t GetCostItemId();
    uint32_t GetCostValue();

    // Debug: raw register state at hook point
    uintptr_t GetDebugEbx();

} // namespace GWA3::TraderHook
