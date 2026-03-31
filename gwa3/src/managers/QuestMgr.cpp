#include <gwa3/managers/QuestMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Log.h>

namespace GWA3::QuestMgr {

static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("QuestMgr: Initialized");
    return true;
}

void Dialog(uint32_t dialogId) {
    CtoS::Dialog(dialogId);
}

void SetActiveQuest(uint32_t questId) {
    CtoS::QuestSetActive(questId);
}

void AbandonQuest(uint32_t questId) {
    CtoS::QuestAbandon(questId);
}

void RequestQuestInfo(uint32_t questId) {
    CtoS::SendPacket(2, Packets::QUEST_REQUEST_INFOS, questId);
}

void SkipCinematic() {
    CtoS::SendPacket(1, Packets::CINEMATIC_SKIP);
}

} // namespace GWA3::QuestMgr
