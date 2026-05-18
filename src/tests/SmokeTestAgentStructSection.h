// Agent-struct smoke-test section. Included by SmokeTest.cpp inside GWA3::SmokeTest.

static void RunAgentStructSmokeSection() {
    Report("--- Agent Struct Read ---");
    // AutoIt chain: [[*AgentBase] + 4*id]
    // *AgentBase = ptr to agent ptr array (NOT GWArray)
    if (Offsets::AgentBase > 0x10000) {
        uintptr_t agentPtrArray = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        Report("AgentBase deref: *0x%08X = 0x%08X (agent ptr array)", Offsets::AgentBase, agentPtrArray);

        // Dump first few entries to understand layout
        if (agentPtrArray > 0x10000) {
            Report("  [0]=0x%08X [1]=0x%08X [2]=0x%08X [3]=0x%08X",
                *reinterpret_cast<uint32_t*>(agentPtrArray),
                *reinterpret_cast<uint32_t*>(agentPtrArray + 4),
                *reinterpret_cast<uint32_t*>(agentPtrArray + 8),
                *reinterpret_cast<uint32_t*>(agentPtrArray + 12));

            // MaxAgents is at AgentBase + 8 (from AutoIt: $max_agents = $agent_base_address + 0x8)
            uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
            Report("  MaxAgents (*AgentBase+8): %u", maxAgents);

            uint32_t myId = (Offsets::MyID > 0x10000) ? *reinterpret_cast<uint32_t*>(Offsets::MyID) : 0;
            Report("  MyID = %u", myId);

            if (myId > 0 && myId < 5000 && agentPtrArray > 0x10000) {
                // Agent ptr at array[myId]
                uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentPtrArray + myId * 4);
                Report("  Agent[%u] ptr = 0x%08X", myId, agentPtr);
                if (agentPtr > 0x10000) {
                    float x = *reinterpret_cast<float*>(agentPtr + 0x74);
                    float y = *reinterpret_cast<float*>(agentPtr + 0x78);
                    float hp = *reinterpret_cast<float*>(agentPtr + 0x134);
                    uint32_t agentId = *reinterpret_cast<uint32_t*>(agentPtr + 0x2C);
                    uint32_t type = *reinterpret_cast<uint32_t*>(agentPtr + 0x9C);
                    Report("  agent_id=%u type=0x%X pos=(%.1f, %.1f) hp=%.2f",
                           agentId, type, x, y, hp);
                    Check("Agent X non-zero", x != 0.0f);
                    Check("Agent Y non-zero", y != 0.0f);
                    Check("Agent HP > 0", hp > 0.0f);
                    Check("Agent type is Living (0xDB)", type == 0xDB);
                } else {
                    Report("  Agent ptr null for id %u", myId);
                }
            }
        }
    }
    Report("");
}
