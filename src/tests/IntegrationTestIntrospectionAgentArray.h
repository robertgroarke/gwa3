bool TestAgentArrayEnumeration() {
    IntReport("=== Agent Array Enumeration ===");

    if (ReadMyId() == 0 || Offsets::AgentBase <= 0x10000) {
        IntSkip("Agent enumeration", "Not in game or AgentBase unresolved");
        IntReport("");
        return false;
    }

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);

        IntReport("  AgentBase=0x%08X agentArr=0x%08X maxAgents=%u",
                  static_cast<unsigned>(Offsets::AgentBase),
                  static_cast<unsigned>(agentArr),
                  maxAgents);
        IntCheck("Agent array pointer valid", agentArr > 0x10000);
        IntCheck("Max agents plausible (1-8192)", maxAgents > 0 && maxAgents <= 8192);

        uint32_t livingCount = 0;
        uint32_t itemCount = 0;
        uint32_t gadgetCount = 0;
        uint32_t otherCount = 0;
        uint32_t allyCount = 0;
        uint32_t foeCount = 0;
        uint32_t npcCount = 0;
        bool foundSelf = false;

        const uint32_t myId = ReadMyId();

        for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
            uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
            if (agentPtr <= 0x10000) continue;

            auto* base = reinterpret_cast<Agent*>(agentPtr);

            if (base->type == 0xDB) {
                livingCount++;
                auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
                if (living->allegiance == 1) allyCount++;
                else if (living->allegiance == 3) foeCount++;
                else if (living->allegiance == 6) npcCount++;

                if (i == myId) {
                    foundSelf = true;
                    IntReport("  Self agent: id=%u type=0x%X hp=%.2f pos=(%.0f,%.0f) primary=%u level=%u",
                              i, base->type, living->hp, living->x, living->y,
                              living->primary, living->level);
                    IntCheck("Self HP > 0", living->hp > 0.0f);
                    IntCheck("Self primary profession valid (1-10)",
                             living->primary >= 1 && living->primary <= 10);
                    IntCheck("Self level plausible (1-20)",
                             living->level >= 1 && living->level <= 20);
                }
            } else if (base->type & 0x400) {
                itemCount++;
            } else if (base->type & 0x200) {
                gadgetCount++;
            } else {
                otherCount++;
            }
        }

        IntReport("  Agent census: living=%u (ally=%u foe=%u npc=%u) item=%u gadget=%u other=%u",
                  livingCount, allyCount, foeCount, npcCount, itemCount, gadgetCount, otherCount);
        IntCheck("Found self in agent array", foundSelf);
        IntCheck("At least 1 living agent", livingCount >= 1);

        Agent* myAgent = AgentMgr::GetMyAgent();
        IntReport("  GetMyAgent(): %p", myAgent);
        IntCheck("GetMyAgent returns non-null", myAgent != nullptr);
        if (myAgent) {
            IntCheck("GetMyAgent type is Living (0xDB)", myAgent->type == 0xDB);
        }

        Agent* byId = AgentMgr::GetAgentByID(myId);
        IntReport("  GetAgentByID(%u): %p", myId, byId);
        IntCheck("GetAgentByID matches GetMyAgent", byId == myAgent);

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        IntCheck("Agent enumeration did not fault", false);
        IntReport("");
        return false;
    }

    IntReport("");
    return true;
}
