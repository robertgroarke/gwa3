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
#include <gwa3/managers/ChatLogMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/bot/BotFramework.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
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
    static uint32_t g_lastChatTimestamp = 0;  // track which chat messages we've already sent

    template <typename T>
    struct SnapshotArrayView {
        T* buffer;
        uint32_t capacity;
        uint32_t size;
        uint32_t param;
    };

    struct SnapshotTradeItemView {
        uint32_t item_id;
        uint32_t quantity;
    };

    struct SnapshotTradeTraderView {
        uint32_t gold;
        SnapshotArrayView<SnapshotTradeItemView> items;
    };

    struct SnapshotTradeContextView {
        uint32_t flags;
        uint32_t h0004[3];
        SnapshotTradeTraderView player;
        SnapshotTradeTraderView partner;
    };

    template <typename Fn>
    static void ForEachAgent(Fn&& fn) {
        const uint32_t maxAgents = AgentMgr::GetMaxAgents();
        for (uint32_t agentId = 1; agentId < maxAgents; ++agentId) {
            auto* agent = AgentMgr::GetAgentByID(agentId);
            if (!agent) continue;
            fn(agent);
        }
    }

    struct NearbyAgentSeed {
        uint32_t agent_id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float distance = 0.0f;
        uint32_t type = 0;
    };

    struct LivingAgentSeed {
        uint32_t agent_id = 0;
        float hp = 0.0f;
        float max_hp = 0.0f;
        float energy = 0.0f;
        float max_energy = 0.0f;
        uint32_t allegiance = 0;
        uint32_t primary = 0;
        uint32_t secondary = 0;
        uint32_t level = 0;
        uint32_t effects = 0;
        uint32_t weapon_type = 0;
        uint32_t model_state = 0;
        uint32_t hex = 0;
        uint32_t player_number = 0;
        uint32_t login_number = 0;
        uint32_t casting_skill_id = 0;
    };

    struct GadgetAgentSeed {
        uint32_t gadget_id = 0;
        uint32_t extra_type = 0;
    };

    struct ItemAgentSeed {
        uint32_t item_id = 0;
        uint32_t owner = 0;
    };

    static bool ReadNearbyAgentSeed(const AgentLiving* me, Agent* agent, float maxRange, NearbyAgentSeed& out) {
        if (!me || !agent) return false;
        __try {
            if (agent->agent_id == me->agent_id) return false;
            const float dist = AgentMgr::GetDistance(me->x, me->y, agent->x, agent->y);
            if (dist > maxRange) return false;
            out.agent_id = agent->agent_id;
            out.x = agent->x;
            out.y = agent->y;
            out.distance = dist;
            out.type = agent->type;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ReadLivingAgentSeed(Agent* agent, LivingAgentSeed& out) {
        if (!agent) return false;
        __try {
            if (agent->type != 0xDB) return false;
            auto* living = reinterpret_cast<AgentLiving*>(agent);
            out.agent_id = living->agent_id;
            out.hp = living->hp;
            out.max_hp = living->max_hp;
            out.energy = living->energy;
            out.max_energy = living->max_energy;
            out.allegiance = living->allegiance;
            out.primary = living->primary;
            out.secondary = living->secondary;
            out.level = living->level;
            out.effects = living->effects;
            out.weapon_type = living->weapon_type;
            out.model_state = living->model_state;
            out.hex = living->hex;
            out.player_number = living->player_number;
            out.login_number = living->login_number;
            out.casting_skill_id = static_cast<uint32_t>(living->skill);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ReadGadgetAgentSeed(Agent* agent, GadgetAgentSeed& out) {
        if (!agent) return false;
        __try {
            if (agent->type != 0x200) return false;
            auto* gadget = reinterpret_cast<AgentGadget*>(agent);
            out.gadget_id = gadget->gadget_id;
            out.extra_type = gadget->extra_type;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ReadItemAgentSeed(Agent* agent, ItemAgentSeed& out) {
        if (!agent) return false;
        __try {
            if (agent->type != 0x400) return false;
            auto* item = reinterpret_cast<AgentItem*>(agent);
            out.item_id = item->item_id;
            out.owner = item->owner;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool TryGetPlayerNameUtf8(uint32_t loginNumber, char (&out)[64]) {
        out[0] = '\0';
        if (loginNumber == 0) return false;
        __try {
            wchar_t* wName = PlayerMgr::GetPlayerName(loginNumber);
            if (!wName || !wName[0]) return false;
            const int written = WideCharToMultiByte(
                CP_UTF8, 0, wName, -1, out, static_cast<int>(sizeof(out) - 1), nullptr, nullptr);
            if (written <= 0) return false;
            out[sizeof(out) - 1] = '\0';
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            out[0] = '\0';
            return false;
        }
    }

    static bool ReadEffectFlags(uint32_t agentId, bool& hasHex, bool& hasEnchant) {
        hasHex = false;
        hasEnchant = false;
        __try {
            auto* agentEffects = EffectMgr::GetAgentEffects(agentId);
            if (!agentEffects || !agentEffects->effects.buffer) return true;
            for (uint32_t ei = 0; ei < agentEffects->effects.size; ++ei) {
                auto& eff = agentEffects->effects.buffer[ei];
                if (eff.skill_id == 0) continue;
                const auto* sd = SkillMgr::GetSkillConstantData(eff.skill_id);
                if (!sd) continue;
                if (sd->type == 1) hasHex = true;
                if (sd->type == 3 || sd->type == 16) hasEnchant = true;
            }
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            hasHex = false;
            hasEnchant = false;
            return false;
        }
    }

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
        uint32_t myId = AgentMgr::GetMyId();
        ForEachAgent([&](Agent* agent) {
            LivingAgentSeed living{};
            if (!ReadLivingAgentSeed(agent, living)) return;
            if (living.agent_id == myId || living.allegiance != 1) return;

            auto* bar = SkillMgr::GetSkillbarByAgentId(living.agent_id);
            if (!bar) return;

            json h;
            h["agent_id"] = living.agent_id;
            h["hp"] = living.hp;
            h["energy"] = living.energy;
            h["primary"] = living.primary;
            h["secondary"] = living.secondary;
            h["level"] = living.level;
            h["is_casting"] = (living.casting_skill_id != 0);
            h["casting_skill_id"] = living.casting_skill_id;
            h["skillbar"] = BuildSkillbarFromBar(bar);
            heroes.push_back(h);
        });
        return heroes;
    }

    // WorldContext resolution delegated to Offsets::ResolveWorldContext()

    static bool ReadVanquishCounters(uint32_t& killed, uint32_t& toKill) {
        killed = 0;
        toKill = 0;
        uintptr_t wc = Offsets::ResolveWorldContext();
        if (!wc) return false;
        __try {
            killed = *reinterpret_cast<uint32_t*>(wc + 0x84C);
            toKill = *reinterpret_cast<uint32_t*>(wc + 0x850);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // Morale: WorldContext + 0x790. Range 40-110 (40=-60%, 100=0%, 110=+10%)
    static int32_t ReadMorale() {
        uintptr_t wc = Offsets::ResolveWorldContext();
        if (!wc) return 0;
        __try {
            uint32_t raw = *reinterpret_cast<uint32_t*>(wc + 0x790);
            // Convert from GW format (40-110) to percentage (-60 to +10)
            return static_cast<int32_t>(raw) - 100;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    // Build map info
    static json BuildMapJson() {
        json m;
        m["map_id"] = MapMgr::GetMapId();
        m["is_loaded"] = MapMgr::GetIsMapLoaded();
        m["loading_state"] = MapMgr::GetLoadingState();  // 0=loading, 1=loaded, 2=disconnected
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

        // Vanquish progress (foes killed / foes to kill)
        uint32_t foesKilled = 0, foesToKill = 0;
        if (ReadVanquishCounters(foesKilled, foesToKill)) {
            m["foes_killed"] = foesKilled;
            m["foes_to_kill"] = foesToKill;
        }

        return m;
    }

    // Build party with per-member status
    static json BuildPartyBasicsJson() {
        json p;
        p["is_defeated"] = PartyMgr::GetIsPartyDefeated();
        p["morale"] = ReadMorale();  // -60 to +10 (0 = no DP/boost)

        // Build party member list from raw agent enumeration.
        auto* me = AgentMgr::GetMyAgent();
        if (me) {
            json members = json::array();
            uint32_t partySize = 0;
            uint32_t deadCount = 0;
            ForEachAgent([&](Agent* agent) {
                LivingAgentSeed living{};
                if (!ReadLivingAgentSeed(agent, living)) return;
                if (living.allegiance != 1 || living.agent_id == me->agent_id) return;

                json m;
                m["agent_id"] = living.agent_id;
                m["hp"] = living.hp;
                m["energy"] = living.energy;
                m["primary"] = living.primary;
                m["level"] = living.level;
                bool alive = living.hp > 0.0f;
                m["is_alive"] = alive;
                m["is_player"] = false;
                m["is_hero"] = false;
                members.push_back(m);
                partySize++;
                if (!alive) deadCount++;
            });
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

        ForEachAgent([&](Agent* agent) {
            NearbyAgentSeed nearby{};
            if (!ReadNearbyAgentSeed(me, agent, maxRange, nearby)) return;

            json a;
            a["id"] = nearby.agent_id;
            a["x"] = nearby.x;
            a["y"] = nearby.y;
            a["distance"] = nearby.distance;
            a["type"] = nearby.type;

            LivingAgentSeed living{};
            GadgetAgentSeed gadget{};
            ItemAgentSeed item{};
            if (ReadLivingAgentSeed(agent, living)) {
                a["agent_type"] = "living";
                a["hp"] = living.hp;
                a["allegiance"] = living.allegiance;
                a["is_alive"] = (living.hp > 0.0f);
                a["player_number"] = living.player_number;
            } else if (ReadGadgetAgentSeed(agent, gadget)) {
                a["agent_type"] = "gadget";
                a["gadget_id"] = gadget.gadget_id;
            } else if (ReadItemAgentSeed(agent, item)) {
                a["agent_type"] = "item";
                a["item_id"] = item.item_id;
                a["owner"] = item.owner;
            } else {
                a["agent_type"] = "unknown";
            }

            agents.push_back(a);
        });

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

        // Dialog body text — prefer decoded, fallback to raw
        const wchar_t* bodyDecoded = DialogMgr::GetDialogBodyDecoded();
        const wchar_t* bodyRaw = DialogMgr::GetDialogBodyRaw();
        if (bodyDecoded && bodyDecoded[0]) {
            char bodyUtf8[1024] = {};
            WideCharToMultiByte(CP_UTF8, 0, bodyDecoded, -1, bodyUtf8, sizeof(bodyUtf8) - 1, nullptr, nullptr);
            d["body"] = bodyUtf8;
        }
        if (bodyRaw && bodyRaw[0]) {
            char rawUtf8[512] = {};
            WideCharToMultiByte(CP_UTF8, 0, bodyRaw, -1, rawUtf8, sizeof(rawUtf8) - 1, nullptr, nullptr);
            d["body_raw"] = rawUtf8;
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

    // SEH-safe helper: read merchant item IDs into a flat buffer.
    // Returns count of valid IDs written (0 on failure).
    static uint32_t ReadMerchantItemIds(uint32_t* outIds, uint32_t maxIds) {
        if (Offsets::BasePointer <= 0x10000) return 0;
        __try {
            uintptr_t p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
            if (p0 <= 0x10000) { Log::Warn("[Snapshot] ReadMerchantItemIds: p0=null"); return 0; }
            uintptr_t p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
            if (p1 <= 0x10000) { Log::Warn("[Snapshot] ReadMerchantItemIds: p1=null"); return 0; }
            uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
            if (p2 <= 0x10000) { Log::Warn("[Snapshot] ReadMerchantItemIds: p2=null"); return 0; }
            uintptr_t base = *reinterpret_cast<uintptr_t*>(p2 + 0x24);
            uint32_t size = *reinterpret_cast<uint32_t*>(p2 + 0x28);
            if (base <= 0x10000 || size == 0 || size > maxIds) {
                Log::Warn("[Snapshot] ReadMerchantItemIds: base=0x%08X size=%u (invalid)",
                          static_cast<unsigned>(base), size);
                return 0;
            }
            for (uint32_t i = 0; i < size; i++) {
                outIds[i] = *reinterpret_cast<uint32_t*>(base + i * 4);
            }
            return size;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] ReadMerchantItemIds: SEH exception");
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

        // Read merchant items using TradeMgr::GetMerchantItemByPosition,
        // which is proven working from the C++ test harness. The previous
        // ReadMerchantItemIds approach returned 0 items from the snapshot
        // thread despite item_count being correct.
        // First gather item data into a plain struct array (SEH-safe),
        // then build JSON from the results.
        struct MerchantItemData { uint32_t item_id, model_id, type, value, quantity, interaction; };
        MerchantItemData itemData[256] = {};
        uint32_t readCount = 0;
        for (uint32_t pos = 1; pos <= itemCount && pos <= 256; ++pos) {
            auto* item = TradeMgr::GetMerchantItemByPosition(pos);
            if (!item) continue;
            auto& d = itemData[readCount++];
            d.item_id = item->item_id;
            d.model_id = item->model_id;
            d.type = item->type;
            d.value = item->value;
            d.quantity = item->quantity;
            d.interaction = item->interaction;
        }

        json items = json::array();
        for (uint32_t i = 0; i < readCount; ++i) {
            json it;
            it["item_id"] = itemData[i].item_id;
            it["model_id"] = itemData[i].model_id;
            it["type"] = itemData[i].type;
            it["value"] = itemData[i].value;
            it["quantity"] = itemData[i].quantity;
            it["interaction"] = itemData[i].interaction;
            items.push_back(it);
        }
        m["items"] = items;
        return m;
    }

    static uintptr_t ResolveTradeContextForSnapshot() {
        const uintptr_t gc = Offsets::ResolveGameContext();
        if (!gc) return 0;
        __try {
            const uintptr_t trade = *reinterpret_cast<uintptr_t*>(gc + 0x58);
            return trade > 0x10000 ? trade : 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static bool ReadTradeFlagsForSnapshot(const SnapshotTradeContextView* ctx, uint32_t& flags) {
        flags = 0;
        if (!ctx) return false;
        __try {
            flags = ctx->flags;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            flags = 0;
            return false;
        }
    }

    static bool ReadTradeTraderHeaderForSnapshot(const SnapshotTradeTraderView* trader,
                                                 uint32_t& gold,
                                                 const SnapshotTradeItemView*& items,
                                                 uint32_t& count) {
        gold = 0;
        items = nullptr;
        count = 0;
        if (!trader) return false;
        __try {
            gold = trader->gold;
            items = trader->items.buffer;
            count = trader->items.size;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            gold = 0;
            items = nullptr;
            count = 0;
            return false;
        }
    }

    static bool ReadTradeItemForSnapshot(const SnapshotTradeItemView* items,
                                         uint32_t index,
                                         uint32_t& itemId,
                                         uint32_t& quantity) {
        itemId = 0;
        quantity = 0;
        if (!items) return false;
        __try {
            itemId = items[index].item_id;
            quantity = items[index].quantity;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            itemId = 0;
            quantity = 0;
            return false;
        }
    }

    static json BuildTradePartyJson(const SnapshotTradeTraderView& trader) {
        json out;
        uint32_t gold = 0;
        uint32_t count = 0;
        const SnapshotTradeItemView* itemsPtr = nullptr;
        ReadTradeTraderHeaderForSnapshot(&trader, gold, itemsPtr, count);
        out["gold"] = gold;
        out["item_count"] = count;
        json items = json::array();
        if (itemsPtr) {
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t itemId = 0;
                uint32_t quantity = 0;
                if (!ReadTradeItemForSnapshot(itemsPtr, i, itemId, quantity)) continue;
                json it;
                it["item_id"] = itemId;
                it["quantity"] = quantity;
                if (auto* item = ItemMgr::GetItemById(itemId)) {
                    it["model_id"] = item->model_id;
                    it["type"] = item->type;
                    it["value"] = item->value;
                }
                items.push_back(it);
            }
        }
        out["items"] = items;
        return out;
    }

    static void ReadTradeWindowUiSafe(uint32_t& frame, uint32_t& state, uint32_t& ctx) {
        frame = 0; state = 0; ctx = 0;
        __try {
            frame = TradeMgr::GetTradeWindowUiFrame();
            state = TradeMgr::GetTradeWindowUiState();
            ctx = TradeMgr::GetTradeWindowUiContext();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            frame = 0; state = 0; ctx = 0;
        }
    }

    static json BuildTradeJson() {
        json t;
        // SKIP frame-based trade window detection — the frame array scan
        // from the bridge thread races with the game thread and causes
        // heap corruption crashes.  Use GameContext flags only.
        const uint32_t uiWindowFrame = 0;
        const uint32_t uiWindowState = 0;
        const uint32_t uiWindowContext = 0;
        const uintptr_t tradePtr = ResolveTradeContextForSnapshot();
        const auto* ctx = reinterpret_cast<const SnapshotTradeContextView*>(tradePtr);

        uint32_t flags = 0;
        if (!ReadTradeFlagsForSnapshot(ctx, flags)) {
            ctx = nullptr;
        }
        // Use flags-only detection (no frame scan)
        const bool uiOpen = (flags != 0);

        t["flags"] = flags;
        t["is_open"] = uiOpen;
        t["is_initiated"] = uiOpen && (flags & 0x1u) != 0;
        t["offer_sent"] = uiOpen && (flags & 0x2u) != 0;
        t["is_accepted"] = uiOpen && (flags & 0x4u) != 0;

        if (ctx) {
            t["player"] = BuildTradePartyJson(ctx->player);
            t["partner"] = BuildTradePartyJson(ctx->partner);
        } else {
            t["player"] = json::object({{"gold", 0}, {"item_count", 0}, {"items", json::array()}});
            t["partner"] = json::object({{"gold", 0}, {"item_count", 0}, {"items", json::array()}});
        }

        t["debug_ui_player_updated_count"] = TradeMgr::GetTradeUiPlayerUpdatedCount();
        t["debug_ui_initiate_count"] = TradeMgr::GetTradeUiInitiateCount();
        t["debug_ui_last_initiate_wparam"] = TradeMgr::GetTradeUiLastInitiateWParam();
        t["debug_ui_session_start_count"] = TradeMgr::GetTradeUiSessionStartCount();
        t["debug_ui_session_updated_count"] = TradeMgr::GetTradeUiSessionUpdatedCount();
        t["debug_ui_last_session_start_state"] = TradeMgr::GetTradeUiLastSessionStartState();
        t["debug_ui_last_session_start_player_number"] = TradeMgr::GetTradeUiLastSessionStartPlayerNumber();
        t["debug_party_button_hit_count"] = TradeMgr::GetPartyButtonCallbackHitCount();
        t["debug_party_button_last_this"] = TradeMgr::GetPartyButtonCallbackLastThis();
        t["debug_party_button_last_arg"] = TradeMgr::GetPartyButtonCallbackLastArg();
        t["debug_capture_count"] = TradeMgr::GetTradeWindowCaptureCount();
        t["debug_window_ctx"] = TradeMgr::GetTradeWindowContext();
        t["debug_window_frame"] = TradeMgr::GetTradeWindowFrame();
        t["debug_ui_window_frame"] = uiWindowFrame;
        t["debug_ui_window_state"] = uiWindowState;
        t["debug_ui_window_context"] = uiWindowContext;
        // Quantity prompt frame scan disabled from bridge thread — the scan
        // iterates the game's frame array concurrently with the game thread,
        // causing crashes when freed frames are accessed.
        t["debug_quantity_prompt_open"] = false;
        t["debug_quantity_prompt_frame"] = 0;
        t["debug_quantity_prompt_child_count"] = 0;
        t["debug_remove_item_available"] =
            flags != 0 && static_cast<uint32_t>(t["player"]["item_count"]) > 0;

        static uint32_t s_lastTradeFlags = 0xFFFFFFFFu;
        static uint32_t s_lastTradeUiFrame = 0xFFFFFFFFu;
        static uint32_t s_lastTradeUiState = 0xFFFFFFFFu;
        static uint32_t s_lastTradeUiOpen = 0xFFFFFFFFu;
        if (s_lastTradeFlags != flags
            || s_lastTradeUiFrame != uiWindowFrame
            || s_lastTradeUiState != uiWindowState
            || s_lastTradeUiOpen != (uiOpen ? 1u : 0u)) {
            Log::Info(
                "[LLM-TradeSnapshot] flags=0x%X is_open=%u uiFrame=0x%08X uiState=0x%X uiCtx=0x%08X player_items=%u partner_items=%u",
                flags,
                uiOpen ? 1u : 0u,
                uiWindowFrame,
                uiWindowState,
                uiWindowContext,
                static_cast<uint32_t>(t["player"]["item_count"]),
                static_cast<uint32_t>(t["partner"]["item_count"]));
            s_lastTradeFlags = flags;
            s_lastTradeUiFrame = uiWindowFrame;
            s_lastTradeUiState = uiWindowState;
            s_lastTradeUiOpen = uiOpen ? 1u : 0u;
        }
        return t;
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

    // Build recent chat messages (only new ones since last call)
    static json BuildChatLogJson() {
        json chatLog = json::array();
        uint32_t count = ChatLogMgr::GetMessageCount();
        if (count == 0) return chatLog;

        // Get messages newer than our last read timestamp
        const ChatLogMgr::ChatEntry* entries[50] = {};
        uint32_t newCount = ChatLogMgr::GetMessagesSince(g_lastChatTimestamp, entries, 50);

        for (uint32_t i = 0; i < newCount; i++) {
            const auto* e = entries[i];
            if (!e) continue;

            json msg;
            msg["channel"] = e->channel_name;

            // Convert sender wchar to UTF-8
            if (e->sender[0]) {
                char senderUtf8[128] = {};
                WideCharToMultiByte(CP_UTF8, 0, e->sender, -1, senderUtf8, sizeof(senderUtf8) - 1, nullptr, nullptr);
                msg["sender"] = senderUtf8;
            }

            // Convert message wchar to UTF-8
            if (e->message[0]) {
                char msgUtf8[512] = {};
                WideCharToMultiByte(CP_UTF8, 0, e->message, -1, msgUtf8, sizeof(msgUtf8) - 1, nullptr, nullptr);
                msg["message"] = msgUtf8;
            }

            if (e->sender_agent_id) {
                msg["sender_agent_id"] = e->sender_agent_id;
            }

            chatLog.push_back(msg);
            g_lastChatTimestamp = e->timestamp_ms;
        }
        return chatLog;
    }

    // Build title progression for key titles
    static json BuildTitlesJson() {
        json titles = json::object();

        struct TitleEntry { uint32_t id; const char* name; };
        static const TitleEntry TRACKED_TITLES[] = {
            {TitleID::Vanguard,    "vanguard"},
            {TitleID::Norn,        "norn"},
            {TitleID::Asura,       "asura"},
            {TitleID::Deldrimor,   "deldrimor"},
            {TitleID::Sunspear,    "sunspear"},
            {TitleID::Lightbringer,"lightbringer"},
            {TitleID::Survivor,    "survivor"},
            {TitleID::KindOfABigDeal, "kind_of_a_big_deal"},
        };

        for (const auto& entry : TRACKED_TITLES) {
            auto* track = PlayerMgr::GetTitleTrack(entry.id);
            if (!track) continue;
            json t;
            t["current_points"] = track->current_points;
            t["current_rank"] = track->current_title_tier_index;
            t["points_needed_next"] = track->points_needed_next_rank;
            t["max_rank"] = track->max_title_rank;
            titles[entry.name] = t;
        }
        return titles;
    }

    // Build quest state: active quest + quest log summary
    static json BuildQuestJson() {
        json q;
        uint32_t activeId = QuestMgr::GetActiveQuestId();
        q["active_quest_id"] = activeId;

        uint32_t logSize = QuestMgr::GetQuestLogSize();
        q["quest_log_size"] = logSize;

        // Active quest details
        if (activeId != 0) {
            Quest* active = QuestMgr::GetQuestById(activeId);
            if (active) {
                json aq;
                aq["quest_id"] = active->quest_id;
                aq["log_state"] = active->log_state;
                aq["is_completed"] = (active->log_state & 0x02) != 0;
                aq["is_primary"] = (active->log_state & 0x20) != 0;
                aq["map_from"] = active->map_from;
                aq["map_to"] = active->map_to;
                aq["marker_x"] = active->marker_x;
                aq["marker_y"] = active->marker_y;

                // Convert encoded name/objectives to UTF-8 if available
                if (active->name && active->name[0]) {
                    char buf[256] = {};
                    WideCharToMultiByte(CP_UTF8, 0, active->name, -1, buf, sizeof(buf) - 1, nullptr, nullptr);
                    aq["name"] = buf;
                }
                if (active->objectives && active->objectives[0]) {
                    char buf[512] = {};
                    WideCharToMultiByte(CP_UTF8, 0, active->objectives, -1, buf, sizeof(buf) - 1, nullptr, nullptr);
                    aq["objectives"] = buf;
                }
                if (active->description && active->description[0]) {
                    char buf[512] = {};
                    WideCharToMultiByte(CP_UTF8, 0, active->description, -1, buf, sizeof(buf) - 1, nullptr, nullptr);
                    aq["description"] = buf;
                }
                q["active_quest"] = aq;
            }
        }

        // Quest log summary (IDs + completion state)
        json log = json::array();
        for (uint32_t i = 0; i < logSize && i < 32; i++) {
            Quest* quest = QuestMgr::GetQuestByIndex(i);
            if (!quest || quest->quest_id == 0) continue;
            json entry;
            entry["quest_id"] = quest->quest_id;
            entry["log_state"] = quest->log_state;
            entry["is_completed"] = (quest->log_state & 0x02) != 0;
            entry["map_from"] = quest->map_from;
            entry["map_to"] = quest->map_to;
            if (quest->name && quest->name[0]) {
                char buf[128] = {};
                WideCharToMultiByte(CP_UTF8, 0, quest->name, -1, buf, sizeof(buf) - 1, nullptr, nullptr);
                entry["name"] = buf;
            }
            log.push_back(entry);
        }
        q["quest_log"] = log;

        return q;
    }

    // Bot state (for advisory mode — shows what Froggy is doing)
    static json BuildBotStateJson() {
        json b;
        auto state = Bot::GetState();
        const char* name = "unknown";
        switch (state) {
            case Bot::BotState::Idle:          name = "idle"; break;
            case Bot::BotState::CharSelect:    name = "char_select"; break;
            case Bot::BotState::InTown:        name = "in_town"; break;
            case Bot::BotState::Traveling:     name = "traveling"; break;
            case Bot::BotState::InDungeon:     name = "in_dungeon"; break;
            case Bot::BotState::Looting:       name = "looting"; break;
            case Bot::BotState::Merchant:      name = "merchant"; break;
            case Bot::BotState::Maintenance:   name = "maintenance"; break;
            case Bot::BotState::Error:         name = "error"; break;
            case Bot::BotState::Stopping:      name = "stopping"; break;
            case Bot::BotState::LLMControlled: name = "llm_controlled"; break;
        }
        b["state"] = name;
        b["is_running"] = Bot::IsRunning();
        b["combat_mode"] = (Bot::GetConfig().combat_mode == Bot::CombatMode::LLM) ? "llm" : "builtin";
        return b;
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
        j["bot"] = BuildBotStateJson();
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
        j["heroes"] = json::array();
        j["trade"] = BuildTradeJson();
        j["dialog"] = BuildDialogJson();
        j["merchant"] = BuildMerchantJson();
        j["quests"] = BuildQuestJson();
        j["chat"] = BuildChatLogJson();
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
        j["heroes"] = json::array();
        j["trade"] = BuildTradeJson();
        j["dialog"] = BuildDialogJson();
        j["merchant"] = BuildMerchantJson();
        j["quests"] = BuildQuestJson();
        j["inventory"] = BuildInventoryJson();
        j["storage"] = json::array();
        j["effects"] = json::array();
        j["titles"] = json::array();
        return JsonToHeap(j, outLength);
    }

} // namespace GWA3::LLM::GameSnapshot
