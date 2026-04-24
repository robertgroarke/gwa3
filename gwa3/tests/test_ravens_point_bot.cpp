#include <gwa3/bot/BotFramework.h>
#include <gwa3/bot/RavensPointBot.h>
#include <gwa3/testing/TestFramework.h>

using namespace GWA3::Bot;

GWA3_TEST(ravens_bot_register_sets_config, {
    auto& cfg = GetConfig();
    cfg = {};

    GWA3::Bot::RavensPointBot::Register();

    GWA3_ASSERT(cfg.hard_mode);
    GWA3_ASSERT_EQ(cfg.target_map_id, 617u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 645u);
    GWA3_ASSERT(cfg.bot_module_name == "RavensPoint");
})
