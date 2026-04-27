#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/HookMarker.h>

#include <Windows.h>
#include <MinHook.h>
#include <cstddef>
#include <cstdarg>
#include <cstring>

namespace GWA3::CtoS {

// Game's PacketSend calling convention:
//   void __cdecl PacketSend(void* packetLocationPtr, uint32_t size, uint32_t* data)
using PacketSendFn = void(__cdecl*)(void*, uint32_t, uint32_t*);

static PacketSendFn s_packetSendFn = nullptr;
static PacketSendFn s_packetSendOriginal = nullptr;  // trampoline for packet tap
static uintptr_t s_packetSendRawAsmTarget = 0;
static bool s_packetTapEnabled = false;
static bool s_packetTapHookInstalled = false;
static uintptr_t s_packetLocation = 0;
static uintptr_t s_packetLocationPtrAddr = 0;
static bool s_initialized = false;
static volatile LONG s_packetTapTotal = 0;
static volatile LONG s_packetTapCounts[0x200] = {};

// Packet tap: intercept ALL outgoing CtoS packets (including game UI actions)
static void __cdecl PacketSendTap(void* loc, uint32_t sizeBytes, uint32_t* data) {
    HookMarker::HookScope _hs(HookMarker::HookId::PacketSendTap);
    if (s_packetTapEnabled && data && sizeBytes >= 4) {
        const uint32_t hdr = data[0];
        InterlockedIncrement(&s_packetTapTotal);
        if (hdr < _countof(s_packetTapCounts)) {
            InterlockedIncrement(&s_packetTapCounts[hdr]);
        }
    }
    if (s_packetSendOriginal) {
        s_packetSendOriginal(loc, sizeBytes, data);
    }
}

static void IssuePacketSend(const uint32_t* data, uint32_t sizeBytes);
static void IssuePacketSendRaw(const uint32_t* data, uint32_t sizeBytes);
static uintptr_t ResolvePacketLocation();

// ===== Engine inline hook for CtoS dispatch =====
// Hooks at Offsets::Engine (0x00C93C91) -- a DIFFERENT function from the
// Render/FrApi function (0x00AE3D10) that GameThread hooks.
// This avoids the lock-ordering deadlock that occurs when PacketSend is
// called from within the Render function.
// Architecture is currently the sender-thread/game-thread dispatch path used by gwa3.

static constexpr uint32_t kQueueSize = 256;
static uintptr_t s_queue[kQueueSize] = {};
static volatile LONG s_queueCounter = 0;
static volatile LONG s_savedCommand = 0;
static volatile LONG s_savedESP = 0;
static volatile LONG s_heartbeat = 0;

static uintptr_t s_engineHookAddr = 0;
static uintptr_t s_engineReturnAddr = 0;
static uint8_t s_engineSavedBytes[8] = {};
static uint8_t s_enginePatchedBytes[8] = {};
static bool s_engineInitialized = false;
static volatile bool s_engineSuspended = false;
static volatile DWORD s_watchdogLastRepairTick = 0;
static volatile DWORD s_watchdogLastRepairLogTick = 0;
static volatile DWORD s_watchdogLastUnexpectedLogTick = 0;
static volatile LONG s_watchdogRepairCount = 0;

// Watchdog: re-patches the Engine hook when game integrity checker restores bytes
static HANDLE s_watchdogThread = nullptr;
static volatile bool s_watchdogRunning = false;

static uintptr_t s_engineReplayTrampoline = 0;

// Naked detour at Engine hook site.
// Minimal engine detour used by gwa3's current packet transport.
// It wakes or drains the queued sender-thread work without the old experimental
// AutoIt-style command lane.
// PacketSend cannot be called from within any frame hook (deadlocks).
// The sender thread runs BETWEEN frames, outside the frame lock.
static HANDLE s_packetReadyEvent = nullptr;

// Packet task ring buffer for sender thread dispatch
struct PacketTask {
    PacketSendFn fn;
    uintptr_t location;
    uint32_t sizeBytes;
    uint32_t data[12];
};
static PacketTask s_packetRing[64];
static volatile LONG s_pktHead = 0;
static volatile LONG s_pktTail = 0;
static volatile LONG s_pktWriteLock = 0;

static DWORD WINAPI PacketSenderThread(LPVOID) {
    while (s_watchdogRunning) { // reuse watchdog flag for lifetime
        WaitForSingleObject(s_packetReadyEvent, 200);
        while (s_pktTail != s_pktHead) {
            LONG idx = s_pktTail % 64;
            PacketTask t = s_packetRing[idx];
            InterlockedIncrement(&s_pktTail);

            uintptr_t loc = t.location;
            const uintptr_t fresh = ResolvePacketLocation();
            if (fresh) loc = fresh;
            __try {
                t.fn(reinterpret_cast<void*>(loc), t.sizeBytes, t.data);
            } __except(
                Log::Error("CtoS: EXCEPTION 0x%08X at 0x%08X calling PacketSend hdr=0x%X",
                    GetExceptionCode(),
                    reinterpret_cast<uintptr_t>(GetExceptionInformation()->ExceptionRecord->ExceptionAddress),
                    t.data[0]),
                EXCEPTION_EXECUTE_HANDLER
            ) {
                Log::Error("CtoS: PacketSend crashed -- continuing");
            }
            Sleep(10);
        }
    }
    return 0;
}

// Packet transport instrumentation.
// s_engineCallTest tracks engine-thread dispatch observations for debugging.

// Pointers used by the dispatch path ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â set during Initialize from Offsets
static volatile LONG s_engineCallTest = 0;
static void (__stdcall* s_engineDispatchOnePtr)() = nullptr;
static int (__stdcall* s_shouldDeferBotshubCommandsPtr)() = nullptr;
static bool s_disableBotshubDeferForIdentSalvage = false;

// Cached offset pointers for the inline asm environment gate (avoids C++
// function calls on the engine hook hot path ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â C++ calls corrupt EBP and
// the game's frame context).
static uintptr_t s_cachedBasePointer = 0;     // = Offsets::BasePointer
static uintptr_t s_cachedEnvironment = 0;     // = Offsets::Environment

// Diagnostic counters for HandleCase vs RegularFlow frequency
static volatile LONG s_deferCount = 0;      // HandleCase ticks (command deferred)
static volatile LONG s_deferWithCmd = 0;     // HandleCase ticks where a command was actually waiting
static volatile LONG s_regularFlowExec = 0;  // RegularFlow ticks where a command was executed

// ===== Game Command Queue =====
// Secondary ring buffer for game function calls that must execute in the
// Engine hook context (same hook point as AutoIt's command queue).
// Used for operations like Salvage that need the game's internal context.
typedef void (*GameCommandFn)(void* params);
struct GameCommand {
    GameCommandFn fn;
    uint8_t params[248]; // 256 - 8 bytes for fn + padding
};
static constexpr LONG kGameCommandQueueSize = 64;
static GameCommand s_cmdRing[kGameCommandQueueSize];
static volatile LONG s_cmdHead = 0;
static volatile LONG s_cmdTail = 0;
static volatile LONG s_cmdWriteLock = 0;

struct BotshubCommandSlot {
    uint8_t bytes[256];
};
static constexpr LONG kBotshubCommandQueueSize = 64;
static BotshubCommandSlot s_botshubCmdRing[kBotshubCommandQueueSize];
static volatile LONG s_botshubCmdHead = 0;
static volatile LONG s_botshubCmdTail = 0;
static volatile LONG s_botshubSavedIndex = 0;
static volatile LONG s_botshubCmdWriteLock = 0;

static void __stdcall EngineDispatchCommand() {
    if (s_cmdTail == s_cmdHead) return;
    LONG idx = s_cmdTail % kGameCommandQueueSize;
    GameCommand cmd = s_cmdRing[idx];
    InterlockedIncrement(&s_cmdTail);
    if (cmd.fn) {
        cmd.fn(cmd.params);
    }
}

static void __stdcall EngineDispatchOne() {
    if (s_pktTail == s_pktHead) return;

    LONG idx = s_pktTail % 64;
    PacketTask t = s_packetRing[idx];
    InterlockedIncrement(&s_pktTail);

    uintptr_t loc = t.location;
    const uintptr_t fresh = ResolvePacketLocation();
    if (fresh) loc = fresh;

    InterlockedIncrement(&s_engineCallTest);
    t.fn(reinterpret_cast<void*>(loc), t.sizeBytes, t.data);
}

static void (__stdcall* s_engineDispatchCmdPtr)() = nullptr;
static int __stdcall ShouldDeferBotshubCommands() {
    if (s_disableBotshubDeferForIdentSalvage) return 0;
    if (!Offsets::BasePointer || !Offsets::Environment) return 0;

    __try {
        uintptr_t world = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (world <= 0x10000) return 0;

        world = *reinterpret_cast<uintptr_t*>(world);
        if (world <= 0x10000) return 0;

        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(world + 0x18);
        if (p1 <= 0x10000) return 0;

        uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x44);
        if (p2 <= 0x10000) return 0;

        const uint32_t state_guard = *reinterpret_cast<uint32_t*>(p2 + 0x19C);
        if (state_guard == 0u) return 0;

        const uint32_t env_index = *reinterpret_cast<uint32_t*>(p2 + 0x198);
        if (env_index == 0u) return 1;

        // Offsets::Environment is the CODE address of the ADD EAX,imm32 operand.
        // We must dereference to get the actual environment array base pointer
        // (same as upstream's `add ebx, dword[Environment]`).
        const uintptr_t env_base = *reinterpret_cast<uintptr_t*>(Offsets::Environment);
        if (env_base <= 0x10000) return 0;

        const uintptr_t env_entry = env_base + static_cast<uintptr_t>(env_index) * 0x7Cu;
        if (env_entry <= 0x10000) return 0;

        const uint32_t flags = *reinterpret_cast<uint32_t*>(env_entry + 0x10);
        return (flags & 0x40001u) ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// 108-byte buffer for fsave/frstor (x87 FPU state, 28 dwords).
// Statically allocated ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â the engine detour is single-threaded per process.
static __declspec(align(16)) uint8_t s_fpuSaveArea[108];

namespace {

struct TradeOfferItemBotshubCommand {
    uintptr_t fn;
    uint32_t item_id;
    uint32_t quantity;
};

struct PacketSendBotshubCommand {
    uintptr_t fn;
    uint32_t size_bytes;
    uint32_t data[12];
};

struct PacketSendGameCommand {
    uint32_t size_bytes;
    uint32_t data[12];
};

struct SalvageBotshubCommand {
    uintptr_t fn;
    uint32_t item_id;
    uint32_t kit_id;
    uint32_t session_id;
};

static uintptr_t s_salvageFunctionPtr = 0;
static uintptr_t s_salvageGlobalPtr = 0;

void __cdecl TradeOfferItemCommandThunk(uint32_t itemId, uint32_t quantity) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: TradeOfferItemCommandThunk dropped item=%u qty=%u -- not initialized", itemId, quantity);
        return;
    }
    const uint32_t data[3] = { Packets::TRADE_ADD_ITEM, itemId, quantity };
    IssuePacketSend(data, sizeof(data));
}

void __cdecl PacketSendBotshubCommandThunk(const uint32_t* data, uint32_t sizeBytes) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: PacketSendBotshubCommandThunk dropped size=%u -- not initialized", sizeBytes);
        return;
    }
    if (!data || sizeBytes == 0u || sizeBytes > sizeof(PacketSendBotshubCommand::data)) {
        Log::Warn("CtoS: PacketSendBotshubCommandThunk rejected size=%u", sizeBytes);
        return;
    }
    Log::Info("CtoS: PacketSendBotshubCommandThunk begin hdr=0x%X size=%u", data[0], sizeBytes);
    IssuePacketSendRaw(data, sizeBytes);
    Log::Info("CtoS: PacketSendBotshubCommandThunk return hdr=0x%X", data[0]);
}

void __cdecl PacketSendBotshubHookedCommandThunk(const uint32_t* data, uint32_t sizeBytes) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: PacketSendBotshubHookedCommandThunk dropped size=%u -- not initialized", sizeBytes);
        return;
    }
    if (!data || sizeBytes == 0u || sizeBytes > sizeof(PacketSendBotshubCommand::data)) {
        Log::Warn("CtoS: PacketSendBotshubHookedCommandThunk rejected size=%u", sizeBytes);
        return;
    }
    Log::Info("CtoS: PacketSendBotshubHookedCommandThunk begin hdr=0x%X size=%u", data[0], sizeBytes);
    IssuePacketSend(data, sizeBytes);
    Log::Info("CtoS: PacketSendBotshubHookedCommandThunk return hdr=0x%X", data[0]);
}

void RawPacketGameCommandInvoker(void* params) {
    auto* cmd = reinterpret_cast<PacketSendGameCommand*>(params);
    if (!cmd || cmd->size_bytes == 0u || cmd->size_bytes > sizeof(cmd->data)) {
        Log::Warn("CtoS: RawPacketGameCommandInvoker rejected size=%u", cmd ? cmd->size_bytes : 0u);
        return;
    }
    Log::Info("CtoS: RawPacketGameCommandInvoker begin hdr=0x%X size=%u", cmd->data[0], cmd->size_bytes);
    IssuePacketSendRaw(cmd->data, cmd->size_bytes);
    Log::Info("CtoS: RawPacketGameCommandInvoker return hdr=0x%X", cmd->data[0]);
}

__declspec(naked) void BotshubTradeOfferItemCommandStub() {
    __asm {
        push dword ptr [eax+8]
        push dword ptr [eax+4]
        call TradeOfferItemCommandThunk
        add esp, 8
        jmp GWA3BotshubCommandReturnThunk
    }
}

__declspec(naked) void BotshubPacketSendCommandStub() {
    __asm {
        lea edx, dword ptr [eax+8]
        push edx
        mov ebx, dword ptr [eax+4]
        push ebx
        mov eax, dword ptr [s_packetLocationPtrAddr]
        test eax, eax
        jnz packet_location_ready
        mov eax, dword ptr [s_packetLocation]
    packet_location_ready:
        push eax
        call dword ptr [s_packetSendRawAsmTarget]
        pop eax
        pop ebx
        pop edx
        jmp GWA3BotshubCommandReturnThunk
    }
}

__declspec(naked) void BotshubPacketSendHookedCommandStub() {
    __asm {
        lea edx, dword ptr [eax+8]
        push edx
        mov ebx, dword ptr [eax+4]
        push ebx
        mov eax, dword ptr [s_packetLocationPtrAddr]
        test eax, eax
        jnz packet_location_ready
        mov eax, dword ptr [s_packetLocation]
    packet_location_ready:
        push eax
        call dword ptr [s_packetSendFn]
        pop eax
        pop ebx
        pop edx
        jmp GWA3BotshubCommandReturnThunk
    }
}

__declspec(naked) void BotshubSalvageCommandStub() {
    __asm {
        push eax
        push ecx
        push ebx
        mov ebx, dword ptr [s_salvageGlobalPtr]
        mov ecx, dword ptr [eax+4]
        mov dword ptr [ebx], ecx
        add ebx, 4
        mov ecx, dword ptr [eax+8]
        mov dword ptr [ebx], ecx
        mov ebx, dword ptr [eax+4]
        push ebx
        mov ebx, dword ptr [eax+8]
        push ebx
        mov ebx, dword ptr [eax+0Ch]
        push ebx
        call dword ptr [s_salvageFunctionPtr]
        add esp, 0Ch
        pop ebx
        pop ecx
        pop eax
        jmp GWA3BotshubCommandReturnThunk
    }
}

} // namespace

extern "C" void __declspec(naked) GWA3BotshubCommandReturnThunk() {
    __asm {
        mov ecx, dword ptr [s_botshubSavedIndex]
        mov edx, dword ptr [s_botshubCmdTail]
        cmp edx, ecx
        jne skip_tail_advance
        mov eax, ecx
        inc eax
        mov dword ptr [s_botshubCmdTail], eax
    skip_tail_advance:
        // Restore FPU state saved at EngineDetourNaked entry.
        // Botshub commands bypass the normal exit path, so without this
        // the fld below operates on a corrupted FPU stack.
        frstor [s_fpuSaveArea]
        popfd
        popad
        // Hardcoded exit: replay original bytes inline, matching upstream
        // MainProc.  No trampoline indirection.
        mov ebp, esp
        fld dword ptr [ebp + 8]
        jmp [s_engineReturnAddr]
    }
}

static __declspec(naked) void EngineDetourNaked() {
    __asm {
        pushad
        pushfd

        // Mark hook active for CrashDiag (EngineDetour=2)
        call dword ptr [GetTickCount]
        mov dword ptr [g_HookTick_EngineDetour], eax
        mov dword ptr [g_pLastActiveHookTickArray+8], eax  // HookId::EngineDetour=2, sizeof(DWORD)*2=8

        // Save x87 FPU state BEFORE dispatching any commands.
        // Commands (Move, ChangeTarget, UseSkill) use floating point and
        // corrupt the FPU stack. The exit replays `fld dword ptr [ebp+8]`
        // which pushes onto the FPU stack ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â corruption here crashes GW
        // after ~10 minutes of active command dispatch.
        fsave [s_fpuSaveArea]

        inc dword ptr [s_heartbeat]

        call dword ptr [s_shouldDeferBotshubCommandsPtr]
        test eax, eax
        jz regular_flow

        // HandleCase: defer queued botshub commands until the engine state
        // matches the upstream RegularFlow path again.
        inc dword ptr [s_deferCount]
        mov ecx, dword ptr [s_botshubCmdTail]
        cmp ecx, dword ptr [s_botshubCmdHead]
        je no_botshub_command
        inc dword ptr [s_deferWithCmd]
        jmp no_botshub_command

    regular_flow:
        // === Botshub command dispatch ===
        mov ecx, dword ptr [s_botshubCmdTail]
        cmp ecx, dword ptr [s_botshubCmdHead]
        je no_botshub_command
        mov eax, ecx
        and eax, 63
        shl eax, 8
        mov edx, offset s_botshubCmdRing
        add eax, edx
        mov edx, dword ptr [eax]
        test edx, edx
        jz no_botshub_command
        inc dword ptr [s_regularFlowExec]
        mov dword ptr [s_botshubSavedIndex], ecx
        mov dword ptr [eax], 0
        jmp edx
    no_botshub_command:

        // Game command dispatch (rare ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â salvage etc.)
        mov eax, dword ptr [s_cmdTail]
        cmp eax, dword ptr [s_cmdHead]
        je no_command
        call dword ptr [s_engineDispatchCmdPtr]
    no_command:

        // Restore x87 FPU state to what it was before command dispatch.
        frstor [s_fpuSaveArea]

        popfd
        popad
        // Hardcoded exit: replay original bytes inline, matching upstream
        // MainProc.  No trampoline indirection.
        mov ebp, esp
        fld dword ptr [ebp + 8]
        jmp [s_engineReturnAddr]
    }
}

static DWORD WINAPI EngineWatchdog(LPVOID) {
    while (s_watchdogRunning) {
        Sleep(5);
        if (!s_engineInitialized || !s_engineHookAddr) continue;
        if (s_engineSuspended) continue;  // Don't re-patch while suspended

        const uint8_t* cur = reinterpret_cast<const uint8_t*>(s_engineHookAddr);
        if (memcmp(cur, s_enginePatchedBytes, 5) == 0) continue;

        const DWORD now = GetTickCount();
        const bool matchesOriginal = memcmp(cur, s_engineSavedBytes, 5) == 0;
        if (!matchesOriginal) {
            if (now - s_watchdogLastUnexpectedLogTick >= 1000) {
                s_watchdogLastUnexpectedLogTick = now;
                Log::Warn("CtoS: [WATCHDOG] Engine hook mismatch ignored (bytes=%02X %02X %02X %02X %02X)",
                          cur[0], cur[1], cur[2], cur[3], cur[4]);
            }
            continue;
        }

        // Only repair known-safe "original bytes restored" cases and throttle
        // writes so the watchdog does not hammer the hook site under load.
        if (now - s_watchdogLastRepairTick < 100) continue;

        DWORD oldProtect;
        if (VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                           PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(reinterpret_cast<void*>(s_engineHookAddr), s_enginePatchedBytes, 5);
            FlushInstructionCache(GetCurrentProcess(),
                                 reinterpret_cast<void*>(s_engineHookAddr), 5);
            VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                           oldProtect, &oldProtect);
            s_watchdogLastRepairTick = now;
            const LONG repairCount = InterlockedIncrement(&s_watchdogRepairCount);
            if (repairCount == 1 || now - s_watchdogLastRepairLogTick >= 1000) {
                s_watchdogLastRepairLogTick = now;
                Log::Info("CtoS: [WATCHDOG] Engine hook re-patched count=%ld", repairCount);
            }
        }
    }
    return 0;
}

static bool InstallEngineHook() {
    s_engineHookAddr = Offsets::Engine;
    if (!s_engineHookAddr) {
        Log::Error("CtoS: Offsets::Engine not resolved");
        return false;
    }

    s_engineReturnAddr = s_engineHookAddr + 5;

    // Save original 5 bytes
    memcpy(s_engineSavedBytes, reinterpret_cast<void*>(s_engineHookAddr), 5);
    Log::Info("CtoS: Engine at 0x%08X, original bytes: %02X %02X %02X %02X %02X",
              s_engineHookAddr,
              s_engineSavedBytes[0], s_engineSavedBytes[1], s_engineSavedBytes[2],
              s_engineSavedBytes[3], s_engineSavedBytes[4]);

    // Build replay trampoline: original 5 bytes + JMP to returnAddr
    void* tramp = VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) {
        Log::Error("CtoS: VirtualAlloc for Engine trampoline failed");
        return false;
    }
    uint8_t* t = reinterpret_cast<uint8_t*>(tramp);
    memcpy(t, s_engineSavedBytes, 5);
    t[5] = 0xE9; // JMP rel32
    int32_t jmpRel = static_cast<int32_t>(s_engineReturnAddr - reinterpret_cast<uintptr_t>(t + 5 + 4));
    memcpy(t + 6, &jmpRel, 4);
    FlushInstructionCache(GetCurrentProcess(), t, 10);
    s_engineReplayTrampoline = reinterpret_cast<uintptr_t>(tramp);
    s_engineDispatchOnePtr = &EngineDispatchOne;
    s_engineDispatchCmdPtr = &EngineDispatchCommand;
    s_shouldDeferBotshubCommandsPtr = &ShouldDeferBotshubCommands;
    Log::Info("CtoS: identify/salvage defer override=%u", s_disableBotshubDeferForIdentSalvage ? 1u : 0u);

    // Cache offset pointers for the inline asm environment gate.
    // BasePointer is already dereferenced in PostProcessOffsets.
    // Environment is a code address ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â read the embedded immediate to get the actual base.
    s_cachedBasePointer = Offsets::BasePointer;
    if (Offsets::Environment > 0x10000) {
        s_cachedEnvironment = *reinterpret_cast<uintptr_t*>(Offsets::Environment);
    }
    Log::Info("CtoS: env gate cached BasePointer=0x%08X Environment=0x%08X",
              s_cachedBasePointer, s_cachedEnvironment);

    // Write 5-byte JMP to our detour
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t patch[5];
    patch[0] = 0xE9;
    int32_t hookRel = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(&EngineDetourNaked) - (s_engineHookAddr + 5));
    memcpy(patch + 1, &hookRel, 4);
    memcpy(reinterpret_cast<void*>(s_engineHookAddr), patch, 5);
    memcpy(s_enginePatchedBytes, patch, 5);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(s_engineHookAddr), 5);

    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   oldProtect, &oldProtect);

    // Start watchdog
    s_watchdogRunning = true;
    s_watchdogThread = CreateThread(nullptr, 0, EngineWatchdog, nullptr, 0, nullptr);

    // Start packet sender thread + event
    s_packetReadyEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    CreateThread(nullptr, 0, PacketSenderThread, nullptr, 0, nullptr);

    s_engineInitialized = true;
    Log::Info("CtoS: Engine hook installed at 0x%08X, sender thread started",
              s_engineHookAddr);
    return true;
}


bool EnqueueGameCommand(GameCommandFn fn, const void* params, size_t paramSize) {
    if (!s_engineInitialized || !fn) return false;
    if (paramSize > sizeof(GameCommand::params)) {
        Log::Warn("CtoS: EnqueueGameCommand param too large (%u > %u)",
                  (uint32_t)paramSize, (uint32_t)sizeof(GameCommand::params));
        return false;
    }

    while (InterlockedCompareExchange(&s_cmdWriteLock, 1, 0) != 0) {
        Sleep(0);
    }

    const LONG head = s_cmdHead;
    const LONG tail = s_cmdTail;
    if (head - tail >= kGameCommandQueueSize) {
        InterlockedExchange(&s_cmdWriteLock, 0);
        Log::Warn("CtoS: EnqueueGameCommand queue full (head=%ld tail=%ld size=%ld)",
                  head, tail, kGameCommandQueueSize);
        return false;
    }

    GameCommand& cmd = s_cmdRing[head % kGameCommandQueueSize];
    ZeroMemory(cmd.params, sizeof(cmd.params));
    if (params && paramSize > 0) {
        memcpy(cmd.params, params, paramSize);
    }
    MemoryBarrier();
    cmd.fn = fn;
    MemoryBarrier();
    InterlockedExchange(&s_cmdHead, head + 1);
    InterlockedExchange(&s_cmdWriteLock, 0);

    Log::Info("CtoS: EnqueueGameCommand fn=0x%08X paramSize=%u idx=%ld",
              reinterpret_cast<uintptr_t>(fn), (uint32_t)paramSize, head);
    return true;
}

bool EnqueueBotshubCommand(const void* slot, size_t slotSize) {
    if (!s_engineInitialized || !slot || slotSize == 0u || slotSize > sizeof(BotshubCommandSlot::bytes)) {
        return false;
    }

    while (InterlockedCompareExchange(&s_botshubCmdWriteLock, 1, 0) != 0) {
        Sleep(0);
    }

    const LONG head = s_botshubCmdHead;
    const LONG tail = s_botshubCmdTail;
    if (head - tail >= kBotshubCommandQueueSize) {
        InterlockedExchange(&s_botshubCmdWriteLock, 0);
        Log::Warn("CtoS: EnqueueBotshubCommand queue full (head=%ld tail=%ld size=%ld)",
                  head, tail, kBotshubCommandQueueSize);
        return false;
    }

    BotshubCommandSlot& cmd = s_botshubCmdRing[head % kBotshubCommandQueueSize];
    ZeroMemory(cmd.bytes, sizeof(cmd.bytes));
    memcpy(cmd.bytes, slot, slotSize);
    MemoryBarrier();
    InterlockedExchange(&s_botshubCmdHead, head + 1);
    InterlockedExchange(&s_botshubCmdWriteLock, 0);

    Log::Info("CtoS: EnqueueBotshubCommand size=%u idx=%ld fn=0x%08X defer=%ld deferCmd=%ld exec=%ld hb=%ld",
              static_cast<uint32_t>(slotSize),
              head,
              static_cast<unsigned>(*reinterpret_cast<const uintptr_t*>(slot)),
              s_deferCount, s_deferWithCmd, s_regularFlowExec, s_heartbeat);
    return true;
}

bool IsBotshubQueueIdle() {
    return s_botshubCmdHead == s_botshubCmdTail;
}

void DumpBotshubQueueState(const char* label) {
    const LONG head = s_botshubCmdHead;
    const LONG tail = s_botshubCmdTail;
    Log::Info("CtoS: Botshub queue state %s head=%ld tail=%ld pending=%ld defer=%ld deferCmd=%ld exec=%ld hb=%ld suspended=%u",
              label ? label : "queue",
              head,
              tail,
              head - tail,
              s_deferCount,
              s_deferWithCmd,
              s_regularFlowExec,
              s_heartbeat,
              s_engineSuspended ? 1u : 0u);

    const LONG pending = head - tail;
    const LONG dumpCount = pending > 3 ? 3 : pending;
    for (LONG i = 0; i < dumpCount; ++i) {
        const LONG idx = tail + i;
        const BotshubCommandSlot& slot = s_botshubCmdRing[idx % kBotshubCommandQueueSize];
        __try {
            const uintptr_t fn = *reinterpret_cast<const uintptr_t*>(slot.bytes + 0);
            const uint32_t a = *reinterpret_cast<const uint32_t*>(slot.bytes + 4);
            const uint32_t b = *reinterpret_cast<const uint32_t*>(slot.bytes + 8);
            const uint32_t c = *reinterpret_cast<const uint32_t*>(slot.bytes + 12);
            const uint32_t d = *reinterpret_cast<const uint32_t*>(slot.bytes + 16);
            Log::Info("CtoS: Botshub queue slot[%ld] fn=0x%08X +4=0x%08X +8=0x%08X +C=0x%08X +10=0x%08X",
                      idx,
                      static_cast<unsigned>(fn),
                      a,
                      b,
                      c,
                      d);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("CtoS: Botshub queue slot[%ld] unreadable", idx);
        }
    }
}

bool IsGameCommandQueueIdle() {
    return s_cmdHead == s_cmdTail;
}

bool IsBotshubCommandLaneAvailable() {
    return s_engineInitialized && !s_engineSuspended;
}

void SuspendEngineHook() {
    if (!s_engineInitialized || !s_engineHookAddr) return;
    s_engineSuspended = true;  // Tell watchdog to stop re-patching
    Sleep(10);  // Let watchdog cycle pass
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(s_engineHookAddr), s_engineSavedBytes, 5);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(s_engineHookAddr), 5);
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   oldProtect, &oldProtect);
    Log::Info("CtoS: Engine hook SUSPENDED (original bytes restored at 0x%08X)", s_engineHookAddr);
}

void ResumeEngineHook() {
    if (!s_engineInitialized || !s_engineHookAddr) return;

    // Re-read current original bytes (may have changed after map transition)
    memcpy(s_engineSavedBytes, reinterpret_cast<void*>(s_engineHookAddr), 5);
    Log::Info("CtoS: ResumeEngineHook re-read original bytes: %02X %02X %02X %02X %02X",
              s_engineSavedBytes[0], s_engineSavedBytes[1], s_engineSavedBytes[2],
              s_engineSavedBytes[3], s_engineSavedBytes[4]);

    // Rebuild trampoline with fresh original bytes
    if (s_engineReplayTrampoline) {
        uint8_t* t = reinterpret_cast<uint8_t*>(s_engineReplayTrampoline);
        memcpy(t, s_engineSavedBytes, 5);
        // JMP back to engineHookAddr + 5 (unchanged)
        t[5] = 0xE9;
        int32_t jmpRel = static_cast<int32_t>(s_engineReturnAddr - reinterpret_cast<uintptr_t>(t + 5 + 4));
        memcpy(t + 6, &jmpRel, 4);
        FlushInstructionCache(GetCurrentProcess(), t, 10);
        Log::Info("CtoS: Trampoline rebuilt with fresh bytes");
    }

    // Re-patch the engine hook
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(reinterpret_cast<void*>(s_engineHookAddr), s_enginePatchedBytes, 5);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(s_engineHookAddr), 5);
    VirtualProtect(reinterpret_cast<void*>(s_engineHookAddr), 5,
                   oldProtect, &oldProtect);
    s_engineSuspended = false;
    Log::Info("CtoS: Engine hook RESUMED at 0x%08X", s_engineHookAddr);
}

bool Initialize() {
    if (s_initialized) return true;

    if (!Offsets::PacketSend || !Offsets::PacketLocation) {
        Log::Error("CtoS: PacketSend or PacketLocation offset not resolved");
        return false;
    }

    s_packetSendFn = reinterpret_cast<PacketSendFn>(Offsets::PacketSend);
    s_packetSendRawAsmTarget = reinterpret_cast<uintptr_t>(s_packetSendFn);
    s_packetLocation = ResolvePacketLocation();
    s_packetLocationPtrAddr = s_packetLocation;
    if (!s_packetLocation) {
        Log::Error("CtoS: PacketLocation resolved null");
        return false;
    }

    s_initialized = true;
    Log::Info("CtoS: Initialized (PacketSend=0x%08X, PacketLocation=0x%08X)",
              Offsets::PacketSend, s_packetLocation);

    if (!s_packetTapHookInstalled) {
        const MH_STATUS createStatus = MH_CreateHook(
            reinterpret_cast<LPVOID>(Offsets::PacketSend),
            reinterpret_cast<LPVOID>(&PacketSendTap),
            reinterpret_cast<LPVOID*>(&s_packetSendOriginal));
        if (createStatus == MH_OK || createStatus == MH_ERROR_ALREADY_CREATED) {
            const MH_STATUS enableStatus =
                MH_EnableHook(reinterpret_cast<LPVOID>(Offsets::PacketSend));
            if (enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED) {
                s_packetTapHookInstalled = true;
                s_packetTapEnabled = true;
                s_packetSendRawAsmTarget = reinterpret_cast<uintptr_t>(s_packetSendOriginal ? s_packetSendOriginal : s_packetSendFn);
                ResetPacketTap();
                Log::Info("CtoS: Packet tap hook enabled at 0x%08X",
                          static_cast<unsigned>(Offsets::PacketSend));
            } else {
                Log::Warn("CtoS: MH_EnableHook failed for packet tap: %s",
                          MH_StatusToString(enableStatus));
            }
        } else {
            Log::Warn("CtoS: MH_CreateHook failed for packet tap: %s",
                      MH_StatusToString(createStatus));
        }
    }

    // Install Engine inline hook for packet dispatch
    if (!InstallEngineHook()) {
        Log::Warn("CtoS: Engine hook failed -- packets will be dropped");
    }

    return true;
}

static uintptr_t ResolvePacketLocation() {
    const uintptr_t ptrAddr = Offsets::PacketLocation;
    if (ptrAddr) {
        const uintptr_t fresh = *reinterpret_cast<uintptr_t*>(ptrAddr);
        if (fresh) {
            s_packetLocation = fresh;
            s_packetLocationPtrAddr = ptrAddr;
        }
    }
    return s_packetLocation;
}

// Issue PacketSend on the current thread (must be game thread).
static void IssuePacketSend(const uint32_t* data, uint32_t sizeBytes) {
    uintptr_t loc = ResolvePacketLocation();
    Log::Info("CtoS: IssuePacketSend hdr=0x%X size=%u loc=0x%08X fn=0x%08X",
              data[0], sizeBytes, static_cast<unsigned>(loc),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_packetSendFn)));
    __try {
        s_packetSendFn(reinterpret_cast<void*>(loc), sizeBytes, const_cast<uint32_t*>(data));
        Log::Info("CtoS: IssuePacketSend returned hdr=0x%X", data[0]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Error("CtoS: PacketSend exception 0x%08X hdr=0x%X",
                   GetExceptionCode(), data[0]);
    }
}

static void IssuePacketSendRaw(const uint32_t* data, uint32_t sizeBytes) {
    uintptr_t loc = ResolvePacketLocation();

    PacketSendFn fn = s_packetSendOriginal ? s_packetSendOriginal : s_packetSendFn;
    Log::Info("CtoS: IssuePacketSendRaw hdr=0x%X size=%u loc=0x%08X fn=0x%08X",
              data[0], sizeBytes, static_cast<unsigned>(loc),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(fn)));
    __try {
        fn(reinterpret_cast<void*>(loc), sizeBytes, const_cast<uint32_t*>(data));
        Log::Info("CtoS: IssuePacketSendRaw returned hdr=0x%X", data[0]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Error("CtoS: Raw PacketSend exception 0x%08X hdr=0x%X",
                   GetExceptionCode(), data[0]);
    }
}

static bool EnqueuePacketTask(const uint32_t* data, uint32_t sizeBytes) {
    if (!s_packetReadyEvent || !s_packetSendFn || !data || sizeBytes == 0 || sizeBytes > sizeof(s_packetRing[0].data)) {
        return false;
    }

    while (InterlockedCompareExchange(&s_pktWriteLock, 1, 0) != 0) {
        Sleep(0);
    }

    const LONG head = s_pktHead;
    const LONG tail = s_pktTail;
    if (head - tail >= 64) {
        InterlockedExchange(&s_pktWriteLock, 0);
        Log::Warn("CtoS: EnqueuePacketTask queue full hdr=0x%X head=%ld tail=%ld",
                  data[0], head, tail);
        return false;
    }

    PacketTask& task = s_packetRing[head % 64];
    ZeroMemory(&task, sizeof(task));
    task.fn = s_packetSendFn;
    task.location = s_packetLocation;
    task.sizeBytes = sizeBytes;
    memcpy(task.data, data, sizeBytes);
    MemoryBarrier();
    InterlockedExchange(&s_pktHead, head + 1);
    InterlockedExchange(&s_pktWriteLock, 0);
    SetEvent(s_packetReadyEvent);
    Log::Info("CtoS: EnqueuePacketTask hdr=0x%X size=%u idx=%ld", data[0], sizeBytes, head);
    return true;
}

// Core send: GWCA pattern ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â¦ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â call PacketSend on game thread only.
// If already on game thread, call directly.  Otherwise copy the packet
// and enqueue via GameThread::Enqueue.  NEVER call from a background
// thread (PacketSend is not thread-safe).
void SendPacket(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacket dropped (header=0x%X) -- not initialized", header);
        return;
    }

    uint32_t data[12];
    data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12; i++) {
        data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    const uint32_t sizeBytes = size * 4;

    if (GameThread::IsOnGameThread()) {
        // Fast path: already on game thread, call directly
        IssuePacketSend(data, sizeBytes);
        return;
    }

    if (GameThread::IsInitialized()) {
        // Off game thread: copy packet data into the lambda capture and
        // enqueue for the game thread to dispatch.
        //
        // Use EnqueueSerialPre, not Enqueue. IssuePacketSend -> PacketSend
        // has internal locks that break when called >1x per frame (the
        // DrainPostQueues comment in GameThread.cpp warns about this and
        // throttles its own post-queue to one-per-frame). The general
        // pre-queue drains EVERYTHING per frame, so under HM combat with
        // 7 heroes all casting (each producing a 0x26 USE_SKILL packet),
        // PacketSend gets called many times per frame and the game
        // crashes after ~minutes. Serial-pre processes one task per
        // frame, matching the lock's expectation.
        struct PktCopy { uint32_t d[12]; uint32_t sz; };
        PktCopy copy{};
        memcpy(copy.d, data, sizeBytes);
        copy.sz = sizeBytes;
        Log::Info("CtoS: SendPacket queueing on GameThread hdr=0x%X size=%u", header, sizeBytes);
        GameThread::EnqueueSerialPre([copy]() {
            IssuePacketSend(copy.d, copy.sz);
        });
        return;
    }

    Log::Warn("CtoS: SendPacket dropped header=0x%X -- GameThread not ready", header);
}

bool SendPacketBotshub(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacketBotshub dropped (header=0x%X) -- not initialized", header);
        return false;
    }
    if (!IsBotshubCommandLaneAvailable() || size == 0u || size > 12u) {
        return false;
    }
    s_packetSendRawAsmTarget = reinterpret_cast<uintptr_t>(s_packetSendOriginal ? s_packetSendOriginal : s_packetSendFn);
    ResolvePacketLocation();
    if (!s_packetSendRawAsmTarget || !s_packetLocation) {
        Log::Warn("CtoS: SendPacketBotshub missing raw send target/location hdr=0x%X fn=0x%08X loc=0x%08X",
                  header,
                  static_cast<unsigned>(s_packetSendRawAsmTarget),
                  static_cast<unsigned>(s_packetLocation));
        return false;
    }

    PacketSendBotshubCommand cmd{};
    cmd.fn = reinterpret_cast<uintptr_t>(&BotshubPacketSendCommandStub);
    cmd.size_bytes = size * 4u;
    cmd.data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12u; ++i) {
        cmd.data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    const size_t slotSize = offsetof(PacketSendBotshubCommand, data) + cmd.size_bytes;
    const bool queued = EnqueueBotshubCommand(&cmd, slotSize);
    Log::Info("CtoS: SendPacketBotshub hdr=0x%X size=%u queued=%d", header, cmd.size_bytes, queued ? 1 : 0);
    return queued;
}

bool SendPacketBotshubHooked(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacketBotshubHooked dropped (header=0x%X) -- not initialized", header);
        return false;
    }
    if (!IsBotshubCommandLaneAvailable() || size == 0u || size > 12u) {
        return false;
    }

    PacketSendBotshubCommand cmd{};
    cmd.fn = reinterpret_cast<uintptr_t>(&BotshubPacketSendHookedCommandStub);
    cmd.size_bytes = size * 4u;
    cmd.data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12u; ++i) {
        cmd.data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    const size_t slotSize = offsetof(PacketSendBotshubCommand, data) + cmd.size_bytes;
    const bool queued = EnqueueBotshubCommand(&cmd, slotSize);
    Log::Info("CtoS: SendPacketBotshubHooked hdr=0x%X size=%u queued=%d", header, cmd.size_bytes, queued ? 1 : 0);
    return queued;
}

bool SendPacketGameCommandRaw(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacketGameCommandRaw dropped (header=0x%X) -- not initialized", header);
        return false;
    }
    if (size == 0u || size > 12u) {
        return false;
    }

    PacketSendGameCommand cmd{};
    cmd.size_bytes = size * 4u;
    cmd.data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12u; ++i) {
        cmd.data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    const bool queued = EnqueueGameCommand(&RawPacketGameCommandInvoker, &cmd, sizeof(cmd));
    Log::Info("CtoS: SendPacketGameCommandRaw hdr=0x%X size=%u queued=%d", header, cmd.size_bytes, queued ? 1 : 0);
    return queued;
}

bool SalvageItemBotshub(uint32_t itemId, uint32_t kitId, uint32_t sessionId) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SalvageItemBotshub dropped item=%u kit=%u session=%u -- not initialized",
                  itemId, kitId, sessionId);
        return false;
    }
    if (!IsBotshubCommandLaneAvailable() || !itemId || !kitId) {
        Log::Warn("CtoS: SalvageItemBotshub rejected item=%u kit=%u session=%u lane=%d",
                  itemId, kitId, sessionId, IsBotshubCommandLaneAvailable() ? 1 : 0);
        return false;
    }
    if (!Offsets::Salvage || !Offsets::SalvageGlobal) {
        Log::Warn("CtoS: SalvageItemBotshub missing offsets salvage=0x%08X global=0x%08X",
                  static_cast<unsigned>(Offsets::Salvage),
                  static_cast<unsigned>(Offsets::SalvageGlobal));
        return false;
    }

    s_salvageFunctionPtr = Offsets::Salvage;
    s_salvageGlobalPtr = Offsets::SalvageGlobal;

    Log::Info("CtoS: SalvageItemBotshub entry item=%u kit=%u session=%u salvageFn=0x%08X",
              itemId,
              kitId,
              sessionId,
              static_cast<unsigned>(s_salvageFunctionPtr));

    SalvageBotshubCommand cmd{};
    cmd.fn = reinterpret_cast<uintptr_t>(&BotshubSalvageCommandStub);
    cmd.item_id = itemId;
    cmd.kit_id = kitId;
    cmd.session_id = sessionId;

    const bool queued = EnqueueBotshubCommand(&cmd, sizeof(cmd));
    Log::Info("CtoS: SalvageItemBotshub item=%u kit=%u session=%u queued=%d salvageFn=0x%08X salvageGlobal=0x%08X",
              itemId, kitId, sessionId, queued ? 1 : 0,
              static_cast<unsigned>(s_salvageFunctionPtr),
              static_cast<unsigned>(s_salvageGlobalPtr));
    return queued;
}

void SendPacketDirect(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacketDirect dropped (header=0x%X) -- not initialized", header);
        return;
    }

    uint32_t data[12];
    data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12; i++) {
        data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    const uint32_t sizeBytes = size * 4;

    // Dispatch via GameThread::EnqueuePost (post-dispatch phase) to ensure
    // we're on the game thread when PacketSend runs, but bypass the engine
    // hook's pre-dispatch queue which has FPU/stack corruption issues.
    // Direct IssuePacketSend from a non-game thread crashes PacketSend.
    if (GameThread::IsOnGameThread()) {
        Log::Info("CtoS: SendPacketDirect hdr=0x%X size=%u (on game thread)", header, sizeBytes);
        IssuePacketSend(data, sizeBytes);
    } else if (GameThread::IsInitialized()) {
        struct PktCopy { uint32_t d[12]; uint32_t sz; };
        PktCopy copy{};
        memcpy(copy.d, data, sizeBytes);
        copy.sz = sizeBytes;
        Log::Info("CtoS: SendPacketDirect hdr=0x%X size=%u (via post-dispatch)", header, sizeBytes);
        GameThread::EnqueuePost([copy]() {
            IssuePacketSend(copy.d, copy.sz);
        });
    } else {
        Log::Warn("CtoS: SendPacketDirect dropped hdr=0x%X -- GameThread not ready", header);
    }
}

void SendPacketDirectRaw(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacketDirectRaw dropped (header=0x%X) -- not initialized", header);
        return;
    }

    uint32_t data[12];
    data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12; i++) {
        data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    const uint32_t sizeBytes = size * 4;

    if (GameThread::IsOnGameThread()) {
        Log::Info("CtoS: SendPacketDirectRaw hdr=0x%X size=%u (on game thread)", header, sizeBytes);
        IssuePacketSendRaw(data, sizeBytes);
    } else if (GameThread::IsInitialized()) {
        struct PktCopy { uint32_t d[12]; uint32_t sz; };
        PktCopy copy{};
        memcpy(copy.d, data, sizeBytes);
        copy.sz = sizeBytes;
        Log::Info("CtoS: SendPacketDirectRaw hdr=0x%X size=%u (via post-dispatch)", header, sizeBytes);
        GameThread::EnqueuePost([copy]() {
            IssuePacketSendRaw(copy.d, copy.sz);
        });
    } else {
        Log::Warn("CtoS: SendPacketDirectRaw dropped hdr=0x%X -- GameThread not ready", header);
    }
}

void SendPacketThreaded(uint32_t size, uint32_t header, ...) {
    if (!s_initialized && !Initialize()) {
        Log::Warn("CtoS: SendPacketThreaded dropped (header=0x%X) -- not initialized", header);
        return;
    }

    uint32_t data[12];
    data[0] = header;

    va_list args;
    va_start(args, header);
    for (uint32_t i = 1; i < size && i < 12; i++) {
        data[i] = va_arg(args, uint32_t);
    }
    va_end(args);

    const uint32_t sizeBytes = size * 4;
    if (!EnqueuePacketTask(data, sizeBytes)) {
        Log::Warn("CtoS: SendPacketThreaded dropped hdr=0x%X size=%u -- enqueue failed", header, sizeBytes);
    }
}

// --- Type-safe wrappers ---


void MoveToCoord(float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    SendPacket(3, Packets::MOVE_TO_COORD, ix, iy);
}

void Dialog(uint32_t dialogId) {
    // Upstream Botshub/AutoIt uses HEADER_DIALOG_SEND (0x3B) for quest/NPC
    // dialog actions such as AcceptQuest and QuestReward.
    SendPacket(2, Packets::DIALOG_SEND, dialogId);
}

void ChangeTarget(uint32_t agentId) {
    SendPacket(2, Packets::TARGET_AGENT, agentId);
}

void ActionAttack(uint32_t agentId, uint32_t callTarget) {
    SendPacket(3, Packets::ACTION_ATTACK, agentId, callTarget);
}

void CancelAction() {
    SendPacket(1, Packets::ACTION_CANCEL);
}

void MapTravel(uint32_t mapId, uint32_t region, uint32_t district, uint32_t language) {
    SendPacket(6, Packets::MAP_TRAVEL, mapId, region, district, language, 0u);
}

void HeroAdd(uint32_t heroId) {
    SendPacket(2, Packets::HERO_ADD, heroId);
}

void HeroKick(uint32_t heroId) {
    SendPacket(2, Packets::HERO_KICK, heroId);
}

void HeroBehavior(uint32_t heroIndex, uint32_t behavior) {
    SendPacket(3, Packets::HERO_BEHAVIOR, heroIndex, behavior);
}

void HeroFlagSingle(uint32_t heroIndex, float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    SendPacket(4, Packets::HERO_FLAG_SINGLE, heroIndex, ix, iy);
}

void HeroFlagAll(float x, float y) {
    uint32_t ix, iy;
    memcpy(&ix, &x, 4);
    memcpy(&iy, &y, 4);
    SendPacket(3, Packets::HERO_FLAG_ALL, ix, iy);
}

void UseItem(uint32_t itemId) {
    SendPacket(2, Packets::ITEM_USE, itemId);
}

void EquipItem(uint32_t itemId) {
    SendPacket(2, Packets::ITEM_EQUIP, itemId);
}

void DropItem(uint32_t itemId) {
    SendPacket(2, Packets::DROP_ITEM, itemId);
}

void PickUpItem(uint32_t itemAgentId) {
    SendPacket(3, Packets::ITEM_INTERACT, itemAgentId, 0u);
}

void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot) {
    SendPacket(4, Packets::ITEM_MOVE, itemId, bagId, slot);
}

void QuestAbandon(uint32_t questId) {
    SendPacket(2, Packets::QUEST_ABANDON, questId);
}

void QuestSetActive(uint32_t questId) {
    SendPacket(2, Packets::QUEST_SET_ACTIVE, questId);
}

void UseSkill(uint32_t skillId, uint32_t targetAgentId, uint32_t callTarget) {
    // Captured live 0x46 traffic is a 20-byte packet whose first payload dword
    // is the resolved skill id. The second payload dword remains zero in those
    // traces, followed by target agent id and call-target flag.
    Log::Info("CtoS: UseSkill packet skillId=%u target=%u callTarget=%u via=pre-dispatch",
              skillId,
              targetAgentId,
              callTarget);
    SendPacket(5, Packets::USE_SKILL, skillId, 0u, targetAgentId, callTarget);
}

void TradeOfferItem(uint32_t itemId, uint32_t quantity) {
    SendPacket(3, Packets::TRADE_ADD_ITEM, itemId, quantity);
}

bool TradeOfferItemBotshub(uint32_t itemId, uint32_t quantity) {
    if (!Initialize()) {
        Log::Warn("CtoS: TradeOfferItemBotshub dropped item=%u qty=%u -- not initialized", itemId, quantity);
        return false;
    }
    TradeOfferItemBotshubCommand cmd{};
    cmd.fn = reinterpret_cast<uintptr_t>(&BotshubTradeOfferItemCommandStub);
    cmd.item_id = itemId;
    cmd.quantity = quantity;
    const bool queued = EnqueueBotshubCommand(&cmd, sizeof(cmd));
    Log::Info("CtoS: TradeOfferItemBotshub item=%u qty=%u queued=%d", itemId, quantity, queued ? 1 : 0);
    return queued;
}

void TradeCancel() {
    SendPacket(1, Packets::TRADE_CANCEL);
}

void TradeAccept() {
    SendPacket(1, Packets::TRADE_ACCEPT);
}

void TradeCancelThreaded() {
    SendPacketThreaded(1, Packets::TRADE_CANCEL);
}

void TradeAcceptThreaded() {
    SendPacketThreaded(1, Packets::TRADE_ACCEPT);
}

// Packet tap diagnostics.
PacketTapSnapshot GetPacketTapSnapshot() {
    PacketTapSnapshot snap{};
    snap.total_packets = static_cast<uint32_t>(
        InterlockedCompareExchange(&s_packetTapTotal, 0, 0));

    for (uint32_t header = 0; header < _countof(s_packetTapCounts); ++header) {
        const uint32_t count = static_cast<uint32_t>(
            InterlockedCompareExchange(&s_packetTapCounts[header], 0, 0));
        if (count == 0) continue;
        ++snap.unique_headers;
        for (uint32_t i = 0; i < _countof(snap.headers); ++i) {
            if (snap.counts[i] >= count) continue;
            for (uint32_t j = _countof(snap.headers) - 1; j > i; --j) {
                snap.headers[j] = snap.headers[j - 1];
                snap.counts[j] = snap.counts[j - 1];
            }
            snap.headers[i] = header;
            snap.counts[i] = count;
            break;
        }
    }
    return snap;
}

void ResetPacketTap() {
    InterlockedExchange(&s_packetTapTotal, 0);
    for (uint32_t header = 0; header < _countof(s_packetTapCounts); ++header) {
        InterlockedExchange(&s_packetTapCounts[header], 0);
    }
}

} // namespace GWA3::CtoS












