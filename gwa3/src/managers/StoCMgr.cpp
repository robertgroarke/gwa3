#include <gwa3/managers/StoCMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <map>
#include <vector>
#include <mutex>
#include <algorithm>

namespace GWA3::StoC {

// Callback entry with altitude for ordering
struct CallbackEntry {
    HookEntry*     entry;
    PacketCallback callback;
    int            altitude;
};

// Per-header callback list
struct HeaderCallbacks {
    std::vector<CallbackEntry> pre;  // altitude <= 0, sorted ascending
    std::vector<CallbackEntry> post; // altitude >  0, sorted ascending
};

static std::map<uint32_t, HeaderCallbacks> s_callbacks;
static std::mutex s_mutex;
static bool s_initialized = false;

// Original StoC handler function pointer (for hook chain)
// The game's StoC dispatcher has signature: void __fastcall Handler(void* this, void* edx, uint32_t* packet)
typedef void(__fastcall* StoCHandlerFn)(void*, void*, uint32_t*);
static StoCHandlerFn s_originalHandler = nullptr;

// Our hook intercept
static void __fastcall StoCHookHandler(void* thisPtr, void* edx, uint32_t* rawPacket) {
    if (!rawPacket) {
        if (s_originalHandler) s_originalHandler(thisPtr, edx, rawPacket);
        return;
    }

    uint32_t header = rawPacket[0];
    auto* packet = reinterpret_cast<PacketBase*>(rawPacket);

    // Fire pre-processing callbacks (altitude <= 0)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_callbacks.find(header);
        if (it != s_callbacks.end()) {
            HookStatus status{false};
            for (auto& cb : it->second.pre) {
                cb.callback(&status, packet);
                if (status.blocked) {
                    return; // packet blocked, don't process or fire post callbacks
                }
            }
        }
    }

    // Call original game handler
    if (s_originalHandler) {
        s_originalHandler(thisPtr, edx, rawPacket);
    }

    // Fire post-processing callbacks (altitude > 0)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_callbacks.find(header);
        if (it != s_callbacks.end()) {
            HookStatus status{false};
            for (auto& cb : it->second.post) {
                cb.callback(&status, packet);
                if (status.blocked) break;
            }
        }
    }
}

bool Initialize() {
    if (s_initialized) return true;

    // TODO: Hook the game's StoC packet dispatcher.
    // This requires finding the dispatch function via Offsets and installing
    // a MinHook detour. The hook target is the function that the game calls
    // for each incoming server packet — typically found via the Engine hook
    // or a specific StoC dispatch pattern.
    //
    // For now, the callback registration API is fully functional but
    // callbacks won't fire until the hook is installed.

    s_initialized = true;
    Log::Info("StoCMgr: Initialized (hook=%s)", s_originalHandler ? "OK" : "PENDING");
    return true;
}

void Shutdown() {
    // TODO: Remove MinHook detour if installed
    std::lock_guard<std::mutex> lock(s_mutex);
    s_callbacks.clear();
    s_originalHandler = nullptr;
    s_initialized = false;
}

bool RegisterPacketCallback(HookEntry* entry, uint32_t header,
                            const PacketCallback& callback, int altitude) {
    if (!entry || !callback) return false;

    std::lock_guard<std::mutex> lock(s_mutex);
    auto& hdr = s_callbacks[header];

    CallbackEntry ce{entry, callback, altitude};

    if (altitude <= 0) {
        hdr.pre.push_back(ce);
        std::sort(hdr.pre.begin(), hdr.pre.end(),
                  [](const CallbackEntry& a, const CallbackEntry& b) {
                      return a.altitude < b.altitude;
                  });
    } else {
        hdr.post.push_back(ce);
        std::sort(hdr.post.begin(), hdr.post.end(),
                  [](const CallbackEntry& a, const CallbackEntry& b) {
                      return a.altitude < b.altitude;
                  });
    }

    return true;
}

bool RegisterPostPacketCallback(HookEntry* entry, uint32_t header,
                                const PacketCallback& callback) {
    return RegisterPacketCallback(entry, header, callback, 1);
}

static void RemoveFromList(std::vector<CallbackEntry>& list, HookEntry* entry) {
    list.erase(
        std::remove_if(list.begin(), list.end(),
                        [entry](const CallbackEntry& ce) { return ce.entry == entry; }),
        list.end());
}

void RemoveCallback(uint32_t header, HookEntry* entry) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_callbacks.find(header);
    if (it == s_callbacks.end()) return;

    RemoveFromList(it->second.pre, entry);
    RemoveFromList(it->second.post, entry);

    if (it->second.pre.empty() && it->second.post.empty()) {
        s_callbacks.erase(it);
    }
}

void RemoveCallbacks(HookEntry* entry) {
    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto it = s_callbacks.begin(); it != s_callbacks.end(); ) {
        RemoveFromList(it->second.pre, entry);
        RemoveFromList(it->second.post, entry);
        if (it->second.pre.empty() && it->second.post.empty()) {
            it = s_callbacks.erase(it);
        } else {
            ++it;
        }
    }
}

bool EmulatePacket(PacketBase* packet) {
    if (!packet) return false;

    uint32_t header = packet->header;

    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_callbacks.find(header);
    if (it == s_callbacks.end()) return false;

    HookStatus status{false};

    // Fire pre callbacks
    for (auto& cb : it->second.pre) {
        cb.callback(&status, packet);
        if (status.blocked) return true;
    }

    // Fire post callbacks
    for (auto& cb : it->second.post) {
        cb.callback(&status, packet);
        if (status.blocked) return true;
    }

    return true;
}

} // namespace GWA3::StoC
