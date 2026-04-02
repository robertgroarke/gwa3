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

    // Run advanced integration tests. Exercises PlayerMgr, CameraMgr,
    // MemoryMgr, deep inventory introspection, agent enumeration,
    // UI frame validation, AreaInfo, hero flagging, skillbar data,
    // hard mode toggle, return-to-outpost, and more.
    int RunAdvancedTest();

} // namespace GWA3::SmokeTest
