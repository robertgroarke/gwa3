#include <gwa3/managers/StoCMgr.h>
#include <gwa3/game/GameTypes.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/HookMarker.h>
#include <gwa3/core/HookMarker.h>

#include <Windows.h>
#include <map>
#include <vector>
#include <algorithm>

namespace GWA3::StoC {

// ===== Game structures for StoC handler table =====

// Each StoC header has a handler entry in the game's handler array
typedef bool(__cdecl* GameStoCHandlerFn)(PacketBase* packet);

struct GameStoCHandler {
    uint32_t*          packet_template;
    uint32_t           template_size;
    GameStoCHandlerFn  handler_func;
};

// GameServer struct (partial — only what we need)
struct GameServerCodec {
    uint8_t  h0000[12];
    struct {
        uint8_t  h0000[12];
        void*    next;
        uint8_t  h0010[12];
        uint32_t ClientCodecArray[4];
        GWArray<GameStoCHandler> handlers;
    }* ls_codec;
    uint8_t  h0010[12];
    uint32_t ClientCodecArray[4];
    GWArray<GameStoCHandler> handlers;
};

struct GameServer {
    uint8_t h0000[8];
    GameServerCodec* gs_codec;
};

// ===== Internal state =====

struct CallbackEntry {
    HookEntry*     entry;
    PacketCallback callback;
    int            altitude;
};

struct HeaderCallbacks {
    std::vector<CallbackEntry> pre;  // altitude <= 0, sorted ascending
    std::vector<CallbackEntry> post; // altitude >  0, sorted ascending
};

static std::map<uint32_t, HeaderCallbacks> s_callbacks;
static CRITICAL_SECTION s_cs;
static bool s_csInitialized = false;
static bool s_initialized = false;
static bool s_hooked = false;

// Original handler table (saved before replacement)
static GameStoCHandler* s_originalHandlers = nullptr;
static GWArray<GameStoCHandler>* s_gameHandlers = nullptr;
static uint32_t s_handlerCount = 0;

// Our replacement handler dispatches to callbacks then calls original
static bool __cdecl StoCDispatcher(PacketBase* packet) {
    HookMarker::HookScope _hookScope(HookMarker::HookId::StoCDispatcher);
    if (!packet || !s_originalHandlers) return true;

    const uint32_t header = packet->header;
    if (header >= s_handlerCount) {
        // Out of range — call original if possible
        return true;
    }

    // Fire pre-processing callbacks (altitude <= 0)
    EnterCriticalSection(&s_cs);
    auto it = s_callbacks.find(header);
    bool blocked = false;
    if (it != s_callbacks.end()) {
        HookStatus status{false};
        for (auto& cb : it->second.pre) {
            cb.callback(&status, packet);
            if (status.blocked) { blocked = true; break; }
        }
    }
    LeaveCriticalSection(&s_cs);

    // Call original game handler (unless blocked)
    if (!blocked && s_originalHandlers[header].handler_func) {
        s_originalHandlers[header].handler_func(packet);
    }

    // Fire post-processing callbacks (altitude > 0)
    EnterCriticalSection(&s_cs);
    it = s_callbacks.find(header);
    if (it != s_callbacks.end()) {
        HookStatus status{false};
        for (auto& cb : it->second.post) {
            cb.callback(&status, packet);
            if (status.blocked) break;
        }
    }
    LeaveCriticalSection(&s_cs);

    return true;
}

// Must run on game thread — handler table isn't populated until GW is fully loaded
static void InitOnGameThread() {
    // Scan pattern: locates the GameServer** pointer
    // GWCA: "\x75\x04\x33\xC0\x5D\xC3\x8B\x41\x08\xA8\x01\x75" at -6
    uintptr_t address = Scanner::Find(
        "\x75\x04\x33\xC0\x5D\xC3\x8B\x41\x08\xA8\x01\x75",
        "xxxxxxxxxxxx", -6);

    if (!address || address < 0x10000) {
        Log::Warn("StoCMgr: GameServer scan pattern failed");
        return;
    }

    __try {
        uintptr_t serverPtrAddr = *reinterpret_cast<uintptr_t*>(address);
        if (serverPtrAddr < 0x10000) {
            Log::Warn("StoCMgr: GameServer deref failed (0x%08X)", serverPtrAddr);
            return;
        }

        GameServer** serverPtr = reinterpret_cast<GameServer**>(serverPtrAddr);
        if (!*serverPtr || reinterpret_cast<uintptr_t>(*serverPtr) < 0x10000) {
            Log::Warn("StoCMgr: GameServer null");
            return;
        }

        GameServerCodec* codec = (*serverPtr)->gs_codec;
        if (!codec || reinterpret_cast<uintptr_t>(codec) < 0x10000) {
            Log::Warn("StoCMgr: gs_codec null");
            return;
        }

        s_gameHandlers = &codec->handlers;
        s_handlerCount = s_gameHandlers->size;

        if (s_handlerCount == 0 || s_handlerCount > 0x300) {
            Log::Warn("StoCMgr: Handler count implausible (%u)", s_handlerCount);
            s_gameHandlers = nullptr;
            return;
        }

        Log::Info("StoCMgr: Found %u StoC handlers at 0x%08X",
                  s_handlerCount, reinterpret_cast<uintptr_t>(s_gameHandlers->buffer));

        // Save original handlers
        s_originalHandlers = new GameStoCHandler[s_handlerCount];
        for (uint32_t i = 0; i < s_handlerCount; ++i) {
            s_originalHandlers[i] = s_gameHandlers->buffer[i];
        }

        // Replace handlers that have registered callbacks
        EnterCriticalSection(&s_cs);
        for (auto& kv : s_callbacks) {
            if (kv.first < s_handlerCount) {
                s_gameHandlers->buffer[kv.first].handler_func = StoCDispatcher;
            }
        }
        LeaveCriticalSection(&s_cs);

        s_hooked = true;
        Log::Info("StoCMgr: Handler table hooked successfully");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Error("StoCMgr: Exception during handler table hook");
    }
}

bool Initialize() {
    if (s_initialized) return true;

    if (!s_csInitialized) {
        InitializeCriticalSection(&s_cs);
        s_csInitialized = true;
    }

    // Hook must run on game thread — handler array isn't populated at startup
    if (GameThread::IsInitialized()) {
        GameThread::Enqueue([]() { InitOnGameThread(); });
    } else {
        Log::Warn("StoCMgr: GameThread not ready, deferring hook installation");
    }

    s_initialized = true;
    Log::Info("StoCMgr: Initialized (hook deferred to game thread)");
    return true;
}

void Shutdown() {
    // Restore original handlers
    if (s_hooked && s_gameHandlers && s_originalHandlers) {
        for (uint32_t i = 0; i < s_handlerCount; ++i) {
            s_gameHandlers->buffer[i].handler_func = s_originalHandlers[i].handler_func;
        }
    }

    EnterCriticalSection(&s_cs);
    s_callbacks.clear();
    if (s_originalHandlers) {
        delete[] s_originalHandlers;
        s_originalHandlers = nullptr;
    }
    s_gameHandlers = nullptr;
    s_hooked = false;
    s_initialized = false;
    LeaveCriticalSection(&s_cs);

    if (s_csInitialized) {
        DeleteCriticalSection(&s_cs);
        s_csInitialized = false;
    }
}

bool RegisterPacketCallback(HookEntry* entry, uint32_t header,
                            const PacketCallback& callback, int altitude) {
    if (!entry || !callback) return false;

    EnterCriticalSection(&s_cs);

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

    // If hook is already installed, replace this header's handler now
    if (s_hooked && s_gameHandlers && header < s_handlerCount) {
        s_gameHandlers->buffer[header].handler_func = StoCDispatcher;
    }

    LeaveCriticalSection(&s_cs);
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
    EnterCriticalSection(&s_cs);
    auto it = s_callbacks.find(header);
    if (it != s_callbacks.end()) {
        RemoveFromList(it->second.pre, entry);
        RemoveFromList(it->second.post, entry);

        // If no callbacks left for this header, restore original handler
        if (it->second.pre.empty() && it->second.post.empty()) {
            if (s_hooked && s_gameHandlers && header < s_handlerCount && s_originalHandlers) {
                s_gameHandlers->buffer[header].handler_func = s_originalHandlers[header].handler_func;
            }
            s_callbacks.erase(it);
        }
    }
    LeaveCriticalSection(&s_cs);
}

void RemoveCallbacks(HookEntry* entry) {
    EnterCriticalSection(&s_cs);
    for (auto it = s_callbacks.begin(); it != s_callbacks.end(); ) {
        RemoveFromList(it->second.pre, entry);
        RemoveFromList(it->second.post, entry);
        if (it->second.pre.empty() && it->second.post.empty()) {
            uint32_t header = it->first;
            if (s_hooked && s_gameHandlers && header < s_handlerCount && s_originalHandlers) {
                s_gameHandlers->buffer[header].handler_func = s_originalHandlers[header].handler_func;
            }
            it = s_callbacks.erase(it);
        } else {
            ++it;
        }
    }
    LeaveCriticalSection(&s_cs);
}

bool EmulatePacket(PacketBase* packet) {
    if (!packet) return false;

    uint32_t header = packet->header;

    EnterCriticalSection(&s_cs);

    auto it = s_callbacks.find(header);
    if (it == s_callbacks.end()) {
        LeaveCriticalSection(&s_cs);
        return false;
    }

    HookStatus status{false};

    for (auto& cb : it->second.pre) {
        cb.callback(&status, packet);
        if (status.blocked) { LeaveCriticalSection(&s_cs); return true; }
    }

    for (auto& cb : it->second.post) {
        cb.callback(&status, packet);
        if (status.blocked) { LeaveCriticalSection(&s_cs); return true; }
    }

    LeaveCriticalSection(&s_cs);
    return true;
}

} // namespace GWA3::StoC


