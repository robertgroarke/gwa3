// Consolidated small integration feature wrappers.
// --- src/tests/IntegrationTestAdvanced.cpp ---
// : Advanced Workflow Integration Tests
// Exercises untested manager APIs with actual game-state mutations:
// item manipulation, salvage, skillbar management, party composition,
// titles, merchant buy/sell, callbacks, GameThread hooks, StoC packets.
// Launched via GWA3_TEST_ADVANCED_WORKFLOW flag.

#include "IntegrationTestInternal.h"

#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Memory.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/CallbackRegistry.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/CameraMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/managers/GuildMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/managers/FriendListMgr.h>
#include <gwa3/managers/ChatLogMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/game/MapIds.h>

#include <Windows.h>
#include <atomic>

namespace GWA3::SmokeTest {

// ===== Helpers =====

#include "IntegrationTestAdvancedSupport.h"
#include "IntegrationTestAdvancedItemMove.h"
#include "IntegrationTestAdvancedGoldTransfer.h"
#include "IntegrationTestAdvancedMemAllocFree.h"
#include "IntegrationTestAdvancedLoadSkillbar.h"
#include "IntegrationTestAdvancedPartyManagement.h"
#include "IntegrationTestAdvancedTitleManagement.h"
#include "IntegrationTestAdvancedCallbackRegistry.h"
#include "IntegrationTestAdvancedGameThreadCallbacks.h"
#include "IntegrationTestAdvancedStoCPacketTypes.h"
#include "IntegrationTestAdvancedQuestManagement.h"
#include "IntegrationTestAdvancedUIFrameInteraction.h"
#include "IntegrationTestAdvancedAgentInteraction.h"
#include "IntegrationTestAdvancedCameraFOV.h"
#include "IntegrationTestAdvancedPersonalDir.h"
#include "IntegrationTestAdvancedExplorableCallTarget.h"

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestNpcDialog.cpp ---
// NPC dialog integration feature group.

#include "IntegrationTestInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>

#include <Windows.h>

namespace GWA3::SmokeTest {

namespace {

void ReportRecentDialogUiTrace(const char* label) {
    uint32_t trace[32] = {};
    const uint32_t count = DialogMgr::GetRecentUITrace(trace, _countof(trace));
    char buf[512] = {};
    size_t used = 0;
    for (uint32_t i = 0; i < count && used + 16 < sizeof(buf); ++i) {
        used += sprintf_s(buf + used, sizeof(buf) - used, "%s0x%X", i == 0 ? "" : " ", trace[i]);
    }
    IntReport("  %s recent UI trace (%u): %s", label, count, count > 0 ? buf : "none");
}

} // namespace

bool TestNpcDialog() {
    IntReport("===  NPC + Dialog ===");

    if (ReadMapId() == 0 || ReadMyId() == 0) {
        IntSkip("NPC dialog", "Not in game");
        return false;
    }

    const uint32_t targetId = FindNearbyNpcLikeAgent(20000.0f);
    if (!targetId) {
        IntSkip("NPC interaction", "No nearby NPC-like living agent found");
        IntReport("");
        return false;
    }

    auto* agent = static_cast<AgentLiving*>(AgentMgr::GetAgentByID(targetId));
    IntReport("  Interacting with agent %u (allegiance=%u, player_number=%u, npc_id=%u)...",
              targetId,
              agent ? agent->allegiance : 0,
              agent ? agent->player_number : 0,
              agent ? agent->transmog_npc_id : 0);
    float npcX = 0.0f;
    float npcY = 0.0f;
    if (TryReadAgentPosition(targetId, npcX, npcY)) {
        const bool nearNpc = MovePlayerNear(npcX, npcY, 120.0f, 12000);
        float meX = 0.0f;
        float meY = 0.0f;
        TryReadAgentPosition(ReadMyId(), meX, meY);
        IntReport("  NPC pre-hook approach: near=%d player=(%.0f, %.0f) npc=(%.0f, %.0f) dist=%.0f",
                  nearNpc ? 1 : 0,
                  meX, meY, npcX, npcY,
                  AgentMgr::GetDistance(meX, meY, npcX, npcY));
    }

    AgentMgr::ChangeTarget(targetId);
    Sleep(250);
    DialogMgr::ResetRecentUITrace();
    const bool dialogUiObserved = DialogMgr::NPCHook(targetId, 2000u);
    Sleep(250);
    IntCheck("NPCHook sent (no crash)", true);
    IntCheck("Dialog UI message observed after NPCHook", dialogUiObserved);
    IntReport("  Dialog hook: lastUi=0x%X armed=0x%X observed=0x%X",
              DialogMgr::GetLastUIMessageId(),
              DialogMgr::GetArmedUIMessageId(),
              DialogMgr::GetObservedUIMessageId());
    ReportRecentDialogUiTrace("NPCHook");

    IntReport("  Sending dialog 0x%X via DialogHook...", GWA3::DialogIds::NPC_TALK);
    DialogMgr::ResetRecentUITrace();
    const bool talkUiObserved = DialogMgr::DialogHook(GWA3::DialogIds::NPC_TALK, 2000u);
    Sleep(250);
    IntCheck("DialogHook sent (no crash)", true);
    IntCheck("Dialog hook captured last dialog id", DialogMgr::GetLastDialogId() == GWA3::DialogIds::NPC_TALK);
    IntReport("  Dialog hook: talkUiObserved=%d lastUi=0x%X armed=0x%X observed=0x%X lastDialogId=0x%X",
              talkUiObserved ? 1 : 0,
              DialogMgr::GetLastUIMessageId(),
              DialogMgr::GetArmedUIMessageId(),
              DialogMgr::GetObservedUIMessageId(),
              DialogMgr::GetLastDialogId());
    ReportRecentDialogUiTrace("DialogHook");

    GameThread::Enqueue([]() {
        AgentMgr::CancelAction();
    });
    Sleep(500);
    IntCheck("CancelAction sent after dialog (no crash)", true);

    IntReport("");
    return true;
}

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestSession.cpp ---
// Session setup, login/bootstrap, and NPC/trader interaction feature groups.

#include "IntegrationTestInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/TradePartnerHook.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/ChatLogMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/game/MapIds.h>

#include <string>

namespace GWA3::SmokeTest {

namespace {
#include "IntegrationTestSessionSupport.h"
#include "IntegrationTestSessionUiCraftSupport.h"

} // namespace

#include "IntegrationTestMerchantQuoteImpl.h"

#include "IntegrationTestConsumableCraftingImpl.h"

#include "IntegrationTestTradeHelperModeImpl.h"

#include "IntegrationTestIdentifySalvageImpl.h"

int RunIdentifySalvageIsolationTest() {
    int failures = 0;
    if (!TestIdentifySalvageIsolation()) ++failures;
    return failures;
}

int RunConsumableCraftingTest() {
    int failures = 0;
    if (!TestConsumableCrafting()) ++failures;
    return failures;
}

} // namespace GWA3::SmokeTest
