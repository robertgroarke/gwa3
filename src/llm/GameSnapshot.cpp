#include <gwa3/llm/GameSnapshot.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/Skill.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/Effect.h>

#include <Windows.h>
#include <nlohmann/json.hpp>
#include <cmath>

using json = nlohmann::json;

namespace GWA3::LLM::GameSnapshot {

    static uint32_t g_tick = 0;

    // Helper: convert json to heap-allocated char*
    static char* JsonToHeap(const json& j, uint32_t* outLength) {
        std::string s = j.dump();
        *outLength = static_cast<uint32_t>(s.size());
        char* buf = new (std::nothrow) char[*outLength + 1];
        if (!buf) { *outLength = 0; return nullptr; }
        memcpy(buf, s.c_str(), *outLength + 1);
        return buf;
    }

    // Build player ("me") object
    static json BuildPlayerJson() {
        json me;
        auto* agent = AgentMgr::GetMyAgent();
        if (!agent) {
            me["agent_id"] = 0;
            return me;
        }

        me["agent_id"] = agent->agent_id;
        me["x"] = agent->x;
        me["y"] = agent->y;
        me["hp"] = agent->hp;
        me["max_hp"] = agent->max_hp;
        me["energy"] = agent->energy;
        me["max_energy"] = agent->max_energy;
        me["primary"] = agent->primary;
        me["secondary"] = agent->secondary;
        me["level"] = agent->level;
        me["target_id"] = AgentMgr::GetTargetId();
        me["effects"] = agent->effects;
        me["allegiance"] = agent->allegiance;

        // Derive movement/casting state from model_state and skill fields
        me["is_moving"] = (agent->move_x != 0.0f || agent->move_y != 0.0f);
        me["is_casting"] = (agent->skill != 0);
        me["skill_casting"] = static_cast<uint32_t>(agent->skill);
        me["model_state"] = agent->model_state;

        return me;
    }

    // Build skillbar array from a Skillbar pointer (works for player and heroes)
    static json BuildSkillbarFromBar(Skillbar* bar) {
        json skills = json::array();
        if (!bar) return skills;

        for (int i = 0; i < 8; i++) {
            json sk;
            sk["slot"] = i;
            uint32_t skillId = bar->skills[i].skill_id;
            sk["skill_id"] = skillId;
            sk["recharge"] = bar->skills[i].recharge;
            sk["adrenaline"] = bar->skills[i].adrenaline_a;
            sk["event"] = bar->skills[i].event;
            if (skillId != 0) {
                const auto* data = SkillMgr::GetSkillConstantData(skillId);
                if (data) {
                    sk["profession"] = data->profession;
                    sk["attribute"] = data->attribute;
                    sk["type"] = data->type;
                    sk["energy_cost"] = data->energy_cost;
                    sk["activation"] = data->activation;
                    sk["aftercast"] = data->aftercast;
                    sk["recharge_time"] = data->recharge;
                    sk["aoe_range"] = data->aoe_range;
                }
            }
            skills.push_back(sk);
        }
        return skills;
    }

    // Build player skillbar
    static json BuildSkillbarJson() {
        return BuildSkillbarFromBar(SkillMgr::GetPlayerSkillbar());
    }

    // Build hero skillbars array (one entry per hero in party)
    static json BuildHeroSkillbarsJson() {
        json heroes = json::array();
        auto* agentArray = AgentMgr::GetAgentArray();
        if (!agentArray || !agentArray->buffer) return heroes;

        uint32_t myId = AgentMgr::GetMyId();
        uint32_t count = agentArray->size;

        for (uint32_t i = 0; i < count; i++) {
            auto* agent = agentArray->buffer[i];
            if (!agent || agent->agent_id == myId) continue;
            if (agent->type != 0xDB) continue;

            auto* living = reinterpret_cast<AgentLiving*>(agent);
            // Heroes are allegiance 1 (ally) and have a skillbar in the array
            if (living->allegiance != 1) continue;

            auto* bar = SkillMgr::GetSkillbarByAgentId(living->agent_id);
            if (!bar) continue; // not a hero (henchmen don't have skillbars we can read)

            json h;
            h["agent_id"] = living->agent_id;
            h["hp"] = living->hp;
            h["energy"] = living->energy;
            h["primary"] = living->primary;
            h["secondary"] = living->secondary;
            h["level"] = living->level;

            uint32_t castingSkillId = static_cast<uint32_t>(living->skill);
            h["is_casting"] = (castingSkillId != 0);
            h["casting_skill_id"] = castingSkillId;

            h["skillbar"] = BuildSkillbarFromBar(bar);
            heroes.push_back(h);
        }
        return heroes;
    }

    // Build map info
    static json BuildMapJson() {
        json m;
        m["map_id"] = MapMgr::GetMapId();
        m["is_loaded"] = MapMgr::GetIsMapLoaded();
        m["instance_time"] = MapMgr::GetInstanceTime();
        m["region"] = MapMgr::GetRegion();
        m["district"] = MapMgr::GetDistrict();
        m["is_cinematic"] = MapMgr::GetIsInCinematic();

        // Determine hard mode from area info if available
        const auto* area = MapMgr::GetAreaInfo(MapMgr::GetMapId());
        if (area) {
            m["map_type"] = area->type;
            m["campaign"] = area->campaign;
        }
        return m;
    }

    // Build party basics
    static json BuildPartyBasicsJson() {
        json p;
        p["is_defeated"] = PartyMgr::GetIsPartyDefeated();
        return p;
    }

    // Build nearby agents array (within range)
    static json BuildNearbyAgentsJson(float maxRange = 2500.0f) {
        json agents = json::array();
        auto* me = AgentMgr::GetMyAgent();
        if (!me) return agents;

        float myX = me->x;
        float myY = me->y;

        auto* agentArray = AgentMgr::GetAgentArray();
        if (!agentArray || !agentArray->buffer) return agents;

        uint32_t count = agentArray->size;
        for (uint32_t i = 0; i < count; i++) {
            auto* agent = agentArray->buffer[i];
            if (!agent || agent->agent_id == me->agent_id) continue;

            float dist = AgentMgr::GetDistance(myX, myY, agent->x, agent->y);
            if (dist > maxRange) continue;

            json a;
            a["id"] = agent->agent_id;
            a["x"] = agent->x;
            a["y"] = agent->y;
            a["distance"] = dist;
            a["type"] = agent->type;

            // Check if it's a living agent (type has 0xDB flag pattern)
            // We detect living by checking if the pointer can be cast safely
            // by looking at the type field
            if (agent->type == 0xDB) {
                auto* living = reinterpret_cast<AgentLiving*>(agent);
                a["agent_type"] = "living";
                a["hp"] = living->hp;
                a["max_hp"] = living->max_hp;
                a["energy"] = living->energy;
                a["max_energy"] = living->max_energy;
                a["allegiance"] = living->allegiance;
                a["primary"] = living->primary;
                a["secondary"] = living->secondary;
                a["level"] = living->level;
                a["is_alive"] = (living->hp > 0.0f);
                a["effects"] = living->effects;
                a["weapon_type"] = living->weapon_type;
                a["model_state"] = living->model_state;

                // Casting state: skill field is nonzero while casting
                uint32_t castingSkillId = static_cast<uint32_t>(living->skill);
                a["is_casting"] = (castingSkillId != 0);
                a["casting_skill_id"] = castingSkillId;
                if (castingSkillId != 0) {
                    const auto* skillData = SkillMgr::GetSkillConstantData(castingSkillId);
                    if (skillData) {
                        a["casting_skill_type"] = skillData->type;
                        a["casting_skill_activation"] = skillData->activation;
                        a["casting_skill_profession"] = skillData->profession;
                    }
                }
            } else if (agent->type == 0x200) {
                a["agent_type"] = "gadget";
            } else if (agent->type == 0x400) {
                auto* item = reinterpret_cast<AgentItem*>(agent);
                a["agent_type"] = "item";
                a["item_id"] = item->item_id;
                a["owner"] = item->owner;
                // Cross-reference with ItemMgr for details
                auto* itemData = ItemMgr::GetItemById(item->item_id);
                if (itemData) {
                    a["model_id"] = itemData->model_id;
                    a["item_type"] = itemData->type;
                    a["quantity"] = itemData->quantity;
                    a["value"] = itemData->value;
                    a["interaction"] = itemData->interaction;
                }
            } else {
                a["agent_type"] = "unknown";
            }

            agents.push_back(a);
        }

        return agents;
    }

    // Serialize a single bag to JSON
    static json SerializeBag(int bagIndex) {
        json b;
        auto* bag = ItemMgr::GetBag(bagIndex);
        if (!bag) return b;

        b["bag_index"] = bagIndex;
        b["item_count"] = bag->items_count;

        json items = json::array();
        if (bag->items.buffer && bag->items.size > 0) {
            for (uint32_t s = 0; s < bag->items.size; s++) {
                auto* item = bag->items.buffer[s];
                if (!item) continue;

                json it;
                it["item_id"] = item->item_id;
                it["model_id"] = item->model_id;
                it["type"] = item->type;
                it["quantity"] = item->quantity;
                it["value"] = item->value;
                it["slot"] = item->slot;
                it["equipped"] = item->equipped;
                it["interaction"] = item->interaction;
                items.push_back(it);
            }
        }
        b["items"] = items;
        return b;
    }

    // Build inventory snapshot (backpack bags 1-4)
    static json BuildInventoryJson() {
        json inv;
        auto* inventory = ItemMgr::GetInventory();
        if (!inventory) return inv;

        inv["gold_character"] = inventory->gold_character;
        inv["gold_storage"] = inventory->gold_storage;

        json bags = json::array();
        for (int i = 1; i <= 4; i++) {
            json b = SerializeBag(i);
            if (!b.is_null()) bags.push_back(b);
        }
        inv["bags"] = bags;
        return inv;
    }

    // Build Xunlai storage snapshot (bags 8-16 are storage panes)
    static json BuildStorageJson() {
        json storage = json::array();
        // Storage panes: bag indices 8-16 (up to 9 panes, depending on unlocks)
        for (int i = 8; i <= 16; i++) {
            json b = SerializeBag(i);
            if (!b.is_null() && b.contains("item_count") && b["item_count"] > 0) {
                storage.push_back(b);
            }
        }
        return storage;
    }

    // Build current dialog state
    static json BuildDialogJson() {
        json d;
        if (!DialogMgr::IsDialogOpen()) {
            d["is_open"] = false;
            return d;
        }

        d["is_open"] = true;
        d["sender_agent_id"] = DialogMgr::GetDialogSenderAgentId();

        // Dialog body text (raw encoded — may contain <a=ID>label</a> tags)
        const wchar_t* bodyRaw = DialogMgr::GetDialogBodyRaw();
        if (bodyRaw && bodyRaw[0]) {
            // Convert wchar_t to UTF-8 for JSON
            char bodyUtf8[512] = {};
            WideCharToMultiByte(CP_UTF8, 0, bodyRaw, -1, bodyUtf8, sizeof(bodyUtf8) - 1, nullptr, nullptr);
            d["body_raw"] = bodyUtf8;
        }

        // Dialog buttons
        uint32_t btnCount = DialogMgr::GetButtonCount();
        json buttons = json::array();
        for (uint32_t i = 0; i < btnCount; i++) {
            const auto* btn = DialogMgr::GetButton(i);
            if (!btn) continue;
            json b;
            b["dialog_id"] = btn->dialog_id;
            b["icon"] = btn->button_icon;
            // Convert button label to UTF-8
            char labelUtf8[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, btn->label, -1, labelUtf8, sizeof(labelUtf8) - 1, nullptr, nullptr);
            b["label"] = labelUtf8;
            if (btn->skill_id != 0xFFFFFFFF) {
                b["skill_id"] = btn->skill_id;
            }
            buttons.push_back(b);
        }
        d["buttons"] = buttons;
        return d;
    }

    // Build effects for the player
    static json BuildPlayerEffectsJson() {
        json effs = json::array();
        auto* myAgent = AgentMgr::GetMyAgent();
        if (!myAgent) return effs;

        auto* effectArray = EffectMgr::GetAgentEffectArray(myAgent->agent_id);
        if (effectArray && effectArray->buffer) {
            for (uint32_t i = 0; i < effectArray->size; i++) {
                auto& eff = effectArray->buffer[i];
                if (eff.skill_id == 0) continue;
                json e;
                e["skill_id"] = eff.skill_id;
                e["attribute_level"] = eff.attribute_level;
                e["duration"] = eff.duration;
                e["agent_id"] = eff.agent_id;
                effs.push_back(e);
            }
        }
        return effs;
    }

    char* SerializeTier1(uint32_t* outLength) {
        g_tick++;
        json j;
        j["type"] = "snapshot";
        j["tier"] = 1;
        j["tick"] = g_tick;
        j["me"] = BuildPlayerJson();
        j["skillbar"] = BuildSkillbarJson();
        j["map"] = BuildMapJson();
        j["party"] = BuildPartyBasicsJson();
        return JsonToHeap(j, outLength);
    }

    char* SerializeTier2(uint32_t* outLength) {
        g_tick++;
        json j;
        j["type"] = "snapshot";
        j["tier"] = 2;
        j["tick"] = g_tick;
        j["me"] = BuildPlayerJson();
        j["skillbar"] = BuildSkillbarJson();
        j["map"] = BuildMapJson();
        j["party"] = BuildPartyBasicsJson();
        j["agents"] = BuildNearbyAgentsJson();
        j["heroes"] = BuildHeroSkillbarsJson();
        j["dialog"] = BuildDialogJson();
        return JsonToHeap(j, outLength);
    }

    char* SerializeTier3(uint32_t* outLength) {
        g_tick++;
        json j;
        j["type"] = "snapshot";
        j["tier"] = 3;
        j["tick"] = g_tick;
        j["me"] = BuildPlayerJson();
        j["skillbar"] = BuildSkillbarJson();
        j["map"] = BuildMapJson();
        j["party"] = BuildPartyBasicsJson();
        j["agents"] = BuildNearbyAgentsJson();
        j["heroes"] = BuildHeroSkillbarsJson();
        j["dialog"] = BuildDialogJson();
        j["inventory"] = BuildInventoryJson();
        j["storage"] = BuildStorageJson();
        j["effects"] = BuildPlayerEffectsJson();
        return JsonToHeap(j, outLength);
    }

} // namespace GWA3::LLM::GameSnapshot
