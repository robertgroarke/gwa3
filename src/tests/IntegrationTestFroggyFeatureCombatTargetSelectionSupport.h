static constexpr uint32_t TEST_ROLE_HEX = (1u << 8);
static constexpr uint32_t TEST_ROLE_PRESSURE = (1u << 9);
static constexpr uint32_t TEST_ROLE_ATTACK = (1u << 10);
static constexpr uint32_t TEST_ROLE_INTERRUPT_HARD = (1u << 11);
static constexpr uint32_t TEST_ROLE_INTERRUPT_SOFT = (1u << 12);
static constexpr uint32_t TEST_ROLE_ENCHANT_REMOVE = (1u << 7);

static bool IsLiveEnemyAgent(uint32_t agentId) {
    auto* a = AgentMgr::GetAgentByID(agentId);
    if (!a || a->type != 0xDB) return false;
    auto* living = static_cast<AgentLiving*>(a);
    return living->allegiance == 3 && living->hp > 0.0f;
}
