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

// --- Singleshot queue ---
// Uses raw function pointers with inline context (no heap allocation).
// CRT heap operations (new/delete/std::function) on the game thread corrupt
// GW's memory and cause a crash ~11s later.
static constexpr uint32_t kMaxQueue = 256;

// Inline callable: stores a function pointer + up to 48 bytes of captured data.
// Avoids std::function heap allocation entirely.
struct InlineTask {
    using Invoker = void(*)(void* storage);
    Invoker invoke;
    alignas(8) char storage[64]; // 64 bytes: fits CtoS packet captures (60 bytes)

    void operator()() { if (invoke) invoke(storage); }
    explicit operator bool() const { return invoke != nullptr; }
};

static InlineTask s_preQueue[kMaxQueue];
static uint32_t s_queueHead = 0;
static uint32_t s_queueTail = 0;

static InlineTask s_postQueue_ring[kMaxQueue];
static uint32_t s_postHead = 0;
static uint32_t s_postTail = 0;

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

// Old Dispatch/DispatchPost removed — replaced by DrainPreQueue/DrainPostQueue below

// --- Queue drain (called via GameCallback-typed pointer from detour) ---
// No heap allocation — InlineTask stores callable inline.
static void __cdecl DrainQueuesOnGameThread(float, int) {
    EnterCriticalSection(&s_cs);
    s_onGameThread = true;
    s_gameThreadId = GetCurrentThreadId();

    while (s_queueTail != s_queueHead) {
        uint32_t idx = s_queueTail;
        s_queueTail = (s_queueTail + 1) % kMaxQueue;
        Log::Info("GameThread: Drain pre[%u] invoke=0x%08X", idx,
                  reinterpret_cast<uintptr_t>(s_preQueue[idx].invoke));
        LeaveCriticalSection(&s_cs);
        s_preQueue[idx]();
        s_preQueue[idx].invoke = nullptr;
        Log::Info("GameThread: Drain pre[%u] done", idx);
        EnterCriticalSection(&s_cs);
    }

    LeaveCriticalSection(&s_cs);
}

static void __cdecl DrainPostQueuesOnGameThread(float, int) {
    EnterCriticalSection(&s_cs);

    // Process at most ONE post-queue task per frame.
    // PacketSend has internal locks that prevent multiple calls per frame —
    // calling it twice from post-dispatch deadlocks the game thread.
    if (s_postTail != s_postHead) {
        uint32_t idx = s_postTail;
        s_postTail = (s_postTail + 1) % kMaxQueue;
        LeaveCriticalSection(&s_cs);
        s_postQueue_ring[idx]();
        s_postQueue_ring[idx].invoke = nullptr;
        EnterCriticalSection(&s_cs);
    }

    s_onGameThread = false;
    s_gameThreadId = 0;
    LeaveCriticalSection(&s_cs);
}

// Construct a Callback (std::function) directly into an InlineTask slot.
// MUST be constructed in-place — returning InlineTask by value would bitwise-copy
// the SBO, creating dangling self-references in the std::function.
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

// --- Watchdog thread: drains queue + monitors hook integrity ---
static DWORD WINAPI HookWatchdog(LPVOID) {
    while (s_watchdogRunning) {
        Sleep(100);
        if (!s_initialized || !s_hookTarget || s_patchSize == 0) continue;

        // Hook integrity check — re-patch if game overwrites our hook
        const uint8_t* cur = reinterpret_cast<const uint8_t*>(s_hookTarget);
        if (memcmp(cur, s_patchedBytes, s_patchSize) != 0) {
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
    return 0;
}

// --- MinHook detour ---
// Drain functions use GameCallback signature so MSVC generates identical
// call-site code as s_originalCallback (which is proven stable).
static GameCallback s_preDrain = reinterpret_cast<GameCallback>(&DrainQueuesOnGameThread);
static GameCallback s_postDrain = reinterpret_cast<GameCallback>(&DrainPostQueuesOnGameThread);

static void __cdecl DetourCallback(float elapsed, int unknown) {
    // Pre-dispatch: drain queued tasks on game thread
    if (s_queueTail != s_queueHead) {
        s_preDrain(elapsed, unknown);
    }

    // Call original game callback
    if (s_originalCallback) {
        s_originalCallback(elapsed, unknown);
    }

    // Post-dispatch
    if (s_postTail != s_postHead) {
        s_postDrain(elapsed, unknown);
    }
}

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
    Log::Info("GameThread: EnqueuePostRaw OK (head=%u tail=%u)", s_postHead, s_postTail);
    LeaveCriticalSection(&s_cs);
}

void EnqueuePost(Callback task) {
    if (!s_initialized) return;
    // Always queue — never fast-path. EnqueuePost must run AFTER the original
    // game callback, but s_onGameThread is true during pre-callback Dispatch too.
    EnterCriticalSection(&s_cs);

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
