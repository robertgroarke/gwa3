#pragma once

#include <gwa3/game/ItemModelIds.h>
#include <gwa3/game/SkillIds.h>

#include <cstddef>
#include <cstdint>

namespace GWA3 {
struct Item;
}

namespace GWA3::DungeonItemActions {

using WaitFn = void(*)(uint32_t ms);
using ItemFilterFn = bool(*)(const Item*);

struct IdentifyOptions {
    uint32_t normal_kit_model = GWA3::ItemModelIds::IDENTIFICATION_KIT;
    uint32_t superior_kit_model = GWA3::ItemModelIds::SUPERIOR_IDENTIFICATION_KIT;
    uint32_t delay_ms = 500u;
};

struct SalvageOptions {
    uint32_t salvage_kit_model = GWA3::ItemModelIds::SUPERIOR_SALVAGE_KIT;
    uint32_t expert_salvage_kit_model = GWA3::ItemModelIds::EXPERT_SALVAGE_KIT;
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

struct ConsetUseOptions {
    uint32_t armor_effect_skill_id = GWA3::SkillIds::ARMOR_OF_SALVATION_ITEM_EFFECT;
    uint32_t essence_effect_skill_id = GWA3::SkillIds::ESSENCE_OF_CELERITY_ITEM_EFFECT;
    uint32_t grail_effect_skill_id = GWA3::SkillIds::GRAIL_OF_MIGHT_ITEM_EFFECT;
    uint32_t armor_model_id = GWA3::ItemModelIds::ARMOR_OF_SALVATION;
    uint32_t essence_model_id = GWA3::ItemModelIds::ESSENCE_OF_CELERITY;
    uint32_t grail_model_id = GWA3::ItemModelIds::GRAIL_OF_MIGHT;
    UseItemOptions use = {};
};

struct ConsetUseResult {
    bool used_armor = false;
    bool used_essence = false;
    bool used_grail = false;
    bool full_active = false;
};

struct ConsetUseAttemptResult {
    bool attempted = false;
    ConsetUseResult consets = {};
};

struct DpRemovalUseResult {
    bool attempted = false;
    uint32_t previous_wipe_count = 0u;
    uint32_t used_model_id = 0u;
    bool wipe_count_reset = false;
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
ConsetUseResult UseFullConsetIfNeeded(uint32_t agent_id, WaitFn wait_ms = nullptr,
                                      const ConsetUseOptions& options = {});
ConsetUseAttemptResult UseConsetsForAgentIfEnabled(
    bool enabled,
    uint32_t agent_id,
    WaitFn wait_ms = nullptr,
    const ConsetUseOptions& options = {});
ConsetUseAttemptResult UseConsetsForCurrentPlayerIfEnabled(
    bool enabled,
    WaitFn wait_ms = nullptr,
    const ConsetUseOptions& options = {},
    const char* log_prefix = nullptr);
uint32_t UseFirstItemByModel(const uint32_t* model_ids, std::size_t model_count,
                             WaitFn wait_ms = nullptr, const UseItemOptions& options = {});
DpRemovalUseResult UseDpRemovalSweetIfNeeded(uint32_t* wipe_count,
                                             WaitFn wait_ms = nullptr,
                                             const UseItemOptions& options = {});

} // namespace GWA3::DungeonItemActions
