static bool EnsureGaddsMerchantOpen(uint32_t* outOpenedMerchantId = nullptr) {
    FroggyFeatureReport("  Moving to merchant (%.0f, %.0f)...", kGaddsMerchantX, kGaddsMerchantY);
    const bool reachedMerchantArea = MovePlayerNearMerchantIsolation(kGaddsMerchantX, kGaddsMerchantY, 550.0f, 15000);
    FroggyFeatureReport("  Initial merchant-area approach reached=%d", reachedMerchantArea ? 1 : 0);
    Sleep(500);

    NpcCandidate merchantCandidates[8];
    const size_t merchantCandidateCount =
        CollectGaddsMerchantCandidates(merchantCandidates, _countof(merchantCandidates));
    if (!merchantCandidateCount) {
        return false;
    }

    bool merchantOpen = false;
    bool reachedAnyNpc = false;
    uint32_t openedMerchantId = 0;
    for (size_t candidateIndex = 0; candidateIndex < merchantCandidateCount && !merchantOpen; ++candidateIndex) {
        const GaddsMerchantOpenAttemptResult result =
            TryOpenGaddsMerchantCandidate(merchantCandidates[candidateIndex], candidateIndex, merchantCandidateCount);
        reachedAnyNpc = reachedAnyNpc || result.reachedNpc;
        merchantOpen = result.merchantOpen;
        if (merchantOpen) {
            openedMerchantId = result.openedMerchantId;
        }
    }

    FroggyFeatureCheck("Reached merchant area", reachedMerchantArea || reachedAnyNpc);
    FroggyFeatureCheck("Merchant window opened", merchantOpen);
    if (merchantOpen) {
        FroggyFeatureReport("  Merchant opened via candidate agent=%u", openedMerchantId);
        ReportMerchantStock();
        if (outOpenedMerchantId) *outOpenedMerchantId = openedMerchantId;
    }
    return merchantOpen;
}
