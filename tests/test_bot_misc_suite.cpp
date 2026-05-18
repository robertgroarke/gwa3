// Consolidated test module generated from small test files.
// --- merged from tests/test_bot_module_suite.cpp ---
#include "BotModuleRegistryTestSupport.h"
#include "BotModuleSelectorTestSupport.h"

// --- tests/test_bot_module_registry_registers_arachnis_module.cpp ---
namespace GWA3::Tests::Consolidated::test_bot_module_registry_registers_arachnis_module {

GWA3_TEST(bot_module_registry_registers_arachnis_module, {
    auto& cfg = GetConfig();
    cfg = {};

    RegisterBotModule(BotModuleKind::ArachnisHaunt);

    GWA3_ASSERT(cfg.bot_module_name == "ArachnisHaunt");
    GWA3_ASSERT_EQ(cfg.target_map_id, 584u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 640u);
})

} // namespace GWA3::Tests::Consolidated::test_bot_module_registry_registers_arachnis_module

// --- tests/test_bot_module_registry_registers_existing_dungeon_modules.cpp ---
namespace GWA3::Tests::Consolidated::test_bot_module_registry_registers_existing_dungeon_modules {

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

} // namespace GWA3::Tests::Consolidated::test_bot_module_registry_registers_existing_dungeon_modules

// --- tests/test_bot_module_registry_registers_ravens_module.cpp ---
namespace GWA3::Tests::Consolidated::test_bot_module_registry_registers_ravens_module {

GWA3_TEST(bot_module_registry_registers_ravens_module, {
    auto& cfg = GetConfig();
    cfg = {};

    RegisterBotModule(BotModuleKind::RavensPoint);

    GWA3_ASSERT(cfg.bot_module_name == "RavensPoint");
    GWA3_ASSERT_EQ(cfg.target_map_id, 617u);
    GWA3_ASSERT_EQ(cfg.outpost_map_id, 645u);
})

} // namespace GWA3::Tests::Consolidated::test_bot_module_registry_registers_ravens_module

// --- tests/test_bot_module_selector_defaults_to_froggy.cpp ---
namespace GWA3::Tests::Consolidated::test_bot_module_selector_defaults_to_froggy {

GWA3_TEST(bot_module_selector_defaults_to_froggy, {
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName(nullptr)), static_cast<int>(BotModuleKind::FroggyHM));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("")), static_cast<int>(BotModuleKind::FroggyHM));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("unknown")), static_cast<int>(BotModuleKind::FroggyHM));
})

} // namespace GWA3::Tests::Consolidated::test_bot_module_selector_defaults_to_froggy

// --- tests/test_bot_module_selector_names_round_trip.cpp ---
namespace GWA3::Tests::Consolidated::test_bot_module_selector_names_round_trip {

GWA3_TEST(bot_module_selector_names_round_trip, {
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::FroggyHM), "FroggyHM") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::RragarsMenagerie), "RragarsMenagerie") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::Kathandrax), "Kathandrax") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::FrostmawsBurrows), "FrostmawsBurrows") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::RavensPoint), "RavensPoint") == 0);
    GWA3_ASSERT(_stricmp(GetBotModuleName(BotModuleKind::ArachnisHaunt), "ArachnisHaunt") == 0);
})

} // namespace GWA3::Tests::Consolidated::test_bot_module_selector_names_round_trip

// --- tests/test_bot_module_selector_parses_new_dungeon_aliases.cpp ---
namespace GWA3::Tests::Consolidated::test_bot_module_selector_parses_new_dungeon_aliases {

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

} // namespace GWA3::Tests::Consolidated::test_bot_module_selector_parses_new_dungeon_aliases

// --- tests/test_bot_module_selector_parses_rragars_aliases.cpp ---
namespace GWA3::Tests::Consolidated::test_bot_module_selector_parses_rragars_aliases {

GWA3_TEST(bot_module_selector_parses_rragars_aliases, {
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("RragarsMenagerie")), static_cast<int>(BotModuleKind::RragarsMenagerie));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("rragar")), static_cast<int>(BotModuleKind::RragarsMenagerie));
    GWA3_ASSERT_EQ(static_cast<int>(ParseBotModuleName("RRAGARSMENAGERIE")), static_cast<int>(BotModuleKind::RragarsMenagerie));
})

} // namespace GWA3::Tests::Consolidated::test_bot_module_selector_parses_rragars_aliases

// --- merged from tests/test_misc_suite.cpp ---
#include "DungeonInteractionsTestSupport.h"
#include "DungeonSkillTestSupport.h"
#include <cstring>
#include <gwa3/game/SkillIds.h>

// --- tests/test_dungeon_skill_build_cache.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_skill_build_cache {

using namespace GWA3::DungeonSkill;

GWA3_TEST(dungeon_skill_build_skill_cache_classifies_common_roles, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    SkillStubs::Reset();

    SkillStubs::SetSkillData(1239u, 13u, 0u, 10u, 2.0f, 0.75f, 20u, 0u);
    SkillStubs::SetSkillData(332u, 9u, 5u, 0u, 0.0f, 0.0f, 0u, 5u);
    SkillStubs::SetSkillbarSkill(1u, 1239u);
    SkillStubs::SetSkillbarSkill(2u, 332u);

    CachedSkill cache[8] = {};
    GWA3_ASSERT(BuildSkillCache(cache));
    GWA3_ASSERT_EQ(cache[0].skill_id, 1239u);
    GWA3_ASSERT(cache[0].hasRole(ROLE_BINDING));
    GWA3_ASSERT(cache[0].hasRole(ROLE_PRESSURE));
    GWA3_ASSERT(cache[0].hasRole(ROLE_PRECAST));
    GWA3_ASSERT_EQ(cache[1].skill_id, 332u);
    GWA3_ASSERT(cache[1].hasRole(ROLE_ATTACK));
    GWA3_ASSERT(cache[1].hasRole(ROLE_OFFENSIVE));
});
} // namespace GWA3::Tests::Consolidated::test_dungeon_skill_build_cache

// --- tests/test_dungeon_skill_can_cast_gates.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_skill_can_cast_gates {


using namespace GWA3::DungeonSkill;
namespace ItemStubs = GWA3::TestStubs::ItemMgr;

GWA3_TEST(dungeon_skill_can_cast_reports_party_and_effect_blocks, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    EffectStubs::Reset();
    PartyStubs::ResetFlags();
    SkillStubs::Reset();

    CachedSkill spell = {};
    spell.skill_id = 500u;
    spell.slot = 0u;
    spell.skill_type = 2u;
    spell.roles = ROLE_OFFENSIVE;

    GWA3_ASSERT(ExplainCanCastFailure(spell) == nullptr);
    auto* me = GWA3::AgentMgr::GetMyAgent();
    GWA3_ASSERT(me != nullptr);
    me->skill = 777u;
    me->model_state = 0x645u;
    GWA3_ASSERT(ExplainCanCastFailure(spell) == nullptr);
    SkillStubs::SetSkillbarSkill(1u, spell.skill_id, 100u, 0u);
    GWA3_ASSERT(std::strcmp(ExplainCanCastFailure(spell), "recharging") == 0);
    SkillStubs::SetSkillbarSkill(1u, spell.skill_id, 0u, 0u);
    EffectStubs::AddEffect(11u);
    GWA3_ASSERT(std::strcmp(ExplainCanCastFailure(spell), "diversion") == 0);
    GWA3_ASSERT(!CanCast(spell));

    EffectStubs::Reset();
    PartyStubs::SetPartyDefeated(true);
    GWA3_ASSERT(std::strcmp(ExplainCanCastFailure(spell), "party_defeated") == 0);
});

GWA3_TEST(dungeon_skill_basic_attack_uses_shared_cast_gates, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    EffectStubs::Reset();
    PartyStubs::ResetFlags();
    SkillStubs::Reset();
    ItemStubs::Reset();

    GWA3_ASSERT(CanBasicAttack());

    auto* me = GWA3::AgentMgr::GetMyAgent();
    GWA3_ASSERT(me != nullptr);
    me->weapon_type = 0u;
    me->weapon_item_type = 0u;
    me->weapon_item_id = 77u;
    GWA3_ASSERT(!CanBasicAttack());

    me->weapon_item_id = 0u;
    ItemStubs::SetHeldBundleItemId(4242u);
    GWA3_ASSERT(!CanBasicAttack());

    ItemStubs::Reset();
    PartyStubs::SetPartyDefeated(true);
    GWA3_ASSERT(!CanBasicAttack());
});
} // namespace GWA3::Tests::Consolidated::test_dungeon_skill_can_cast_gates

// --- tests/test_dungeon_skill_can_use_gates.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_skill_can_use_gates {



using namespace GWA3::DungeonSkill;

GWA3_TEST(dungeon_skill_can_use_skill_applies_common_gates, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 0.95f);
    EffectStubs::Reset();
    PartyStubs::ResetFlags();
    SkillStubs::Reset();

    auto* me = GWA3::AgentMgr::GetMyAgent();
    GWA3_ASSERT(me != nullptr);
    me->energy = 1.0f;
    me->max_energy = 10u;
    me->skill = 777u;

    CachedSkill heal = {};
    heal.roles = ROLE_HEAL_SINGLE;
    heal.skill_type = 2u;
    heal.target_type = 3u;
    GWA3_ASSERT(!CanUseSkill(heal, 0u));
    me->skill = 0u;

    CachedSkill survival = {};
    survival.skill_id = 2358u;
    survival.roles = ROLE_SURVIVAL;
    survival.skill_type = 3u;
    survival.target_type = 0u;
    GWA3_ASSERT(!CanUseSkill(survival, 0u));

    me->hp = 0.25f;
    GWA3_ASSERT(CanUseSkill(survival, 0u));
    EffectStubs::AddEffectForAgent(me->agent_id, 2358u, 8.0f);
    GWA3_ASSERT(!CanUseSkill(survival, 0u));

    EffectStubs::Reset();
    CachedSkill precast = {};
    precast.skill_id = 4000u;
    precast.roles = ROLE_PRECAST;
    precast.skill_type = 0u;
    precast.target_type = 0u;
    EffectStubs::AddEffectForAgent(me->agent_id, 4000u, 6.0f);
    GWA3_ASSERT(!CanUseSkill(precast, 0u));

    EffectStubs::Reset();
    CachedSkill energyGate = {};
    energyGate.skill_id = 5000u;
    energyGate.roles = ROLE_OFFENSIVE;
    energyGate.skill_type = 2u;
    energyGate.target_type = 5u;
    energyGate.energy_cost = 15u;
    GWA3_ASSERT(!CanUseSkill(energyGate, 10u));

    SkillStubs::SetSkillData(GWA3::SkillIds::FINISH_HIM, 2u, 5u, 5u, 0.5f, 0.75f, 0u, 0u);
    AgentStubs::AddNpc(20u, 100.0f, 0.0f, 3u, 0.80f);
    CachedSkill finishHim = {};
    finishHim.skill_id = GWA3::SkillIds::FINISH_HIM;
    finishHim.roles = ROLE_PRESSURE;
    finishHim.skill_type = 2u;
    finishHim.target_type = 5u;
    finishHim.energy_cost = 5u;
    me->energy = 1.0f;
    me->max_energy = 30u;
    GWA3_ASSERT(!CanUseSkill(finishHim, 20u));
    GWA3_ASSERT(std::strcmp(ExplainCanUseSkillFailure(finishHim, 20u), "finish_him_hp_high") == 0);
    static_cast<GWA3::AgentLiving*>(GWA3::AgentMgr::GetAgentByID(20u))->hp = 0.30f;
    GWA3_ASSERT(CanUseSkill(finishHim, 20u));

    EffectStubs::Reset();
    CachedSkill quickenedEnergy = {};
    quickenedEnergy.skill_id = 6000u;
    quickenedEnergy.roles = ROLE_OFFENSIVE;
    quickenedEnergy.skill_type = 2u;
    quickenedEnergy.target_type = 0u;
    quickenedEnergy.energy_cost = 8u;
    me->hp = 1.0f;
    me->energy = 1.0f;
    me->max_energy = 10u;
    GWA3_ASSERT(CanUseSkill(quickenedEnergy, 0u));
    EffectStubs::AddEffectForAgent(me->agent_id, 475u, 10.0f);
    GWA3_ASSERT(std::strcmp(ExplainCanUseSkillFailure(quickenedEnergy, 0u), "energy_low") == 0);
});
} // namespace GWA3::Tests::Consolidated::test_dungeon_skill_can_use_gates

// --- tests/test_dungeon_skill_target_resolution.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_skill_target_resolution {

using namespace GWA3::DungeonSkill;

GWA3_TEST(dungeon_skill_target_resolution_prefers_contextual_targets, {
    AgentStubs::ResetAgents();
    AgentStubs::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    EffectStubs::Reset();
    SkillStubs::Reset();

    AgentStubs::AddNpc(2u, 100.0f, 0.0f, 1u, 0.40f);
    AgentStubs::AddNpc(3u, 150.0f, 0.0f, 1u, 0.0f);
    AgentStubs::AddNpc(10u, 600.0f, 0.0f, 3u, 1.0f);
    AgentStubs::AddNpc(11u, 500.0f, 0.0f, 3u, 1.0f);
    AgentStubs::AddNpc(12u, 200.0f, 0.0f, 3u, 1.0f);
    AgentStubs::AddNpc(13u, 700.0f, 0.0f, 3u, 1.0f);

    static_cast<GWA3::AgentLiving*>(GWA3::AgentMgr::GetAgentByID(11u))->skill = 777u;
    static_cast<GWA3::AgentLiving*>(GWA3::AgentMgr::GetAgentByID(13u))->hex = 1u;

    SkillStubs::SetSkillData(900u, 3u, 3u, 5u, 1.0f, 0.5f, 15u, 0u);
    EffectStubs::AddEffectForAgent(10u, 900u, 12.0f);

    CachedSkill heal = {};
    heal.roles = ROLE_HEAL_SINGLE;
    heal.target_type = 3u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(heal, 0u), 2u);

    CachedSkill selfTarget = {};
    selfTarget.roles = ROLE_PRECAST | ROLE_BINDING;
    selfTarget.target_type = 0u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(selfTarget, 99u), 1u);

    CachedSkill resurrect = {};
    resurrect.roles = ROLE_RESURRECT;
    resurrect.target_type = 6u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(resurrect, 0u), 3u);

    CachedSkill interrupt = {};
    interrupt.roles = ROLE_INTERRUPT_HARD;
    interrupt.target_type = 5u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(interrupt, 99u), 11u);

    CachedSkill enchantRemove = {};
    enchantRemove.roles = ROLE_ENCHANT_REMOVE;
    enchantRemove.target_type = 5u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(enchantRemove, 99u), 10u);

    CachedSkill attack = {};
    attack.roles = ROLE_ATTACK;
    attack.target_type = 5u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(attack, 99u), 10u);

    CachedSkill hex = {};
    hex.roles = ROLE_HEX;
    hex.target_type = 5u;
    GWA3_ASSERT_EQ(ResolveSkillTarget(hex, 99u), 11u);
});
} // namespace GWA3::Tests::Consolidated::test_dungeon_skill_target_resolution

// --- tests/test_dungeon_skill_target_type_requirement.cpp ---
namespace GWA3::Tests::Consolidated::test_dungeon_skill_target_type_requirement {

using namespace GWA3::DungeonSkill;

GWA3_TEST(dungeon_skill_target_type_requirement_matches_cast_gating, {
    GWA3_ASSERT(SkillTargetTypeRequiresResolvedTarget(1u));
    GWA3_ASSERT(!SkillTargetTypeRequiresResolvedTarget(3u));
    GWA3_ASSERT(SkillTargetTypeRequiresResolvedTarget(4u));
    GWA3_ASSERT(SkillTargetTypeRequiresResolvedTarget(5u));
    GWA3_ASSERT(SkillTargetTypeRequiresResolvedTarget(6u));
    GWA3_ASSERT(SkillTargetTypeRequiresResolvedTarget(14u));
})
} // namespace GWA3::Tests::Consolidated::test_dungeon_skill_target_type_requirement
