#pragma once

#include <cstdint>

namespace GWA3::ChatLogMgr {

    // StoC packet headers for chat messages
    constexpr uint32_t SMSG_CHAT_MESSAGE_CORE   = 0x005D;  // 93 — message text
    constexpr uint32_t SMSG_CHAT_MESSAGE_SERVER  = 0x005E;  // 94 — system/server message
    constexpr uint32_t SMSG_CHAT_MESSAGE_NPC     = 0x005F;  // 95 — NPC speech
    constexpr uint32_t SMSG_CHAT_MESSAGE_GLOBAL  = 0x0060;  // 96 — guild/alliance chat
    constexpr uint32_t SMSG_CHAT_MESSAGE_LOCAL   = 0x0061;  // 97 — local instance chat

    // Chat channels (matches GWCA Chat::Channel)
    enum Channel : int {
        Alliance = 0,
        Allies   = 1,
        All      = 3,
        Emote    = 6,
        Warning  = 7,
        Guild    = 9,
        Global   = 10,
        Team     = 11,
        Trade    = 12,
        Advisory = 13,
        Whisper  = 14,
    };

    struct ChatEntry {
        uint32_t timestamp_ms;    // GetTickCount at receipt
        int      channel;         // Channel enum value
        char     channel_name[16]; // "all", "guild", "team", etc.
        wchar_t  message[256];    // message body (raw encoded)
        wchar_t  sender[64];      // sender name (decoded if available)
        uint32_t sender_agent_id; // for NPC messages
    };

    // Initialize: register StoC packet callbacks.
    // Must be called after StoC::Initialize().
    bool Initialize();

    void Shutdown();

    // Get the number of messages in the ring buffer.
    uint32_t GetMessageCount();

    // Get a message by index (0 = oldest in buffer, count-1 = newest).
    // Returns nullptr if out of range.
    const ChatEntry* GetMessage(uint32_t index);

    // Get messages newer than the given timestamp. Returns count written to outBuf.
    // outBuf must point to an array of at least maxEntries ChatEntry*.
    uint32_t GetMessagesSince(uint32_t timestampMs, const ChatEntry** outBuf, uint32_t maxEntries);

} // namespace GWA3::ChatLogMgr
