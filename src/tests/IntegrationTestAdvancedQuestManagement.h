bool TestQuestManagement() {
    IntReport("=== Quest Management ===");

    if (ReadMyId() == 0) { IntSkip("QuestMgmt", "Not in game"); IntReport(""); return false; }

    const uint32_t activeQuestBefore = QuestMgr::GetActiveQuestId();
    const uint32_t alternateQuestId = FindAlternateQuestId(activeQuestBefore);
    IntReport("  Active quest before mutation: %u", activeQuestBefore);
    IntReport("  Alternate quest candidate: %u", alternateQuestId);
    if (activeQuestBefore == 0) {
        IntSkip("SetActiveQuest", "No active quest selected");
    } else if (alternateQuestId == 0) {
        IntSkip("SetActiveQuest", "No alternate quest available in quest log");
    } else {
    IntReport("  SetActiveQuest(0) — deselect...");
    QuestMgr::SetActiveQuest(alternateQuestId);
    Sleep(1000);
    const uint32_t activeQuestAfterSet = QuestMgr::GetActiveQuestId();
    IntReport("  Active quest after set: %u", activeQuestAfterSet);
    IntCheck("SetActiveQuest switched active quest", activeQuestAfterSet == alternateQuestId);

    IntReport("  Restoring active quest %u...", activeQuestBefore);
    QuestMgr::SetActiveQuest(activeQuestBefore);
    Sleep(1000);
    const uint32_t activeQuestAfterRestore = QuestMgr::GetActiveQuestId();
    IntReport("  Active quest after restore: %u", activeQuestAfterRestore);
    IntCheck("SetActiveQuest restored original active quest", activeQuestAfterRestore == activeQuestBefore);
    }

    IntReport("  SkipCinematic...");
    MapMgr::SkipCinematic();
    Sleep(300);
    IntSkip("SkipCinematic", "No observable cinematic state exposed in current harness");

    IntReport("");
    return true;
}

// ===== UI Frame Interaction =====
