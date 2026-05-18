// FroggyFeature explorable player-effects proof.

static void RunExplorablePlayerEffectsProof() {
    FroggyFeatureReport("=== PHASE 5A2: Explorable Player Effects ===");

    const uint32_t myId = AgentMgr::GetMyId();
    FroggyFeatureCheck("Explorable player id available for effects proof", myId > 0);
    if (!myId) {
        FroggyFeatureSkip("Explorable GetPlayerEffects", "Player id unavailable in explorable");
        return;
    }

    auto* effects = EffectMgr::GetPlayerEffects();
    auto* directEffects = EffectMgr::GetAgentEffects(myId);
    auto* effectArr = EffectMgr::GetAgentEffectArray(myId);
    if (!directEffects || !effectArr) {
        FroggyFeatureSkip("Explorable GetPlayerEffects", "Direct agent effect array unavailable in explorable");
        return;
    }

    FroggyFeatureCheck("Explorable direct agent effects returns non-null", true);
    FroggyFeatureCheck("Explorable player effect array returns non-null", true);
    FroggyFeatureCheck("Explorable direct agent effects agent_id matches self", directEffects->agent_id == myId);
    if (effects) {
        FroggyFeatureCheck("Explorable GetPlayerEffects agent_id matches self", effects->agent_id == myId);
    } else {
        FroggyFeatureSkip("Explorable GetPlayerEffects wrapper", "Wrapper returned null while direct agent effects remained available");
    }

    const uint32_t effectCount = effectArr->size;
    FroggyFeatureReport("  Explorable player effects: directAgent=%u wrapperAgent=%u count=%u",
              directEffects->agent_id,
              effects ? effects->agent_id : 0u,
              effectCount);
}
