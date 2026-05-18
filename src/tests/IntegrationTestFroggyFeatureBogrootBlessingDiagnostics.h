static void ReportEotNTitleTracksForBlessing() {
    static const struct { uint32_t id; const char* name; } kTitles[] = {
        {TitleID::Asura, "Asura"},
        {TitleID::Norn, "Norn"},
        {TitleID::Deldrimor, "Deldrimor"},
        {TitleID::Vanguard, "Vanguard"},
    };
    for (const auto& t : kTitles) {
        Title* track = PlayerMgr::GetTitleTrack(t.id);
        if (track) {
            FroggyFeatureReport("  Title %s (id=%u): points=%u tier=%u/%u needed_next=%u maxRank=%u",
                      t.name, t.id, track->current_points, track->current_title_tier_index,
                      track->max_title_tier_index, track->points_needed_next_rank, track->max_title_rank);
        } else {
            FroggyFeatureReport("  Title %s (id=%u): NOT FOUND", t.name, t.id);
        }
    }
}

static void ReportPlayerEffectsForBlessing(const char* label, uint32_t myId) {
    auto* playerEffects = EffectMgr::GetPlayerEffects();
    if (playerEffects && myId > 0) {
        auto* effectArr = EffectMgr::GetAgentEffectArray(myId);
        uint32_t effectCount = effectArr ? effectArr->size : 0;
        FroggyFeatureReport("  Effects %s interaction: agent=%u count=%u", label, playerEffects->agent_id, effectCount);
        if (effectArr) {
            for (uint32_t i = 0; i < effectCount && i < 10; i++) {
                Effect& e = effectArr->buffer[i];
                FroggyFeatureReport("    effect[%u]: skill=%u attr=%u duration=%.1f agent=%u",
                          i, e.skill_id, e.attribute_level, e.duration, e.agent_id);
            }
        }
    } else {
        FroggyFeatureReport("  Effects %s: playerEffects=%p myId=%u", label, playerEffects, myId);
    }
}

static void ReportBlessingSkillEffects(uint32_t myId) {
    FroggyFeatureReport("  HasEffect check: Dwarven(%u)=%d Asuran(%u)=%d Norn(%u)=%d Vanguard(%u)=%d",
              SkillIds::GREAT_DWARFS_BLESSING,
              myId ? EffectMgr::HasEffect(myId, SkillIds::GREAT_DWARFS_BLESSING) : -1,
              SkillIds::ASURAN_BODYGUARD_ID_2481,
              myId ? EffectMgr::HasEffect(myId, SkillIds::ASURAN_BODYGUARD_ID_2481) : -1,
              SkillIds::NORN_HUNTING_PARTY,
              myId ? EffectMgr::HasEffect(myId, SkillIds::NORN_HUNTING_PARTY) : -1,
              SkillIds::VANGUARD_PATROL,
              myId ? EffectMgr::HasEffect(myId, SkillIds::VANGUARD_PATROL) : -1);
}

static void ReportBogrootBlessingPreInteractionDiagnostics(uint32_t myId) {
    FroggyFeatureReport("  MyID=%u MapID=%u", myId, MapMgr::GetMapId());
    ReportEotNTitleTracksForBlessing();
    ReportPlayerEffectsForBlessing("BEFORE", myId);
    ReportBlessingSkillEffects(myId);
}

static void ReportBogrootBlessingPostInteractionDiagnostics() {
    const uint32_t myId = AgentMgr::GetMyId();
    ReportPlayerEffectsForBlessing("AFTER", myId);
    ReportBlessingSkillEffects(myId);
}
