// Game-state smoke-test section. Included by SmokeTest.cpp inside GWA3::SmokeTest.

static void RunGameStateSmokeSection(const SmokeRawOffsetMatches& raw_matches) {
    Report("--- Game State (via post-processed Offsets::) ---");

    // Read MyID: Offsets::MyID is a pointer to the MyID storage
    if (Offsets::MyID > 0x10000) {
        uint32_t myId = *reinterpret_cast<uint32_t*>(Offsets::MyID);
        Report("Offsets::MyID -> *0x%08X = %u", Offsets::MyID, myId);
        Check("MyID via Offsets plausible (0-10000)", myId > 0 && myId <= 10000);
    } else {
        Report("Offsets::MyID = 0x%08X (too low)", Offsets::MyID);
    }

    // Also read via re-scan operand for comparison
    if (raw_matches.raw_my_id) {
        uintptr_t myIdAddr = *reinterpret_cast<uint32_t*>(raw_matches.raw_my_id + 8);
        if (myIdAddr > 0x10000) {
            uint32_t myId2 = *reinterpret_cast<uint32_t*>(myIdAddr);
            Report("Re-scan operand -> *0x%08X = %u", myIdAddr, myId2);
        }
    }

    // MapID: read via BasePointer pointer chain: *BasePointer -> +0x18 -> +0x44 -> +0x198
    if (Offsets::BasePointer > 0x10000) {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx > 0x10000) {
            uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
            if (p1 > 0x10000) {
                uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x44);
                if (p2 > 0x10000) {
                    int32_t mapId = *reinterpret_cast<int32_t*>(p2 + 0x198);
                    Report("MapID via BasePointer chain: [0x%08X]+18->[0x%08X]+44->[0x%08X]+198 = %d",
                           ctx, p1, p2, mapId);
                    Check("MapID plausible (0-2000)", mapId > 0 && mapId <= 2000);
                } else { Report("MapID chain: p2 null"); }
            } else { Report("MapID chain: p1 null"); }
        } else { Report("MapID chain: ctx null"); }
    }

    // Ping: post-processed
    if (Offsets::Ping > 0x10000) {
        uint32_t ping = *reinterpret_cast<uint32_t*>(Offsets::Ping);
        Report("Offsets::Ping -> *0x%08X = %u ms", Offsets::Ping, ping);
        Check("Ping via Offsets plausible (0-5000)", ping <= 5000);
    }

    // BasePointer -> game context
    if (Offsets::BasePointer > 0x10000) {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        Report("Offsets::BasePointer -> *0x%08X = 0x%08X", Offsets::BasePointer, ctx);
        Check("BasePointer deref valid", ctx > 0x10000);
    }

    // AgentBase
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        Report("Offsets::AgentBase -> *0x%08X = 0x%08X", Offsets::AgentBase, agentArr);
        Check("AgentBase deref valid", agentArr > 0x10000);
    }

    // SkillBase
    if (Offsets::SkillBase > 0x10000) {
        uintptr_t skillArr = *reinterpret_cast<uintptr_t*>(Offsets::SkillBase);
        Report("Offsets::SkillBase -> *0x%08X = 0x%08X", Offsets::SkillBase, skillArr);
        Check("SkillBase deref valid", skillArr > 0x10000);
    }
    Report("");
}
