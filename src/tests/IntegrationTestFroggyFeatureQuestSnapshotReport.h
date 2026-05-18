static void ReportQuestSnapshot(const char* label) {
    const uint32_t activeQuest = QuestMgr::GetActiveQuestId();
    const uint32_t questLogSize = QuestMgr::GetQuestLogSize();
    Quest* quest = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR);
    FroggyFeatureReport("  %s: activeQuest=0x%X questLogSize=%u tekksQuest=%p",
              label, activeQuest, questLogSize, quest);
    if (quest) {
        FroggyFeatureReport("    Tekks quest: id=0x%X logState=%u map_from=%u map_to=%u marker=(%.0f, %.0f)",
                  quest->quest_id, quest->log_state, quest->map_from, quest->map_to,
                  quest->marker_x, quest->marker_y);

        uint8_t bytes[8] = {};
        if (TryCaptureQuestObjectiveBytes(quest, bytes)) {
            FroggyFeatureReport("    Tekks quest objectives=%p bytes=%02X %02X %02X %02X %02X %02X %02X %02X",
                      quest->objectives,
                      bytes[0], bytes[1], bytes[2], bytes[3],
                      bytes[4], bytes[5], bytes[6], bytes[7]);
        } else {
            FroggyFeatureReport("    Tekks quest objectives=%p bytes=<unreadable>",
                      quest->objectives);
        }
    }
}
