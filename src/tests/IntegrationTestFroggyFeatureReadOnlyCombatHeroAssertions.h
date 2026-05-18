static bool AreReadOnlyCombatPartyHeroesAlive(PartyInfo* playerParty) {
    bool heroesAlive = true;
    const uint32_t cap = playerParty->heroes.size < 8 ? playerParty->heroes.size : 8;
    for (uint32_t i = 0; i < cap; ++i) {
        const uint32_t heroId = playerParty->heroes.buffer[i].agent_id;
        auto* hero = GetAgentLivingRaw(heroId);
        if (!hero || hero->hp <= 0.0f) {
            heroesAlive = false;
            break;
        }
    }
    return heroesAlive;
}

static void AssertReadOnlyCombatHeroesAliveBeforeDwell(PartyInfo* playerParty) {
    if (playerParty && playerParty->heroes.buffer && playerParty->heroes.size > 0) {
        FroggyFeatureCheck("Combat preconditions heroes alive before dwell", AreReadOnlyCombatPartyHeroesAlive(playerParty));
    } else {
        FroggyFeatureSkip("Combat preconditions hero alive check", "Player party heroes unavailable");
    }
}

static void AssertReadOnlyCombatHeroesAliveAfterDwell(PartyInfo* playerParty) {
    if (playerParty && playerParty->heroes.buffer && playerParty->heroes.size > 0) {
        FroggyFeatureCheck("Combat preconditions heroes alive after dwell", AreReadOnlyCombatPartyHeroesAlive(playerParty));
    }
}
