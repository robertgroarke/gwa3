static void ResetMerchantNpcCandidateBuffer(NpcCandidate* out, size_t maxOut) {
    for (size_t i = 0; i < maxOut; ++i) {
        out[i] = {};
        out[i].score = 0xFFFFFFFFu;
    }
}

static bool InsertRankedMerchantNpcCandidate(const NpcCandidate& candidate, NpcCandidate* out, size_t maxOut) {
    size_t insertAt = maxOut;
    for (size_t slot = 0; slot < maxOut; ++slot) {
        if (candidate.score < out[slot].score) {
            insertAt = slot;
            break;
        }
    }
    if (insertAt == maxOut) return false;

    for (size_t slot = maxOut - 1; slot > insertAt; --slot) {
        out[slot] = out[slot - 1];
    }
    out[insertAt] = candidate;
    return true;
}
