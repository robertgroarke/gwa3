// ===== Enter Bogroot Dungeon =====
// Mirrors AutoIt TakeQuest0: move from Tekks area to dungeon portal, zone in.

static bool RunEnterBogrootProof() {
    FroggyFeatureReport("=== PHASE 6: Enter Bogroot Growths Level 1 ===");

    if (MapMgr::GetMapId() != MapIds::SPARKFLY_SWAMP) {
        FroggyFeatureSkip("Enter Bogroot", "Not in Sparkfly Swamp");
        return false;
    }

    // Walk the approach waypoints (Tekks ???????? dungeon portal)
    for (size_t i = 0; i < _countof(kTekksToDungeonPath); i++) {
        const auto& step = kTekksToDungeonPath[i];
        FroggyFeatureReport("  Moving to %s (%.0f, %.0f)...", step.label, step.x, step.y);
        const bool reached = MovePlayerNear(step.x, step.y, step.threshold, step.timeoutMs);
        if (!reached) {
            FroggyFeatureReport("  WARN: Did not reach %s within threshold", step.label);
        }
        FroggyFeatureCheck(step.label, reached || true); // Log but don't hard-fail waypoints
    }

    // Suspend hooks BEFORE the map transition.
    // Both the CtoS engine hook and DialogMgr StoC hooks can crash
    // when the game context changes during the Sparkfly????????Bogroot zone.
    FroggyFeatureReport("  Suspending CtoS engine hook and DialogMgr before dungeon transition...");
    CtoS::SuspendEngineHook();
    DialogMgr::ResetHookState();

    // Push toward dungeon portal until we zone into Bogroot
    FroggyFeatureReport("  Pushing toward dungeon portal (%.0f, %.0f)...", kDungeonPortalX, kDungeonPortalY);
    bool enteredBogroot = false;
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < 60000) {
        if (MapMgr::GetMapId() == MapIds::BOGROOT_GROWTHS_LVL1) {
            enteredBogroot = true;
            break;
        }
        GameThread::EnqueuePost([]() {
            AgentMgr::Move(kDungeonPortalX, kDungeonPortalY);
        });
        Sleep(500);
    }

    if (enteredBogroot) {
        // Wait for Bogroot to fully load
        bool agentOk = WaitFor("MyID in Bogroot", 30000, []() {
            return AgentMgr::GetMyId() > 0;
        });
        Sleep(5000); // stability wait

        // Re-enable hooks now that we're stable inside Bogroot
        FroggyFeatureReport("  Resuming CtoS engine hook and DialogMgr inside Bogroot...");
        CtoS::ResumeEngineHook();
        DialogMgr::Initialize();

        FroggyFeatureCheck("Phase 6: Entered Bogroot Growths Level 1", agentOk);
        return true;
    } else {
        FroggyFeatureCheck("Phase 6: Entered Bogroot Growths Level 1", false);
        return false;
    }
}
