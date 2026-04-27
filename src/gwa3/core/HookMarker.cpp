#include <gwa3/core/HookMarker.h>
#include <gwa3/core/Log.h>

#include <Windows.h>

namespace GWA3::HookMarker {

// Thread-local: zero = no hook active.  Each thread gets its own copy.
__declspec(thread) int t_activeHookId = 0;

// Global tick array - one slot per HookId.  Written on every hook entry.
volatile DWORD g_lastActiveHookTick[static_cast<int>(HookId::HookCount)] = {};

const char* HookIdToString(HookId id) {
    switch (id) {
        case HookId::None:                return "None";
        case HookId::PacketSendTap:       return "PacketSendTap";
        case HookId::EngineDetour:        return "EngineDetour";
        case HookId::GameThreadDetour:    return "GameThreadDetour";
        case HookId::UIMessageDetour:     return "UIMessageDetour";
        case HookId::SendDialogDetour:    return "SendDialogDetour";
        case HookId::SendSignpostDetour:  return "SendSignpostDetour";
        case HookId::RequestQuoteDetour:  return "RequestQuoteDetour";
        case HookId::TraderResponseDetour:return "TraderResponseDetour";
        case HookId::RenderDetour:        return "RenderDetour";
        case HookId::TargetLogDetour:     return "TargetLogDetour";
        case HookId::TradePartnerDetour:  return "TradePartnerDetour";
        case HookId::WriteWhisperDetour:  return "WriteWhisperDetour";
        case HookId::EncStringDecodeDetour: return "EncStringDecodeDetour";
        case HookId::StoCDispatcher:      return "StoCDispatcher";
        default:                          return "Unknown";
    }
}

void DumpOnCrash() {
    // NOTE: This function is called from a VectoredExceptionHandler.
    // __declspec(thread) TLS reads can crash if the crashing thread's
    // TEB/TLS slots are corrupted.  We read TLS inside __try and fall
    // back to the global tick arrays (which are always safe to read
    // from any context) when TLS is unavailable.

    // 1. Try to read the crashing thread's active hook from TLS.
    //    If TLS is corrupted, this will AV and we'll catch it.
    int activeHookId = 0;
    __try {
        activeHookId = t_activeHookId;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        // TLS read crashed â€” fall through to global-only analysis
        Log::Error("CrashDiag: active hook TLS read failed (corrupted TLS), using global tick array only");
    }

    if (activeHookId != 0) {
        Log::Error("CrashDiag: active hook=%s(%d) tid=%u",
                   HookIdToString(static_cast<HookId>(activeHookId)),
                   activeHookId,
                   GetCurrentThreadId());
    } else {
        Log::Error("CrashDiag: active hook=None(0) tid=%u (no hook active on this thread, or TLS unavailable)",
                   GetCurrentThreadId());
    }

    // 2. Snapshot the global tick array â€” these are plain volatile DWORDs,
    //    always safe to read from any context.
    const DWORD now = GetTickCount();
    DWORD snap[static_cast<int>(HookId::HookCount)] = {};
    for (int i = 0; i < static_cast<int>(HookId::HookCount); ++i) {
        snap[i] = g_lastActiveHookTick[i];
    }

    // 3. Also snapshot the C-linkage asm globals
    DWORD asmEngineTick = g_HookTick_EngineDetour;
    DWORD asmRenderTick = g_HookTick_RenderDetour;
    DWORD asmTargetLogTick = g_HookTick_TargetLogDetour;
    DWORD asmTradePartnerTick = g_HookTick_TradePartnerDetour;
    DWORD asmGameThreadTick = g_HookTick_GameThreadDetour;

    Log::Error("CrashDiag: hook tick snapshot (now=%u):", now);
    for (int i = 1; i < static_cast<int>(HookId::HookCount); ++i) {
        if (snap[i] == 0) continue;
        const DWORD age = now - snap[i];
        Log::Error("CrashDiag:   %s(%d) last_enter=%u age_ms=%u",
                   HookIdToString(static_cast<HookId>(i)), i,
                   snap[i], age);
    }

    // Log C-linkage asm hook ticks (these are the most critical for
    // the naked detours that can't write the C++ array directly)
    if (asmEngineTick > 0) {
        Log::Error("CrashDiag:   EngineDetour(asm) last_enter=%u age_ms=%u",
                   asmEngineTick, now - asmEngineTick);
    }
    if (asmRenderTick > 0) {
        Log::Error("CrashDiag:   RenderDetour(asm) last_enter=%u age_ms=%u",
                   asmRenderTick, now - asmRenderTick);
    }
    if (asmTargetLogTick > 0) {
        Log::Error("CrashDiag:   TargetLogDetour(asm) last_enter=%u age_ms=%u",
                   asmTargetLogTick, now - asmTargetLogTick);
    }
    if (asmTradePartnerTick > 0) {
        Log::Error("CrashDiag:   TradePartnerDetour(asm) last_enter=%u age_ms=%u",
                   asmTradePartnerTick, now - asmTradePartnerTick);
    }
    if (asmGameThreadTick > 0) {
        Log::Error("CrashDiag:   GameThreadDetour(asm) last_enter=%u age_ms=%u",
                   asmGameThreadTick, now - asmGameThreadTick);
    }

    // 4. Most-recently-entered hook (the one most likely to be the crash site)
    int mostRecent = 0;
    DWORD mostRecentAge = 0xFFFFFFFF;
    for (int i = 1; i < static_cast<int>(HookId::HookCount); ++i) {
        if (snap[i] == 0) continue;
        const DWORD age = now - snap[i];
        if (age < mostRecentAge) {
            mostRecentAge = age;
            mostRecent = i;
        }
    }
    // Also check asm globals
    struct { int id; DWORD tick; } asmTicks[] = {
        { static_cast<int>(HookId::EngineDetour), asmEngineTick },
        { static_cast<int>(HookId::RenderDetour), asmRenderTick },
        { static_cast<int>(HookId::TargetLogDetour), asmTargetLogTick },
        { static_cast<int>(HookId::TradePartnerDetour), asmTradePartnerTick },
        { static_cast<int>(HookId::GameThreadDetour), asmGameThreadTick },
    };
    for (auto& at : asmTicks) {
        if (at.tick == 0) continue;
        const DWORD age = now - at.tick;
        if (age < mostRecentAge) {
            mostRecentAge = age;
            mostRecent = at.id;
        }
    }
    if (mostRecent > 0) {
        Log::Error("CrashDiag: most recently entered hook=%s(%d) age_ms=%u",
                   HookIdToString(static_cast<HookId>(mostRecent)),
                   mostRecent, mostRecentAge);
    }
}

bool SelfTest() {
    Log::Info("HookMarker: SelfTest starting");

    // Test 1: HookScope RAII
    {
        const int prev = t_activeHookId;
        {
            HookScope scope1(HookId::PacketSendTap);
            if (t_activeHookId != static_cast<int>(HookId::PacketSendTap)) {
                Log::Error("HookMarker: SelfTest FAILED - HookScope didn't set t_activeHookId");
                return false;
            }
            if (g_lastActiveHookTick[static_cast<int>(HookId::PacketSendTap)] == 0) {
                Log::Error("HookMarker: SelfTest FAILED - g_lastActiveHookTick not written");
                return false;
            }
        }
        if (t_activeHookId != prev) {
            Log::Error("HookMarker: SelfTest FAILED - HookScope didn't restore t_activeHookId");
            return false;
        }
    }

    // Test 2: Manual Enter/Leave
    {
        const int prev = t_activeHookId;
        Enter(HookId::StoCDispatcher);
        if (t_activeHookId != static_cast<int>(HookId::StoCDispatcher)) {
            Log::Error("HookMarker: SelfTest FAILED - Enter didn't set t_activeHookId");
            return false;
        }
        if (g_lastActiveHookTick[static_cast<int>(HookId::StoCDispatcher)] == 0) {
            Log::Error("HookMarker: SelfTest FAILED - Enter didn't write tick");
            return false;
        }
        Leave(prev);
        if (t_activeHookId != prev) {
            Log::Error("HookMarker: SelfTest FAILED - Leave didn't restore t_activeHookId");
            return false;
        }
    }

    // Test 3: C-linkage globals
    {
        g_HookTick_EngineDetour = GetTickCount();
        g_HookTick_RenderDetour = GetTickCount();
        g_HookTick_TargetLogDetour = GetTickCount();
        g_HookTick_TradePartnerDetour = GetTickCount();
        g_HookTick_GameThreadDetour = GetTickCount();

        if (g_HookTick_EngineDetour == 0) {
            Log::Error("HookMarker: SelfTest FAILED - C-linkage global not writable");
            return false;
        }
    }

    // Test 4: DumpOnCrash runs without crashing
    {
        Enter(HookId::EngineDetour);
        DumpOnCrash();
        Leave(0);
    }

    Log::Info("HookMarker: SelfTest PASSED");
    return true;
}

} // namespace GWA3::HookMarker

// === C-linkage globals for naked asm detour access ===
extern "C" volatile DWORD g_HookTick_EngineDetour = 0;
extern "C" volatile DWORD g_HookTick_RenderDetour = 0;
extern "C" volatile DWORD g_HookTick_TargetLogDetour = 0;
extern "C" volatile DWORD g_HookTick_TradePartnerDetour = 0;
extern "C" volatile DWORD g_HookTick_GameThreadDetour = 0;

// C-linkage pointer for naked asm access to g_lastActiveHookTick array
extern "C" volatile DWORD* g_pLastActiveHookTickArray = GWA3::HookMarker::g_lastActiveHookTick;