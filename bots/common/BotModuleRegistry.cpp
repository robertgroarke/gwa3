#include <bots/common/BotModuleRegistry.h>

#include <bots/arachnis_haunt/ArachnisHauntBot.h>
#include <bots/frostmaws_burrows/FrostmawsBurrowsBot.h>
#ifndef GWA3_DUNGEON_TEST_BUILD
#include <bots/froggy/FroggyHM.h>
#endif
#include <bots/kathandrax/KathandraxBot.h>
#include <bots/ravens_point/RavensPointBot.h>
#include <bots/rragars_menagerie/RragarsMenagerieBot.h>

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
