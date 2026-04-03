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

// --- Singleshot queue ---
static std::vector<Callback> s_queue;

// --- Persistent callback registry ---
struct CallbackRecord {
    int altitude;
    HookEntry* entry;
    Callback cb;
};
static std::vector<CallbackRecord> s_registry;

// --- Dispatcher (runs before original game callback each frame) ---
static void Dispatch() {
    EnterCriticalSection(&s_cs);
    s_onGameThread = true;
    s_gameThreadId = GetCurrentThreadId();

    // Phase 1: Drain singleshot queue (swap to local to avoid holding lock during execution)
    std::vector<Callback> local;
    if (!s_queue.empty()) {
        local.swap(s_queue);
    }

    // Phase 2: Copy persistent callbacks (so removals during iteration are safe)
    std::vector<CallbackRecord> localRegistry = s_registry;

    LeaveCriticalSection(&s_cs);

    // Execute singleshot tasks
    for (auto& task : local) {
        task();
    }

    // Execute persistent callbacks (already altitude-sorted)
    for (auto& rec : localRegistry) {
        rec.cb();
    }
}

// --- MinHook detour ---
static void __cdecl DetourCallback(float elapsed, int unknown) {
    Dispatch();
    if (s_originalCallback) {
        s_originalCallback(elapsed, unknown);
    }

    EnterCriticalSection(&s_cs);
    s_onGameThread = false;
    s_gameThreadId = 0;
    LeaveCriticalSection(&s_cs);
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

    s_initialized = true;
    Log::Info("GameThread: Hook installed successfully at 0x%08X", s_hookTarget);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    // Disable and remove hook
    MH_DisableHook(reinterpret_cast<LPVOID>(s_hookTarget));
    MH_RemoveHook(reinterpret_cast<LPVOID>(s_hookTarget));

    // Clear all queued work
    EnterCriticalSection(&s_cs);
    s_queue.clear();
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

    s_queue.push_back(std::move(task));
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
