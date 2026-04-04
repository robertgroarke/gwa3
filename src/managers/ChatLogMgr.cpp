#include <gwa3/managers/ChatLogMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <mutex>
#include <cstring>

namespace GWA3::ChatLogMgr {

    static constexpr uint32_t RING_SIZE = 100;  // keep last 100 messages

    // StoC packet structures
    #pragma pack(push, 1)
    struct StoC_MessageCore {
        uint32_t header;       // 0x005D
        wchar_t  message[122];
    };

    struct StoC_MessageServer {
        uint32_t header;       // 0x005E
        uint32_t id;
        uint32_t channel;
    };

    struct StoC_MessageNPC {
        uint32_t header;       // 0x005F
        uint32_t agent_id;
        uint32_t channel;
        wchar_t  sender_name[8];
    };

    struct StoC_MessageGlobal {
        uint32_t header;       // 0x0060
        uint32_t channel;
        wchar_t  sender_name[32];
        wchar_t  sender_guild[6];
    };

    struct StoC_MessageLocal {
        uint32_t header;       // 0x0061
        uint32_t player_number;
        uint32_t channel;
    };
    #pragma pack(pop)

    static std::mutex g_mutex;
    static ChatEntry g_ring[RING_SIZE] = {};
    static uint32_t g_writeIndex = 0;  // next slot to write
    static uint32_t g_count = 0;       // total messages written (wraps)

    // Pending message core — arrives before the channel/sender packet
    static wchar_t g_pendingMessage[256] = {};
    static bool g_hasPending = false;

    static StoC::HookEntry g_hookCore;
    static StoC::HookEntry g_hookServer;
    static StoC::HookEntry g_hookNPC;
    static StoC::HookEntry g_hookGlobal;
    static StoC::HookEntry g_hookLocal;

    static const char* ChannelToName(int ch) {
        switch (ch) {
            case Alliance: return "alliance";
            case Allies:   return "allies";
            case All:      return "all";
            case Emote:    return "emote";
            case Warning:  return "warning";
            case Guild:    return "guild";
            case Global:   return "global";
            case Team:     return "team";
            case Trade:    return "trade";
            case Advisory: return "advisory";
            case Whisper:  return "whisper";
            default:       return "unknown";
        }
    }

    static void CommitMessage(int channel, const wchar_t* sender, uint32_t senderAgentId) {
        if (!g_hasPending) return;
        std::lock_guard<std::mutex> lock(g_mutex);

        ChatEntry& entry = g_ring[g_writeIndex % RING_SIZE];
        entry.timestamp_ms = GetTickCount();
        entry.channel = channel;
        strncpy_s(entry.channel_name, ChannelToName(channel), sizeof(entry.channel_name) - 1);
        wcsncpy_s(entry.message, g_pendingMessage, 255);
        if (sender) {
            wcsncpy_s(entry.sender, sender, 63);
        } else {
            entry.sender[0] = L'\0';
        }
        entry.sender_agent_id = senderAgentId;

        g_writeIndex++;
        if (g_count < RING_SIZE) g_count++;
        g_hasPending = false;
    }

    static void OnMessageCore(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_MessageCore*>(packet);
        // Store the message text, waiting for the channel/sender packet
        wcsncpy_s(g_pendingMessage, p->message, 255);
        g_hasPending = true;
    }

    static void OnMessageServer(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_MessageServer*>(packet);
        CommitMessage(static_cast<int>(p->channel), L"[Server]", 0);
    }

    static void OnMessageNPC(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_MessageNPC*>(packet);
        CommitMessage(static_cast<int>(p->channel), p->sender_name, p->agent_id);
    }

    static void OnMessageGlobal(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_MessageGlobal*>(packet);
        CommitMessage(static_cast<int>(p->channel), p->sender_name, 0);
    }

    static void OnMessageLocal(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_MessageLocal*>(packet);
        // Local messages have player_number, not a name string.
        // Store player_number in a temp buffer — the LLM can cross-reference.
        wchar_t nameBuf[64] = {};
        swprintf_s(nameBuf, L"Player#%u", p->player_number);
        CommitMessage(static_cast<int>(p->channel), nameBuf, 0);
    }

    bool Initialize() {
        bool ok = true;
        ok &= StoC::RegisterPostPacketCallback(&g_hookCore, SMSG_CHAT_MESSAGE_CORE, OnMessageCore);
        ok &= StoC::RegisterPostPacketCallback(&g_hookServer, SMSG_CHAT_MESSAGE_SERVER, OnMessageServer);
        ok &= StoC::RegisterPostPacketCallback(&g_hookNPC, SMSG_CHAT_MESSAGE_NPC, OnMessageNPC);
        ok &= StoC::RegisterPostPacketCallback(&g_hookGlobal, SMSG_CHAT_MESSAGE_GLOBAL, OnMessageGlobal);
        ok &= StoC::RegisterPostPacketCallback(&g_hookLocal, SMSG_CHAT_MESSAGE_LOCAL, OnMessageLocal);

        if (ok) {
            GWA3::Log::Info("[ChatLogMgr] Initialized — capturing chat on 5 packet types");
        } else {
            GWA3::Log::Warn("[ChatLogMgr] Some packet callbacks failed to register");
        }
        return ok;
    }

    void Shutdown() {
        StoC::RemoveCallbacks(&g_hookCore);
        StoC::RemoveCallbacks(&g_hookServer);
        StoC::RemoveCallbacks(&g_hookNPC);
        StoC::RemoveCallbacks(&g_hookGlobal);
        StoC::RemoveCallbacks(&g_hookLocal);
        GWA3::Log::Info("[ChatLogMgr] Shutdown");
    }

    uint32_t GetMessageCount() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_count;
    }

    const ChatEntry* GetMessage(uint32_t index) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (index >= g_count) return nullptr;
        // Ring buffer: oldest message is at (writeIndex - count), newest at (writeIndex - 1)
        uint32_t ringIdx = (g_writeIndex - g_count + index) % RING_SIZE;
        return &g_ring[ringIdx];
    }

    uint32_t GetMessagesSince(uint32_t timestampMs, const ChatEntry** outBuf, uint32_t maxEntries) {
        std::lock_guard<std::mutex> lock(g_mutex);
        uint32_t written = 0;
        for (uint32_t i = 0; i < g_count && written < maxEntries; i++) {
            uint32_t ringIdx = (g_writeIndex - g_count + i) % RING_SIZE;
            if (g_ring[ringIdx].timestamp_ms > timestampMs) {
                outBuf[written++] = &g_ring[ringIdx];
            }
        }
        return written;
    }

} // namespace GWA3::ChatLogMgr
