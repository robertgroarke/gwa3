bool TestGuildData() {
    IntReport("=== Guild Data Introspection ===");

    if (ReadMyId() == 0) {
        IntSkip("Guild data", "Not in game");
        IntReport("");
        return false;
    }

    GWArray<Guild*>* guildArray = GuildMgr::GetGuildArray();
    IntReport("  GuildArray: %p (size=%u)", guildArray, guildArray ? guildArray->size : 0);

    if (!guildArray || guildArray->size == 0) {
        IntSkip("Guild array contents", "No guilds in context (player may not be in a guild)");
        IntCheck("GuildMgr queries ran without crash", true);
        IntReport("");
        return true;
    }

    IntCheck("GuildArray has entries", guildArray->size > 0);

    const uint32_t playerGuildIdx = GuildMgr::GetPlayerGuildIndex();
    IntReport("  PlayerGuildIndex: %u", playerGuildIdx);

    if (guildArray->buffer && guildArray->size > 0) {
        Guild* first = guildArray->buffer[0];
        if (first) {
            char nameBuf[64] = {};
            for (int i = 0; i < 31 && first->name[i]; ++i) {
                nameBuf[i] = (first->name[i] < 128) ? static_cast<char>(first->name[i]) : '?';
            }
            char tagBuf[16] = {};
            for (int i = 0; i < 7 && first->tag[i]; ++i) {
                tagBuf[i] = (first->tag[i] < 128) ? static_cast<char>(first->tag[i]) : '?';
            }
            IntReport("  First guild: index=%u rank=%u name='%s' tag='[%s]' rating=%u faction=%u",
                      first->index, first->rank, nameBuf, tagBuf, first->rating, first->faction);
            IntCheck("First guild has valid index", first->index > 0);
        }
    }

    Guild* playerGuild = GuildMgr::GetPlayerGuild();
    IntReport("  GetPlayerGuild: %p", playerGuild);
    if (playerGuild) {
        char nameBuf[64] = {};
        for (int i = 0; i < 31 && playerGuild->name[i]; ++i) {
            nameBuf[i] = (playerGuild->name[i] < 128) ? static_cast<char>(playerGuild->name[i]) : '?';
        }
        IntReport("  Player guild: '%s' index=%u", nameBuf, playerGuild->index);
        IntCheck("Player guild index matches GetPlayerGuildIndex", playerGuild->index == playerGuildIdx);
    } else if (playerGuildIdx == 0) {
        IntSkip("Player guild details", "Player not in a guild");
    } else {
        IntCheck("GetPlayerGuild returned non-null for non-zero index", false);
    }

    wchar_t* announcement = GuildMgr::GetPlayerGuildAnnouncement();
    if (announcement) {
        char annBuf[64] = {};
        for (int i = 0; i < 63 && announcement[i]; ++i) {
            annBuf[i] = (announcement[i] < 128) ? static_cast<char>(announcement[i]) : '?';
        }
        IntReport("  Guild announcement: '%s'", annBuf);
    } else {
        IntReport("  Guild announcement: (none)");
    }
    IntCheck("Guild announcement query ran without crash", true);

    IntReport("");
    return true;
}
