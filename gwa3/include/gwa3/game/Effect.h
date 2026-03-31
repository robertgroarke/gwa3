#pragma once

// GWA3 Effect/Buff structs — matches GWCA GameEntities/Skill.h (effects section).
// Cross-referenced with GWA2_Assembly.au3 $EFFECT_STRUCT_TEMPLATE / $BUFF_STRUCT_TEMPLATE.

#include <cstdint>
#include <gwa3/game/GameTypes.h>

namespace GWA3 {

struct Effect { // total: 0x18 / 24 bytes
    /* +h0000 */ uint32_t skill_id;
    /* +h0004 */ uint32_t attribute_level;
    /* +h0008 */ uint32_t effect_id;
    /* +h000C */ uint32_t agent_id;
    /* +h0010 */ float    duration;
    /* +h0014 */ uint32_t timestamp;
};

struct Buff { // total: 0x10 / 16 bytes
    /* +h0000 */ uint32_t skill_id;
    /* +h0004 */ uint32_t h0004;
    /* +h0008 */ uint32_t buff_id;
    /* +h000C */ uint32_t target_agent_id;
};

struct AgentEffects { // total: 0x24 / 36 bytes
    /* +h0000 */ uint32_t          agent_id;
    /* +h0004 */ GWArray<Buff>     buffs;
    /* +h0014 */ GWArray<Effect>   effects;
};

} // namespace GWA3
