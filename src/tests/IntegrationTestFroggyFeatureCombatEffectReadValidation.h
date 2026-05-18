static void RunCombatEffectReadValidation(uint32_t foeId, const char* label) {
    FroggyFeatureReport("=== PHASE 5D: Combat Effect Read Validation (%s) ===", label ? label : "default");

    const uint32_t myId = AgentMgr::GetMyId();
    FroggyFeatureCheck("Combat effect read bogus player effect is false", !EffectMgr::HasEffect(myId, 9999));
    FroggyFeatureCheck("Combat effect read bogus foe effect is false", !EffectMgr::HasEffect(foeId, 9999));
    FroggyFeatureCheck("Combat effect read bogus player buff is false", !EffectMgr::HasBuff(myId, 9999));
    FroggyFeatureCheck("Combat effect read bogus foe buff is false", !EffectMgr::HasBuff(foeId, 9999));

    auto* partyEffects = EffectMgr::GetPartyEffectsArray();
    if (!partyEffects || !partyEffects->buffer || partyEffects->size == 0) {
        FroggyFeatureSkip("Combat effect array validation", "Party effect array empty in current encounter");
        return;
    }

    FroggyFeatureCheck("Combat effect array size plausible", partyEffects->size <= 64);
    auto* playerEffects = EffectMgr::GetAgentEffects(myId);
    if (playerEffects) {
        FroggyFeatureCheck("Combat player effects agent id matches self", playerEffects->agent_id == myId);
        auto* playerEffectArray = EffectMgr::GetAgentEffectArray(myId);
        if (playerEffectArray) {
            FroggyFeatureCheck("Combat player effect array size plausible", playerEffectArray->size <= 500);
            if (playerEffectArray->size > 0) {
                const uint32_t effectSkill = playerEffectArray->buffer[0].skill_id;
                FroggyFeatureCheck("Combat player effect lookup round-trips first effect",
                         EffectMgr::GetEffectBySkillId(myId, effectSkill) != nullptr &&
                         EffectMgr::HasEffect(myId, effectSkill));
            }
        }

        auto* playerBuffArray = EffectMgr::GetAgentBuffArray(myId);
        if (playerBuffArray) {
            FroggyFeatureCheck("Combat player buff array size plausible", playerBuffArray->size <= 500);
            if (playerBuffArray->size > 0) {
                const uint32_t buffSkill = playerBuffArray->buffer[0].skill_id;
                FroggyFeatureCheck("Combat player buff lookup round-trips first buff",
                         EffectMgr::GetBuffBySkillId(myId, buffSkill) != nullptr &&
                         EffectMgr::HasBuff(myId, buffSkill));
            }
        }
    } else {
        FroggyFeatureSkip("Combat player effect lookup", "Player not present in current party effect array");
    }
}
