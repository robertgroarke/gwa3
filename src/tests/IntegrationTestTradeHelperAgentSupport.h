uintptr_t GetAgentPtrRaw(uint32_t agentId) {
    if (Offsets::AgentBase <= 0x10000 || agentId == 0 || agentId >= 5000) return 0;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        if (agentArr <= 0x10000) return 0;
        return *reinterpret_cast<uintptr_t*>(agentArr + agentId * 4);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
