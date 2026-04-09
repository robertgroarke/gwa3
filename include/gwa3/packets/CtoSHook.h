#pragma once

#include <cstdint>

namespace GWA3::CtoSHook {

    // Install a mid-function hook at Offsets::Render for CtoS shellcode dispatch.
    // This hook coexists with GameThread's MinHook at the function prologue.
    // A watchdog thread re-patches the hook when the game's integrity checker
    // restores original bytes (~38s cycle).
    bool Initialize();
    void Shutdown();

    // Queue a shellcode command for execution from the render thread.
    // The command must be a VirtualAlloc'd buffer with executable shellcode
    // ending in RET (0xC3).
    bool EnqueueCommand(uintptr_t command);

    // Experimental AutoIt-style packet lane.
    // This is intentionally isolated from the main CtoS sender-thread path and is
    // only meant for interaction-command experiments while we compare hook context.
    bool SendPacketCommand(uint32_t size, uint32_t header, ...);

    bool IsInitialized();
    uint32_t GetHeartbeat();

} // namespace GWA3::CtoSHook
