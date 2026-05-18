// Consolidated small dungeon-oriented integration feature wrappers.
// --- src/tests/IntegrationTestFroggyFeature.cpp ---
// IntegrationTestFroggyFeature.cpp - Phased Froggy feature tests
// Run via: injector.exe --test-froggy
//
// Self-setting-up: travels to outpost, adds heroes, opens merchant,
// enters explorable, finds enemies, then returns. No manual setup needed.

#include "IntegrationTestInternal.h"
#include <gwa3/core/Log.h>
#include <gwa3/core/SmokeTest.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/dungeon/DungeonEffects.h>
#include <bots/froggy/FroggyHM.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/Party.h>
#include <gwa3/game/Skill.h>
#include <gwa3/game/SkillIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/game/Effect.h>
#include <gwa3/game/Title.h>
#include <gwa3/managers/PlayerMgr.h>

#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace GWA3::SmokeTest {

static int s_passed = 0;
static int s_failed = 0;
static int s_skipped = 0;
static bool s_isolatedExplorableFlaggingMode = false;
static bool s_enableInvasiveSparkflyCombatProofs = false;
static bool s_preferDirectTekksStagingForDebug = false;

#include "IntegrationTestFroggyFeatureConfig.h"

#include "IntegrationTestFroggyFeatureSupport.h"

#include "IntegrationTestFroggyFeatureDungeonPhases.h"

#include "IntegrationTestFroggyFeatureCombatProofs.h"

#include "IntegrationTestFroggyFeatureMerchantMaintenance.h"

#include "IntegrationTestFroggyFeatureFeatureRunnerSupport.h"

#include "IntegrationTestFroggyFeatureFeatureRunner.h"

#include "IntegrationTestFroggyFeatureSparkflyRouteRunner.h"

#include "IntegrationTestFroggyFeatureEntrypoints.h"

} // namespace GWA3::SmokeTest

// --- src/tests/IntegrationTestMapTravel.cpp ---
// Outpost travel integration feature group.

#include "IntegrationTestInternal.h"

#include <gwa3/game/MapIds.h>
#include <gwa3/managers/MapMgr.h>

namespace GWA3::SmokeTest {

bool TestMapTravel() {
    IntReport("===  Outpost Travel ===");

    const uint32_t startMapId = ReadMapId();
    if (startMapId == 0) {
        IntSkip("Map travel", "Not in game");
        return false;
    }

    const uint32_t targetMapId =
        (startMapId == MapIds::GADDS_ENCAMPMENT) ? MapIds::LONGEYES_LEDGE : MapIds::GADDS_ENCAMPMENT;
    IntReport("  Traveling from map %u to map %u...", startMapId, targetMapId);

    IntReport("  Calling MapMgr::Travel...");
    MapMgr::Travel(targetMapId);
    IntReport("  MapMgr::Travel returned, waiting for transition...");

    const bool transitioned = WaitFor("MapID changes to target outpost", 60000, [startMapId, targetMapId]() {
        const uint32_t mapId = ReadMapId();
        return mapId != 0 && mapId != startMapId && mapId == targetMapId;
    });
    IntCheck("Outpost travel reached target map", transitioned);

    if (!transitioned) {
        IntReport("");
        return false;
    }

    const bool myIdReady = WaitFor("MyID valid after outpost travel", 30000, []() {
        return ReadMyId() > 0;
    });
    IntCheck("MyID valid after travel", myIdReady);

    const uint32_t endMapId = ReadMapId();
    const uint32_t endMyId = ReadMyId();
    IntReport("  After travel: MapID=%u, MyID=%u", endMapId, endMyId);

    IntReport("");
    return transitioned && myIdReady;
}

} // namespace GWA3::SmokeTest
