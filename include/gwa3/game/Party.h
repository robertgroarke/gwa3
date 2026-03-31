#pragma once

// GWA3 Party/Hero structs — matches GWCA GameEntities/Party.h and Hero.h memory layout.

#include <cstdint>
#include <gwa3/game/GameTypes.h>

namespace GWA3 {

struct PlayerPartyMember { // total: 0x0C / 12 bytes
    /* +h0000 */ uint32_t login_number;
    /* +h0004 */ uint32_t called_target_id;
    /* +h0008 */ uint32_t state;
};

struct HeroPartyMember { // total: 0x18 / 24 bytes
    /* +h0000 */ uint32_t agent_id;
    /* +h0004 */ uint32_t owner_player_id;
    /* +h0008 */ uint32_t hero_id;
    /* +h000C */ uint32_t h000C;
    /* +h0010 */ uint32_t h0010;
    /* +h0014 */ uint32_t level;
};

struct HenchmanPartyMember { // total: 0x34 / 52 bytes
    /* +h0000 */ uint32_t agent_id;
    /* +h0004 */ uint32_t h0004[10];
    /* +h002C */ uint32_t profession;
    /* +h0030 */ uint32_t level;
};

struct PartyInfo { // total: 0x84 / 132 bytes
    /* +h0000 */ uint32_t                         party_id;
    /* +h0004 */ GWArray<PlayerPartyMember>        players;
    /* +h0014 */ GWArray<HenchmanPartyMember>      henchmen;
    /* +h0024 */ GWArray<HeroPartyMember>          heroes;
    /* +h0034 */ GWArray<uint32_t>                 others;
    /* +h0044 */ uint32_t                         h0044[14];
    /* +h007C */ TLink                            invite_link;
};

struct HeroFlag { // total: 0x24 / 36 bytes
    /* +h0000 */ uint32_t hero_id;
    /* +h0004 */ uint32_t agent_id;
    /* +h0008 */ uint32_t level;
    /* +h000C */ uint32_t hero_behavior;
    /* +h0010 */ Vec2f    flag;
    /* +h0018 */ uint32_t h0018;
    /* +h001C */ uint32_t h001C;
    /* +h0020 */ uint32_t locked_target_id;
};

struct HeroInfo { // total: 0x78 / 120 bytes
    /* +h0000 */ uint32_t hero_id;
    /* +h0004 */ uint32_t agent_id;
    /* +h0008 */ uint32_t level;
    /* +h000C */ uint32_t primary;
    /* +h0010 */ uint32_t secondary;
    /* +h0014 */ uint32_t hero_file_id;
    /* +h0018 */ uint32_t model_file_id;
    /* +h001C */ uint8_t  h001C[52];
    /* +h0050 */ wchar_t  name[20];
};

} // namespace GWA3
