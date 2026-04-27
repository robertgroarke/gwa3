// Froggy Tekks quest and dialog diagnostics.

static void LogTekksQuestSnapshot(const char* label) {
    Quest* quest = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR);
    Log::Info("Froggy: %s activeQuest=0x%X questPresent=%d questLogSize=%u lastDialog=0x%X",
              label,
              QuestMgr::GetActiveQuestId(),
              quest != nullptr ? 1 : 0,
              QuestMgr::GetQuestLogSize(),
              DialogMgr::GetLastDialogId());
    if (quest) {
        Log::Info("Froggy: %s quest: id=0x%X logState=%u map_from=%u map_to=%u marker=(%.0f, %.0f)",
                  label,
                  quest->quest_id,
                  quest->log_state,
                  quest->map_from,
                  quest->map_to,
                  quest->marker_x,
                  quest->marker_y);
    }
}

static bool RefreshTekksQuestReadyForDungeonEntry(const char* label, uint32_t refreshDelayMs) {
    QuestMgr::RequestQuestInfo(GWA3::QuestIds::TEKKS_WAR);
    if (refreshDelayMs > 0u) {
        WaitMs(refreshDelayMs);
    }

    Quest* quest = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR);
    uint32_t activeQuestId = QuestMgr::GetActiveQuestId();
    if (quest != nullptr && activeQuestId != GWA3::QuestIds::TEKKS_WAR) {
        QuestMgr::SetActiveQuest(GWA3::QuestIds::TEKKS_WAR);
        WaitMs(TEKKS_SET_ACTIVE_DWELL_MS);
        activeQuestId = QuestMgr::GetActiveQuestId();
        quest = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR);
    }

    const bool ready = IsTekksQuestReadyForDungeonEntry(quest != nullptr, activeQuestId);
    Log::Info("Froggy: %s Tekks quest ready=%d activeQuest=0x%X questPresent=%d questLogSize=%u",
              label,
              ready ? 1 : 0,
              activeQuestId,
              quest != nullptr ? 1 : 0,
              QuestMgr::GetQuestLogSize());
    return ready;
}

struct TekksDialogSnapshot {
    uint32_t ping = 0u;
    bool quest_present = false;
    DungeonDialog::DialogSnapshot dialog = {};
    bool has_dungeon_entry = false;
    bool has_talk = false;
    bool has_accept = false;
    bool has_reward = false;
};

static TekksDialogSnapshot CaptureTekksDialogSnapshot() {
    TekksDialogSnapshot snapshot;
    snapshot.ping = ChatMgr::GetPing();
    snapshot.quest_present = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR) != nullptr;
    snapshot.dialog = DungeonDialog::CaptureDialogSnapshot();
    snapshot.has_dungeon_entry = DungeonDialog::HasDialogButton(GWA3::DialogIds::TekksWar::DUNGEON_ENTRY);
    snapshot.has_talk = DungeonDialog::HasDialogButton(GWA3::DialogIds::NPC_TALK);
    snapshot.has_accept = DungeonDialog::HasDialogButton(GWA3::DialogIds::TekksWar::QUEST_ACCEPT);
    snapshot.has_reward = DungeonDialog::HasDialogButton(GWA3::DialogIds::TekksWar::QUEST_REWARD);
    return snapshot;
}

static void LogTekksDialogSnapshot(const TekksDialogSnapshot& snapshot) {
    Log::Info("Froggy: Tekks dialog snapshot visible=%d sender=%u buttons=%u hasReward=%d hasAccept=%d hasTalk=%d hasDungeonEntry=%d",
              snapshot.dialog.dialog_open ? 1 : 0,
              snapshot.dialog.sender_agent_id,
              snapshot.dialog.button_count,
              snapshot.has_reward ? 1 : 0,
              snapshot.has_accept ? 1 : 0,
              snapshot.has_talk ? 1 : 0,
              snapshot.has_dungeon_entry ? 1 : 0);
}
