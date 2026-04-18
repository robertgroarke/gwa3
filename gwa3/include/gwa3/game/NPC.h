#pragma once

// GWA3 NPC struct — matches GWCA GameEntities/NPC.h memory layout.
// Stored in WorldContext.npcs (NPCArray at +0x7FC) and indexed by the
// NPC's player_number (which, for NPCs, is actually the NPC id).
// Used as the fallback name source when AgentInfo.name_enc is null.

#include <cstdint>

namespace GWA3 {

struct NPC { // total: 0x30 / 48 bytes
    /* +h0000 */ uint32_t  model_file_id;
    /* +h0004 */ uint32_t  h0004;
    /* +h0008 */ uint32_t  scale;
    /* +h000C */ uint32_t  sex;
    /* +h0010 */ uint32_t  npc_flags;
    /* +h0014 */ uint32_t  primary;
    /* +h0018 */ uint32_t  h0018;
    /* +h001C */ uint8_t   default_level;
    /* +h001D */ uint8_t   h001D[3];
    /* +h0020 */ wchar_t*  name_enc;
    /* +h0024 */ uint32_t* model_files;
    /* +h0028 */ uint32_t  files_count;
    /* +h002C */ uint32_t  files_capacity;
};

} // namespace GWA3
