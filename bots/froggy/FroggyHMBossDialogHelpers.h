// Froggy boss reward dialog readiness and interaction helpers.

static bool HasBossRewardButton() {
    return DungeonDialog::HasDialogButton(GWA3::DialogIds::TekksWar::QUEST_REWARD);
}

static bool StopWhenBossRewardDialogReady(uint32_t npcId, void*) {
    return DungeonDialog::IsDialogOpenFromSenderWithButton(npcId, GWA3::DialogIds::TekksWar::QUEST_REWARD);
}

static void LogBossRewardButtons(const char* label) {
    DungeonDialog::LogDialogButtons("Froggy", label ? label : "Boss reward buttons");
}

static DungeonDialog::DialogAdvanceOptions MakeBossRewardAdvanceOptions(uint32_t npcId, int maxPasses) {
    DungeonDialog::DialogAdvanceOptions options = {};
    options.sender_agent_id = npcId;
    options.request_quest_id_after_dialog = GWA3::QuestIds::TEKKS_WAR;
    options.max_passes = maxPasses;
    options.change_target_before_dialog = true;
    options.log_prefix = "Froggy";
    options.label = "Boss reward";
    return options;
}

static bool AdvanceBossDialogToReward(uint32_t npcId, int maxPasses) {
    LogBossRewardButtons("Boss reward advance");
    const auto options = MakeBossRewardAdvanceOptions(npcId, maxPasses);
    return DungeonDialog::AdvanceDialogToButton(GWA3::DialogIds::TekksWar::QUEST_REWARD, options);
}

static DungeonInteractions::DirectNpcInteractOptions MakeBossRewardDirectInteractOptions() {
    DungeonInteractions::DirectNpcInteractOptions options = {};
    options.target_wait_ms = BOSS_REWARD_INTERACT_TARGET_WAIT_MS;
    options.pass_wait_ms = BOSS_REWARD_INTERACT_PASS_WAIT_MS;
    options.passes = BOSS_REWARD_INTERACT_PASSES;
    options.log_prefix = "Froggy";
    options.label = "Boss reward";
    options.wait_ms = &WaitMs;
    options.stop_condition = &StopWhenBossRewardDialogReady;
    return options;
}

static void RetryBossRewardDialogAfterFailedAccept() {
    Log::Info("Froggy: Boss reward retrying direct dialog button 0x%X", GWA3::DialogIds::TekksWar::QUEST_REWARD);
    DungeonQuestRuntime::QuestDialogOptions retryOptions = {};
    retryOptions.post_dialog_wait_ms = BOSS_REWARD_RETRY_POST_DIALOG_WAIT_MS;
    retryOptions.refresh_delay_ms = BOSS_REWARD_RETRY_REFRESH_DELAY_MS;
    retryOptions.log_prefix = "Froggy";
    retryOptions.label = "Boss reward retry";
    (void)DungeonQuestRuntime::SendDialogAndRefreshQuest(
        GWA3::DialogIds::TekksWar::QUEST_REWARD,
        GWA3::QuestIds::TEKKS_WAR,
        retryOptions);
}
