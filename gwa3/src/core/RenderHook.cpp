#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <MinHook.h>
#include <Windows.h>
#include <vector>
#include <mutex>

namespace GWA3::RenderHook {

// The render callback signature varies, but the AutoIt hook pattern suggests
// it's a void function with some args. We detour it and drain our queue.
using RenderCallbackFn = int(__cdecl*)(int);

static RenderCallbackFn s_originalRender = nullptr;
static CRITICAL_SECTION s_cs;
static std::vector<std::function<void()>> s_queue;
static bool s_initialized = false;

static int __cdecl RenderDetour(int arg) {
    // Drain queued render-context tasks
    EnterCriticalSection(&s_cs);
    std::vector<std::function<void()>> local;
    if (!s_queue.empty()) {
        local.swap(s_queue);
    }
    LeaveCriticalSection(&s_cs);

    for (auto& task : local) {
        task();
    }

    // Call original render callback
    return s_originalRender(arg);
}

bool Initialize() {
    if (s_initialized) return true;
    if (!Offsets::Render) {
        Log::Error("RenderHook: Render offset not resolved");
        return false;
    }

    InitializeCriticalSection(&s_cs);

    MH_STATUS status = MH_CreateHook(
        reinterpret_cast<LPVOID>(Offsets::Render),
        reinterpret_cast<LPVOID>(&RenderDetour),
        reinterpret_cast<LPVOID*>(&s_originalRender)
    );
    if (status != MH_OK) {
        Log::Error("RenderHook: MH_CreateHook failed: %s", MH_StatusToString(status));
        DeleteCriticalSection(&s_cs);
        return false;
    }

    status = MH_EnableHook(reinterpret_cast<LPVOID>(Offsets::Render));
    if (status != MH_OK) {
        Log::Error("RenderHook: MH_EnableHook failed: %s", MH_StatusToString(status));
        MH_RemoveHook(reinterpret_cast<LPVOID>(Offsets::Render));
        DeleteCriticalSection(&s_cs);
        return false;
    }

    s_initialized = true;
    Log::Info("RenderHook: Installed at 0x%08X", Offsets::Render);
    return true;
}

void Shutdown() {
    if (!s_initialized) return;
    MH_DisableHook(reinterpret_cast<LPVOID>(Offsets::Render));
    MH_RemoveHook(reinterpret_cast<LPVOID>(Offsets::Render));
    EnterCriticalSection(&s_cs);
    s_queue.clear();
    LeaveCriticalSection(&s_cs);
    DeleteCriticalSection(&s_cs);
    s_initialized = false;
    Log::Info("RenderHook: Shutdown");
}

void Enqueue(std::function<void()> task) {
    if (!s_initialized) return;
    EnterCriticalSection(&s_cs);
    s_queue.push_back(std::move(task));
    LeaveCriticalSection(&s_cs);
}

bool IsInitialized() { return s_initialized; }

} // namespace GWA3::RenderHook
