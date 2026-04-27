// Froggy boss reward acceptance helpers. Included by FroggyHMBossWaypointHandler.h.

static void AcceptBossRewardFromNpc(uint32_t tekksId) {
    LogAgentIdentity("Boss reward NPC", tekksId);
    auto* npc = AgentMgr::GetAgentByID(tekksId);
    if (npc) MoveToAndWait(npc->x, npc->y, BOSS_REWARD_NPC_MOVE_THRESHOLD);
    DialogMgr::ClearDialog();
    DialogMgr::ResetHookState();
    const auto interactOptions = MakeBossRewardDirectInteractOptions();
    (void)DungeonInteractions::PulseDirectNpcInteract(tekksId, interactOptions);
    WaitMs(BOSS_REWARD_DIALOG_DWELL_MS);
    AgentMgr::ChangeTarget(tekksId);
    WaitMs(BOSS_REWARD_TARGET_SETTLE_MS);
    const bool rewardButtonReady = AdvanceBossDialogToReward(tekksId, BOSS_REWARD_ADVANCE_MAX_PASSES);
    Log::Info("Froggy: Boss reward prep target=%u sender=%u buttons=%u dialogOpen=%d",
              AgentMgr::GetTargetId(),
              DialogMgr::GetDialogSenderAgentId(),
              DialogMgr::GetButtonCount(),
              DialogMgr::IsDialogOpen() ? 1 : 0);
    Log::Info("Froggy: Boss reward button ready=%d", rewardButtonReady ? 1 : 0);
    LogBossRewardButtons("Boss reward prep");
    const bool rewardCleared = DungeonQuestRuntime::AcceptQuestRewardWithRetry(
        GWA3::QuestIds::TEKKS_WAR,
        tekksId,
        BOSS_REWARD_ACCEPT_RETRY_TIMEOUT_MS);
    Log::Info("Froggy: Boss QuestReward cleared=%d questPresent=%d",
              rewardCleared ? 1 : 0,
              QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR) ? 1 : 0);
    if (!rewardCleared && DialogMgr::IsDialogOpen() &&
        DialogMgr::GetDialogSenderAgentId() == tekksId &&
        HasBossRewardButton()) {
        AgentMgr::ChangeTarget(tekksId);
        WaitMs(BOSS_REWARD_TARGET_SETTLE_MS);
        RetryBossRewardDialogAfterFailedAccept();
    }
}

static void AcceptBossRewardFallback() {
    Log::Info("Froggy: Boss reward NPC not found near staging coords; sending reward dialog directly");
    const bool rewardCleared = DungeonQuestRuntime::AcceptQuestRewardWithRetry(
        GWA3::QuestIds::TEKKS_WAR,
        0,
        BOSS_REWARD_ACCEPT_RETRY_TIMEOUT_MS);
    Log::Info("Froggy: Boss QuestReward fallback cleared=%d questPresent=%d",
              rewardCleared ? 1 : 0,
              QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR) ? 1 : 0);
    DungeonDialog::SendDialogWithRetry(
        GWA3::DialogIds::TekksWar::QUEST_REWARD,
        BOSS_REWARD_FALLBACK_SEND_ATTEMPTS,
        BOSS_REWARD_FALLBACK_SEND_DELAY_MS);
    QuestMgr::RequestQuestInfo(GWA3::QuestIds::TEKKS_WAR);
    WaitMs(BOSS_REWARD_FALLBACK_REFRESH_DELAY_MS);
}
