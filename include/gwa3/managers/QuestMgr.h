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
    // frames with decoded quest-name strings, which can then be read
    // via the sibling-decode memory pattern. Only works while in an outpost or
    // explorable (ignored in pre-game / char-select).
    void ToggleQuestLogWindow();

    // Cinematic
    void SkipCinematic();

    // Quest state reading (WorldContext + 0x528 / 0x52C)
    uint32_t GetActiveQuestId();
    Quest* GetQuestById(uint32_t questId);
    Quest* GetQuestByIndex(uint32_t index);
    uint32_t GetQuestLogSize();

} // namespace GWA3::QuestMgr
