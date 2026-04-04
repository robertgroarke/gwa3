#include <gwa3/llm/GameSnapshot.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/Offsets.h>
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
        uint32_t maxAgents = AgentMgr::GetMaxAgents();
        if (maxAgents == 0) return heroes;

        uint32_t myId = AgentMgr::GetMyId();
        uint32_t count = maxAgents;

        for (uint32_t i = 0; i < count; i++) {
            auto* agent = AgentMgr::GetAgentByID(i);
            if (!agent || agent->agent_id == myId || agent->agent_id == 0) continue;
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

    // Build party with per-member status
    static json BuildPartyBasicsJson() {
        json p;
        p["is_defeated"] = PartyMgr::GetIsPartyDefeated();

        // Build party member list from agent array (allies near us)
        auto* me = AgentMgr::GetMyAgent();
        uint32_t maxAgents = AgentMgr::GetMaxAgents();
        if (me && maxAgents > 0) {
            json members = json::array();
            uint32_t partySize = 0;
            uint32_t deadCount = 0;
            for (uint32_t i = 0; i < maxAgents; i++) {
                auto* agent = AgentMgr::GetAgentByID(i);
                if (!agent || agent->type != 0xDB) continue;
                auto* living = reinterpret_cast<AgentLiving*>(agent);
                if (living->allegiance != 1) continue; // allies only

                json m;
                m["agent_id"] = living->agent_id;
                m["hp"] = living->hp;
                m["energy"] = living->energy;
                m["primary"] = living->primary;
                m["level"] = living->level;
                bool alive = living->hp > 0.0f;
                m["is_alive"] = alive;
                m["is_player"] = (living->agent_id == me->agent_id);
                // Has a skillbar = hero; no skillbar = henchman
                bool isHero = (SkillMgr::GetSkillbarByAgentId(living->agent_id) != nullptr);
                m["is_hero"] = isHero;
                members.push_back(m);
                partySize++;
                if (!alive) deadCount++;
            }
            // Include self
            {
                json m;
                m["agent_id"] = me->agent_id;
                m["hp"] = me->hp;
                m["energy"] = me->energy;
                m["primary"] = me->primary;
                m["level"] = me->level;
                m["is_alive"] = (me->hp > 0.0f);
                m["is_player"] = true;
                m["is_hero"] = false;
                members.push_back(m);
                partySize++;
                if (me->hp <= 0.0f) deadCount++;
            }
            p["members"] = members;
            p["size"] = partySize;
            p["dead_count"] = deadCount;
        }
        return p;
    }

    // Build nearby agents array (within range)
    static json BuildNearbyAgentsJson(float maxRange = 2500.0f) {
        json agents = json::array();
        auto* me = AgentMgr::GetMyAgent();
        if (!me) return agents;

        float myX = me->x;
        float myY = me->y;

        uint32_t maxAgents = AgentMgr::GetMaxAgents();
        if (maxAgents == 0) return agents;

        uint32_t count = maxAgents;
        for (uint32_t i = 0; i < count; i++) {
            auto* agent = AgentMgr::GetAgentByID(i);
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
                a["hex"] = living->hex;
                a["player_number"] = living->player_number;

                // Casting state
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

                // Per-agent buffs and effects (hex/enchant detection)
                auto* agentEffects = EffectMgr::GetAgentEffects(living->agent_id);
                if (agentEffects) {
                    bool hasHex = false;
                    bool hasEnchant = false;
                    json activeEffects = json::array();
                    if (agentEffects->effects.buffer) {
                        for (uint32_t ei = 0; ei < agentEffects->effects.size; ei++) {
                            auto& eff = agentEffects->effects.buffer[ei];
                            if (eff.skill_id == 0) continue;
                            json e;
                            e["skill_id"] = eff.skill_id;
                            e["duration"] = eff.duration;
                            e["timestamp"] = eff.timestamp;
                            const auto* sd = SkillMgr::GetSkillConstantData(eff.skill_id);
                            if (sd) {
                                e["type"] = sd->type;
                                if (sd->type == 1) hasHex = true;        // Hex
                                if (sd->type == 3 || sd->type == 16) hasEnchant = true; // Enchantment/Flash Enchantment
                            }
                            activeEffects.push_back(e);
                        }
                    }
                    a["has_hex"] = hasHex;
                    a["has_enchantment"] = hasEnchant;
                    if (!activeEffects.empty()) {
                        a["active_effects"] = activeEffects;
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
                // Rarity from interaction flags
                uint32_t inter = item->interaction;
                const char* rarity = "white";
                if (inter == 2627) rarity = "green";
                else if (inter == 2624) rarity = "gold";
                else if (inter == 2626) rarity = "purple";
                else if (inter == 2623) rarity = "blue";
                else if (inter == 2622) rarity = "gray";
                it["rarity"] = rarity;
                items.push_back(it);
            }
        }
        b["items"] = items;
        return b;
    }

    // Build inventory snapshot (backpack bags 1-4) with free slot count
    static json BuildInventoryJson() {
        json inv;
        auto* inventory = ItemMgr::GetInventory();
        if (!inventory) return inv;

        inv["gold_character"] = inventory->gold_character;
        inv["gold_storage"] = inventory->gold_storage;

        json bags = json::array();
        uint32_t totalFreeSlots = 0;
        for (int i = 1; i <= 4; i++) {
            auto* bag = ItemMgr::GetBag(i);
            if (!bag) continue;
            json b = SerializeBag(i);
            if (!b.is_null()) {
                // Count free slots in this bag
                uint32_t capacity = bag->items.size;  // allocated slot count
                uint32_t used = bag->items_count;
                uint32_t free = (capacity > used) ? (capacity - used) : 0;
                b["free_slots"] = free;
                totalFreeSlots += free;
                bags.push_back(b);
            }
        }
        inv["bags"] = bags;
        inv["free_slots_total"] = totalFreeSlots;
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
        wchar_t bodyRaw[256] = {}; DialogMgr::GetDialogBodyRaw(bodyRaw, 256);
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
            DialogMgr::DialogButton btn;
            if (!DialogMgr::GetButton(i, btn)) continue;
            json b;
            b["dialog_id"] = btn.dialog_id;
            b["icon"] = btn.button_icon;
            // Convert button label to UTF-8
            char labelUtf8[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, btn.label, -1, labelUtf8, sizeof(labelUtf8) - 1, nullptr, nullptr);
            b["label"] = labelUtf8;
            if (btn.skill_id != 0xFFFFFFFF) {
                b["skill_id"] = btn.skill_id;
            }
            buttons.push_back(b);
        }
        d["buttons"] = buttons;
        return d;
    }

    // SEH-safe helper: read merchant item IDs into a flat buffer.
    // Returns count of valid IDs written (0 on failure).
    static uint32_t ReadMerchantItemIds(uint32_t* outIds, uint32_t maxIds) {
        if (Offsets::BasePointer <= 0x10000) return 0;
        __try {
            uintptr_t p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
            if (p0 <= 0x10000) return 0;
            uintptr_t p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
            if (p1 <= 0x10000) return 0;
            uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
            if (p2 <= 0x10000) return 0;
            uintptr_t base = *reinterpret_cast<uintptr_t*>(p2 + 0x24);
            uint32_t size = *reinterpret_cast<uint32_t*>(p2 + 0x28);
            if (base <= 0x10000 || size == 0 || size > maxIds) return 0;
            for (uint32_t i = 0; i < size; i++) {
                outIds[i] = *reinterpret_cast<uint32_t*>(base + i * 4);
            }
            return size;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    // Build merchant/trader window state
    static json BuildMerchantJson() {
        json m;
        uint32_t itemCount = TradeMgr::GetMerchantItemCount();
        m["is_open"] = (itemCount > 0);
        if (itemCount == 0) return m;

        m["item_count"] = itemCount;

        // Last quote from TraderHook
        uint32_t quoteId = TraderHook::GetQuoteId();
        uint32_t costItemId = TraderHook::GetCostItemId();
        uint32_t costValue = TraderHook::GetCostValue();
        if (quoteId > 0 && costValue > 0) {
            json quote;
            quote["quote_id"] = quoteId;
            quote["cost_item_id"] = costItemId;  // 0 = gold, nonzero = material item ID
            quote["cost_value"] = costValue;
            m["last_quote"] = quote;
        }

        // Read merchant item IDs via SEH-safe helper
        uint32_t ids[256] = {};
        uint32_t count = ReadMerchantItemIds(ids, 256);

        json items = json::array();
        for (uint32_t i = 0; i < count; i++) {
            if (!ids[i]) continue;
            auto* item = ItemMgr::GetItemById(ids[i]);
            if (!item) continue;

            json it;
            it["item_id"] = item->item_id;
            it["model_id"] = item->model_id;
            it["type"] = item->type;
            it["value"] = item->value;
            it["quantity"] = item->quantity;
            it["interaction"] = item->interaction;
            items.push_back(it);
        }
        m["items"] = items;
        return m;
    }

    // Build effects for the player (with skill type and time remaining)
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
                e["timestamp"] = eff.timestamp;
                e["caster_agent_id"] = eff.agent_id;
                // Time remaining
                float remaining = EffectMgr::GetEffectTimeRemaining(myAgent->agent_id, eff.skill_id);
                e["time_remaining"] = remaining;
                // Skill type for hex/enchant classification
                const auto* sd = SkillMgr::GetSkillConstantData(eff.skill_id);
                if (sd) {
                    e["type"] = sd->type;
                }
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
        j["merchant"] = BuildMerchantJson();
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
        j["merchant"] = BuildMerchantJson();
        j["inventory"] = BuildInventoryJson();
        j["storage"] = BuildStorageJson();
        j["effects"] = BuildPlayerEffectsJson();
        return JsonToHeap(j, outLength);
    }

} // namespace GWA3::LLM::GameSnapshot
