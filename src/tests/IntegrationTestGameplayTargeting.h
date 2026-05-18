bool TestTargeting() {
    IntReport("===  continued: Targeting ===");

    uint32_t myId = ReadMyId();
    if (myId == 0) {
        IntSkip("Targeting", "Not in game");
        return false;
    }

    if (!WaitForPlayerWorldReady(10000)) {
        IntReport("  Player runtime state: TypeMap=0x%X ModelState=%u", GetPlayerTypeMap(), GetPlayerModelState());
        IntSkip("Targeting", "Player world state not ready");
        return false;
    }

    uint32_t targetId = 0;
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
        if (agentArr > 0x10000) {
            for (uint32_t i = 1; i < maxAgents && i < 200; i++) {
                if (i == myId) continue;
                uintptr_t ap = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
                if (ap > 0x10000) {
                    targetId = i;
                    break;
                }
            }
        }
    }

    if (targetId == 0) {
        IntSkip("Targeting", "No other agents found");
        return false;
    }

    IntReport("  Targeting agent %u via ChangeTarget...", targetId);
    const uint32_t targetBefore = AgentMgr::GetTargetId();
    IntReport("  CurrentTarget before change: %u", targetBefore);
    if (targetBefore == 0) {
        DumpCurrentTargetCandidates("before", targetId);
    }
    GameThread::Enqueue([targetId]() {
        AgentMgr::ChangeTarget(targetId);
    });

    const bool targetChanged = WaitFor("CurrentTarget updates after ChangeTarget", 5000, [targetId]() {
        return AgentMgr::GetTargetId() == targetId;
    });
    const uint32_t targetAfter = AgentMgr::GetTargetId();
    IntReport("  CurrentTarget after change: %u", targetAfter);
    if (targetAfter == 0) {
        DumpCurrentTargetCandidates("after", targetId);
    }
    IntCheck("CurrentTarget matches requested target", targetChanged);

    IntReport("");
    return targetChanged;
}

// Skill activation
