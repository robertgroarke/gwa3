#include <gwa3/bot/ArachnisHauntBot.h>
#include <gwa3/bot/BotFramework.h>
#include <gwa3/testing/TestFramework.h>

using namespace GWA3::Bot;

GWA3_TEST(arachnis_bot_register_sets_config, {
    auto& cfg = GetConfig();
    cfg = {};

    GWA3::Bot::ArachnisHauntBot::Register();

    GWA3_ASSERT(cfg.hard_mode);
    GWA3_ASSERT_EQ(cfg.target_map_id, 584u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 640u);
    GWA3_ASSERT(cfg.bot_module_name == "ArachnisHaunt");
})
