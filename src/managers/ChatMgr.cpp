#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <MinHook.h>
#include <cstring>
#include <cstdio>

namespace GWA3::ChatMgr {

static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("ChatMgr: Initialized");
    return true;
}

// Native SendChat: void __cdecl (wchar_t* message, uint32_t agent_id)
typedef void(__cdecl* NativeSendChatFn)(wchar_t*, uint32_t);

void SendChat(const wchar_t* message, wchar_t channel) {
    if (!message || !message[0]) return;

    size_t len = wcslen(message);
    if (len == 0) return;

    // Build message with channel prefix
    wchar_t buf[142];
    buf[0] = channel;
    size_t copyLen = (len < 140) ? len : 140;
    memcpy(buf + 1, message, copyLen * sizeof(wchar_t));
    buf[1 + copyLen] = L'\0';

    // Prefer native SendChat function when resolved
    if (Offsets::SendChatFunc && Offsets::SendChatFunc > 0x10000) {
        auto fn = reinterpret_cast<NativeSendChatFn>(Offsets::SendChatFunc);
        fn(buf, 0);
        return;
    }

    // Fallback: raw packet send (limited to ~4 dwords of message)
    uint32_t dataLen = static_cast<uint32_t>((copyLen + 2) * sizeof(wchar_t));
    uint32_t dwordCount = (dataLen + 3) / 4;
    CtoS::SendPacket(1 + dwordCount, Packets::SEND_CHAT,
                     reinterpret_cast<uint32_t*>(buf)[0],
                     reinterpret_cast<uint32_t*>(buf)[1],
                     reinterpret_cast<uint32_t*>(buf)[2],
                     reinterpret_cast<uint32_t*>(buf)[3]);
}

// GWCA UIMessage::kWriteToChatLog = 0x1000007E
static constexpr uint32_t kWriteToChatLog = 0x1000007Eu;

// UIChatMessage struct passed to SendUIMessage for kWriteToChatLog
struct UIChatMessage {
    uint32_t channel;
    wchar_t* message;
    uint32_t channel2;
};

// Native AddToChatLog: void __cdecl (wchar_t* message, uint32_t channel)
typedef void(__cdecl* NativeAddToChatLogFn)(wchar_t*, uint32_t);

void WriteToChat(const wchar_t* message, uint32_t channel) {
    if (!message || !message[0]) return;

    // Wrap message in GW encoding tags: \x108\x107<text>\x1
    wchar_t encoded[300];
    int len = swprintf(encoded, 300, L"\x108\x107%s\x1", message);
    if (len <= 0) return;

    // Prefer native AddToChatLog when resolved (callable from game thread)
    if (Offsets::AddToChatLog && Offsets::AddToChatLog > 0x10000) {
        size_t byteLen = (static_cast<size_t>(len) + 1) * sizeof(wchar_t);
        wchar_t* heap = static_cast<wchar_t*>(malloc(byteLen));
        if (!heap) return;
        memcpy(heap, encoded, byteLen);

        GameThread::Enqueue([heap, channel]() {
            auto fn = reinterpret_cast<NativeAddToChatLogFn>(Offsets::AddToChatLog);
            fn(heap, channel);
            free(heap);
        });
        return;
    }

    // Fallback: UIMessage dispatch
    size_t byteLen = (static_cast<size_t>(len) + 1) * sizeof(wchar_t);
    wchar_t* heap = static_cast<wchar_t*>(malloc(byteLen));
    if (!heap) return;
    memcpy(heap, encoded, byteLen);

    GameThread::Enqueue([heap, channel]() {
        UIChatMessage param;
        param.channel = channel;
        param.message = heap;
        param.channel2 = channel;
        UIMgr::SendUIMessage(kWriteToChatLog, &param, nullptr);
        free(heap);
    });
}

uint32_t GetPing() {
    if (!Offsets::Ping) return 0;
    uint32_t ping = *reinterpret_cast<uint32_t*>(Offsets::Ping);
    return ping < 10 ? 10 : ping;
}

// ===== Render toggle via MinHook on GwEndScene =====
// Signature: bool __cdecl GwEndScene(void* ctx, void* unk)
typedef bool(__cdecl* GwEndSceneFn)(void*, void*);
static GwEndSceneFn s_originalEndScene = nullptr;
static bool s_renderHookInstalled = false;
static bool s_renderingDisabled = false;

static bool __cdecl RenderHookFn(void* ctx, void* unk) {
    if (s_renderingDisabled) {
        return true; // skip rendering this frame
    }
    return s_originalEndScene(ctx, unk);
}

static void InstallRenderHook() {
    if (s_renderHookInstalled) return;

    // Prefer GwEndScene (GWCA pattern) over Render (AutoIt hook pattern)
    uintptr_t target = 0;
    if (Offsets::GwEndScene && Offsets::GwEndScene > 0x10000) {
        target = Offsets::GwEndScene;
        Log::Info("ChatMgr: Using GwEndScene at 0x%08X for render hook", target);
    } else if (Offsets::Render && Offsets::Render > 0x10000) {
        target = Offsets::Render;
        Log::Info("ChatMgr: Using Render fallback at 0x%08X for render hook", target);
    } else {
        Log::Warn("ChatMgr: No render offset resolved, cannot install render hook");
        return;
    }

    MH_STATUS status = MH_CreateHook(
        reinterpret_cast<LPVOID>(target),
        reinterpret_cast<LPVOID>(&RenderHookFn),
        reinterpret_cast<LPVOID*>(&s_originalEndScene));
    if (status != MH_OK) {
        Log::Warn("ChatMgr: MH_CreateHook for render failed: %s", MH_StatusToString(status));
        return;
    }

    status = MH_EnableHook(reinterpret_cast<LPVOID>(target));
    if (status != MH_OK) {
        Log::Warn("ChatMgr: MH_EnableHook for render failed: %s", MH_StatusToString(status));
        return;
    }

    s_renderHookInstalled = true;
    Log::Info("ChatMgr: Render hook installed at 0x%08X", Offsets::Render);
}

void SetRenderingEnabled(bool enabled) {
    if (!s_renderHookInstalled) {
        InstallRenderHook();
    }
    if (!s_renderHookInstalled) return;
    s_renderingDisabled = !enabled;
}

} // namespace GWA3::ChatMgr
