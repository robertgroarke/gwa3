#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/QuestMgr.h>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        ActionResult HandleInteractNpc(const json& p) {
            if (!p.contains("agent_id")) return MakeError("missing agent_id");
            uint32_t id = p["agent_id"].get<uint32_t>();
            if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
            GWA3::GameThread::Enqueue([id]() { AgentMgr::InteractNPC(id); });
            return MakeOk();
        }

        ActionResult HandleInteractPlayer(const json& p) {
            if (!p.contains("agent_id")) return MakeError("missing agent_id");
            uint32_t id = p["agent_id"].get<uint32_t>();
            if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
            GWA3::GameThread::Enqueue([id]() { AgentMgr::InteractPlayer(id); });
            return MakeOk();
        }

        ActionResult HandleInteractSignpost(const json& p) {
            if (!p.contains("agent_id")) return MakeError("missing agent_id");
            uint32_t id = p["agent_id"].get<uint32_t>();
            if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
            GWA3::GameThread::Enqueue([id]() { AgentMgr::InteractSignpost(id); });
            return MakeOk();
        }

        ActionResult HandleDialog(const json& p) {
            if (!p.contains("dialog_id")) return MakeError("missing dialog_id");
            uint32_t id = p["dialog_id"].get<uint32_t>();
            GWA3::GameThread::Enqueue([id]() { QuestMgr::Dialog(id); });
            return MakeOk();
        }

    } // namespace

    void RegisterInteractionActions(ActionDispatchTable& dispatch) {
        dispatch["interact_npc"] = HandleInteractNpc;
        dispatch["interact_player"] = HandleInteractPlayer;
        dispatch["interact_signpost"] = HandleInteractSignpost;
        dispatch["dialog"] = HandleDialog;
    }

} // namespace GWA3::LLM::ActionExecutor
