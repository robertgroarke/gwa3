static void RunLastCombatActionTargetSelectionCheck() {
    const auto info = Bot::Froggy::g_combatSession.last_action;
    if (!info.valid) {
        FroggyFeatureSkip("Combat target selection coverage", "No builtin combat step metadata available");
        return;
    }
    FroggyFeatureCheck("Combat target selection has last combat step info", true);

    if (info.target_type == 5 || info.auto_attack) {
        FroggyFeatureCheck("Chosen combat action resolved non-zero foe target", info.target_id != 0);
        FroggyFeatureCheck("Chosen combat action target is live foe", IsLiveEnemyAgent(info.target_id));
    } else {
        FroggyFeatureSkip("Combat target selection - chosen foe-target action",
                "Last builtin combat step did not use a foe-targeting action");
    }
}
