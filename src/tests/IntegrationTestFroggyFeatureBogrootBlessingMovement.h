static void MoveToBogrootBlessingShrine() {
    for (size_t i = 0; i < _countof(kBogrootToBlessingPath); i++) {
        const auto& step = kBogrootToBlessingPath[i];
        FroggyFeatureReport("  Moving to %s (%.0f, %.0f)...", step.label, step.x, step.y);
        MovePlayerNear(step.x, step.y, step.threshold, step.timeoutMs);
    }
}
