#pragma once

#include <cstddef>
#include <cstdint>

#include <gwa3/dungeon/DungeonItemActions.h>
#include <gwa3/managers/MaintenanceMgr.h>

namespace GWA3::DungeonVendor {

using WaitFn = void(*)(uint32_t ms);
using MoveToPointFn = void(*)(float x, float y, float threshold);
using MoveToPointResultFn = bool(*)(float x, float y, float threshold);

struct MerchantContextOptions {
    uint32_t merchant_root_hash = 3613855137u;
    uint32_t change_target_delay_ms = 250u;
    uint32_t interact_attempts = 3u;
    uint32_t interact_delay_ms = 500u;
    uint32_t post_interact_delay_ms = 2500u;
    uint32_t wait_timeout_ms = 1500u;
    uint32_t wait_poll_ms = 100u;
};

struct MerchantContextNearCoordsOptions {
    uint16_t preferred_player_number = 0u;
    float candidate_move_threshold = 120.0f;
    const char* log_prefix = nullptr;
    MerchantContextOptions merchant = {};
};

struct NpcServiceOptions {
    float anchor_threshold = 350.0f;
    float search_radius = 900.0f;
    float agent_threshold = 120.0f;
    uint32_t interact_delay_ms = 1500u;
};

struct SellAtMerchantOptions {
    NpcServiceOptions npc = {};
    MerchantContextOptions merchant = {};
    DungeonItemActions::SellOptions sell = {};
};

struct DepositAtStorageOptions {
    NpcServiceOptions npc = {};
    DungeonItemActions::DepositOptions deposit = {};
};

struct MaintenanceLocation {
    uint32_t outpost_map_id = 0u;
    float merchant_x = 0.0f;
    float merchant_y = 0.0f;
    float merchant_move_threshold = 350.0f;
    float merchant_search_radius = 2500.0f;
    uint16_t merchant_player_number = 0u;
    float xunlai_chest_x = 0.0f;
    float xunlai_chest_y = 0.0f;
    float material_trader_x = 0.0f;
    float material_trader_y = 0.0f;
    uint16_t material_trader_player_number = 0u;
};

struct MaintenanceStateOptions {
    uint32_t fallback_outpost_map_id = 0u;
    uint32_t outpost_ready_timeout_ms = 60000u;
    uint32_t town_runtime_timeout_ms = 10000u;
    uint32_t retry_wait_ms = 500u;
    uint32_t post_maintenance_wait_ms = 500u;
    uint32_t critical_free_slots = 3u;
    uint32_t deposit_gold_keep_on_char = 10000u;
    DungeonItemActions::UseItemOptions dp_removal = {5000u};
    const char* log_prefix = nullptr;
    MoveToPointResultFn move_to_point = nullptr;
    WaitFn wait_ms = nullptr;
};

enum class MaintenanceStateResult {
    Done,
    NeedsMaintenance,
    Retry,
    Stop,
};

bool WaitForMerchantContext(uint32_t timeoutMs, WaitFn wait_ms = nullptr,
                            uint32_t merchant_root_hash = 3613855137u,
                            uint32_t poll_ms = 100u);
bool OpenMerchantContextWithLegacyPacket(uint32_t npcId, WaitFn wait_ms = nullptr,
                                         const MerchantContextOptions& options = {});
bool OpenMerchantContextNearCoords(float searchX, float searchY, float searchRadius,
                                   MoveToPointResultFn move_to_point,
                                   WaitFn wait_ms = nullptr,
                                   const MerchantContextNearCoordsOptions& options = {});
int SellItemsAtMerchant(float anchorX, float anchorY, MoveToPointFn move_to_point,
                        DungeonItemActions::ItemFilterFn should_sell,
                        WaitFn wait_ms = nullptr, const SellAtMerchantOptions& options = {});
int DepositItemsAtStorage(float anchorX, float anchorY, MoveToPointFn move_to_point,
                          DungeonItemActions::ItemFilterFn should_store,
                          WaitFn wait_ms = nullptr, const DepositAtStorageOptions& options = {});
MaintenanceMgr::Config BuildMaintenanceConfig(uint32_t outpost_map_id,
                                              const MaintenanceLocation& location);
bool OpenMaintenanceMerchantContext(const MaintenanceLocation& location,
                                    MoveToPointResultFn move_to_point,
                                    WaitFn wait_ms = nullptr,
                                    const char* log_prefix = nullptr);
MaintenanceStateResult RunMerchantMaintenanceState(uint32_t configured_outpost_map_id,
                                                   const MaintenanceLocation& location,
                                                   const MaintenanceStateOptions& options = {});
MaintenanceStateResult RunFullMaintenanceState(uint32_t configured_outpost_map_id,
                                               uint32_t* wipe_count,
                                               const MaintenanceLocation& location,
                                               const MaintenanceStateOptions& options = {});

} // namespace GWA3::DungeonVendor
