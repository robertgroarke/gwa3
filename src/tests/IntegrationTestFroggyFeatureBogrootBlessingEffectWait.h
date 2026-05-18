static bool WaitForBogrootBlessingEffect() {
    bool blessed = HasAnyBlessing();
    if (!blessed) {
        for (int attempt = 0; attempt < 6; ++attempt) {
            Sleep(500);
            if (HasAnyBlessing()) {
                blessed = true;
                break;
            }
        }
    }
    return blessed;
}
