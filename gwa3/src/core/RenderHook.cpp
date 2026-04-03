#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>

namespace GWA3::RenderHook {

static constexpr uint32_t kQueueSize = 256;
static constexpr uint32_t kPatchSize = 5;

static volatile LONG s_queueCounter = 0;
static volatile LONG s_savedCommand = 0;
static volatile LONG s_mapLoaded = 0;
static volatile LONG s_disableRendering = 0;
static volatile LONG s_heartbeat = 0;  // incremented every render frame
static uintptr_t s_queue[kQueueSize] = {};
static bool s_initialized = false;
static uintptr_t s_returnAddr = 0;
static uint8_t s_savedBytes[kPatchSize] = {};

// Naked detour — called via JMP from mid-function patch.
// Mirrors the AutoIt RenderingModProc path:
// process one queued command during pre-game, then replay the original
// overwritten instructions before jumping back to Render+0xA.
static __declspec(naked) void RenderDetourNaked() {
    __asm {
        pushad
        pushfd

        mov eax, dword ptr [s_queueCounter]
        mov ecx, eax
        mov ebx, dword ptr [s_queue + eax * 4]
        test ebx, ebx
        jz skip_queue

        mov dword ptr [s_queue + eax * 4], 0
        mov dword ptr [s_savedCommand], ebx

        mov eax, ecx
        inc eax
        cmp eax, kQueueSize
        jnz no_reset
        xor eax, eax
no_reset:
        mov dword ptr [s_queueCounter], eax
        call dword ptr [s_savedCommand]

skip_queue:
        inc dword ptr [s_heartbeat]
        popfd
        popad
        add esp, 4
        cmp dword ptr [s_disableRendering], 1
        jmp [s_returnAddr]
    }
}

bool Initialize() {
    if (s_initialized) return true;
    if (!Offsets::Render) {
        Log::Error("RenderHook: Render offset not resolved");
        return false;
    }

    uintptr_t hookAddr = Offsets::Render;
    s_returnAddr = hookAddr + 0xA; // AutoIt uses RenderingModReturn = Render + 0xA
    s_queueCounter = 0;
    s_savedCommand = 0;
    s_mapLoaded = 0;
    s_disableRendering = 0;
    ZeroMemory(s_queue, sizeof(s_queue));

    // AutoIt's WriteDetour writes a 5-byte JMP at the hook site and returns
    // to Render+0xA from the detour body. Keep bytes +5..+9 intact.
    memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

    // Write only the 5-byte JMP detour, matching AutoIt's behavior.
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t patch[kPatchSize];
    patch[0] = 0xE9; // JMP rel32
    int32_t rel = reinterpret_cast<uintptr_t>(&RenderDetourNaked) - (hookAddr + 5);
    memcpy(patch + 1, &rel, 4);

    memcpy(reinterpret_cast<void*>(hookAddr), patch, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);

    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    s_initialized = true;
    Log::Info("RenderHook: Installed at 0x%08X -> return at 0x%08X",
              hookAddr, s_returnAddr);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    // Restore original bytes
    uintptr_t hookAddr = Offsets::Render;
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(hookAddr), s_savedBytes, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    ZeroMemory(s_queue, sizeof(s_queue));
    s_queueCounter = 0;
    s_savedCommand = 0;
    s_mapLoaded = 0;
    s_initialized = false;
    Log::Info("RenderHook: Shutdown");
}

bool EnqueueCommand(uintptr_t command) {
    if (!s_initialized || command < 0x10000) return false;

    for (uint32_t attempt = 0; attempt < kQueueSize; ++attempt) {
        uint32_t index = (static_cast<uint32_t>(s_queueCounter) + attempt) % kQueueSize;
        if (s_queue[index] != 0) continue;
        s_queue[index] = command;
        return true;
    }

    Log::Warn("RenderHook: queue full, dropping command 0x%08X", command);
    return false;
}

void SetMapLoaded(bool loaded) {
    s_mapLoaded = loaded ? 1 : 0;
    Log::Info("RenderHook: MapLoaded=%d", loaded ? 1 : 0);
}

bool IsInitialized() {
    return s_initialized;
}

uint32_t GetQueueCounter() {
    return static_cast<uint32_t>(s_queueCounter);
}

uint32_t GetPendingCount() {
    uint32_t count = 0;
    for (uint32_t i = 0; i < kQueueSize; i++) {
        if (s_queue[i] != 0) count++;
    }
    return count;
}

uint32_t GetHeartbeat() {
    return static_cast<uint32_t>(s_heartbeat);
}

bool IsRenderFrozen(uint32_t timeoutMs) {
    if (!s_initialized) return true;
    uint32_t before = static_cast<uint32_t>(s_heartbeat);
    Sleep(timeoutMs);
    uint32_t after = static_cast<uint32_t>(s_heartbeat);
    return (after == before);
}

} // namespace GWA3::RenderHook
