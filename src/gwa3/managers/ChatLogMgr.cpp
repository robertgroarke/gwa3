#include <gwa3/managers/ChatLogMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/HookMarker.h>
#include <gwa3/core/HookMarker.h>

#include <MinHook.h>
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
    typedef void(__cdecl* WriteWhisperFn)(uint32_t, wchar_t*, wchar_t*);
    static WriteWhisperFn g_retWriteWhisper = nullptr;
    static uintptr_t g_writeWhisperAddr = 0;
    static bool g_whisperHookEnabled = false;

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

    static void PushMessage(int channel, const wchar_t* message, const wchar_t* sender, uint32_t senderAgentId) {
        if (!message || !message[0]) return;
        std::lock_guard<std::mutex> lock(g_mutex);

        ChatEntry& entry = g_ring[g_writeIndex % RING_SIZE];
        entry.timestamp_ms = GetTickCount();
        entry.channel = channel;
        strncpy_s(entry.channel_name, ChannelToName(channel), sizeof(entry.channel_name) - 1);
        wcsncpy_s(entry.message, message, 255);
        if (sender) {
            wcsncpy_s(entry.sender, sender, 63);
        } else {
            entry.sender[0] = L'\0';
        }
        entry.sender_agent_id = senderAgentId;

        g_writeIndex++;
        if (g_count < RING_SIZE) g_count++;
    }

    static void CommitMessage(int channel, const wchar_t* sender, uint32_t senderAgentId) {
        if (!g_hasPending) return;
        PushMessage(channel, g_pendingMessage, sender, senderAgentId);
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

    static void __cdecl OnWriteWhisper(uint32_t unk, wchar_t* from, wchar_t* msg) {
        HookMarker::HookScope _hookScope(HookMarker::HookId::WriteWhisperDetour);
        PushMessage(static_cast<int>(Whisper), msg, from, 0);
        if (g_retWriteWhisper) {
            g_retWriteWhisper(unk, from, msg);
        }
    }

    bool Initialize() {
        bool ok = true;
        ok &= StoC::RegisterPostPacketCallback(&g_hookCore, SMSG_CHAT_MESSAGE_CORE, OnMessageCore);
        ok &= StoC::RegisterPostPacketCallback(&g_hookServer, SMSG_CHAT_MESSAGE_SERVER, OnMessageServer);
        ok &= StoC::RegisterPostPacketCallback(&g_hookNPC, SMSG_CHAT_MESSAGE_NPC, OnMessageNPC);
        ok &= StoC::RegisterPostPacketCallback(&g_hookGlobal, SMSG_CHAT_MESSAGE_GLOBAL, OnMessageGlobal);
        ok &= StoC::RegisterPostPacketCallback(&g_hookLocal, SMSG_CHAT_MESSAGE_LOCAL, OnMessageLocal);

        if (!g_whisperHookEnabled) {
            g_writeWhisperAddr = Scanner::Find("\x83\xC4\x04\x8D\x58\x2E", "xxxxxx", -0x18);
            if (g_writeWhisperAddr > 0x10000) {
                const MH_STATUS createStatus = MH_CreateHook(
                    reinterpret_cast<void*>(g_writeWhisperAddr),
                    reinterpret_cast<void*>(&OnWriteWhisper),
                    reinterpret_cast<void**>(&g_retWriteWhisper));
                if (createStatus == MH_OK) {
                    const MH_STATUS enableStatus = MH_EnableHook(reinterpret_cast<void*>(g_writeWhisperAddr));
                    if (enableStatus == MH_OK) {
                        g_whisperHookEnabled = true;
                        GWA3::Log::Info("[ChatLogMgr] Whisper hook enabled at 0x%08X", static_cast<unsigned>(g_writeWhisperAddr));
                    } else {
                        GWA3::Log::Warn("[ChatLogMgr] MH_EnableHook(WriteWhisper) failed: %s", MH_StatusToString(enableStatus));
                    }
                } else {
                    GWA3::Log::Warn("[ChatLogMgr] MH_CreateHook(WriteWhisper) failed: %s", MH_StatusToString(createStatus));
                }
            } else {
                GWA3::Log::Warn("[ChatLogMgr] WriteWhisper scan failed");
            }
        }

        if (ok) {
            GWA3::Log::Info("[ChatLogMgr] Initialized — capturing chat on 5 packet types plus whisper hook");
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
        if (g_whisperHookEnabled && g_writeWhisperAddr > 0x10000) {
            MH_DisableHook(reinterpret_cast<void*>(g_writeWhisperAddr));
            MH_RemoveHook(reinterpret_cast<void*>(g_writeWhisperAddr));
            g_whisperHookEnabled = false;
        }
        GWA3::Log::Info("[ChatLogMgr] Shutdown");
    }

    uint32_t GetMessageCount() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_count;
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


