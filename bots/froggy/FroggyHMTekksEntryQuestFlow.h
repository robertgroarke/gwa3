// Froggy Tekks quest dialog and dungeon-entry flow helpers.

static DungeonInteractions::DirectNpcInteractOptions MakeTekksDirectInteractOptions(
    uint32_t targetWaitMs,
    uint32_t passWaitMs,
    int passes,
    const char* label) {
    DungeonInteractions::DirectNpcInteractOptions options = {};
    options.target_wait_ms = targetWaitMs;
    options.pass_wait_ms = passWaitMs;
    options.passes = passes;
    options.log_prefix = "Froggy";
    options.label = label;
    options.wait_ms = &WaitMs;
    return options;
}

static DungeonQuestRuntime::QuestDialogResult SendTekksQuestDialog(
    uint32_t dialogId,
    const char* label,
    uint32_t postDialogWaitMs,
    uint32_t refreshDelayMs = TEKKS_DIALOG_REFRESH_DELAY_MS) {
    DungeonQuestRuntime::QuestDialogOptions options = {};
    options.post_dialog_wait_ms = postDialogWaitMs;
    options.refresh_delay_ms = refreshDelayMs;
    options.log_prefix = "Froggy";
    options.label = label;
    return DungeonQuestRuntime::SendDialogAndRefreshQuest(dialogId, GWA3::QuestIds::TEKKS_WAR, options);
}

static void AdvanceTekksPostRewardDialog(uint32_t tekksId, uint32_t ping) {
    DungeonDialog::DialogAdvanceOptions advanceOptions = {};
    advanceOptions.sender_agent_id = tekksId;
    advanceOptions.post_dialog_wait_ms = TEKKS_POST_REWARD_WAIT_BASE_MS + ping;
    advanceOptions.max_buttons_per_pass = TEKKS_POST_REWARD_MAX_BUTTONS_PER_PASS;
    advanceOptions.max_passes = TEKKS_POST_REWARD_MAX_PASSES;
    advanceOptions.log_prefix = "Froggy";
    advanceOptions.label = "Tekks post-reward";
    (void)DungeonDialog::AdvanceDialogToButton(GWA3::DialogIds::TekksWar::QUEST_ACCEPT, advanceOptions);
}

static void ReopenTekksAcceptAfterReward(uint32_t tekksId, uint32_t ping) {
    auto acceptInteract = MakeTekksDirectInteractOptions(
        TEKKS_REOPEN_ACCEPT_TARGET_WAIT_BASE_MS + ping,
        TEKKS_REOPEN_ACCEPT_PASS_WAIT_MS,
        TEKKS_REOPEN_ACCEPT_INTERACT_PASSES,
        "Tekks re-interact for new accept");
    for (int go = 0; go < TEKKS_REOPEN_ACCEPT_ATTEMPTS; ++go) {
        (void)DungeonInteractions::PulseDirectNpcInteract(tekksId, acceptInteract);
        if (!(DialogMgr::IsDialogOpen() &&
              DialogMgr::GetDialogSenderAgentId() == tekksId)) {
            continue;
        }
        if (DungeonDialog::HasDialogButton(GWA3::DialogIds::TekksWar::QUEST_ACCEPT)) {
            break;
        }
        AdvanceTekksPostRewardDialog(tekksId, ping);
        if (DungeonDialog::HasDialogButton(GWA3::DialogIds::TekksWar::QUEST_ACCEPT)) {
            break;
        }
    }
}

static bool VerifyTekksEntryReady(uint32_t tekksId, uint32_t timeoutMs);

static bool AcceptTekksQuestAndEnter(uint32_t tekksId, uint32_t ping) {
    Log::Info("Froggy: Tekks sending accept dialog 0x%X", GWA3::DialogIds::TekksWar::QUEST_ACCEPT);
    QuestMgr::Dialog(GWA3::DialogIds::TekksWar::QUEST_ACCEPT);
    WaitMs(TEKKS_ACCEPT_WAIT_BASE_MS + ping);
    DungeonQuestRuntime::QuestVerificationOptions tekksAcceptVerify = {};
    tekksAcceptVerify.timeout_ms = TEKKS_ACCEPT_VERIFY_TIMEOUT_MS;
    tekksAcceptVerify.refresh_interval_ms = TEKKS_ACCEPT_VERIFY_REFRESH_INTERVAL_MS;
    tekksAcceptVerify.refresh_delay_ms = TEKKS_DIALOG_REFRESH_DELAY_MS;
    tekksAcceptVerify.poll_ms = TEKKS_ACCEPT_VERIFY_POLL_MS;
    const bool acceptedQuestPresent =
        DungeonQuestRuntime::WaitForQuestState(GWA3::QuestIds::TEKKS_WAR, true, tekksAcceptVerify);
    if (acceptedQuestPresent && QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR) != nullptr) {
        QuestMgr::SetActiveQuest(GWA3::QuestIds::TEKKS_WAR);
        WaitMs(TEKKS_SET_ACTIVE_DWELL_MS);
    }
    LogTekksQuestSnapshot("Tekks accept snapshot");
    if (!RefreshTekksQuestReadyForDungeonEntry("Tekks accept verification")) {
        Log::Info("Froggy: Tekks accept did not place quest in log; refusing dungeon-entry dialog");
        return false;
    }

    Log::Info("Froggy: Tekks sending talk dialog 0x%X", GWA3::DialogIds::NPC_TALK);
    QuestMgr::Dialog(GWA3::DialogIds::NPC_TALK);
    WaitMs(TEKKS_TALK_WAIT_BASE_MS + ping);
    Log::Info("Froggy: Tekks after Dialog(0x%X) lastDialog=0x%X", GWA3::DialogIds::NPC_TALK, DialogMgr::GetLastDialogId());

    (void)SendTekksQuestDialog(GWA3::DialogIds::TekksWar::DUNGEON_ENTRY, "Tekks dungeon-entry", TEKKS_ENTRY_DIALOG_WAIT_BASE_MS + ping);
    LogTekksQuestSnapshot("Tekks dungeon-entry complete snapshot");

    const bool finalQuestPresent = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR) != nullptr;
    const uint32_t finalActiveQuest = QuestMgr::GetActiveQuestId();
    const bool entryReady = VerifyTekksEntryReady(tekksId, TEKKS_ENTRY_VERIFY_WAIT_BASE_MS + ping);
    const bool confirmed = IsTekksDungeonEntryConfirmed(finalQuestPresent, finalActiveQuest, entryReady);
    Log::Info("Froggy: Tekks dungeon entry sequence complete questPresent=%d activeQuest=0x%X lastDialog=0x%X entryReady=%d confirmed=%d",
              finalQuestPresent ? 1 : 0,
              finalActiveQuest,
              DialogMgr::GetLastDialogId(),
              entryReady ? 1 : 0,
              confirmed ? 1 : 0);
    return confirmed;
}

static bool VerifyTekksEntryReady(uint32_t tekksId, uint32_t timeoutMs) {
    DungeonQuestRuntime::DungeonEntryReadyOptions options = {};
    options.quest_id = GWA3::QuestIds::TEKKS_WAR;
    options.entry_dialog_id = GWA3::DialogIds::TekksWar::DUNGEON_ENTRY;
    options.npc_id = tekksId;
    options.timeout_ms = timeoutMs;
    options.refresh_interval_ms = TEKKS_ENTRY_VERIFY_REFRESH_INTERVAL_MS;
    options.poll_ms = TEKKS_ENTRY_VERIFY_POLL_MS;
    options.log_prefix = "Froggy";
    options.label = "Tekks entry verify";
    return DungeonQuestRuntime::WaitForDungeonEntryReady(options).ready;
}
