#pragma once

// MemoryMgr — GW client version, skill timer, window handle, game heap.

#include <cstdint>

namespace GWA3::MemoryMgr {

    bool Initialize();

    // Client build number (from PE version info)
    uint32_t GetGWVersion();

    // Global skill timer (milliseconds, used for recharge calculations)
    uint32_t GetSkillTimer();

    // HWND of the Guild Wars game window
    void* GetGWWindowHandle();

    // GW user data directory (e.g. "C:\Users\...\Documents\Guild Wars")
    bool GetPersonalDir(wchar_t* buf, uint32_t bufLen);

    // Game heap allocation (USE AT YOUR OWN RISK — no RAII, must manually free)
    void* MemAlloc(uint32_t size);
    void  MemFree(void* ptr);

} // namespace GWA3::MemoryMgr
