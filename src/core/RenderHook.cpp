#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <vector>
#include <mutex>

namespace GWA3::RenderHook {

static CRITICAL_SECTION s_cs;
static std::vector<std::function<void()>> s_queue;
static bool s_initialized = false;
static uintptr_t s_returnAddr = 0;
static uint8_t s_savedBytes[10] = {};

// C++ function to drain the render queue — called from naked detour
static void __cdecl DrainRenderQueue() {
    EnterCriticalSection(&s_cs);
    std::vector<std::function<void()>> local;
    if (!s_queue.empty()) {
        local.swap(s_queue);
    }
    LeaveCriticalSection(&s_cs);

    for (auto& task : local) {
        task();
    }
}

// Naked detour — called via JMP from mid-function patch.
// Preserves all registers, drains queue, then jumps back.
static __declspec(naked) void RenderDetourNaked() {
    __asm {
        pushad
        pushfd
        call DrainRenderQueue
        popfd
        popad
        jmp [s_returnAddr]
    }
}

bool Initialize() {
    if (s_initialized) return true;
    if (!Offsets::Render) {
        Log::Error("RenderHook: Render offset not resolved");
        return false;
    }

    InitializeCriticalSection(&s_cs);

    uintptr_t hookAddr = Offsets::Render;
    s_returnAddr = hookAddr + 0xA; // AutoIt uses RenderingModReturn = Render + 0xA

    // Save original 10 bytes
    memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), 10);

    // Write JMP to our detour (5-byte JMP + 5 NOPs to fill 10 bytes)
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), 10, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t patch[10];
    patch[0] = 0xE9; // JMP rel32
    int32_t rel = reinterpret_cast<uintptr_t>(&RenderDetourNaked) - (hookAddr + 5);
    memcpy(patch + 1, &rel, 4);
    for (int i = 5; i < 10; i++) patch[i] = 0x90; // NOP

    memcpy(reinterpret_cast<void*>(hookAddr), patch, 10);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), 10);

    VirtualProtect(reinterpret_cast<void*>(hookAddr), 10, oldProtect, &oldProtect);

    s_initialized = true;
    Log::Info("RenderHook: Mid-function JMP installed at 0x%08X -> return at 0x%08X",
              hookAddr, s_returnAddr);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    // Restore original bytes
    uintptr_t hookAddr = Offsets::Render;
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(hookAddr), 10, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(hookAddr), s_savedBytes, 10);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), 10);
    VirtualProtect(reinterpret_cast<void*>(hookAddr), 10, oldProtect, &oldProtect);

    EnterCriticalSection(&s_cs);
    s_queue.clear();
    LeaveCriticalSection(&s_cs);
    DeleteCriticalSection(&s_cs);
    s_initialized = false;
    Log::Info("RenderHook: Shutdown");
}

void Enqueue(std::function<void()> task) {
    if (!s_initialized) return;
    EnterCriticalSection(&s_cs);
    s_queue.push_back(std::move(task));
    LeaveCriticalSection(&s_cs);
}

bool IsInitialized() { return s_initialized; }

} // namespace GWA3::RenderHook
