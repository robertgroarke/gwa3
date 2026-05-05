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
uint32_t g_accept_unclaimed_count = 0u;
uint32_t g_last_accept_unclaimed_bag_index = 0u;
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
uint32_t g_test_last_dialog_id = 0u;
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
GWA3::DialogMgr::DialogButton g_test_dialog_buttons[8] = {};
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

bool Travel(uint32_t, uint32_t, uint32_t, uint32_t) {
    return true;
}

void ReturnToOutpost() {
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

void SetPlayerEquippedItems(uint16_t weaponItemId, uint16_t offhandItemId) {
    if (!g_test_has_player_agent) {
        SetPlayerAgent(0.0f, 0.0f, 1.0f);
    }
    g_test_player_agent.weapon_item_id = weaponItemId;
    g_test_player_agent.offhand_item_id = offhandItemId;
}

void ClearPlayerAgent() {
    g_test_player_agent = {};
    g_test_has_player_agent = false;
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
