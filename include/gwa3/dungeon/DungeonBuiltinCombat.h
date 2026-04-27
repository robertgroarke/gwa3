#pragma once

#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonQuest.h>

#include <cstdint>

namespace GWA3::DungeonBuiltinCombat {

void WaitMs(uint32_t ms);
bool IsPlayerOrPartyDead();
bool IsCurrentMapLoaded();
void QueueAggroMove(float x, float y);
void AutoAttackTarget(uint32_t targetId);
void FightTargetWithBuiltinCombat(uint32_t targetId);
void FightTargetWithPriorityBuiltinCombat(uint32_t targetId);
DungeonCombat::CombatCallbacks MakeCombatCallbacks();
void ConfigureBuiltinAggroAdvanceOptions(
    DungeonCombat::AggroAdvanceOptions& options,
    uint32_t timeoutMs,
    bool pickupAfterClear);
bool MoveToPointWithAggro(
    float x,
    float y,
    uint32_t mapId,
    float tolerance = 250.0f,
    float fightRange = 1200.0f,
    uint32_t timeoutMs = 30000u);
bool FollowTravelPathWithAggro(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t mapId,
    float tolerance = 250.0f,
    float fightRange = 1200.0f,
    uint32_t timeoutMs = 30000u);

} // namespace GWA3::DungeonBuiltinCombat
