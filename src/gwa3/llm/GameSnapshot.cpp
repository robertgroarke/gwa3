#include <gwa3/llm/GameSnapshot.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/MerchantMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/ChatLogMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <bots/common/BotFramework.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/Skill.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/Effect.h>
#include <gwa3/utils/EncStringCache.h>

#include <Windows.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <string>
#include <utility>

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

    // Helper: convert json to heap-allocated char*
    static char* JsonToHeap(const json& j, uint32_t* outLength) {
        std::string s = j.dump();
        *outLength = static_cast<uint32_t>(s.size());
        char* buf = new (std::nothrow) char[*outLength + 1];
        if (!buf) { *outLength = 0; return nullptr; }
        memcpy(buf, s.c_str(), *outLength + 1);
        return buf;
    }

    // Convert a wchar_t* to strict UTF-8 via WideCharToMultiByte with
    // WC_ERR_INVALID_CHARS. Returns empty on failure. This matters for
    // GW's encoded-string wchars: those contain PUA codepoints and
    // occasionally lone surrogate halves which, without the strict
    // flag, get written as invalid UTF-8 bytes that then crash Python's
    // .decode("utf-8") when the snapshot JSON reaches the bridge.
    static std::string SafeWideToUtf8(const wchar_t* p) {
        if (!p || !p[0]) return {};
        char buf[1024] = {};
        int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                    p, -1, buf, sizeof(buf) - 1,
                                    nullptr, nullptr);
        return (n > 0) ? std::string(buf) : std::string{};
    }

    // Does the wide-char string look like plain human-readable text
    // (as opposed to GW's encoded-reference format)? Used by
    // EmitBestText to decide whether the raw WideCharToMultiByte
    // fallback is meaningful.
    //
    // Positive-filter approach: only accept strings whose first wchar
    // is in a normal Latin-range codepoint block. GW's encoded format
    // uses a sprawling and hard-to-enumerate set of sentinels — on
    // live inventories we've seen 0x8101/0x8102 (database refs),
    // 0x2xxx / 0x1xxx (format strings), 0x4E00..0x9FFF (CJK in
    // Reforged NPCs), 0xE000..0xF8FF (PUA), AND 0x0A40 (Gurmukhi) /
    // 0x010A (Latin Ext-A) used as item modifier sentinels on
    // `single_item_name`. Enumerating all of these plays whack-a-mole;
    // inverting the test to "starts with basic Latin or Latin-1" is a
    // tighter gate.
    //
    // Accepted first-wchar ranges:
    //   0x0020..0x007E  Basic Latin (printable ASCII)
    //   0x00A0..0x024F  Latin-1 Supplement + Latin Extended-A/B
    //                   (catches accented European characters in
    //                   player account handles or NPC names)
    //
    // Side effect: non-Latin account handles (Korean/Chinese/etc.
    // players on Asian shards) get filtered too, but the DLL log
    // confirms English clients report languageId=0,
    // so those handles decode through the cached-name path anyway.
    static bool LooksPlainText(const wchar_t* p) {
        if (!p || !p[0]) return false;
        const wchar_t c0 = p[0];
        if (c0 >= 0x0020 && c0 <= 0x007E) return true;
        if (c0 >= 0x00A0 && c0 <= 0x024F) return true;
        return false;
    }

    // Emit the raw encoded wide-char string as `<key>_enc`, and the
    // decoded UTF-8 form as `<key>` when the passive decode-hook cache
    // already has it. Used for quest fields where the LLM must see the
    // raw encoded marker even if the decode is not yet available.
    //
    // `_enc` is only emitted when the raw wchar sequence produces valid
    // UTF-8. GW-encoded strings often don't, and shipping invalid bytes
    // through the pipe breaks the Python bridge. Callers that want a
    // guaranteed field use EmitBestText below instead.
    static void EmitEnc(json& dst, const wchar_t* p,
                        const char* rawKey, const char* decodedKey) {
        if (!p || !p[0]) return;
        std::string raw = SafeWideToUtf8(p);
        if (!raw.empty()) dst[rawKey] = std::move(raw);
        std::string dec = EncStringCache::Lookup(p);
        if (!dec.empty()) dst[decodedKey] = std::move(dec);
    }

    // Emit a wchar_t* as the best human-readable UTF-8 form into
    // dst[key]: prefer the passive decode-hook cache, fall back to
    // direct WideCharToMultiByte (strict UTF-8). Use this for fields
    // where the caller only wants one string (not the enc + decoded
    // pair), e.g. dialog button labels or item/agent names.
    //
    // The raw fallback is gated on LooksEncoded(): if the wchar
    // sequence is in GW's encoded-reference format (PUA sentinels
    // etc.), emitting its UTF-8 bytes produces human-unreadable
    // gibberish that clutters the snapshot and confuses the LLM.
    // In that case we omit the field entirely and let the caller
    // retry on the next snapshot after the passive decode hook has
    // had a chance to fill the cache.
    //
    // The optional `fallback` pointer is tried if the primary source
    // is encoded + not yet cached + has no readable raw UTF-8. Used
    // for players where `name_enc` (title-decorated, cache-only) can
    // be backed by `name` (plain account handle) — emits something
    // immediately rather than waiting for GW to render a nametag.
    static void EmitBestText(json& dst, const wchar_t* p, const char* key,
                             const wchar_t* fallback = nullptr) {
        if (p && p[0]) {
            std::string dec = EncStringCache::Lookup(p);
            if (!dec.empty()) {
                dst[key] = std::move(dec);
                return;
            }
            if (LooksPlainText(p)) {
                std::string raw = SafeWideToUtf8(p);
                if (!raw.empty()) {
                    dst[key] = std::move(raw);
                    return;
                }
            }
        }
        if (!fallback || !fallback[0]) return;
        if (!LooksPlainText(fallback)) return;
        std::string raw = SafeWideToUtf8(fallback);
        if (!raw.empty()) dst[key] = std::move(raw);
    }

    static void EmitCachedText(json& dst, const wchar_t* p, const char* key,
                               const wchar_t* fallback = nullptr) {
        if (p != nullptr) {
            std::string dec = EncStringCache::Lookup(p);
            if (!dec.empty()) {
                dst[key] = std::move(dec);
                return;
            }
        }
        if (fallback != nullptr) {
            std::string dec = EncStringCache::Lookup(fallback);
            if (!dec.empty()) dst[key] = std::move(dec);
        }
    }

    // SEH-only helper: read agent->equip plus the 7 gear-slot item_ids
    // into a plain struct. Returns true if the equip pointer is
    // plausible; item_ids are zero-filled on per-slot fault so callers
    // can skip empty slots without further guards.
    //
    // This lives in its own function because C++ destructors (json
    // objects in the caller) conflict with __try/__except in MSVC —
    // C2712 rejects mixing them in one function.
    // SEH-isolated: snapshot the equipped-item flags + pointers for
    // bag 0 into a plain array. Caller builds JSON from the snapshot,
    // which keeps __try/__except out of the function that holds json
    // objects (MSVC C2712: "Cannot use __try in functions that
    // require object unwinding").
    struct EquippedItem {
        Item* item;
        uint32_t slot;      // 0..6 per GWCA Equipment union
        uint32_t item_id;
        uint32_t model_id;
        wchar_t* name_enc;
        wchar_t* complete_name_enc;
        wchar_t* single_item_name;
        wchar_t* info_string;
    };
    __declspec(noinline) static uint32_t ReadEquippedItems(EquippedItem* out,
                                                           uint32_t max) {
        // Read every item in bag 22 (GWCA Bag::Equipped_Items). Armor
        // pieces live here alongside the active weapon+offhand, but
        // `Item.equipped` is only set for the active weapon set, NOT
        // for worn armor. So we iterate ALL items in the bag and
        // bucket them by `Item.slot` (0..6 = weapon, offhand, chest,
        // legs, head, feet, hands). Costume slots 7-8 are skipped.
        //
        // If two items report the same slot (e.g., weapon set A + B
        // both in slot 0), the later iteration wins. The active weapon
        // has `equipped != 0`, so we prefer that when there's a tie.
        uint32_t written = 0;
        Bag* bag = ItemMgr::GetBag(22);
        if (!bag) return 0;
        __try {
            if (!bag->items.buffer || bag->items.size == 0 ||
                bag->items.size > 64) {
                return 0;
            }
            // Track best record per slot: equipped-flag wins ties.
            EquippedItem slots[7] = {};
            uint8_t slotFilled[7] = {0};
            uint8_t slotEquipped[7] = {0};
            for (uint32_t i = 0; i < bag->items.size; ++i) {
                Item* item = bag->items.buffer[i];
                if (!item) continue;
                uint32_t slot = item->slot;
                if (slot >= 7) continue;
                uint8_t equipped = item->equipped;
                // If this slot already has a filled record AND the
                // existing one is the active weapon (equipped!=0),
                // don't overwrite.
                if (slotFilled[slot] && slotEquipped[slot] && !equipped) {
                    continue;
                }
                auto& rec = slots[slot];
                rec.item              = item;
                rec.slot              = slot;
                rec.item_id           = item->item_id;
                rec.model_id          = item->model_id;
                rec.name_enc          = item->name_enc;
                rec.complete_name_enc = item->complete_name_enc;
                rec.single_item_name  = item->single_item_name;
                rec.info_string       = item->info_string;
                slotFilled[slot] = 1;
                slotEquipped[slot] = equipped ? 1 : 0;
            }
            for (uint32_t s = 0; s < 7 && written < max; ++s) {
                if (slotFilled[s]) out[written++] = slots[s];
            }
            return written;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return written;
        }
    }

    // SEH-isolated read of AgentLiving's 16-bit weapon + offhand
    // fields. Used as a fallback for heroes (whose full equipment
    // isn't yet plumbed — see EmitEquipmentForAgent below).
    struct AgentWeaponIds {
        uint16_t weapon_id16;
        uint16_t offhand_id16;
    };
    __declspec(noinline) static bool ReadAgentWeaponIds(
            const AgentLiving* agent, AgentWeaponIds* out) {
        if (!agent || !out) return false;
        *out = {};
        __try {
            out->weapon_id16 = agent->weapon_item_id;
            out->offhand_id16 = agent->offhand_item_id;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // Helper: resolve a single slot from an item_id and add to `eq`.
    static void EmitOneEquipmentSlot(json& eq, uint32_t itemId,
                                     const char* slotName) {
        if (itemId == 0) return;
        Item* item = ItemMgr::GetItemById(itemId);
        if (!item) return;
        json it;
        it["item_id"] = itemId;
        it["model_id"] = item->model_id;
        EmitBestText(it, item->name_enc, "name", item->single_item_name);
        EmitBestText(it, item->complete_name_enc, "full_name");
        EmitBestText(it, item->info_string, "info_string");
        eq[slotName] = it;
    }

    // Emit an `equipment` sub-object for the given living agent.
    //
    // For the PLAYER (isMe=true): iterate bag 22 (GWCA
    // Bag::Equipped_Items), bucket by Item.slot. Produces all 7 gear
    // slots when the character has equipped them (verified in d4dac60).
    //
    // For HEROES: full bag iteration isn't possible because
    // ItemMgr::GetBag() returns player-local bags only; hero
    // inventories aren't plumbed into our struct definitions yet.
    // Fall back to AgentLiving.weapon_item_id / offhand_item_id — the
    // 16-bit fields populate in combat when the weapon is drawn.
    // Armor slots for heroes are not emitted (future work: hero
    // inventory RE; the Item.agent_id field at +0x04 is 0 across the
    // player's bags, so the "item belongs to this agent_id" cross-
    // reference path doesn't work — probed empirically with
    // tools/_probe_all_items.py).
    //
    // Item names flow through the same name / full_name / info_string
    // triple as everywhere else, reusing EmitBestText's positive-gate
    // encoding filter.
    static void EmitEquipmentForAgent(json& dst, const AgentLiving* agent,
                                      bool isMe) {
        if (!agent) return;

        json eq = json::object();

        if (isMe) {
            static const char* kSlotNames[7] = {
                "weapon", "offhand", "chest", "legs",
                "head", "feet", "hands",
            };
            EquippedItem items[16] = {};
            uint32_t count = ReadEquippedItems(items, 16);
            for (uint32_t i = 0; i < count; ++i) {
                const auto& r = items[i];
                json it;
                it["item_id"] = r.item_id;
                it["model_id"] = r.model_id;
                EmitBestText(it, r.name_enc, "name", r.single_item_name);
                EmitBestText(it, r.complete_name_enc, "full_name");
                EmitBestText(it, r.info_string, "info_string");
                eq[kSlotNames[r.slot]] = it;
            }
        } else {
            // Hero fallback: weapon + offhand only, from direct fields.
            AgentWeaponIds ids{};
            if (ReadAgentWeaponIds(agent, &ids)) {
                EmitOneEquipmentSlot(eq, ids.weapon_id16,  "weapon");
                EmitOneEquipmentSlot(eq, ids.offhand_id16, "offhand");
            }
        }

        if (!eq.empty()) dst["equipment"] = eq;
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

        EmitEquipmentForAgent(me, agent, /*isMe=*/true);
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

    // SEH-isolated: copy the PartyInfo.heroes array into POD records.
    // Called off the json-building path because json destructors
    // conflict with __try/__except in MSVC (C2712).
    struct HeroPartyEntry {
        uint32_t hero_id;
        uint32_t agent_id;
        uint32_t owner_player_id;
        uint32_t level;
    };
    __declspec(noinline) static uint32_t ReadPartyHeroes(HeroPartyEntry* out,
                                                          uint32_t max) {
        PartyInfo* party = PartyMgr::ResolvePlayerParty();
        if (!party) return 0;
        uint32_t written = 0;
        __try {
            auto& arr = party->heroes;
            if (!arr.buffer || arr.size == 0 || arr.size > 16) return 0;
            for (uint32_t i = 0; i < arr.size && written < max; ++i) {
                const auto& h = arr.buffer[i];
                auto& r = out[written++];
                r.hero_id = h.hero_id;
                r.agent_id = h.agent_id;
                r.owner_player_id = h.owner_player_id;
                r.level = h.level;
            }
            return written;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return written;
        }
    }

    struct PlayerPartyEntry {
        uint32_t login_number;
        uint32_t called_target_id;
        uint32_t state;
    };
    __declspec(noinline) static uint32_t ReadPartyPlayers(PlayerPartyEntry* out,
                                                           uint32_t max) {
        PartyInfo* party = PartyMgr::ResolvePlayerParty();
        if (!party) return 0;
        uint32_t written = 0;
        __try {
            auto& arr = party->players;
            if (!arr.buffer || arr.size == 0 || arr.size > 16) return 0;
            for (uint32_t i = 0; i < arr.size && written < max; ++i) {
                const auto& p = arr.buffer[i];
                auto& r = out[written++];
                r.login_number = p.login_number;
                r.called_target_id = p.called_target_id;
                r.state = p.state;
            }
            return written;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return written;
        }
    }

    struct HenchPartyEntry {
        uint32_t agent_id;
        uint32_t profession;
        uint32_t level;
    };
    __declspec(noinline) static uint32_t ReadPartyHenchmen(HenchPartyEntry* out,
                                                            uint32_t max) {
        PartyInfo* party = PartyMgr::ResolvePlayerParty();
        if (!party) return 0;
        uint32_t written = 0;
        __try {
            auto& arr = party->henchmen;
            if (!arr.buffer || arr.size == 0 || arr.size > 16) return 0;
            for (uint32_t i = 0; i < arr.size && written < max; ++i) {
                const auto& h = arr.buffer[i];
                auto& r = out[written++];
                r.agent_id = h.agent_id;
                r.profession = h.profession;
                r.level = h.level;
            }
            return written;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return written;
        }
    }

    // SEH-isolated: look up WorldContext->hero_info[hero_id] and copy
    // primary/secondary/name into a POD record. Per GWCA's
    // WorldContext.h, the array is at +0x594, but Reforged drifts
    // field offsets; if 0x594 yields an invalid buffer we bail.
    struct HeroInfoRecord {
        bool     ok;
        uint32_t hero_id;
        uint32_t agent_id;
        uint32_t level;
        uint32_t primary;
        uint32_t secondary;
        uint32_t hero_file_id;
        uint32_t model_file_id;
        wchar_t  name[20];
    };
    __declspec(noinline) static HeroInfoRecord ReadHeroInfoByHeroId(uint32_t heroId) {
        HeroInfoRecord r{};
        uintptr_t wc = Offsets::ResolveWorldContext();
        if (!wc) return r;
        __try {
            auto* arr = reinterpret_cast<GWArray<HeroInfo>*>(wc + 0x594);
            if (!arr->buffer || arr->size == 0 || arr->size > 64) return r;
            for (uint32_t i = 0; i < arr->size; ++i) {
                const auto& hi = arr->buffer[i];
                if (hi.hero_id != heroId) continue;
                r.ok = true;
                r.hero_id = hi.hero_id;
                r.agent_id = hi.agent_id;
                r.level = hi.level;
                r.primary = hi.primary;
                r.secondary = hi.secondary;
                r.hero_file_id = hi.hero_file_id;
                r.model_file_id = hi.model_file_id;
                for (int k = 0; k < 20; ++k) r.name[k] = hi.name[k];
                return r;
            }
            return r;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            r.ok = false;
            return r;
        }
    }

    // Build hero skillbars array (one entry per hero in party).
    //
    // Driven by PartyInfo.heroes[] rather than agent enumeration —
    // heroes in OUTPOSTS don't exist as AgentLiving instances (they
    // only spawn as agents when the player enters an explorable
    // area), so ForEachAgent + allegiance==1 misses them entirely.
    // Reading PartyInfo directly gives us the roster regardless of
    // in-game vs outpost state.
    //
    // HeroPartyMember gives us (hero_id, agent_id, level, owner).
    // When agent_id != 0 (explorable / rendered), we enrich with the
    // usual living-agent fields (hp, energy, profession, casting
    // state) + skillbar + 16-bit weapon/offhand. When agent_id == 0
    // (outpost) we emit just the party-level fields.
    //
    // Either way, we also look up WorldContext->hero_info[hero_id] to
    // fill primary/secondary/name which are available even in outpost.
    static json BuildHeroSkillbarsJson() {
        json heroes = json::array();
        HeroPartyEntry entries[16] = {};
        uint32_t count = ReadPartyHeroes(entries, 16);
        if (count == 0) return heroes;

        for (uint32_t i = 0; i < count; ++i) {
            const auto& e = entries[i];
            json h;
            h["hero_id"] = e.hero_id;
            h["agent_id"] = e.agent_id;
            h["owner_player_id"] = e.owner_player_id;
            h["level"] = e.level;

            // WorldContext->hero_info populates prof/name even in outpost.
            HeroInfoRecord info = ReadHeroInfoByHeroId(e.hero_id);
            if (info.ok) {
                h["primary"] = info.primary;
                h["secondary"] = info.secondary;
                h["hero_file_id"] = info.hero_file_id;
                h["model_file_id"] = info.model_file_id;
                // Emit name only if it looks plausible (first char ASCII/Latin).
                if (info.name[0] >= 0x20 && info.name[0] < 0x2000) {
                    // Narrow the UTF-16 name into UTF-8 (bounded, ASCII-safe
                    // path for hero display names which are all Latin).
                    char narrow[64] = {};
                    int w = 0;
                    for (int k = 0; k < 20 && info.name[k] != 0 && w < 63; ++k) {
                        wchar_t c = info.name[k];
                        if (c < 0x80) narrow[w++] = static_cast<char>(c);
                    }
                    narrow[w] = 0;
                    if (narrow[0]) h["name"] = narrow;
                }
            }

            // Enrich when the hero is spawned as an agent (explorable).
            if (e.agent_id != 0) {
                Agent* agent = AgentMgr::GetAgentByID(e.agent_id);
                if (agent) {
                    LivingAgentSeed living{};
                    if (ReadLivingAgentSeed(agent, living)) {
                        h["hp"] = living.hp;
                        h["energy"] = living.energy;
                        // Only overwrite prof fields if HeroInfo didn't fill them
                        // and the living read produced non-zero values.
                        if (!info.ok && living.primary != 0) {
                            h["primary"] = living.primary;
                            h["secondary"] = living.secondary;
                        }
                        h["is_casting"] = (living.casting_skill_id != 0);
                        h["casting_skill_id"] = living.casting_skill_id;
                    }
                    auto* bar = SkillMgr::GetSkillbarByAgentId(e.agent_id);
                    if (bar) h["skillbar"] = BuildSkillbarFromBar(bar);
                    EmitEquipmentForAgent(
                        h, reinterpret_cast<AgentLiving*>(agent),
                        /*isMe=*/false);
                }
            }

            heroes.push_back(h);
        }
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

    // Build party with per-member status.
    //
    // Driven by PartyInfo (PartyMgr::ResolvePlayerParty) so we see the
    // roster in OUTPOSTS too. The old implementation used
    // ForEachAgent+allegiance==1 which only finds agents currently
    // rendered in the world — in Embark Beach heroes aren't spawned
    // yet, so they were invisible to the snapshot even while shown
    // clearly in the Party Formation UI.
    //
    // Per-hero agent_id / hp / energy / profession are enriched only
    // when the hero is actually spawned (agent_id != 0 and the Agent
    // resolves via AgentMgr). In outposts we emit identity fields
    // (hero_id, level, owner) without live-agent data.
    static json BuildPartyBasicsJson() {
        json p;
        p["is_defeated"] = PartyMgr::GetIsPartyDefeated();
        p["morale"] = ReadMorale();

        auto* me = AgentMgr::GetMyAgent();
        json members = json::array();
        uint32_t size = 0;
        uint32_t dead = 0;

        // Players (including self).
        PlayerPartyEntry players[16] = {};
        uint32_t playerCount = ReadPartyPlayers(players, 16);
        for (uint32_t i = 0; i < playerCount; ++i) {
            const auto& pl = players[i];
            json m;
            m["login_number"] = pl.login_number;
            m["called_target_id"] = pl.called_target_id;
            m["state"] = pl.state;
            m["is_player"] = true;
            m["is_hero"] = false;
            if (me && pl.login_number ==
                       static_cast<uint32_t>(me->login_number)) {
                m["agent_id"] = me->agent_id;
                m["hp"] = me->hp;
                m["energy"] = me->energy;
                m["primary"] = me->primary;
                m["secondary"] = me->secondary;
                m["level"] = me->level;
                bool alive = me->hp > 0.0f;
                m["is_alive"] = alive;
                if (!alive) dead++;
            }
            members.push_back(m);
            size++;
        }

        // Heroes: PartyInfo.heroes[] is the authoritative roster in
        // outposts AND explorables. Enrich with AgentLiving when
        // the hero is spawned.
        HeroPartyEntry heroes[16] = {};
        uint32_t heroCount = ReadPartyHeroes(heroes, 16);
        for (uint32_t i = 0; i < heroCount; ++i) {
            const auto& e = heroes[i];
            json m;
            m["hero_id"] = e.hero_id;
            m["agent_id"] = e.agent_id;
            m["owner_player_id"] = e.owner_player_id;
            m["level"] = e.level;
            m["is_player"] = false;
            m["is_hero"] = true;
            if (e.agent_id != 0) {
                Agent* agent = AgentMgr::GetAgentByID(e.agent_id);
                if (agent) {
                    LivingAgentSeed living{};
                    if (ReadLivingAgentSeed(agent, living)) {
                        m["hp"] = living.hp;
                        m["energy"] = living.energy;
                        m["primary"] = living.primary;
                        m["secondary"] = living.secondary;
                        bool alive = living.hp > 0.0f;
                        m["is_alive"] = alive;
                        if (!alive) dead++;
                    }
                }
            }
            members.push_back(m);
            size++;
        }

        // Henchmen: PartyInfo.henchmen[]
        HenchPartyEntry henchmen[16] = {};
        uint32_t henchCount = ReadPartyHenchmen(henchmen, 16);
        for (uint32_t i = 0; i < henchCount; ++i) {
            const auto& hm = henchmen[i];
            json m;
            m["agent_id"] = hm.agent_id;
            m["profession"] = hm.profession;
            m["level"] = hm.level;
            m["is_player"] = false;
            m["is_hero"] = false;
            m["is_henchman"] = true;
            members.push_back(m);
            size++;
        }

        p["members"] = members;
        p["size"] = size;
        p["dead_count"] = dead;
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
                // Decoded nametag: players come from WorldContext.players,
                // NPCs from WorldContext.agent_infos (with NPCArray fallback).
                // EmitBestText prefers cached decoded text from the passive
                // ValidateAsyncDecodeStr hook; if the tag hasn't rendered
                // yet the field is simply omitted. For players we also
                // pass the plain `Player.name` handle as a fallback so
                // the LLM sees at least the bare account name even when
                // the title-decorated `name_enc` hasn't decoded yet.
                if (wchar_t* encName = AgentMgr::GetAgentEncName(agent)) {
                    wchar_t* plainName = AgentMgr::GetAgentPlainName(agent);
                    EmitBestText(a, encName, "name", plainName);
                }
            } else if (ReadGadgetAgentSeed(agent, gadget)) {
                a["agent_type"] = "gadget";
                a["gadget_id"] = gadget.gadget_id;
                // Decoded gadget name (chest, signpost, portal, shrine).
                // Resolved via AgentContext.agent_summary_info with
                // GadgetContext.GadgetInfo fallback. Same lazy-cache
                // pattern as living agents and items.
                if (wchar_t* encName = AgentMgr::GetAgentEncName(agent)) {
                    EmitBestText(a, encName, "name");
                }
            } else if (ReadItemAgentSeed(agent, item)) {
                a["agent_type"] = "item";
                a["item_id"] = item.item_id;
                a["owner"] = item.owner;
                // Resolve to the backing Item* so we can surface the
                // decoded item name — the passive decode hook caches
                // whatever tooltip GW renders when hovering. Emit
                // both the base `name` (e.g. "Longsword") and the
                // richer `full_name` (e.g. "Fiery Longsword of
                // Fortitude"), with `single_item_name` as a plain
                // fallback for the base name when the encoded form
                // hasn't decoded yet.
                Item* inv = ItemMgr::GetItemById(item.item_id);
                if (inv) {
                    a["model_id"] = inv->model_id;
                    EmitBestText(a, inv->name_enc, "name",
                                 inv->single_item_name);
                    EmitBestText(a, inv->complete_name_enc, "full_name");
                    // Tooltip body (damage range, armor, requirement,
                    // inherent mods, runes/insignias) — GW's
                    // hover-text decoded via the passive hook.
                    EmitBestText(a, inv->info_string, "info_string");
                }
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
                // Decoded item name (when GW has rendered the tooltip at
                // least once, the passive hook has cached it). `name`
                // is the base model name, `full_name` includes prefix/
                // suffix modifiers, `single_item_name` backs `name`
                // with a plain form when the encoded ref isn't cached.
                // `info_string` carries the tooltip body text:
                // damage range, armor, attribute requirement, inherent
                // modifiers, runes/insignias — what Gemma needs to
                // make equipment decisions.
                EmitBestText(it, item->name_enc, "name",
                             item->single_item_name);
                EmitBestText(it, item->complete_name_enc, "full_name");
                EmitBestText(it, item->info_string, "info_string");
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
        // Always populate gold from safe getters — they handle null inventory pointer
        inv["gold_character"] = ItemMgr::GetGoldCharacter();
        inv["gold_storage"] = ItemMgr::GetGoldStorage();

        auto* inventory = ItemMgr::GetInventory();
        if (!inventory) {
            inv["bags"] = json::array();
            inv["free_slots_total"] = 0;
            return inv;
        }

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
            // Prefer the decoded form from the passive decode-hook cache;
            // fall back to raw UTF-8 of the wchar_t (which may be gibberish
            // for encoded labels that haven't been rendered yet).
            EmitBestText(b, btn->label, "label");
            if (btn->skill_id != 0xFFFFFFFF) {
                b["skill_id"] = btn->skill_id;
            }
            buttons.push_back(b);
        }
        d["buttons"] = buttons;
        return d;
    }

    struct MerchantItemData {
        uint32_t item_id = 0;
        uint32_t model_id = 0;
        uint32_t type = 0;
        uint32_t value = 0;
        uint32_t quantity = 0;
        uint32_t interaction = 0;
        wchar_t* name_enc = nullptr;
        wchar_t* complete_name_enc = nullptr;
        wchar_t* single_item_name = nullptr;
        wchar_t* info_string = nullptr;
    };

    __declspec(noinline) static bool ReadMerchantItemData(const Item* item,
                                                          MerchantItemData& out) {
        if (item == nullptr) return false;
        out = {};
        __try {
            out.item_id = item->item_id;
            out.model_id = item->model_id;
            out.type = item->type;
            out.value = item->value;
            out.quantity = item->quantity;
            out.interaction = item->interaction;
            out.name_enc = item->name_enc;
            out.complete_name_enc = item->complete_name_enc;
            out.single_item_name = item->single_item_name;
            out.info_string = item->info_string;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            out = {};
            return false;
        }
    }

    // Build merchant/trader window state
    static json BuildMerchantJson() {
        json m;
        m["is_open"] = false;

        if (!MapMgr::GetIsMapLoaded()) {
            m["skipped"] = "map_not_loaded";
            return m;
        }

        const auto* area = MapMgr::GetAreaInfo(MapMgr::GetMapId());
        if (area && IsExplorableMapRegionType(area->type)) {
            // Merchant item arrays can stay populated with stale town-window
            // pointers after zoning. Do not touch them in explorable maps.
            m["skipped"] = "explorable";
            return m;
        }

        uint32_t itemCount = MerchantMgr::GetMerchantItemCount();
        if (itemCount == 0) return m;
        m["is_open"] = true;

        m["item_count"] = itemCount;

        char merchantDetailFlag[8] = {};
        const DWORD merchantDetailLen = GetEnvironmentVariableA(
            "GWA3_SNAPSHOT_MERCHANT_DETAILS",
            merchantDetailFlag,
            static_cast<DWORD>(sizeof(merchantDetailFlag)));
        const bool includeMerchantDetails =
            merchantDetailLen > 0 &&
            merchantDetailLen < sizeof(merchantDetailFlag) &&
            merchantDetailFlag[0] == '1';
        if (!includeMerchantDetails) {
            m["items"] = json::array();
            m["items_skipped"] = "set GWA3_SNAPSHOT_MERCHANT_DETAILS=1 for item details";
            return m;
        }

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

        // Read merchant items using MerchantMgr::GetMerchantItemByPosition,
        // which is proven working through the TradeMgr path. The previous
        // raw pointer-chain approach returned 0 items from the snapshot thread
        // despite item_count being correct.
        // First gather item data into a plain struct array (SEH-safe),
        // then build JSON from the results.
        MerchantItemData itemData[256] = {};
        uint32_t readCount = 0;
        for (uint32_t pos = 1; pos <= itemCount && pos <= 256; ++pos) {
            auto* item = MerchantMgr::GetMerchantItemByPosition(pos);
            if (!item) continue;
            if (!ReadMerchantItemData(item, itemData[readCount])) {
                Log::Warn("[Snapshot] BuildMerchantJson: failed reading merchant item at pos=%u", pos);
                continue;
            }
            ++readCount;
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
            EmitCachedText(it, itemData[i].name_enc, "name",
                           itemData[i].single_item_name);
            EmitCachedText(it, itemData[i].complete_name_enc, "full_name");
            EmitCachedText(it, itemData[i].info_string, "info_string");
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

        const auto ctosTap = CtoS::GetPacketTapSnapshot();
        const auto findPacketCount = [&ctosTap](uint32_t header) -> uint32_t {
            for (uint32_t i = 0; i < _countof(ctosTap.headers); ++i) {
                if (ctosTap.headers[i] == header) {
                    return ctosTap.counts[i];
                }
            }
            return 0u;
        };
        t["debug_ctos_packet_total"] = ctosTap.total_packets;
        t["debug_ctos_trade_submit_offer_count"] = findPacketCount(Packets::TRADE_SUBMIT_OFFER);
        t["debug_ctos_trade_accept_count"] = findPacketCount(Packets::TRADE_ACCEPT);
        t["debug_ctos_trade_cancel_count"] = findPacketCount(Packets::TRADE_CANCEL);
        t["debug_ctos_trade_add_item_count"] = findPacketCount(Packets::TRADE_ADD_ITEM);
        json ctosHeaders = json::array();
        for (uint32_t i = 0; i < _countof(ctosTap.headers); ++i) {
            if (ctosTap.counts[i] == 0) continue;
            ctosHeaders.push_back({
                {"header", ctosTap.headers[i]},
                {"count", ctosTap.counts[i]},
            });
        }
        t["debug_ctos_packets"] = ctosHeaders;

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

                EmitEnc(aq, active->name,        "name_enc",        "name");
                EmitEnc(aq, active->objectives,  "objectives_enc",  "objectives");
                EmitEnc(aq, active->description, "description_enc", "description");
                EmitEnc(aq, active->location,    "location_enc",    "location");
                EmitEnc(aq, active->npc,         "npc_enc",         "npc");
                q["active_quest"] = aq;
            }
        }

        // Quest log summary (IDs + completion state + marker + location/npc)
        json log = json::array();
        for (uint32_t i = 0; i < logSize && i < 32; i++) {
            Quest* quest = QuestMgr::GetQuestByIndex(i);
            if (!quest || quest->quest_id == 0) continue;
            json entry;
            entry["quest_id"] = quest->quest_id;
            entry["log_state"] = quest->log_state;
            entry["is_completed"] = (quest->log_state & 0x02) != 0;
            entry["is_primary"] = (quest->log_state & 0x20) != 0;
            entry["is_area_primary"] = (quest->log_state & 0x40) != 0;
            entry["is_active"] = (quest->quest_id == activeId);
            entry["map_from"] = quest->map_from;
            entry["map_to"] = quest->map_to;
            entry["marker_x"] = quest->marker_x;
            entry["marker_y"] = quest->marker_y;
            EmitEnc(entry, quest->name,     "name_enc",     "name");
            EmitEnc(entry, quest->location, "location_enc", "location");
            EmitEnc(entry, quest->npc,      "npc_enc",      "npc");
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




    // -----------------------------------------------------------------------
    // Per-builder SEH isolation + tier serializers
    // -----------------------------------------------------------------------
    //
    // The bridge thread reads GW game memory concurrently with the game
    // thread.  If the game thread frees or moves a structure while the
    // bridge thread is iterating it, we get ACCESS_VIOLATION crashes.
    // Each Build*Json() sub-builder reads different GW structures; if any
    // one crashes we want to lose just that section of the snapshot, not
    // the entire tier or IPC connection.
    //
    // MSVC C2712 prevents __try/__except in functions that have C++ objects
    // with destructors (nlohmann::json, std::string, std::vector, etc).
    // The two-function SEH pattern avoids C2712:
    //
    //   TryBuildFoo()         -- has json locals, NO __try
    //     calls BuildFooInto(&result) on success path
    //     or fills {"seh_error":true} on failure path
    //
    //   BuildFooInto(json*)   -- __declspec(noinline), no C++ locals,
    //                             has __try, calls BuildFooJson_SEH
    //
    //   BuildFooJson_SEH(json*)
    //                         -- __declspec(noinline), no C++ locals,
    //                            has __try, calls builder and writes
    //                            *out = BuildFooJson().  MSVC C2712
    //                            triggers because the json return
    //                            value is a temporary with a dtor, so
    //                            this layer does NOT exist; instead
    //                            BuildFooInto does the __try and calls
    //                            a void WriteFooJson(json*) that
    //                            constructs in-place.
    //
    // Wait -- the above paragraph acknowledges C2712 blocks the SEH
    // layer around `*out = BuildFooJson()`.  The working pattern is
    // simpler: BuildFooInto_SEH is __try and calls WriteFooJson(out).
    // WriteFooJson constructs the json directly into *out via
    // move-assignment from the returned temporary.  The key insight
    // is that WriteFooJson is a SEPARATE function call from within
    // __try; the json temporary lives in WriteFooJson's frame, not
    // in BuildFooInto_SEH's frame.  MSVC C2712 only checks the
    // __try function's own locals, not locals of functions it calls.
    // So BuildFooInto_SEH can have __try as long as it has no C++
    // locals of its own.
    //
    // Verified: this is exactly how ReadEquippedItems, ReadLivingAgentSeed,
    // etc. already work in this file -- __try functions that call into
    // game memory, called from json-building code that has destructors.

    // -- Write*Json: construct json directly into an output pointer ----------
    // Each WriteFoo(json* out) does *out = BuildFooJson().
    // The temporary json from BuildFooJson() is constructed in
    // WriteFoo's frame, then move-assigned to *out.  WriteFoo itself
    // has no __try, so C2712 does not apply.

    static void WritePlayerJson(json* out)      { *out = BuildPlayerJson(); }
    static void WriteSkillbarJson(json* out)     { *out = BuildSkillbarJson(); }
    static void WriteMapJson(json* out)          { *out = BuildMapJson(); }
    static void WritePartyBasicsJson(json* out)  { *out = BuildPartyBasicsJson(); }
    static void WriteBotStateJson(json* out)     { *out = BuildBotStateJson(); }
    static void WriteNearbyAgentsJson(json* out) { *out = BuildNearbyAgentsJson(); }
    static void WriteHeroSkillbarsJson(json* out){ *out = BuildHeroSkillbarsJson(); }
    static void WriteTradeJson(json* out)        { *out = BuildTradeJson(); }
    static void WriteDialogJson(json* out)       { *out = BuildDialogJson(); }
    static void WriteMerchantJson(json* out)     { *out = BuildMerchantJson(); }
    static void WriteQuestJson(json* out)        { *out = BuildQuestJson(); }
    static void WriteChatLogJson(json* out)      { *out = BuildChatLogJson(); }
    static void WriteInventoryJson(json* out)    { *out = BuildInventoryJson(); }

    // -- SEH gateways --------------------------------------------------------
    // Call WriteFoo(out) inside __try.  These functions have NO C++ locals
    // with destructors -- only the raw json* parameter and a bool return --
    // so MSVC C2712 does not fire.

    __declspec(noinline) static bool TryWritePlayerJson(json* out) {
        __try { WritePlayerJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildPlayerJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteSkillbarJson(json* out) {
        __try { WriteSkillbarJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildSkillbarJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteMapJson(json* out) {
        __try { WriteMapJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildMapJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWritePartyBasicsJson(json* out) {
        __try { WritePartyBasicsJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildPartyBasicsJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteBotStateJson(json* out) {
        __try { WriteBotStateJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildBotStateJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteNearbyAgentsJson(json* out) {
        __try { WriteNearbyAgentsJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildNearbyAgentsJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteHeroSkillbarsJson(json* out) {
        __try { WriteHeroSkillbarsJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildHeroSkillbarsJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteTradeJson(json* out) {
        __try { WriteTradeJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildTradeJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteDialogJson(json* out) {
        __try { WriteDialogJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildDialogJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteMerchantJson(json* out) {
        __try { WriteMerchantJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildMerchantJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteQuestJson(json* out) {
        __try { WriteQuestJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildQuestJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteChatLogJson(json* out) {
        __try { WriteChatLogJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildChatLogJson");
            return false;
        }
    }
    __declspec(noinline) static bool TryWriteInventoryJson(json* out) {
        __try { WriteInventoryJson(out); return true; }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("[Snapshot] SEH in BuildInventoryJson");
            return false;
        }
    }

    // -- TryBuild* wrappers --------------------------------------------------
    // Each TryBuildFoo() calls TryWriteFoo(&result).  If the write fails
    // (SEH caught), a fallback placeholder is written instead.  These
    // functions have json locals but NO __try, so C2712 does not apply.

    static json TryBuildPlayerJson() {
        json result;
        if (!TryWritePlayerJson(&result)) {
            result = json::object();
            result["seh_error"] = true;
            result["agent_id"] = 0;
        }
        return result;
    }
    static json TryBuildSkillbarJson() {
        json result;
        if (!TryWriteSkillbarJson(&result))
            result = json::array();
        return result;
    }
    static json TryBuildMapJson() {
        json result;
        if (!TryWriteMapJson(&result)) {
            result = json::object();
            result["seh_error"] = true;
        }
        return result;
    }
    static json TryBuildPartyBasicsJson() {
        json result;
        if (!TryWritePartyBasicsJson(&result)) {
            result = json::object();
            result["seh_error"] = true;
            result["size"] = 0;
        }
        return result;
    }
    static json TryBuildBotStateJson() {
        json result;
        if (!TryWriteBotStateJson(&result)) {
            result = json::object();
            result["state"] = "seh_error";
        }
        return result;
    }
    static json TryBuildNearbyAgentsJson() {
        json result;
        if (!TryWriteNearbyAgentsJson(&result))
            result = json::array();
        return result;
    }
    static json TryBuildHeroSkillbarsJson() {
        json result;
        if (!TryWriteHeroSkillbarsJson(&result))
            result = json::array();
        return result;
    }
    static json TryBuildTradeJson() {
        json result;
        if (!TryWriteTradeJson(&result)) {
            result = json::object();
            result["seh_error"] = true;
            result["is_open"] = false;
        }
        return result;
    }
    static json TryBuildDialogJson() {
        json result;
        if (!TryWriteDialogJson(&result)) {
            result = json::object();
            result["seh_error"] = true;
            result["is_open"] = false;
        }
        return result;
    }
    static json TryBuildMerchantJson() {
        json result;
        if (!TryWriteMerchantJson(&result)) {
            result = json::object();
            result["seh_error"] = true;
            result["is_open"] = false;
        }
        return result;
    }
    static json TryBuildQuestJson() {
        json result;
        if (!TryWriteQuestJson(&result)) {
            result = json::object();
            result["seh_error"] = true;
            result["quest_log_size"] = 0;
        }
        return result;
    }
    static json TryBuildChatLogJson() {
        json result;
        if (!TryWriteChatLogJson(&result))
            result = json::array();
        return result;
    }
    static json TryBuildInventoryJson() {
        json result;
        if (!TryWriteInventoryJson(&result)) {
            result = json::object();
            result["seh_error"] = true;
            result["gold_character"] = 0;
            result["gold_storage"] = 0;
        }
        return result;
    }
    // -- Tier serializers using TryBuild* for per-builder isolation -----------

    char* SerializeTier1(uint32_t* outLength) {
        g_tick++;
        json j;
        j["type"] = "snapshot";
        j["tier"] = 1;
        j["tick"] = g_tick;
        j["me"] = TryBuildPlayerJson();
        j["skillbar"] = TryBuildSkillbarJson();
        j["map"] = TryBuildMapJson();
        j["party"] = TryBuildPartyBasicsJson();
        j["bot"] = TryBuildBotStateJson();
        return JsonToHeap(j, outLength);
    }

    char* SerializeTier2(uint32_t* outLength) {
        g_tick++;
        json j;
        j["type"] = "snapshot";
        j["tier"] = 2;
        j["tick"] = g_tick;
        j["me"] = TryBuildPlayerJson();
        j["skillbar"] = TryBuildSkillbarJson();
        j["map"] = TryBuildMapJson();
        j["party"] = TryBuildPartyBasicsJson();
        j["agents"] = TryBuildNearbyAgentsJson();
        j["heroes"] = TryBuildHeroSkillbarsJson();
        j["trade"] = TryBuildTradeJson();
        j["dialog"] = TryBuildDialogJson();
        j["merchant"] = TryBuildMerchantJson();
        j["quests"] = TryBuildQuestJson();
        j["chat"] = TryBuildChatLogJson();
        return JsonToHeap(j, outLength);
    }

    char* SerializeTier3(uint32_t* outLength) {
        g_tick++;
        json j;
        j["type"] = "snapshot";
        j["tier"] = 3;
        j["tick"] = g_tick;
        j["me"] = TryBuildPlayerJson();
        j["skillbar"] = TryBuildSkillbarJson();
        j["map"] = TryBuildMapJson();
        j["party"] = TryBuildPartyBasicsJson();
        j["agents"] = TryBuildNearbyAgentsJson();
        j["heroes"] = TryBuildHeroSkillbarsJson();
        j["trade"] = TryBuildTradeJson();
        j["dialog"] = TryBuildDialogJson();
        j["merchant"] = TryBuildMerchantJson();
        j["quests"] = TryBuildQuestJson();
        j["inventory"] = TryBuildInventoryJson();
        j["storage"] = json::array();
        j["effects"] = json::array();
        j["titles"] = json::array();
        return JsonToHeap(j, outLength);
    }

} // namespace GWA3::LLM::GameSnapshot
