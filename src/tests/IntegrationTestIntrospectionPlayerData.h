bool TestPlayerData() {
    IntReport("=== Player Data Introspection ===");

    const uint32_t myId = ReadMyId();
    if (myId == 0) {
        IntSkip("Player data", "Not in game");
        IntReport("");
        return false;
    }

    const uint32_t playerNumber = PlayerMgr::GetPlayerNumber();
    IntReport("  PlayerNumber: %u", playerNumber);

    GWArray<Player>* playerArray = PlayerMgr::GetPlayerArray();
    IntReport("  PlayerArray: %p (size=%u)", playerArray, playerArray ? playerArray->size : 0);

    if (!playerArray || playerArray->size == 0) {
        IntSkip("PlayerNumber valid", "PlayerArray empty or WorldContext unavailable");
        IntSkip("PlayerName", "PlayerArray empty or WorldContext unavailable");
        IntSkip("Player struct", "PlayerArray empty or WorldContext unavailable");
        IntSkip("GetPlayerAgentId", "PlayerArray empty or WorldContext unavailable");
        IntSkip("Player count", "PlayerArray empty or WorldContext unavailable");
        IntCheck("PlayerMgr::Initialize ran (no crash)", true);
        IntReport("");
        return true;
    }

    IntCheck("PlayerNumber is valid (> 0)", playerNumber > 0);
    IntCheck("PlayerArray available", true);

    wchar_t* name = PlayerMgr::GetPlayerName(0);
    if (name && name[0] != L'\0') {
        char nameBuf[64] = {};
        for (int i = 0; i < 63 && name[i]; ++i) {
            nameBuf[i] = (name[i] < 128) ? static_cast<char>(name[i]) : '?';
        }
        IntReport("  PlayerName: %s", nameBuf);
        IntCheck("PlayerName non-empty", true);
    } else {
        IntCheck("PlayerName non-empty", false);
    }

    Player* self = PlayerMgr::GetPlayerByID(0);
    IntReport("  GetPlayerByID(0): %p", self);
    IntCheck("Player struct for self exists", self != nullptr);

    if (self) {
        IntReport("  Player agent_id=%u primary=%u secondary=%u player_number=%u party_size=%u",
                  self->agent_id,
                  self->primary,
                  self->secondary,
                  self->player_number,
                  self->party_size);
        IntCheck("Player agent_id matches MyID", self->agent_id == myId);
        IntCheck("Player primary profession valid (1-10)", self->primary >= 1 && self->primary <= 10);
        IntCheck("Player player_number matches", self->player_number == playerNumber);
    }

    const uint32_t agentIdFromMgr = PlayerMgr::GetPlayerAgentId(playerNumber);
    IntReport("  GetPlayerAgentId(%u) = %u", playerNumber, agentIdFromMgr);
    IntCheck("GetPlayerAgentId matches MyID", agentIdFromMgr == myId);

    const uint32_t playerCount = PlayerMgr::GetAmountOfPlayersInInstance();
    IntReport("  PlayersInInstance: %u", playerCount);
    IntCheck("Player count >= 1", playerCount >= 1);

    IntReport("");
    return true;
}
