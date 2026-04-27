// Froggy Tekks quest and dungeon-entry preparation. Included by FroggyHM.cpp
// while this interaction flow is progressively moved into shared quest helpers.

#include "FroggyHMTekksEntryDiagnostics.h"
#include "FroggyHMTekksEntryQuestFlow.h"

static bool PrepareTekksDungeonEntry() {
    const auto entryPlan = GetEntryBootstrapPlan();
    const auto& tekksAnchor = entryPlan.npc;

    Log::Info("Froggy: Preparing Tekks dungeon entry sequence");
    // ---- Find Tekks and move close ----
    MoveToAndWait(tekksAnchor.x, tekksAnchor.y, TEKKS_ANCHOR_MOVE_THRESHOLD);
    WaitMs(TEKKS_PRE_INTERACT_DWELL_MS);
    AgentMgr::CancelAction();
    WaitMs(TEKKS_CANCEL_DWELL_MS);

    uint32_t tekksId = DungeonInteractions::FindNearestNpc(
        tekksAnchor.x,
        tekksAnchor.y,
        TEKKS_NPC_SEARCH_RADIUS);
    if (!tekksId) {
        Log::Info("Froggy: Tekks NPC not found near (%.0f, %.0f)", tekksAnchor.x, tekksAnchor.y);
        return false;
    }
    Log::Info("Froggy: Tekks NPC found agent=%u", tekksId);
    LogTekksQuestSnapshot("Tekks pre-interact snapshot");

    auto* tekks = AgentMgr::GetAgentByID(tekksId);
    if (tekks) {
        MoveToAndWait(tekks->x, tekks->y, TEKKS_NPC_MOVE_THRESHOLD);
        WaitForLocalPositionSettle(TEKKS_NPC_SETTLE_TIMEOUT_MS, TEKKS_NPC_SETTLE_DISTANCE);
    }

    // ---- Send GoNPC packet via SendPacketDirect (bypasses engine hook) ----
    // The CtoS engine hook crashes on INTERACT_NPC (0x39) packets due to
    // FPU state corruption in the detour. SendPacketDirect calls PacketSend
    // directly on the current thread, bypassing GameThread::Enqueue and
    // the engine hook detour entirely.
    {
        auto* me = AgentMgr::GetMyAgent();
        Log::Info("Froggy: Tekks PRE-GoNPC pos=(%.0f, %.0f) tekks=(%.0f, %.0f) dist=%.0f",
                  me ? me->x : 0, me ? me->y : 0,
                  tekks ? tekks->x : 0, tekks ? tekks->y : 0,
                  (me && tekks) ? AgentMgr::GetDistance(me->x, me->y, tekks->x, tekks->y) : -1.0f);
    }

    auto initialInteract = MakeTekksDirectInteractOptions(
        TEKKS_INITIAL_INTERACT_TARGET_WAIT_MS,
        TEKKS_INITIAL_INTERACT_PASS_WAIT_MS,
        TEKKS_INITIAL_INTERACT_PASSES,
        "Tekks");
    (void)DungeonInteractions::PulseDirectNpcInteract(tekksId, initialInteract);

    // ---- Dwell then blind dialog sends (AutoIt TakeQuest0 flow) ----
    WaitMs(TEKKS_POST_INTERACT_DWELL_MS);
    {
        auto* me = AgentMgr::GetMyAgent();
        tekks = AgentMgr::GetAgentByID(tekksId);
        const float dist = (me && tekks) ? AgentMgr::GetDistance(me->x, me->y, tekks->x, tekks->y) : -1.0f;
        Log::Info("Froggy: Tekks POST-GoNPC pos=(%.0f, %.0f) dist=%.0f dialogOpen=%d buttons=%u sender=%u lastDialog=0x%X",
                  me ? me->x : 0, me ? me->y : 0, dist,
                  DialogMgr::IsDialogOpen() ? 1 : 0,
                  DialogMgr::GetButtonCount(),
                  DialogMgr::GetDialogSenderAgentId(),
                  DialogMgr::GetLastDialogId());
    }

    const auto snapshot = CaptureTekksDialogSnapshot();
    const uint32_t ping = snapshot.ping;
    LogTekksDialogSnapshot(snapshot);

    if (snapshot.dialog.dialog_open && snapshot.dialog.sender_agent_id == tekksId && snapshot.has_dungeon_entry) {
        if (!RefreshTekksQuestReadyForDungeonEntry("Tekks direct dungeon-entry precheck")) {
            Log::Info("Froggy: Tekks direct dungeon-entry button visible without Tekks's War; retrying quest flow later");
            return false;
        }
        const auto entryResult = SendTekksQuestDialog(
            GWA3::DialogIds::TekksWar::DUNGEON_ENTRY, "Tekks direct dungeon-entry", TEKKS_DIRECT_ENTRY_WAIT_BASE_MS + ping);
        LogTekksQuestSnapshot("Tekks direct dungeon-entry snapshot");
        const bool confirmed = IsTekksDungeonEntryConfirmed(
            entryResult.quest_present,
            entryResult.active_quest_id,
            entryResult.last_dialog_id == GWA3::DialogIds::TekksWar::DUNGEON_ENTRY);
        Log::Info("Froggy: Tekks direct dungeon-entry complete questPresent=%d activeQuest=0x%X lastDialog=0x%X confirmed=%d",
                  entryResult.quest_present ? 1 : 0,
                  entryResult.active_quest_id,
                  entryResult.last_dialog_id,
                  confirmed ? 1 : 0);
        return confirmed;
    }

    if (snapshot.quest_present) {
        const auto rewardResult = SendTekksQuestDialog(
            GWA3::DialogIds::TekksWar::QUEST_REWARD, "Tekks reward-first", TEKKS_REWARD_FIRST_WAIT_BASE_MS + ping);
        const bool clearedAfterReward = !rewardResult.quest_present;
        Log::Info("Froggy: Tekks reward-first snapshot clearedAfterReward=%d", clearedAfterReward ? 1 : 0);
        if (clearedAfterReward) {
            ReopenTekksAcceptAfterReward(tekksId, ping);
        }
    }

    return AcceptTekksQuestAndEnter(tekksId, ping);
}
