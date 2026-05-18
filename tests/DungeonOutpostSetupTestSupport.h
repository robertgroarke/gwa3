#pragma once

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/testing/TestFramework.h>

#include <string>

namespace OutpostSetup = GWA3::DungeonOutpostSetup;

namespace GWA3::TestStubs::MapMgr {
void SetHardModeEnabled(bool enabled);
bool HardModeEnabled();
} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::TestStubs::PartyMgr {
void ResetFlags();
void ResetHeroes();
uint32_t HeroIdAt(uint32_t index);
uint32_t HeroBehaviorAt(uint32_t index);
} // namespace GWA3::TestStubs::PartyMgr

namespace GWA3::TestStubs::PlayerMgr {
void Reset();
void SetPlayerName(const wchar_t* name);
} // namespace GWA3::TestStubs::PlayerMgr
