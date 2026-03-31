#pragma once

// GWA3 Agent structs — matches GWCA GameEntities/Agent.h memory layout.
// Cross-referenced with GWA2_Assembly.au3 $AGENT_STRUCT_TEMPLATE.

#include <cstdint>
#include <gwa3/game/GameTypes.h>

namespace GWA3 {

struct Agent { // total: 0xC4 / 196 bytes
    /* +h0000 */ uint32_t* vtable;
    /* +h0004 */ uint32_t  h0004;
    /* +h0008 */ uint32_t  h0008;
    /* +h000C */ uint32_t  h000C[2];
    /* +h0014 */ uint32_t  timer;
    /* +h0018 */ uint32_t  timer2;
    /* +h001C */ TLink     link;
    /* +h0024 */ TLink     link2;
    /* +h002C */ uint32_t  agent_id;
    /* +h0030 */ float     z;
    /* +h0034 */ float     width1;
    /* +h0038 */ float     height1;
    /* +h003C */ float     width2;
    /* +h0040 */ float     height2;
    /* +h0044 */ float     width3;
    /* +h0048 */ float     height3;
    /* +h004C */ float     rotation_angle;
    /* +h0050 */ float     rotation_cos;
    /* +h0054 */ float     rotation_sin;
    /* +h0058 */ uint32_t  name_properties;
    /* +h005C */ uint32_t  ground;
    /* +h0060 */ uint32_t  h0060;
    /* +h0064 */ Vec3f     terrain_normal;
    /* +h0070 */ uint8_t   h0070[4];
    /* +h0074 */ float     x;
    /* +h0078 */ float     y;
    /* +h007C */ uint32_t  plane;
    /* +h0080 */ uint8_t   h0080[4];
    /* +h0084 */ float     name_tag_x;
    /* +h0088 */ float     name_tag_y;
    /* +h008C */ float     name_tag_z;
    /* +h0090 */ uint16_t  visual_effects;
    /* +h0092 */ uint16_t  h0092;
    /* +h0094 */ uint32_t  h0094[2];
    /* +h009C */ uint32_t  type;
    /* +h00A0 */ float     move_x;
    /* +h00A4 */ float     move_y;
    /* +h00A8 */ uint32_t  h00A8;
    /* +h00AC */ float     rotation_cos2;
    /* +h00B0 */ float     rotation_sin2;
    /* +h00B4 */ uint32_t  h00B4[4];
};

struct AgentItem : public Agent { // total: 0xD4 / 212 bytes
    /* +h00C4 */ uint32_t owner;
    /* +h00C8 */ uint32_t item_id;
    /* +h00CC */ uint32_t h00CC;
    /* +h00D0 */ uint32_t extra_type;
};

struct AgentGadget : public Agent { // total: 0xE4 / 228 bytes
    /* +h00C4 */ uint32_t h00C4;
    /* +h00C8 */ uint32_t h00C8;
    /* +h00CC */ uint32_t extra_type;
    /* +h00D0 */ uint32_t gadget_id;
    /* +h00D4 */ uint32_t h00D4[4];
};

struct AgentLiving : public Agent { // total: 0x1C4 / 452 bytes
    /* +h00C4 */ uint32_t owner;
    /* +h00C8 */ uint32_t h00C8_living;
    /* +h00CC */ uint32_t h00CC_living;
    /* +h00D0 */ uint32_t h00D0_living;
    /* +h00D4 */ uint32_t h00D4_living[3];
    /* +h00E0 */ float    animation_type;
    /* +h00E4 */ uint32_t h00E4[2];
    /* +h00EC */ float    weapon_attack_speed;
    /* +h00F0 */ float    attack_speed_modifier;
    /* +h00F4 */ uint16_t player_number;
    /* +h00F6 */ uint16_t agent_model_type;
    /* +h00F8 */ uint32_t transmog_npc_id;
    /* +h00FC */ void*    equip;
    /* +h0100 */ uint32_t h0100;
    /* +h0104 */ uint32_t h0104;
    /* +h0108 */ void*    tags;
    /* +h010C */ uint16_t h010C;
    /* +h010E */ uint8_t  primary;
    /* +h010F */ uint8_t  secondary;
    /* +h0110 */ uint8_t  level;
    /* +h0111 */ uint8_t  team_id;
    /* +h0112 */ uint8_t  h0112[2];
    /* +h0114 */ uint32_t h0114;
    /* +h0118 */ float    energy_regen;
    /* +h011C */ uint32_t h011C;
    /* +h0120 */ float    energy;
    /* +h0124 */ uint32_t max_energy;
    /* +h0128 */ uint32_t h0128;
    /* +h012C */ float    hp_pips;
    /* +h0130 */ uint32_t h0130;
    /* +h0134 */ float    hp;
    /* +h0138 */ uint32_t max_hp;
    /* +h013C */ uint32_t effects;
    /* +h0140 */ uint32_t h0140;
    /* +h0144 */ uint8_t  hex;
    /* +h0145 */ uint8_t  h0145[19];
    /* +h0158 */ uint32_t model_state;
    /* +h015C */ uint32_t type_map;
    /* +h0160 */ uint32_t h0160[4];
    /* +h0170 */ uint32_t in_spirit_range;
    /* +h0174 */ TList    visible_effects;
    /* +h0180 */ uint32_t h0180;
    /* +h0184 */ uint32_t login_number;
    /* +h0188 */ float    animation_speed;
    /* +h018C */ uint32_t animation_code;
    /* +h0190 */ uint32_t animation_id;
    /* +h0194 */ uint8_t  h0194[32];
    /* +h01B4 */ uint8_t  dagger_status;
    /* +h01B5 */ uint8_t  allegiance;
    /* +h01B6 */ uint16_t weapon_type;
    /* +h01B8 */ uint16_t skill;
    /* +h01BA */ uint16_t h01BA;
    /* +h01BC */ uint8_t  weapon_item_type;
    /* +h01BD */ uint8_t  offhand_item_type;
    /* +h01BE */ uint16_t weapon_item_id;
    /* +h01C0 */ uint16_t offhand_item_id;
};

struct MapAgent { // total: 0x34 / 52 bytes
    /* +h0000 */ float    cur_energy;
    /* +h0004 */ float    max_energy;
    /* +h0008 */ float    energy_regen;
    /* +h000C */ uint32_t skill_timestamp;
    /* +h0010 */ float    h0010;
    /* +h0014 */ float    max_energy2;
    /* +h0018 */ float    h0018;
    /* +h001C */ uint32_t h001C;
    /* +h0020 */ float    cur_health;
    /* +h0024 */ float    max_health;
    /* +h0028 */ float    health_regen;
    /* +h002C */ uint32_t h002C;
    /* +h0030 */ uint32_t effects;
};

struct AgentInfo { // total: 0x38 / 56 bytes
    /* +h0000 */ uint32_t h0000[13];
    /* +h0034 */ wchar_t* name_enc;
};

} // namespace GWA3
