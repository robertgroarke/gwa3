#pragma once

#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/dungeon/DungeonItemPolicy.h>
#include <gwa3/dungeon/DungeonVendor.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/testing/TestFramework.h>

#include <cstdint>

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void SetPlayerAgent(float x, float y, float hp);
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
void SetNpcPlayerNumber(uint32_t agentId, uint16_t playerNumber);
uint32_t LastChangedTargetId();
uint32_t LastInteractedNpcId();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
uint32_t LastMovedItemId();
uint32_t LastMoveBagId();
uint32_t AcceptUnclaimedCount();
uint32_t LastAcceptUnclaimedBagIndex();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::TradeMgr {

void Reset();
void SetMerchantItemCount(uint32_t count);
void SetMerchantItemModel(uint32_t position, uint32_t modelId);
uint32_t TransactCount();
uint32_t LastTransactItemId();

} // namespace GWA3::TestStubs::TradeMgr

namespace GWA3::TestStubs::UIMgr {

void Reset();
void SetFrameVisible(uint32_t hash, bool visible);

} // namespace GWA3::TestStubs::UIMgr

namespace GWA3::TestStubs::CtoS {

void Reset();
uint32_t SendPacketCount();
uint32_t LastSendPacketHeader();
uint32_t LastSendPacketArg1();
uint32_t LastSendPacketArg2();

} // namespace GWA3::TestStubs::CtoS

namespace GWA3::Tests::DungeonVendorSupport {

inline void VendorNoWait(uint32_t) {
}

inline void VendorMove(float, float, float) {
}

inline bool VendorMoveResult(float, float, float) {
    return true;
}

} // namespace GWA3::Tests::DungeonVendorSupport
