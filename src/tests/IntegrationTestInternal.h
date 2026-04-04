#pragma once

#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/SkillMgr.h>

#include <cstdint>
#include <functional>

namespace GWA3::SmokeTest {

// Shared integration-test enums and snapshots.

enum class MapRegionType : uint32_t {
    AllianceBattle = 0,
    Arena = 1,
    ExplorableZone = 2,
    GuildBattleArea = 3,
    GuildHall = 4,
    MissionOutpost = 5,
    CooperativeMission = 6,
    CompetitiveMission = 7,
    EliteMission = 8,
    Challenge = 9,
    Outpost = 10,
    ZaishenBattle = 11,
    HeroesAscent = 12,
    City = 13,
    MissionArea = 14,
    HeroBattleOutpost = 15,
    HeroBattleArea = 16,
    EotnMission = 17,
    Dungeon = 18,
    Marketplace = 19,
};

struct InventorySnapshot {
    uint32_t count = 0;
    uint32_t goldCharacter = 0;
    uint32_t goldStorage = 0;
    uint64_t itemIdSum = 0;
    uint64_t modelIdSum = 0;
    uint64_t quantitySum = 0;
};

struct SkillTestCandidate {
    uint32_t slot = 0;
    uint32_t skillId = 0;
    uint32_t targetId = 0;
    uint8_t targetType = 0;
    uint8_t energyCost = 0;
    uint32_t type = 0;
    uint32_t baseRecharge = 0;
    float activation = 0.0f;
};

// Common reporting and runtime helpers used across extracted test modules.

void IntReport(const char* fmt, ...);
void IntCheck(const char* name, bool condition);
void IntSkip(const char* name, const char* reason);

bool WaitFor(const char* desc, int timeoutMs, const std::function<bool()>& predicate);
uint32_t ReadMapId();
uint32_t ReadMyId();
bool TryReadAgentPosition(uint32_t agentId, float& x, float& y);
AgentLiving* GetAgentLivingRaw(uint32_t agentId);
uint32_t GetPlayerTypeMap();
uint32_t GetPlayerModelState();
bool IsPlayerRuntimeReady(bool requireSkillbar);
void DumpCurrentTargetCandidates(const char* phase, uint32_t requestedTarget);
uint32_t FindNearbyNpcLikeAgent(float maxDistance);
uint32_t GetCurrentEnergyPoints();
bool MovePlayerNear(float x, float y, float threshold, int timeoutMs);
bool WaitForStablePlayerState(int timeoutMs = 3000);
bool WaitForPlayerWorldReady(int timeoutMs = 5000);
InventorySnapshot CaptureInventorySnapshot();
bool InventoryChangedMeaningfully(const InventorySnapshot& before, const InventorySnapshot& after);
AgentItem* FindGroundItemByAgentId(uint32_t agentId);
AgentItem* FindNearbyGroundItem(float maxDistance);
const char* DescribeMapRegionType(uint32_t type);
bool IsSkillCastMapType(uint32_t type);
bool TryForceNearbyLootDrop();
void DumpSkillbarForSkillTest();
bool TryChooseSkillTestCandidate(SkillTestCandidate& out);

// Session, login, and trader/dialog flow (IntegrationTestSession.cpp).

bool TestCharSelectLogin();
bool TestHeroSetup();
bool TestNpcDialog();
bool TestMerchantQuote();
bool TestMapTravel();

// Gameplay and combat interactions (IntegrationTestGameplay.cpp).

bool TestMovement();
bool TestTargeting();
bool TestSkillActivation();
bool TestLootPickup();

// World-state transitions and explorable/outpost interactions (IntegrationTestWorld.cpp).

bool TestExplorableEntry();
bool TestHeroFlagging();
bool TestHardModeToggle();
bool TestReturnToOutpost();
bool TestChatWriteLocal();
bool TestSkillbarDataValidation();
bool TestPartyState();
bool TestTargetLogHook();

// Runtime introspection and read-only validation (IntegrationTestIntrospection.cpp).

bool TestPlayerData();
bool TestCameraIntrospection();
bool TestClientInfo();
bool TestInventoryIntrospection();
bool TestAgentArrayEnumeration();
bool TestUIFrameValidation();
bool TestAreaInfoValidation();

// Runtime systems coverage (IntegrationTestSystems.cpp).

bool TestGuildData();
bool TestMapStateQueries();
bool TestPingStability();
bool TestWeaponSetValidation();
bool TestAgentDistanceCrossCheck();
bool TestCameraControls();
bool TestRenderingToggle();
bool TestEffectArray();
bool TestStoCHook();

// Offset and patch validation (IntegrationTestOffsets.cpp).

bool TestPostProcessEffectOffset();
bool TestGwEndSceneOffset();
bool TestItemClickOffset();
bool TestRequestQuestInfoOffset();
bool TestFriendListOffsets();
bool TestDrawOnCompassOffset();
bool TestChatColorOffsets();
bool TestCameraUpdateBypassPatch();
bool TestTradeOffsets();
bool TestLevelDataBypassPatch();
bool TestMapPortBypassPatch();
bool TestSendChatOffset();
bool TestAddToChatLogOffset();
bool TestSkipCinematicOffset();

// Advanced workflow tests (GWA3-074..089) — IntegrationTestAdvanced.cpp
bool TestItemMove();
bool TestGoldTransfer();
bool TestMemAllocFree();
bool TestLoadSkillbar();
bool TestPartyManagement();
bool TestTitleManagement();
bool TestCallbackRegistry();
bool TestGameThreadCallbacks();
bool TestStoCPacketTypes();
bool TestQuestManagement();
bool TestUIFrameInteraction();
bool TestAgentInteraction();
bool TestCameraFOV();
bool TestPersonalDir();

} // namespace GWA3::SmokeTest
