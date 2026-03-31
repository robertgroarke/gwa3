#include <gwa3/core/Memory.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::Memory {

// --- Patch implementation ---

Patch::Patch()
    : address(0), patchedBytes(nullptr), originalBytes(nullptr),
      size(0), enabled(false), staged(false) {}

Patch::~Patch() {
    if (enabled) Disable();
    delete[] patchedBytes;
    delete[] originalBytes;
}

Patch::Patch(Patch&& other) noexcept
    : address(other.address), patchedBytes(other.patchedBytes),
      originalBytes(other.originalBytes), size(other.size),
      enabled(other.enabled), staged(other.staged) {
    other.patchedBytes = nullptr;
    other.originalBytes = nullptr;
    other.staged = false;
    other.enabled = false;
}

Patch& Patch::operator=(Patch&& other) noexcept {
    if (this != &other) {
        if (enabled) Disable();
        delete[] patchedBytes;
        delete[] originalBytes;
        address = other.address;
        patchedBytes = other.patchedBytes;
        originalBytes = other.originalBytes;
        size = other.size;
        enabled = other.enabled;
        staged = other.staged;
        other.patchedBytes = nullptr;
        other.originalBytes = nullptr;
        other.staged = false;
        other.enabled = false;
    }
    return *this;
}

bool Patch::SetPatch(uintptr_t addr, const uint8_t* bytes, uint32_t len) {
    if (enabled) Disable();
    delete[] patchedBytes;
    delete[] originalBytes;

    address = addr;
    size = len;
    patchedBytes = new uint8_t[len];
    originalBytes = new uint8_t[len];
    memcpy(patchedBytes, bytes, len);
    memset(originalBytes, 0, len);
    enabled = false;
    staged = true;
    return true;
}

bool Patch::SetRedirect(uintptr_t addr, uintptr_t target) {
    uint8_t jmp[5];
    jmp[0] = 0xE9; // JMP rel32
    int32_t rel = static_cast<int32_t>(target - addr - 5);
    memcpy(jmp + 1, &rel, 4);
    return SetPatch(addr, jmp, 5);
}

bool Patch::Enable() {
    if (!staged || enabled || !address || !size) return false;

    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log::Error("MemoryPatcher: VirtualProtect failed at 0x%08X (error %lu)", address, GetLastError());
        return false;
    }

    // Save original bytes
    memcpy(originalBytes, reinterpret_cast<void*>(address), size);

    // Write patched bytes
    memcpy(reinterpret_cast<void*>(address), patchedBytes, size);

    // Flush instruction cache
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);

    VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);

    enabled = true;
    Log::Info("MemoryPatcher: Enabled patch at 0x%08X (%u bytes)", address, size);
    return true;
}

bool Patch::Disable() {
    if (!enabled || !address || !size) return false;

    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log::Error("MemoryPatcher: VirtualProtect failed at 0x%08X (error %lu)", address, GetLastError());
        return false;
    }

    // Restore original bytes
    memcpy(reinterpret_cast<void*>(address), originalBytes, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);

    VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);

    enabled = false;
    Log::Info("MemoryPatcher: Disabled patch at 0x%08X", address);
    return true;
}

bool Patch::Toggle() {
    return enabled ? Disable() : Enable();
}

// --- Global patch instances ---

static Patch s_cameraUnlock;
static Patch s_levelDataBypass;
static Patch s_mapPortBypass;

Patch& GetCameraUnlockPatch()    { return s_cameraUnlock; }
Patch& GetLevelDataBypassPatch() { return s_levelDataBypass; }
Patch& GetMapPortBypassPatch()   { return s_mapPortBypass; }

void EnableAllPatches() {
    if (s_cameraUnlock.staged)    s_cameraUnlock.Enable();
    if (s_levelDataBypass.staged) s_levelDataBypass.Enable();
    if (s_mapPortBypass.staged)   s_mapPortBypass.Enable();
}

void DisableAllPatches() {
    if (s_cameraUnlock.enabled)    s_cameraUnlock.Disable();
    if (s_levelDataBypass.enabled) s_levelDataBypass.Disable();
    if (s_mapPortBypass.enabled)   s_mapPortBypass.Disable();
}

} // namespace GWA3::Memory
