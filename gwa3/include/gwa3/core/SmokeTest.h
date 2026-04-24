#pragma once

namespace GWA3::SmokeTest {

    // Run the injection smoke test. Reads game state, validates patterns,
    // writes report to gwa3_smoke_report.txt. Read-only — no hooks, no commands.
    // Returns number of failed checks.
    int RunSmokeTest();

    // Run the bot framework smoke test. Validates thread lifecycle,
    // state transitions, config loading. No game commands.
    // Returns number of failed checks.
    int RunBotFrameworkTest();

    // Run behavioral command tests. Sends movement, targeting, packets.
    // Requires logged-in character in an outpost.
    // Returns number of failed checks.
    int RunBehavioralTest();

    // Run integration tests. Chains: char select -> login -> hero setup ->
    // movement -> targeting. Single injection session.
    int RunIntegrationTest();

    // Run isolated NPC/dialog experiment. Keeps the unstable interaction
    // out of the stable integration suite while we debug it.
    int RunNpcDialogTest();

    // Run isolated merchant/trader quote test. Opens a trader window and
    // requests a quote without buying or selling anything.
    int RunMerchantQuoteTest();

    // Run isolated identify/salvage repro. Uses stage flags so identify,
    // salvage-open, and full single-item salvage can be debugged directly.
    int RunIdentifySalvageIsolationTest();

    // Run consumable crafting test (conset cycle: gold, materials, craft).
    int RunConsumableCraftingTest();

    // Run advanced integration tests. Exercises PlayerMgr, CameraMgr,
    // MemoryMgr, deep inventory introspection, agent enumeration,
    // UI frame validation, AreaInfo, hero flagging, skillbar data,
    // hard mode toggle, return-to-outpost, and more.
    int RunAdvancedTest();

    // Run advanced workflow tests. Exercises item manipulation, salvage,
    // skillbar management, party composition, titles, callbacks, StoC
    // packet coverage, and other previously untested APIs.
    int RunAdvancedWorkflowTest();

    // Run Froggy feature tests. Unit tests for pure logic (decoding,
    // filtering) plus integration tests for inventory/skillbar/merchant
    // functions. Requires logged-in character.
    int RunFroggyFeatureTest();

    // Run the Raven's Point feature test. Boots the Raven module and
    // waits for the route to progress into the dungeon.
    int RunRavensPointFeatureTest();

    // Run the Arachnis Haunt feature test. Verifies the reward bounce,
    // live dungeon entry, dungeon progression, and return to Magus Stones.
    int RunArachnisHauntFeatureTest();

    // Run the Rragar's Menagerie feature test. Boots the Rragars module and
    // waits for the full dungeon route to return to Doomlore after reward.
    int RunRragarsMenagerieFeatureTest();

    // Run the isolated explorable hero-flagging slice from the Froggy flow.
    // This intentionally stops shortly after validating flag/unflag behavior.
    int RunFroggyExplorableFlaggingTest();

    // Run the narrowed Sparkfly route/combat slice from the Froggy flow.
    // This skips the noisy merchant/hero-setup phases and focuses on movement
    // plus builtin combat telemetry in Sparkfly.
    int RunFroggySparkflyRouteTest();

    // Run the long-lived player trade helper lane.
    int RunTradeHelperMode();

} // namespace GWA3::SmokeTest
