#include <gwa3/managers/ChatMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <cstring>

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

void WriteToChat(const wchar_t* message, uint32_t channel) {
    // TODO: implement via PostMessage offset when chat log hook is in place
    (void)message;
    (void)channel;
}

uint32_t GetPing() {
    if (!Offsets::Ping) return 0;
    return *reinterpret_cast<uint32_t*>(Offsets::Ping);
}

void SetRenderingEnabled(bool enabled) {
    // TODO: implement via Render hook offset
    (void)enabled;
}

} // namespace GWA3::ChatMgr
