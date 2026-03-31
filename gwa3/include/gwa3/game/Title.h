#pragma once

// GWA3 Title structs — matches GWCA GameEntities/Title.h memory layout.
// Cross-referenced with GWA2_Assembly.au3 $TITLE_STRUCT_TEMPLATE.

#include <cstdint>
#include <gwa3/game/GameTypes.h>

namespace GWA3 {

struct Title { // total: 0x2C / 44 bytes
    /* +h0000 */ uint32_t props;
    /* +h0004 */ uint32_t current_points;
    /* +h0008 */ uint32_t current_title_tier_index;
    /* +h000C */ uint32_t points_needed_current_rank;
    /* +h0010 */ uint32_t h0010;
    /* +h0014 */ uint32_t next_title_tier_index;
    /* +h0018 */ uint32_t points_needed_next_rank;
    /* +h001C */ uint32_t max_title_rank;
    /* +h0020 */ uint32_t max_title_tier_index;
    /* +h0024 */ wchar_t* points_desc;
    /* +h0028 */ wchar_t* h0028;

    bool IsPercentageBased() const { return (props & 1) != 0; }
    bool HasTiers()          const { return (props & 3) == 2; }
};

struct TitleTier { // total: 0x0C / 12 bytes
    /* +h0000 */ uint32_t props;
    /* +h0004 */ uint32_t tier_number;
    /* +h0008 */ wchar_t* tier_name_enc;

    bool IsPercentageBased() const { return (props & 1) != 0; }
};

struct TitleClientData { // total: 0x0C / 12 bytes
    /* +h0000 */ uint32_t title_flags;
    /* +h0004 */ uint32_t title_id;
    /* +h0008 */ uint32_t name_id;
};

// Common title IDs used by the bot
namespace TitleID {
    constexpr uint32_t Hero                 = 0;
    constexpr uint32_t TyrianCartographer   = 1;
    constexpr uint32_t CanthanCartographer  = 2;
    constexpr uint32_t Gladiator            = 3;
    constexpr uint32_t Champion             = 4;
    constexpr uint32_t Kurzick              = 7;
    constexpr uint32_t Luxon                = 8;
    constexpr uint32_t Drunkard             = 9;
    constexpr uint32_t Survivor             = 11;
    constexpr uint32_t KindOfABigDeal       = 12;
    constexpr uint32_t Sunspear             = 20;
    constexpr uint32_t Lightbringer         = 21;
    constexpr uint32_t ElonianCartographer  = 22;
    constexpr uint32_t Vanguard             = 28;
    constexpr uint32_t Norn                 = 29;
    constexpr uint32_t Asura                = 30;
    constexpr uint32_t Deldrimor            = 31;
    constexpr uint32_t Master               = 34;
}

} // namespace GWA3
