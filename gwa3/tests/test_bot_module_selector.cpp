#include <gwa3/bot/BotModuleSelector.h>
#include <gwa3/testing/TestFramework.h>

using namespace GWA3::Bot;

GWA3_TEST(bot_module_selector_defaults_to_froggy, {
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName(nullptr)), static_cast<int>(BotModuleKind::FroggyHM));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("")), static_cast<int>(BotModuleKind::FroggyHM));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("unknown")), static_cast<int>(BotModuleKind::FroggyHM));
})

GWA3_TEST(bot_module_selector_parses_rragars_aliases, {
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("RragarsMenagerie")), static_cast<int>(BotModuleKind::RragarsMenagerie));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("rragar")), static_cast<int>(BotModuleKind::RragarsMenagerie));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("RRAGARSMENAGERIE")), static_cast<int>(BotModuleKind::RragarsMenagerie));
})

GWA3_TEST(bot_module_selector_parses_new_dungeon_aliases, {
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("Kathandrax")), static_cast<int>(BotModuleKind::Kathandrax));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("catacombsofkathandrax")), static_cast<int>(BotModuleKind::Kathandrax));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("FrostmawsBurrows")), static_cast<int>(BotModuleKind::FrostmawsBurrows));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("frostmaw")), static_cast<int>(BotModuleKind::FrostmawsBurrows));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("RavensPoint")), static_cast<int>(BotModuleKind::RavensPoint));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("ravens")), static_cast<int>(BotModuleKind::RavensPoint));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("ArachnisHaunt")), static_cast<int>(BotModuleKind::ArachnisHaunt));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("arachnis")), static_cast<int>(BotModuleKind::ArachnisHaunt));
})

GWA3_TEST(bot_module_selector_names_round_trip, {
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::FroggyHM), "FroggyHM") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::RragarsMenagerie), "RragarsMenagerie") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::Kathandrax), "Kathandrax") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::FrostmawsBurrows), "FrostmawsBurrows") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::RavensPoint), "RavensPoint") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::ArachnisHaunt), "ArachnisHaunt") == 0);
})
