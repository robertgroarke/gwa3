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

} // namespace GWA3::Bot::Froggy
