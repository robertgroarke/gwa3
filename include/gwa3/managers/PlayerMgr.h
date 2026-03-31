#pragma once

// GWA3-049: PlayerMgr — title tracking, player data, profession changes.

#include <gwa3/game/Player.h>
#include <gwa3/game/Title.h>
#include <cstdint>

namespace GWA3::PlayerMgr {

    bool Initialize();

    // ===== Title Management =====

    // Display a title for reputation gain
    bool SetActiveTitle(uint32_t titleId);

    // Hide the currently displayed title
    bool RemoveActiveTitle();

    // Get title progress data (null if title has no progress)
    Title* GetTitleTrack(uint32_t titleId);

    // Get title client data (flags, name)
    TitleClientData* GetTitleData(uint32_t titleId);

    // Which title is currently displayed
    uint32_t GetActiveTitleId();

    // Get the active title's progress struct
    Title* GetActiveTitle();

    // ===== Player Data =====

    // Get player struct by player_id (0 = self)
    Player* GetPlayerByID(uint32_t playerId = 0);

    // Get player name (0 = self)
    wchar_t* GetPlayerName(uint32_t playerId = 0);

    // Get player by name
    Player* GetPlayerByName(const wchar_t* name);

    // Get the player array (all players in the instance)
    GWArray<Player>* GetPlayerArray();

    // Number of the currently logged-in character
    uint32_t GetPlayerNumber();

    // Agent ID of a player by player number
    uint32_t GetPlayerAgentId(uint32_t playerId);

    // Player count in instance
    uint32_t GetAmountOfPlayersInInstance();

    // ===== Profession =====

    // Change secondary profession (heroIndex=0 for self)
    bool ChangeSecondProfession(uint32_t profession, uint32_t heroIndex = 0);

} // namespace GWA3::PlayerMgr
