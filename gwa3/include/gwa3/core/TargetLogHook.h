#pragma once

#include <cstdint>

namespace GWA3::TargetLogHook {

    bool Initialize();
    void Shutdown();
    bool IsInitialized();
    uint32_t GetTarget(uint32_t agentId);
    uint32_t GetCallCount();
    uint32_t GetStoreCount();

} // namespace GWA3::TargetLogHook
