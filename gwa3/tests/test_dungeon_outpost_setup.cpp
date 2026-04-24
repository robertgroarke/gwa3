#include <gwa3/bot/BotFramework.h>
#include <gwa3/bot/DungeonOutpostSetup.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/testing/TestFramework.h>

namespace OutpostSetup = GWA3::Bot::DungeonOutpostSetup;

namespace GWA3::TestStubs::MapMgr {
void SetHardModeEnabled(bool enabled);
bool HardModeEnabled();
}

namespace GWA3::TestStubs::PartyMgr {
void ResetFlags();
void ResetHeroes();
uint32_t HeroIdAt(uint32_t index);
uint32_t HeroBehaviorAt(uint32_t index);
}

namespace GWA3::TestStubs::PlayerMgr {
void Reset();
void SetPlayerName(const wchar_t* name);
}

GWA3_TEST(dungeon_outpost_setup_resolves_preferred_hero_config_from_json, {
    char file[64] = {};

    GWA3_ASSERT(OutpostSetup::ResolvePreferredHeroConfigFromJson(
        "{\"B E A S T R I T\":{\"hero_config\":\"Mercs\"},\"D I S C O P A N I C\":{\"hero_config\":\"Standard\"}}",
        "B E A S T R I T",
        file,
        sizeof(file)));
    GWA3_ASSERT(std::string(file) == "Mercs.txt");

    GWA3_ASSERT(OutpostSetup::ResolvePreferredHeroConfigFromJson(
        "{\"D I S C O P A N I C\":{\"hero_config\":\"Standard\"}}",
        "Unknown",
        file,
        sizeof(file)));
    GWA3_ASSERT(std::string(file) == "Standard.txt");
})

GWA3_TEST(dungeon_outpost_setup_decodes_skill_templates, {
    uint32_t skills[8] = {};
    GWA3_ASSERT(OutpostSetup::DecodeSkillTemplate("OAOiAyk8gNtePuwJ00ZaNbJA", skills));
    GWA3_ASSERT(skills[0] != 0u || skills[1] != 0u);
    GWA3_ASSERT(!OutpostSetup::DecodeSkillTemplate("", skills));
})

GWA3_TEST(dungeon_outpost_setup_applies_party_setup_for_beastrit_config, {
    GWA3::TestStubs::MapMgr::SetHardModeEnabled(false);
    GWA3::TestStubs::PartyMgr::ResetFlags();
    GWA3::TestStubs::PartyMgr::ResetHeroes();
    GWA3::TestStubs::PlayerMgr::Reset();
    GWA3::TestStubs::PlayerMgr::SetPlayerName(L"B E A S T R I T");

    auto& cfg = GWA3::Bot::GetConfig();
    cfg = {};
    cfg.hard_mode = true;

    GWA3_ASSERT(OutpostSetup::ApplyOutpostSetup(cfg));
    GWA3_ASSERT(cfg.hero_config_file == "Mercs.txt");
    GWA3_ASSERT_EQ(GWA3::PartyMgr::CountPartyHeroes(), 7u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::PartyMgr::HeroIdAt(0), 30u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::PartyMgr::HeroIdAt(6), 29u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::PartyMgr::HeroBehaviorAt(0), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::PartyMgr::HeroBehaviorAt(6), 1u);
    GWA3_ASSERT(GWA3::TestStubs::MapMgr::HardModeEnabled());
})
