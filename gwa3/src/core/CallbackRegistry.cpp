#include <gwa3/core/CallbackRegistry.h>
#include <gwa3/core/Log.h>

#include <map>
#include <vector>
#include <mutex>
#include <algorithm>

namespace GWA3::CallbackRegistry {

// Callback record — matches GWCA's 0x30-byte layout conceptually
struct CallbackRecord {
    int32_t    altitude;
    HookEntry* entry;
    UICallback callback;
};

// Per-message callback list (altitude-sorted)
using CallbackList = std::vector<CallbackRecord>;

// Module ownership range record (0x0C bytes in GWCA)
struct ModuleRange {
    uintptr_t module_handle; // owning module
    uintptr_t range_start;   // HookEntry address range start
    uintptr_t range_end;     // HookEntry address range end
};

// === State ===
static std::map<uint32_t, CallbackList> s_uiCallbacks;       // per-messageId
static std::map<uint32_t, CallbackList> s_frameCallbacks;     // per-messageId
static CallbackList                     s_createCallbacks;    // single global list
static std::vector<ModuleRange>         s_moduleRanges;
static std::mutex                       s_mutex;
static bool                             s_initialized = false;

// === Helpers ===

static void InsertSorted(CallbackList& list, CallbackRecord&& rec) {
    auto it = std::lower_bound(list.begin(), list.end(), rec.altitude,
        [](const CallbackRecord& r, int alt) { return r.altitude < alt; });
    list.insert(it, std::move(rec));
}

static void RemoveByEntry(CallbackList& list, HookEntry* entry) {
    list.erase(
        std::remove_if(list.begin(), list.end(),
                        [entry](const CallbackRecord& r) { return r.entry == entry; }),
        list.end());
}

static void TrackModuleOwnership(HookEntry* entry) {
    // In GWCA, HookEntry addresses are resolved to module handles via
    // GetModuleHandleEx. For GWA3, we use a simpler approach:
    // each registration tracks the HookEntry in a flat set for bulk cleanup.
    // Module range tracking is available for external consumers who need it.
    (void)entry; // Ownership tracked implicitly via entry pointer matching
}

static void DispatchList(const CallbackList& list, uint32_t msgId, void* wp, void* lp) {
    for (const auto& rec : list) {
        HookStatus status{false};
        rec.callback(&status, msgId, wp, lp);
        if (status.blocked) break;
    }
}

// === Public API ===

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("CallbackRegistry: Initialized");
    return true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_uiCallbacks.clear();
    s_frameCallbacks.clear();
    s_createCallbacks.clear();
    s_moduleRanges.clear();
    s_initialized = false;
}

// === UIMessage ===

bool RegisterUIMessageCallback(HookEntry* entry, uint32_t messageId,
                               const UICallback& callback, int altitude) {
    if (!entry || !callback) return false;
    std::lock_guard<std::mutex> lock(s_mutex);

    // Remove any existing registration for this entry+messageId
    RemoveByEntry(s_uiCallbacks[messageId], entry);

    InsertSorted(s_uiCallbacks[messageId], {altitude, entry, callback});
    TrackModuleOwnership(entry);
    return true;
}

void RemoveUIMessageCallback(HookEntry* entry, uint32_t messageId) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_uiCallbacks.find(messageId);
    if (it != s_uiCallbacks.end()) {
        RemoveByEntry(it->second, entry);
        if (it->second.empty()) s_uiCallbacks.erase(it);
    }
}

// === Bulk Cleanup ===

void RemoveCallbacks(HookEntry* entry) {
    std::lock_guard<std::mutex> lock(s_mutex);

    // Remove from all UI message callbacks
    for (auto it = s_uiCallbacks.begin(); it != s_uiCallbacks.end(); ) {
        RemoveByEntry(it->second, entry);
        if (it->second.empty()) it = s_uiCallbacks.erase(it);
        else ++it;
    }

    // Remove from all frame message callbacks
    for (auto it = s_frameCallbacks.begin(); it != s_frameCallbacks.end(); ) {
        RemoveByEntry(it->second, entry);
        if (it->second.empty()) it = s_frameCallbacks.erase(it);
        else ++it;
    }

    // Remove from create-component callbacks
    RemoveByEntry(s_createCallbacks, entry);
}

// === Dispatch ===

void DispatchUIMessage(uint32_t messageId, void* wparam, void* lparam) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_uiCallbacks.find(messageId);
    if (it != s_uiCallbacks.end()) {
        DispatchList(it->second, messageId, wparam, lparam);
    }
}

} // namespace GWA3::CallbackRegistry
