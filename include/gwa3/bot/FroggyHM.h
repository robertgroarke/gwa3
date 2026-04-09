#pragma once

#include <gwa3/bot/BotFramework.h>

namespace GWA3::Bot::Froggy {

    // Register all Froggy HM state handlers with the bot framework.
    void Register();

    // State handlers
    BotState HandleCharSelect(BotConfig& cfg);
    BotState HandleTownSetup(BotConfig& cfg);
    BotState HandleTravel(BotConfig& cfg);
    BotState HandleDungeon(BotConfig& cfg);
    BotState HandleLoot(BotConfig& cfg);
    BotState HandleMerchant(BotConfig& cfg);
    BotState HandleMaintenance(BotConfig& cfg);
    BotState HandleError(BotConfig& cfg);

    // Run unit tests for Froggy's internal pure-logic functions.
    // Returns number of failures.
    int RunFroggyUnitTests();

    // Execute one bounded builtin combat step against a target for integration testing.
    bool ExecuteBuiltinCombatStep(uint32_t targetId);

    // Retrieve a short description of the last builtin combat action chosen.
    const char* GetLastCombatStepDescription();

    // Dump Froggy's builtin combat decision context for a target.
    void DebugDumpBuiltinCombatDecision(uint32_t targetId);

} // namespace GWA3::Bot::Froggy
