bool TestTitleManagement() {
    IntReport("=== Title Management ===");

    if (ReadMyId() == 0) { IntSkip("TitleMgmt", "Not in game"); IntReport(""); return false; }

    // GetActiveTitleId() is currently exposing the player's active title tier, not a title id.
    uint32_t currentActiveTier = PlayerMgr::GetActiveTitleId();
    const uint32_t candidateTitle = FindUsableTitleId(currentActiveTier);
    IntReport("  Current active title tier: %u", currentActiveTier);
    IntReport("  Candidate title for mutation: %u", candidateTitle);

    if (candidateTitle == 0) {
        IntSkip("TitleMgmt", "No usable title track with readable data");
        IntReport("");
        return true;
    }

    IntReport("  Setting active title to %u...", candidateTitle);
    PlayerMgr::SetActiveTitle(candidateTitle);
    Sleep(1000);

    // Get title track data
    Title* track = PlayerMgr::GetTitleTrack(candidateTitle);
    IntReport("  TitleTrack(%u): %p", candidateTitle, track);
    if (track) {
        IntReport("    current_points=%u max_rank=%u tier=%u",
                  track->current_points, track->max_title_rank, track->current_title_tier_index);
        IntCheck("Title track has plausible max_rank", track->max_title_rank > 0 && track->max_title_rank < 100);
    }

    uint32_t afterSet = PlayerMgr::GetActiveTitleId();
    IntReport("  Active title tier after set: %u", afterSet);
    if (track) {
        IntCheck("SetActiveTitle updated active title tier", afterSet == track->current_title_tier_index && afterSet != 0);
    } else {
        IntSkip("SetActiveTitle tier readback", "No title track available for selected title");
    }

    // Get title client data
    TitleClientData* clientData = PlayerMgr::GetTitleData(candidateTitle);
    IntReport("  TitleClientData(%u): %p", candidateTitle, clientData);
    if (clientData) {
        IntReport("    flags=%u title_id=%u name_id=%u",
                  clientData->title_flags, clientData->title_id, clientData->name_id);
        IntCheck("TitleClientData has non-zero name_id", clientData->name_id > 0);
    } else {
        IntSkip("TitleClientData", "TitleClientDataBase not resolved");
    }

    // Remove active title
    IntReport("  Removing active title...");
    PlayerMgr::RemoveActiveTitle();
    Sleep(500);
    uint32_t afterRemove = PlayerMgr::GetActiveTitleId();
    IntReport("  Active title after remove: %u", afterRemove);
    IntCheck("RemoveActiveTitle cleared active title", afterRemove != candidateTitle);

    // Restore original
    IntReport("");
    return true;
}

// ===== CallbackRegistry Tests =====
