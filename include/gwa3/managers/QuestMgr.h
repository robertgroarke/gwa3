#pragma once

#include <gwa3/game/Quest.h>
#include <cstdint>

namespace GWA3::QuestMgr {

    bool Initialize();

    // Dialog
    void Dialog(uint32_t dialogId);

    // Quest management
    void SetActiveQuest(uint32_t questId);
    void AbandonQuest(uint32_t questId);
    void RequestQuestInfo(uint32_t questId);

    // Cinematic
    void SkipCinematic();

} // namespace GWA3::QuestMgr
