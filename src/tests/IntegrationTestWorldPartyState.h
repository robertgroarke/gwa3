bool TestPartyState() {
    IntReport("=== Party State Validation ===");

    if (ReadMyId() == 0) {
        IntSkip("Party state", "Not in game");
        IntReport("");
        return false;
    }

    const bool defeated = PartyMgr::GetIsPartyDefeated();
    IntReport("  IsPartyDefeated: %d", defeated);
    IntCheck("Party is not defeated", !defeated);

    IntReport("");
    return true;
}
