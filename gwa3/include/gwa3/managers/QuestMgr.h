#pragma once

#include <gwa3/game/Quest.h>
#include <cstdint>

namespace GWA3::QuestMgr {

    bool Initialize();

    // Dialog
    // Prefers the native SendDialog/SendSignpostDialog path when resolved.
    // Falls back to the raw packet path if no native function pointer is available.
    void Dialog(uint32_t dialogId);

    // Quest management
    void SetActiveQuest(uint32_t questId);
    void AbandonQuest(uint32_t questId);
    void RequestQuestInfo(uint32_t questId);

    // Toggle the in-game Quest Log window. Under the hood this fires
    // ControlAction 0x8E (ControlAction_OpenQuestLog per GWCA),
    // matching AutoIt BotsHub's `PerformAction(0x8E)` path.
    //
    // Toggles: if the window is closed it opens; if open it closes.
    // Side effect of interest: opening it populates GW's UI label
    // frames with decoded quest-name strings, which we can then read
    // via the sibling-decode memory pattern documented in
    // QUEST_LOG_RESEARCH.md. Only works while in an outpost or
    // explorable (ignored in pre-game / char-select).
    void ToggleQuestLogWindow();

    // Walk the UI FrameArray looking for label-style frame contexts
    // that store `encoded | '\0' | decoded | '\0'` text (the pattern
    // documented in GWCA_UIMessage_Research.md 3644-3656). For each
    // pair whose encoded content matches one of the strings in the
    // current quest log, populate EncStringCache so the snapshot
    // reader can surface the decoded form.
    //
    // Returns the number of quest strings resolved. Zero is returned
    // when no labels have been rendered yet (open the Quest Log
    // UI first via ToggleQuestLogWindow to populate the labels).
    uint32_t ScanLabelFramesForQuestStrings();

    // Cinematic
    void SkipCinematic();

    // Quest state reading (WorldContext + 0x528 / 0x52C)
    uint32_t GetActiveQuestId();
    Quest* GetQuestById(uint32_t questId);
    Quest* GetQuestByIndex(uint32_t index);
    uint32_t GetQuestLogSize();

} // namespace GWA3::QuestMgr
