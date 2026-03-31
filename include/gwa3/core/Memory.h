#pragma once

#include <cstdint>
#include <vector>

namespace GWA3::Memory {

    // A single memory patch: replaces bytes at target address.
    struct Patch {
        uintptr_t address;
        uint8_t*  patchedBytes;
        uint8_t*  originalBytes;
        uint32_t  size;
        bool      enabled;
        bool      staged;

        Patch();
        ~Patch();
        Patch(const Patch&) = delete;
        Patch& operator=(const Patch&) = delete;
        Patch(Patch&& other) noexcept;
        Patch& operator=(Patch&& other) noexcept;

        // Stage a patch (bytes are buffered, not yet applied)
        bool SetPatch(uintptr_t addr, const uint8_t* bytes, uint32_t len);

        // Stage a 5-byte JMP redirect
        bool SetRedirect(uintptr_t addr, uintptr_t target);

        // Apply the staged patch
        bool Enable();

        // Restore original bytes
        bool Disable();

        // Toggle on/off
        bool Toggle();
    };

    // Global patcher — manages a collection of named patches.
    void EnableAllPatches();
    void DisableAllPatches();

    // Register named patches for known game-side modifications.
    Patch& GetCameraUnlockPatch();
    Patch& GetLevelDataBypassPatch();
    Patch& GetMapPortBypassPatch();

} // namespace GWA3::Memory
