#include <gwa3/core/GameThread.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>

#include <vector>
#include <algorithm>

namespace GWA3::GameThread {

static CRITICAL_SECTION s_cs;
static bool s_initialized = false;
static bool s_onGameThread = false;
static DWORD s_gameThreadId = 0;

static uintptr_t s_hookTarget = 0;

// --- VEH INT3 hook state ---
static PVOID s_vehHandle = nullptr;
static uint8_t s_originalByte = 0;      // byte replaced by INT3 (expected: 0x55 = push ebp)
static volatile bool s_hookSuspended = false;

// --- Watchdog ---
static HANDLE s_watchdogThread = nullptr;
static volatile bool s_watchdogRunning = false;
static volatile LONG s_repatchCount = 0;

// --- Post-queue sender thread ---
// PacketSend cannot be called from VEH exception dispatch context.
// The VEH handler signals this event; the sender thread wakes and
// dispatches one post-queue task outside any exception context.
static HANDLE s_postDispatchEvent = nullptr;
static HANDLE s_postSenderThread = nullptr;
static volatile bool s_postSenderRunning = false;
static volatile bool s_postSenderEnabled = false; // set true after bootstrap

// --- Singleshot queue ---
// Uses raw function pointers with inline context (no heap allocation).
// CRT heap operations (new/delete/std::function) on the game thread corrupt
// GW's memory and cause a crash ~11s later.
static constexpr uint32_t kMaxQueue = 256;

struct InlineTask {
    using Invoker = void(*)(void* storage);
    Invoker invoke;
    alignas(8) char storage[64];

    void operator()() { if (invoke) invoke(storage); }
    explicit operator bool() const { return invoke != nullptr; }
};

static InlineTask s_preQueue[kMaxQueue];
static volatile uint32_t s_queueHead = 0;
static volatile uint32_t s_queueTail = 0;

static InlineTask s_postQueue_ring[kMaxQueue];
static volatile uint32_t s_postHead = 0;
static volatile uint32_t s_postTail = 0;

// Legacy — kept for API compatibility
static std::vector<Callback> s_queue;
static std::vector<Callback> s_postQueue;

// --- Persistent callback registry ---
struct CallbackRecord {
    int altitude;
    HookEntry* entry;
    Callback cb;
};
static std::vector<CallbackRecord> s_registry;

// Construct a Callback (std::function) directly into an InlineTask slot.
static void EmplaceCallback(InlineTask& slot, Callback&& task) {
    static_assert(sizeof(Callback) <= sizeof(InlineTask::storage),
                  "Callback exceeds InlineTask storage");
    slot.invoke = [](void* storage) {
        auto& fn = *reinterpret_cast<Callback*>(storage);
        fn();
        fn.~Callback();
    };
    new (slot.storage) Callback(std::move(task));
}

// ===== VEH INT3 Handler =====
// Catches EXCEPTION_BREAKPOINT at our hook address, drains queues,
// emulates the replaced instruction (push ebp), and continues.
// Re-entrancy safe: if PacketSend internally reaches the hook address,
// the handler detects re-entry and just emulates the instruction.

static volatile LONG s_inHandler = 0;

static volatile LONG s_vehHitCount = 0;
static volatile LONG s_tryFail = 0;

// --- Post-queue trampoline ---
// Allocated as RWX shellcode. When VEH handler detects post-queue work,
// it sets EIP to this trampoline instead of hookTarget+1. The trampoline
// runs OUTSIDE VEH exception context (normal game thread execution):
//   pushad/pushfd
//   restore original byte (so PacketSend doesn't re-hit INT3)
//   call DispatchOnePostTask
//   re-patch INT3
//   popfd/popad
//   push ebp              (emulate replaced instruction)
//   jmp hookTarget+1      (continue normal function)
static void* s_postTrampoline = nullptr;

static void __stdcall DispatchOnePostTask() {
    EnterCriticalSection(&s_cs);
    if (s_postTail != s_postHead) {
        uint32_t idx = s_postTail;
        s_postTail = (s_postTail + 1) % kMaxQueue;
        LeaveCriticalSection(&s_cs);
        s_postQueue_ring[idx]();
        s_postQueue_ring[idx].invoke = nullptr;
    } else {
        LeaveCriticalSection(&s_cs);
    }
}

static LONG CALLBACK VehHandler(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
        return EXCEPTION_CONTINUE_SEARCH;

    uintptr_t faultAddr = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
    if (faultAddr != s_hookTarget)
        return EXCEPTION_CONTINUE_SEARCH;

    LONG hitNum = InterlockedIncrement(&s_vehHitCount);
    // Log first few hits + whenever queue has work
    if (hitNum <= 3 || (s_queueTail != s_queueHead && hitNum % 100 == 0)) {
        Log::Info("GameThread: VEH hit #%d tid=%u qH=%u qT=%u",
                  static_cast<int>(hitNum), GetCurrentThreadId(), s_queueHead, s_queueTail);
    }

    // Re-entrancy: PacketSend can reach this address internally.
    // Just emulate the original instruction and return.
    if (InterlockedCompareExchange(&s_inHandler, 1, 0) != 0) {
        // Emulate: push ebp (opcode 0x55)
        ep->ContextRecord->Esp -= 4;
        *reinterpret_cast<uint32_t*>(ep->ContextRecord->Esp) = ep->ContextRecord->Ebp;
        ep->ContextRecord->Eip = static_cast<DWORD>(s_hookTarget + 1);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // --- Game thread dispatch ---
    EnterCriticalSection(&s_cs);

    s_onGameThread = true;
    s_gameThreadId = GetCurrentThreadId();

    // Pre-queue: drain all
    while (s_queueTail != s_queueHead) {
        uint32_t idx = s_queueTail;
        s_queueTail = (s_queueTail + 1) % kMaxQueue;
        LeaveCriticalSection(&s_cs);
        s_preQueue[idx]();
        s_preQueue[idx].invoke = nullptr;
        EnterCriticalSection(&s_cs);
    }

    // Post-queue: set flag for trampoline dispatch.
    // PacketSend cannot be called from VEH exception context.
    // Instead, we redirect EIP to a trampoline that calls our dispatch
    // function in normal game thread context, then continues normally.
    bool hasPostWork = (s_postTail != s_postHead);

    s_onGameThread = false;
    s_gameThreadId = 0;
    LeaveCriticalSection(&s_cs);

    // If post-queue has work, redirect to the trampoline which calls
    // PacketSend dispatch in normal game thread context (not VEH).
    // Otherwise, just emulate push ebp and continue normally.
    if (hasPostWork) {
        ep->ContextRecord->Eip = static_cast<DWORD>(
            reinterpret_cast<uintptr_t>(s_postTrampoline));
    } else {
        // Emulate: push ebp
        ep->ContextRecord->Esp -= 4;
        *reinterpret_cast<uint32_t*>(ep->ContextRecord->Esp) = ep->ContextRecord->Ebp;
        ep->ContextRecord->Eip = static_cast<DWORD>(s_hookTarget + 1);
    }

    InterlockedExchange(&s_inHandler, 0);
    return EXCEPTION_CONTINUE_EXECUTION;
}

// --- Find hook target by walking backward from assertion site to function prologue ---
static uintptr_t FindFunctionStart(uintptr_t assertionSite) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(assertionSite);
    for (int i = 0; i < 0x300; i++) {
        if (p[-i] == 0x55 && p[-i + 1] == 0x8B && p[-i + 2] == 0xEC) {
            if (i > 0) {
                uint8_t prev = p[-i - 1];
                if (prev == 0xCC || prev == 0xC3 || prev == 0x00 || prev == 0x90) {
                    return assertionSite - i;
                }
            } else {
                return assertionSite - i;
            }
        }
    }
    return 0;
}

// --- Post-queue sender: dispatches one task per signal, outside VEH context ---
static DWORD WINAPI PostSenderThread(LPVOID) {
    while (s_postSenderRunning) {
        WaitForSingleObject(s_postDispatchEvent, 200);
        if (!s_postSenderRunning) break;
        if (!s_postSenderEnabled) continue; // wait until bootstrap complete

        // Suspend INT3, dispatch one task, restore INT3
        EnterCriticalSection(&s_cs);
        if (s_postTail == s_postHead) {
            LeaveCriticalSection(&s_cs);
            continue;
        }
        uint32_t idx = s_postTail;
        s_postTail = (s_postTail + 1) % kMaxQueue;
        LeaveCriticalSection(&s_cs);

        // Call directly — the VEH re-entrancy guard handles any INT3 hits
        // that PacketSend triggers internally (emulates push ebp, no dispatch).
        // Bisect tests proved PacketSend works from non-VEH thread context
        // with INT3 active.
        s_postQueue_ring[idx]();
        s_postQueue_ring[idx].invoke = nullptr;
    }
    return 0;
}

// --- Watchdog: re-patches INT3 when game's integrity checker restores original byte ---
static DWORD WINAPI HookWatchdog(LPVOID) {
    while (s_watchdogRunning) {
        Sleep(100);
        if (!s_initialized || !s_hookTarget) continue;
        if (s_hookSuspended) continue;

        // Periodically log hit count for diagnostics
        static int watchdogCycles = 0;
        if (++watchdogCycles % 50 == 1) { // every ~5 seconds
            Log::Info("GameThread: [WATCHDOG] VEH hits=%d tryFail=%d qH=%u qT=%u",
                      static_cast<int>(s_vehHitCount), static_cast<int>(s_tryFail),
                      s_queueHead, s_queueTail);
        }

        uint8_t cur = *reinterpret_cast<const uint8_t*>(s_hookTarget);
        if (cur != 0xCC) {
            *reinterpret_cast<uint8_t*>(s_hookTarget) = 0xCC;
            FlushInstructionCache(GetCurrentProcess(),
                                 reinterpret_cast<void*>(s_hookTarget), 1);
            InterlockedIncrement(&s_repatchCount);
            Log::Info("GameThread: [WATCHDOG] INT3 re-patched (count=%d)",
                      static_cast<int>(s_repatchCount));
        }
    }
    return 0;
}

bool Initialize() {
    if (s_initialized) return true;

    InitializeCriticalSection(&s_cs);

    // Find the FrApi/Engine shared hook site
    uintptr_t assertSite = Scanner::FindAssertion(
        "P:\\Code\\Engine\\Frame\\FrApi.cpp", "renderElapsed >= 0", 0);
    if (!assertSite) {
        assertSite = Scanner::FindAssertion("FrApi.cpp", "renderElapsed >= 0", 0);
    }
    if (!assertSite) {
        Log::Error("GameThread: FindAssertion for FrApi.cpp / renderElapsed failed");
        DeleteCriticalSection(&s_cs);
        return false;
    }

    s_hookTarget = FindFunctionStart(assertSite);
    if (!s_hookTarget) {
        Log::Error("GameThread: Could not find function start from assertion 0x%08X", assertSite);
        DeleteCriticalSection(&s_cs);
        return false;
    }

    // Save and validate original byte (must be 0x55 = push ebp)
    s_originalByte = *reinterpret_cast<uint8_t*>(s_hookTarget);
    if (s_originalByte != 0x55) {
        Log::Error("GameThread: Unexpected byte 0x%02X at hook target 0x%08X (expected 0x55)",
                   s_originalByte, s_hookTarget);
        DeleteCriticalSection(&s_cs);
        return false;
    }
    Log::Info("GameThread: Hook target at 0x%08X (byte=0x%02X)", s_hookTarget, s_originalByte);

    // Register VEH (first in chain)
    s_vehHandle = AddVectoredExceptionHandler(1, VehHandler);
    if (!s_vehHandle) {
        Log::Error("GameThread: AddVectoredExceptionHandler failed");
        DeleteCriticalSection(&s_cs);
        return false;
    }

    // Make hook site writable (keep it RWX for watchdog + suspend/resume)
    DWORD oldProtect;
    VirtualProtect(reinterpret_cast<void*>(s_hookTarget), 1,
                   PAGE_EXECUTE_READWRITE, &oldProtect);

    // Write INT3 (single atomic byte write — no race with game thread)
    *reinterpret_cast<uint8_t*>(s_hookTarget) = 0xCC;
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_hookTarget), 1);

    // Build post-queue dispatch trampoline
    s_postTrampoline = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (s_postTrampoline) {
        uint8_t* sc = reinterpret_cast<uint8_t*>(s_postTrampoline);
        size_t i = 0;
        auto emit8 = [&](uint8_t v) { sc[i++] = v; };
        auto emit32 = [&](uint32_t v) { memcpy(sc + i, &v, 4); i += 4; };

        // Restore original byte (C6 05 [addr] [byte])
        emit8(0xC6); emit8(0x05);
        emit32(static_cast<uint32_t>(s_hookTarget));
        emit8(s_originalByte);

        // call DispatchOnePostTask (__stdcall, saves/restores own regs)
        emit8(0xE8);
        uintptr_t callAddr = reinterpret_cast<uintptr_t>(&DispatchOnePostTask);
        int32_t callRel = static_cast<int32_t>(callAddr - reinterpret_cast<uintptr_t>(sc + i + 4));
        emit32(*reinterpret_cast<uint32_t*>(&callRel));

        // Re-patch INT3 (C6 05 [addr] CC)
        emit8(0xC6); emit8(0x05);
        emit32(static_cast<uint32_t>(s_hookTarget));
        emit8(0xCC);

        // push ebp (emulate replaced instruction)
        emit8(0x55);

        // jmp hookTarget+1
        emit8(0xE9);
        int32_t jmpRel = static_cast<int32_t>((s_hookTarget + 1) - reinterpret_cast<uintptr_t>(sc + i + 4));
        emit32(*reinterpret_cast<uint32_t*>(&jmpRel));

        FlushInstructionCache(GetCurrentProcess(), sc, static_cast<DWORD>(i));
        Log::Info("GameThread: Post-queue trampoline at 0x%08X (%u bytes)",
                  reinterpret_cast<uintptr_t>(s_postTrampoline), static_cast<unsigned>(i));
    }

    // Start watchdog
    s_watchdogRunning = true;
    s_watchdogThread = CreateThread(nullptr, 0, HookWatchdog, nullptr, 0, nullptr);

    // Start post-queue sender thread
    s_postDispatchEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    s_postSenderRunning = true;
    s_postSenderThread = CreateThread(nullptr, 0, PostSenderThread, nullptr, 0, nullptr);

    s_initialized = true;
    Log::Info("GameThread: VEH INT3 hook installed at 0x%08X (watchdog active)", s_hookTarget);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    // Stop watchdog
    s_watchdogRunning = false;
    if (s_watchdogThread) {
        WaitForSingleObject(s_watchdogThread, 2000);
        CloseHandle(s_watchdogThread);
        s_watchdogThread = nullptr;
    }

    // Stop post-queue sender
    s_postSenderRunning = false;
    if (s_postDispatchEvent) SetEvent(s_postDispatchEvent);
    if (s_postSenderThread) {
        WaitForSingleObject(s_postSenderThread, 2000);
        CloseHandle(s_postSenderThread);
        s_postSenderThread = nullptr;
    }
    if (s_postDispatchEvent) {
        CloseHandle(s_postDispatchEvent);
        s_postDispatchEvent = nullptr;
    }

    // Restore original byte
    if (s_hookTarget) {
        *reinterpret_cast<uint8_t*>(s_hookTarget) = s_originalByte;
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_hookTarget), 1);
    }

    // Remove VEH
    if (s_vehHandle) {
        RemoveVectoredExceptionHandler(s_vehHandle);
        s_vehHandle = nullptr;
    }

    // Clear all queued work
    EnterCriticalSection(&s_cs);
    s_queueHead = s_queueTail = 0;
    s_postHead = s_postTail = 0;
    for (uint32_t i = 0; i < kMaxQueue; i++) {
        s_preQueue[i].invoke = nullptr;
        s_postQueue_ring[i].invoke = nullptr;
    }
    s_registry.clear();
    s_onGameThread = false;
    s_gameThreadId = 0;
    s_initialized = false;
    LeaveCriticalSection(&s_cs);

    DeleteCriticalSection(&s_cs);
    s_hookTarget = 0;

    Log::Info("GameThread: Shutdown complete");
}

void Enqueue(Callback task) {
    if (!s_initialized) return;

    EnterCriticalSection(&s_cs);

    if (s_onGameThread && s_gameThreadId == GetCurrentThreadId()) {
        LeaveCriticalSection(&s_cs);
        task();
        return;
    }

    uint32_t next = (s_queueHead + 1) % kMaxQueue;
    if (next == s_queueTail) {
        LeaveCriticalSection(&s_cs);
        Log::Warn("GameThread: Enqueue ring buffer full, dropping task");
        return;
    }
    EmplaceCallback(s_preQueue[s_queueHead], std::move(task));
    s_queueHead = next;
    LeaveCriticalSection(&s_cs);
}

void EnqueueRaw(InlineTask::Invoker invoker, const void* data, size_t dataSize) {
    if (!s_initialized || !invoker) return;
    if (dataSize > sizeof(InlineTask::storage)) return;

    EnterCriticalSection(&s_cs);

    uint32_t next = (s_queueHead + 1) % kMaxQueue;
    if (next == s_queueTail) {
        LeaveCriticalSection(&s_cs);
        Log::Warn("GameThread: EnqueueRaw ring buffer full");
        return;
    }
    auto& slot = s_preQueue[s_queueHead];
    slot.invoke = invoker;
    memcpy(slot.storage, data, dataSize);
    s_queueHead = next;
    LeaveCriticalSection(&s_cs);
}

void EnqueuePostRaw(InlineTask::Invoker invoker, const void* data, size_t dataSize) {
    if (!s_initialized || !invoker) return;
    if (dataSize > sizeof(InlineTask::storage)) return;

    EnterCriticalSection(&s_cs);

    uint32_t next = (s_postHead + 1) % kMaxQueue;
    if (next == s_postTail) {
        LeaveCriticalSection(&s_cs);
        Log::Warn("GameThread: EnqueuePostRaw ring buffer full");
        return;
    }
    auto& slot = s_postQueue_ring[s_postHead];
    slot.invoke = invoker;
    memcpy(slot.storage, data, dataSize);
    s_postHead = next;
    LeaveCriticalSection(&s_cs);
}

void EnqueuePost(Callback task) {
    if (!s_initialized) return;
    // Always queue — never fast-path. EnqueuePost must run AFTER the original
    // game callback, but s_onGameThread is true during pre-callback Dispatch too.
    EnterCriticalSection(&s_cs);

    if (s_onGameThread && s_gameThreadId == GetCurrentThreadId()) {
        LeaveCriticalSection(&s_cs);
        task();
        return;
    }

    uint32_t next = (s_postHead + 1) % kMaxQueue;
    if (next == s_postTail) {
        LeaveCriticalSection(&s_cs);
        Log::Warn("GameThread: EnqueuePost ring buffer full, dropping task");
        return;
    }
    EmplaceCallback(s_postQueue_ring[s_postHead], std::move(task));
    s_postHead = next;
    LeaveCriticalSection(&s_cs);
}

void RegisterCallback(HookEntry* entry, Callback cb, int altitude) {
    if (!s_initialized || !entry) return;

    EnterCriticalSection(&s_cs);
    s_registry.erase(
        std::remove_if(s_registry.begin(), s_registry.end(),
            [entry](const CallbackRecord& r) { return r.entry == entry; }),
        s_registry.end()
    );
    CallbackRecord rec{altitude, entry, std::move(cb)};
    auto it = std::lower_bound(s_registry.begin(), s_registry.end(), rec,
        [](const CallbackRecord& a, const CallbackRecord& b) {
            return a.altitude > b.altitude;
        });
    s_registry.insert(it, std::move(rec));
    LeaveCriticalSection(&s_cs);
}

void RemoveCallback(HookEntry* entry) {
    if (!s_initialized || !entry) return;

    EnterCriticalSection(&s_cs);
    s_registry.erase(
        std::remove_if(s_registry.begin(), s_registry.end(),
            [entry](const CallbackRecord& r) { return r.entry == entry; }),
        s_registry.end()
    );
    LeaveCriticalSection(&s_cs);
}

bool IsOnGameThread() {
    if (!s_initialized) return false;
    EnterCriticalSection(&s_cs);
    bool result = s_onGameThread && s_gameThreadId == GetCurrentThreadId();
    LeaveCriticalSection(&s_cs);
    return result;
}

bool IsInitialized() {
    return s_initialized;
}

void EnablePostDispatch() {
    s_postSenderEnabled = true;
    Log::Info("GameThread: Post-dispatch sender enabled");
}

// Atomic single-byte writes — no thread suspension needed.
void SuspendHook() {
    if (!s_initialized || !s_hookTarget) return;
    s_hookSuspended = true;
    *reinterpret_cast<uint8_t*>(s_hookTarget) = s_originalByte;
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_hookTarget), 1);
}

void ResumeHook() {
    if (!s_initialized || !s_hookTarget) return;
    *reinterpret_cast<uint8_t*>(s_hookTarget) = 0xCC;
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_hookTarget), 1);
    s_hookSuspended = false;
}

} // namespace GWA3::GameThread
