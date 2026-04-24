#include <gwa3/bot/BotFramework.h>
#include <gwa3/bot/BotModuleRegistry.h>
#include <gwa3/testing/TestFramework.h>

using namespace GWA3::Bot;

GWA3_TEST(bot_module_registry_registers_ravens_module, {
    auto& cfg = GetConfig();
    cfg = {};

    RegisterBotModule(BotModuleKind::RavensPoint);

    GWA3_ASSERT(cfg.bot_module_name == "RavensPoint");
    GWA3_ASSERT_EQ(cfg.target_map_id, 617u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 645u);
})

GWA3_TEST(bot_module_registry_registers_arachnis_module, {
    auto& cfg = GetConfig();
    cfg = {};

    RegisterBotModule(BotModuleKind::ArachnisHaunt);

    GWA3_ASSERT(cfg.bot_module_name == "ArachnisHaunt");
    GWA3_ASSERT_EQ(cfg.target_map_id, 584u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 640u);
})

GWA3_TEST(bot_module_registry_registers_existing_dungeon_modules, {
    auto& cfg = GetConfig();
    cfg = {};

    RegisterBotModule(BotModuleKind::Kathandrax);
    GWA3_ASSERT(cfg.bot_module_name == "Kathandrax");
    GWA3_ASSERT_EQ(cfg.target_map_id, 570u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 648u);

    cfg = {};
    RegisterBotModule(BotModuleKind::FrostmawsBurrows);
    GWA3_ASSERT(cfg.bot_module_name == "FrostmawsBurrows");
    GWA3_ASSERT_EQ(cfg.target_map_id, 630u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 643u);

    cfg = {};
    RegisterBotModule(BotModuleKind::RragarsMenagerie);
    GWA3_ASSERT(cfg.bot_module_name == "RragarsMenagerie");
    GWA3_ASSERT_EQ(cfg.target_map_id, 573u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 648u);
})
