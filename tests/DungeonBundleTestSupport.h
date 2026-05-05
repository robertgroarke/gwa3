#pragma once

#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/testing/TestFramework.h>

#include <cstdint>

namespace AgentStubs = GWA3::TestStubs::AgentMgr;
namespace ItemStubs = GWA3::TestStubs::ItemMgr;

namespace GWA3::TestStubs::AgentMgr {

void ResetAgents();
void AddAgent(uint32_t agentId, float x, float y, uint32_t type);
void AddGadgetAgent(uint32_t agentId, float x, float y, uint32_t gadgetId);
void AddItemAgent(uint32_t agentId, float x, float y, uint32_t itemId, uint32_t owner);
void SetPlayerAgent(float x, float y, float hp);
void SetPlayerEquippedItems(uint16_t weaponItemId, uint16_t offhandItemId);
uint32_t LastInteractedSignpostId();
uint32_t LastChangedTargetId();
uint32_t MoveCount();
float LastMoveX();
float LastMoveY();
uint32_t SignpostInteractionCount();
uint32_t ActionInteractCount();

} // namespace GWA3::TestStubs::AgentMgr

namespace GWA3::TestStubs::ItemMgr {

void Reset();
void SetBagCapacity(uint32_t bagIndex, uint32_t slotCount);
void SetBagItem(uint32_t bagIndex, uint32_t slotIndex, uint32_t itemId, uint32_t modelId,
                uint8_t type, uint16_t quantity, uint16_t value, uint32_t interaction,
                uint16_t rarity);
uint32_t LastPickedItemAgentId();
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

} // namespace GWA3::TestStubs::UIMgr

namespace GWA3::Tests::DungeonBundle {

inline float g_lastMoveX = 0.0f;
inline float g_lastMoveY = 0.0f;
inline float g_lastMoveThreshold = 0.0f;
inline int g_moveCallCount = 0;

inline void ResetMoveRecorder() {
    g_lastMoveX = 0.0f;
    g_lastMoveY = 0.0f;
    g_lastMoveThreshold = 0.0f;
    g_moveCallCount = 0;
}

inline void RecordMoveToPoint(float x, float y, float threshold) {
    g_lastMoveX = x;
    g_lastMoveY = y;
    g_lastMoveThreshold = threshold;
    ++g_moveCallCount;
}

} // namespace GWA3::Tests::DungeonBundle
