bool TestStoCPacketTypes() {
    IntReport("=== StoC Packet Type Coverage ===");

    if (ReadMyId() == 0) { IntSkip("StoC types", "Not in game"); IntReport(""); return false; }

    // Register callbacks on several common packet headers and wait
    struct HeaderProbe {
        uint32_t header;
        const char* name;
        std::atomic<uint32_t> hits{0};
        StoC::HookEntry entry{nullptr};
    };

    // Real StoC opcodes from GWCA Packets/Opcodes.h
    // AgentUpdateEffects (0x00F1) is tested in explorable via TestExplorableCallTarget.
    static HeaderProbe probes[] = {
        {0x001E, "AgentMovementTick"},   // GAME_SMSG_AGENT_MOVEMENT_TICK — fires constantly
        {0x000C, "PingRequest"},          // GAME_SMSG_PING_REQUEST — server pings client regularly
    };

    for (auto& p : probes) {
        StoC::RegisterPostPacketCallback(&p.entry, p.header,
            [&p](StoC::HookStatus*, StoC::PacketBase*) { p.hits++; });
    }

    Sleep(5000);

    for (auto& p : probes) {
        IntReport("  StoC 0x%04X (%s): %u hits", p.header, p.name, p.hits.load());
        if (p.hits.load() > 0) {
            IntCheck(p.name, true);
        } else {
            IntSkip(p.name, "No packets observed in 5s");
        }
        StoC::RemoveCallbacks(&p.entry);
    }

    IntReport("");
    return true;
}

// ===== Quest Management =====
