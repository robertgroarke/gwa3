#pragma once

// GWA3 Map structs — matches GWCA GameEntities/Map.h memory layout.
// Cross-referenced with GWA2_Assembly.au3 $AREA_INFO_STRUCT_TEMPLATE.

#include <cstdint>

namespace GWA3 {

struct AreaInfo { // total: 0x7C / 124 bytes
    /* +h0000 */ uint32_t campaign;
    /* +h0004 */ uint32_t continent;
    /* +h0008 */ uint32_t region;
    /* +h000C */ uint32_t type;
    /* +h0010 */ uint32_t flags;
    /* +h0014 */ uint32_t thumbnail_id;
    /* +h0018 */ uint32_t min_party_size;
    /* +h001C */ uint32_t max_party_size;
    /* +h0020 */ uint32_t min_player_size;
    /* +h0024 */ uint32_t max_player_size;
    /* +h0028 */ uint32_t controlled_outpost_id;
    /* +h002C */ uint32_t fraction_mission;
    /* +h0030 */ uint32_t min_level;
    /* +h0034 */ uint32_t max_level;
    /* +h0038 */ uint32_t needed_pq;
    /* +h003C */ uint32_t mission_maps_to;
    /* +h0040 */ uint32_t x;
    /* +h0044 */ uint32_t y;
    /* +h0048 */ uint32_t icon_start_x;
    /* +h004C */ uint32_t icon_start_y;
    /* +h0050 */ uint32_t icon_end_x;
    /* +h0054 */ uint32_t icon_end_y;
    /* +h0058 */ uint32_t icon_start_x_dupe;
    /* +h005C */ uint32_t icon_start_y_dupe;
    /* +h0060 */ uint32_t icon_end_x_dupe;
    /* +h0064 */ uint32_t icon_end_y_dupe;
    /* +h0068 */ uint32_t file_id;
    /* +h006C */ uint32_t mission_chronology;
    /* +h0070 */ uint32_t ha_map_chronology;
    /* +h0074 */ uint32_t name_id;
    /* +h0078 */ uint32_t description_id;
};

struct MissionMapIcon { // total: 0x28 / 40 bytes
    /* +h0000 */ uint32_t index;
    /* +h0004 */ float    x;
    /* +h0008 */ float    y;
    /* +h000C */ uint32_t h000C;
    /* +h0010 */ uint32_t h0010;
    /* +h0014 */ uint32_t option;
    /* +h0018 */ uint32_t h0018;
    /* +h001C */ uint32_t model_id;
    /* +h0020 */ uint32_t h0020;
    /* +h0024 */ uint32_t h0024;
};

} // namespace GWA3
