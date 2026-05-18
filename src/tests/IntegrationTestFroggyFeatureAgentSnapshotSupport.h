static bool TrySnapshotLivingAgent(uint32_t agentId, LivingAgentSnapshot& out) {
    out = {};
    auto* a = AgentMgr::GetAgentByID(agentId);
    if (!a) return false;
    __try {
        auto* living = static_cast<AgentLiving*>(a);
        out.agentId = living->agent_id;
        out.type = living->type;
        out.allegiance = living->allegiance;
        out.hp = living->hp;
        out.effects = living->effects;
        out.playerNumber = living->player_number;
        out.npcId = living->transmog_npc_id;
        out.x = living->x;
        out.y = living->y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
