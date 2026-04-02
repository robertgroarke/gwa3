#include <gwa3/core/TraderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::TraderHook {

static constexpr uint32_t kPatchSize = 5;

static bool s_initialized = false;
static uintptr_t s_returnAddr = 0;
static uint8_t s_savedBytes[kPatchSize] = {};
static volatile LONG s_quoteId = 0;
static volatile LONG s_costItemId = 0;
static volatile LONG s_costValue = 0;

static __declspec(naked) void TraderDetourNaked() {
    __asm {
        push eax
        mov eax, dword ptr [ebx+28]
        mov eax, [eax]
        mov dword ptr [s_costItemId], eax
        mov eax, dword ptr [ebx+28]
        mov eax, [eax+4]
        mov dword ptr [s_costValue], eax
        pop eax
        mov ebx, dword ptr [ebp+0Ch]
        mov esi, eax
        push eax
        mov eax, dword ptr [s_quoteId]
        inc eax
        cmp eax, 200
        jnz trader_skip_reset
        xor eax, eax
trader_skip_reset:
        mov dword ptr [s_quoteId], eax
        pop eax
        jmp [s_returnAddr]
    }
}

bool Initialize() {
    if (s_initialized) return true;
    if (!Offsets::Trader) {
        Log::Warn("TraderHook: Trader offset not resolved");
        return false;
    }

    const uintptr_t hookAddr = Offsets::Trader;
    s_returnAddr = hookAddr + kPatchSize;
    memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);
    Reset();

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log::Error("TraderHook: VirtualProtect failed");
        return false;
    }

    uint8_t patch[kPatchSize];
    patch[0] = 0xE9;
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&TraderDetourNaked) - (hookAddr + 5));
    memcpy(patch + 1, &rel, sizeof(rel));
    memcpy(reinterpret_cast<void*>(hookAddr), patch, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
    VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);

    s_initialized = true;
    Log::Info("TraderHook: Installed at 0x%08X -> return at 0x%08X", hookAddr, s_returnAddr);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    const uintptr_t hookAddr = Offsets::Trader;
    DWORD oldProtect = 0;
    if (VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(reinterpret_cast<void*>(hookAddr), s_savedBytes, kPatchSize);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
        VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);
    }

    Reset();
    s_initialized = false;
    Log::Info("TraderHook: Shutdown");
}

bool IsInitialized() {
    return s_initialized;
}

void Reset() {
    s_quoteId = 0;
    s_costItemId = 0;
    s_costValue = 0;
}

uint32_t GetQuoteId() {
    return static_cast<uint32_t>(s_quoteId);
}

uint32_t GetCostItemId() {
    return static_cast<uint32_t>(s_costItemId);
}

uint32_t GetCostValue() {
    return static_cast<uint32_t>(s_costValue);
}

} // namespace GWA3::TraderHook
