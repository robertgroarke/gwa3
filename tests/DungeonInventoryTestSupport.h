#pragma once

#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/game/Item.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
void ClearBagItem(uint32_t bagIndex, uint32_t slotIndex);
uint32_t LastDroppedItemId();

} // namespace GWA3::TestStubs::ItemMgr
