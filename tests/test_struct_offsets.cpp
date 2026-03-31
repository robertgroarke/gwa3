// GWA3-043: Compile-time struct offset and size validation.
// Every field offset and struct size is checked via static_assert.
// Cross-referenced against GWCA headers and GWA2_Assembly.au3.
// If any offset is wrong, the build fails immediately.

#include <gwa3/game/GameTypes.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/Skill.h>
#include <gwa3/game/Effect.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/Quest.h>
#include <gwa3/game/Map.h>
#include <gwa3/game/Camera.h>
#include <gwa3/game/Guild.h>
#include <gwa3/game/Party.h>
#include <gwa3/game/Player.h>
#include <gwa3/game/Title.h>
#include <gwa3/testing/TestFramework.h>

#include <cstddef>

// MSVC warns about offsetof on non-standard-layout types (inheritance).
// GWCA uses the same pattern and it works correctly on MSVC x86.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // size_t to uint32_t
#pragma warning(disable: 4324) // structure padded
#endif

using namespace GWA3;

// ===== GameTypes =====
GWA3_CHECK_SIZE(GWArray<void*>, 16);
GWA3_CHECK_SIZE(TLink,           8);
GWA3_CHECK_SIZE(TList,          12);
GWA3_CHECK_SIZE(Vec3f,          12);
GWA3_CHECK_SIZE(Vec2f,           8);
GWA3_CHECK_SIZE(GamePos,        12);

// ===== Agent (base) — 0xC4 / 196 bytes =====
GWA3_CHECK_SIZE(Agent, 0xC4);
GWA3_CHECK_OFFSET(Agent, vtable,          0x00);
GWA3_CHECK_OFFSET(Agent, h0004,           0x04);
GWA3_CHECK_OFFSET(Agent, h0008,           0x08);
GWA3_CHECK_OFFSET(Agent, h000C,           0x0C);
GWA3_CHECK_OFFSET(Agent, timer,           0x14);
GWA3_CHECK_OFFSET(Agent, timer2,          0x18);
GWA3_CHECK_OFFSET(Agent, link,            0x1C);
GWA3_CHECK_OFFSET(Agent, link2,           0x24);
GWA3_CHECK_OFFSET(Agent, agent_id,        0x2C);
GWA3_CHECK_OFFSET(Agent, z,               0x30);
GWA3_CHECK_OFFSET(Agent, width1,          0x34);
GWA3_CHECK_OFFSET(Agent, height1,         0x38);
GWA3_CHECK_OFFSET(Agent, width2,          0x3C);
GWA3_CHECK_OFFSET(Agent, height2,         0x40);
GWA3_CHECK_OFFSET(Agent, width3,          0x44);
GWA3_CHECK_OFFSET(Agent, height3,         0x48);
GWA3_CHECK_OFFSET(Agent, rotation_angle,  0x4C);
GWA3_CHECK_OFFSET(Agent, rotation_cos,    0x50);
GWA3_CHECK_OFFSET(Agent, rotation_sin,    0x54);
GWA3_CHECK_OFFSET(Agent, name_properties, 0x58);
GWA3_CHECK_OFFSET(Agent, ground,          0x5C);
GWA3_CHECK_OFFSET(Agent, h0060,           0x60);
GWA3_CHECK_OFFSET(Agent, terrain_normal,  0x64);
GWA3_CHECK_OFFSET(Agent, h0070,           0x70);
GWA3_CHECK_OFFSET(Agent, x,               0x74);
GWA3_CHECK_OFFSET(Agent, y,               0x78);
GWA3_CHECK_OFFSET(Agent, plane,           0x7C);
GWA3_CHECK_OFFSET(Agent, h0080,           0x80);
GWA3_CHECK_OFFSET(Agent, name_tag_x,      0x84);
GWA3_CHECK_OFFSET(Agent, name_tag_y,      0x88);
GWA3_CHECK_OFFSET(Agent, name_tag_z,      0x8C);
GWA3_CHECK_OFFSET(Agent, visual_effects,  0x90);
GWA3_CHECK_OFFSET(Agent, h0092,           0x92);
GWA3_CHECK_OFFSET(Agent, h0094,           0x94);
GWA3_CHECK_OFFSET(Agent, type,            0x9C);
GWA3_CHECK_OFFSET(Agent, move_x,          0xA0);
GWA3_CHECK_OFFSET(Agent, move_y,          0xA4);
GWA3_CHECK_OFFSET(Agent, h00A8,           0xA8);
GWA3_CHECK_OFFSET(Agent, rotation_cos2,   0xAC);
GWA3_CHECK_OFFSET(Agent, rotation_sin2,   0xB0);
GWA3_CHECK_OFFSET(Agent, h00B4,           0xB4);

// ===== AgentItem — 0xD4 / 212 bytes =====
GWA3_CHECK_SIZE(AgentItem, 212);
GWA3_CHECK_OFFSET(AgentItem, owner,      0xC4);
GWA3_CHECK_OFFSET(AgentItem, item_id,    0xC8);
GWA3_CHECK_OFFSET(AgentItem, extra_type, 0xD0);

// ===== AgentGadget — 0xE4 / 228 bytes =====
GWA3_CHECK_SIZE(AgentGadget, 228);
GWA3_CHECK_OFFSET(AgentGadget, gadget_id,  0xD0);
GWA3_CHECK_OFFSET(AgentGadget, extra_type, 0xCC);

// ===== AgentLiving — 0x1C4 / 452 bytes =====
GWA3_CHECK_SIZE(AgentLiving, 0x1C4);
GWA3_CHECK_OFFSET(AgentLiving, owner,                0xC4);
GWA3_CHECK_OFFSET(AgentLiving, animation_type,       0xE0);
GWA3_CHECK_OFFSET(AgentLiving, weapon_attack_speed,  0xEC);
GWA3_CHECK_OFFSET(AgentLiving, attack_speed_modifier,0xF0);
GWA3_CHECK_OFFSET(AgentLiving, player_number,        0xF4);
GWA3_CHECK_OFFSET(AgentLiving, agent_model_type,     0xF6);
GWA3_CHECK_OFFSET(AgentLiving, transmog_npc_id,      0xF8);
GWA3_CHECK_OFFSET(AgentLiving, equip,                0xFC);
GWA3_CHECK_OFFSET(AgentLiving, tags,                 0x108);
GWA3_CHECK_OFFSET(AgentLiving, h010C,                0x10C);
GWA3_CHECK_OFFSET(AgentLiving, primary,              0x10E);
GWA3_CHECK_OFFSET(AgentLiving, secondary,            0x10F);
GWA3_CHECK_OFFSET(AgentLiving, level,                0x110);
GWA3_CHECK_OFFSET(AgentLiving, team_id,              0x111);
GWA3_CHECK_OFFSET(AgentLiving, energy_regen,         0x118);
GWA3_CHECK_OFFSET(AgentLiving, energy,               0x120);
GWA3_CHECK_OFFSET(AgentLiving, max_energy,           0x124);
GWA3_CHECK_OFFSET(AgentLiving, hp_pips,              0x12C);
GWA3_CHECK_OFFSET(AgentLiving, hp,                   0x134);
GWA3_CHECK_OFFSET(AgentLiving, max_hp,               0x138);
GWA3_CHECK_OFFSET(AgentLiving, effects,              0x13C);
GWA3_CHECK_OFFSET(AgentLiving, hex,                  0x144);
GWA3_CHECK_OFFSET(AgentLiving, model_state,          0x158);
GWA3_CHECK_OFFSET(AgentLiving, type_map,             0x15C);
GWA3_CHECK_OFFSET(AgentLiving, in_spirit_range,      0x170);
GWA3_CHECK_OFFSET(AgentLiving, visible_effects,      0x174);
GWA3_CHECK_OFFSET(AgentLiving, login_number,         0x184);
GWA3_CHECK_OFFSET(AgentLiving, animation_speed,      0x188);
GWA3_CHECK_OFFSET(AgentLiving, animation_code,       0x18C);
GWA3_CHECK_OFFSET(AgentLiving, animation_id,         0x190);
GWA3_CHECK_OFFSET(AgentLiving, dagger_status,        0x1B4);
GWA3_CHECK_OFFSET(AgentLiving, allegiance,           0x1B5);
GWA3_CHECK_OFFSET(AgentLiving, weapon_type,          0x1B6);
GWA3_CHECK_OFFSET(AgentLiving, skill,                0x1B8);
GWA3_CHECK_OFFSET(AgentLiving, weapon_item_type,     0x1BC);
GWA3_CHECK_OFFSET(AgentLiving, offhand_item_type,    0x1BD);
GWA3_CHECK_OFFSET(AgentLiving, weapon_item_id,       0x1BE);
GWA3_CHECK_OFFSET(AgentLiving, offhand_item_id,      0x1C0);

// ===== MapAgent — 0x34 / 52 bytes =====
GWA3_CHECK_SIZE(MapAgent, 0x34);
GWA3_CHECK_OFFSET(MapAgent, cur_energy,      0x00);
GWA3_CHECK_OFFSET(MapAgent, max_energy,      0x04);
GWA3_CHECK_OFFSET(MapAgent, energy_regen,    0x08);
GWA3_CHECK_OFFSET(MapAgent, skill_timestamp, 0x0C);
GWA3_CHECK_OFFSET(MapAgent, cur_health,      0x20);
GWA3_CHECK_OFFSET(MapAgent, max_health,      0x24);
GWA3_CHECK_OFFSET(MapAgent, health_regen,    0x28);
GWA3_CHECK_OFFSET(MapAgent, effects,         0x30);

// ===== AgentInfo — 0x38 / 56 bytes =====
GWA3_CHECK_SIZE(AgentInfo, 0x38);
GWA3_CHECK_OFFSET(AgentInfo, name_enc, 0x34);

// ===== Skill — 0xA4 / 164 bytes =====
GWA3_CHECK_SIZE(Skill, 0xA4);
GWA3_CHECK_OFFSET(Skill, skill_id,         0x00);
GWA3_CHECK_OFFSET(Skill, campaign,         0x08);
GWA3_CHECK_OFFSET(Skill, type,             0x0C);
GWA3_CHECK_OFFSET(Skill, special,          0x10);
GWA3_CHECK_OFFSET(Skill, combo_req,        0x14);
GWA3_CHECK_OFFSET(Skill, effect1,          0x18);
GWA3_CHECK_OFFSET(Skill, condition,        0x1C);
GWA3_CHECK_OFFSET(Skill, effect2,          0x20);
GWA3_CHECK_OFFSET(Skill, weapon_req,       0x24);
GWA3_CHECK_OFFSET(Skill, profession,       0x28);
GWA3_CHECK_OFFSET(Skill, attribute,        0x29);
GWA3_CHECK_OFFSET(Skill, title,            0x2A);
GWA3_CHECK_OFFSET(Skill, skill_id_pvp,     0x2C);
GWA3_CHECK_OFFSET(Skill, combo,            0x30);
GWA3_CHECK_OFFSET(Skill, target,           0x31);
GWA3_CHECK_OFFSET(Skill, skill_equip_type, 0x33);
GWA3_CHECK_OFFSET(Skill, overcast,         0x34);
GWA3_CHECK_OFFSET(Skill, energy_cost,      0x35);
GWA3_CHECK_OFFSET(Skill, health_cost,      0x36);
GWA3_CHECK_OFFSET(Skill, adrenaline,       0x38);
GWA3_CHECK_OFFSET(Skill, activation,       0x3C);
GWA3_CHECK_OFFSET(Skill, aftercast,        0x40);
GWA3_CHECK_OFFSET(Skill, duration0,        0x44);
GWA3_CHECK_OFFSET(Skill, duration15,       0x48);
GWA3_CHECK_OFFSET(Skill, recharge,         0x4C);
GWA3_CHECK_OFFSET(Skill, skill_arguments,  0x58);
GWA3_CHECK_OFFSET(Skill, scale0,           0x5C);
GWA3_CHECK_OFFSET(Skill, scale15,          0x60);
GWA3_CHECK_OFFSET(Skill, bonus_scale0,     0x64);
GWA3_CHECK_OFFSET(Skill, bonus_scale15,    0x68);
GWA3_CHECK_OFFSET(Skill, aoe_range,        0x6C);
GWA3_CHECK_OFFSET(Skill, const_effect,     0x70);
GWA3_CHECK_OFFSET(Skill, icon_file_id,     0x8C);
GWA3_CHECK_OFFSET(Skill, name,             0x98);
GWA3_CHECK_OFFSET(Skill, concise,          0x9C);
GWA3_CHECK_OFFSET(Skill, description,      0xA0);

// ===== SkillbarSkill — 0x14 / 20 bytes =====
GWA3_CHECK_SIZE(SkillbarSkill, 20);
GWA3_CHECK_OFFSET(SkillbarSkill, adrenaline_a, 0x00);
GWA3_CHECK_OFFSET(SkillbarSkill, adrenaline_b, 0x04);
GWA3_CHECK_OFFSET(SkillbarSkill, recharge,     0x08);
GWA3_CHECK_OFFSET(SkillbarSkill, skill_id,     0x0C);
GWA3_CHECK_OFFSET(SkillbarSkill, event,        0x10);

// ===== Skillbar — 0xBC / 188 bytes =====
GWA3_CHECK_SIZE(Skillbar, 0xBC);
GWA3_CHECK_OFFSET(Skillbar, agent_id,   0x00);
GWA3_CHECK_OFFSET(Skillbar, skills,     0x04);
GWA3_CHECK_OFFSET(Skillbar, disabled,   0xA4);
GWA3_CHECK_OFFSET(Skillbar, cast_array, 0xA8);
GWA3_CHECK_OFFSET(Skillbar, h00B8,      0xB8);

// ===== Effect — 0x18 / 24 bytes =====
GWA3_CHECK_SIZE(Effect, 24);
GWA3_CHECK_OFFSET(Effect, skill_id,        0x00);
GWA3_CHECK_OFFSET(Effect, attribute_level, 0x04);
GWA3_CHECK_OFFSET(Effect, effect_id,       0x08);
GWA3_CHECK_OFFSET(Effect, agent_id,        0x0C);
GWA3_CHECK_OFFSET(Effect, duration,        0x10);
GWA3_CHECK_OFFSET(Effect, timestamp,       0x14);

// ===== Buff — 0x10 / 16 bytes =====
GWA3_CHECK_SIZE(Buff, 16);
GWA3_CHECK_OFFSET(Buff, skill_id,        0x00);
GWA3_CHECK_OFFSET(Buff, buff_id,         0x08);
GWA3_CHECK_OFFSET(Buff, target_agent_id, 0x0C);

// ===== AgentEffects — 0x24 / 36 bytes =====
GWA3_CHECK_SIZE(AgentEffects, 36);
GWA3_CHECK_OFFSET(AgentEffects, agent_id, 0x00);
GWA3_CHECK_OFFSET(AgentEffects, buffs,    0x04);
GWA3_CHECK_OFFSET(AgentEffects, effects,  0x14);

// ===== Item — 0x54 / 84 bytes =====
GWA3_CHECK_SIZE(Item, 84);
GWA3_CHECK_OFFSET(Item, item_id,                0x00);
GWA3_CHECK_OFFSET(Item, agent_id,               0x04);
GWA3_CHECK_OFFSET(Item, bag_equipped,            0x08);
GWA3_CHECK_OFFSET(Item, bag,                     0x0C);
GWA3_CHECK_OFFSET(Item, mod_struct,              0x10);
GWA3_CHECK_OFFSET(Item, mod_struct_size,         0x14);
GWA3_CHECK_OFFSET(Item, customized,              0x18);
GWA3_CHECK_OFFSET(Item, model_file_id,           0x1C);
GWA3_CHECK_OFFSET(Item, type,                    0x20);
GWA3_CHECK_OFFSET(Item, dye,                     0x21);
GWA3_CHECK_OFFSET(Item, value,                   0x24);
GWA3_CHECK_OFFSET(Item, interaction,             0x28);
GWA3_CHECK_OFFSET(Item, model_id,                0x2C);
GWA3_CHECK_OFFSET(Item, info_string,             0x30);
GWA3_CHECK_OFFSET(Item, name_enc,                0x34);
GWA3_CHECK_OFFSET(Item, complete_name_enc,       0x38);
GWA3_CHECK_OFFSET(Item, single_item_name,        0x3C);
GWA3_CHECK_OFFSET(Item, item_formula,            0x48);
GWA3_CHECK_OFFSET(Item, is_material_salvageable, 0x4A);
GWA3_CHECK_OFFSET(Item, quantity,                0x4C);
GWA3_CHECK_OFFSET(Item, equipped,                0x4E);
GWA3_CHECK_OFFSET(Item, profession,              0x4F);
GWA3_CHECK_OFFSET(Item, slot,                    0x50);

// ===== DyeInfo — 3 bytes =====
GWA3_CHECK_SIZE(DyeInfo, 3);

// ===== ItemData — 0x10 / 16 bytes =====
GWA3_CHECK_SIZE(ItemData, 0x10);
GWA3_CHECK_OFFSET(ItemData, model_file_id, 0x00);
GWA3_CHECK_OFFSET(ItemData, type,          0x04);
GWA3_CHECK_OFFSET(ItemData, dye,           0x05);
GWA3_CHECK_OFFSET(ItemData, value,         0x08);
GWA3_CHECK_OFFSET(ItemData, interaction,   0x0C);

// ===== Bag — 0x28 / 40 bytes =====
GWA3_CHECK_SIZE(Bag, 40);
GWA3_CHECK_OFFSET(Bag, bag_type,       0x00);
GWA3_CHECK_OFFSET(Bag, index,          0x04);
GWA3_CHECK_OFFSET(Bag, h0008,          0x08);
GWA3_CHECK_OFFSET(Bag, container_item, 0x0C);
GWA3_CHECK_OFFSET(Bag, items_count,    0x10);
GWA3_CHECK_OFFSET(Bag, bag_array,      0x14);
GWA3_CHECK_OFFSET(Bag, items,          0x18);

// ===== WeaponSet — 0x08 / 8 bytes =====
GWA3_CHECK_SIZE(WeaponSet, 8);
GWA3_CHECK_OFFSET(WeaponSet, weapon,  0x00);
GWA3_CHECK_OFFSET(WeaponSet, offhand, 0x04);

// ===== Inventory — 0x98 / 152 bytes =====
GWA3_CHECK_SIZE(Inventory, 152);
GWA3_CHECK_OFFSET(Inventory, bags,                    0x00);
GWA3_CHECK_OFFSET(Inventory, bundle,                  0x5C);
GWA3_CHECK_OFFSET(Inventory, storage_panes_unlocked,  0x60);
GWA3_CHECK_OFFSET(Inventory, weapon_sets,             0x64);
GWA3_CHECK_OFFSET(Inventory, active_weapon_set,       0x84);
GWA3_CHECK_OFFSET(Inventory, gold_character,          0x90);
GWA3_CHECK_OFFSET(Inventory, gold_storage,            0x94);

// ===== Quest — 0x34 / 52 bytes =====
GWA3_CHECK_SIZE(Quest, 52);
GWA3_CHECK_OFFSET(Quest, quest_id,     0x00);
GWA3_CHECK_OFFSET(Quest, log_state,    0x04);
GWA3_CHECK_OFFSET(Quest, location,     0x08);
GWA3_CHECK_OFFSET(Quest, name,         0x0C);
GWA3_CHECK_OFFSET(Quest, npc,          0x10);
GWA3_CHECK_OFFSET(Quest, map_from,     0x14);
GWA3_CHECK_OFFSET(Quest, marker_x,     0x18);
GWA3_CHECK_OFFSET(Quest, marker_y,     0x1C);
GWA3_CHECK_OFFSET(Quest, marker_plane, 0x20);
GWA3_CHECK_OFFSET(Quest, map_to,       0x28);
GWA3_CHECK_OFFSET(Quest, description,  0x2C);
GWA3_CHECK_OFFSET(Quest, objectives,   0x30);

// ===== MissionObjective — 0x0C / 12 bytes =====
GWA3_CHECK_SIZE(MissionObjective, 12);
GWA3_CHECK_OFFSET(MissionObjective, objective_id, 0x00);
GWA3_CHECK_OFFSET(MissionObjective, enc_str,      0x04);
GWA3_CHECK_OFFSET(MissionObjective, type,         0x08);

// ===== AreaInfo — 0x7C / 124 bytes =====
GWA3_CHECK_SIZE(AreaInfo, 124);
GWA3_CHECK_OFFSET(AreaInfo, campaign,               0x00);
GWA3_CHECK_OFFSET(AreaInfo, continent,              0x04);
GWA3_CHECK_OFFSET(AreaInfo, region,                 0x08);
GWA3_CHECK_OFFSET(AreaInfo, type,                   0x0C);
GWA3_CHECK_OFFSET(AreaInfo, flags,                  0x10);
GWA3_CHECK_OFFSET(AreaInfo, thumbnail_id,           0x14);
GWA3_CHECK_OFFSET(AreaInfo, min_party_size,         0x18);
GWA3_CHECK_OFFSET(AreaInfo, max_party_size,         0x1C);
GWA3_CHECK_OFFSET(AreaInfo, controlled_outpost_id,  0x28);
GWA3_CHECK_OFFSET(AreaInfo, mission_maps_to,        0x3C);
GWA3_CHECK_OFFSET(AreaInfo, x,                      0x40);
GWA3_CHECK_OFFSET(AreaInfo, y,                      0x44);
GWA3_CHECK_OFFSET(AreaInfo, file_id,                0x68);
GWA3_CHECK_OFFSET(AreaInfo, mission_chronology,     0x6C);
GWA3_CHECK_OFFSET(AreaInfo, name_id,                0x74);
GWA3_CHECK_OFFSET(AreaInfo, description_id,         0x78);

// ===== MissionMapIcon — 0x28 / 40 bytes =====
GWA3_CHECK_SIZE(MissionMapIcon, 40);
GWA3_CHECK_OFFSET(MissionMapIcon, index,    0x00);
GWA3_CHECK_OFFSET(MissionMapIcon, x,        0x04);
GWA3_CHECK_OFFSET(MissionMapIcon, y,        0x08);
GWA3_CHECK_OFFSET(MissionMapIcon, option,   0x14);
GWA3_CHECK_OFFSET(MissionMapIcon, model_id, 0x1C);

// ===== PlayerPartyMember — 0x0C / 12 bytes =====
GWA3_CHECK_SIZE(PlayerPartyMember, 12);
GWA3_CHECK_OFFSET(PlayerPartyMember, login_number,     0x00);
GWA3_CHECK_OFFSET(PlayerPartyMember, called_target_id, 0x04);
GWA3_CHECK_OFFSET(PlayerPartyMember, state,            0x08);

// ===== HeroPartyMember — 0x18 / 24 bytes =====
GWA3_CHECK_SIZE(HeroPartyMember, 24);
GWA3_CHECK_OFFSET(HeroPartyMember, agent_id,        0x00);
GWA3_CHECK_OFFSET(HeroPartyMember, owner_player_id, 0x04);
GWA3_CHECK_OFFSET(HeroPartyMember, hero_id,         0x08);
GWA3_CHECK_OFFSET(HeroPartyMember, level,           0x14);

// ===== HenchmanPartyMember — 0x34 / 52 bytes =====
GWA3_CHECK_SIZE(HenchmanPartyMember, 52);
GWA3_CHECK_OFFSET(HenchmanPartyMember, agent_id,   0x00);
GWA3_CHECK_OFFSET(HenchmanPartyMember, profession, 0x2C);
GWA3_CHECK_OFFSET(HenchmanPartyMember, level,      0x30);

// ===== PartyInfo — 0x84 / 132 bytes =====
GWA3_CHECK_SIZE(PartyInfo, 132);
GWA3_CHECK_OFFSET(PartyInfo, party_id,    0x00);
GWA3_CHECK_OFFSET(PartyInfo, players,     0x04);
GWA3_CHECK_OFFSET(PartyInfo, henchmen,    0x14);
GWA3_CHECK_OFFSET(PartyInfo, heroes,      0x24);
GWA3_CHECK_OFFSET(PartyInfo, others,      0x34);
GWA3_CHECK_OFFSET(PartyInfo, h0044,       0x44);
GWA3_CHECK_OFFSET(PartyInfo, invite_link, 0x7C);

// ===== HeroFlag — 0x24 / 36 bytes =====
GWA3_CHECK_SIZE(HeroFlag, 0x24);
GWA3_CHECK_OFFSET(HeroFlag, hero_id,          0x00);
GWA3_CHECK_OFFSET(HeroFlag, agent_id,         0x04);
GWA3_CHECK_OFFSET(HeroFlag, level,            0x08);
GWA3_CHECK_OFFSET(HeroFlag, hero_behavior,    0x0C);
GWA3_CHECK_OFFSET(HeroFlag, flag,             0x10);
GWA3_CHECK_OFFSET(HeroFlag, locked_target_id, 0x20);

// ===== HeroInfo — 0x78 / 120 bytes =====
GWA3_CHECK_SIZE(HeroInfo, 120);
GWA3_CHECK_OFFSET(HeroInfo, hero_id,       0x00);
GWA3_CHECK_OFFSET(HeroInfo, agent_id,      0x04);
GWA3_CHECK_OFFSET(HeroInfo, level,         0x08);
GWA3_CHECK_OFFSET(HeroInfo, primary,       0x0C);
GWA3_CHECK_OFFSET(HeroInfo, secondary,     0x10);
GWA3_CHECK_OFFSET(HeroInfo, hero_file_id,  0x14);
GWA3_CHECK_OFFSET(HeroInfo, model_file_id, 0x18);
GWA3_CHECK_OFFSET(HeroInfo, name,          0x50);

// ===== Player — 0x50 / 80 bytes =====
GWA3_CHECK_SIZE(Player, 0x50);
GWA3_CHECK_OFFSET(Player, agent_id,                    0x00);
GWA3_CHECK_OFFSET(Player, appearance_bitmap,           0x10);
GWA3_CHECK_OFFSET(Player, flags,                       0x14);
GWA3_CHECK_OFFSET(Player, primary,                     0x18);
GWA3_CHECK_OFFSET(Player, secondary,                   0x1C);
GWA3_CHECK_OFFSET(Player, name_enc,                    0x24);
GWA3_CHECK_OFFSET(Player, name,                        0x28);
GWA3_CHECK_OFFSET(Player, party_leader_player_number,  0x2C);
GWA3_CHECK_OFFSET(Player, active_title_tier,           0x30);
GWA3_CHECK_OFFSET(Player, player_number,               0x38);
GWA3_CHECK_OFFSET(Player, party_size,                  0x3C);
GWA3_CHECK_OFFSET(Player, h0040,                       0x40);

// ===== Camera — 0x120 / 288 bytes =====
GWA3_CHECK_SIZE(Camera, 0x120);
GWA3_CHECK_OFFSET(Camera, look_at_agent_id, 0x00);
GWA3_CHECK_OFFSET(Camera, max_distance,     0x10);
GWA3_CHECK_OFFSET(Camera, yaw,              0x18);
GWA3_CHECK_OFFSET(Camera, pitch,            0x1C);
GWA3_CHECK_OFFSET(Camera, distance,         0x20);
GWA3_CHECK_OFFSET(Camera, yaw_right_click,  0x34);
GWA3_CHECK_OFFSET(Camera, distance2,        0x40);
GWA3_CHECK_OFFSET(Camera, time_in_the_map,  0x58);
GWA3_CHECK_OFFSET(Camera, yaw_to_go,        0x60);
GWA3_CHECK_OFFSET(Camera, pitch_to_go,      0x64);
GWA3_CHECK_OFFSET(Camera, dist_to_go,       0x68);
GWA3_CHECK_OFFSET(Camera, max_distance2,    0x6C);
GWA3_CHECK_OFFSET(Camera, position,         0x78);
GWA3_CHECK_OFFSET(Camera, camera_pos_to_go, 0x84);
GWA3_CHECK_OFFSET(Camera, cam_pos_inverted, 0x90);
GWA3_CHECK_OFFSET(Camera, look_at_target,   0xA8);
GWA3_CHECK_OFFSET(Camera, look_at_to_go,    0xB4);
GWA3_CHECK_OFFSET(Camera, field_of_view,    0xC0);
GWA3_CHECK_OFFSET(Camera, field_of_view2,   0xC4);
GWA3_CHECK_OFFSET(Camera, camera_mode,      0x11C);

// ===== GHKey — 0x10 / 16 bytes =====
GWA3_CHECK_SIZE(GHKey, 16);

// ===== CapeDesign — 0x1C / 28 bytes =====
GWA3_CHECK_SIZE(CapeDesign, 0x1C);
GWA3_CHECK_OFFSET(CapeDesign, cape_bg_color,     0x00);
GWA3_CHECK_OFFSET(CapeDesign, cape_detail_color, 0x04);
GWA3_CHECK_OFFSET(CapeDesign, cape_emblem_color, 0x08);
GWA3_CHECK_OFFSET(CapeDesign, cape_shape,        0x0C);
GWA3_CHECK_OFFSET(CapeDesign, cape_detail,       0x10);
GWA3_CHECK_OFFSET(CapeDesign, cape_emblem,       0x14);
GWA3_CHECK_OFFSET(CapeDesign, cape_trim,         0x18);

// ===== Guild — 0xAC / 172 bytes =====
GWA3_CHECK_SIZE(Guild, 172);
GWA3_CHECK_OFFSET(Guild, key,              0x00);
GWA3_CHECK_OFFSET(Guild, index,            0x24);
GWA3_CHECK_OFFSET(Guild, rank,             0x28);
GWA3_CHECK_OFFSET(Guild, features,         0x2C);
GWA3_CHECK_OFFSET(Guild, name,             0x30);
GWA3_CHECK_OFFSET(Guild, rating,           0x70);
GWA3_CHECK_OFFSET(Guild, faction,          0x74);
GWA3_CHECK_OFFSET(Guild, faction_point,    0x78);
GWA3_CHECK_OFFSET(Guild, qualifier_point,  0x7C);
GWA3_CHECK_OFFSET(Guild, tag,              0x80);
GWA3_CHECK_OFFSET(Guild, cape,             0x90);

// ===== GuildPlayer — 0x174 / 372 bytes =====
GWA3_CHECK_SIZE(GuildPlayer, 372);
GWA3_CHECK_OFFSET(GuildPlayer, vtable,        0x00);
GWA3_CHECK_OFFSET(GuildPlayer, name_ptr,      0x04);
GWA3_CHECK_OFFSET(GuildPlayer, invited_name,  0x08);
GWA3_CHECK_OFFSET(GuildPlayer, current_name,  0x30);
GWA3_CHECK_OFFSET(GuildPlayer, inviter_name,  0x58);
GWA3_CHECK_OFFSET(GuildPlayer, invite_time,   0x80);
GWA3_CHECK_OFFSET(GuildPlayer, promoter_name, 0x84);
GWA3_CHECK_OFFSET(GuildPlayer, offline,       0xDC);
GWA3_CHECK_OFFSET(GuildPlayer, member_type,   0xE0);
GWA3_CHECK_OFFSET(GuildPlayer, status,        0xE4);

// ===== GuildHistoryEvent — 0x208 / 520 bytes =====
GWA3_CHECK_SIZE(GuildHistoryEvent, 520);
GWA3_CHECK_OFFSET(GuildHistoryEvent, time1, 0x00);
GWA3_CHECK_OFFSET(GuildHistoryEvent, time2, 0x04);
GWA3_CHECK_OFFSET(GuildHistoryEvent, name,  0x08);

// ===== TownAlliance — 0x78 / 120 bytes =====
GWA3_CHECK_SIZE(TownAlliance, 0x78);
GWA3_CHECK_OFFSET(TownAlliance, rank,       0x00);
GWA3_CHECK_OFFSET(TownAlliance, allegiance, 0x04);
GWA3_CHECK_OFFSET(TownAlliance, faction,    0x08);
GWA3_CHECK_OFFSET(TownAlliance, name,       0x0C);
GWA3_CHECK_OFFSET(TownAlliance, tag,        0x4C);
GWA3_CHECK_OFFSET(TownAlliance, cape,       0x58);
GWA3_CHECK_OFFSET(TownAlliance, map_id,     0x74);

// ===== Title — 0x2C / 44 bytes =====
GWA3_CHECK_SIZE(Title, 0x2C);
GWA3_CHECK_OFFSET(Title, props,                      0x00);
GWA3_CHECK_OFFSET(Title, current_points,             0x04);
GWA3_CHECK_OFFSET(Title, current_title_tier_index,   0x08);
GWA3_CHECK_OFFSET(Title, points_needed_current_rank, 0x0C);
GWA3_CHECK_OFFSET(Title, next_title_tier_index,      0x14);
GWA3_CHECK_OFFSET(Title, points_needed_next_rank,    0x18);
GWA3_CHECK_OFFSET(Title, max_title_rank,             0x1C);
GWA3_CHECK_OFFSET(Title, max_title_tier_index,       0x20);
GWA3_CHECK_OFFSET(Title, points_desc,                0x24);
GWA3_CHECK_OFFSET(Title, h0028,                      0x28);

// ===== TitleTier — 0x0C / 12 bytes =====
GWA3_CHECK_SIZE(TitleTier, 0x0C);
GWA3_CHECK_OFFSET(TitleTier, props,         0x00);
GWA3_CHECK_OFFSET(TitleTier, tier_number,   0x04);
GWA3_CHECK_OFFSET(TitleTier, tier_name_enc, 0x08);

// ===== TitleClientData — 0x0C / 12 bytes =====
GWA3_CHECK_SIZE(TitleClientData, 0x0C);
GWA3_CHECK_OFFSET(TitleClientData, title_flags, 0x00);
GWA3_CHECK_OFFSET(TitleClientData, title_id,    0x04);
GWA3_CHECK_OFFSET(TitleClientData, name_id,     0x08);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Runtime confirmation that all static_asserts passed (if we get here, the build succeeded)
GWA3_TEST(struct_offsets_compile_check, {
    GWA3_ASSERT(true);
})
