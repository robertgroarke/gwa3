#pragma once

#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/testing/TestFramework.h>

#include <cstddef>
#include <cstdint>

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs();
std::size_t DialogCount();
uint32_t DialogAt(std::size_t index);

} // namespace GWA3::TestStubs::QuestMgr

namespace GWA3::TestStubs::DialogMgr {

void Reset();
void SetDialogState(bool open, uint32_t senderAgentId, uint32_t buttonCount);
void SetDialogButton(uint32_t index, uint32_t dialogId, uint32_t buttonIcon);

} // namespace GWA3::TestStubs::DialogMgr
