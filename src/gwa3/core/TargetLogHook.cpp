#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/HookMarker.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::TargetLogHook {

static constexpr uint32_t kTargetLogSize = 0x200;
static constexpr uint32_t kPatchSize = 5;

static bool s_initialized = false;
static uintptr_t s_returnAddr = 0;
static uint8_t s_savedBytes[kPatchSize] = {};
static uint32_t s_targetLog[kTargetLogSize] = {};
static volatile LONG s_callCount = 0;
static volatile LONG s_storeCount = 0;

static __declspec(naked) void TargetLogDetourNaked() {
    __asm {
        // Mark hook active for CrashDiag
        call dword ptr [GetTickCount]
        mov dword ptr [g_HookTick_TargetLogDetour], eax
        inc dword ptr [s_callCount]
        cmp ecx, 4
        jz target_log_main
        cmp ecx, 0x20
        jz target_log_main
        cmp ecx, 0x32
        jz target_log_main
        cmp ecx, 0x3C
        jz target_log_main
        jmp target_log_exit

target_log_main:
        pushad
        mov ecx, dword ptr [ebp+8]
        test ecx, ecx
        jnz target_log_store
        mov ecx, edx

target_log_store:
        inc dword ptr [s_storeCount]
        mov eax, offset s_targetLog
        lea eax, [eax + edx*4]
        mov dword ptr [eax], ecx
        popad

target_log_exit:
        push ebx
        push esi
        push edi
        mov edi, edx
        jmp [s_returnAddr]
    }
}

bool Initialize() {
    if (s_initialized) return true;
    if (!Offsets::TargetLog) {
        Log::Warn("TargetLogHook: TargetLog offset not resolved");
        return false;
    }

    const uintptr_t hookAddr = Offsets::TargetLog;
    s_returnAddr = hookAddr + kPatchSize;
    ZeroMemory(s_targetLog, sizeof(s_targetLog));
    s_callCount = 0;
    s_storeCount = 0;
    memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log::Error("TargetLogHook: VirtualProtect failed");
        return false;
    }

    uint8_t patch[kPatchSize];
    patch[0] = 0xE9;
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&TargetLogDetourNaked) - (hookAddr + 5));
    memcpy(patch + 1, &rel, sizeof(rel));
    memcpy(reinterpret_cast<void*>(hookAddr), patch, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    s_initialized = true;
    Log::Info("TargetLogHook: Installed at 0x%08X -> return at 0x%08X", hookAddr, s_returnAddr);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    const uintptr_t hookAddr = Offsets::TargetLog;
    DWORD oldProtect = 0;
    if (VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(reinterpret_cast<void*>(hookAddr), s_savedBytes, kPatchSize);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
        VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);
    }

    ZeroMemory(s_targetLog, sizeof(s_targetLog));
    s_callCount = 0;
    s_storeCount = 0;
    s_initialized = false;
    Log::Info("TargetLogHook: Shutdown");
}

bool IsInitialized() {
    return s_initialized;
}

uint32_t GetTarget(uint32_t agentId) {
    if (!s_initialized || agentId >= kTargetLogSize) return 0;
    return s_targetLog[agentId];
}

uint32_t GetCallCount() {
    return static_cast<uint32_t>(s_callCount);
}

uint32_t GetStoreCount() {
    return static_cast<uint32_t>(s_storeCount);
}

} // namespace GWA3::TargetLogHook
