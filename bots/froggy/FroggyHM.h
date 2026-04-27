#pragma once

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonCombatRoutine.h>

namespace GWA3::Bot::Froggy {

    using LastCombatStepInfo = DungeonCombatRoutine::SkillActionResult;

    struct SparkflyTraversalCombatStats {
        uint32_t quick_step_attempts = 0;
        uint32_t skill_steps = 0;
        uint32_t auto_attack_steps = 0;
        uint32_t settle_requests = 0;
        uint32_t unsettled_skips = 0;
        uint32_t last_target_id = 0;
    };

    struct DungeonLoopTelemetry {
        bool started_in_lvl1 = false;
        bool started_in_lvl2 = false;
        bool entered_lvl2 = false;
        bool lvl1_to_lvl2_started = false;
        bool map_loaded = false;
        bool player_alive = false;
        bool boss_started = false;
        bool boss_completed = false;
        uint32_t chest_attempts = 0;
        uint32_t chest_successes = 0;
        bool reward_attempted = false;
        bool reward_dialog_latched = false;
        bool returned_to_sparkfly = false;
        uint32_t final_map_id = 0;
        uint32_t last_dialog_id = 0;
        uint32_t last_waypoint_index = 0;
        uint32_t waypoint_iterations = 0;
        uint32_t lvl1_to_lvl2_attempts = 0;
        uint32_t target_id = 0;
        uint32_t lvl1_portal_id = 0;
        uint32_t nearby_enemy_count = 0;
        float player_hp = 0.0f;
        float player_x = 0.0f;
        float player_y = 0.0f;
        float dist_to_exit = 0.0f;
        float nearest_enemy_dist = 0.0f;
        float lvl1_portal_x = 0.0f;
        float lvl1_portal_y = 0.0f;
        float lvl1_portal_dist = 0.0f;
        char last_waypoint_label[32] = {};
    };

    struct PostSparkflyRunDecision {
        BotState next_state = BotState::InDungeon;
        bool maintenance_deferred = false;
    };

    inline constexpr PostSparkflyRunDecision ResolvePostSparkflyRunDecision(bool maintenanceNeeded) {
        return {
            BotState::InDungeon,
            maintenanceNeeded,
        };
    }

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

    // Execute one bounded builtin combat step against a target.
    bool ExecuteBuiltinCombatStep(uint32_t targetId, bool quickStep = false);

    // Execute Froggy's real aggro-move path toward a waypoint.
    bool DebugAggroMoveTo(float x, float y, float fightRange = 1350.0f);
    bool DebugClearAggroInPlace(float fightRange = 1350.0f);

    // Reuse Froggy's actual Sparkfly waypoint route and Tekks staging move.
    bool DebugRunSparkflyRouteToTekks();
    bool DebugPrepareTekksDungeonEntry();

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

    // Read/reset Sparkfly traversal combat telemetry for route validation.
    void ResetSparkflyTraversalCombatStats();
    SparkflyTraversalCombatStats GetSparkflyTraversalCombatStats();

    // Run/read the full Bogroot loop.
    bool DebugRunDungeonLoopFromCurrentMap();
    void ResetDungeonLoopTelemetry();
    DungeonLoopTelemetry GetDungeonLoopTelemetry();

    // Debug helpers for validating target-selection branches.
    bool DebugResolveSyntheticSkillTarget(uint32_t roleMask, uint8_t targetType,
                                          uint32_t defaultFoeId, uint32_t& outTargetId);
    bool DebugResolveUsableSkillTargetForSlot(uint32_t slot, uint32_t defaultFoeId,
                                              uint32_t& outSkillId, uint32_t& outTargetId, uint8_t& outTargetType);
    uint32_t DebugGetCastingEnemy();
    uint32_t DebugGetEnchantedEnemy();
    uint32_t DebugGetMeleeRangeEnemy();

} // namespace GWA3::Bot::Froggy
