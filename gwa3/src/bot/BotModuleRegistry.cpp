#include <gwa3/bot/BotModuleRegistry.h>

#include <gwa3/bot/ArachnisHauntBot.h>
#include <gwa3/bot/FrostmawsBurrowsBot.h>
#ifndef GWA3_DUNGEON_TEST_BUILD
#include <gwa3/bot/FroggyHM.h>
#endif
#include <gwa3/bot/KathandraxBot.h>
#include <gwa3/bot/RavensPointBot.h>
#include <gwa3/bot/RragarsMenagerieBot.h>

namespace GWA3::Bot {

void RegisterBotModule(BotModuleKind kind) {
    switch (kind) {
    case BotModuleKind::RragarsMenagerie:
        RragarsMenagerieBot::Register();
        break;
    case BotModuleKind::Kathandrax:
        KathandraxBot::Register();
        break;
    case BotModuleKind::FrostmawsBurrows:
        FrostmawsBurrowsBot::Register();
        break;
    case BotModuleKind::RavensPoint:
        RavensPointBot::Register();
        break;
    case BotModuleKind::ArachnisHaunt:
        ArachnisHauntBot::Register();
        break;
    case BotModuleKind::FroggyHM:
    default:
#ifndef GWA3_DUNGEON_TEST_BUILD
        Froggy::Register();
#else
        RragarsMenagerieBot::Register();
#endif
        break;
    }
}

} // namespace GWA3::Bot
