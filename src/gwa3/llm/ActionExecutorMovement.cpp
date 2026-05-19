#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <bots/froggy/FroggyHM.h>

#include <cmath>
#include <thread>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        bool IsCoordinateOutOfRange(float x, float y) {
            return std::abs(x) > 100000 || std::abs(y) > 100000;
        }

        bool IsMapActionReady() {
            return MapMgr::GetMapId() != 0 && AgentMgr::GetMyId() != 0;
        }

        ActionResult HandleMoveTo(const json& p) {
            if (!p.contains("x") || !p.contains("y")) return MakeError("missing x or y");
            float x = p["x"].get<float>();
            float y = p["y"].get<float>();
            if (IsCoordinateOutOfRange(x, y)) return MakeError("coordinates_out_of_range");
            if (!IsMapActionReady()) return MakeError("map_not_loaded");
            std::thread([x, y]() {
                (void)GWA3::DungeonNavigation::MoveToAndWait(x, y, 250.0f, 30000u);
            }).detach();
            return MakeOk();
        }

        ActionResult HandleAggroMoveTo(const json& p) {
            if (!p.contains("x") || !p.contains("y")) return MakeError("missing x or y");
            float x = p["x"].get<float>();
            float y = p["y"].get<float>();
            if (IsCoordinateOutOfRange(x, y)) return MakeError("coordinates_out_of_range");
            if (!IsMapActionReady()) return MakeError("map_not_loaded");
            const float fightRange = p.value("fight_range", 1350.0f);
            std::thread([x, y, fightRange]() {
                Bot::Froggy::DebugAggroMoveTo(x, y, fightRange);
            }).detach();
            return MakeOk();
        }

        ActionResult HandleChangeTarget(const json& p) {
            if (!p.contains("agent_id")) return MakeError("missing agent_id");
            uint32_t id = p["agent_id"].get<uint32_t>();
            if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
            GWA3::GameThread::Enqueue([id]() { AgentMgr::ChangeTarget(id); });
            return MakeOk();
        }

        ActionResult HandleCancelAction(const json&) {
            GWA3::GameThread::Enqueue([]() { AgentMgr::CancelAction(); });
            return MakeOk();
        }

    } // namespace

    void RegisterMovementActions(ActionDispatchTable& dispatch) {
        dispatch["move_to"] = HandleMoveTo;
        dispatch["aggro_move_to"] = HandleAggroMoveTo;
        dispatch["change_target"] = HandleChangeTarget;
        dispatch["cancel_action"] = HandleCancelAction;
    }

} // namespace GWA3::LLM::ActionExecutor
