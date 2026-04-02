#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

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

void SendChat(const wchar_t* message, wchar_t channel) {
    if (!message) return;

    // Chat packets: header SEND_CHAT, then channel prefix + wchar message
    // The game expects: SendPacket(size, SEND_CHAT, <wchar_t data...>)
    // For simplicity, use the packet with the channel prefix built into message
    size_t len = wcslen(message);
    if (len == 0) return;

    // Build message with channel prefix
    wchar_t buf[142]; // max chat length ~140 chars
    buf[0] = channel;
    size_t copyLen = (len < 140) ? len : 140;
    memcpy(buf + 1, message, copyLen * sizeof(wchar_t));
    buf[1 + copyLen] = L'\0';

    // Send as raw dwords: SEND_CHAT header + wchar data packed into dwords
    uint32_t dataLen = static_cast<uint32_t>((copyLen + 2) * sizeof(wchar_t)); // +1 prefix +1 null
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

void WriteToChat(const wchar_t* message, uint32_t channel) {
    if (!message || !message[0]) return;

    // Wrap message in GW encoding tags: \x108\x107<text>\x1
    // Copy into a heap-allocated buffer so it survives until the game thread runs.
    wchar_t encoded[300];
    int len = swprintf(encoded, 300, L"\x108\x107%s\x1", message);
    if (len <= 0) return;

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

void SetRenderingEnabled(bool enabled) {
    // TODO: implement via Render hook offset
    (void)enabled;
}

} // namespace GWA3::ChatMgr
