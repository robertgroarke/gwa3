static bool IssueCombatApproachMove(const AgentLiving& me, const AgentLiving& foe, float desiredRange) {
    const float dx = foe.x - me.x;
    const float dy = foe.y - me.y;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) return false;

    const float engageBuffer = desiredRange > 120.0f ? 120.0f : desiredRange * 0.5f;
    const float stepBack = desiredRange > engageBuffer ? desiredRange - engageBuffer : desiredRange;
    const float scale = (len - stepBack) / len;
    const float moveX = me.x + dx * scale;
    const float moveY = me.y + dy * scale;

    GameThread::EnqueuePost([moveX, moveY]() {
        AgentMgr::Move(moveX, moveY);
    });
    return true;
}
