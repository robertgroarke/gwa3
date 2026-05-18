#pragma once

#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/testing/TestFramework.h>

#include <cstddef>
#include <cstdint>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace QuestStubs = GWA3::TestStubs::QuestMgr;
namespace QuestRuntime = GWA3::DungeonQuestRuntime;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddAgent(uint32_t agentId, float x, float y, uint32_t type);
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance = 6u, float hp = 1.0f);
void SetPlayerAgent(float x, float y, float hp = 1.0f);
uint32_t ChangeTargetCount();
uint32_t LastChangedTargetId();
uint32_t LastInteractedNpcId();
uint32_t NpcInteractionCount();
uint32_t MoveCount();
float LastMoveX();
float LastMoveY();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::ItemMgr {

void Reset();
uint32_t LastPickedItemAgentId();

} // namespace GWA3::TestStubs::ItemMgr

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs();
void ResetQuestState();
void SetQuest(uint32_t questId, uint32_t logState = 1u);
void SetQuestGrantedOnDialog(uint32_t dialogId, uint32_t questId, uint32_t logState = 1u);
std::size_t DialogCount();
uint32_t DialogAt(std::size_t index);
uint32_t RequestQuestInfoCount();

} // namespace GWA3::TestStubs::QuestMgr

namespace GWA3::TestStubs::CtoS {

void Reset();
uint32_t SendPacketCount();
uint32_t LastSendPacketHeader();
uint32_t LastSendPacketArg1();

} // namespace GWA3::TestStubs::CtoS

namespace GWA3::TestStubs::MapMgr {

void Reset();
void SetMapId(uint32_t mapId);
void SetLoadingState(uint32_t loadingState);
void SetMoveSetsMapId(uint32_t mapId);
void SetMoveSetsMapIdOnPoint(float x, float y, uint32_t mapId);

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::TestStubs::DialogMgr {

void Reset();
void SetDialogState(bool open, uint32_t senderAgentId, uint32_t buttonCount);
void SetDialogButton(uint32_t index, uint32_t dialogId, uint32_t buttonIcon);
void SetAutoDialogOnNpcInteract(bool enabled, uint32_t buttonCount = 1u);

} // namespace GWA3::TestStubs::DialogMgr
