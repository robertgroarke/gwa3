#include <gwa3/llm/EventPush.h>
#include <gwa3/llm/IpcServer.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/core/Log.h>

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace GWA3::LLM::EventPush {

    // --- StoC packet structures (from GWCA) ---
    #pragma pack(push, 1)

    struct StoC_InstanceLoadInfo {  // 0x0199 (409)
        uint32_t header;
        uint32_t agent_id;
        uint32_t map_id;
        uint32_t is_explorable;
        uint32_t district;
        uint32_t language;
        uint32_t is_observer;
    };

    struct StoC_MapLoaded {  // 0x00F2 (242)
        uint32_t header;
    };

    struct StoC_PartyDefeated {  // 0x01D8 (472)
        uint32_t header;
    };

    struct StoC_AgentState {  // 0x00F1 (241)
        uint32_t header;
        uint32_t agent_id;
        uint32_t state;  // bitmask: 16=dead, 1024=degen, 2048=hexed, etc.
    };

    struct StoC_SkillActivate {  // 0x00E4 (228)
        uint32_t header;
        uint32_t agent_id;
        uint32_t skill_id;
        uint32_t skill_instance;
    };

    struct StoC_ItemUpdateOwner {  // 0x0135 (309)
        uint32_t header;
        uint32_t item_id;
        uint32_t owner_agent_id;
        uint32_t seconds_reserved;
    };

    struct StoC_AgentAdd {  // 0x0020 (32)
        uint32_t header;
        uint32_t agent_id;
        // Many more fields — we only need agent_id
    };

    struct StoC_AgentRemove {  // 0x0021 (33)
        uint32_t header;
        uint32_t agent_id;
    };

    struct StoC_CinematicPlay {  // 0x0100 (256)
        uint32_t header;
        uint32_t play;
    };

    struct StoC_QuestAdd {  // 0x0049 (73)
        uint32_t header;
        uint32_t quest_id;
        // More fields follow but we only need quest_id
    };

    struct StoC_QuestRemove {  // 0x0051 (81)
        uint32_t header;
        uint32_t quest_id;
    };

    struct StoC_DungeonReward {  // 0x006C (108)
        uint32_t header;
        uint32_t experience;
        uint32_t gold;
        uint32_t skill_points;
    };

    #pragma pack(pop)

    // --- Hook entries ---
    static StoC::HookEntry g_hookInstanceLoad;
    static StoC::HookEntry g_hookMapLoaded;
    static StoC::HookEntry g_hookPartyDefeated;
    static StoC::HookEntry g_hookAgentState;
    static StoC::HookEntry g_hookSkillActivate;
    static StoC::HookEntry g_hookItemOwner;
    static StoC::HookEntry g_hookAgentAdd;
    static StoC::HookEntry g_hookAgentRemove;
    static StoC::HookEntry g_hookCinematic;
    static StoC::HookEntry g_hookQuestAdd;
    static StoC::HookEntry g_hookQuestRemove;
    static StoC::HookEntry g_hookDungeonReward;

    // --- Send helper ---
    static void SendEvent(const json& j) {
        if (!IpcServer::IsClientConnected()) return;
        std::string s = j.dump();
        IpcServer::Send(s.c_str(), static_cast<uint32_t>(s.size()));
    }

    // --- Callbacks ---

    static void OnInstanceLoad(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_InstanceLoadInfo*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = "instance_load";
        j["map_id"] = p->map_id;
        j["is_explorable"] = (p->is_explorable != 0);
        j["district"] = p->district;
        SendEvent(j);
    }

    static void OnMapLoaded(StoC::HookStatus*, StoC::PacketBase*) {
        json j;
        j["type"] = "event";
        j["event"] = "map_loaded";
        SendEvent(j);
    }

    static void OnPartyDefeated(StoC::HookStatus*, StoC::PacketBase*) {
        json j;
        j["type"] = "event";
        j["event"] = "party_defeated";
        SendEvent(j);
    }

    static void OnAgentState(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_AgentState*>(pkt);
        // Only push death events (bit 16 = dead)
        if (p->state & 16) {
            json j;
            j["type"] = "event";
            j["event"] = "agent_died";
            j["agent_id"] = p->agent_id;
            SendEvent(j);
        }
    }

    static void OnSkillActivate(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_SkillActivate*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = "skill_activated";
        j["agent_id"] = p->agent_id;
        j["skill_id"] = p->skill_id;
        SendEvent(j);
    }

    static void OnItemOwner(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_ItemUpdateOwner*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = "item_owner_changed";
        j["item_id"] = p->item_id;
        j["owner_agent_id"] = p->owner_agent_id;
        SendEvent(j);
    }

    static void OnAgentAdd(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_AgentAdd*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = "agent_spawned";
        j["agent_id"] = p->agent_id;
        SendEvent(j);
    }

    static void OnAgentRemove(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_AgentRemove*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = "agent_despawned";
        j["agent_id"] = p->agent_id;
        SendEvent(j);
    }

    static void OnCinematic(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_CinematicPlay*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = p->play ? "cinematic_start" : "cinematic_end";
        SendEvent(j);
    }

    static void OnQuestAdd(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_QuestAdd*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = "quest_added";
        j["quest_id"] = p->quest_id;
        SendEvent(j);
    }

    static void OnQuestRemove(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_QuestRemove*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = "quest_removed";
        j["quest_id"] = p->quest_id;
        SendEvent(j);
    }

    static void OnDungeonReward(StoC::HookStatus*, StoC::PacketBase* pkt) {
        auto* p = reinterpret_cast<StoC_DungeonReward*>(pkt);
        json j;
        j["type"] = "event";
        j["event"] = "dungeon_reward";
        j["experience"] = p->experience;
        j["gold"] = p->gold;
        j["skill_points"] = p->skill_points;
        SendEvent(j);
    }

    // --- Lifecycle ---

    bool Initialize() {
        bool ok = true;

        // Tier 1: Critical state changes
        ok &= StoC::RegisterPostPacketCallback(&g_hookInstanceLoad, 0x0199, OnInstanceLoad);
        ok &= StoC::RegisterPostPacketCallback(&g_hookMapLoaded, 0x00F2, OnMapLoaded);
        ok &= StoC::RegisterPostPacketCallback(&g_hookPartyDefeated, 0x01D8, OnPartyDefeated);
        ok &= StoC::RegisterPostPacketCallback(&g_hookAgentState, 0x00F1, OnAgentState);
        ok &= StoC::RegisterPostPacketCallback(&g_hookCinematic, 0x0100, OnCinematic);

        // Tier 2: Combat & items
        ok &= StoC::RegisterPostPacketCallback(&g_hookSkillActivate, 0x00E4, OnSkillActivate);
        ok &= StoC::RegisterPostPacketCallback(&g_hookItemOwner, 0x0135, OnItemOwner);
        ok &= StoC::RegisterPostPacketCallback(&g_hookAgentAdd, 0x0020, OnAgentAdd);
        ok &= StoC::RegisterPostPacketCallback(&g_hookAgentRemove, 0x0021, OnAgentRemove);

        // Tier 3: Quest & dungeon
        ok &= StoC::RegisterPostPacketCallback(&g_hookQuestAdd, 0x0049, OnQuestAdd);
        ok &= StoC::RegisterPostPacketCallback(&g_hookQuestRemove, 0x0051, OnQuestRemove);
        ok &= StoC::RegisterPostPacketCallback(&g_hookDungeonReward, 0x006C, OnDungeonReward);

        if (ok) {
            GWA3::Log::Info("[EventPush] Initialized — 12 StoC packet hooks active");
        } else {
            GWA3::Log::Warn("[EventPush] Some packet callbacks failed to register");
        }
        return ok;
    }

    void Shutdown() {
        StoC::RemoveCallbacks(&g_hookInstanceLoad);
        StoC::RemoveCallbacks(&g_hookMapLoaded);
        StoC::RemoveCallbacks(&g_hookPartyDefeated);
        StoC::RemoveCallbacks(&g_hookAgentState);
        StoC::RemoveCallbacks(&g_hookSkillActivate);
        StoC::RemoveCallbacks(&g_hookItemOwner);
        StoC::RemoveCallbacks(&g_hookAgentAdd);
        StoC::RemoveCallbacks(&g_hookAgentRemove);
        StoC::RemoveCallbacks(&g_hookCinematic);
        StoC::RemoveCallbacks(&g_hookQuestAdd);
        StoC::RemoveCallbacks(&g_hookQuestRemove);
        StoC::RemoveCallbacks(&g_hookDungeonReward);
        GWA3::Log::Info("[EventPush] Shutdown");
    }

} // namespace GWA3::LLM::EventPush
