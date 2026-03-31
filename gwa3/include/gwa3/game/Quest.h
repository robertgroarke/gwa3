#pragma once

// GWA3 Quest structs — matches GWCA GameEntities/Quest.h memory layout.
// Cross-referenced with GWA2_Assembly.au3 $QUEST_STRUCT_TEMPLATE.

#include <cstdint>

namespace GWA3 {

struct Quest { // total: 0x34 / 52 bytes
    /* +h0000 */ uint32_t quest_id;
    /* +h0004 */ uint32_t log_state;
    /* +h0008 */ wchar_t* location;
    /* +h000C */ wchar_t* name;
    /* +h0010 */ wchar_t* npc;
    /* +h0014 */ uint32_t map_from;
    /* +h0018 */ float    marker_x;
    /* +h001C */ float    marker_y;
    /* +h0020 */ uint32_t marker_plane;
    /* +h0024 */ uint32_t h0024;
    /* +h0028 */ uint32_t map_to;
    /* +h002C */ wchar_t* description;
    /* +h0030 */ wchar_t* objectives;
};

struct MissionObjective { // total: 0x0C / 12 bytes
    /* +h0000 */ uint32_t objective_id;
    /* +h0004 */ wchar_t* enc_str;
    /* +h0008 */ uint32_t type;
};

} // namespace GWA3
