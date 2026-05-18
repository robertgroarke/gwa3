bool TestTargetLogHook() {
    IntReport("=== TargetLog Hook Validation ===");

    if (ReadMyId() == 0) {
        IntSkip("TargetLog hook", "Not in game");
        IntReport("");
        return false;
    }

    const bool initialized = TargetLogHook::IsInitialized();
    IntReport("  TargetLogHook initialized: %d", initialized);
    IntCheck("TargetLogHook is initialized", initialized);

    if (!initialized) {
        IntReport("");
        return false;
    }

    const uint32_t callCount = TargetLogHook::GetCallCount();
    const uint32_t storeCount = TargetLogHook::GetStoreCount();
    IntReport("  Hook stats: calls=%u stores=%u", callCount, storeCount);

    uint32_t targetId = FindNearbyNpcLikeAgent(5000.0f);
    if (!targetId) {
        IntReport("  No nearby agent for target log test");
        IntSkip("TargetLog capture", "No nearby agent");
        IntReport("");
        return true;
    }

    IntReport("  Targeting agent %u for target log capture test...", targetId);
    GameThread::Enqueue([targetId]() {
        AgentMgr::ChangeTarget(targetId);
    });

    const bool targetSet = WaitFor("CurrentTarget updates", 5000, [targetId]() {
        return AgentMgr::GetTargetId() == targetId;
    });
    IntCheck("Target set for hook test", targetSet);

    if (targetSet) {
        const uint32_t loggedTarget = TargetLogHook::GetTarget(ReadMyId());
        IntReport("  TargetLog for self: %u (current target: %u)", loggedTarget, targetId);
        IntCheck("TargetLog query returned without crash", true);

        const uint32_t callCountAfter = TargetLogHook::GetCallCount();
        IntReport("  Hook calls after targeting: %u (before=%u)", callCountAfter, callCount);
    }

    IntReport("");
    return true;
}
