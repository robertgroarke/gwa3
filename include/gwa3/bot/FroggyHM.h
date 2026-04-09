#pragma once

#include <gwa3/bot/BotFramework.h>

namespace GWA3::Bot::Froggy {

    struct LastCombatStepInfo {
        bool valid = false;
        bool used_skill = false;
        bool auto_attack = false;
        int slot = 0; // 1-8, or 0 for non-skill actions
        uint32_t skill_id = 0;
        uint32_t target_id = 0;
        uint32_t role_mask = 0;
        uint8_t target_type = 0;
        uint32_t started_at_ms = 0;
        uint32_t finished_at_ms = 0;
        uint32_t expected_aftercast_ms = 0;
    };

    // Register all Froggy HM state handlers with the bot framework.
    void Register();

    // State handlers
    BotState HandleCharSelect(BotConfig& cfg);
    BotState HandleTownSetup(BotConfig& cfg);
    BotState HandleTravel(BotConfig& cfg);
    BotState HandleDungeon(BotConfig& cfg);
    BotState HandleLoot(BotConfig& cfg);
    BotState HandleMerchant(BotConfig& cfg);
    BotState HandleMaintenance(BotConfig& cfg);
    BotState HandleError(BotConfig& cfg);

    // Run unit tests for Froggy's internal pure-logic functions.
    // Returns number of failures.
    int RunFroggyUnitTests();

    // Execute one bounded builtin combat step against a target for integration testing.
    bool ExecuteBuiltinCombatStep(uint32_t targetId);

    // Retrieve a short description of the last builtin combat action chosen.
    const char* GetLastCombatStepDescription();

    // Refresh Froggy's cached player skillbar and role classification.
    bool RefreshCombatSkillbar();

    // Dump Froggy's builtin combat decision context for a target.
    void DebugDumpBuiltinCombatDecision(uint32_t targetId);

    // Read back the most recent builtin combat decision dump lines.
    int GetBuiltinCombatDecisionDumpCount();
    const char* GetBuiltinCombatDecisionDumpLine(int index);
    int GetCombatDebugTraceCount();
    const char* GetCombatDebugTraceLine(int index);

    // Read structured details for the most recent builtin combat step.
    LastCombatStepInfo GetLastCombatStepInfo();

    // Debug helpers for validating target-selection branches in integration tests.
    bool DebugResolveFirstSkillTarget(uint32_t roleMask, uint32_t defaultFoeId,
                                      uint32_t& outSkillId, uint32_t& outTargetId, uint8_t& outTargetType);
    uint32_t DebugGetCastingEnemy();
    uint32_t DebugGetEnchantedEnemy();
    uint32_t DebugGetMeleeRangeEnemy();

} // namespace GWA3::Bot::Froggy
