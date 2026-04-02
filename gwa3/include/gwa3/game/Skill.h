#pragma once

// GWA3 Skill structs — matches GWCA GameEntities/Skill.h memory layout.
// Cross-referenced with GWA2_Assembly.au3 $SKILL_STRUCT_TEMPLATE / $SKILLBAR_STRUCT_TEMPLATE.

#include <cstdint>
#include <gwa3/game/GameTypes.h>

namespace GWA3 {

struct Skill { // total: 0xA4 / 164 bytes
    /* +h0000 */ uint32_t skill_id;
    /* +h0004 */ uint32_t h0004;
    /* +h0008 */ uint32_t campaign;
    /* +h000C */ uint32_t type;
    /* +h0010 */ uint32_t special;
    /* +h0014 */ uint32_t combo_req;
    /* +h0018 */ uint32_t effect1;
    /* +h001C */ uint32_t condition;
    /* +h0020 */ uint32_t effect2;
    /* +h0024 */ uint32_t weapon_req;
    /* +h0028 */ uint8_t  profession;
    /* +h0029 */ uint8_t  attribute;
    /* +h002A */ uint16_t title;
    /* +h002C */ uint32_t skill_id_pvp;
    /* +h0030 */ uint8_t  combo;
    /* +h0031 */ uint8_t  target;
    /* +h0032 */ uint8_t  h0032;
    /* +h0033 */ uint8_t  skill_equip_type;
    /* +h0034 */ uint8_t  overcast;
    /* +h0035 */ uint8_t  energy_cost;
    /* +h0036 */ uint8_t  health_cost;
    /* +h0037 */ uint8_t  h0037;
    /* +h0038 */ uint32_t adrenaline;
    /* +h003C */ float    activation;
    /* +h0040 */ float    aftercast;
    /* +h0044 */ uint32_t duration0;
    /* +h0048 */ uint32_t duration15;
    /* +h004C */ uint32_t recharge;
    /* +h0050 */ uint16_t h0050[4];
    /* +h0058 */ uint32_t skill_arguments;
    /* +h005C */ uint32_t scale0;
    /* +h0060 */ uint32_t scale15;
    /* +h0064 */ uint32_t bonus_scale0;
    /* +h0068 */ uint32_t bonus_scale15;
    /* +h006C */ float    aoe_range;
    /* +h0070 */ float    const_effect;
    /* +h0074 */ uint32_t caster_overhead_animation_id;
    /* +h0078 */ uint32_t caster_body_animation_id;
    /* +h007C */ uint32_t target_body_animation_id;
    /* +h0080 */ uint32_t target_overhead_animation_id;
    /* +h0084 */ uint32_t projectile_animation_1_id;
    /* +h0088 */ uint32_t projectile_animation_2_id;
    /* +h008C */ uint32_t icon_file_id;
    /* +h0090 */ uint32_t icon_file_id_2;
    /* +h0094 */ uint32_t icon_file_id_hi_res;
    /* +h0098 */ uint32_t name;
    /* +h009C */ uint32_t concise;
    /* +h00A0 */ uint32_t description;
};

struct SkillbarSkill { // total: 0x14 / 20 bytes
    /* +h0000 */ uint32_t adrenaline_a;
    /* +h0004 */ uint32_t adrenaline_b;
    /* +h0008 */ uint32_t recharge;
    /* +h000C */ uint32_t skill_id;
    /* +h0010 */ uint32_t event;
};

struct SkillbarCast { // total: 0x0C / 12 bytes
    /* +h0000 */ uint16_t h0000;
    /* +h0002 */ uint16_t h0002; // padding to align skill_id
    /* +h0004 */ uint32_t skill_id;
    /* +h0008 */ uint32_t h0008;
};

struct Skillbar { // total: 0xBC / 188 bytes
    /* +h0000 */ uint32_t      agent_id;
    /* +h0004 */ SkillbarSkill skills[8];
    /* +h00A4 */ uint32_t      disabled;
    /* +h00A8 */ GWArray<SkillbarCast> cast_array;
    /* +h00B8 */ uint32_t      h00B8;
};

// ===== Effect / Buff structs (from GWCA GameEntities/Skill.h) =====

struct Effect { // total: 0x18 / 24 bytes
    /* +h0000 */ uint32_t skill_id;
    /* +h0004 */ uint32_t attribute_level;
    /* +h0008 */ uint32_t effect_id;
    /* +h000C */ uint32_t agent_id;       // non-zero = maintained enchantment caster
    /* +h0010 */ float    duration;        // seconds, 0 if no duration
    /* +h0014 */ uint32_t timestamp;       // GW-timestamp when applied
};

struct Buff { // total: 0x10 / 16 bytes
    /* +h0000 */ uint32_t skill_id;
    /* +h0004 */ uint32_t h0004;
    /* +h0008 */ uint32_t buff_id;
    /* +h000C */ uint32_t target_agent_id; // 0 if no target
};

struct AgentEffects { // total: 0x24 / 36 bytes
    /* +h0000 */ uint32_t          agent_id;
    /* +h0004 */ GWArray<Buff>     buffs;
    /* +h0014 */ GWArray<Effect>   effects;
};

} // namespace GWA3
