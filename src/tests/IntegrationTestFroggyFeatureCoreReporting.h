static void FroggyFeatureReport(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log::Info("[FROGGY-TEST] %s", buf);
}

static void FroggyFeatureCheck(const char* name, bool cond) {
    if (cond) {
        s_passed++;
        FroggyFeatureReport("  [PASS] %s", name);
    } else {
        s_failed++;
        FroggyFeatureReport("  [FAIL] %s", name);
    }
}

static void FroggyFeatureSkip(const char* name, const char* reason) {
    s_skipped++;
    FroggyFeatureReport("  [SKIP] %s - %s", name, reason);
}

static void SkipInvasiveCombatProofSuite(const char* reason) {
    FroggyFeatureSkip("Builtin combat single-step proof", reason);
    FroggyFeatureSkip("Combat target selection coverage", reason);
    FroggyFeatureSkip("Cast gating and safety assertions", reason);
    FroggyFeatureSkip("Spirit chain regression", reason);
}
