#include <gwa3/bot/BotFramework.h>
#include <gwa3/bot/FrostmawsBurrowsBot.h>
#include <gwa3/testing/TestFramework.h>

using namespace GWA3::Bot;

GWA3_TEST(frostmaws_bot_register_sets_config, {
    auto& cfg = GetConfig();
    cfg = {};

    GWA3::Bot::FrostmawsBurrowsBot::Register();

    GWA3_ASSERT(cfg.hard_mode);
    GWA3_ASSERT_EQ(cfg.target_map_id, 630u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 643u);
    GWA3_ASSERT(cfg.bot_module_name == "FrostmawsBurrows");
})
