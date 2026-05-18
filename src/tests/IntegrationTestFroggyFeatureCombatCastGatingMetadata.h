static bool ValidateCombatCastGatingStepInfo(const Bot::Froggy::LastCombatStepInfo& info) {
    if (!info.valid) {
        FroggyFeatureSkip("Cast gating and safety assertions", "No builtin combat step metadata available");
        return false;
    }
    FroggyFeatureCheck("Cast gating has last combat step info", true);
    if (!info.used_skill) {
        FroggyFeatureSkip("Cast gating and safety assertions", "Last builtin combat step was not a skill cast");
        return false;
    }

    FroggyFeatureCheck("Chosen combat skill slot is in range", info.slot >= 1 && info.slot <= 8);
    return info.slot >= 1 && info.slot <= 8;
}
