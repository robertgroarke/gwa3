#pragma once

#include <gwa3/dungeon/DungeonQuest.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>

#include <cstdint>

namespace GWA3::Bot::Froggy {

inline constexpr uint32_t BOGROOT_DUNGEON_MAPS[] = {
    MapIds::BOGROOT_GROWTHS_LVL1,
    MapIds::BOGROOT_GROWTHS_LVL2,
};
inline constexpr int BOGROOT_DUNGEON_MAP_COUNT = static_cast<int>(sizeof(BOGROOT_DUNGEON_MAPS) / sizeof(BOGROOT_DUNGEON_MAPS[0]));
inline constexpr float AGGRO_BOGROOT_SIDESTEP_RANDOM_RADIUS = 500.0f;
inline constexpr uint32_t AGGRO_BOGROOT_FIGHT_BUDGET_MS = 4000u;
inline constexpr float AGGRO_BOGROOT_LOOT_RADIUS = 3000.0f;

inline constexpr uint32_t BLESSING_TITLE_ID = 0x27u;
inline constexpr uint32_t BLESSING_ACCEPT_DIALOG_ID = 0x84u;
inline constexpr uint32_t BLESSING_TITLE_SETTLE_MS = 1000u;
inline constexpr uint32_t BLESSING_SETTLE_TIMEOUT_MS = 1200u;
inline constexpr float BLESSING_PRIMARY_SEARCH_RADIUS = 900.0f;
inline constexpr float BLESSING_FALLBACK_SEARCH_RADIUS = 1500.0f;
inline constexpr float BLESSING_SIGNPOST_MOVE_THRESHOLD = 120.0f;
inline constexpr float BLESSING_NPC_MOVE_THRESHOLD = 90.0f;
inline constexpr float BLESSING_SETTLE_DISTANCE = 15.0f;
inline constexpr float BOSS_CHEST_X = 14876.0f;
inline constexpr float BOSS_CHEST_Y = -19033.0f;
inline constexpr uint32_t BOSS_WAYPOINT_POST_FIGHT_LOOT_DELAY_MS = 3000u;
inline constexpr float BOSS_WAYPOINT_LOOT_RADIUS = 1500.0f;
inline constexpr float BOSS_CHEST_OPEN_RADIUS = 5000.0f;
inline constexpr float BOSS_CHEST_LOOT_RADIUS = 5000.0f;
inline constexpr uint32_t BOSS_CHEST_FIRST_LOOT_DELAY_MS = 2000u;
inline constexpr uint32_t BOSS_CHEST_RETRY_LOOT_DELAY_MS = 1000u;
inline constexpr uint32_t BOSS_REWARD_SETTLE_TIMEOUT_MS = 1000u;
inline constexpr float BOSS_REWARD_SETTLE_DISTANCE = 15.0f;
inline constexpr float BOSS_REWARD_NPC_SEARCH_RADIUS = 6000.0f;
inline constexpr float BOSS_REWARD_LOCAL_NPC_SEARCH_RADIUS = 3500.0f;
inline constexpr float BOSS_REWARD_NPC_MOVE_THRESHOLD = 120.0f;
inline constexpr uint32_t BOSS_REWARD_INTERACT_TARGET_WAIT_MS = 500u;
inline constexpr uint32_t BOSS_REWARD_INTERACT_PASS_WAIT_MS = 1500u;
inline constexpr int BOSS_REWARD_INTERACT_PASSES = 3;
inline constexpr uint32_t BOSS_REWARD_DIALOG_DWELL_MS = 1000u;
inline constexpr uint32_t BOSS_REWARD_TARGET_SETTLE_MS = 150u;
inline constexpr int BOSS_REWARD_ADVANCE_MAX_PASSES = 4;
inline constexpr uint32_t BOSS_REWARD_ACCEPT_RETRY_TIMEOUT_MS = 5000u;
inline constexpr uint32_t BOSS_REWARD_RETRY_POST_DIALOG_WAIT_MS = 1000u;
inline constexpr uint32_t BOSS_REWARD_RETRY_REFRESH_DELAY_MS = 500u;
inline constexpr int BOSS_REWARD_FALLBACK_SEND_ATTEMPTS = 1;
inline constexpr uint32_t BOSS_REWARD_FALLBACK_SEND_DELAY_MS = 1000u;
inline constexpr uint32_t BOSS_REWARD_FALLBACK_REFRESH_DELAY_MS = 500u;
inline constexpr uint32_t BOSS_POST_REWARD_LONG_TOTAL_WAIT_MS = 210000u;
inline constexpr uint32_t BOSS_POST_REWARD_SHORT_TOTAL_WAIT_MS = 45000u;
inline constexpr uint32_t BOSS_POST_REWARD_LONG_BOGROOT_WAIT_MS = 180000u;
inline constexpr uint32_t BOSS_POST_REWARD_SHORT_BOGROOT_WAIT_MS = 30000u;
inline constexpr float DEFAULT_LOOT_PICKUP_RADIUS = 1200.0f;
inline constexpr float DEFAULT_CHEST_OPEN_RADIUS = 1500.0f;
inline constexpr float CHEST_BUNDLE_MIN_SIGNPOST_RADIUS = 1500.0f;
inline constexpr float CHEST_BUNDLE_MIN_LOOT_RADIUS = 5000.0f;
inline constexpr float CHEST_BUNDLE_LOOT_RADIUS_MULTIPLIER = 4.0f;
inline constexpr int CHEST_BUNDLE_OPEN_ATTEMPTS = 2;
inline constexpr int CHEST_BUNDLE_PICKUP_ATTEMPTS = 2;
inline constexpr uint32_t CHEST_BUNDLE_OPEN_RETRY_DELAY_MS = 500u;
inline constexpr uint32_t CHEST_BUNDLE_PICKUP_RETRY_DELAY_MS = 500u;
inline constexpr uint32_t CHEST_BUNDLE_VERIFY_DELAY_MS = 1000u;
inline constexpr float BOGROOT_BOSS_KEY_DOOR_PROBE_X = 17482.0f;
inline constexpr float BOGROOT_BOSS_KEY_DOOR_PROBE_Y = -6661.0f;
inline constexpr float BOGROOT_BOSS_KEY_X = 16854.0f;
inline constexpr float BOGROOT_BOSS_KEY_Y = -5830.0f;
inline constexpr float BOGROOT_BOSS_KEY_SCAN_RANGE = 18000.0f;
inline constexpr float BOGROOT_BOSS_DOOR_X = 17925.0f;
inline constexpr float BOGROOT_BOSS_DOOR_Y = -6197.0f;
inline constexpr uint32_t BOGROOT_BOSS_KEY_MODELS[] = {
    ItemModelIds::DUNGEON_KEY_SORROWS_FURNACE,
    ItemModelIds::DUNGEON_KEY_PRISON,
    ItemModelIds::DUNGEON_KEY_BOGROOT,
};
inline constexpr int BOGROOT_BOSS_KEY_MODEL_COUNT = static_cast<int>(
    sizeof(BOGROOT_BOSS_KEY_MODELS) / sizeof(BOGROOT_BOSS_KEY_MODELS[0]));
inline constexpr float SPARKFLY_DUNGEON_SIDE_THRESHOLD = 12000.0f;
inline constexpr float SPARKFLY_TEKKS_SHORT_MOVE_THRESHOLD = 700.0f;
inline constexpr float SPARKFLY_TEKKS_SHORT_ACCEPT_DISTANCE = 900.0f;
inline constexpr float SPARKFLY_TEKKS_FULL_MOVE_THRESHOLD = 900.0f;
inline constexpr float SPARKFLY_TEKKS_FULL_ACCEPT_DISTANCE = 1100.0f;
inline constexpr float MAP_TRANSITION_STAGE_THRESHOLD = 300.0f;
inline constexpr uint32_t MAP_TRANSITION_REVERSE_PUSH_DELAY_MS = 500u;
inline constexpr uint32_t MAP_TRANSITION_RETURN_PUSH_DELAY_MS = 1000u;
inline constexpr uint32_t MAP_TRANSITION_READY_TIMEOUT_MS = 60000u;
inline constexpr uint32_t MAP_TRANSITION_PUSH_TIMEOUT_MS = 30000u;
inline constexpr uint32_t MAP_TRANSITION_PUSH_INTERVAL_MS = 3000u;
inline constexpr uint32_t MAP_TRANSITION_ENTRY_ROUTE_DWELL_MS = 1000u;
inline constexpr float TEKKS_NPC_SEARCH_RADIUS = 1800.0f;
inline constexpr float TEKKS_ANCHOR_MOVE_THRESHOLD = 500.0f;
inline constexpr uint32_t TEKKS_PRE_INTERACT_DWELL_MS = 500u;
inline constexpr uint32_t TEKKS_CANCEL_DWELL_MS = 500u;
inline constexpr float TEKKS_NPC_MOVE_THRESHOLD = 100.0f;
inline constexpr uint32_t TEKKS_NPC_SETTLE_TIMEOUT_MS = 1000u;
inline constexpr float TEKKS_NPC_SETTLE_DISTANCE = 15.0f;
inline constexpr uint32_t TEKKS_INITIAL_INTERACT_TARGET_WAIT_MS = 500u;
inline constexpr uint32_t TEKKS_INITIAL_INTERACT_PASS_WAIT_MS = 2000u;
inline constexpr int TEKKS_INITIAL_INTERACT_PASSES = 3;
inline constexpr uint32_t TEKKS_POST_INTERACT_DWELL_MS = 2000u;
inline constexpr uint32_t TEKKS_DIRECT_ENTRY_WAIT_BASE_MS = 1000u;
inline constexpr uint32_t TEKKS_REWARD_FIRST_WAIT_BASE_MS = 500u;
inline constexpr uint32_t TEKKS_DIALOG_REFRESH_DELAY_MS = 150u;
inline constexpr uint32_t TEKKS_POST_REWARD_WAIT_BASE_MS = 500u;
inline constexpr uint32_t TEKKS_POST_REWARD_MAX_BUTTONS_PER_PASS = 4u;
inline constexpr int TEKKS_POST_REWARD_MAX_PASSES = 4;
inline constexpr uint32_t TEKKS_REOPEN_ACCEPT_TARGET_WAIT_BASE_MS = 150u;
inline constexpr uint32_t TEKKS_REOPEN_ACCEPT_PASS_WAIT_MS = 1500u;
inline constexpr int TEKKS_REOPEN_ACCEPT_INTERACT_PASSES = 1;
inline constexpr int TEKKS_REOPEN_ACCEPT_ATTEMPTS = 3;
inline constexpr uint32_t TEKKS_ACCEPT_WAIT_BASE_MS = 500u;
inline constexpr uint32_t TEKKS_ACCEPT_VERIFY_TIMEOUT_MS = 2500u;
inline constexpr uint32_t TEKKS_ACCEPT_VERIFY_REFRESH_INTERVAL_MS = 500u;
inline constexpr uint32_t TEKKS_ACCEPT_VERIFY_POLL_MS = 100u;
inline constexpr uint32_t TEKKS_SET_ACTIVE_DWELL_MS = 150u;
inline constexpr uint32_t TEKKS_TALK_WAIT_BASE_MS = 500u;
inline constexpr uint32_t TEKKS_ENTRY_DIALOG_WAIT_BASE_MS = 1000u;
inline constexpr uint32_t TEKKS_ENTRY_VERIFY_WAIT_BASE_MS = 2000u;
inline constexpr uint32_t TEKKS_ENTRY_VERIFY_REFRESH_INTERVAL_MS = 500u;
inline constexpr uint32_t TEKKS_ENTRY_VERIFY_POLL_MS = 100u;
inline constexpr uint32_t TEKKS_QUEST_REFRESH_DELAY_MS = 250u;
inline constexpr int TEKKS_DIALOG_RESET_FAILURE_THRESHOLD = 5;
inline constexpr uint32_t TEKKS_DIALOG_RESET_SETTLE_MS = 1000u;
inline constexpr float TELEMETRY_NEAREST_ENEMY_RANGE = 5000.0f;
inline constexpr float TELEMETRY_NEARBY_ENEMY_RANGE = 1800.0f;
inline constexpr float BOGROOT_LVL1_TO_LVL2_HANDOFF_CLEARANCE = 3000.0f;
inline constexpr float BOGROOT_LVL1_TO_LVL2_PORTAL_SEARCH_RADIUS = 1200.0f;
inline constexpr uint32_t BOGROOT_LVL1_TO_LVL2_TIMEOUT_MS = 60000u;
inline constexpr uint32_t BOGROOT_LVL1_TO_LVL2_LOG_INTERVAL_MS = 2000u;
inline constexpr uint32_t BOGROOT_LVL1_TO_LVL2_MOVE_POLL_MS = 250u;
inline constexpr uint32_t BOGROOT_LVL2_SPAWN_READY_TIMEOUT_MS = 15000u;
inline constexpr uint32_t BOGROOT_LVL2_SPAWN_SETTLE_TIMEOUT_MS = 1500u;
inline constexpr float BOGROOT_LVL2_SPAWN_SETTLE_DISTANCE = 24.0f;

inline constexpr DungeonQuest::TravelPoint SPARKFLY_TEKKS_STAGE = {12061.0f, 22485.0f};
inline constexpr DungeonQuest::TravelPoint SPARKFLY_TEKKS_SEARCH = {12396.0f, 22407.0f};
inline constexpr DungeonQuest::TravelPoint SPARKFLY_DUNGEON_ENTRY_STAGE = {12228.0f, 22677.0f};
inline constexpr DungeonQuest::TravelPoint BOGROOT_LVL1_TO_LVL2_PORTAL = {7665.0f, -19050.0f};
inline constexpr DungeonQuest::TravelPoint BOGROOT_REVERSE_TO_SPARKFLY_STAGE = {14747.0f, 480.0f};
inline constexpr DungeonQuest::TravelPoint BOGROOT_RETURN_TO_SPARKFLY_STAGE = {14876.0f, 632.0f};
inline constexpr DungeonQuest::TravelPoint BOGROOT_RETURN_TO_SPARKFLY_PUSH = {14700.0f, 450.0f};
inline constexpr DungeonQuest::TravelPoint GADDS_MERCHANT = {-8374.0f, -22491.0f};
inline constexpr DungeonQuest::TravelPoint GADDS_XUNLAI = {-10481.0f, -22787.0f};
inline constexpr DungeonQuest::TravelPoint GADDS_MATERIAL_TRADER = {-9097.0f, -23353.0f};
inline constexpr DungeonQuest::TravelPoint GADDS_TO_SPARKFLY_PATH[] = {
    {-10018.0f, -21892.0f},
    {-9550.0f, -20400.0f},
};
inline constexpr int GADDS_TO_SPARKFLY_PATH_COUNT = static_cast<int>(
    sizeof(GADDS_TO_SPARKFLY_PATH) / sizeof(GADDS_TO_SPARKFLY_PATH[0]));
inline constexpr DungeonQuest::TravelPoint GADDS_TO_SPARKFLY_ZONE = {-9451.0f, -19766.0f};
inline constexpr uint16_t GADDS_MERCHANT_PLAYER_NUMBER = 6060u;
inline constexpr uint16_t GADDS_MATERIAL_TRADER_PLAYER_NUMBER = 6763u;

inline constexpr bool IsBogrootMapId(uint32_t mapId) {
    return mapId == MapIds::BOGROOT_GROWTHS_LVL1 || mapId == MapIds::BOGROOT_GROWTHS_LVL2;
}

inline constexpr bool IsTekksQuestReadyForDungeonEntry(bool questPresent, uint32_t activeQuestId) {
    return questPresent || activeQuestId == QuestIds::TEKKS_WAR;
}

inline constexpr bool IsTekksDungeonEntryConfirmed(bool questPresent, uint32_t activeQuestId, bool entryReady) {
    return entryReady && IsTekksQuestReadyForDungeonEntry(questPresent, activeQuestId);
}

inline constexpr uint32_t ENTRY_DIALOGS[] = {
    DialogIds::NPC_TALK,
    DialogIds::TekksWar::QUEST_ACCEPT,
};

inline constexpr uint32_t REWARD_DIALOGS[] = {
    DialogIds::TekksWar::QUEST_REWARD,
};

inline constexpr DungeonQuest::TravelPoint ENTRY_PATH[] = {
    {12228.0f, 22677.0f},
    {12470.0f, 25036.0f},
    {12968.0f, 26219.0f},
};

inline DungeonQuest::BootstrapPlan GetEntryBootstrapPlan() {
    DungeonQuest::BootstrapPlan plan;
    plan.npc = {SPARKFLY_TEKKS_SEARCH.x, SPARKFLY_TEKKS_SEARCH.y, 1500.0f};
    plan.dialog_ids = ENTRY_DIALOGS;
    plan.dialog_count = static_cast<int>(sizeof(ENTRY_DIALOGS) / sizeof(ENTRY_DIALOGS[0]));
    plan.dialog_repeats = 2;
    plan.entry_path = ENTRY_PATH;
    plan.entry_path_count = static_cast<int>(sizeof(ENTRY_PATH) / sizeof(ENTRY_PATH[0]));
    plan.zone_point = {13097.0f, 26393.0f};
    plan.entry_map_id = MapIds::SPARKFLY_SWAMP;
    plan.target_map_id = MapIds::BOGROOT_GROWTHS_LVL1;
    return plan;
}

inline DungeonQuest::QuestNpcAnchor GetRewardNpcAnchor() {
    return {14618.0f, -17828.0f, 1500.0f};
}

inline DungeonQuest::DialogPlan GetRewardDialogPlan() {
    return {
        REWARD_DIALOGS,
        static_cast<int>(sizeof(REWARD_DIALOGS) / sizeof(REWARD_DIALOGS[0])),
        3,
    };
}

} // namespace GWA3::Bot::Froggy
