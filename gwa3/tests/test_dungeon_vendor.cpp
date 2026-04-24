#include <gwa3/bot/DungeonInventory.h>
#include <gwa3/bot/DungeonItemPolicy.h>
#include <gwa3/bot/DungeonVendor.h>
#include <gwa3/game/Agent.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/testing/TestFramework.h>

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void SetPlayerAgent(float x, float y, float hp);
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
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

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::TradeMgr {

void Reset();
void SetMerchantItemCount(uint32_t count);
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

using namespace GWA3::Bot::DungeonVendor;

namespace {

void VendorNoWait(uint32_t) {
}

void VendorMove(float, float, float) {
}

} // namespace

GWA3_TEST(dungeon_vendor_wait_for_merchant_context_accepts_visible_frame, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::UIMgr::SetFrameVisible(3613855137u, true);

    GWA3_ASSERT(WaitForMerchantContext(1u, &VendorNoWait));
})

GWA3_TEST(dungeon_vendor_open_merchant_context_uses_legacy_packet_path, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::TradeMgr::SetMerchantItemCount(1u);

    GWA3_ASSERT(OpenMerchantContextWithLegacyPacket(55u, &VendorNoWait));
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastChangedTargetId(), 55u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::SendPacketCount(), 3u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketHeader(), GWA3::Packets::INTERACT_NPC);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketArg1(), 55u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::CtoS::LastSendPacketArg2(), 0u);
})

GWA3_TEST(dungeon_vendor_sell_items_at_merchant_moves_opens_and_sells, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::UIMgr::Reset();
    GWA3::TestStubs::CtoS::Reset();
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(77u, 25.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::TradeMgr::SetMerchantItemCount(1u);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 300u, 18001u,
                                         27u, 1u, 75u, 0x1u,
                                         GWA3::Bot::DungeonInventory::RARITY_BLUE);

    const int sold = SellItemsAtMerchant(0.0f, 0.0f, &VendorMove,
                                         &GWA3::Bot::DungeonItemPolicy::ShouldSellItem,
                                         &VendorNoWait);
    GWA3_ASSERT_EQ(sold, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::TransactCount(), 1u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::TradeMgr::LastTransactItemId(), 300u);
})

GWA3_TEST(dungeon_vendor_deposit_items_at_storage_moves_interacts_and_deposits, {
    GWA3::TestStubs::TradeMgr::Reset();
    GWA3::TestStubs::ItemMgr::Reset();
    GWA3::TestStubs::AgentMgr::ResetAgents();
    GWA3::TestStubs::AgentMgr::SetPlayerAgent(0.0f, 0.0f, 1.0f);
    GWA3::TestStubs::AgentMgr::AddNpc(88u, 25.0f, 0.0f, 6u, 1.0f);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(1u, 1u);
    GWA3::TestStubs::ItemMgr::SetBagCapacity(8u, 2u);
    GWA3::TestStubs::ItemMgr::SetBagItem(1u, 0u, 400u, 930u,
                                         GWA3::Bot::DungeonItemPolicy::ITEM_TYPE_MATERIAL,
                                         1u, 0u, 0u,
                                         GWA3::Bot::DungeonInventory::RARITY_GOLD);

    const int deposited = DepositItemsAtStorage(0.0f, 0.0f, &VendorMove,
                                                &GWA3::Bot::DungeonItemPolicy::ShouldStoreItem,
                                                &VendorNoWait);
    GWA3_ASSERT_EQ(deposited, 1);
    GWA3_ASSERT_EQ(GWA3::TestStubs::AgentMgr::LastInteractedNpcId(), 88u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMovedItemId(), 400u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::ItemMgr::LastMoveBagId(), 8u);
})
