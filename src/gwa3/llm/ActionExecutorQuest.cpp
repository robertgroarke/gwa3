#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/managers/QuestMgr.h>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        ActionResult HandleSetActiveQuest(const json& p) {
            if (!p.contains("quest_id")) return MakeError("missing quest_id");
            uint32_t id = p["quest_id"].get<uint32_t>();
            if (id == 0) return MakeError("quest_id_zero");
            GWA3::GameThread::Enqueue([id]() { QuestMgr::SetActiveQuest(id); });
            return MakeOk();
        }

        ActionResult HandleAbandonQuest(const json& p) {
            if (!p.contains("quest_id")) return MakeError("missing quest_id");
            uint32_t id = p["quest_id"].get<uint32_t>();
            if (id == 0) return MakeError("quest_id_zero");
            if (!QuestMgr::GetQuestById(id)) return MakeError("quest_not_in_log");
            GWA3::GameThread::Enqueue([id]() { QuestMgr::AbandonQuest(id); });
            return MakeOk();
        }

        ActionResult HandleRequestQuestInfo(const json& p) {
            if (!p.contains("quest_id")) return MakeError("missing quest_id");
            uint32_t id = p["quest_id"].get<uint32_t>();
            if (id == 0) return MakeError("quest_id_zero");
            GWA3::GameThread::Enqueue([id]() { QuestMgr::RequestQuestInfo(id); });
            return MakeOk();
        }

        ActionResult HandleOpenQuestLog(const json&) {
            GWA3::GameThread::Enqueue([]() { QuestMgr::ToggleQuestLogWindow(); });
            return MakeOk();
        }

    } // namespace

    void RegisterQuestActions(ActionDispatchTable& dispatch) {
        dispatch["set_active_quest"] = HandleSetActiveQuest;
        dispatch["abandon_quest"] = HandleAbandonQuest;
        dispatch["request_quest_info"] = HandleRequestQuestInfo;
        dispatch["open_quest_log"] = HandleOpenQuestLog;
    }

} // namespace GWA3::LLM::ActionExecutor
