// Froggy dungeon blessing interaction flow. Included by FroggyHM.cpp
// while the shrine-specific behavior is separated from the route state machine.

// ===== Dungeon Blessing Grab =====
// Mirrors AutoIt BotsHub pattern: SetDisplayedTitle -> GoNearestNPCToCoords -> Dialog(0x84)
// Must disable DialogMgr StoC hooks during interaction - StringEncoding::DecodeStr
// times out in Bogroot dungeons and crashes the game via StoC callback corruption.

static void GrabDungeonBlessing(float shrineX, float shrineY) {
    if (DungeonEffects::HasAnyDungeonBlessing()) {
        Log::Info("Froggy: Blessing already active, skipping");
        return;
    }

    // Bogroot's shrine needs the Deldrimor title selected before dialog(0x84).
    const auto titleResult = DungeonEffects::EnsureActiveTitle(
        BLESSING_TITLE_ID,
        BLESSING_TITLE_SETTLE_MS,
        &WaitMs);
    if (titleResult.applied) {
        Log::Info("Froggy: Blessing setting Deldrimor title current=0x%X target=0x%X",
                  titleResult.previous_title_id,
                  BLESSING_TITLE_ID);
    }

    NearbyNpcCandidate candidates[8] = {};
    size_t candidateCount =
        CollectNearbyNpcCandidates(shrineX, shrineY, BLESSING_PRIMARY_SEARCH_RADIUS, candidates, _countof(candidates));
    float loggedRadius = BLESSING_PRIMARY_SEARCH_RADIUS;
    if (candidateCount == 0) {
        loggedRadius = BLESSING_FALLBACK_SEARCH_RADIUS;
        candidateCount = CollectNearbyNpcCandidates(shrineX, shrineY, loggedRadius, candidates, _countof(candidates));
    }
    LogNearbyNpcCandidates("Blessing", shrineX, shrineY, loggedRadius, candidates, candidateCount);
    LogNearbySignposts(shrineX, shrineY, loggedRadius, "Blessing signposts", false);

    DungeonInteractions::InteractCandidate interactCandidates[2] = {};
    const size_t interactCandidateCount = DungeonInteractions::CollectNearestInteractCandidates(
        shrineX,
        shrineY,
        BLESSING_PRIMARY_SEARCH_RADIUS,
        BLESSING_FALLBACK_SEARCH_RADIUS,
        interactCandidates,
        _countof(interactCandidates));

    if (interactCandidateCount == 0) {
        Log::Warn("Froggy: Blessing found no interactable NPC or signpost near shrine=(%.0f, %.0f)", shrineX, shrineY);
        return;
    }

    for (size_t i = 0; i < interactCandidateCount; ++i) {
        const auto& candidate = interactCandidates[i];
        Log::Info("Froggy: Blessing candidate[%u] kind=%s agent=%u distToShrine=%.0f pos=(%.0f, %.0f)",
                  static_cast<unsigned>(i),
                  candidate.use_signpost ? "signpost" : "npc",
                  candidate.agent_id,
                  candidate.dist_to_anchor,
                  candidate.x,
                  candidate.y);
        LogAgentIdentity(candidate.use_signpost ? "Blessing signpost" : "Blessing NPC", candidate.agent_id);
    }

    // AutoIt shape: GoToNPCNearXY -> Dialog(0x84). The important detail is that
    // the blessing send must happen under the shrine NPC's dialog context, not a
    // stale Tekks dialog that may still be latched from dungeon entry.
    for (size_t candidateIndex = 0;
         candidateIndex < interactCandidateCount && !DungeonEffects::HasAnyDungeonBlessing();
         ++candidateIndex) {
        const auto& candidate = interactCandidates[candidateIndex];
        const float moveThreshold =
            candidate.use_signpost ? BLESSING_SIGNPOST_MOVE_THRESHOLD : BLESSING_NPC_MOVE_THRESHOLD;
        MoveToAndWait(candidate.x, candidate.y, moveThreshold);
        WaitForLocalPositionSettle(BLESSING_SETTLE_TIMEOUT_MS, BLESSING_SETTLE_DISTANCE);

        DungeonInteractions::CandidateDialogOptions dialogOptions;
        dialogOptions.dialog_id = BLESSING_ACCEPT_DIALOG_ID;
        dialogOptions.candidate_index = candidateIndex;
        dialogOptions.log_prefix = "Froggy: Blessing";
        dialogOptions.wait_ms = &WaitMs;
        dialogOptions.stop_condition = &DungeonEffects::HasBlessing;
        (void)DungeonInteractions::InteractCandidateAndSendDialog(candidate, dialogOptions);
    }

    if (DungeonEffects::HasAnyDungeonBlessing()) {
        Log::Info("Froggy: Blessing confirmed active");
    } else {
        Log::Warn("Froggy: Blessing effect not detected after shrine interaction");
    }
}
