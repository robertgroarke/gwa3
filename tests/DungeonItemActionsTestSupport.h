#pragma once

#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/dungeon/DungeonItemActions.h>
#include <gwa3/dungeon/DungeonItemPolicy.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/testing/TestFramework.h>

#include <cstdint>

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
uint32_t LastUsedItemId();
uint32_t LastIdentifiedItemId();
uint32_t LastIdentifyKitId();
uint32_t IdentifyCount();
uint32_t LastSalvagedItemId();
uint32_t LastSalvageKitId();
uint32_t SalvageOpenCount();
uint32_t SalvageDoneCount();
uint32_t LastMovedItemId();
uint32_t LastMoveBagId();
uint32_t LastMoveSlot();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::TradeMgr {

uint32_t LastTransactType();
uint32_t LastTransactQuantity();
uint32_t LastTransactItemId();
uint32_t TransactCount();

} // namespace GWA3::TestStubs::TradeMgr

namespace GWA3::TestStubs::EffectMgr {

void Reset();
void AddEffectForAgent(uint32_t agentId, uint32_t skillId, float duration);

} // namespace GWA3::TestStubs::EffectMgr

namespace GWA3::Tests::DungeonItemActionsSupport {

inline void ItemActionsNoWait(uint32_t) {
}

inline bool ShouldIdentifyGoldOrPurple(const GWA3::Item* item) {
    if (!item || (item->interaction & 0x1u) != 0u) {
        return false;
    }
    const uint16_t rarity = GWA3::DungeonInventory::GetItemRarity(item);
    return rarity == GWA3::DungeonInventory::RARITY_GOLD ||
           rarity == GWA3::DungeonInventory::RARITY_PURPLE;
}

} // namespace GWA3::Tests::DungeonItemActionsSupport
