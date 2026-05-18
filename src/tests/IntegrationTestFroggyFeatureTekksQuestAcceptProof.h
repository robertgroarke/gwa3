#include "IntegrationTestFroggyFeatureTekksQuestApproachSupport.h"
#include "IntegrationTestFroggyFeatureTekksQuestAcceptAssertions.h"

static bool RunTekksQuestAcceptProof() {
    FroggyFeatureReport("=== PHASE 5M: Tekks Quest Accept Handling ===");

    ReportQuestSnapshot("Quest state before Tekks interact");
    const uint32_t activeBefore = QuestMgr::GetActiveQuestId();
    const uint32_t questLogBefore = QuestMgr::GetQuestLogSize();
    Quest* questBefore = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR);

    float npcX = 0.0f;
    float npcY = 0.0f;
    if (!ApproachTekksQuestNpc(npcX, npcY)) {
        FroggyFeatureSkip("Tekks quest accept", "Could not resolve Tekks NPC");
        return false;
    }

    FroggyFeatureReport("  Clearing local aggro before Tekks interaction...");
    const bool localAggroCleared = Bot::Froggy::DebugClearAggroInPlace(1250.0f);
    FroggyFeatureReport("  Tekks local aggro clear result=%d targetAfterClear=%u",
              localAggroCleared ? 1 : 0,
              AgentMgr::GetTargetId());
    AgentMgr::CancelAction();
    const bool idleBeforeInteract = WaitForPlayerCombatIdle(3000, "Player combat idle before Tekks interact");
    FroggyFeatureReport("  Tekks idle before interact=%d target=%u",
              idleBeforeInteract ? 1 : 0,
              AgentMgr::GetTargetId());
    WaitForPlayerPositionSettle(1200, 15.0f);
    FroggyFeatureReport("  Delegating Tekks preparation to Froggy's real dungeon-entry path...");
    const bool prepared = Bot::Froggy::DebugPrepareTekksDungeonEntry();
    ReportDialogSnapshot("After Froggy Tekks preparation");
    ReportQuestSnapshot("Quest state after Froggy Tekks preparation");

    const uint32_t activeAfter = QuestMgr::GetActiveQuestId();
    Quest* questAfter = QuestMgr::GetQuestById(GWA3::QuestIds::TEKKS_WAR);
    const uint32_t questLogAfter = QuestMgr::GetQuestLogSize();

    CheckTekksQuestAcceptResult(
        prepared,
        activeBefore,
        activeAfter,
        questLogBefore,
        questLogAfter,
        questBefore,
        questAfter);

    return prepared;
}
