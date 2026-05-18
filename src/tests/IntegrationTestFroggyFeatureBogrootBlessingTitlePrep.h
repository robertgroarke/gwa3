static bool EnsureDeldrimorTitleForBogrootBlessing() {
    const uint32_t activeTitleBefore = PlayerMgr::GetActiveTitleId();
    FroggyFeatureReport("  Active title before: %u", activeTitleBefore);
    if (activeTitleBefore == 0) {
        PlayerMgr::SetActiveTitle(TITLE_DISPLAY_DELDRIMOR);
        Sleep(1000);
        const uint32_t activeTitleAfter = PlayerMgr::GetActiveTitleId();
        FroggyFeatureReport("  Active title after SetActiveTitle(%u): %u", TITLE_DISPLAY_DELDRIMOR, activeTitleAfter);
        FroggyFeatureCheck("Deldrimor title set for blessing", activeTitleAfter != 0);
        return activeTitleAfter != 0;
    }

    FroggyFeatureReport("  Title already active (%u), keeping current display", activeTitleBefore);
    FroggyFeatureCheck("Title already set for blessing", true);
    return true;
}
