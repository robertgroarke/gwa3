#include <bots/froggy/FroggyHM.h>
#include <bots/froggy/FroggyHMConfig.h>
#include <bots/froggy/FroggyHMRoutePolicy.h>
#include <bots/froggy/FroggyHMRoutes.h>
#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/dungeon/DungeonDiagnostics.h>
#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/dungeon/DungeonEffects.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonInventory.h>
#include <gwa3/dungeon/DungeonItemActions.h>
#include <gwa3/dungeon/DungeonItemPolicy.h>
#include <gwa3/dungeon/DungeonLoot.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <gwa3/dungeon/DungeonRoute.h>
#include <gwa3/dungeon/DungeonRuntime.h>
#include <gwa3/dungeon/DungeonSkill.h>
#include <gwa3/dungeon/DungeonVendor.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/game/Quest.h>

#include <Windows.h>
#include <cmath>
#include <cstdlib>
#include <cstdarg>
#include <cstddef>
#include <cstdio>

namespace GWA3::Bot::Froggy {

using DungeonDiagnostics::CollectNearbyNpcCandidates;
using DungeonDiagnostics::LogAgentIdentity;
using DungeonDiagnostics::LogNearbyNpcCandidates;
using DungeonDiagnostics::LogNearbySignposts;
using DungeonDiagnostics::NearbyNpcCandidate;

using Waypoint = DungeonRoute::Waypoint;

// ===== Run Statistics =====
static uint32_t s_runCount = 0;
static uint32_t s_failCount = 0;
static uint32_t s_wipeCount = 0;
static DWORD s_runStartTime = 0;
static DWORD s_totalStartTime = 0;
static DWORD s_bestRunTime = 0xFFFFFFFF;
static int s_tekksQuestEntryFailureCount = 0;
static DungeonLoopTelemetry s_dungeonLoopTelemetry = {};

using DungeonRuntime::IsDead;
using DungeonRuntime::IsMapLoaded;
using DungeonRuntime::WaitMs;
using DungeonNavigation::WaitForLocalPositionSettle;

// ===== Forward declarations =====
static int PickupNearbyLoot(float maxRange = DEFAULT_LOOT_PICKUP_RADIUS);
static bool AcquireBogrootBossKey();
static void UseDpRemovalIfNeeded();
static bool OpenChestAt(float chestX, float chestY, float searchRadius = DEFAULT_CHEST_OPEN_RADIUS);
static void FollowWaypoints(const Waypoint* wps, int count, bool ignoreBotRunning);
static bool RefreshTekksQuestReadyForDungeonEntry(
    const char* label,
    uint32_t refreshDelayMs = TEKKS_QUEST_REFRESH_DELAY_MS);

#include "FroggyHMCombatState.h"
#include "FroggyHMCombatTrace.h"
#include "FroggyHMCombatSkillUse.h"

#include "FroggyHMMovementCombatRuntime.h"
#include "FroggyHMAggroCombatRuntime.h"
#include "FroggyHMTransitionRuntime.h"
#include "FroggyHMLocalClearSupport.h"
#include "FroggyHMLocalClearRuntime.h"
#include "FroggyHMAggroMoveRuntime.h"

#include "FroggyHMTransitions.h"

static void GrabDungeonBlessing(float shrineX, float shrineY); // forward decl
static bool OpenDungeonDoorAt(float doorX, float doorY);       // forward decl
#include "FroggyHMBossWaypointHandler.h"
#include "FroggyHMWaypointTraversal.h"

#include "FroggyHMTekksEntry.h"

#include "FroggyHMLootAndDoors.h"

#include "FroggyHMBlessing.h"

#include "FroggyHMMaintenance.h"

#include "FroggyHMDungeonLoopRuntime.h"

#include "FroggyHMTownStates.h"
#include "FroggyHMDungeonState.h"
#include "FroggyHMMerchantStates.h"
#include "FroggyHMErrorState.h"

// ===== Registration =====

void Register() {
    Bot::RegisterStateHandler(BotState::CharSelect, HandleCharSelect);
    Bot::RegisterStateHandler(BotState::InTown, HandleTownSetup);
    Bot::RegisterStateHandler(BotState::Traveling, HandleTravel);
    Bot::RegisterStateHandler(BotState::InDungeon, HandleDungeon);
    Bot::RegisterStateHandler(BotState::Looting, HandleLoot);
    Bot::RegisterStateHandler(BotState::Merchant, HandleMerchant);
    Bot::RegisterStateHandler(BotState::Maintenance, HandleMaintenance);
    Bot::RegisterStateHandler(BotState::Error, HandleError);

    // Default route config. Hero templates are resolved from config/gwa3.ini.
    auto& cfg = Bot::GetConfig();
    cfg.hero_config_file.clear();
    for (uint32_t& hero_id : cfg.hero_ids) {
        hero_id = 0u;
    }
    cfg.hard_mode = true;
    cfg.target_map_id = MapIds::BOGROOT_GROWTHS_LVL1;
    cfg.outpost_map_id = MapIds::GADDS_ENCAMPMENT;
    cfg.bot_module_name = "FroggyHM";

    s_runCount = 0;
    s_failCount = 0;
    s_wipeCount = 0;
    s_totalStartTime = GetTickCount();

    LogBot("Froggy HM module registered (hard mode)");
}

#include "FroggyHMDebugMovement.h"
#include "FroggyHMDebugCombat.h"
#include "FroggyHMDebugCombatDump.h"
#include "FroggyHMDebugDungeonLoop.h"

} // namespace GWA3::Bot::Froggy
