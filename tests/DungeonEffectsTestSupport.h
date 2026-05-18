#pragma once

#include <gwa3/dungeon/DungeonEffects.h>
#include <bots/froggy/FroggyHM.h>
#include <gwa3/game/SkillIds.h>
#include <gwa3/game/Title.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/testing/TestFramework.h>

#include <cstddef>

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddNpc(uint32_t agentId, float x, float y, uint8_t allegiance, float hp);
uint32_t LastInteractedNpcId();
uint32_t NpcInteractionCount();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs();
std::size_t DialogCount();
uint32_t DialogAt(std::size_t index);
void SetBlessingDialogEffect(uint32_t dialogId, uint32_t skillId);

} // namespace GWA3::TestStubs::QuestMgr

namespace GWA3::TestStubs::EffectMgr {

void Reset();
void AddEffect(uint32_t skillId);

} // namespace GWA3::TestStubs::EffectMgr

namespace GWA3::TestStubs::PlayerMgr {
void Reset();
} // namespace GWA3::TestStubs::PlayerMgr

namespace GWA3::TestStubs::DialogMgr {

void Reset();
uint32_t ShutdownCount();
uint32_t InitializeCount();

} // namespace GWA3::TestStubs::DialogMgr

namespace GWA3::Tests::DungeonEffects {

inline uint32_t g_effectWaitMs = 0u;

inline void RecordEffectWait(uint32_t ms) {
    g_effectWaitMs += ms;
}

} // namespace GWA3::Tests::DungeonEffects
