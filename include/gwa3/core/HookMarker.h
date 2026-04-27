#pragma once

#include <cstdint>
#include <Windows.h>

namespace GWA3::HookMarker {

// Enum of every hook/detour in the DLL.  The numeric values are stable -
// do NOT renumber; only append new entries before HookCount.
enum class HookId : int {
    None                = 0,
    PacketSendTap       = 1,
    EngineDetour        = 2,
    GameThreadDetour    = 3,
    UIMessageDetour     = 4,
    SendDialogDetour    = 5,
    SendSignpostDetour  = 6,
    RequestQuoteDetour  = 7,
    TraderResponseDetour= 8,
    RenderDetour        = 9,
    TargetLogDetour     = 10,
    TradePartnerDetour  = 11,
    WriteWhisperDetour  = 12,
    EncStringDecodeDetour=13,
    StoCDispatcher      = 14,
    HookCount
};

// Thread-local: which hook the current thread is inside right now.
// Zero-cost on x86 - a single TLS index read per access.
__declspec(thread) extern int t_activeHookId;

// Global array: last GetTickCount() at which each hook was entered.
// Volatile DWORDs - no lock needed; CrashDiag reads these only after
// a crash, so stale values are acceptable.
extern volatile DWORD g_lastActiveHookTick[];

// RAII scope - sets the thread-local on construction, clears on
// destruction.  No allocations, no heap, single TLS write per
// enter/exit.
struct HookScope {
    explicit HookScope(HookId id) : m_prev(t_activeHookId) {
        t_activeHookId = static_cast<int>(id);
        g_lastActiveHookTick[static_cast<int>(id)] = GetTickCount();
    }
    ~HookScope() {
        t_activeHookId = m_prev;
    }
    HookScope(const HookScope&) = delete;
    HookScope& operator=(const HookScope&) = delete;
private:
    int m_prev;
};

// Lightweight hook ID set for functions that can't use RAII (e.g. __try).
// Call Enter before the body, Leave after.  Overhead is one TLS write +
// one global store.
static inline void Enter(HookId id) {
    t_activeHookId = static_cast<int>(id);
    g_lastActiveHookTick[static_cast<int>(id)] = GetTickCount();
}
static inline void Leave(int prev) {
    t_activeHookId = prev;
}

// Read the current thread's active hook without modifying it.
static inline HookId GetCurrentHook() {
    return static_cast<HookId>(t_activeHookId);
}

// CrashDiag helper: log the active hook for the crashing thread
// plus the g_lastActiveHookTick snapshot.
void DumpOnCrash();

// Return a human-readable name for a HookId.
const char* HookIdToString(HookId id);

// Self-test: verifies HookScope, Enter/Leave, and DumpOnCrash produce
// expected output. Returns true on success. Safe to call at any time.
bool SelfTest();

} // namespace GWA3::HookMarker

// === C-linkage globals for naked asm detour access ===
// MSVC inline asm cannot reference C++ namespaced variables.
// These extern "C" globals provide a bridge: the naked asm detours
// write to these tick variables on entry, and the HookMarker::DumpOnCrash
// reads them after a crash.
extern "C" volatile DWORD g_HookTick_EngineDetour;
extern "C" volatile DWORD g_HookTick_RenderDetour;
extern "C" volatile DWORD g_HookTick_TargetLogDetour;
extern "C" volatile DWORD g_HookTick_TradePartnerDetour;
extern "C" volatile DWORD g_HookTick_GameThreadDetour;
// C-linkage pointer to g_lastActiveHookTick array for naked asm access
// The array is indexed by HookId. EngineDetour=2, so index 2 = offset 8 bytes.
extern "C" volatile DWORD* g_pLastActiveHookTickArray;