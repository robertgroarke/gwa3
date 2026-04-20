#include <gwa3/core/TradePartnerHook.h>

#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/HookMarker.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::TradePartnerHook {

static constexpr uint32_t kPatchSize = 5;

static bool s_initialized = false;
static uintptr_t s_hookAddr = 0;
static uintptr_t s_returnAddr = 0;
static uintptr_t s_trampoline = 0;
static uint8_t s_savedBytes[kPatchSize] = {};
static volatile LONG s_hitCount = 0;
static volatile LONG s_lastEax = 0;
static volatile LONG s_lastEcx = 0;
static volatile LONG s_lastEdx = 0;
static volatile LONG s_prevHookId = 0;
static void (__stdcall *s_pHookMarkerEnterTradePartnerDetour)() = nullptr;
static void (__stdcall *s_pHookMarkerLeaveTradePartnerDetour)() = nullptr;

static void __stdcall HookMarkerEnterTradePartnerDetourThunk() {
    s_prevHookId = HookMarker::t_activeHookId;
    HookMarker::Enter(HookMarker::HookId::TradePartnerDetour);
}

static void __stdcall HookMarkerLeaveTradePartnerDetourThunk() {
    HookMarker::Leave(static_cast<int>(s_prevHookId));
}

static __declspec(naked) void TradePartnerDetourNaked() {
    __asm {
        pushfd
        pushad

        call dword ptr [s_pHookMarkerEnterTradePartnerDetour]

        inc dword ptr [s_hitCount]

        mov eax, dword ptr [esp+28]
        mov dword ptr [s_lastEax], eax
        mov eax, dword ptr [esp+24]
        mov dword ptr [s_lastEcx], eax
        mov eax, dword ptr [esp+20]
        mov dword ptr [s_lastEdx], eax

        call dword ptr [s_pHookMarkerLeaveTradePartnerDetour]
        popad
        popfd
        jmp [s_trampoline]
    }
}

bool Initialize() {
    if (s_initialized) return true;
    if (Offsets::TradePartner <= 0x10000) {
        Log::Warn("TradePartnerHook: TradePartner offset not resolved");
        return false;
    }

    s_hookAddr = Offsets::TradePartner;
    s_returnAddr = s_hookAddr + kPatchSize;
    memcpy(s_savedBytes, reinterpret_cast<void*>(s_hookAddr), kPatchSize);
    Reset();

    s_trampoline = reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!s_trampoline) {
        Log::Error("TradePartnerHook: VirtualAlloc failed");
        return false;
    }

    memcpy(reinterpret_cast<void*>(s_trampoline), s_savedBytes, kPatchSize);
    const uintptr_t trampJump = s_trampoline + kPatchSize;
    *reinterpret_cast<uint8_t*>(trampJump) = 0xE9;
    const int32_t trampRel = static_cast<int32_t>(s_returnAddr - (trampJump + 5));
    memcpy(reinterpret_cast<void*>(trampJump + 1), &trampRel, sizeof(trampRel));

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(s_hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log::Error("TradePartnerHook: VirtualProtect failed");
        VirtualFree(reinterpret_cast<void*>(s_trampoline), 0, MEM_RELEASE);
        s_trampoline = 0;
        return false;
    }

    uint8_t patch[kPatchSize];
    patch[0] = 0xE9;
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&TradePartnerDetourNaked) - (s_hookAddr + 5));
    memcpy(patch + 1, &rel, sizeof(rel));
    memcpy(reinterpret_cast<void*>(s_hookAddr), patch, kPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_hookAddr), kPatchSize);
    VirtualProtect(reinterpret_cast<void*>(s_hookAddr), kPatchSize, oldProtect, &oldProtect);

    s_pHookMarkerEnterTradePartnerDetour = &HookMarkerEnterTradePartnerDetourThunk;
    s_pHookMarkerLeaveTradePartnerDetour = &HookMarkerLeaveTradePartnerDetourThunk;
    s_initialized = true;
    Log::Info("TradePartnerHook: Installed at 0x%08X -> trampoline 0x%08X",
              static_cast<unsigned>(s_hookAddr),
              static_cast<unsigned>(s_trampoline));
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    DWORD oldProtect = 0;
    if (VirtualProtect(reinterpret_cast<void*>(s_hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(reinterpret_cast<void*>(s_hookAddr), s_savedBytes, kPatchSize);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_hookAddr), kPatchSize);
        VirtualProtect(reinterpret_cast<void*>(s_hookAddr), kPatchSize, oldProtect, &oldProtect);
    }

    if (s_trampoline) {
        VirtualFree(reinterpret_cast<void*>(s_trampoline), 0, MEM_RELEASE);
        s_trampoline = 0;
    }

    s_hookAddr = 0;
    s_returnAddr = 0;
    Reset();
    s_initialized = false;
    Log::Info("TradePartnerHook: Shutdown");
}

bool IsInitialized() {
    return s_initialized;
}

void Reset() {
    s_hitCount = 0;
    s_lastEax = 0;
    s_lastEcx = 0;
    s_lastEdx = 0;
}

uint32_t GetHitCount() {
    return static_cast<uint32_t>(s_hitCount);
}

uint32_t GetLastEax() {
    return static_cast<uint32_t>(s_lastEax);
}

uint32_t GetLastEcx() {
    return static_cast<uint32_t>(s_lastEcx);
}

uint32_t GetLastEdx() {
    return static_cast<uint32_t>(s_lastEdx);
}

} // namespace GWA3::TradePartnerHook
