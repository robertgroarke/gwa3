#include <gwa3/testing/TestFramework.h>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cmath>
#include <string>
#include <gwa3/game/Agent.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/Quest.h>
#include <gwa3/bot/BotFramework.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/game/Skill.h>
#include <gwa3/packets/Headers.h>

namespace {

constexpr std::size_t kMaxTestAgents = 64;
constexpr std::size_t kMaxDialogHistory = 64;
constexpr std::size_t kMaxEffectSkills = 16;
constexpr std::size_t kMaxTrackedEffectAgents = 16;
constexpr std::size_t kMaxTrackedEffectsPerAgent = 8;
constexpr std::size_t kMaxTestSkillConstants = 32;
constexpr std::size_t kMaxTestBags = 24;
constexpr std::size_t kMaxTestItemsPerBag = 40;

GWA3::AgentLiving g_test_agents[kMaxTestAgents] = {};
std::size_t g_test_agent_count = 0;
uint32_t g_test_agent_max_id = 0u;
GWA3::Inventory g_test_inventory = {};
GWA3::Item g_test_bundle_item = {};
GWA3::Bag g_test_bags[kMaxTestBags] = {};
GWA3::Item g_test_items[kMaxTestBags][kMaxTestItemsPerBag] = {};
GWA3::Item* g_test_item_ptrs[kMaxTestBags][kMaxTestItemsPerBag] = {};
wchar_t g_test_item_names[kMaxTestBags][kMaxTestItemsPerBag][2] = {};
GWA3::Quest g_test_quest = {};
GWA3::Quest g_test_dialog_granted_quest = {};
uint32_t g_test_active_quest_id = 0u;
uint32_t g_test_dialog_grants_quest_dialog_id = 0u;
uint32_t g_test_request_quest_info_count = 0u;
uint32_t g_last_dropped_item_id = 0u;
uint32_t g_last_picked_item_agent_id = 0u;
uint32_t g_last_used_item_id = 0u;
uint32_t g_last_identified_item_id = 0u;
uint32_t g_last_identify_kit_id = 0u;
uint32_t g_last_salvage_kit_id = 0u;
uint32_t g_last_salvaged_item_id = 0u;
uint32_t g_pending_salvage_item_id = 0u;
uint32_t g_last_moved_item_id = 0u;
uint32_t g_last_move_bag_id = 0u;
uint32_t g_last_move_slot = 0u;
uint32_t g_last_transact_type = 0u;
uint32_t g_last_transact_quantity = 0u;
uint32_t g_last_transact_item_id = 0u;
uint32_t g_test_merchant_item_count = 0u;
uint32_t g_last_buy_materials_model_id = 0u;
uint32_t g_last_buy_materials_quantity = 0u;
uint32_t g_test_visible_frame_hash = 0u;
bool g_test_frame_visible = false;
bool g_test_action_key_down_result = false;
bool g_test_perform_ui_action_result = false;
uint32_t g_send_packet_count = 0u;
uint32_t g_last_send_packet_size = 0u;
uint32_t g_last_send_packet_header = 0u;
uint32_t g_last_send_packet_arg1 = 0u;
uint32_t g_last_send_packet_arg2 = 0u;
uint32_t g_last_action_key_down = 0u;
uint32_t g_action_key_down_count = 0u;
uint32_t g_last_perform_ui_action = 0u;
uint32_t g_perform_ui_action_count = 0u;
uint32_t g_last_perform_ui_action_direct = 0u;
uint32_t g_perform_ui_action_direct_count = 0u;
uint32_t g_identify_count = 0u;
uint32_t g_salvage_open_count = 0u;
uint32_t g_salvage_done_count = 0u;
uint32_t g_move_item_count = 0u;
uint32_t g_use_item_count = 0u;
uint32_t g_transact_count = 0u;
uint32_t g_last_interacted_npc_id = 0u;
uint32_t g_last_interacted_signpost_id = 0u;
uint32_t g_last_changed_target_id = 0u;
uint32_t g_last_called_target_id = 0u;
uint32_t g_last_attack_target_id = 0u;
float g_last_move_x = 0.0f;
float g_last_move_y = 0.0f;
float g_last_flag_all_x = 0.0f;
float g_last_flag_all_y = 0.0f;
uint32_t g_move_count = 0u;
uint32_t g_change_target_count = 0u;
uint32_t g_call_target_count = 0u;
uint32_t g_attack_count = 0u;
uint32_t g_action_interact_count = 0u;
uint32_t g_flag_all_count = 0u;
uint32_t g_unflag_all_count = 0u;
uint32_t g_test_map_id = 0u;
uint32_t g_test_loading_state = 1u;
uint32_t g_test_move_sets_map_id = 0u;
float g_test_move_transition_x = 0.0f;
float g_test_move_transition_y = 0.0f;
bool g_test_move_transition_requires_point = false;
uint32_t g_signpost_interaction_count = 0u;
uint32_t g_npc_interaction_count = 0u;
uint32_t g_dialog_history[kMaxDialogHistory] = {};
std::size_t g_dialog_count = 0;
uint32_t g_test_my_agent_id = 1u;
GWA3::AgentLiving g_test_player_agent = {};
bool g_test_has_player_agent = false;
uint32_t g_test_active_title_id = 0u;
uint32_t g_test_effect_skill_ids[kMaxEffectSkills] = {};
std::size_t g_test_effect_skill_count = 0u;
uint32_t g_test_dialog_applies_effect_skill_id = 0u;
uint32_t g_test_dialog_applies_effect_dialog_id = 0u;
uint32_t g_dialog_shutdown_count = 0u;
uint32_t g_dialog_initialize_count = 0u;
bool g_test_dialog_open = false;
uint32_t g_test_dialog_sender_id = 0u;
uint32_t g_test_dialog_button_count = 0u;
bool g_test_auto_open_dialog_on_npc_interact = false;
uint32_t g_test_auto_dialog_button_count = 0u;
GWA3::Bot::BotConfig g_test_bot_config = {};
GWA3::Bot::BotState g_test_bot_state = GWA3::Bot::BotState::Idle;
GWA3::AgentEffects g_test_agent_effects = {};
GWA3::GWArray<GWA3::Effect> g_test_effect_array = {};
GWA3::AgentEffects g_test_agent_effects_by_agent[kMaxTrackedEffectAgents] = {};
GWA3::GWArray<GWA3::Effect> g_test_effect_arrays_by_agent[kMaxTrackedEffectAgents] = {};
GWA3::Effect g_test_effects_by_agent[kMaxTrackedEffectAgents][kMaxTrackedEffectsPerAgent] = {};
uint32_t g_test_effect_agent_ids[kMaxTrackedEffectAgents] = {};
std::size_t g_test_effect_counts_by_agent[kMaxTrackedEffectAgents] = {};
GWA3::Skillbar g_test_player_skillbar = {};
GWA3::Skill g_test_skill_constants[kMaxTestSkillConstants] = {};
uint32_t g_test_skill_constant_ids[kMaxTestSkillConstants] = {};
std::size_t g_test_skill_constant_count = 0u;
uint32_t g_last_used_skill_slot = 0u;
uint32_t g_last_used_skill_target_id = 0u;
uint32_t g_last_used_skill_call_target = 0u;
bool g_test_party_defeated = false;
bool g_test_botshub_queue_idle = true;

} // namespace

namespace GWA3::TestStubs::EffectMgr {
void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration);
}

namespace GWA3::MapMgr {

void Travel(uint32_t, uint32_t, uint32_t, uint32_t) {
}

uint32_t GetMapId() {
    return g_test_map_id;
}

uint32_t GetLoadingState() {
    return g_test_loading_state;
}

bool GetIsMapLoaded() {
    return g_test_loading_state == 1u;
}

} // namespace GWA3::MapMgr

namespace GWA3::TestStubs::MapMgr {

void Reset() {
    g_test_map_id = 0u;
    g_test_loading_state = 1u;
    g_test_move_sets_map_id = 0u;
    g_test_move_transition_x = 0.0f;
    g_test_move_transition_y = 0.0f;
    g_test_move_transition_requires_point = false;
}

void SetMapId(uint32_t mapId) {
    g_test_map_id = mapId;
}

void SetLoadingState(uint32_t loadingState) {
    g_test_loading_state = loadingState;
}

void SetMoveSetsMapId(uint32_t mapId) {
    g_test_move_sets_map_id = mapId;
    g_test_move_transition_requires_point = false;
}

void SetMoveSetsMapIdOnPoint(float x, float y, uint32_t mapId) {
    g_test_move_sets_map_id = mapId;
    g_test_move_transition_x = x;
    g_test_move_transition_y = y;
    g_test_move_transition_requires_point = true;
}

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::Bot {

void Start() {
}

void Stop() {
}

bool IsRunning() {
    return false;
}

BotState GetState() {
    return g_test_bot_state;
}

void SetState(BotState state) {
    g_test_bot_state = state;
}

void RegisterStateHandler(BotState, StateHandler) {
}

BotConfig& GetConfig() {
    return g_test_bot_config;
}

void LoadConfigFromIni(const char*) {
}

void LogBot(const char*, ...) {
}

} // namespace GWA3::Bot

namespace GWA3::GameThread {

bool Initialize() {
    return false;
}

void Shutdown() {
}

void Enqueue(Callback task) {
    if (task) task();
}

void EnqueueRaw(RawInvoker invoker, const void* data, size_t) {
    if (invoker) invoker(const_cast<void*>(data));
}

void EnqueuePostRaw(RawInvoker invoker, const void* data, size_t) {
    if (invoker) invoker(const_cast<void*>(data));
}

void EnqueuePost(Callback task) {
    if (task) task();
}

void RegisterCallback(HookEntry*, Callback, int) {
}

void RemoveCallback(HookEntry*) {
}

bool IsOnGameThread() {
    return true;
}

bool IsInitialized() {
    return false;
}

} // namespace GWA3::GameThread

namespace GWA3::AgentMgr {

void Move(float x, float y) {
    g_last_move_x = x;
    g_last_move_y = y;
    ++g_move_count;
    if (g_test_has_player_agent) {
        g_test_player_agent.x = x;
        g_test_player_agent.y = y;
    }
    if (g_test_move_sets_map_id != 0u &&
        (!g_test_move_transition_requires_point ||
         (std::fabs(g_test_move_transition_x - x) < 0.01f &&
          std::fabs(g_test_move_transition_y - y) < 0.01f))) {
        g_test_map_id = g_test_move_sets_map_id;
    }
}

void ResetMoveState(const char*) {
}

void ChangeTarget(uint32_t agentId) {
    g_last_changed_target_id = agentId;
    ++g_change_target_count;
}

void ForceChangeTarget(uint32_t agentId) {
    g_last_changed_target_id = agentId;
    ++g_change_target_count;
}

void Attack(uint32_t agentId) {
    g_last_attack_target_id = agentId;
    ++g_attack_count;
}

bool ActionInteract() {
    ++g_action_interact_count;
    return true;
}

bool InteractAgentWorldAction(uint32_t agentId, bool) {
    g_last_interacted_signpost_id = agentId;
    ++g_signpost_interaction_count;
    return true;
}

void CallTarget(uint32_t agentId) {
    g_last_called_target_id = agentId;
    ++g_call_target_count;
}

void CancelAction() {
    if (g_test_has_player_agent) {
        g_test_player_agent.move_x = 0.0f;
        g_test_player_agent.move_y = 0.0f;
    }
}

void InteractNPC(uint32_t agentId) {
    g_last_interacted_npc_id = agentId;
    ++g_npc_interaction_count;
    if (g_test_auto_open_dialog_on_npc_interact) {
        g_test_dialog_open = true;
        g_test_dialog_sender_id = agentId;
        g_test_dialog_button_count = g_test_auto_dialog_button_count;
    }
}

void InteractSignpost(uint32_t agentId) {
    g_last_interacted_signpost_id = agentId;
    ++g_signpost_interaction_count;
}

void InteractSignpostLegacy(uint32_t agentId) {
    g_last_interacted_signpost_id = agentId;
    ++g_signpost_interaction_count;
}

float GetDistance(float x1, float y1, float x2, float y2) {
    const float dx = x1 - x2;
    const float dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

AgentLiving* GetMyAgent() {
    return g_test_has_player_agent ? &g_test_player_agent : nullptr;
}

bool IsCasting(const AgentLiving* agent) {
    if (!agent) return false;
    return agent->skill != 0u ||
           agent->model_state == 0x41u ||
           agent->model_state == 0x245u ||
           agent->model_state == 0x645u;
}

Agent* GetAgentByID(uint32_t agentId) {
    for (std::size_t i = 0; i < g_test_agent_count; ++i) {
        if (g_test_agents[i].agent_id == agentId) {
            return reinterpret_cast<Agent*>(&g_test_agents[i]);
        }
    }
    return nullptr;
}

bool GetAgentExists(uint32_t agentId) {
    return GetAgentByID(agentId) != nullptr;
}

uint32_t GetMaxAgents() {
    return g_test_agent_max_id + 1u;
}

uint32_t GetMyId() {
    return g_test_my_agent_id;
}

uint32_t GetTargetId() {
    return g_last_changed_target_id;
}

float GetSquaredDistance(float x1, float y1, float x2, float y2) {
    const float dx = x1 - x2;
    const float dy = y1 - y2;
    return dx * dx + dy * dy;
}

} // namespace GWA3::AgentMgr

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents() {
    for (std::size_t i = 0; i < kMaxTestAgents; ++i) {
        g_test_agents[i] = {};
    }
    g_test_agent_count = 0;
    g_test_agent_max_id = 0u;
    g_last_interacted_npc_id = 0u;
    g_last_interacted_signpost_id = 0u;
    g_last_changed_target_id = 0u;
    g_last_called_target_id = 0u;
    g_last_attack_target_id = 0u;
    g_last_move_x = 0.0f;
    g_last_move_y = 0.0f;
    g_last_flag_all_x = 0.0f;
    g_last_flag_all_y = 0.0f;
    g_move_count = 0u;
    g_change_target_count = 0u;
    g_call_target_count = 0u;
    g_attack_count = 0u;
    g_action_interact_count = 0u;
    g_flag_all_count = 0u;
    g_unflag_all_count = 0u;
    g_signpost_interaction_count = 0u;
    g_npc_interaction_count = 0u;
    g_test_player_agent = {};
    g_test_player_agent.agent_id = g_test_my_agent_id;
    g_test_player_agent.hp = 1.0f;
    g_test_has_player_agent = true;
    g_test_botshub_queue_idle = true;
}

void AddAgent(uint32_t agentId, float x, float y, uint32_t type) {
    if (g_test_agent_count >= kMaxTestAgents) {
        return;
    }

    auto& agent = g_test_agents[g_test_agent_count++];
    agent = {};
    agent.agent_id = agentId;
    agent.x = x;
    agent.y = y;
    agent.type = type;
    agent.hp = 1.0f;
    if (agentId > g_test_agent_max_id) {
        g_test_agent_max_id = agentId;
    }
}

void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp) {
    if (g_test_agent_count >= kMaxTestAgents) {
        return;
    }

    auto& agent = g_test_agents[g_test_agent_count++];
    agent = {};
    agent.agent_id = agentId;
    agent.x = x;
    agent.y = y;
    agent.type = 0xDBu;
    agent.allegiance = allegiance;
    agent.hp = hp;
    if (agentId > g_test_agent_max_id) {
        g_test_agent_max_id = agentId;
    }
}

void AddItemAgent(uint32_t agentId, float x, float y, uint32_t itemId, uint32_t owner) {
    if (g_test_agent_count >= kMaxTestAgents) {
        return;
    }

    auto& agent = g_test_agents[g_test_agent_count++];
    agent = {};
    agent.agent_id = agentId;
    agent.x = x;
    agent.y = y;
    agent.type = 0x400u;
    agent.owner = owner;
    agent.h00C8_living = itemId;
    if (agentId > g_test_agent_max_id) {
        g_test_agent_max_id = agentId;
    }
}

void AddGadgetAgent(uint32_t agentId, float x, float y, uint32_t gadgetId) {
    if (g_test_agent_count >= kMaxTestAgents) {
        return;
    }

    auto& agent = g_test_agents[g_test_agent_count++];
    agent = {};
    agent.agent_id = agentId;
    agent.x = x;
    agent.y = y;
    agent.type = 0x200u;
    agent.h00D0_living = gadgetId;
    if (agentId > g_test_agent_max_id) {
        g_test_agent_max_id = agentId;
    }
}

void SetPlayerAgent(float x, float y, float hp) {
    g_test_player_agent = {};
    g_test_player_agent.agent_id = g_test_my_agent_id;
    g_test_player_agent.type = 0xDBu;
    g_test_player_agent.allegiance = 1u;
    g_test_player_agent.x = x;
    g_test_player_agent.y = y;
    g_test_player_agent.hp = hp;
    g_test_player_agent.energy = 1.0f;
    g_test_player_agent.max_energy = 30u;
    g_test_player_agent.model_state = 0u;
    g_test_player_agent.hex = 0u;
    g_test_player_agent.skill = 0u;
    g_test_has_player_agent = true;
}

uint32_t LastInteractedNpcId() {
    return g_last_interacted_npc_id;
}

uint32_t LastInteractedSignpostId() {
    return g_last_interacted_signpost_id;
}

uint32_t NpcInteractionCount() {
    return g_npc_interaction_count;
}

uint32_t SignpostInteractionCount() {
    return g_signpost_interaction_count;
}

uint32_t ActionInteractCount() {
    return g_action_interact_count;
}

uint32_t MoveCount() {
    return g_move_count;
}

uint32_t ChangeTargetCount() {
    return g_change_target_count;
}

uint32_t CallTargetCount() {
    return g_call_target_count;
}

uint32_t AttackCount() {
    return g_attack_count;
}

uint32_t LastChangedTargetId() {
    return g_last_changed_target_id;
}

uint32_t LastCalledTargetId() {
    return g_last_called_target_id;
}

uint32_t LastAttackTargetId() {
    return g_last_attack_target_id;
}

float LastMoveX() {
    return g_last_move_x;
}

float LastMoveY() {
    return g_last_move_y;
}

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::PartyMgr {

void FlagAll(float x, float y) {
    g_last_flag_all_x = x;
    g_last_flag_all_y = y;
    ++g_flag_all_count;
}

void UnflagAll() {
    ++g_unflag_all_count;
}

bool GetIsPartyDefeated() {
    return g_test_party_defeated;
}

} // namespace GWA3::PartyMgr

namespace GWA3::TestStubs::PartyMgr {

void ResetFlags() {
    g_last_flag_all_x = 0.0f;
    g_last_flag_all_y = 0.0f;
    g_flag_all_count = 0u;
    g_unflag_all_count = 0u;
    g_test_party_defeated = false;
}

uint32_t FlagAllCount() {
    return g_flag_all_count;
}

uint32_t UnflagAllCount() {
    return g_unflag_all_count;
}

float LastFlagAllX() {
    return g_last_flag_all_x;
}

float LastFlagAllY() {
    return g_last_flag_all_y;
}

void SetPartyDefeated(bool defeated) {
    g_test_party_defeated = defeated;
}

} // namespace GWA3::TestStubs::PartyMgr

namespace GWA3::TestStubs::ItemMgr {

void Reset() {
    g_test_inventory = {};
    g_test_bundle_item = {};
    for (std::size_t bagIndex = 0; bagIndex < kMaxTestBags; ++bagIndex) {
        g_test_bags[bagIndex] = {};
        for (std::size_t slotIndex = 0; slotIndex < kMaxTestItemsPerBag; ++slotIndex) {
            g_test_items[bagIndex][slotIndex] = {};
            g_test_item_ptrs[bagIndex][slotIndex] = nullptr;
            g_test_item_names[bagIndex][slotIndex][0] = 0;
            g_test_item_names[bagIndex][slotIndex][1] = 0;
        }
    }
    g_last_dropped_item_id = 0u;
    g_last_picked_item_agent_id = 0u;
    g_last_used_item_id = 0u;
    g_last_identified_item_id = 0u;
    g_last_identify_kit_id = 0u;
    g_last_salvage_kit_id = 0u;
    g_last_salvaged_item_id = 0u;
    g_pending_salvage_item_id = 0u;
    g_last_moved_item_id = 0u;
    g_last_move_bag_id = 0u;
    g_last_move_slot = 0u;
    g_last_transact_type = 0u;
    g_last_transact_quantity = 0u;
    g_last_transact_item_id = 0u;
    g_identify_count = 0u;
    g_salvage_open_count = 0u;
    g_salvage_done_count = 0u;
    g_move_item_count = 0u;
    g_use_item_count = 0u;
    g_transact_count = 0u;
}

void SetGold(uint32_t charGold, uint32_t storageGold) {
    g_test_inventory.gold_character = charGold;
    g_test_inventory.gold_storage = storageGold;
}

void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount) {
    if (bagIndex >= kMaxTestBags) {
        return;
    }
    if (slotCount > kMaxTestItemsPerBag) {
        slotCount = static_cast<uint32_t>(kMaxTestItemsPerBag);
    }

    auto& bag = g_test_bags[bagIndex];
    bag = {};
    bag.index = bagIndex;
    bag.items.buffer = g_test_item_ptrs[bagIndex];
    bag.items.capacity = static_cast<uint32_t>(kMaxTestItemsPerBag);
    bag.items.size = slotCount;
    bag.items_count = 0u;
    g_test_inventory.bags[bagIndex] = &bag;
}

void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity) {
    if (bagIndex >= kMaxTestBags || slotIndex >= kMaxTestItemsPerBag) {
        return;
    }
    if (g_test_inventory.bags[bagIndex] == nullptr) {
        SetBagCapacity(bagIndex, static_cast<uint32_t>(slotIndex + 1u));
    }

    auto& item = g_test_items[bagIndex][slotIndex];
    item = {};
    item.item_id = itemId;
    item.model_id = modelId;
    item.type = type;
    item.quantity = quantity;
    item.value = value;
    item.interaction = interaction;
    if (rarity != 0u) {
        g_test_item_names[bagIndex][slotIndex][0] = static_cast<wchar_t>(rarity);
        item.name_enc = g_test_item_names[bagIndex][slotIndex];
    }
    g_test_item_ptrs[bagIndex][slotIndex] = &item;

    auto* bag = g_test_inventory.bags[bagIndex];
    item.bag = bag;
    if (bag && slotIndex >= bag->items.size) {
        bag->items.size = static_cast<uint32_t>(slotIndex + 1u);
    }
    if (bag) {
        uint32_t count = 0u;
        for (uint32_t i = 0u; i < bag->items.size; ++i) {
            if (g_test_item_ptrs[bagIndex][i] != nullptr) {
                ++count;
            }
        }
        bag->items_count = count;
    }
}

void ClearBagItem(uint32_t bagIndex, uint32_t slotIndex) {
    if (bagIndex >= kMaxTestBags || slotIndex >= kMaxTestItemsPerBag) {
        return;
    }
    g_test_items[bagIndex][slotIndex] = {};
    g_test_item_ptrs[bagIndex][slotIndex] = nullptr;
    g_test_item_names[bagIndex][slotIndex][0] = 0;
    g_test_item_names[bagIndex][slotIndex][1] = 0;
    auto* bag = g_test_inventory.bags[bagIndex];
    if (bag) {
        uint32_t count = 0u;
        for (uint32_t i = 0u; i < bag->items.size; ++i) {
            if (g_test_item_ptrs[bagIndex][i] != nullptr) {
                ++count;
            }
        }
        bag->items_count = count;
    }
}

void SetHeldBundleItemId(uint32_t itemId) {
    g_test_bundle_item = {};
    g_test_bundle_item.item_id = itemId;
    g_test_inventory.bundle = itemId == 0u ? nullptr : &g_test_bundle_item;
}

uint32_t LastDroppedItemId() {
    return g_last_dropped_item_id;
}

uint32_t LastPickedItemAgentId() {
    return g_last_picked_item_agent_id;
}

uint32_t LastUsedItemId() {
    return g_last_used_item_id;
}

uint32_t LastIdentifiedItemId() {
    return g_last_identified_item_id;
}

uint32_t LastIdentifyKitId() {
    return g_last_identify_kit_id;
}

uint32_t IdentifyCount() {
    return g_identify_count;
}

uint32_t LastSalvagedItemId() {
    return g_last_salvaged_item_id;
}

uint32_t LastSalvageKitId() {
    return g_last_salvage_kit_id;
}

uint32_t SalvageOpenCount() {
    return g_salvage_open_count;
}

uint32_t SalvageDoneCount() {
    return g_salvage_done_count;
}

uint32_t LastMovedItemId() {
    return g_last_moved_item_id;
}

uint32_t LastMoveBagId() {
    return g_last_move_bag_id;
}

uint32_t LastMoveSlot() {
    return g_last_move_slot;
}

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs() {
    for (std::size_t i = 0; i < kMaxDialogHistory; ++i) {
        g_dialog_history[i] = 0u;
    }
    g_dialog_count = 0;
}

std::size_t DialogCount() {
    return g_dialog_count;
}

uint32_t DialogAt(std::size_t index) {
    return index < g_dialog_count ? g_dialog_history[index] : 0u;
}

void ResetQuestState() {
    g_test_quest = {};
    g_test_dialog_granted_quest = {};
    g_test_active_quest_id = 0u;
    g_test_dialog_grants_quest_dialog_id = 0u;
    g_test_request_quest_info_count = 0u;
}

void SetQuest(uint32_t questId, uint32_t logState) {
    g_test_quest = {};
    g_test_quest.quest_id = questId;
    g_test_quest.log_state = logState;
}

void SetQuestGrantedOnDialog(uint32_t dialogId, uint32_t questId, uint32_t logState) {
    g_test_dialog_grants_quest_dialog_id = dialogId;
    g_test_dialog_granted_quest = {};
    g_test_dialog_granted_quest.quest_id = questId;
    g_test_dialog_granted_quest.log_state = logState;
}

uint32_t RequestQuestInfoCount() {
    return g_test_request_quest_info_count;
}

void SetBlessingDialogEffect(uint32_t dialogId, uint32_t skillId) {
    g_test_dialog_applies_effect_dialog_id = dialogId;
    g_test_dialog_applies_effect_skill_id = skillId;
}

} // namespace GWA3::TestStubs::QuestMgr

namespace GWA3::ItemMgr {

namespace {

void RecountBag(Bag* bag) {
    if (!bag || !bag->items.buffer) return;
    uint32_t count = 0u;
    for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
        if (bag->items.buffer[slot] != nullptr) {
            ++count;
        }
    }
    bag->items_count = count;
}

Item* DetachItem(uint32_t itemId) {
    for (std::size_t bagIndex = 0; bagIndex < kMaxTestBags; ++bagIndex) {
        auto* bag = g_test_inventory.bags[bagIndex];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (item && item->item_id == itemId) {
                bag->items.buffer[slot] = nullptr;
                item->bag = nullptr;
                RecountBag(bag);
                return item;
            }
        }
    }
    return nullptr;
}

} // namespace

Inventory* GetInventory() {
    return &g_test_inventory;
}

Bag* GetBag(uint32_t bagIndex) {
    return bagIndex < kMaxTestBags ? g_test_inventory.bags[bagIndex] : nullptr;
}

Item* GetItemById(uint32_t itemId) {
    if (g_test_inventory.bundle && g_test_inventory.bundle->item_id == itemId) {
        return g_test_inventory.bundle;
    }
    for (std::size_t bagIndex = 0; bagIndex < kMaxTestBags; ++bagIndex) {
        auto* bag = g_test_inventory.bags[bagIndex];
        if (!bag || !bag->items.buffer) {
            continue;
        }
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            auto* item = bag->items.buffer[slot];
            if (item && item->item_id == itemId) {
                return item;
            }
        }
    }
    return nullptr;
}

uint32_t GetGoldCharacter() {
    return g_test_inventory.gold_character;
}

uint32_t GetGoldStorage() {
    return g_test_inventory.gold_storage;
}

void ChangeGold(uint32_t charGold, uint32_t storageGold) {
    g_test_inventory.gold_character = charGold;
    g_test_inventory.gold_storage = storageGold;
}

void DropItem(uint32_t itemId) {
    g_last_dropped_item_id = itemId;
    if (g_test_inventory.bundle && g_test_inventory.bundle->item_id == itemId) {
        g_test_inventory.bundle = nullptr;
        return;
    }
    (void)DetachItem(itemId);
}

void PickUpItem(uint32_t itemAgentId) {
    g_last_picked_item_agent_id = itemAgentId;
    auto* agent = GWA3::AgentMgr::GetAgentByID(itemAgentId);
    if (!agent || agent->type != 0x400u) {
        return;
    }

    auto* itemAgent = static_cast<GWA3::AgentItem*>(agent);
    auto* item = GetItemById(itemAgent->item_id);
    if (item && item->type == 6u) {
        GWA3::TestStubs::ItemMgr::SetHeldBundleItemId(item->item_id);
    }
}

void UseItem(uint32_t itemId) {
    g_last_used_item_id = itemId;
    ++g_use_item_count;
}

void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot) {
    g_last_moved_item_id = itemId;
    g_last_move_bag_id = bagId;
    g_last_move_slot = slot;
    ++g_move_item_count;

    auto* item = DetachItem(itemId);
    auto* bag = GetBag(bagId);
    if (!item || !bag || !bag->items.buffer || slot >= bag->items.capacity) {
        return;
    }
    if (slot >= bag->items.size) {
        bag->items.size = slot + 1u;
    }
    bag->items.buffer[slot] = item;
    RecountBag(bag);
}

void IdentifyItem(uint32_t itemId, uint32_t kitId) {
    g_last_identified_item_id = itemId;
    g_last_identify_kit_id = kitId;
    ++g_identify_count;
    auto* item = GetItemById(itemId);
    if (item) {
        item->interaction |= 0x1u;
    }
}

void SalvageSessionOpen(uint32_t kitId, uint32_t itemId) {
    g_last_salvage_kit_id = kitId;
    g_last_salvaged_item_id = itemId;
    g_pending_salvage_item_id = itemId;
    ++g_salvage_open_count;
}

void SalvageMaterials() {
    if (g_pending_salvage_item_id != 0u) {
        (void)DetachItem(g_pending_salvage_item_id);
    }
}

void SalvageSessionDone() {
    g_pending_salvage_item_id = 0u;
    ++g_salvage_done_count;
}

} // namespace GWA3::ItemMgr

namespace GWA3::QuestMgr {

void Dialog(uint32_t dialogId) {
    if (g_dialog_count >= kMaxDialogHistory) {
        return;
    }
    g_dialog_history[g_dialog_count++] = dialogId;
    if (dialogId == g_test_dialog_applies_effect_dialog_id &&
        g_test_dialog_applies_effect_skill_id != 0u &&
        g_test_effect_skill_count < kMaxEffectSkills) {
        g_test_effect_skill_ids[g_test_effect_skill_count++] = g_test_dialog_applies_effect_skill_id;
        GWA3::TestStubs::EffectMgr::AddEffectForAgent(
            g_test_my_agent_id,
            g_test_dialog_applies_effect_skill_id,
            10.0f);
    }
    if (dialogId == g_test_dialog_grants_quest_dialog_id &&
        g_test_dialog_granted_quest.quest_id != 0u) {
        g_test_quest = g_test_dialog_granted_quest;
    }
}

void SetActiveQuest(uint32_t questId) {
    g_test_active_quest_id = questId;
}

void RequestQuestInfo(uint32_t) {
    ++g_test_request_quest_info_count;
}

uint32_t GetActiveQuestId() {
    return g_test_active_quest_id;
}

Quest* GetQuestById(uint32_t questId) {
    return g_test_quest.quest_id == questId ? &g_test_quest : nullptr;
}

} // namespace GWA3::QuestMgr

namespace GWA3::Log {

void Info(const char*, ...) {
}

} // namespace GWA3::Log

namespace GWA3::TestStubs::TradeMgr {

void Reset() {
    g_last_transact_type = 0u;
    g_last_transact_quantity = 0u;
    g_last_transact_item_id = 0u;
    g_transact_count = 0u;
    g_test_merchant_item_count = 0u;
    g_last_buy_materials_model_id = 0u;
    g_last_buy_materials_quantity = 0u;
}

void SetMerchantItemCount(uint32_t count) {
    g_test_merchant_item_count = count;
}

uint32_t LastTransactType() {
    return g_last_transact_type;
}

uint32_t LastTransactQuantity() {
    return g_last_transact_quantity;
}

uint32_t LastTransactItemId() {
    return g_last_transact_item_id;
}

uint32_t TransactCount() {
    return g_transact_count;
}

uint32_t LastBuyMaterialsModelId() {
    return g_last_buy_materials_model_id;
}

uint32_t LastBuyMaterialsQuantity() {
    return g_last_buy_materials_quantity;
}

} // namespace GWA3::TestStubs::TradeMgr

namespace GWA3::TradeMgr {

void BuyMaterials(uint32_t modelId, uint32_t quantity) {
    g_last_buy_materials_model_id = modelId;
    g_last_buy_materials_quantity = quantity;
}

uint32_t GetMerchantItemCount() {
    return g_test_merchant_item_count;
}

void TransactItems(uint32_t type, uint32_t quantity, uint32_t itemId) {
    g_last_transact_type = type;
    g_last_transact_quantity = quantity;
    g_last_transact_item_id = itemId;
    ++g_transact_count;
    GWA3::ItemMgr::DropItem(itemId);
}

} // namespace GWA3::TradeMgr

namespace GWA3::TestStubs::UIMgr {

void Reset() {
    g_test_visible_frame_hash = 0u;
    g_test_frame_visible = false;
    g_test_action_key_down_result = false;
    g_test_perform_ui_action_result = false;
    g_last_action_key_down = 0u;
    g_action_key_down_count = 0u;
    g_last_perform_ui_action = 0u;
    g_perform_ui_action_count = 0u;
    g_last_perform_ui_action_direct = 0u;
    g_perform_ui_action_direct_count = 0u;
}

void SetFrameVisible(uint32_t hash, bool visible) {
    g_test_visible_frame_hash = hash;
    g_test_frame_visible = visible;
}

void SetActionKeyDownResult(bool result) {
    g_test_action_key_down_result = result;
}

void SetPerformUiActionResult(bool result) {
    g_test_perform_ui_action_result = result;
}

uint32_t LastActionKeyDown() {
    return g_last_action_key_down;
}

uint32_t ActionKeyDownCount() {
    return g_action_key_down_count;
}

uint32_t LastPerformUiAction() {
    return g_last_perform_ui_action;
}

uint32_t PerformUiActionCount() {
    return g_perform_ui_action_count;
}

uint32_t LastPerformUiActionDirect() {
    return g_last_perform_ui_action_direct;
}

uint32_t PerformUiActionDirectCount() {
    return g_perform_ui_action_direct_count;
}

} // namespace GWA3::TestStubs::UIMgr

namespace GWA3::UIMgr {

bool IsFrameVisible(uint32_t hash) {
    return g_test_frame_visible && g_test_visible_frame_hash == hash;
}

bool ActionKeyDown(uint32_t action) {
    g_last_action_key_down = action;
    ++g_action_key_down_count;
    return g_test_action_key_down_result;
}

bool PerformUiAction(uint32_t action) {
    g_last_perform_ui_action = action;
    ++g_perform_ui_action_count;
    return g_test_perform_ui_action_result;
}

bool PerformUiActionDirect(uint32_t action) {
    g_last_perform_ui_action_direct = action;
    ++g_perform_ui_action_direct_count;
    return g_test_perform_ui_action_result;
}

} // namespace GWA3::UIMgr

namespace GWA3::TestStubs::CtoS {

void Reset() {
    g_send_packet_count = 0u;
    g_last_send_packet_size = 0u;
    g_last_send_packet_header = 0u;
    g_last_send_packet_arg1 = 0u;
    g_last_send_packet_arg2 = 0u;
}

uint32_t SendPacketCount() {
    return g_send_packet_count;
}

uint32_t LastSendPacketHeader() {
    return g_last_send_packet_header;
}

uint32_t LastSendPacketArg1() {
    return g_last_send_packet_arg1;
}

uint32_t LastSendPacketArg2() {
    return g_last_send_packet_arg2;
}

} // namespace GWA3::TestStubs::CtoS

namespace GWA3::CtoS {

bool IsBotshubQueueIdle() {
    return g_test_botshub_queue_idle;
}

void SuspendEngineHook() {
}

void ResumeEngineHook() {
}

void SendPacket(uint32_t size, uint32_t header, ...) {
    g_last_send_packet_size = size;
    g_last_send_packet_header = header;
    ++g_send_packet_count;

    va_list args;
    va_start(args, header);
    g_last_send_packet_arg1 = va_arg(args, uint32_t);
    g_last_send_packet_arg2 = va_arg(args, uint32_t);
    va_end(args);
}

void SendPacketDirect(uint32_t size, uint32_t header, ...) {
    g_last_send_packet_size = size;
    g_last_send_packet_header = header;
    ++g_send_packet_count;

    va_list args;
    va_start(args, header);
    g_last_send_packet_arg1 = va_arg(args, uint32_t);
    g_last_send_packet_arg2 = va_arg(args, uint32_t);
    va_end(args);

    if (header == GWA3::Packets::INTERACT_NPC && g_test_auto_open_dialog_on_npc_interact) {
        g_test_dialog_open = true;
        g_test_dialog_sender_id = g_last_send_packet_arg1;
        g_test_dialog_button_count = g_test_auto_dialog_button_count;
    }
}

} // namespace GWA3::CtoS

namespace GWA3::TestStubs::EffectMgr {

void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration);

void Reset() {
    for (std::size_t i = 0; i < kMaxEffectSkills; ++i) {
        g_test_effect_skill_ids[i] = 0u;
    }
    g_test_effect_skill_count = 0u;
    for (std::size_t agentIndex = 0; agentIndex < kMaxTrackedEffectAgents; ++agentIndex) {
        g_test_effect_agent_ids[agentIndex] = 0u;
        g_test_effect_counts_by_agent[agentIndex] = 0u;
        g_test_agent_effects_by_agent[agentIndex] = {};
        g_test_effect_arrays_by_agent[agentIndex] = {};
        for (std::size_t effectIndex = 0; effectIndex < kMaxTrackedEffectsPerAgent; ++effectIndex) {
            g_test_effects_by_agent[agentIndex][effectIndex] = {};
        }
    }
}

void AddEffect(uint32_t skillId) {
    if (g_test_effect_skill_count >= kMaxEffectSkills) {
        return;
    }
    g_test_effect_skill_ids[g_test_effect_skill_count++] = skillId;
    AddEffectForAgent(g_test_my_agent_id, skillId, 10.0f);
}

void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration) {
    if (agentId == 0u || skillId == 0u) {
        return;
    }

    std::size_t slot = kMaxTrackedEffectAgents;
    for (std::size_t i = 0; i < kMaxTrackedEffectAgents; ++i) {
        if (g_test_effect_agent_ids[i] == agentId) {
            slot = i;
            break;
        }
        if (slot == kMaxTrackedEffectAgents && g_test_effect_agent_ids[i] == 0u) {
            slot = i;
        }
    }
    if (slot >= kMaxTrackedEffectAgents) {
        return;
    }

    if (g_test_effect_agent_ids[slot] == 0u) {
        g_test_effect_agent_ids[slot] = agentId;
    }
    if (g_test_effect_counts_by_agent[slot] >= kMaxTrackedEffectsPerAgent) {
        return;
    }

    auto& effect = g_test_effects_by_agent[slot][g_test_effect_counts_by_agent[slot]++];
    effect = {};
    effect.agent_id = agentId;
    effect.skill_id = skillId;
    effect.duration = duration;
}

} // namespace GWA3::TestStubs::EffectMgr

namespace GWA3::EffectMgr {

AgentEffects* GetAgentEffects(uint32_t agentId) {
    for (std::size_t i = 0; i < kMaxTrackedEffectAgents; ++i) {
        if (g_test_effect_agent_ids[i] != agentId) {
            continue;
        }
        g_test_effect_arrays_by_agent[i].buffer = g_test_effects_by_agent[i];
        g_test_effect_arrays_by_agent[i].capacity = static_cast<uint32_t>(kMaxTrackedEffectsPerAgent);
        g_test_effect_arrays_by_agent[i].size = static_cast<uint32_t>(g_test_effect_counts_by_agent[i]);
        g_test_effect_arrays_by_agent[i].growth = 0u;
        g_test_agent_effects_by_agent[i].agent_id = agentId;
        g_test_agent_effects_by_agent[i].buffs = {};
        g_test_agent_effects_by_agent[i].effects = g_test_effect_arrays_by_agent[i];
        return &g_test_agent_effects_by_agent[i];
    }
    return nullptr;
}

AgentEffects* GetPlayerEffects() {
    return GetAgentEffects(g_test_my_agent_id);
}

GWArray<Effect>* GetAgentEffectArray(uint32_t agentId) {
    auto* effects = GetAgentEffects(agentId);
    return effects ? &effects->effects : nullptr;
}

GWArray<Buff>* GetAgentBuffArray(uint32_t agentId) {
    auto* effects = GetAgentEffects(agentId);
    return effects ? &effects->buffs : nullptr;
}

Effect* GetEffectBySkillId(uint32_t agentId, uint32_t skillId) {
    auto* effects = GetAgentEffects(agentId);
    if (!effects || !effects->effects.buffer) {
        return nullptr;
    }
    for (uint32_t i = 0; i < effects->effects.size; ++i) {
        if (effects->effects.buffer[i].skill_id == skillId) {
            return &effects->effects.buffer[i];
        }
    }
    return nullptr;
}

Buff* GetBuffBySkillId(uint32_t agentId, uint32_t skillId) {
    auto* effects = GetAgentEffects(agentId);
    if (!effects || !effects->buffs.buffer) {
        return nullptr;
    }
    for (uint32_t i = 0; i < effects->buffs.size; ++i) {
        if (effects->buffs.buffer[i].skill_id == skillId) {
            return &effects->buffs.buffer[i];
        }
    }
    return nullptr;
}

bool HasEffect(uint32_t agentId, uint32_t skillId) {
    auto* effects = GetAgentEffects(agentId);
    if (!effects || !effects->effects.buffer) {
        return false;
    }
    for (uint32_t i = 0; i < effects->effects.size; ++i) {
        if (effects->effects.buffer[i].skill_id == skillId) {
            return true;
        }
    }
    return false;
}

bool DropBuff(uint32_t) {
    return false;
}

float GetEffectTimeRemaining(uint32_t agentId, uint32_t skillId) {
    auto* effects = GetAgentEffects(agentId);
    if (!effects || !effects->effects.buffer) {
        return 0.0f;
    }
    for (uint32_t i = 0; i < effects->effects.size; ++i) {
        if (effects->effects.buffer[i].skill_id == skillId) {
            return effects->effects.buffer[i].duration;
        }
    }
    return 0.0f;
}

} // namespace GWA3::EffectMgr

namespace GWA3::TestStubs::SkillMgr {

void Reset() {
    g_test_player_skillbar = {};
    g_test_player_skillbar.agent_id = g_test_my_agent_id;
    g_test_skill_constant_count = 0u;
    for (std::size_t i = 0; i < kMaxTestSkillConstants; ++i) {
        g_test_skill_constants[i] = {};
        g_test_skill_constant_ids[i] = 0u;
    }
    g_last_used_skill_slot = 0u;
    g_last_used_skill_target_id = 0u;
    g_last_used_skill_call_target = 0u;
}

void SetSkillData(uint32_t skillId, uint32_t type, uint8_t target, uint8_t energyCost,
                  float activation, float aftercast, uint32_t recharge, uint32_t adrenaline) {
    for (std::size_t i = 0; i < g_test_skill_constant_count; ++i) {
        if (g_test_skill_constant_ids[i] != skillId) {
            continue;
        }
        g_test_skill_constants[i].skill_id = skillId;
        g_test_skill_constants[i].type = type;
        g_test_skill_constants[i].target = target;
        g_test_skill_constants[i].energy_cost = energyCost;
        g_test_skill_constants[i].activation = activation;
        g_test_skill_constants[i].aftercast = aftercast;
        g_test_skill_constants[i].recharge = recharge;
        g_test_skill_constants[i].adrenaline = adrenaline;
        return;
    }
    if (g_test_skill_constant_count >= kMaxTestSkillConstants) {
        return;
    }
    const std::size_t index = g_test_skill_constant_count++;
    g_test_skill_constant_ids[index] = skillId;
    g_test_skill_constants[index] = {};
    g_test_skill_constants[index].skill_id = skillId;
    g_test_skill_constants[index].type = type;
    g_test_skill_constants[index].target = target;
    g_test_skill_constants[index].energy_cost = energyCost;
    g_test_skill_constants[index].activation = activation;
    g_test_skill_constants[index].aftercast = aftercast;
    g_test_skill_constants[index].recharge = recharge;
    g_test_skill_constants[index].adrenaline = adrenaline;
}

void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t recharge, uint32_t adrenaline) {
    if (slot < 1u || slot > 8u) {
        return;
    }
    auto& skill = g_test_player_skillbar.skills[slot - 1u];
    skill.skill_id = skillId;
    skill.recharge = recharge;
    skill.adrenaline_a = adrenaline;
}

uint32_t LastUsedSkillSlot() {
    return g_last_used_skill_slot;
}

uint32_t LastUsedSkillTargetId() {
    return g_last_used_skill_target_id;
}

uint32_t LastUsedSkillCallTarget() {
    return g_last_used_skill_call_target;
}

} // namespace GWA3::TestStubs::SkillMgr

namespace GWA3::SkillMgr {

void UseSkill(uint32_t slot, uint32_t targetAgentId, uint32_t callTarget) {
    g_last_used_skill_slot = slot;
    g_last_used_skill_target_id = targetAgentId;
    g_last_used_skill_call_target = callTarget;
    if (slot >= 1u && slot <= 8u) {
        g_test_player_skillbar.skills[slot - 1u].recharge = 100u;
    }
}

void UseHeroSkill(uint32_t, uint32_t, uint32_t) {
}

void LoadSkillbar(const uint32_t skillIds[8], uint32_t) {
    for (uint32_t i = 0; i < 8u; ++i) {
        g_test_player_skillbar.skills[i].skill_id = skillIds ? skillIds[i] : 0u;
        g_test_player_skillbar.skills[i].recharge = 0u;
        g_test_player_skillbar.skills[i].adrenaline_a = 0u;
        g_test_player_skillbar.skills[i].adrenaline_b = 0u;
    }
}

void SetSkillbarSkill(uint32_t slot, uint32_t skillId, uint32_t) {
    if (slot < 1u || slot > 8u) {
        return;
    }
    g_test_player_skillbar.skills[slot - 1u].skill_id = skillId;
}

void ToggleHeroSkillSlot(uint32_t, uint32_t) {
}

Skillbar* GetPlayerSkillbar() {
    g_test_player_skillbar.agent_id = g_test_my_agent_id;
    return &g_test_player_skillbar;
}

Skillbar* GetSkillbarByAgentId(uint32_t agentId) {
    return agentId == g_test_my_agent_id ? &g_test_player_skillbar : nullptr;
}

SkillbarSkill* GetSkillbarSkill(uint32_t slot) {
    if (slot < 1u || slot > 8u) {
        return nullptr;
    }
    return &g_test_player_skillbar.skills[slot - 1u];
}

const Skill* GetSkillConstantData(uint32_t skillId) {
    for (std::size_t i = 0; i < g_test_skill_constant_count; ++i) {
        if (g_test_skill_constant_ids[i] == skillId) {
            return &g_test_skill_constants[i];
        }
    }
    return nullptr;
}

} // namespace GWA3::SkillMgr

namespace GWA3::TestStubs::PlayerMgr {

void Reset() {
    g_test_active_title_id = 0u;
}

} // namespace GWA3::TestStubs::PlayerMgr

namespace GWA3::PlayerMgr {

bool SetActiveTitle(uint32_t titleId) {
    g_test_active_title_id = titleId;
    return true;
}

uint32_t GetActiveTitleId() {
    return g_test_active_title_id;
}

} // namespace GWA3::PlayerMgr

namespace GWA3::TestStubs::DialogMgr {

void Reset() {
    g_dialog_shutdown_count = 0u;
    g_dialog_initialize_count = 0u;
    g_test_dialog_open = false;
    g_test_dialog_sender_id = 0u;
    g_test_dialog_button_count = 0u;
    g_test_auto_open_dialog_on_npc_interact = false;
    g_test_auto_dialog_button_count = 0u;
}

uint32_t ShutdownCount() {
    return g_dialog_shutdown_count;
}

uint32_t InitializeCount() {
    return g_dialog_initialize_count;
}

void SetDialogState(bool open, uint32_t senderAgentId, uint32_t buttonCount) {
    g_test_dialog_open = open;
    g_test_dialog_sender_id = senderAgentId;
    g_test_dialog_button_count = buttonCount;
}

void SetAutoDialogOnNpcInteract(bool enabled, uint32_t buttonCount) {
    g_test_auto_open_dialog_on_npc_interact = enabled;
    g_test_auto_dialog_button_count = buttonCount;
}

} // namespace GWA3::TestStubs::DialogMgr

namespace GWA3::DialogMgr {

bool Initialize() {
    ++g_dialog_initialize_count;
    return true;
}

void Shutdown() {
    ++g_dialog_shutdown_count;
}

bool IsDialogOpen() {
    return g_test_dialog_open;
}

uint32_t GetDialogSenderAgentId() {
    return g_test_dialog_sender_id;
}

uint32_t GetButtonCount() {
    return g_test_dialog_button_count;
}

void ClearDialog() {
    g_test_dialog_open = false;
    g_test_dialog_sender_id = 0u;
    g_test_dialog_button_count = 0u;
}

void ResetHookState() {
}

void ResetRecentUITrace() {
}

} // namespace GWA3::DialogMgr

#include "test_bot_module_selector.cpp"
#include "test_bot_module_registry.cpp"
#include "test_arachnis_haunt.cpp"
#include "test_arachnis_haunt_bot.cpp"
#include "test_dungeon_bundle.cpp"
#include "test_dungeon_combat.cpp"
#include "test_dungeon_combat_routine.cpp"
#include "test_dungeon_checkpoint.cpp"
#include "test_dungeon_dialog.cpp"
#include "test_dungeon_item_actions.cpp"
#include "test_dungeon_inventory.cpp"
#include "test_dungeon_item_policy.cpp"
#include "test_dungeon_interactions.cpp"
#include "test_dungeon_loot.cpp"
#include "test_dungeon_quest.cpp"
#include "test_dungeon_quest_runtime.cpp"
#include "test_dungeon_skill.cpp"
#include "test_froggy_shared_adapters.cpp"
#include "test_frostmaws_burrows.cpp"
#include "test_frostmaws_burrows_bot.cpp"
#include "test_kathandrax_bot.cpp"
#include "test_kathandrax.cpp"
#include "test_dungeon_navigation.cpp"
#include "test_dungeon_outpost_setup.cpp"
#include "test_dungeon_route.cpp"
#include "test_ravens_point.cpp"
#include "test_ravens_point_bot.cpp"
#include "test_rragars_menagerie.cpp"
#include "test_rragars_menagerie_bot.cpp"
#include "test_dungeon_travel.cpp"
#include "test_dungeon_vendor.cpp"

int main() {
    return GWA3::Testing::RunAll();
}
