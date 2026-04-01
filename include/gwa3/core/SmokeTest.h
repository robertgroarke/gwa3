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

} // namespace GWA3::SmokeTest
