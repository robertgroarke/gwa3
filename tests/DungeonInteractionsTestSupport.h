#pragma once

#include <cstddef>
#include <cstdint>

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
void SetHeldBundleItemId(uint32_t itemId);
uint32_t LastDroppedItemId();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::UIMgr {

void Reset();
void SetActionKeyDownResult(bool result);
void SetPerformUiActionResult(bool result);
uint32_t LastActionKeyDown();
uint32_t ActionKeyDownCount();
uint32_t LastPerformUiAction();
uint32_t PerformUiActionCount();
uint32_t LastPerformUiActionDirect();
uint32_t PerformUiActionDirectCount();

} // namespace GWA3::TestStubs::UIMgr

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddAgent(uint32_t agentId, float x, float y, uint32_t type);
void AddItemAgent(uint32_t agentId, float x, float y, uint32_t itemId, uint32_t owner);
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
uint32_t LastInteractedNpcId();
uint32_t LastInteractedSignpostId();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::DialogMgr {

void Reset();
void SetDialogState(bool open, uint32_t senderAgentId, uint32_t buttonCount);
void SetAutoDialogOnNpcInteract(bool enabled, uint32_t buttonCount);

} // namespace GWA3::TestStubs::DialogMgr

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs();
std::size_t DialogCount();
uint32_t DialogAt(std::size_t index);

} // namespace GWA3::TestStubs::QuestMgr

namespace GWA3::TestStubs::CtoS {

void Reset();
uint32_t SendPacketCount();
uint32_t LastSendPacketHeader();
uint32_t LastSendPacketArg1();
uint32_t LastSendPacketArg2();

} // namespace GWA3::TestStubs::CtoS

namespace GWA3::Tests::DungeonInteractionsSupport {

void NoInteractionWait(uint32_t ms);
bool StopWhenNpcDialogOpen(uint32_t npcId, void* userData);

} // namespace GWA3::Tests::DungeonInteractionsSupport
