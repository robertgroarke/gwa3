#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/managers/SkillMgr.h>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        ActionResult HandleLoadSkillbar(const json& p) {
            if (!p.contains("skill_ids")) return MakeError("missing skill_ids");
            auto ids = p["skill_ids"];
            if (!ids.is_array() || ids.size() != 8) return MakeError("skill_ids must be array of 8");
            uint32_t skillIds[8] = {};
            for (int i = 0; i < 8; i++) {
                skillIds[i] = ids[i].get<uint32_t>();
            }
            uint32_t heroIndex = p.value("hero_index", 0u);
            GWA3::GameThread::Enqueue([skillIds, heroIndex]() {
                SkillMgr::LoadSkillbar(skillIds, heroIndex);
            });
            return MakeOk();
        }

    } // namespace

    void RegisterSkillbarActions(ActionDispatchTable& dispatch) {
        dispatch["load_skillbar"] = HandleLoadSkillbar;
    }

} // namespace GWA3::LLM::ActionExecutor
