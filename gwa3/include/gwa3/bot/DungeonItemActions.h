#pragma once

#include <cstdint>

namespace GWA3 {
struct Item;
}

namespace GWA3::Bot::DungeonItemActions {

using WaitFn = void(*)(uint32_t ms);
using ItemFilterFn = bool(*)(const Item*);

struct IdentifyOptions {
    uint32_t normal_kit_model = 2992u;
    uint32_t superior_kit_model = 5899u;
    uint32_t delay_ms = 500u;
};

struct SalvageOptions {
    uint32_t salvage_kit_model = 2993u;
    uint32_t expert_salvage_kit_model = 2991u;
    uint32_t open_delay_ms = 800u;
    uint32_t materials_delay_ms = 800u;
    uint32_t done_delay_ms = 300u;
};

struct SellOptions {
    uint32_t transact_type = 0xBu;
    uint32_t delay_ms = 200u;
    uint32_t extra_delay_ms = 0u;
};

struct DepositOptions {
    uint32_t first_storage_bag = 8u;
    uint32_t last_storage_bag = 16u;
    uint32_t delay_ms = 500u;
};

struct UseItemOptions {
    uint32_t delay_ms = 1000u;
};

int IdentifyItems(ItemFilterFn should_identify, WaitFn wait_ms = nullptr,
                  const IdentifyOptions& options = {});
int SalvageItems(ItemFilterFn should_salvage, WaitFn wait_ms = nullptr,
                 const SalvageOptions& options = {});
int SellItems(ItemFilterFn should_sell, WaitFn wait_ms = nullptr,
              const SellOptions& options = {});
int DepositItemsToStorage(ItemFilterFn should_store, WaitFn wait_ms = nullptr,
                          const DepositOptions& options = {});
bool UseItemByModel(uint32_t model_id, WaitFn wait_ms = nullptr,
                    const UseItemOptions& options = {});
bool UseItemIfEffectMissing(uint32_t agent_id, uint32_t effect_skill_id, uint32_t model_id,
                            WaitFn wait_ms = nullptr, const UseItemOptions& options = {});

} // namespace GWA3::Bot::DungeonItemActions
