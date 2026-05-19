#pragma once

#include <gwa3/llm/ActionExecutor.h>

#include <functional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace GWA3::LLM::ActionExecutor {

    using ActionHandler = std::function<ActionResult(const nlohmann::json& params)>;
    using ActionDispatchTable = std::unordered_map<std::string, ActionHandler>;

    ActionResult MakeOk();
    ActionResult MakeError(const char* msg);

    void RegisterMovementActions(ActionDispatchTable& dispatch);
    void RegisterCombatActions(ActionDispatchTable& dispatch);
    void RegisterInteractionActions(ActionDispatchTable& dispatch);
    void RegisterQuestActions(ActionDispatchTable& dispatch);
    void RegisterPartyActions(ActionDispatchTable& dispatch);
    void RegisterTravelActions(ActionDispatchTable& dispatch);
    void RegisterItemActions(ActionDispatchTable& dispatch);
    void RegisterSkillbarActions(ActionDispatchTable& dispatch);
    void RegisterTradeAndCraftingActions(ActionDispatchTable& dispatch);
    void RegisterFroggyActions(ActionDispatchTable& dispatch);
    void RegisterBotControlActions(ActionDispatchTable& dispatch);
    void RegisterUtilityActions(ActionDispatchTable& dispatch);

} // namespace GWA3::LLM::ActionExecutor
