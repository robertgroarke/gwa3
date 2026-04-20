#pragma once

#include <cstdint>

namespace GWA3::TradePartnerHook {

    bool Initialize();
    void Shutdown();
    bool IsInitialized();

    void Reset();
    uint32_t GetHitCount();
    uint32_t GetLastEax();
    uint32_t GetLastEcx();
    uint32_t GetLastEdx();

} // namespace GWA3::TradePartnerHook
