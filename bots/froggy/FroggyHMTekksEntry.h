// Froggy Tekks quest and dungeon-entry preparation. Froggy supplies the
// Tekks-specific ids, coordinates, and timings; DungeonQuestRuntime owns the
// reusable quest-giver entry flow.

static DungeonQuestRuntime::QuestGiverEntryPlan MakeTekksDungeonEntryPlan() {
    const auto bootstrap = GetEntryBootstrapPlan();
    DungeonQuestRuntime::QuestGiverEntryPlan plan = {};
    plan.npc = bootstrap.npc;
    plan.npc.search_radius = TEKKS_NPC_SEARCH_RADIUS;
    plan.quest_id = GWA3::QuestIds::TEKKS_WAR;
    plan.dialogs.talk = GWA3::DialogIds::NPC_TALK;
    plan.dialogs.accept = GWA3::DialogIds::TekksWar::QUEST_ACCEPT;
    plan.dialogs.reward = GWA3::DialogIds::TekksWar::QUEST_REWARD;
    plan.dialogs.dungeon_entry = GWA3::DialogIds::TekksWar::DUNGEON_ENTRY;
    return plan;
}

static DungeonQuestRuntime::QuestGiverEntryOptions MakeTekksDungeonEntryOptions() {
    DungeonQuestRuntime::QuestGiverEntryOptions options = {};
    options.anchor_move_threshold = TEKKS_ANCHOR_MOVE_THRESHOLD;
    options.pre_interact_dwell_ms = TEKKS_PRE_INTERACT_DWELL_MS;
    options.cancel_dwell_ms = TEKKS_CANCEL_DWELL_MS;
    options.npc_move_threshold = TEKKS_NPC_MOVE_THRESHOLD;
    options.npc_settle_timeout_ms = TEKKS_NPC_SETTLE_TIMEOUT_MS;
    options.npc_settle_distance = TEKKS_NPC_SETTLE_DISTANCE;
    options.initial_interact_target_wait_ms = TEKKS_INITIAL_INTERACT_TARGET_WAIT_MS;
    options.initial_interact_pass_wait_ms = TEKKS_INITIAL_INTERACT_PASS_WAIT_MS;
    options.initial_interact_passes = TEKKS_INITIAL_INTERACT_PASSES;
    options.post_interact_dwell_ms = TEKKS_POST_INTERACT_DWELL_MS;
    options.direct_entry_wait_base_ms = TEKKS_DIRECT_ENTRY_WAIT_BASE_MS;
    options.reward_first_wait_base_ms = TEKKS_REWARD_FIRST_WAIT_BASE_MS;
    options.dialog_refresh_delay_ms = TEKKS_DIALOG_REFRESH_DELAY_MS;
    options.post_reward_wait_base_ms = TEKKS_POST_REWARD_WAIT_BASE_MS;
    options.post_reward_max_buttons_per_pass = TEKKS_POST_REWARD_MAX_BUTTONS_PER_PASS;
    options.post_reward_max_passes = TEKKS_POST_REWARD_MAX_PASSES;
    options.reopen_accept_target_wait_base_ms = TEKKS_REOPEN_ACCEPT_TARGET_WAIT_BASE_MS;
    options.reopen_accept_pass_wait_ms = TEKKS_REOPEN_ACCEPT_PASS_WAIT_MS;
    options.reopen_accept_interact_passes = TEKKS_REOPEN_ACCEPT_INTERACT_PASSES;
    options.reopen_accept_attempts = TEKKS_REOPEN_ACCEPT_ATTEMPTS;
    options.accept_wait_base_ms = TEKKS_ACCEPT_WAIT_BASE_MS;
    options.accept_verify.timeout_ms = TEKKS_ACCEPT_VERIFY_TIMEOUT_MS;
    options.accept_verify.refresh_interval_ms = TEKKS_ACCEPT_VERIFY_REFRESH_INTERVAL_MS;
    options.accept_verify.refresh_delay_ms = TEKKS_DIALOG_REFRESH_DELAY_MS;
    options.accept_verify.poll_ms = TEKKS_ACCEPT_VERIFY_POLL_MS;
    options.accept_verify.post_set_active_delay_ms = TEKKS_SET_ACTIVE_DWELL_MS;
    options.talk_wait_base_ms = TEKKS_TALK_WAIT_BASE_MS;
    options.entry_dialog_wait_base_ms = TEKKS_ENTRY_DIALOG_WAIT_BASE_MS;
    options.entry_verify_wait_base_ms = TEKKS_ENTRY_VERIFY_WAIT_BASE_MS;
    options.entry_verify_refresh_interval_ms = TEKKS_ENTRY_VERIFY_REFRESH_INTERVAL_MS;
    options.entry_verify_poll_ms = TEKKS_ENTRY_VERIFY_POLL_MS;
    options.post_set_active_delay_ms = TEKKS_SET_ACTIVE_DWELL_MS;
    options.log_prefix = "Froggy";
    options.label = "Tekks";
    return options;
}

static bool RefreshTekksQuestReadyForDungeonEntry(const char* label, uint32_t refreshDelayMs) {
    DungeonQuestRuntime::QuestReadyOptions options = {};
    options.refresh_delay_ms = refreshDelayMs;
    options.post_set_active_delay_ms = TEKKS_SET_ACTIVE_DWELL_MS;
    options.log_prefix = "Froggy";
    options.label = label;
    return DungeonQuestRuntime::RefreshQuestReadyForDungeonEntry(
        GWA3::QuestIds::TEKKS_WAR,
        options).ready;
}

static bool PrepareTekksDungeonEntry() {
    const auto result = DungeonQuestRuntime::PrepareDungeonEntryFromQuestGiver(
        MakeTekksDungeonEntryPlan(),
        MakeTekksDungeonEntryOptions());
    return result.confirmed;
}
