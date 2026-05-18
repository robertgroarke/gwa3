bool TestEffectArray() {
    IntReport("=== Effect/Buff Array ===");

    if (ReadMyId() == 0) {
        IntSkip("Effect array", "Not in game");
        IntReport("");
        return false;
    }

    GWArray<AgentEffects>* partyEffects = EffectMgr::GetPartyEffectsArray();
    IntReport("  PartyEffectsArray: %p (size=%u)",
              partyEffects, partyEffects ? partyEffects->size : 0);

    if (!partyEffects || partyEffects->size == 0) {
        IntSkip("Effect array contents", "Party effects array empty (may be in outpost with no effects)");
        IntCheck("EffectMgr queries ran without crash", true);
        IntReport("");
        return true;
    }

    IntCheck("Party effects array has entries", partyEffects->size > 0);

    AgentEffects* playerAE = EffectMgr::GetPlayerEffects();
    IntReport("  Player AgentEffects: %p", playerAE);

    if (playerAE) {
        IntReport("  Player agent_id=%u effects=%u buffs=%u",
                  playerAE->agent_id,
                  playerAE->effects.size,
                  playerAE->buffs.size);
        IntCheck("Player agent_id matches MyID", playerAE->agent_id == ReadMyId());

        for (uint32_t i = 0; i < playerAE->effects.size && i < 5; ++i) {
            const Effect& eff = playerAE->effects.buffer[i];
            IntReport("    Effect[%u]: skill=%u attr=%u id=%u agent=%u duration=%.1f timestamp=%u",
                      i, eff.skill_id, eff.attribute_level, eff.effect_id,
                      eff.agent_id, eff.duration, eff.timestamp);
        }

        for (uint32_t i = 0; i < playerAE->buffs.size && i < 5; ++i) {
            const Buff& buff = playerAE->buffs.buffer[i];
            IntReport("    Buff[%u]: skill=%u id=%u target=%u",
                      i, buff.skill_id, buff.buff_id, buff.target_agent_id);
        }
    } else {
        IntSkip("Player effects detail", "Player not in party effects array");
    }

    IntReport("  Party effect agents:");
    for (uint32_t i = 0; i < partyEffects->size && i < 8; ++i) {
        const AgentEffects& ae = partyEffects->buffer[i];
        IntReport("    [%u] agent=%u effects=%u buffs=%u",
                  i, ae.agent_id, ae.effects.size, ae.buffs.size);
    }

    const bool hasTestEffect = EffectMgr::HasEffect(ReadMyId(), 9999);
    IntReport("  HasEffect(myId, 9999) = %d (expected false)", hasTestEffect);
    IntCheck("HasEffect returns false for bogus skill", !hasTestEffect);

    const bool hasTestBuff = EffectMgr::HasBuff(ReadMyId(), 9999);
    IntReport("  HasBuff(myId, 9999) = %d (expected false)", hasTestBuff);
    IntCheck("HasBuff returns false for bogus skill", !hasTestBuff);

    const float remaining = EffectMgr::GetEffectTimeRemaining(ReadMyId(), 9999);
    IntReport("  GetEffectTimeRemaining(myId, 9999) = %.1f (expected 0)", remaining);
    IntCheck("Time remaining is 0 for non-existent effect", remaining == 0.0f);

    IntReport("");
    return true;
}
