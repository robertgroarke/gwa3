static bool ValidateSettledCombatApproachRange(uint32_t foeId, float desiredRange) {
    WaitForPlayerPositionSettle(1200, 20.0f);
    WaitFor("Botshub queue idle after combat approach", 1200, []() {
        return CtoS::IsBotshubQueueIdle();
    });

    auto* me = GetAgentLivingRaw(AgentMgr::GetMyId());
    auto* foe = GetAgentLivingRaw(foeId);
    if (!me || !foe || foe->hp <= 0.0f) return false;
    const float settledDist = AgentMgr::GetDistance(me->x, me->y, foe->x, foe->y);
    return settledDist <= desiredRange;
}
