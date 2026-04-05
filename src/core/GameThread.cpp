#include <gwa3/core/GameThread.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>

#include <MinHook.h>
#include <vector>
#include <algorithm>

namespace GWA3::GameThread {

// The game's render/frame callback signature: void __cdecl (float elapsed, int unknown)
using GameCallback = void(__cdecl*)(float, int);

static CRITICAL_SECTION s_cs;
static bool s_initialized = false;
static bool s_onGameThread = false;
static DWORD s_gameThreadId = 0;

static GameCallback s_originalCallback = nullptr;
static uintptr_t s_hookTarget = 0;

// --- Hook integrity watchdog (declaration, definition after queue) ---
static uint8_t s_patchedBytes[16] = {};
static uint32_t s_patchSize = 0;
static HANDLE s_watchdogThread = nullptr;
static volatile bool s_watchdogRunning = false;
static volatile LONG s_repatchCount = 0;
static HANDLE s_drainEvent = nullptr;

// --- Singleshot queue (raw function pointers — no std::function on game thread) ---
// std::function::operator() crashes GW when called from the render thread
// (MSVC CRT vtable dispatch corrupts game state after ~11 seconds).
// Use raw void(*)() pointers with separate void* context.
static constexpr uint32_t kMaxQueue = 256;

struct RawTask {
    void (*fn)(void*);
    void* ctx;
};
static RawTask s_rawQueue[kMaxQueue];
static uint32_t s_queueHead = 0;
static uint32_t s_queueTail = 0;

static RawTask s_rawPostQueue[kMaxQueue];
static uint32_t s_postHead = 0;
static uint32_t s_postTail = 0;

// std::function queue — stored separately, drained from init thread
static std::vector<Callback> s_queue;
static std::vector<Callback> s_postQueue;

// --- Persistent callback registry ---
struct CallbackRecord {
    int altitude;
    HookEntry* entry;
    Callback cb;
};
static std::vector<CallbackRecord> s_registry;

// Old Dispatch/DispatchPost removed — replaced by DrainPreQueue/DrainPostQueue below

// --- Queue drain helpers (noinline to keep detour stack frame small) ---
// The game's render callback uses sub esp,0x220 (544 bytes). If the detour's
// stack frame is too large (e.g. from std::function locals), the combined
// stack usage overflows the game thread's stack.
__declspec(noinline) static void DrainPreQueue() {
    // Called with CS held. Uses raw function pointers — no CRT vtable dispatch.
    while (s_queueTail != s_queueHead) {
        RawTask task = s_rawQueue[s_queueTail];
        s_rawQueue[s_queueTail] = {nullptr, nullptr};
        s_queueTail = (s_queueTail + 1) % kMaxQueue;
        LeaveCriticalSection(&s_cs);
        if (task.fn) task.fn(task.ctx);
        EnterCriticalSection(&s_cs);
    }
}

__declspec(noinline) static void DrainPostQueue() {
    // Called with CS held.
    while (s_postTail != s_postHead) {
        RawTask task = s_rawPostQueue[s_postTail];
        s_rawPostQueue[s_postTail] = {nullptr, nullptr};
        s_postTail = (s_postTail + 1) % kMaxQueue;
        LeaveCriticalSection(&s_cs);
        if (task.fn) task.fn(task.ctx);
        EnterCriticalSection(&s_cs);
    }
}

// Wrap a std::function into a heap-allocated raw task.
// The ctx is a heap-allocated Callback* that gets freed after invocation.
static void RawCallbackTrampoline(void* ctx) {
    auto* cb = static_cast<Callback*>(ctx);
    (*cb)();
    delete cb;
}

// --- Watchdog thread: drains queue + monitors hook integrity ---
static DWORD WINAPI HookWatchdog(LPVOID) {
    while (s_watchdogRunning) {
        DWORD waitResult = WaitForSingleObject(s_drainEvent, 50);
        if (!s_initialized) continue;

        // Drain queued tasks on watchdog thread (not the game thread)
        if (waitResult == WAIT_OBJECT_0) {
            EnterCriticalSection(&s_cs);
            while (s_queueTail != s_queueHead) {
                RawTask task = s_rawQueue[s_queueTail];
                s_rawQueue[s_queueTail] = {nullptr, nullptr};
                s_queueTail = (s_queueTail + 1) % kMaxQueue;
                LeaveCriticalSection(&s_cs);
                if (task.fn) task.fn(task.ctx);
                EnterCriticalSection(&s_cs);
            }
            while (s_postTail != s_postHead) {
                RawTask task = s_rawPostQueue[s_postTail];
                s_rawPostQueue[s_postTail] = {nullptr, nullptr};
                s_postTail = (s_postTail + 1) % kMaxQueue;
                LeaveCriticalSection(&s_cs);
                if (task.fn) task.fn(task.ctx);
                EnterCriticalSection(&s_cs);
            }
            LeaveCriticalSection(&s_cs);
        }

        // Hook integrity check
        if (s_hookTarget && s_patchSize > 0) {
            const uint8_t* cur = reinterpret_cast<const uint8_t*>(s_hookTarget);
            bool intact = (memcmp(cur, s_patchedBytes, s_patchSize) == 0);
            if (!intact) {
                DWORD oldProtect;
                if (VirtualProtect(reinterpret_cast<void*>(s_hookTarget), s_patchSize,
                                   PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    memcpy(reinterpret_cast<void*>(s_hookTarget), s_patchedBytes, s_patchSize);
                    FlushInstructionCache(GetCurrentProcess(),
                                         reinterpret_cast<void*>(s_hookTarget), s_patchSize);
                    VirtualProtect(reinterpret_cast<void*>(s_hookTarget), s_patchSize,
                                   oldProtect, &oldProtect);
                    InterlockedIncrement(&s_repatchCount);
                    Log::Info("GameThread: [WATCHDOG] Hook re-patched (count=%d)",
                              static_cast<int>(s_repatchCount));
                }
            }
        }
    }
    return 0;
}

// --- MinHook detour (minimal stack footprint) ---
// Disable /GS buffer security check — the security cookie prologue/epilogue
// conflicts with the game's render callback stack expectations.
#pragma optimize("", off)
#pragma runtime_checks("", off)
static volatile uint32_t s_detourTest = 0;
__declspec(safebuffers) static void __cdecl DetourCallback(float elapsed, int unknown) {
    EnterCriticalSection(&s_cs);
    s_onGameThread = true;
    s_gameThreadId = GetCurrentThreadId();
    // Signal the watchdog thread to drain the queue if there are pending tasks.
    // We can't call std::function or indirect function pointers from the detour —
    // MSVC generates code that crashes GW's render callback (stack/register issue).
    if (s_queueTail != s_queueHead) {
        SetEvent(s_drainEvent);
    }
    LeaveCriticalSection(&s_cs);

    if (s_originalCallback) {
        s_originalCallback(elapsed, unknown);
    }

    EnterCriticalSection(&s_cs);
    if (s_postTail != s_postHead) {
        SetEvent(s_drainEvent); // signal watchdog to drain post-queue too
    }
    s_onGameThread = false;
    s_gameThreadId = 0;
    LeaveCriticalSection(&s_cs);
}
#pragma runtime_checks("", restore)
#pragma optimize("", on)

// --- Find hook target by walking backward from assertion site to function prologue ---
static uintptr_t FindFunctionStart(uintptr_t assertionSite) {
    // Walk backward looking for a typical function prologue:
    // 55         push ebp
    // 8B EC      mov ebp, esp
    // or:
    // 55         push ebp  (standalone at function boundary after CC padding)
    const uint8_t* p = reinterpret_cast<const uint8_t*>(assertionSite);
    for (int i = 0; i < 0x300; i++) {
        // Check for push ebp / mov ebp, esp (55 8B EC)
        if (p[-i] == 0x55 && p[-i + 1] == 0x8B && p[-i + 2] == 0xEC) {
            // Verify we're at a function boundary: preceded by CC, C3, or 00
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

bool Initialize() {
    if (s_initialized) return true;

#if CRASH_TEST == 2
    Log::Info("GameThread: [CRASH_TEST=2] SKIPPED — testing MinHook conflict");
    return false;
#endif

    InitializeCriticalSection(&s_cs);

    // Find the assertion site for the render callback
    // Try full path first (GW Reforged uses full paths), then short name
    uintptr_t assertSite = Scanner::FindAssertion(
        "P:\\Code\\Engine\\Frame\\FrApi.cpp", "renderElapsed >= 0", 0);
    if (!assertSite) {
        // Fallback: try short filename (original GWCA convention)
        assertSite = Scanner::FindAssertion("FrApi.cpp", "renderElapsed >= 0", 0);
    }
    if (!assertSite) {
        Log::Error("GameThread: FindAssertion for FrApi.cpp / renderElapsed failed");
        DeleteCriticalSection(&s_cs);
        return false;
    }
    Log::Info("GameThread: Assertion site at 0x%08X", assertSite);

    // Walk backward to function start
    s_hookTarget = FindFunctionStart(assertSite);
    if (!s_hookTarget) {
        Log::Error("GameThread: Could not find function start from assertion site 0x%08X", assertSite);
        DeleteCriticalSection(&s_cs);
        return false;
    }
    Log::Info("GameThread: Hook target (function start) at 0x%08X", s_hookTarget);

    // Initialize MinHook (idempotent — safe to call multiple times)
    MH_STATUS mhStatus = MH_Initialize();
    if (mhStatus != MH_OK && mhStatus != MH_ERROR_ALREADY_INITIALIZED) {
        Log::Error("GameThread: MH_Initialize failed: %s", MH_StatusToString(mhStatus));
        DeleteCriticalSection(&s_cs);
        return false;
    }

    // Create the hook
    mhStatus = MH_CreateHook(
        reinterpret_cast<LPVOID>(s_hookTarget),
        reinterpret_cast<LPVOID>(&DetourCallback),
        reinterpret_cast<LPVOID*>(&s_originalCallback)
    );
    if (mhStatus != MH_OK) {
        Log::Error("GameThread: MH_CreateHook failed: %s", MH_StatusToString(mhStatus));
        DeleteCriticalSection(&s_cs);
        return false;
    }

    // Enable the hook
    mhStatus = MH_EnableHook(reinterpret_cast<LPVOID>(s_hookTarget));
    if (mhStatus != MH_OK) {
        Log::Error("GameThread: MH_EnableHook failed: %s", MH_StatusToString(mhStatus));
        MH_RemoveHook(reinterpret_cast<LPVOID>(s_hookTarget));
        DeleteCriticalSection(&s_cs);
        return false;
    }

    // Save the patched bytes so the watchdog can re-apply them if overwritten
    s_patchSize = 5; // MinHook uses a 5-byte JMP (E9 rel32)
    memcpy(s_patchedBytes, reinterpret_cast<void*>(s_hookTarget), s_patchSize);

    Log::Info("GameThread: Patched bytes at 0x%08X: %02X %02X %02X %02X %02X",
              s_hookTarget,
              s_patchedBytes[0], s_patchedBytes[1], s_patchedBytes[2],
              s_patchedBytes[3], s_patchedBytes[4]);

    // Dump the MinHook trampoline (s_originalCallback points to it)
    if (s_originalCallback) {
        const uint8_t* t = reinterpret_cast<const uint8_t*>(s_originalCallback);
        Log::Info("GameThread: Trampoline at 0x%08X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                  reinterpret_cast<uintptr_t>(s_originalCallback),
                  t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7],
                  t[8], t[9], t[10], t[11], t[12], t[13], t[14], t[15]);
    }

    // Create drain event and start watchdog thread
    s_drainEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr); // auto-reset
    s_watchdogRunning = true;
    s_watchdogThread = CreateThread(nullptr, 0, HookWatchdog, nullptr, 0, nullptr);

    s_initialized = true;
    Log::Info("GameThread: Hook installed successfully at 0x%08X (watchdog active)", s_hookTarget);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    // Stop watchdog first
    s_watchdogRunning = false;
    if (s_drainEvent) SetEvent(s_drainEvent); // wake watchdog so it exits
    if (s_watchdogThread) {
        WaitForSingleObject(s_watchdogThread, 2000);
        CloseHandle(s_watchdogThread);
        s_watchdogThread = nullptr;
    }
    if (s_drainEvent) {
        CloseHandle(s_drainEvent);
        s_drainEvent = nullptr;
    }

    // Disable and remove hook
    MH_DisableHook(reinterpret_cast<LPVOID>(s_hookTarget));
    MH_RemoveHook(reinterpret_cast<LPVOID>(s_hookTarget));

    // Clear all queued work
    EnterCriticalSection(&s_cs);
    // Free any pending raw tasks
    while (s_queueTail != s_queueHead) {
        auto& t = s_rawQueue[s_queueTail];
        if (t.fn == RawCallbackTrampoline && t.ctx) delete static_cast<Callback*>(t.ctx);
        t = {nullptr, nullptr};
        s_queueTail = (s_queueTail + 1) % kMaxQueue;
    }
    while (s_postTail != s_postHead) {
        auto& t = s_rawPostQueue[s_postTail];
        if (t.fn == RawCallbackTrampoline && t.ctx) delete static_cast<Callback*>(t.ctx);
        t = {nullptr, nullptr};
        s_postTail = (s_postTail + 1) % kMaxQueue;
    }
    s_queueHead = s_queueTail = 0;
    s_postHead = s_postTail = 0;
    s_registry.clear();
    s_onGameThread = false;
    s_gameThreadId = 0;
    s_initialized = false;
    LeaveCriticalSection(&s_cs);

    DeleteCriticalSection(&s_cs);
    s_originalCallback = nullptr;
    s_hookTarget = 0;

    Log::Info("GameThread: Shutdown complete");
}

void Enqueue(Callback task) {
    if (!s_initialized) return;

    EnterCriticalSection(&s_cs);

    // Fast path: if already on game thread, execute immediately
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
    s_rawQueue[s_queueHead] = {RawCallbackTrampoline, new Callback(std::move(task))};
    s_queueHead = next;
    LeaveCriticalSection(&s_cs);
}

void EnqueuePost(Callback task) {
    if (!s_initialized) return;
    // Always queue — never fast-path. EnqueuePost must run AFTER the original
    // game callback, but s_onGameThread is true during pre-callback Dispatch too.
    EnterCriticalSection(&s_cs);

    // Fast path: if already on game thread during post-dispatch, execute immediately
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
    s_rawPostQueue[s_postHead] = {RawCallbackTrampoline, new Callback(std::move(task))};
    s_postHead = next;
    LeaveCriticalSection(&s_cs);
}

void RegisterCallback(HookEntry* entry, Callback cb, int altitude) {
    if (!s_initialized || !entry) return;

    EnterCriticalSection(&s_cs);

    // Remove existing registration for this entry
    s_registry.erase(
        std::remove_if(s_registry.begin(), s_registry.end(),
            [entry](const CallbackRecord& r) { return r.entry == entry; }),
        s_registry.end()
    );

    // Insert altitude-sorted (descending — higher altitude executes first)
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

} // namespace GWA3::GameThread
