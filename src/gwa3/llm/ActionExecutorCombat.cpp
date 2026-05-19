#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/game/Agent.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        bool IsMapActionReady() {
            return MapMgr::GetMapId() != 0 && AgentMgr::GetMyId() != 0;
        }

        ActionResult HandleAttack(const json& p) {
            if (!p.contains("agent_id")) return MakeError("missing agent_id");
            uint32_t id = p["agent_id"].get<uint32_t>();
            if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
            if (!IsMapActionReady()) return MakeError("map_not_loaded");
            auto* agent = AgentMgr::GetAgentByID(id);
            if (agent && agent->type == 0xDB) {
                auto* living = reinterpret_cast<AgentLiving*>(agent);
                if (living->hp <= 0.0f) return MakeError("target_dead");
            }
            GWA3::GameThread::Enqueue([id]() { AgentMgr::Attack(id); });
            return MakeOk();
        }

        ActionResult HandleCallTarget(const json& p) {
            if (!p.contains("agent_id")) return MakeError("missing agent_id");
            uint32_t id = p["agent_id"].get<uint32_t>();
            if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
            GWA3::GameThread::Enqueue([id]() { AgentMgr::CallTarget(id); });
            return MakeOk();
        }

        ActionResult HandleUseSkill(const json& p) {
            if (!p.contains("slot")) return MakeError("missing slot");
            uint32_t slot = p["slot"].get<uint32_t>();
            if (slot >= 8) return MakeError("invalid_slot");
            if (!IsMapActionReady()) return MakeError("map_not_loaded");

            auto* skill = SkillMgr::GetSkillbarSkill(slot);
            if (skill && skill->recharge > 0) return MakeError("skill_on_recharge");

            uint32_t target = p.value("target_agent_id", 0u);
            uint32_t callTarget = p.value("call_target", 0u);
            uint32_t nativeSlot = slot + 1u;
            GWA3::GameThread::Enqueue([nativeSlot, target, callTarget]() {
                SkillMgr::UseSkill(nativeSlot, target, callTarget);
            });
            return MakeOk();
        }

        ActionResult HandleUseHeroSkill(const json& p) {
            if (!p.contains("hero_index") || !p.contains("slot")) return MakeError("missing hero_index or slot");
            uint32_t heroIdx = p["hero_index"].get<uint32_t>();
            uint32_t slot = p["slot"].get<uint32_t>();
            if (slot >= 8) return MakeError("invalid_slot");
            uint32_t target = p.value("target_agent_id", 0u);
            uint32_t nativeSlot = slot + 1u;
            GWA3::GameThread::Enqueue([heroIdx, nativeSlot, target]() {
                SkillMgr::UseHeroSkill(heroIdx, nativeSlot, target);
            });
            return MakeOk();
        }

    } // namespace

    void RegisterCombatActions(ActionDispatchTable& dispatch) {
        dispatch["attack"] = HandleAttack;
        dispatch["call_target"] = HandleCallTarget;
        dispatch["use_skill"] = HandleUseSkill;
        dispatch["use_hero_skill"] = HandleUseHeroSkill;
    }

} // namespace GWA3::LLM::ActionExecutor
