#pragma once

#include <cstdint>

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetGold(uint32_t charGold, uint32_t storageGold);
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
uint32_t LastPickedItemAgentId();
uint32_t LastDroppedItemId();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void SetPlayerAgent(float x, float y, float hp);
void AddItemAgent(uint32_t agentId, float x, float y, uint32_t itemId, uint32_t owner);
void AddGadgetAgent(uint32_t agentId, float x, float y, uint32_t gadgetId);
uint32_t LastInteractedSignpostId();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::MapMgr {

void SetLoadingState(uint32_t loadingState);

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::Tests::DungeonLootSupport {

void MoveNear(float x, float y, float range);

void ResetBossKeyCallbacks();
void RecordBossKeyCombatMove(float x, float y, float fightRange);
int RecordBossKeyPickupNearbyLoot(float maxRange);
bool RecordBossKeyOpenDoor(float x, float y);
bool RecordBundleOpen(float x, float y, float range);

void SetBossKeyOpenDoorResult(bool result);
void SetBundleOpenResult(bool result);

uint32_t CombatMoveCount();
uint32_t PickupNearbyCount();
uint32_t OpenDoorCount();
uint32_t BundleOpenCount();
float LastCombatMoveX();
float LastCombatMoveY();
float LastCombatMoveRange();
float LastPickupRange();
float LastDoorX();
float LastDoorY();

} // namespace GWA3::Tests::DungeonLootSupport
