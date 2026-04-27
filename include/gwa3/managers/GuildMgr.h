#pragma once

// GuildMgr — guild data, guild hall travel.

#include <gwa3/game/Guild.h>
#include <cstdint>

namespace GWA3::GuildMgr {

    bool Initialize();

    // Get player's guild info (null if not in a guild or offset not resolved)
    Guild* GetPlayerGuild();

    // Get guild array (all guilds in context)
    GWArray<Guild*>* GetGuildArray();

    // Get guild by ID from the guild array
    Guild* GetGuildInfo(uint32_t guildId);

    // Player's guild index in the guild array
    uint32_t GetPlayerGuildIndex();

    // Guild announcement text (null if unavailable)
    wchar_t* GetPlayerGuildAnnouncement();

    // Guild hall travel
    bool TravelGH();
    bool TravelGH(const GHKey& key);

} // namespace GWA3::GuildMgr
