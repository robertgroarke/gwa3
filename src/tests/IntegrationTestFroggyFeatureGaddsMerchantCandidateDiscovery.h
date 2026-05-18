static size_t CollectGaddsMerchantCandidates(NpcCandidate* merchantCandidates, size_t maxMerchantCandidates) {
    DumpMerchantNpcCandidates(
        kGaddsMerchantX,
        kGaddsMerchantY,
        kGaddsMerchantCandidateRange,
        kGaddsMerchantPlayerNumber);

    size_t merchantCandidateCount = CollectMerchantNpcCandidates(
        kGaddsMerchantX,
        kGaddsMerchantY,
        kGaddsMerchantCandidateRange,
        kGaddsMerchantPlayerNumber,
        merchantCandidates,
        maxMerchantCandidates);
    if (merchantCandidateCount) {
        return merchantCandidateCount;
    }

    FroggyFeatureReport("  No merchant candidates at 1500 range, walking closer...");
    MovePlayerNearMerchantIsolation(kGaddsMerchantX, kGaddsMerchantY, 200.0f, 10000);
    Sleep(500);
    DumpMerchantNpcCandidates(
        kGaddsMerchantX,
        kGaddsMerchantY,
        kGaddsMerchantCandidateRange,
        kGaddsMerchantPlayerNumber);
    return CollectMerchantNpcCandidates(
        kGaddsMerchantX,
        kGaddsMerchantY,
        kGaddsMerchantCandidateRange,
        kGaddsMerchantPlayerNumber,
        merchantCandidates,
        maxMerchantCandidates);
}
