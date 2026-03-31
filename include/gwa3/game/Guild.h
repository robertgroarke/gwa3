#pragma once

// GWA3 Guild structs — matches GWCA GameEntities/Guild.h memory layout.

#include <cstdint>
#include <gwa3/game/GameTypes.h>

namespace GWA3 {

struct GHKey {
    uint32_t k[4];
};

struct CapeDesign { // total: 0x1C / 28 bytes
    /* +h0000 */ uint32_t cape_bg_color;
    /* +h0004 */ uint32_t cape_detail_color;
    /* +h0008 */ uint32_t cape_emblem_color;
    /* +h000C */ uint32_t cape_shape;
    /* +h0010 */ uint32_t cape_detail;
    /* +h0014 */ uint32_t cape_emblem;
    /* +h0018 */ uint32_t cape_trim;
};

struct Guild { // total: 0xAC / 172 bytes
    /* +h0000 */ GHKey    key;
    /* +h0010 */ uint32_t h0010[5];
    /* +h0024 */ uint32_t index;
    /* +h0028 */ uint32_t rank;
    /* +h002C */ uint32_t features;
    /* +h0030 */ wchar_t  name[32];
    /* +h0070 */ uint32_t rating;
    /* +h0074 */ uint32_t faction;
    /* +h0078 */ uint32_t faction_point;
    /* +h007C */ uint32_t qualifier_point;
    /* +h0080 */ wchar_t  tag[8];
    /* +h0090 */ CapeDesign cape;
};

struct GuildPlayer { // total: 0x174 / 372 bytes
    /* +h0000 */ void*    vtable;
    /* +h0004 */ wchar_t* name_ptr;
    /* +h0008 */ wchar_t  invited_name[20];
    /* +h0030 */ wchar_t  current_name[20];
    /* +h0058 */ wchar_t  inviter_name[20];
    /* +h0080 */ uint32_t invite_time;
    /* +h0084 */ wchar_t  promoter_name[20];
    /* +h00AC */ uint32_t h00AC[12];
    /* +h00DC */ uint32_t offline;
    /* +h00E0 */ uint32_t member_type;
    /* +h00E4 */ uint32_t status;
    /* +h00E8 */ uint32_t h00E8[35];
};

struct GuildHistoryEvent { // total: 0x208 / 520 bytes
    /* +h0000 */ uint32_t time1;
    /* +h0004 */ uint32_t time2;
    /* +h0008 */ wchar_t  name[256];
};

struct TownAlliance { // total: 0x78 / 120 bytes
    /* +h0000 */ uint32_t   rank;
    /* +h0004 */ uint32_t   allegiance;
    /* +h0008 */ uint32_t   faction;
    /* +h000C */ wchar_t    name[32];
    /* +h004C */ wchar_t    tag[5];
    /* +h0056 */ uint8_t    _padding[2];
    /* +h0058 */ CapeDesign cape;
    /* +h0074 */ uint32_t   map_id;
};

} // namespace GWA3
