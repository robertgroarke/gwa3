#include "IntegrationTestFroggyFeatureCombatApproachState.h"
#include "IntegrationTestFroggyFeatureCombatApproachValidation.h"
#include "IntegrationTestFroggyFeatureCombatApproachMoveIssue.h"

static bool MoveNearFoeForCombat(uint32_t foeId, float desiredRange, DWORD timeoutMs) {
    const DWORD start = GetTickCount();
    CombatApproachState approach;

    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = GetAgentLivingRaw(AgentMgr::GetMyId());
        auto* foe = GetAgentLivingRaw(foeId);
        if (!me || !foe || foe->hp <= 0.0f) return false;

        const float dist = AgentMgr::GetDistance(me->x, me->y, foe->x, foe->y);
        if (dist <= desiredRange) {
            if (ValidateSettledCombatApproachRange(foeId, desiredRange)) return true;
        }

        const DWORD now = GetTickCount();
        if (ShouldIssueCombatApproachMove(approach, now) && GameThread::IsInitialized()) {
            if (!IssueCombatApproachMove(*me, *foe, desiredRange)) return true;
            approach.moveIssued = true;
            approach.lastIssue = now;
        }

        Sleep(500);
        UpdateCombatApproachPosition(approach);
    }

    return false;
}
