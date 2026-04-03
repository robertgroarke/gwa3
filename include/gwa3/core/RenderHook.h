#pragma once

#include <cstdint>

namespace GWA3::RenderHook {

    // Install the pre-game render detour at the same mid-function seam
    // used by AutoIt's RenderingModProc.
    bool Initialize();
    void Shutdown();

    // Queue an executable command stub. The stub is called from the render
    // detour during pre-game / character select.
    bool EnqueueCommand(uintptr_t command);

    // Once the client is on a map, stop draining the pre-game queue while
    // still replaying the overwritten original render instructions.
    void SetMapLoaded(bool loaded);

    bool IsInitialized();

    // Diagnostic: queue counter and pending entry count
    uint32_t GetQueueCounter();
    uint32_t GetPendingCount();

    // Heartbeat: incremented every render frame (~60fps)
    uint32_t GetHeartbeat();

    // Check if the JMP patch is still intact at the hook site
    bool IsHookIntact();

    // Crash detection helper (not used directly — watchdog thread compares heartbeats)
    bool IsCrashDetected();

} // namespace GWA3::RenderHook
