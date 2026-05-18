// Consolidated small integration runner wrappers.
// --- src/tests/IntegrationTestAdvancedSuiteRunner.cpp ---
#include "IntegrationTestInternal.h"

#include <gwa3/core/Log.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/managers/MapMgr.h>

namespace GWA3::SmokeTest {

int RunAdvancedTest() {
    BeginIntegrationRun("=== GWA3 Advanced Integration Test Suite ===");

    const bool inGame = TestCharSelectLogin();

    if (inGame) {
        WaitForSessionHydrationIfNeeded();

        // Read-only introspection tests (safe in any map type)
        TestPlayerData();
        TestCameraIntrospection();
        TestClientInfo();
        TestInventoryIntrospection();
        TestAgentArrayEnumeration();
        TestUIFrameValidation();
        TestAreaInfoValidation();
        TestSkillbarDataValidation();
        TestPartyState();
        TestTargetLogHook();
        TestGuildData();
        TestMapStateQueries();
        TestPingStability();
        TestWeaponSetValidation();
        TestAgentDistanceCrossCheck();
        TestCameraControls();
        TestPostProcessEffectOffset();
        TestGwEndSceneOffset();
        TestItemClickOffset();
        TestSendChatOffset();
        TestAddToChatLogOffset();
        TestSkipCinematicOffset();
        TestRequestQuestInfoOffset();
        TestFriendListOffsets();
        TestDrawOnCompassOffset();
        TestChatColorOffsets();
        TestCameraUpdateBypassPatch();
        TestTradeOffsets();
        TestLevelDataBypassPatch();
        TestMapPortBypassPatch();
        TestEffectArray();
        TestStoCHook();
        TestRenderingToggle();

        // Chat write (local only, safe anywhere)
        TestChatWriteLocal();

        // Outpost-only tests
        const AreaInfo* area = MapMgr::GetAreaInfo(ReadMapId());
        const bool inOutpost = area && !IsSkillCastMapType(area->type);

        if (inOutpost) {
            IntSkip("Hero Flagging (043)", "Outpost session - hero commands only valid in explorable");
            TestHardModeToggle();
        } else {
            IntSkip("Hero Flagging (043)", "Not in outpost");
            IntSkip("Hard Mode Toggle (046)", "Not in outpost");
        }
    } else {
        IntSkip("All advanced tests", "Login failed");
    }

    FinishIntegrationRunSummary();

    Log::Info("[INTG] Advanced complete: %d passed, %d failed, %d skipped",
              GetIntegrationPassedCount(),
              GetIntegrationFailedCount(),
              GetIntegrationSkippedCount());
    StopWatchdog();
    Log::Info("[INTG] Heartbeat at exit: %u, crashDetected=%d",
              RenderHook::GetHeartbeat(), RuntimeCrashDetected() ? 1 : 0);
    return GetIntegrationFailedCount();
}

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestAdvancedWorkflowRunner.cpp ---
#include "IntegrationTestInternal.h"

#include <gwa3/core/Log.h>

namespace GWA3::SmokeTest {

#include "IntegrationTestAdvancedWorkflowSteps.h"

int RunAdvancedWorkflowTest() {
    StartWatchdog();

    BeginIntegrationRun("=== GWA3 Advanced Workflow Test Suite (074-089) ===");

    bool inGame = TestCharSelectLogin();

    if (inGame) {
        WaitForPlayerWorldReady(10000);
        if (AbortWorkflowIfRuntimeFailed("bootstrap")) goto workflow_done;

        // 074: Item workflows
        if (!RunAdvancedWorkflowStep("TestItemMove", &TestItemMove)) goto workflow_done;
        if (!RunAdvancedWorkflowStep("TestGoldTransfer", &TestGoldTransfer)) goto workflow_done;
        if (!RunAdvancedWorkflowStep("TestMemAllocFree", &TestMemAllocFree)) goto workflow_done;

        // 076: Skillbar management
        if (!RunAdvancedWorkflowStep("TestLoadSkillbar", &TestLoadSkillbar)) goto workflow_done;

        // 077: Party management
        if (!RunAdvancedWorkflowStep("TestPartyManagement", &TestPartyManagement)) goto workflow_done;

        // 078: Title management
        if (!RunAdvancedWorkflowStep("TestTitleManagement", &TestTitleManagement)) goto workflow_done;

        // 080: Callback registry
        if (!RunAdvancedWorkflowStep("TestCallbackRegistry", &TestCallbackRegistry)) goto workflow_done;

        // 081: GameThread persistent callbacks
        if (!RunAdvancedWorkflowStep("TestGameThreadCallbacks", &TestGameThreadCallbacks)) goto workflow_done;

        // 082: StoC packet type coverage
        if (!RunAdvancedWorkflowStep("TestStoCPacketTypes", &TestStoCPacketTypes)) goto workflow_done;

        // 083: Quest management
        if (!RunAdvancedWorkflowStep("TestQuestManagement", &TestQuestManagement)) goto workflow_done;

        // 085: UI frame interaction
        if (!RunAdvancedWorkflowStep("TestUIFrameInteraction", &TestUIFrameInteraction)) goto workflow_done;

        // 086: Agent interaction
        if (!RunAdvancedWorkflowStep("TestAgentInteraction", &TestAgentInteraction)) goto workflow_done;

        // 087: Camera FOV
        if (!RunAdvancedWorkflowStep("TestCameraFOV", &TestCameraFOV)) goto workflow_done;

        // 089: Memory personal dir
        if (!RunAdvancedWorkflowStep("TestPersonalDir", &TestPersonalDir)) goto workflow_done;

        // 086b: CallTarget in explorable (walks into Sparkfly Swamp)
        if (!RunAdvancedWorkflowStep("TestExplorableCallTarget", &TestExplorableCallTarget)) goto workflow_done;
    } else {
        IntSkip("All advanced workflow tests", "Login failed");
    }

workflow_done:
    FinishIntegrationRunSummary();

    StopWatchdog();
    Log::Info("[INTG] Advanced workflow complete: %d passed, %d failed, %d skipped",
              GetIntegrationPassedCount(),
              GetIntegrationFailedCount(),
              GetIntegrationSkippedCount());
    Log::Info("[INTG] Advanced workflow exit state: crashDetected=%d disconnectDetected=%d",
              ShouldAbortForRuntimeFailure() ? 1 : 0,
              RuntimeDisconnectDetected() ? 1 : 0);
    return GetIntegrationFailedCount();
}

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestMainRunner.cpp ---
#include "IntegrationTestInternal.h"

#include <gwa3/core/Log.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>

namespace GWA3::SmokeTest {

#include "IntegrationTestMainFlowSupport.h"

int RunIntegrationTest() {
    StartWatchdog();

    BeginIntegrationRun("=== GWA3 Integration Test Suite ===");

    // Phase 1: character select and login
    bool inGame = TestCharSelectLogin();

    if (inGame) {
        const bool startedExplorable = IsCurrentMapSkillCastable();
        const bool sessionHydrated = WaitForSessionHydrationIfNeeded();

        if (startedExplorable) {
            RunExplorableStartIntegrationFlow(sessionHydrated);
        } else {
            RunOutpostStartIntegrationFlow();
        }
    } else {
        SkipMainIntegrationSuiteForLoginFailure();
    }

    FinishIntegrationRunSummary();

    Log::Info("[INTG] Complete: %d passed, %d failed, %d skipped",
              GetIntegrationPassedCount(),
              GetIntegrationFailedCount(),
              GetIntegrationSkippedCount());
    StopWatchdog();
    Log::Info("[INTG] Heartbeat at exit: %u, crashDetected=%d",
              RenderHook::GetHeartbeat(), RuntimeCrashDetected() ? 1 : 0);
    return GetIntegrationFailedCount();
}

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestMerchantQuoteRunner.cpp ---
#include "IntegrationTestInternal.h"

#include <gwa3/core/Log.h>
#include <gwa3/core/RenderHook.h>

namespace GWA3::SmokeTest {

int RunMerchantQuoteTest() {
    BeginIntegrationRun("=== GWA3 Merchant/Quote Test ===");

    const bool inGame = TestCharSelectLogin();
    if (inGame) {
        WaitForSessionHydrationIfNeeded();
        TestMerchantQuote();
    } else {
        IntSkip("Merchant + Quote (032)", "Login failed");
    }

    FinishIntegrationRunSummary();

    Log::Info("[INTG] Merchant/Quote complete: %d passed, %d failed, %d skipped",
              GetIntegrationPassedCount(),
              GetIntegrationFailedCount(),
              GetIntegrationSkippedCount());
    StopWatchdog();
    Log::Info("[INTG] Heartbeat at exit: %u, crashDetected=%d",
              RenderHook::GetHeartbeat(), ShouldAbortForRuntimeFailure() ? 1 : 0);
    return GetIntegrationFailedCount();
}

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestNpcDialogRunner.cpp ---
#include "IntegrationTestInternal.h"

#include <gwa3/core/Log.h>
#include <gwa3/core/RenderHook.h>

namespace GWA3::SmokeTest {

int RunNpcDialogTest() {
    BeginIntegrationRun("=== GWA3 NPC/Dialog Test ===");

    const bool inGame = TestCharSelectLogin();
    if (inGame) {
        WaitForSessionHydrationIfNeeded();
        TestNpcDialog();
    } else {
        IntSkip("NPC + Dialog (032)", "Login failed");
    }

    FinishIntegrationRunSummary();

    Log::Info("[INTG] NPC/Dialog complete: %d passed, %d failed, %d skipped",
              GetIntegrationPassedCount(),
              GetIntegrationFailedCount(),
              GetIntegrationSkippedCount());
    StopWatchdog();
    Log::Info("[INTG] Heartbeat at exit: %u, crashDetected=%d",
              RenderHook::GetHeartbeat(), ShouldAbortForRuntimeFailure() ? 1 : 0);
    return GetIntegrationFailedCount();
}

} // namespace GWA3::SmokeTest
