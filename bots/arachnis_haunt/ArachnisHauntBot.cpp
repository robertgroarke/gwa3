#include <bots/arachnis_haunt/ArachnisHauntBot.h>

#include <bots/arachnis_haunt/ArachnisHaunt.h>
#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/dungeon/DungeonEffects.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonItemActions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <gwa3/dungeon/DungeonSkill.h>
#include <gwa3/dungeon/DungeonVendor.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/Party.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace GWA3::Bot::ArachnisHauntBot {

using namespace GWA3::Bot;
using namespace GWA3::Bot::ArachnisHaunt;

namespace {

constexpr uint32_t kAsuraFlameStaffModelId = 24350u;
constexpr uint32_t kAsuraFlameStaffEffectSkillId = 2429u;
constexpr uint32_t kArachnisQuestId = 0x31Au;
constexpr float kGaddsMerchantX = -8374.0f;
constexpr float kGaddsMerchantY = -22491.0f;
constexpr float kGaddsXunlaiX = -10481.0f;
constexpr float kGaddsXunlaiY = -22787.0f;
constexpr float kGaddsMaterialTraderX = -9097.0f;
constexpr float kGaddsMaterialTraderY = -23353.0f;
constexpr uint16_t kGaddsMerchantPlayerNumber = 6060u;
constexpr uint16_t kGaddsMaterialTraderPlayerNumber = 6763u;

uint32_t s_runCount = 0u;
uint32_t s_failCount = 0u;
DWORD s_runStartTime = 0u;
DWORD s_bestRunTime = 0xFFFFFFFFu;
bool s_outpostSetupApplied = false;
DWORD s_lastArachnisDefensePulse = 0u;

void LogArachnisMain(const char* fmt, ...) {
    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    Log::Info("ArachnisDbg: %s", message);
}

bool HasHeldBundle();
bool HasEffectOnlyFlameStaffBundle();
bool EnsureHeldBundleNearPlayer();
bool IsPlayerOrPartyDead();
bool IsCurrentMapLoadedForTravel();
bool IsPlayerDeadForStaffSafeSkills();
void StaffSafeWaitMs(uint32_t ms);
void StaffSafeNoAutoAttack(uint32_t targetId);
bool RunStaffSafeCombatAssist(float range, uint32_t budgetMs, const char* context);
bool WaitForHeldBundleState(bool expectedHeld, uint32_t timeoutMs, const char* context);
bool WaitForMovementToSettle(uint32_t timeoutMs, const char* context);
bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs, const char* context = nullptr);
bool FinalizeZoneTransition(
    uint32_t targetMapId,
    const char* context,
    uint32_t settleMs = 3000u,
    uint32_t readyTimeoutMs = 30000u);
bool HandleArachnisAggroWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    int waypointIndex,
    DungeonRoute::WaypointLabelKind labelKind,
    DungeonCombat::AggroWaypointPhase phase,
    void* userData);
bool PickUpObjectiveBundle(const FlameStaffObjective& objective, uint32_t mapId);
bool RecoverFlameStaffAfterRevive(
    const char* routeName,
    int waypointIndex,
    uint32_t mapId,
    const char* context);
DungeonNavigation::RouteFollowResult FollowRouteWhileCarryingBundle(
    const RouteDefinition& route,
    const char* context,
    const DungeonNavigation::RouteFollowOptions& options);
bool TryDropFlameStaffBundle(const char* context);
void LogHeldBundleState(
    const char* context,
    const DungeonRoute::Waypoint* waypoint = nullptr,
    int waypointIndex = -1);
void LogBundleCarryTarget(
    const char* context,
    int legIndex,
    int legCount,
    const DungeonQuest::TravelPoint& point);
void SuspendTransitionSensitiveHooks(const char* context);
void ResumeTransitionSensitiveHooks(const char* context);
DungeonQuestRuntime::DialogExecutionOptions BuildQuestDialogOptions();
bool NeedsArachnisMaintenance();
bool RunArachnisTownMaintenanceIfNeeded();
bool EnsureArachnisOutpostSetup(BotConfig& cfg);
bool ValidateArachnisLiveHeroProfessions(const BotConfig& cfg, const char* context);
bool WaitForArachnisLiveHeroSetup(const BotConfig& cfg, const char* context, uint32_t timeoutMs);
void UseArachnisConsetsIfEnabled(const BotConfig& cfg, const char* context);

struct ZoneTransitionAttemptResult {
    bool zoned = false;
    bool ready = false;
};

struct AggroWaypointTraceContext {
    const char* route_name = nullptr;
    const char* context = nullptr;
};

ZoneTransitionAttemptResult ZoneThroughPointWithTransitionHooks(
    float x,
    float y,
    uint32_t targetMapId,
    const char* context,
    uint32_t timeoutMs = 60000u,
    uint32_t settleMs = 3000u,
    uint32_t readyTimeoutMs = 30000u);

void WaitMs(DWORD ms) {
    Sleep(ms);
}

bool IsPlayerOrPartyDead() {
    auto* me = AgentMgr::GetMyAgent();
    return me == nullptr || me->hp <= 0.0f || PartyMgr::GetIsPartyDefeated();
}

bool IsCurrentMapLoadedForTravel() {
    return MapMgr::GetMapId() != 0u && MapMgr::GetIsMapLoaded();
}

void WaitMaintenanceMs(uint32_t ms) {
    Sleep(ms);
}

void TravelWaitMs(uint32_t ms) {
    WaitMs(ms);
}

bool MoveMaintenancePoint(float x, float y, float threshold) {
    return DungeonNavigation::MoveToAndWait(
        x,
        y,
        threshold,
        30000u,
        1000u,
        MapMgr::GetMapId()).arrived;
}

bool ContainsNoCase(const char* haystack, const char* needle) {
    if (haystack == nullptr || needle == nullptr || *needle == '\0') {
        return false;
    }
    const std::size_t needleLen = std::strlen(needle);
    for (const char* p = haystack; *p != '\0'; ++p) {
        if (_strnicmp(p, needle, needleLen) == 0) {
            return true;
        }
    }
    return false;
}

bool WaitForStableOutpostMap(uint32_t mapId, uint32_t settleMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < settleMs) {
        if (MapMgr::GetMapId() != mapId ||
            !MapMgr::GetIsMapLoaded() ||
            AgentMgr::GetMyId() == 0u ||
            AgentMgr::GetMyAgent() == nullptr) {
            return false;
        }
        WaitMs(250u);
    }
    return true;
}

bool TravelOutpostAndWait(uint32_t mapId, const char* label, uint32_t timeoutMs = 90000u, uint32_t settleMs = 5000u) {
    const uint32_t currentMap = MapMgr::GetMapId();
    LogArachnisMain("outpost-travel begin label=%s target=%u current=%u loading=%u",
                    label ? label : "",
                    mapId,
                    currentMap,
                    MapMgr::GetLoadingState());

    if (currentMap == mapId) {
        const bool ready = WaitForMapReady(mapId, timeoutMs, label);
        const bool stable = ready && WaitForStableOutpostMap(mapId, settleMs);
        LogArachnisMain("outpost-travel already-there label=%s target=%u ready=%d stable=%d loading=%u",
                        label ? label : "",
                        mapId,
                        ready ? 1 : 0,
                        stable ? 1 : 0,
                        MapMgr::GetLoadingState());
        return stable;
    }

    if (currentMap != 0u && !MapMgr::GetIsMapLoaded()) {
        const bool settledBeforeTravel = WaitForMapReady(currentMap, 30000u, label);
        LogArachnisMain("outpost-travel pre-settle label=%s current=%u settled=%d loading=%u",
                        label ? label : "",
                        currentMap,
                        settledBeforeTravel ? 1 : 0,
                        MapMgr::GetLoadingState());
        if (!settledBeforeTravel) {
            return false;
        }
    }

    struct TravelAttempt {
        uint32_t region;
        uint32_t district;
        uint32_t language;
        const char* name;
    };
    constexpr TravelAttempt attempts[] = {
        {0u, 0u, 0u, "default"},
        {4u, 0u, 0u, "asia-japan-english"},
        {0u, 0u, 0u, "default-retry"},
    };
    const uint32_t attemptTimeoutMs = timeoutMs < 45000u ? timeoutMs : 45000u;
    for (const auto& attempt : attempts) {
        if (!MapMgr::Travel(mapId, attempt.region, attempt.district, attempt.language)) {
            LogArachnisMain("outpost-travel rejected label=%s attempt=%s target=%u current=%u loading=%u",
                            label ? label : "",
                            attempt.name,
                            mapId,
                            MapMgr::GetMapId(),
                            MapMgr::GetLoadingState());
            continue;
        }

        const bool ready = WaitForMapReady(mapId, attemptTimeoutMs, label);
        const bool stable = ready && WaitForStableOutpostMap(mapId, settleMs);
        LogArachnisMain("outpost-travel result label=%s attempt=%s target=%u ready=%d stable=%d current=%u loading=%u",
                        label ? label : "",
                        attempt.name,
                        mapId,
                        ready ? 1 : 0,
                        stable ? 1 : 0,
                        MapMgr::GetMapId(),
                        MapMgr::GetLoadingState());
        if (stable) {
            return true;
        }
    }
    return false;
}

DungeonVendor::MaintenanceLocation MakeArachnisMaintenanceLocation() {
    DungeonVendor::MaintenanceLocation location = {};
    location.outpost_map_id = GWA3::MapIds::GADDS_ENCAMPMENT;
    location.merchant_x = kGaddsMerchantX;
    location.merchant_y = kGaddsMerchantY;
    location.merchant_move_threshold = 350.0f;
    location.merchant_search_radius = 2500.0f;
    location.merchant_player_number = kGaddsMerchantPlayerNumber;
    location.xunlai_chest_x = kGaddsXunlaiX;
    location.xunlai_chest_y = kGaddsXunlaiY;
    location.material_trader_x = kGaddsMaterialTraderX;
    location.material_trader_y = kGaddsMaterialTraderY;
    location.material_trader_player_number = kGaddsMaterialTraderPlayerNumber;
    return location;
}

MaintenanceMgr::Config MakeArachnisMaintenanceConfig() {
    auto config = DungeonVendor::BuildMaintenanceConfig(
        GWA3::MapIds::GADDS_ENCAMPMENT,
        MakeArachnisMaintenanceLocation());
    config.minFreeSlots = 8u;
    config.depositKeepOnChar = 5000u;
    config.depositWhenCharacterGoldAtLeast = 80000u;
    config.consetStorageGoldTrigger = 700000u;
    config.consetStorageGoldFloor = 700000u;
    config.consetMaterialStackTrigger = 0xFFFFFFFFu;
    config.consetMaterialPressureFreeSlots = 0u;
    config.enableConsetRestock = true;
    return config;
}

bool NeedsArachnisMaintenance() {
    const auto config = MakeArachnisMaintenanceConfig();
    auto launchGateConfig = config;
    launchGateConfig.targetCharacterConsetsEach = 0u;
    if (MaintenanceMgr::NeedsMaintenance(launchGateConfig)) {
        return true;
    }
    if (MaintenanceMgr::NeedsCharacterConsetRestock(config)) {
        LogArachnisMain("maintenance deferred: character consets below target; will continue and restock on next full maintenance");
    }
    if (config.enableConsetRestock &&
        ItemMgr::GetGoldStorage() > config.consetStorageGoldTrigger) {
        LogArachnisMain("maintenance needed storageGold=%u trigger=%u",
                        ItemMgr::GetGoldStorage(),
                        config.consetStorageGoldTrigger);
        return true;
    }
    return false;
}

bool RunArachnisTownMaintenanceIfNeeded() {
    const auto config = MakeArachnisMaintenanceConfig();
    if (!NeedsArachnisMaintenance()) {
        return true;
    }

    LogBot("Arachnis: maintenance needed - traveling to Gadd's Encampment");
    if (MapMgr::GetMapId() != GWA3::MapIds::GADDS_ENCAMPMENT) {
        if (!TravelOutpostAndWait(GWA3::MapIds::GADDS_ENCAMPMENT, "maintenance-to-gadds")) {
            LogBot("Arachnis: failed reaching Gadd's Encampment for maintenance");
            return false;
        }
    }

    const auto location = MakeArachnisMaintenanceLocation();
    if (DungeonVendor::OpenMaintenanceMerchantContext(
            location,
            &MoveMaintenancePoint,
            &WaitMaintenanceMs,
            "Arachnis")) {
        MaintenanceMgr::PerformMaintenance(config);
        WaitMs(500u);
    } else {
        LogBot("Arachnis: maintenance merchant failed to open; depositing excess gold only");
        MaintenanceMgr::DepositGold(config.depositKeepOnChar);
    }

    const uint32_t freeSlots = MaintenanceMgr::CountFreeSlots();
    const bool stillNeedsMaintenance = NeedsArachnisMaintenance();
    LogArachnisMain("maintenance result freeSlots=%u minFreeSlots=%u stillNeeds=%d charGold=%u storageGold=%u",
                    freeSlots,
                    config.minFreeSlots,
                    stillNeedsMaintenance ? 1 : 0,
                    ItemMgr::GetGoldCharacter(),
                    ItemMgr::GetGoldStorage());

    if (freeSlots < 3u || stillNeedsMaintenance) {
        LogBot("Arachnis: maintenance did not clear inventory/gold enough; stopping before next run");
        return false;
    }

    s_outpostSetupApplied = false;
    LogBot("Arachnis: returning to Rata Sum after maintenance");
    MapMgr::Travel(GWA3::MapIds::RATA_SUM);
    return DungeonNavigation::WaitForMapId(GWA3::MapIds::RATA_SUM, 60000u) &&
           WaitForMapReady(GWA3::MapIds::RATA_SUM, 15000u, "maintenance-return-rata");
}

const char* SelectArachnisHeroConfigFile(const BotConfig& cfg) {
    if (!cfg.hero_config_file.empty() &&
        ContainsNoCase(cfg.hero_config_file.c_str(), "Arachnis")) {
        return cfg.hero_config_file.c_str();
    }

    const wchar_t* playerName = PlayerMgr::GetPlayerName(0u);
    if (playerName != nullptr && *playerName != L'\0') {
        if (std::wcsstr(playerName, L"GWA3 SAMPLE ONE") != nullptr) {
            return "ArachnisMercs.txt";
        }
        if (std::wcsstr(playerName, L"GWA3 SAMPLE FOUR") != nullptr ||
            std::wcsstr(playerName, L"GWA3 SAMPLE TWO") != nullptr ||
            std::wcsstr(playerName, L"GWA3 SAMPLE THREE") != nullptr ||
            std::wcsstr(playerName, L"GWA3 SAMPLE FIVE") != nullptr) {
            return "ArachnisStandard.txt";
        }
    }

    char preferred[64] = {};
    DungeonOutpostSetup::ResolvePreferredHeroConfigFile(
        preferred,
        sizeof(preferred),
        "Standard.txt");
    return ContainsNoCase(preferred, "Merc") ? "ArachnisMercs.txt" : "ArachnisStandard.txt";
}

bool EnsureArachnisOutpostSetup(BotConfig& cfg) {
    if (s_outpostSetupApplied && PartyMgr::CountPartyHeroes() >= 7u) {
        if (!WaitForArachnisLiveHeroSetup(cfg, "rata-existing-setup", 5000u)) {
            LogBot("Arachnis: cached outpost setup is invalid; reapplying hero templates");
            s_outpostSetupApplied = false;
        } else {
            if (!PartyMgr::GetIsHardMode()) {
                MapMgr::SetHardMode(true);
                WaitMs(500u);
            }
            return true;
        }
    }

    const char* preferredConfig = SelectArachnisHeroConfigFile(cfg);
    DungeonOutpostSetup::Options options = {};
    options.default_hero_config_file = preferredConfig;
    options.hero_behavior = 1u;

    cfg.hero_config_file = preferredConfig;
    LogBot("Arachnis: applying Rata Sum outpost setup with %s", preferredConfig);
    if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg, options)) {
        const char* fallbackConfig = ContainsNoCase(preferredConfig, "Merc")
            ? "ArachnisStandard.txt"
            : "ArachnisMercs.txt";
        LogBot("Arachnis: outpost setup with %s failed; retrying %s",
               preferredConfig,
               fallbackConfig);
        cfg.hero_config_file = fallbackConfig;
        options.default_hero_config_file = fallbackConfig;
        if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg, options)) {
            LogBot("Arachnis: Rata Sum outpost setup failed");
            return false;
        }
    }

    s_outpostSetupApplied = true;
    if (!WaitForArachnisLiveHeroSetup(cfg, "rata-post-setup", 8000u)) {
        s_outpostSetupApplied = false;
        LogBot("Arachnis: Rata Sum outpost setup validation failed after applying templates");
        return false;
    }
    return true;
}

void PulseArachnisDefensiveHeroShouts(const char* context, int waypointIndex) {
    const DWORD now = GetTickCount();
    if (s_lastArachnisDefensePulse != 0u && (now - s_lastArachnisDefensePulse) < 8000u) {
        return;
    }
    s_lastArachnisDefensePulse = now;

    LogArachnisMain("defensive-shout pulse context=%s waypoint=%d",
                    context != nullptr ? context : "",
                    waypointIndex);

    SkillMgr::UseHeroSkill(4u, 6u, 0u); // Charge
    SkillMgr::UseHeroSkill(4u, 7u, 0u); // Watch Yourself
    SkillMgr::UseHeroSkill(4u, 8u, 0u); // Shields Up
    SkillMgr::UseHeroSkill(1u, 8u, 0u); // Shields Up
    SkillMgr::UseHeroSkill(7u, 7u, 0u); // Watch Yourself
    SkillMgr::UseHeroSkill(7u, 8u, 0u); // Shields Up
}

void FightTargetWithArachnisMagusCombat(uint32_t targetId) {
    if (MapMgr::GetMapId() == GWA3::MapIds::MAGUS_STONES) {
        PulseArachnisDefensiveHeroShouts("magus-clear", -1);
    }
    DungeonBuiltinCombat::FightTargetWithBuiltinCombat(targetId);
}

DungeonCombat::CombatCallbacks MakeArachnisMagusCombatCallbacks() {
    auto callbacks = DungeonBuiltinCombat::MakeCombatCallbacks();
    callbacks.fight_target = &FightTargetWithArachnisMagusCombat;
    return callbacks;
}

bool ValidateArachnisLiveHeroProfessions(const BotConfig& cfg, const char* context) {
    const char* configFile = cfg.hero_config_file.empty()
        ? SelectArachnisHeroConfigFile(cfg)
        : cfg.hero_config_file.c_str();

    DungeonOutpostSetup::HeroTemplate templates[DungeonOutpostSetup::kMaxHeroTemplates] = {};
    const std::size_t templateCount =
        DungeonOutpostSetup::LoadHeroTemplatesFromFile(configFile, templates, DungeonOutpostSetup::kMaxHeroTemplates);
    if (templateCount == 0u) {
        LogBot("Arachnis: live hero setup validation has no templates at %s config=%s",
               context ? context : "unknown",
               configFile);
        return false;
    }

    PartyInfo* party = PartyMgr::ResolvePlayerParty();
    if (!party || !party->heroes.buffer || party->heroes.size == 0u || party->heroes.size > 16u) {
        LogBot("Arachnis: live hero setup validation waiting for party at %s heroes=%u config=%s",
               context ? context : "unknown",
               PartyMgr::CountPartyHeroes(),
               configFile);
        return false;
    }

    bool checkedLiveHero = false;
    bool checkedSkillbar = false;
    uint32_t checkedProfessionCount = 0u;
    uint32_t checkedSkillbarCount = 0u;
    __try {
        const std::size_t heroCount = party->heroes.size < templateCount ? party->heroes.size : templateCount;
        for (std::size_t i = 0u; i < heroCount; ++i) {
            const auto& hero = templates[i];
            const HeroPartyMember& member = party->heroes.buffer[i];
            if (hero.secondary_profession != 0u &&
                hero.secondary_profession != hero.primary_profession) {
                checkedLiveHero = true;
                ++checkedProfessionCount;
                uint32_t livePrimary = 0u;
                uint32_t liveSecondary = 0u;
                if (!DungeonOutpostSetup::ReadHeroProfessions(member.agent_id, livePrimary, liveSecondary)) {
                    LogBot("Arachnis: hero slot %u profession unavailable at %s hero=%u agent=%u expected=%u config=%s",
                           static_cast<unsigned>(i + 1u),
                           context ? context : "unknown",
                           hero.hero_id,
                           member.agent_id,
                           hero.secondary_profession,
                           configFile);
                    return false;
                }
                if (liveSecondary != hero.secondary_profession) {
                    LogBot("Arachnis: hero slot %u secondary mismatch at %s hero=%u agent=%u live=%u expected=%u config=%s",
                           static_cast<unsigned>(i + 1u),
                           context ? context : "unknown",
                           hero.hero_id,
                           member.agent_id,
                           liveSecondary,
                           hero.secondary_profession,
                           configFile);
                    return false;
                }
            }

            Skillbar* bar = SkillMgr::GetSkillbarByAgentId(member.agent_id);
            if (!bar) {
                LogBot("Arachnis: hero slot %u skillbar missing at %s hero=%u agent=%u config=%s",
                       static_cast<unsigned>(i + 1u),
                       context ? context : "unknown",
                       hero.hero_id,
                       member.agent_id,
                       configFile);
                return false;
            }

            checkedSkillbar = true;
            ++checkedSkillbarCount;
            uint32_t disabled = 0u;
            __try {
                disabled = bar->disabled;
                for (uint32_t slot = 0u; slot < 8u; ++slot) {
                    const uint32_t expected = hero.skills[slot];
                    const uint32_t actual = bar->skills[slot].skill_id;
                    if (actual == expected) {
                        continue;
                    }

                    LogBot("Arachnis: hero slot %u skill mismatch at %s hero=%u agent=%u slot=%u expected=%u actual=%u disabled=0x%08X config=%s",
                           static_cast<unsigned>(i + 1u),
                           context ? context : "unknown",
                           hero.hero_id,
                           member.agent_id,
                           slot + 1u,
                           expected,
                           actual,
                           disabled,
                           configFile);
                    return false;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                LogBot("Arachnis: hero slot %u skillbar validation faulted at %s hero=%u agent=%u config=%s",
                       static_cast<unsigned>(i + 1u),
                       context ? context : "unknown",
                       hero.hero_id,
                       member.agent_id,
                       configFile);
                return false;
            }

            if (disabled != 0u) {
                LogBot("Arachnis: hero slot %u skillbar disabled at %s hero=%u agent=%u disabled=0x%08X config=%s",
                       static_cast<unsigned>(i + 1u),
                       context ? context : "unknown",
                       hero.hero_id,
                       member.agent_id,
                       disabled,
                       configFile);
                return false;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LogBot("Arachnis: live hero profession validation faulted at %s; failing setup validation",
               context ? context : "unknown");
        return false;
    }

    if (checkedLiveHero || checkedSkillbar) {
        LogBot("Arachnis: live hero setup validated at %s with %s professions=%u skillbars=%u",
               context ? context : "unknown",
               configFile,
               checkedProfessionCount,
               checkedSkillbarCount);
    } else {
        LogBot("Arachnis: live hero setup validation had no readable heroes at %s with %s",
               context ? context : "unknown",
               configFile);
        return false;
    }
    return true;
}

bool WaitForArachnisLiveHeroSetup(const BotConfig& cfg, const char* context, uint32_t timeoutMs) {
    const DWORD start = GetTickCount();
    DWORD lastLogMs = 0u;
    LogArachnisMain("live hero setup validation begin context=%s heroes=%u timeoutMs=%u",
                    context != nullptr ? context : "",
                    PartyMgr::CountPartyHeroes(),
                    timeoutMs);
    while ((GetTickCount() - start) < timeoutMs) {
        const uint32_t heroes = PartyMgr::CountPartyHeroes();
        if (heroes >= 7u && ValidateArachnisLiveHeroProfessions(cfg, context)) {
            LogArachnisMain("live hero setup validation success context=%s heroes=%u elapsedMs=%u",
                            context != nullptr ? context : "",
                            heroes,
                            GetTickCount() - start);
            return true;
        }

        const DWORD now = GetTickCount();
        if (lastLogMs == 0u || (now - lastLogMs) >= 2500u) {
            LogArachnisMain("waiting for live hero setup context=%s heroes=%u elapsedMs=%u timeoutMs=%u",
                            context != nullptr ? context : "",
                            heroes,
                            now - start,
                            timeoutMs);
            lastLogMs = now;
        }
        WaitMs(250u);
    }

    LogBot("Arachnis: live hero setup validation timed out at %s heroes=%u",
           context ? context : "unknown",
           PartyMgr::CountPartyHeroes());
    return false;
}

void UseArachnisConsetsIfEnabled(const BotConfig& cfg, const char* context) {
    const auto result = DungeonItemActions::UseConsetsForCurrentPlayerIfEnabled(
        cfg.use_consets,
        &WaitMaintenanceMs,
        {},
        "Arachnis");
    if (result.attempted) {
        LogArachnisMain("conset attempt context=%s armor=%d essence=%d grail=%d full=%d",
                        context != nullptr ? context : "",
                        result.consets.used_armor ? 1 : 0,
                        result.consets.used_essence ? 1 : 0,
                        result.consets.used_grail ? 1 : 0,
                        result.consets.full_active ? 1 : 0);
    }
}

const char* GetMapReadyStateLabel(uint32_t mapId, float& hp, float& x, float& y) {
    hp = 0.0f;
    x = 0.0f;
    y = 0.0f;

    if (MapMgr::GetMapId() != mapId) {
        return "wrong-map";
    }
    if (MapMgr::GetLoadingState() != 1u) {
        return "loading";
    }
    if (AgentMgr::GetMyId() == 0u) {
        return "missing-my-id";
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return "missing-agent";
    }

    hp = me->hp;
    x = me->x;
    y = me->y;
    if (me->hp <= 0.0f) {
        return "dead-agent";
    }
    if (me->x == 0.0f && me->y == 0.0f) {
        return "zero-position";
    }

    return "ready";
}

bool IsExpectedTransitionReadyPosition(
    uint32_t mapId,
    const char* context,
    float x,
    float y) {
    if (mapId != GWA3::MapIds::ARACHNIS_HAUNT_LVL1 || context == nullptr) {
        return true;
    }
    if (std::strstr(context, "entry") == nullptr) {
        return true;
    }

    // Immediately after zoning, the map id can update before the player agent
    // coordinates hydrate. Level 1 entry spawn is in the northeast corner.
    return x > 12000.0f && y > 15000.0f;
}

bool MoveToWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    uint32_t mapId,
    float tolerance = 250.0f,
    uint32_t timeoutMs = 30000u) {
    return DungeonNavigation::MoveToAndWait(
        waypoint.x,
        waypoint.y,
        tolerance,
        timeoutMs,
        1000u,
        mapId).arrived;
}

bool RecoverHeldBundleAfterTravelRevive(
    uint32_t mapId,
    const char* context,
    const char* recoveryRouteName,
    int recoveryWaypointIndex) {
    if (HasHeldBundle()) {
        return true;
    }
    if (EnsureHeldBundleNearPlayer()) {
        return true;
    }
    if (recoveryRouteName == nullptr || *recoveryRouteName == '\0') {
        return false;
    }

    LogArachnisMain("bundle-travel route-aware recovery context=%s route=%s index=%d map=%u",
                    context != nullptr ? context : "",
                    recoveryRouteName,
                    recoveryWaypointIndex,
                    mapId);
    return RecoverFlameStaffAfterRevive(
        recoveryRouteName,
        recoveryWaypointIndex,
        mapId,
        context);
}

bool MoveToTravelPointWhileCarryingBundle(
    const DungeonQuest::TravelPoint& point,
    uint32_t mapId,
    float tolerance,
    uint32_t timeoutMs,
    const char* context,
    const char* recoveryRouteName = nullptr,
    int recoveryWaypointIndex = 0) {
    const uint32_t carryTimeoutMs = timeoutMs < 120000u ? 120000u : timeoutMs;
    const DWORD start = GetTickCount();
    DWORD lastMoveMs = 0u;
    DWORD lastAssistMs = 0u;
    DWORD lastProgressMs = 0u;
    float lastProgressDistance = 0.0f;
    int lowProgressRecoveries = 0;

    while ((GetTickCount() - start) < carryTimeoutMs) {
        if (mapId != 0u && MapMgr::GetMapId() != mapId) {
            LogArachnisMain("bundle-travel abort map-change context=%s expectedMap=%u currentMap=%u",
                            context != nullptr ? context : "",
                            mapId,
                            MapMgr::GetMapId());
            return false;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (me == nullptr) {
            LogArachnisMain("bundle-travel abort missing-player context=%s map=%u",
                            context != nullptr ? context : "",
                            MapMgr::GetMapId());
            return false;
        }

        if (PartyMgr::GetIsPartyDefeated()) {
            LogArachnisMain("bundle-travel abort party-defeated context=%s player=(%.0f, %.0f) hp=%.2f",
                            context != nullptr ? context : "",
                            me->x,
                            me->y,
                            me->hp);
            return false;
        }

        if (me->hp <= 0.0f) {
            LogArachnisMain("bundle-travel player down context=%s map=%u player=(%.0f, %.0f)",
                            context != nullptr ? context : "",
                            MapMgr::GetMapId(),
                            me->x,
                            me->y);
            const DWORD reviveStart = GetTickCount();
            bool revived = false;
            while ((GetTickCount() - reviveStart) < 30000u) {
                if (mapId != 0u && MapMgr::GetMapId() != mapId) {
                    return false;
                }
                if (PartyMgr::GetIsPartyDefeated()) {
                    break;
                }
                me = AgentMgr::GetMyAgent();
                if (me != nullptr && me->hp > 0.0f) {
                    revived = true;
                    break;
                }
                WaitMs(500u);
            }
            if (!revived) {
                LogArachnisMain("bundle-travel abort revive-timeout context=%s partyDefeated=%d",
                                context != nullptr ? context : "",
                                PartyMgr::GetIsPartyDefeated() ? 1 : 0);
                return false;
            }
            WaitMs(1000u);
            if (!RecoverHeldBundleAfterTravelRevive(
                    mapId,
                    context,
                    recoveryRouteName,
                    recoveryWaypointIndex)) {
                LogArachnisMain("bundle-travel abort missing-staff-after-revive context=%s",
                                context != nullptr ? context : "");
                return false;
            }
            lastMoveMs = 0u;
            continue;
        }

        if (me->hp < 0.35f) {
            LogArachnisMain("bundle-travel low-hp pause context=%s player=(%.0f, %.0f) hp=%.2f",
                            context != nullptr ? context : "",
                            me->x,
                            me->y,
                            me->hp);
            AgentMgr::Move(me->x, me->y);
            (void)RunStaffSafeCombatAssist(1500.0f, 6000u, "bundle-travel-low-hp");
            const DWORD healStart = GetTickCount();
            while ((GetTickCount() - healStart) < 8000u) {
                me = AgentMgr::GetMyAgent();
                if (me == nullptr || me->hp <= 0.0f || me->hp >= 0.70f || PartyMgr::GetIsPartyDefeated()) {
                    break;
                }
                WaitMs(500u);
            }
            lastMoveMs = 0u;
            continue;
        }

        const float distance = AgentMgr::GetDistance(me->x, me->y, point.x, point.y);
        if (distance <= tolerance) {
            LogArachnisMain("bundle-travel arrived context=%s target=(%.0f, %.0f) player=(%.0f, %.0f) distance=%.0f",
                            context != nullptr ? context : "",
                            point.x,
                            point.y,
                            me->x,
                            me->y,
                            distance);
            return true;
        }

        const float nearestEnemyDistance = DungeonCombat::GetNearestLivingEnemyDistance(1500.0f);
        const DWORD now = GetTickCount();
        if (lastProgressMs == 0u || distance < (lastProgressDistance - 75.0f)) {
            lastProgressMs = now;
            lastProgressDistance = distance;
        } else if ((now - lastProgressMs) >= 8000u && lowProgressRecoveries < 6) {
            ++lowProgressRecoveries;
            LogArachnisMain("bundle-travel low-progress recovery context=%s recovery=%d target=(%.0f, %.0f) player=(%.0f, %.0f) distance=%.0f previous=%.0f enemyDistance=%.0f",
                            context != nullptr ? context : "",
                            lowProgressRecoveries,
                            point.x,
                            point.y,
                            me->x,
                            me->y,
                            distance,
                            lastProgressDistance,
                            nearestEnemyDistance);
            AgentMgr::Move(me->x, me->y);
            WaitMs(250u);
            (void)RunStaffSafeCombatAssist(1500.0f, 4000u, "bundle-travel-low-progress");
            AgentMgr::Move(point.x, point.y);
            lastMoveMs = now;
            lastProgressMs = now;
            lastProgressDistance = distance;
            continue;
        }

        if (nearestEnemyDistance <= 1400.0f &&
            (lastAssistMs == 0u || (now - lastAssistMs) >= 6000u)) {
            LogArachnisMain("bundle-travel enemy-near assist context=%s target=(%.0f, %.0f) player=(%.0f, %.0f) hp=%.2f enemyDistance=%.0f",
                            context != nullptr ? context : "",
                            point.x,
                            point.y,
                            me->x,
                            me->y,
                            me->hp,
                            nearestEnemyDistance);
            AgentMgr::Move(me->x, me->y);
            (void)RunStaffSafeCombatAssist(1500.0f, 5000u, "bundle-travel-enemy-near");
            lastAssistMs = GetTickCount();
            lastMoveMs = 0u;
            continue;
        }

        if (lastMoveMs == 0u || (now - lastMoveMs) >= 1000u) {
            LogArachnisMain("bundle-travel move context=%s target=(%.0f, %.0f) player=(%.0f, %.0f) distance=%.0f tolerance=%.0f",
                            context != nullptr ? context : "",
                            point.x,
                            point.y,
                            me->x,
                            me->y,
                            distance,
                            tolerance);
            AgentMgr::Move(point.x, point.y);
            lastMoveMs = now;
        }
        WaitMs(150u);
    }

    auto* me = AgentMgr::GetMyAgent();
    const float remaining = me != nullptr
        ? AgentMgr::GetDistance(me->x, me->y, point.x, point.y)
        : -1.0f;
    LogArachnisMain("bundle-travel abort timeout context=%s target=(%.0f, %.0f) player=(%.0f, %.0f) remaining=%.0f tolerance=%.0f timeoutMs=%u",
                    context != nullptr ? context : "",
                    point.x,
                    point.y,
                    me ? me->x : 0.0f,
                    me ? me->y : 0.0f,
                    remaining,
                    tolerance,
                    carryTimeoutMs);
    return false;
}

bool MoveToTravelPoint(
    const DungeonQuest::TravelPoint& point,
    uint32_t mapId,
    float tolerance = 250.0f,
    uint32_t timeoutMs = 30000u) {
    if (HasEffectOnlyFlameStaffBundle()) {
        return MoveToTravelPointWhileCarryingBundle(point, mapId, tolerance, timeoutMs, "travel-point");
    }

    return DungeonNavigation::MoveToAndWait(
        point.x,
        point.y,
        tolerance,
        timeoutMs,
        1000u,
        mapId).arrived;
}

void MoveToPointForDoor(float x, float y, float threshold) {
    (void)DungeonNavigation::MoveToAndWait(
        x,
        y,
        threshold,
        30000u,
        1000u,
        MapMgr::GetMapId());
}

bool ZoneThroughPoint(float x, float y, uint32_t targetMapId, uint32_t timeoutMs = 60000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        AgentMgr::Move(x, y);
        if (MapMgr::GetMapId() == targetMapId) {
            return true;
        }
        if (DungeonNavigation::WaitForMapId(targetMapId, 250u)) {
            return true;
        }
        WaitMs(250u);
    }
    return false;
}

void SuspendTransitionSensitiveHooks(const char* context) {
    LogArachnisMain("transition hooks suspend context=%s map=%u loading=%u myId=%u",
                    context != nullptr ? context : "",
                    MapMgr::GetMapId(),
                    MapMgr::GetLoadingState(),
                    AgentMgr::GetMyId());
    AgentMgr::ResetMoveState("Arachnis transition suspend");
    CtoS::SuspendEngineHook();
    DialogMgr::Shutdown();
}

void ResumeTransitionSensitiveHooks(const char* context) {
    AgentMgr::ResetMoveState("Arachnis transition resume");
    CtoS::ResumeEngineHook();
    const bool dialogReady = DialogMgr::Initialize();
    LogArachnisMain("transition hooks resume context=%s map=%u loading=%u myId=%u dialogReady=%d",
                    context != nullptr ? context : "",
                    MapMgr::GetMapId(),
                    MapMgr::GetLoadingState(),
                    AgentMgr::GetMyId(),
                    dialogReady ? 1 : 0);
}

ZoneTransitionAttemptResult ZoneThroughPointWithTransitionHooks(
    float x,
    float y,
    uint32_t targetMapId,
    const char* context,
    uint32_t timeoutMs,
    uint32_t settleMs,
    uint32_t readyTimeoutMs) {
    ZoneTransitionAttemptResult result{};
    SuspendTransitionSensitiveHooks(context);
    result.zoned = ZoneThroughPoint(x, y, targetMapId, timeoutMs);
    if (result.zoned) {
        result.ready = FinalizeZoneTransition(targetMapId, context, settleMs, readyTimeoutMs);
    }
    ResumeTransitionSensitiveHooks(context);
    return result;
}

DungeonQuestRuntime::DialogExecutionOptions BuildQuestDialogOptions() {
    DungeonQuestRuntime::DialogExecutionOptions options;
    options.move_to_actual_npc = true;
    options.move_to_npc_tolerance = 180.0f;
    options.move_to_npc_timeout_ms = 18000u;
    options.cancel_action_before_interact = true;
    options.clear_dialog_state_before_interact = true;
    options.require_dialog_before_send = true;
    options.send_dialog_without_ready = true;
    options.pre_interact_settle_ms = 500u;
    options.change_target_delay_ms = 250u;
    options.interact_count = 3;
    options.interact_delay_ms = 1500u;
    options.post_interact_delay_ms = 1000u;
    options.dialog_wait_timeout_ms = 2500u;
    options.repeat_delay_ms = 750u;
    options.max_retries_per_dialog = 2;
    options.npc_dialog_candidate_count = 6;
    options.use_direct_npc_interact = true;
    options.log_npc_dialog_candidates = true;
    return options;
}

bool ZoneAtTravelPointWithRetries(
    const DungeonQuest::TravelPoint& point,
    uint32_t currentMapId,
    uint32_t targetMapId,
    const char* label,
    int attempts = 3) {
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (MapMgr::GetMapId() == targetMapId) {
            LogArachnisMain("zone attempt already-target label=%s attempt=%d/%d targetMap=%u",
                            label != nullptr ? label : "",
                            attempt + 1,
                            attempts,
                            targetMapId);
            return FinalizeZoneTransition(targetMapId, label);
        }

        LogArachnisMain("zone attempt start label=%s attempt=%d/%d currentMap=%u targetMap=%u point=(%.0f, %.0f)",
                        label != nullptr ? label : "",
                        attempt + 1,
                        attempts,
                        currentMapId,
                        targetMapId,
                        point.x,
                        point.y);
        if (!MoveToTravelPoint(point, currentMapId, 300.0f, 15000u)) {
            if (MapMgr::GetMapId() == targetMapId) {
                LogArachnisMain("zone attempt reach converted-to-zoned label=%s attempt=%d/%d targetMap=%u loading=%u myId=%u",
                                label != nullptr ? label : "",
                                attempt + 1,
                                attempts,
                                targetMapId,
                                MapMgr::GetLoadingState(),
                                AgentMgr::GetMyId());
                return FinalizeZoneTransition(targetMapId, label);
            }
            LogBot("Arachnis: failed reaching %s point on attempt %d/%d",
                   label,
                   attempt + 1,
                   attempts);
            LogArachnisMain("zone attempt reach failed label=%s attempt=%d/%d currentMap=%u targetMap=%u",
                            label != nullptr ? label : "",
                            attempt + 1,
                            attempts,
                            MapMgr::GetMapId(),
                            targetMapId);
            WaitMs(500u);
            continue;
        }

        AgentMgr::CancelAction();
        WaitMs(100u);
        AgentMgr::ChangeTarget(0u);
        WaitMs(150u);

        const ZoneTransitionAttemptResult transition =
            ZoneThroughPointWithTransitionHooks(point.x, point.y, targetMapId, label, 15000u);
        LogArachnisMain("zone attempt observed label=%s attempt=%d/%d zoned=%d currentMap=%u loading=%u myId=%u",
                        label != nullptr ? label : "",
                        attempt + 1,
                        attempts,
                        transition.zoned ? 1 : 0,
                        MapMgr::GetMapId(),
                        MapMgr::GetLoadingState(),
                        AgentMgr::GetMyId());
        if (transition.ready) {
            return true;
        }

        LogBot("Arachnis: %s zone attempt %d/%d failed",
               label,
               attempt + 1,
               attempts);
        LogArachnisMain("zone attempt failed label=%s attempt=%d/%d currentMap=%u loading=%u myId=%u",
                        label != nullptr ? label : "",
                        attempt + 1,
                        attempts,
                        MapMgr::GetMapId(),
                        MapMgr::GetLoadingState(),
                        AgentMgr::GetMyId());
        WaitMs(500u);
    }

    return false;
}

bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs, const char* context) {
    const DWORD start = GetTickCount();
    bool observedTargetMap = false;
    DWORD observedTargetElapsedMs = 0u;
    const char* lastState = "timeout";
    uint32_t lastMapId = MapMgr::GetMapId();
    uint32_t lastLoadingState = MapMgr::GetLoadingState();
    uint32_t lastMyId = AgentMgr::GetMyId();
    float lastHp = 0.0f;
    float lastX = 0.0f;
    float lastY = 0.0f;
    while ((GetTickCount() - start) < timeoutMs) {
        lastMapId = MapMgr::GetMapId();
        lastLoadingState = MapMgr::GetLoadingState();
        lastMyId = AgentMgr::GetMyId();
        if (!observedTargetMap && lastMapId == mapId) {
            observedTargetMap = true;
            observedTargetElapsedMs = GetTickCount() - start;
            LogArachnisMain("map ready wait observed context=%s targetMap=%u elapsedMs=%lu loading=%u myId=%u",
                            context != nullptr ? context : "",
                            mapId,
                            static_cast<unsigned long>(observedTargetElapsedMs),
                            lastLoadingState,
                            lastMyId);
        }

        lastState = GetMapReadyStateLabel(mapId, lastHp, lastX, lastY);
        if (lastState[0] == 'r' && lastState[1] == 'e') {
            if (!IsExpectedTransitionReadyPosition(mapId, context, lastX, lastY)) {
                lastState = "stale-entry-position";
                LogArachnisMain("map ready wait stale-position context=%s targetMap=%u elapsedMs=%lu player=(%.0f, %.0f)",
                                context != nullptr ? context : "",
                                mapId,
                                static_cast<unsigned long>(GetTickCount() - start),
                                lastX,
                                lastY);
                WaitMs(250u);
                continue;
            }
            if (observedTargetMap) {
                LogArachnisMain("map ready wait success context=%s targetMap=%u elapsedMs=%lu observedMs=%lu player=(%.0f, %.0f) hp=%.2f",
                                context != nullptr ? context : "",
                                mapId,
                                static_cast<unsigned long>(GetTickCount() - start),
                                static_cast<unsigned long>(observedTargetElapsedMs),
                                lastX,
                                lastY,
                                lastHp);
            }
            return true;
        }

        WaitMs(250u);
    }

    LogBot("Arachnis: map %u ready wait timed out after %lu ms context=%s state=%s currentMap=%u loading=%u myId=%u player=(%.0f, %.0f) hp=%.2f",
           mapId,
           static_cast<unsigned long>(timeoutMs),
           context != nullptr ? context : "",
           lastState,
           lastMapId,
           lastLoadingState,
           lastMyId,
           lastX,
           lastY,
           lastHp);
    LogArachnisMain("map ready wait timeout context=%s targetMap=%u elapsedMs=%lu state=%s currentMap=%u loading=%u myId=%u player=(%.0f, %.0f) hp=%.2f",
                    context != nullptr ? context : "",
                    mapId,
                    static_cast<unsigned long>(timeoutMs),
                    lastState,
                    lastMapId,
                    lastLoadingState,
                    lastMyId,
                    lastX,
                    lastY,
                    lastHp);
    return false;
}

bool FinalizeZoneTransition(
    uint32_t targetMapId,
    const char* context,
    uint32_t settleMs,
    uint32_t readyTimeoutMs) {
    LogArachnisMain("zone finalize start context=%s targetMap=%u currentMap=%u loading=%u myId=%u settleMs=%u readyTimeoutMs=%u",
                    context != nullptr ? context : "",
                    targetMapId,
                    MapMgr::GetMapId(),
                    MapMgr::GetLoadingState(),
                    AgentMgr::GetMyId(),
                    settleMs,
                    readyTimeoutMs);
    if (MapMgr::GetMapId() != targetMapId) {
        LogArachnisMain("zone finalize skipped context=%s targetMap=%u currentMap=%u loading=%u myId=%u",
                        context != nullptr ? context : "",
                        targetMapId,
                        MapMgr::GetMapId(),
                        MapMgr::GetLoadingState(),
                        AgentMgr::GetMyId());
        return false;
    }

    if (settleMs > 0u) {
        WaitMs(settleMs);
    }

    const bool ready = WaitForMapReady(targetMapId, readyTimeoutMs, context);
    LogArachnisMain("zone finalize end context=%s targetMap=%u ready=%d currentMap=%u loading=%u myId=%u",
                    context != nullptr ? context : "",
                    targetMapId,
                    ready ? 1 : 0,
                    MapMgr::GetMapId(),
                    MapMgr::GetLoadingState(),
                    AgentMgr::GetMyId());
    return ready;
}

bool WaitForPartyRecovery(uint32_t mapId, uint32_t minHeroes = 1u, uint32_t timeoutMs = 8000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
            WaitMs(250u);
            continue;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (me == nullptr || me->hp <= 0.0f) {
            WaitMs(250u);
            continue;
        }

        if (PartyMgr::CountPartyHeroes() >= minHeroes) {
            return true;
        }

        WaitMs(250u);
    }

    return false;
}

bool WaitForPlayerAliveStable(
    uint32_t mapId,
    uint32_t minHeroes,
    uint32_t timeoutMs,
    uint32_t stableMs,
    const char* context,
    float minHp = 0.0f) {
    const DWORD start = GetTickCount();
    DWORD stableStart = 0u;
    float lastHp = 0.0f;
    uint32_t lastHeroCount = 0u;
    const DWORD hardTimeoutMs = timeoutMs + stableMs + 5000u;

    while (true) {
        const DWORD now = GetTickCount();
        const DWORD elapsed = now - start;
        if (elapsed >= timeoutMs && stableStart == 0u) {
            break;
        }
        if (elapsed >= hardTimeoutMs) {
            break;
        }

        const bool mapReady = MapMgr::GetMapId() == mapId && MapMgr::GetIsMapLoaded();
        auto* me = mapReady ? AgentMgr::GetMyAgent() : nullptr;
        const uint32_t heroCount = PartyMgr::CountPartyHeroes();
        const bool alive = mapReady &&
                           me != nullptr &&
                           me->hp >= minHp &&
                           !PartyMgr::GetIsPartyDefeated() &&
                           heroCount >= minHeroes;

        if (alive) {
            if (stableStart == 0u) {
                stableStart = now;
                LogArachnisMain("alive-stable observed context=%s map=%u heroes=%u hp=%.2f minHp=%.2f player=(%.0f, %.0f)",
                                context != nullptr ? context : "",
                                mapId,
                                heroCount,
                                me->hp,
                                minHp,
                                me->x,
                                me->y);
            }
            if ((now - stableStart) >= stableMs) {
                LogArachnisMain("alive-stable ready context=%s map=%u heroes=%u stableMs=%lu hp=%.2f minHp=%.2f player=(%.0f, %.0f)",
                                context != nullptr ? context : "",
                                mapId,
                                heroCount,
                                static_cast<unsigned long>(now - stableStart),
                                me->hp,
                                minHp,
                                me->x,
                                me->y);
                return true;
            }
        } else {
            stableStart = 0u;
        }

        lastHp = me != nullptr ? me->hp : 0.0f;
        lastHeroCount = heroCount;
        WaitMs(250u);
    }

    auto* me = AgentMgr::GetMyAgent();
    LogArachnisMain("alive-stable timeout context=%s map=%u currentMap=%u loaded=%d heroes=%u hp=%.2f minHp=%.2f lastHp=%.2f partyDefeated=%d player=(%.0f, %.0f)",
                    context != nullptr ? context : "",
                    mapId,
                    MapMgr::GetMapId(),
                    MapMgr::GetIsMapLoaded() ? 1 : 0,
                    lastHeroCount,
                    me != nullptr ? me->hp : 0.0f,
                    minHp,
                    lastHp,
                    PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                    me != nullptr ? me->x : 0.0f,
                    me != nullptr ? me->y : 0.0f);
    return false;
}

int GetNearestWaypointIndexForRoute(const RouteDefinition& route) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return -1;
    }

    return DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);
}

void LogRouteFailureSnapshot(
    const RouteDefinition& route,
    const char* context,
    const DungeonNavigation::RouteFollowResult& followResult) {
    auto* me = AgentMgr::GetMyAgent();
    const int failedIndex = followResult.failed_index;
    const bool hasFailedWaypoint =
        failedIndex >= 0 &&
        failedIndex < route.waypoint_count &&
        route.waypoints != nullptr;
    const auto* waypoint = hasFailedWaypoint ? &route.waypoints[failedIndex] : nullptr;
    const float remaining = (me != nullptr && waypoint != nullptr)
        ? AgentMgr::GetDistance(me->x, me->y, waypoint->x, waypoint->y)
        : 0.0f;
    const float nearestEnemy = DungeonCombat::GetNearestLivingEnemyDistance(2600.0f);

    LogArachnisMain(
        "route failure snapshot route=%s context=%s map=%u expectedMap=%u loaded=%d hp=%.2f partyDefeated=%d failedIndex=%d label=%s target=(%.0f, %.0f) player=(%.0f, %.0f) remaining=%.0f held=%d effectOnly=%d nearestEnemy2600=%.0f",
        route.name != nullptr ? route.name : "",
        context != nullptr ? context : "",
        MapMgr::GetMapId(),
        route.map_id,
        MapMgr::GetIsMapLoaded() ? 1 : 0,
        me != nullptr ? me->hp : 0.0f,
        PartyMgr::GetIsPartyDefeated() ? 1 : 0,
        failedIndex,
        waypoint != nullptr && waypoint->label != nullptr ? waypoint->label : "",
        waypoint != nullptr ? waypoint->x : 0.0f,
        waypoint != nullptr ? waypoint->y : 0.0f,
        me != nullptr ? me->x : 0.0f,
        me != nullptr ? me->y : 0.0f,
        remaining,
        HasHeldBundle() ? 1 : 0,
        HasEffectOnlyFlameStaffBundle() ? 1 : 0,
        nearestEnemy);
}

bool FollowTravelPath(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t mapId,
    float tolerance = 250.0f) {
    if (points == nullptr || count <= 0) {
        return false;
    }

    if (HasEffectOnlyFlameStaffBundle()) {
        for (int i = 0; i < count; ++i) {
            if (!MoveToTravelPointWhileCarryingBundle(points[i], mapId, tolerance, 30000u, "travel-path")) {
                return false;
            }
        }
        return true;
    }

    return DungeonQuestRuntime::FollowTravelPath(
        points,
        count,
        mapId,
        tolerance,
        30000u,
        1000u);
}

void ArachnisTravelFightInAggro(
    float aggroRange,
    bool /*careful*/,
    void* userData,
    bool /*waitForSkillCompletion*/,
    uint32_t maxFightMs) {
    if (userData != nullptr) {
        const float overrideRange = *static_cast<const float*>(userData);
        if (overrideRange > aggroRange) {
            aggroRange = overrideRange;
        }
    }

    const DWORD start = GetTickCount();
    DWORD lastAttackMs = 0u;
    DWORD lastFightMs = 0u;

    while ((GetTickCount() - start) < maxFightMs) {
        if (IsPlayerOrPartyDead() || !IsCurrentMapLoadedForTravel()) {
            return;
        }

        if (MapMgr::GetMapId() == GWA3::MapIds::MAGUS_STONES) {
            PulseArachnisDefensiveHeroShouts("magus-travel-fight", -1);
        }

        float foeDistance = 99999.0f;
        const uint32_t foeId = DungeonCombat::FindNearestLivingEnemy(aggroRange, &foeDistance);
        if (foeId == 0u) {
            return;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (AgentMgr::IsCasting(me)) {
            WaitMs(100u);
            continue;
        }

        const DWORD now = GetTickCount();
        if ((now - lastAttackMs) >= 750u) {
            AgentMgr::Attack(foeId);
            lastAttackMs = now;
        }
        if ((now - lastFightMs) >= 500u) {
            DungeonBuiltinCombat::FightTargetWithBuiltinCombat(foeId);
            lastFightMs = now;
        }
        WaitMs(150u);
    }
}

DungeonNavigation::AggroMoveCallbacks MakeArachnisTravelAggroCallbacks(const float* fightRangeOverride = nullptr) {
    DungeonNavigation::AggroMoveCallbacks callbacks;
    callbacks.is_dead = &IsPlayerOrPartyDead;
    callbacks.is_map_loaded = &IsCurrentMapLoadedForTravel;
    callbacks.wait_ms = &TravelWaitMs;
    callbacks.fight_in_aggro = &ArachnisTravelFightInAggro;
    callbacks.user_data = const_cast<float*>(fightRangeOverride);
    return callbacks;
}

bool MoveToTravelPointWithOpportunisticAggro(
    const DungeonQuest::TravelPoint& point,
    uint32_t mapId,
    float tolerance,
    float fightRange,
    uint32_t timeoutMs,
    const char* context) {
    const bool magusDungeonApproach = ContainsNoCase(context, "RunMagusToDungeon");
    DWORD attemptStart = GetTickCount();
    uint32_t localReviveAttempts = 0u;
    const bool finalHixxApproach = ContainsNoCase(context, "final-hixx");
    const bool lateMagusPoint =
        finalHixxApproach ||
        point.y < -10000.0f ||
        (point.x < -8500.0f && point.y < -8000.0f);
    const bool wideMagusTailPoint = magusDungeonApproach && fightRange > 1250.0f;
    const bool preClearMagusPoint = false;
    const float lateMagusMinFightRange = finalHixxApproach ? 1400.0f : 0.0f;
    const float effectiveFightRange =
        magusDungeonApproach && lateMagusPoint && lateMagusMinFightRange > 0.0f && fightRange < lateMagusMinFightRange
            ? lateMagusMinFightRange
            : fightRange;
    const float travelFightRange =
        magusDungeonApproach && finalHixxApproach && effectiveFightRange > 1400.0f
            ? 1400.0f
            : effectiveFightRange;
    const float magusSafeHp = finalHixxApproach
        ? 0.75f
        : (lateMagusPoint ? 0.85f : 0.70f);

    auto stabilizeMagusApproach = [&]() -> bool {
        if (!magusDungeonApproach) {
            return true;
        }
        if (MapMgr::GetMapId() != mapId || PartyMgr::GetIsPartyDefeated()) {
            return false;
        }
        auto* me = AgentMgr::GetMyAgent();
        if (me == nullptr || me->hp <= 0.0f) {
            return false;
        }
        if (me->hp >= magusSafeHp) {
            return true;
        }
        LogArachnisMain("travel opportunistic low-hp stabilize context=%s hp=%.2f target=(%.0f, %.0f)",
                        context != nullptr ? context : "",
                        me->hp,
                        point.x,
                        point.y);
        ArachnisTravelFightInAggro(
            effectiveFightRange > (lateMagusPoint ? 2200.0f : 1800.0f)
                ? effectiveFightRange
                : (lateMagusPoint ? 2200.0f : 1800.0f),
            false,
            nullptr,
            true,
            lateMagusPoint ? 20000u : 7000u);
        const bool stable = WaitForPlayerAliveStable(
            mapId,
            1u,
            lateMagusPoint ? 45000u : 15000u,
            lateMagusPoint ? 2500u : 1000u,
            context,
            magusSafeHp);
        auto* after = AgentMgr::GetMyAgent();
        const bool aliveAfter = MapMgr::GetMapId() == mapId &&
                                after != nullptr &&
                                after->hp > 0.0f &&
                                !PartyMgr::GetIsPartyDefeated();
        const float afterDistance = DungeonCombat::DistanceToPoint(point.x, point.y);
        const bool stillNearTarget = afterDistance <= (tolerance + 600.0f);
        if (!stable || !aliveAfter || !stillNearTarget) {
            LogArachnisMain("travel opportunistic low-hp stabilize failed context=%s stable=%d alive=%d near=%d hp=%.2f minSafe=%.2f remaining=%.0f target=(%.0f, %.0f)",
                            context != nullptr ? context : "",
                            stable ? 1 : 0,
                            aliveAfter ? 1 : 0,
                            stillNearTarget ? 1 : 0,
                            after != nullptr ? after->hp : 0.0f,
                            magusSafeHp,
                            afterDistance,
                            point.x,
                            point.y);
            return false;
        }
        return true;
    };

    auto recoverFromLocalRevive = [&](float remaining) -> bool {
        if (!magusDungeonApproach ||
            MapMgr::GetMapId() != mapId ||
            PartyMgr::GetIsPartyDefeated() ||
            localReviveAttempts >= 4u) {
            return false;
        }

        ++localReviveAttempts;
        LogArachnisMain("travel opportunistic revive recovery context=%s attempt=%u remaining=%.0f target=(%.0f, %.0f)",
                        context != nullptr ? context : "",
                        localReviveAttempts,
                        remaining,
                        point.x,
                        point.y);
        if (!WaitForPlayerAliveStable(
                mapId,
                1u,
                45000u,
                2500u,
                context,
                magusSafeHp)) {
            return false;
        }
        float revivedRemaining = DungeonCombat::DistanceToPoint(point.x, point.y);
        if (lateMagusPoint && revivedRemaining > (tolerance + 3000.0f)) {
            LogArachnisMain("travel opportunistic revive recovery route-reset context=%s attempt=%u remaining=%.0f tolerance=%.0f target=(%.0f, %.0f)",
                            context != nullptr ? context : "",
                            localReviveAttempts,
                            revivedRemaining,
                            tolerance,
                            point.x,
                            point.y);
            return false;
        }
        ArachnisTravelFightInAggro(
            effectiveFightRange > (lateMagusPoint ? 2400.0f : 2000.0f)
                ? effectiveFightRange
                : (lateMagusPoint ? 2400.0f : 2000.0f),
            false,
            nullptr,
            true,
            lateMagusPoint ? 25000u : 15000u);
        if (!WaitForPlayerAliveStable(
                mapId,
                1u,
                lateMagusPoint ? 45000u : 20000u,
                lateMagusPoint ? 2500u : 1000u,
                context,
                magusSafeHp)) {
            return false;
        }
        revivedRemaining = DungeonCombat::DistanceToPoint(point.x, point.y);
        if (lateMagusPoint && revivedRemaining > (tolerance + 3000.0f)) {
            LogArachnisMain("travel opportunistic post-fight recovery route-reset context=%s attempt=%u remaining=%.0f tolerance=%.0f target=(%.0f, %.0f)",
                            context != nullptr ? context : "",
                            localReviveAttempts,
                            revivedRemaining,
                            tolerance,
                            point.x,
                            point.y);
            return false;
        }
        attemptStart = GetTickCount();
        return true;
    };

    while ((GetTickCount() - attemptStart) < timeoutMs) {
        if (MapMgr::GetMapId() != mapId ||
            (!MapMgr::GetIsMapLoaded() && !magusDungeonApproach)) {
            return false;
        }

        auto* currentMe = AgentMgr::GetMyAgent();
        if (magusDungeonApproach &&
            MapMgr::GetMapId() == mapId &&
            !PartyMgr::GetIsPartyDefeated() &&
            (currentMe == nullptr || currentMe->hp <= 0.0f)) {
            if (recoverFromLocalRevive(DungeonCombat::DistanceToPoint(point.x, point.y))) {
                continue;
            }
            return false;
        }

        if (DungeonCombat::DistanceToPoint(point.x, point.y) <= tolerance) {
            if (stabilizeMagusApproach()) {
                return true;
            }
            auto* afterStabilize = AgentMgr::GetMyAgent();
            if (magusDungeonApproach &&
                MapMgr::GetMapId() == mapId &&
                !PartyMgr::GetIsPartyDefeated() &&
                (afterStabilize == nullptr || afterStabilize->hp <= 0.0f) &&
                localReviveAttempts < 4u) {
                continue;
            }
            return false;
        }

        const DWORD elapsed = GetTickCount() - attemptStart;
        const uint32_t remainingBudget = elapsed < timeoutMs ? (timeoutMs - elapsed) : 1u;
        const float moveFightRange = 0.0f;

        if (preClearMagusPoint) {
            const uint32_t nearbyBeforeMove =
                DungeonCombat::CountLivingEnemiesInRange(effectiveFightRange);
            if (nearbyBeforeMove > 0u) {
                LogArachnisMain("travel opportunistic pre-clear context=%s enemies=%u fightRange=%.0f moveGate=%.0f target=(%.0f, %.0f)",
                                context != nullptr ? context : "",
                                nearbyBeforeMove,
                                effectiveFightRange,
                                moveFightRange,
                                point.x,
                                point.y);
                ArachnisTravelFightInAggro(
                    effectiveFightRange,
                    false,
                    nullptr,
                    true,
                    finalHixxApproach ? 60000u : (lateMagusPoint ? 45000u : 30000u));
                auto* afterPreClear = AgentMgr::GetMyAgent();
                if (MapMgr::GetMapId() != mapId ||
                    PartyMgr::GetIsPartyDefeated() ||
                    afterPreClear == nullptr ||
                    afterPreClear->hp <= 0.0f) {
                    if (recoverFromLocalRevive(DungeonCombat::DistanceToPoint(point.x, point.y))) {
                        continue;
                    }
                    return false;
                }
            }
        }

        if (magusDungeonApproach && lateMagusPoint) {
            auto* beforeMove = AgentMgr::GetMyAgent();
            if (beforeMove != nullptr &&
                beforeMove->hp > 0.0f &&
                beforeMove->hp < magusSafeHp) {
                LogArachnisMain("travel opportunistic pre-move hp stabilize context=%s hp=%.2f minSafe=%.2f fightRange=%.0f target=(%.0f, %.0f)",
                                context != nullptr ? context : "",
                                beforeMove->hp,
                                magusSafeHp,
                                effectiveFightRange,
                                point.x,
                                point.y);
                ArachnisTravelFightInAggro(
                    effectiveFightRange,
                    false,
                    nullptr,
                    true,
                    finalHixxApproach ? 45000u : 30000u);
                if (!WaitForPlayerAliveStable(
                        mapId,
                        1u,
                        finalHixxApproach ? 60000u : 45000u,
                        2500u,
                        context,
                        magusSafeHp)) {
                    if (recoverFromLocalRevive(DungeonCombat::DistanceToPoint(point.x, point.y))) {
                        continue;
                    }
                    return false;
                }
            }
        }

        DungeonNavigation::AggroMoveOptions options;
        options.profile = DungeonNavigation::AggroMoveProfile::Opportunistic;
        options.arrival_threshold = tolerance;
        options.move_enemy_gate_range = moveFightRange;
        options.move_budget_ms = remainingBudget;
        options.force_move_after_ms = finalHixxApproach
            ? 20000u
            : (wideMagusTailPoint ? 12000u : 8000u);
        options.opportunistic_fight_budget_ms = magusDungeonApproach
            ? (finalHixxApproach ? 18000u : (lateMagusPoint ? 12000u : (wideMagusTailPoint ? 9000u : 7000u)))
            : 3500u;
        options.blocked_limit = 20;
        options.log_prefix = "Arachnis";

        LogArachnisMain("travel opportunistic start context=%s map=%u target=(%.0f, %.0f) tolerance=%.0f fightRange=%.0f moveGate=%.0f timeout=%u localRevives=%u",
                        context != nullptr ? context : "",
                        mapId,
                        point.x,
                        point.y,
                        tolerance,
                        travelFightRange,
                        moveFightRange > 0.0f ? moveFightRange : travelFightRange,
                        remainingBudget,
                        localReviveAttempts);
        DungeonNavigation::AggroMoveTo(
            point.x,
            point.y,
            travelFightRange,
            MakeArachnisTravelAggroCallbacks(&effectiveFightRange),
            options);

        const float remaining = DungeonCombat::DistanceToPoint(point.x, point.y);
        auto* me = AgentMgr::GetMyAgent();
        const uint32_t currentMap = MapMgr::GetMapId();
        const bool loaded = MapMgr::GetIsMapLoaded();
        const bool sameMap = currentMap == mapId;
        const bool partyDefeated = PartyMgr::GetIsPartyDefeated();
        const bool playerAlive = me != nullptr && me->hp > 0.0f;
        const bool alive = playerAlive && !partyDefeated;
        const bool mapReady = sameMap && loaded;
        const bool nearTarget = remaining <= tolerance;
        const bool withinSoftDistance = magusDungeonApproach && remaining <= (tolerance + 600.0f);
        const bool tolerantMapReady = magusDungeonApproach && sameMap && me != nullptr;
        const bool softReached = (mapReady || tolerantMapReady) && alive && withinSoftDistance;
        const bool reached = ((mapReady && alive) ||
                              (tolerantMapReady && alive && (nearTarget || withinSoftDistance))) &&
                             (nearTarget || withinSoftDistance);
        LogArachnisMain("travel opportunistic end context=%s reached=%d soft=%d map=%u loaded=%d sameMap=%d alive=%d playerAlive=%d partyDefeated=%d hp=%.2f remaining=%.0f tolerance=%.0f localRevives=%u",
                        context != nullptr ? context : "",
                        reached ? 1 : 0,
                        softReached ? 1 : 0,
                        currentMap,
                        loaded ? 1 : 0,
                        sameMap ? 1 : 0,
                        alive ? 1 : 0,
                        playerAlive ? 1 : 0,
                        partyDefeated ? 1 : 0,
                        me ? me->hp : 0.0f,
                        remaining,
                        tolerance,
                        localReviveAttempts);

        if (reached) {
            if (stabilizeMagusApproach()) {
                return true;
            }
            auto* afterStabilize = AgentMgr::GetMyAgent();
            if (magusDungeonApproach &&
                sameMap &&
                !PartyMgr::GetIsPartyDefeated() &&
                (afterStabilize == nullptr || afterStabilize->hp <= 0.0f) &&
                localReviveAttempts < 4u) {
                continue;
            }
            return false;
        }

        if (magusDungeonApproach &&
            sameMap &&
            !partyDefeated &&
            !playerAlive &&
            localReviveAttempts < 4u) {
            if (recoverFromLocalRevive(remaining)) {
                continue;
            }
            return false;
        }

        return false;
    }

    return false;
}

bool FollowTravelPathWithOpportunisticAggro(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t mapId,
    float tolerance,
    float fightRange,
    uint32_t timeoutMs,
    const char* context) {
    if (points == nullptr || count <= 0) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        char stepContext[128];
        snprintf(stepContext, sizeof(stepContext), "%s step=%d/%d", context != nullptr ? context : "travel", i + 1, count);
        if (!MoveToTravelPointWithOpportunisticAggro(
                points[i],
                mapId,
                tolerance,
                fightRange,
                timeoutMs,
                stepContext)) {
            return false;
        }
    }
    return true;
}

bool MoveToTravelPointWithAggro(
    const DungeonQuest::TravelPoint& point,
    uint32_t mapId,
    float tolerance = 250.0f,
    float fightRange = 1200.0f,
    uint32_t timeoutMs = 30000u) {
    if (HasEffectOnlyFlameStaffBundle()) {
        return MoveToTravelPointWhileCarryingBundle(point, mapId, tolerance, timeoutMs, "travel-point-aggro");
    }

    return DungeonBuiltinCombat::MoveToPointWithAggro(
        point.x,
        point.y,
        mapId,
        tolerance,
        fightRange,
        timeoutMs);
}

bool FollowTravelPathWithAggro(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t mapId,
    float tolerance = 250.0f,
    float fightRange = 1200.0f,
    uint32_t timeoutMs = 30000u) {
    if (HasEffectOnlyFlameStaffBundle()) {
        for (int i = 0; i < count; ++i) {
            if (!MoveToTravelPointWhileCarryingBundle(points[i], mapId, tolerance, timeoutMs, "travel-path-aggro")) {
                return false;
            }
        }
        return true;
    }

    if (mapId == GWA3::MapIds::MAGUS_STONES) {
        return FollowTravelPathWithOpportunisticAggro(
            points,
            count,
            mapId,
            tolerance,
            fightRange,
            timeoutMs,
            "magus-travel-path");
    }

    return DungeonBuiltinCombat::FollowTravelPathWithAggro(
        points,
        count,
        mapId,
        tolerance,
        fightRange,
        timeoutMs);
}

DungeonNavigation::RouteFollowResult FollowRouteWithOpportunisticAggro(
    const RouteDefinition& route,
    const char* context,
    const DungeonNavigation::RouteFollowOptions& options) {
    DungeonNavigation::RouteFollowResult result;
    if (route.waypoints == nullptr || route.waypoint_count <= 0) {
        result.failed_index = 0;
        return result;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (me == nullptr) {
        result.failed_index = 0;
        return result;
    }

    int index = DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);

    AggroWaypointTraceContext waypointTrace;
    waypointTrace.route_name = route.name;
    waypointTrace.context = context;

    int retriesUsed = 0;
    while (index < route.waypoint_count) {
        if (route.map_id != 0u && MapMgr::GetMapId() != route.map_id) {
            result.map_changed = true;
            return result;
        }
        if (IsPlayerOrPartyDead()) {
            result.failed_index = index;
            return result;
        }

        const auto& waypoint = route.waypoints[index];
        float tolerance = options.default_tolerance;
        float fightRange = 1200.0f;
        if (waypoint.fight_range > 0.0f) {
            fightRange = waypoint.fight_range;
            if (options.use_waypoint_fight_range_as_tolerance) {
                tolerance = waypoint.fight_range;
            }
        }

        if (!HandleArachnisAggroWaypoint(
                waypoint,
                index,
                DungeonRoute::ClassifyWaypointLabel(waypoint.label),
                DungeonCombat::AggroWaypointPhase::BeforeAdvance,
                &waypointTrace)) {
            result.failed_index = index;
            return result;
        }

        const DungeonQuest::TravelPoint point{waypoint.x, waypoint.y};
        char stepContext[160];
        snprintf(stepContext,
                 sizeof(stepContext),
                 "%s route=%s waypoint=%d",
                 context != nullptr ? context : "",
                 route.name != nullptr ? route.name : "",
                 index);
        if (!MoveToTravelPointWithOpportunisticAggro(
                point,
                route.map_id,
                tolerance,
                fightRange,
                options.waypoint_timeout_ms,
                stepContext)) {
            if (retriesUsed < options.max_backtrack_retries) {
                ++retriesUsed;
                result.retries_used = retriesUsed;
                auto* retryMe = AgentMgr::GetMyAgent();
                LogArachnisMain("route retry route=%s context=%s failedIndex=%d retry=%d player=(%.0f, %.0f)",
                                route.name != nullptr ? route.name : "",
                                context != nullptr ? context : "",
                                index,
                                retriesUsed,
                                retryMe ? retryMe->x : 0.0f,
                                retryMe ? retryMe->y : 0.0f);
                ArachnisTravelFightInAggro(
                    fightRange > 1800.0f ? fightRange : 1800.0f,
                    false,
                    nullptr,
                    true,
                    6000u);
                WaitMs(500u);
                const int backtrack = options.backtrack_count > 0 ? options.backtrack_count : 1;
                index = (index > backtrack) ? (index - backtrack) : 0;
                continue;
            }
            result.failed_index = index;
            return result;
        }

        if (!HandleArachnisAggroWaypoint(
                waypoint,
                index,
                DungeonRoute::ClassifyWaypointLabel(waypoint.label),
                DungeonCombat::AggroWaypointPhase::AfterAdvance,
                &waypointTrace)) {
            result.failed_index = index;
            return result;
        }

        ++index;
    }

    result.completed = true;
    result.retries_used = retriesUsed;
    return result;
}

bool FollowRouteWithRetries(
    RouteId routeId,
    const char* context,
    const DungeonNavigation::RouteFollowOptions& options) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    const DWORD routeStart = GetTickCount();
    auto* me = AgentMgr::GetMyAgent();
    const int startIndex = GetNearestWaypointIndexForRoute(route);
    LogArachnisMain("route start route=%s context=%s map=%u startIndex=%d player=(%.0f, %.0f) aggro=%d",
                    route.name,
                    context != nullptr ? context : "",
                    route.map_id,
                    startIndex,
                    me ? me->x : 0.0f,
                    me ? me->y : 0.0f,
                    UsesAggroTraversal(routeId) ? 1 : 0);

    DungeonNavigation::RouteFollowResult followResult;
    if (UsesAggroTraversal(routeId)) {
        if (HasHeldBundle()) {
            followResult = FollowRouteWhileCarryingBundle(route, context, options);
        } else {
            DungeonCombat::AggroAdvanceOptions aggroOptions;
            DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(
                aggroOptions,
                options.waypoint_timeout_ms,
                false);
            const bool magusApproach = routeId == RouteId::RunMagusToDungeon;
            aggroOptions.clear_options.flag_heroes = false;
            aggroOptions.clear_options.change_target = true;
            aggroOptions.clear_options.call_target = magusApproach;
            aggroOptions.clear_options.timeout_ms = options.waypoint_timeout_ms;
            aggroOptions.clear_options.target_timeout_ms = options.waypoint_timeout_ms;
            if (magusApproach) {
                aggroOptions.clear_options.minimum_local_clear_range = 1900.0f;
                aggroOptions.clear_options.extra_clear_range = 300.0f;
                aggroOptions.clear_options.chase_during_clear = true;
                aggroOptions.clear_options.chase_distance = 950.0f;
                aggroOptions.clear_options.pickup_after_clear = false;
                aggroOptions.clear_options.quiet_confirmation_ms = 1000u;
                aggroOptions.clear_options.loop_wait_ms = 250u;
                aggroOptions.clear_options.fight_reissue_ms = 600u;
                aggroOptions.clear_options.attack_reissue_ms = 600u;
                aggroOptions.clear_options.timeout_ms = 75000u;
                aggroOptions.clear_options.target_timeout_ms = 25000u;
            } else {
                aggroOptions.clear_options.chase_during_clear = true;
            }

            const DungeonCombat::CombatCallbacks combatCallbacks = magusApproach
                ? MakeArachnisMagusCombatCallbacks()
                : DungeonBuiltinCombat::MakeCombatCallbacks();
            AggroWaypointTraceContext waypointTrace;
            waypointTrace.route_name = route.name;
            waypointTrace.context = context;
            DungeonCombat::AggroWaypointCallbacks waypointCallbacks;
            waypointCallbacks.on_waypoint = &HandleArachnisAggroWaypoint;
            waypointCallbacks.user_data = &waypointTrace;
            if (magusApproach && route.waypoint_count > 1) {
                const int conservativeCount = route.waypoint_count > 8 ? 7 : (route.waypoint_count - 1);
                const bool runConservativePrefix = startIndex < conservativeCount;
                if (runConservativePrefix) {
                    followResult = DungeonCombat::FollowWaypointsWithAggro(
                        route.waypoints,
                        conservativeCount,
                        route.map_id,
                        combatCallbacks,
                        options,
                        aggroOptions,
                        waypointCallbacks);
                } else {
                    followResult.completed = true;
                }
                if (followResult.completed && !followResult.map_changed) {
                    const int tailStartIndex = startIndex > conservativeCount ? startIndex : conservativeCount;
                    for (int tailIndex = tailStartIndex; tailIndex < route.waypoint_count; ++tailIndex) {
                        const auto& tailWaypoint = route.waypoints[tailIndex];
                        const DungeonQuest::TravelPoint tailPoint{
                            tailWaypoint.x,
                            tailWaypoint.y};
                        const bool finalWaypoint = tailIndex == route.waypoint_count - 1;
                        const bool finalStagingWaypoint = tailIndex == route.waypoint_count - 2;
                        const bool lateMagusTail = tailIndex >= 13;
                        const bool midMagusTail = tailIndex >= 9;
                        const bool dangerMagusTail = tailIndex >= 7;
                        const char* tailContext =
                            finalWaypoint ? "RunMagusToDungeon final-hixx-approach" : "RunMagusToDungeon opportunistic tail";
                        const float tailFightRange = finalWaypoint
                            ? 1400.0f
                            : (lateMagusTail ? 1600.0f : (midMagusTail ? 1500.0f : (dangerMagusTail ? 1400.0f : 1200.0f)));
                        const uint32_t tailTimeoutMs = finalWaypoint ? 60000u : 120000u;
                        const float tailTolerance = finalWaypoint
                            ? 3000.0f
                            : (finalStagingWaypoint ? 900.0f : (tailWaypoint.fight_range > 0.0f ? tailWaypoint.fight_range : 1300.0f));
                        LogArachnisMain("route opportunistic tail route=%s context=%s index=%d label=%s target=(%.0f, %.0f) tolerance=%.0f final=%d",
                                        route.name,
                                        tailContext,
                                        tailIndex,
                                        tailWaypoint.label != nullptr ? tailWaypoint.label : "",
                                        tailWaypoint.x,
                                        tailWaypoint.y,
                                        tailTolerance,
                                        finalWaypoint ? 1 : 0);
                        const bool reached = MoveToTravelPointWithOpportunisticAggro(
                            tailPoint,
                            route.map_id,
                            tailTolerance,
                            tailFightRange,
                            tailTimeoutMs,
                            tailContext);
                        if (!reached) {
                            followResult.completed = false;
                            followResult.failed_index = tailIndex;
                            LogArachnisMain("route opportunistic tail failed route=%s context=%s index=%d currentMap=%u",
                                            route.name,
                                            context != nullptr ? context : "",
                                            tailIndex,
                                            MapMgr::GetMapId());
                            break;
                        }
                    }
                }
            } else if (routeId == RouteId::Level1Phase2 && route.waypoint_count > 14) {
                constexpr int kPhase2StaffWaypointIndex = 13;
                const bool alreadyHoldingStaff = HasHeldBundle();
                if (!alreadyHoldingStaff) {
                    int phase2StartIndex = startIndex;
                    if (phase2StartIndex < 0) {
                        phase2StartIndex = 0;
                    }
                    if (phase2StartIndex > kPhase2StaffWaypointIndex) {
                        phase2StartIndex = kPhase2StaffWaypointIndex;
                    }
                    const int phase2WaypointCount = kPhase2StaffWaypointIndex - phase2StartIndex + 1;
                    LogArachnisMain("route phase2 split pre-staff route=%s context=%s startIndex=%d staffIndex=%d count=%d",
                                    route.name,
                                    context != nullptr ? context : "",
                                    phase2StartIndex,
                                    kPhase2StaffWaypointIndex,
                                    phase2WaypointCount);
                    followResult = DungeonCombat::FollowWaypointsWithAggro(
                        &route.waypoints[phase2StartIndex],
                        phase2WaypointCount,
                        route.map_id,
                        combatCallbacks,
                        options,
                        aggroOptions,
                        waypointCallbacks);
                    if (!followResult.completed && followResult.failed_index >= 0) {
                        followResult.failed_index += phase2StartIndex;
                    }
                } else {
                    followResult.completed = true;
                }

                if (followResult.completed && !followResult.map_changed) {
                    if (!HasHeldBundle()) {
                        followResult.completed = false;
                        followResult.failed_index = kPhase2StaffWaypointIndex;
                        LogArachnisMain("route phase2 split abort no staff after pickup route=%s context=%s",
                                        route.name,
                                        context != nullptr ? context : "");
                    } else {
                        LogArachnisMain("route phase2 split handoff to bundle-carry route=%s context=%s",
                                        route.name,
                                        context != nullptr ? context : "");
                        followResult = FollowRouteWhileCarryingBundle(route, context, options);
                    }
                }
            } else {
                followResult = DungeonCombat::FollowWaypointsWithAggro(
                    route.waypoints,
                    route.waypoint_count,
                    route.map_id,
                    combatCallbacks,
                    options,
                    aggroOptions,
                    waypointCallbacks);
            }
        }
    } else {
        followResult = DungeonNavigation::FollowWaypoints(
            route.waypoints,
            route.waypoint_count,
            route.map_id,
            options);
    }
    LogArachnisMain("route result route=%s context=%s completed=%d mapChanged=%d failedIndex=%d retries=%d durationMs=%lu",
                    route.name,
                    context != nullptr ? context : "",
                    followResult.completed ? 1 : 0,
                    followResult.map_changed ? 1 : 0,
                    followResult.failed_index,
                    followResult.retries_used,
                    static_cast<unsigned long>(GetTickCount() - routeStart));
    if (followResult.completed || followResult.map_changed) {
        return true;
    }

    LogRouteFailureSnapshot(route, context, followResult);
    if (followResult.failed_index >= 0 && followResult.failed_index < route.waypoint_count) {
        LogBot("Arachnis: failed %s at waypoint %d (%s) after %d retries",
               context,
               followResult.failed_index,
               route.waypoints[followResult.failed_index].label,
               followResult.retries_used);
    } else {
        LogBot("Arachnis: failed %s after %d retries",
               context,
               followResult.retries_used);
    }
    return false;
}

bool MaybeAcquireBlessing(RouteId routeId) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    const int startIndex = GetNearestWaypointIndexForRoute(route);
    if (startIndex < 0) {
        return false;
    }

    const BlessingAnchor* blessing = FindBlessingAnchor(routeId, startIndex);
    if (blessing == nullptr) {
        return true;
    }

    if (!MoveToTravelPoint({blessing->x, blessing->y}, route.map_id, 300.0f, 15000u)) {
        LogBot("Arachnis: failed reaching blessing anchor on %s", route.name);
        return false;
    }

    GWA3::DungeonEffects::BlessingAcquireOptions options;
    options.required_title_id = blessing->required_title_id;
    options.accept_dialog_id = blessing->accept_dialog_id;
    options.dialog_retries = 3;
    options.dialog_delay_ms = 2500u;
    const auto result = GWA3::DungeonEffects::TryAcquireBlessingAt(blessing->x, blessing->y, options);
    if (result.confirmed) {
        return true;
    }

    if (result.dialog_sent) {
        WaitMs(1500u);
        if (GWA3::DungeonEffects::HasBlessing()) {
            return true;
        }
        LogBot("Arachnis: blessing not yet confirmed on %s, continuing after dialog send", route.name);
        return true;
    }

    if (!result.confirmed) {
        LogBot("Arachnis: blessing acquisition failed on %s (npc=%u dialogSent=%d)",
               route.name,
               result.npc_id,
               result.dialog_sent ? 1 : 0);
        return false;
    }

    return true;
}

bool ExecuteRoute(RouteId routeId, const char* context, bool waitForTransition = false) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    LogArachnisMain("execute route route=%s context=%s waitForTransition=%d",
                    route.name,
                    context != nullptr ? context : "",
                    waitForTransition ? 1 : 0);
    if (!MaybeAcquireBlessing(routeId)) {
        LogArachnisMain("execute route blessing failed route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        return false;
    }

    if (routeId == RouteId::RunMagusToDungeon) {
        LogArachnisMain("execute route waiting for hero setup route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        if (!WaitForArachnisLiveHeroSetup(Bot::GetConfig(), "route-magus-pre-entry", 20000u)) {
            LogBot("Arachnis: live hero setup invalid before Magus dungeon approach");
            return false;
        }
    }

    DungeonNavigation::RouteFollowOptions options;
    options.use_waypoint_fight_range_as_tolerance = true;
    options.waypoint_timeout_ms = 60000u;
    options.max_backtrack_retries = 3;
    if (routeId == RouteId::RunMagusToDungeon) {
        options.waypoint_timeout_ms = 120000u;
        options.max_backtrack_retries = 5;
    } else if (UsesAggroTraversal(routeId)) {
        options.waypoint_timeout_ms = 120000u;
        options.max_backtrack_retries = 5;
    }

    bool routeCompleted = FollowRouteWithRetries(routeId, context, options);
    if (!routeCompleted &&
        routeId == RouteId::Level1Phase2 &&
        !HasHeldBundle()) {
        LogArachnisMain("execute route phase2 no-staff failure is recoverable route=%s context=%s currentMap=%u",
                        route.name,
                        context != nullptr ? context : "",
                        MapMgr::GetMapId());
    }

    if (!routeCompleted &&
        UsesAggroTraversal(routeId) &&
        route.map_id != 0u &&
        MapMgr::GetMapId() == route.map_id) {
        const bool level1Phase2 = routeId == RouteId::Level1Phase2;
        const uint32_t recoveryTimeoutMs =
            routeId == RouteId::RunMagusToDungeon ? 120000u : (level1Phase2 ? 90000u : 45000u);
        const uint32_t recoveryStableMs =
            routeId == RouteId::RunMagusToDungeon ? 2500u : (level1Phase2 ? 2000u : 1000u);
        const float recoveryMinHp =
            routeId == RouteId::RunMagusToDungeon ? 0.30f : (level1Phase2 ? 0.35f : 0.20f);
        const uint32_t maxRecoveryAttempts =
            routeId == RouteId::RunMagusToDungeon ? 6u : (level1Phase2 ? 4u : 2u);
        for (uint32_t recoveryAttempt = 1u;
             recoveryAttempt <= maxRecoveryAttempts && !routeCompleted;
             ++recoveryAttempt) {
            LogArachnisMain("execute route retry after recovery route=%s context=%s attempt=%u currentMap=%u partyDefeated=%d held=%d",
                            route.name,
                            context != nullptr ? context : "",
                            recoveryAttempt,
                            MapMgr::GetMapId(),
                            PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                            HasHeldBundle() ? 1 : 0);
            if (!WaitForPlayerAliveStable(
                    route.map_id,
                    1u,
                    recoveryTimeoutMs,
                    recoveryStableMs,
                    context,
                    recoveryMinHp)) {
                break;
            }
            routeCompleted = FollowRouteWithRetries(routeId, context, options);
        }
    }

    if (!routeCompleted) {
        return false;
    }

    if (!waitForTransition) {
        LogArachnisMain("execute route finished route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        return true;
    }

    if (route.next_map_id == 0u || route.waypoint_count <= 0) {
        LogArachnisMain("execute route no transition route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        return true;
    }

    const auto& zonePoint = route.waypoints[route.waypoint_count - 1];
    LogArachnisMain("execute route transition route=%s context=%s zoneTarget=(%.0f, %.0f) nextMap=%u",
                    route.name,
                    context != nullptr ? context : "",
                    zonePoint.x,
                    zonePoint.y,
                    route.next_map_id);
    const ZoneTransitionAttemptResult transition = ZoneThroughPointWithTransitionHooks(
        zonePoint.x,
        zonePoint.y,
        route.next_map_id,
        context);
    if (!transition.zoned) {
        LogBot("Arachnis: failed zoning after %s", route.name);
        LogArachnisMain("execute route transition failed route=%s context=%s",
                        route.name,
                        context != nullptr ? context : "");
        return false;
    }
    LogArachnisMain("execute route transition ready route=%s context=%s nextMap=%u ready=%d",
                    route.name,
                    context != nullptr ? context : "",
                    route.next_map_id,
                    transition.ready ? 1 : 0);
    return transition.ready;
}

bool HasHeldBundle() {
    if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
        return true;
    }

    if (DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(kAsuraFlameStaffModelId) != 0u) {
        return true;
    }

    const uint32_t myId = AgentMgr::GetMyId();
    return myId != 0u && EffectMgr::HasEffect(myId, kAsuraFlameStaffEffectSkillId);
}

bool HasEffectOnlyFlameStaffBundle() {
    if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
        return false;
    }
    if (DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(kAsuraFlameStaffModelId) != 0u) {
        return false;
    }

    const uint32_t myId = AgentMgr::GetMyId();
    return myId != 0u && EffectMgr::HasEffect(myId, kAsuraFlameStaffEffectSkillId);
}

float GetFlameStaffEffectRemainingSeconds() {
    const uint32_t myId = AgentMgr::GetMyId();
    return myId != 0u ? EffectMgr::GetEffectTimeRemaining(myId, kAsuraFlameStaffEffectSkillId) : 0.0f;
}

bool IsPlayerDeadForStaffSafeSkills() {
    auto* me = AgentMgr::GetMyAgent();
    return me == nullptr || me->hp <= 0.0f || PartyMgr::GetIsPartyDefeated();
}

void StaffSafeWaitMs(uint32_t ms) {
    WaitMs(static_cast<DWORD>(ms));
}

void StaffSafeNoAutoAttack(uint32_t targetId) {
    (void)targetId;
}

bool RunStaffSafeCombatAssist(float range, uint32_t budgetMs, const char* context) {
    DungeonSkill::CachedSkill skillCache[8] = {};
    if (!DungeonSkill::BuildSkillCache(skillCache)) {
        return false;
    }

    const DWORD start = GetTickCount();
    uint32_t currentTargetId = 0u;
    DWORD lastHeroCallMs = 0u;
    int usedCount = 0;
    while ((GetTickCount() - start) < budgetMs) {
        if (MapMgr::GetMapId() == 0u || MapMgr::GetLoadingState() != 1u || IsPlayerDeadForStaffSafeSkills()) {
            break;
        }

        if (ContainsNoCase(context, "phase2")) {
            PulseArachnisDefensiveHeroShouts(context, -1);
        }

        float enemyDistance = 0.0f;
        const uint32_t targetId = DungeonCombat::FindNearestLivingEnemy(range, &enemyDistance);
        if (targetId == 0u) {
            return usedCount > 0;
        }

        const DWORD now = GetTickCount();
        if (targetId != currentTargetId || (now - lastHeroCallMs) >= 2500u) {
            AgentMgr::ChangeTarget(targetId);
            AgentMgr::CallTarget(targetId);
            currentTargetId = targetId;
            lastHeroCallMs = now;
        }

        bool usedThisStep[8] = {};
        DungeonCombatRoutine::SkillExecutionContext skillContext;
        skillContext.skill_cache = skillCache;
        skillContext.skill_used_this_step = usedThisStep;
        skillContext.skill_count = 8u;
        skillContext.wait_ms = &StaffSafeWaitMs;
        skillContext.is_dead = &IsPlayerDeadForStaffSafeSkills;

        DungeonCombatRoutine::SkillActionResult action = {};
        const bool acted = DungeonCombatRoutine::ExecuteCombatStep(
            targetId,
            skillContext,
            nullptr,
            action);
        if (acted && action.used_skill) {
            ++usedCount;
            LogArachnisMain("bundle-carry staff-safe skill context=%s target=%u distance=%.0f slot=%d skill=%u",
                            context != nullptr ? context : "",
                            targetId,
                            enemyDistance,
                            action.slot,
                            action.skill_id);
            continue;
        }

        WaitMs(350u);
    }

    return usedCount > 0;
}

bool WaitForHeldBundleState(bool expectedHeld, uint32_t timeoutMs, const char* context) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (HasHeldBundle() == expectedHeld) {
            LogHeldBundleState(context);
            return true;
        }
        WaitMs(100u);
    }

    LogHeldBundleState(context);
    return false;
}

bool WaitForMovementToSettle(uint32_t timeoutMs, const char* context) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = AgentMgr::GetMyAgent();
        if (me != nullptr && me->move_x == 0.0f && me->move_y == 0.0f) {
            LogHeldBundleState(context);
            return true;
        }
        WaitMs(100u);
    }

    LogHeldBundleState(context);
    return false;
}

bool TryDropFlameStaffBundle(const char* context) {
    const uint32_t myId = AgentMgr::GetMyId();
    const auto* effect = myId != 0u
                             ? EffectMgr::GetEffectBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                             : nullptr;
    const auto* buff = myId != 0u
                           ? EffectMgr::GetBuffBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                           : nullptr;

    LogArachnisMain(
        "flame-staff native drop probe context=%s heldItem=%u effectSkill=%u effectId=%u buffSkill=%u buffId=%u target=%u",
        context != nullptr ? context : "",
        DungeonInteractions::GetHeldBundleItemId(),
        effect != nullptr ? effect->skill_id : 0u,
        effect != nullptr ? effect->effect_id : 0u,
        buff != nullptr ? buff->skill_id : 0u,
        buff != nullptr ? buff->buff_id : 0u,
        buff != nullptr ? buff->target_agent_id : 0u);
    if (buff == nullptr || buff->buff_id == 0u) {
        return false;
    }

    const bool dropped = EffectMgr::DropBuff(buff->buff_id);
    LogBot("Arachnis: native flame staff drop context=%s buffId=%u result=%d",
           context != nullptr ? context : "",
           buff->buff_id,
           dropped ? 1 : 0);
    LogArachnisMain("flame-staff native drop context=%s buffId=%u result=%d",
                    context != nullptr ? context : "",
                    buff->buff_id,
                    dropped ? 1 : 0);
    return dropped;
}

void LogHeldBundleState(
    const char* context,
    const DungeonRoute::Waypoint* waypoint,
    int waypointIndex) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t heldItemId = DungeonInteractions::GetHeldBundleItemId();
    const uint32_t heldOrEquippedStaffItemId =
        DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(kAsuraFlameStaffModelId);
    const uint32_t myId = AgentMgr::GetMyId();
    const auto* effect = myId != 0u
                             ? EffectMgr::GetEffectBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                             : nullptr;
    const auto* buff = myId != 0u
                           ? EffectMgr::GetBuffBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                           : nullptr;
    const bool hasEffect = effect != nullptr;

    if (waypoint != nullptr) {
        LogBot("Arachnis: %s waypoint %d (%s) player=(%.0f, %.0f) heldItem=%u staffItem=%u effect=%d effectId=%u remaining=%.1f buffId=%u",
               context,
               waypointIndex,
               waypoint->label != nullptr ? waypoint->label : "",
               me ? me->x : 0.0f,
               me ? me->y : 0.0f,
               heldItemId,
               heldOrEquippedStaffItemId,
               hasEffect ? 1 : 0,
               effect != nullptr ? effect->effect_id : 0u,
               hasEffect ? GetFlameStaffEffectRemainingSeconds() : 0.0f,
               buff != nullptr ? buff->buff_id : 0u);
        LogArachnisMain("%s waypoint=%d label=%s player=(%.0f, %.0f) heldItem=%u staffItem=%u effect=%d effectId=%u remaining=%.1f buffId=%u",
                        context,
                        waypointIndex,
                        waypoint->label != nullptr ? waypoint->label : "",
                        me ? me->x : 0.0f,
                        me ? me->y : 0.0f,
                        heldItemId,
                        heldOrEquippedStaffItemId,
                        hasEffect ? 1 : 0,
                        effect != nullptr ? effect->effect_id : 0u,
                        hasEffect ? GetFlameStaffEffectRemainingSeconds() : 0.0f,
                        buff != nullptr ? buff->buff_id : 0u);
        return;
    }

    LogBot("Arachnis: %s player=(%.0f, %.0f) heldItem=%u staffItem=%u effect=%d effectId=%u remaining=%.1f buffId=%u",
           context,
           me ? me->x : 0.0f,
           me ? me->y : 0.0f,
           heldItemId,
           heldOrEquippedStaffItemId,
           hasEffect ? 1 : 0,
           effect != nullptr ? effect->effect_id : 0u,
           hasEffect ? GetFlameStaffEffectRemainingSeconds() : 0.0f,
           buff != nullptr ? buff->buff_id : 0u);
    LogArachnisMain("%s player=(%.0f, %.0f) heldItem=%u staffItem=%u effect=%d effectId=%u remaining=%.1f buffId=%u",
                    context,
                    me ? me->x : 0.0f,
                    me ? me->y : 0.0f,
                    heldItemId,
                    heldOrEquippedStaffItemId,
                    hasEffect ? 1 : 0,
                    effect != nullptr ? effect->effect_id : 0u,
                    hasEffect ? GetFlameStaffEffectRemainingSeconds() : 0.0f,
                    buff != nullptr ? buff->buff_id : 0u);
}

void LogBundleCarryTarget(
    const char* context,
    int legIndex,
    int legCount,
    const DungeonQuest::TravelPoint& point) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t heldItemId = DungeonInteractions::GetHeldBundleItemId();
    const uint32_t heldOrEquippedStaffItemId =
        DungeonBundle::GetHeldOrEquippedBundleItemIdByModel(kAsuraFlameStaffModelId);
    const uint32_t myId = AgentMgr::GetMyId();
    const auto* effect = myId != 0u
                             ? EffectMgr::GetEffectBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                             : nullptr;
    const auto* buff = myId != 0u
                           ? EffectMgr::GetBuffBySkillId(myId, kAsuraFlameStaffEffectSkillId)
                           : nullptr;
    const bool hasEffect = effect != nullptr;

    LogBot("Arachnis: %s leg %d/%d target=(%.0f, %.0f) player=(%.0f, %.0f) heldItem=%u staffItem=%u effect=%d effectId=%u buffId=%u",
           context,
           legIndex,
           legCount,
           point.x,
           point.y,
           me ? me->x : 0.0f,
           me ? me->y : 0.0f,
           heldItemId,
           heldOrEquippedStaffItemId,
           hasEffect ? 1 : 0,
           effect != nullptr ? effect->effect_id : 0u,
           buff != nullptr ? buff->buff_id : 0u);
    LogArachnisMain("%s leg=%d/%d target=(%.0f, %.0f) player=(%.0f, %.0f) heldItem=%u staffItem=%u held=%d effectOnly=%d effect=%d effectId=%u buffId=%u enemyDistance=%.0f",
                    context != nullptr ? context : "",
                    legIndex,
                    legCount,
                    point.x,
                    point.y,
                    me ? me->x : 0.0f,
                    me ? me->y : 0.0f,
                    heldItemId,
                    heldOrEquippedStaffItemId,
                    HasHeldBundle() ? 1 : 0,
                    HasEffectOnlyFlameStaffBundle() ? 1 : 0,
                    hasEffect ? 1 : 0,
                    effect != nullptr ? effect->effect_id : 0u,
                    buff != nullptr ? buff->buff_id : 0u,
                    DungeonCombat::GetNearestLivingEnemyDistance(1800.0f));
}

bool EnsureHeldBundleNearPlayer() {
    if (HasHeldBundle()) {
        LogHeldBundleState("ensure-held-bundle already holding");
        return true;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return false;
    }

    LogHeldBundleState("ensure-held-bundle begin");
    const bool picked = DungeonBundle::PickUpHeldBundleByModelNearPoint(
        me->x,
        me->y,
        kAsuraFlameStaffModelId,
        18000.0f,
        3,
        500u);
    if (!picked) {
        LogHeldBundleState("ensure-held-bundle no pickup");
        LogBot("Arachnis: expected flame staff bundle near player at (%.0f, %.0f)", me->x, me->y);
        return false;
    }
    WaitMs(500u);
    LogHeldBundleState("ensure-held-bundle end");
    if (!HasHeldBundle()) {
        LogBot("Arachnis: interacted with flame staff near player but no bundle is held");
        return false;
    }
    return true;
}

bool TryDirectFlameStaffPickupNearPoint(
    const char* context,
    float x,
    float y,
    float searchRadius,
    int attempts,
    uint32_t delayMs,
    bool requireHeldAfter) {
    const bool hadHeld = HasHeldBundle();
    for (int attempt = 0; attempt < attempts; ++attempt) {
        const uint32_t itemAgentId =
            DungeonInteractions::FindNearestItemByModel(x, y, searchRadius, kAsuraFlameStaffModelId);
        auto* itemAgentBase = itemAgentId != 0u ? AgentMgr::GetAgentByID(itemAgentId) : nullptr;
        auto* me = AgentMgr::GetMyAgent();
        uint32_t itemId = 0u;
        uint32_t modelId = 0u;
        uint32_t owner = 0u;
        float itemX = 0.0f;
        float itemY = 0.0f;
        float playerDistance = 0.0f;
        if (itemAgentBase != nullptr && itemAgentBase->type == 0x400u) {
            const auto* itemAgent = static_cast<const AgentItem*>(itemAgentBase);
            itemId = itemAgent->item_id;
            owner = itemAgent->owner;
            itemX = itemAgent->x;
            itemY = itemAgent->y;
            const auto* item = ItemMgr::GetItemById(itemId);
            modelId = item != nullptr ? item->model_id : 0u;
            playerDistance = me != nullptr
                ? AgentMgr::GetDistance(me->x, me->y, itemX, itemY)
                : 0.0f;
        }

        LogArachnisMain("flame-staff direct-pickup context=%s attempt=%d/%d itemAgent=%u item=%u model=%u owner=%u item=(%.0f, %.0f) player=(%.0f, %.0f) dist=%.0f heldBefore=%d requireHeld=%d",
                        context != nullptr ? context : "",
                        attempt + 1,
                        attempts,
                        itemAgentId,
                        itemId,
                        modelId,
                        owner,
                        itemX,
                        itemY,
                        me != nullptr ? me->x : 0.0f,
                        me != nullptr ? me->y : 0.0f,
                        playerDistance,
                        HasHeldBundle() ? 1 : 0,
                        requireHeldAfter ? 1 : 0);

        if (itemAgentId == 0u) {
            WaitMs(delayMs);
            if (HasHeldBundle()) {
                return true;
            }
            continue;
        }

        if (me != nullptr && playerDistance > 120.0f) {
            const auto approach = DungeonNavigation::MoveToAgent(
                itemAgentId,
                120.0f,
                5000u,
                250u,
                MapMgr::GetMapId());
            auto* afterApproach = AgentMgr::GetMyAgent();
            const float afterDistance =
                afterApproach != nullptr
                    ? AgentMgr::GetDistance(afterApproach->x, afterApproach->y, itemX, itemY)
                    : 99999.0f;
            LogArachnisMain("flame-staff direct-pickup approach context=%s attempt=%d arrived=%d timedOut=%d mapChanged=%d dist=%.0f",
                            context != nullptr ? context : "",
                            attempt + 1,
                            approach.arrived ? 1 : 0,
                            approach.timed_out ? 1 : 0,
                            approach.map_changed ? 1 : 0,
                            afterDistance);
        }

        const DWORD pickupStart = GetTickCount();
        int pulse = 0;
        while ((GetTickCount() - pickupStart) < 6000u && pulse < 24) {
            if (HasHeldBundle()) {
                LogArachnisMain("flame-staff direct-pickup acquired context=%s attempt=%d pulse=%d",
                                context != nullptr ? context : "",
                                attempt + 1,
                                pulse);
                return true;
            }

            auto* currentItemAgent = AgentMgr::GetAgentByID(itemAgentId);
            if (currentItemAgent == nullptr ||
                currentItemAgent != itemAgentBase ||
                currentItemAgent->type != 0x400u) {
                WaitMs(500u);
                LogArachnisMain("flame-staff direct-pickup ground-agent changed context=%s attempt=%d pulse=%d hasHeld=%d",
                                context != nullptr ? context : "",
                                attempt + 1,
                                pulse,
                                HasHeldBundle() ? 1 : 0);
                if (HasHeldBundle()) {
                    return true;
                }
                break;
            }

            ItemMgr::PickUpItem(itemAgentId);
            if (pulse >= 4 && (pulse % 2) == 0) {
                const bool worldActionQueued =
                    AgentMgr::InteractAgentWorldAction(itemAgentId, false);
                LogArachnisMain("flame-staff direct-pickup world-action context=%s attempt=%d pulse=%d itemAgent=%u queued=%d",
                                context != nullptr ? context : "",
                                attempt + 1,
                                pulse,
                                itemAgentId,
                                worldActionQueued ? 1 : 0);
            }
            if ((pulse % 4) == 3) {
                (void)AgentMgr::ActionInteract();
            }

            WaitMs(250u);
            ++pulse;
        }

        LogHeldBundleState(context);
    }

    return HasHeldBundle() || (!requireHeldAfter && hadHeld);
}

bool WaitForFlameStaffExpiryAtPoint(float x, float y, uint32_t timeoutMs, const char* context) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = AgentMgr::GetMyAgent();
        const float remaining = GetFlameStaffEffectRemainingSeconds();
        LogArachnisMain("flame-staff expiry-wait context=%s player=(%.0f, %.0f) target=(%.0f, %.0f) held=%d remaining=%.1f",
                        context != nullptr ? context : "",
                        me != nullptr ? me->x : 0.0f,
                        me != nullptr ? me->y : 0.0f,
                        x,
                        y,
                        HasHeldBundle() ? 1 : 0,
                        remaining);
        if (!HasHeldBundle()) {
            return true;
        }
        if (me == nullptr || me->hp <= 0.0f || PartyMgr::GetIsPartyDefeated()) {
            return false;
        }
        AgentMgr::Move(x, y);
        if (DungeonCombat::GetNearestLivingEnemyDistance(1800.0f) <= 1600.0f) {
            (void)RunStaffSafeCombatAssist(1800.0f, 5000u, "flame-staff-refresh-wait");
        } else {
            WaitMs(1000u);
        }
    }
    return !HasHeldBundle();
}

DungeonNavigation::RouteFollowResult FollowRouteWhileCarryingBundle(
    const RouteDefinition& route,
    const char* context,
    const DungeonNavigation::RouteFollowOptions& options) {
    DungeonNavigation::RouteFollowResult result;
    auto* me = AgentMgr::GetMyAgent();
    if (me == nullptr || route.waypoints == nullptr || route.waypoint_count <= 0) {
        result.failed_index = 0;
        return result;
    }

    int index = DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);
    if (index < 0) {
        result.failed_index = 0;
        return result;
    }

    AggroWaypointTraceContext trace;
    trace.route_name = route.name;
    trace.context = context;

    LogArachnisMain("bundle-carry route start route=%s context=%s startIndex=%d player=(%.0f, %.0f)",
                    route.name,
                    context != nullptr ? context : "",
                    index,
                    me->x,
                    me->y);

    for (; index < route.waypoint_count; ++index) {
        const auto& waypoint = route.waypoints[index];
        const auto labelKind = DungeonRoute::ClassifyWaypointLabel(waypoint.label);
        const bool staffPickupWaypoint = labelKind == DungeonRoute::WaypointLabelKind::AsuraFlameStaff;
        const bool level1Phase3CarryRoute =
            route.name != nullptr && std::strcmp(route.name, "Level1Phase3") == 0;
        float tolerance = options.default_tolerance;
        if (waypoint.fight_range > 0.0f && options.use_waypoint_fight_range_as_tolerance) {
            tolerance = waypoint.fight_range;
        }
        if (tolerance > 1200.0f) {
            tolerance = 1200.0f;
        }
        if (level1Phase3CarryRoute) {
            // AutoIt used the third waypoint field as fight range while still moving to <250.
            // These web burns need tight contact or the server snaps us back behind the web.
            tolerance = 300.0f;
        }

        if (!HandleArachnisAggroWaypoint(
                waypoint,
                index,
                labelKind,
                DungeonCombat::AggroWaypointPhase::BeforeAdvance,
                &trace)) {
            result.failed_index = index;
            return result;
        }

        const DWORD start = GetTickCount();
        const bool phase2StaffCarrySegment =
            route.name != nullptr && std::strcmp(route.name, "Level1Phase2") == 0 && index >= 2;
        const bool dangerousStaffCarrySegment =
            route.name != nullptr && std::strcmp(route.name, "Level1Phase2") == 0 && index >= 14;
        const bool finalPhase2StaffCarrySegment =
            route.name != nullptr && std::strcmp(route.name, "Level1Phase2") == 0 && index >= 17;
        const bool tightPhase3Corner =
            level1Phase3CarryRoute && index >= 4 && index <= 9;
        const bool latePhase3Choke =
            level1Phase3CarryRoute && index >= 8 && index <= 9;
        const bool phase3StaffCarrySegment =
            level1Phase3CarryRoute && index >= 3;
        const bool earlyPhase2StaffCarrySegment = phase2StaffCarrySegment && !dangerousStaffCarrySegment;
        const bool earlyPhase2ClearGateSegment =
            earlyPhase2StaffCarrySegment && !staffPickupWaypoint && index >= 2 && index <= 13;
        const uint32_t carryWaypointTimeoutMs =
            dangerousStaffCarrySegment
                ? 240000u
                : ((earlyPhase2ClearGateSegment || phase3StaffCarrySegment)
                       ? 180000u
                       : (options.waypoint_timeout_ms < 120000u ? 120000u : options.waypoint_timeout_ms));
        DWORD lastMoveMs = 0u;
        DWORD lastAssistMs = 0u;
        DWORD lastClearHoldLogMs = 0u;
        float lastProgressX = me->x;
        float lastProgressY = me->y;
        int lowMovementReissues = 0;
        bool arrived = false;
        while ((GetTickCount() - start) < carryWaypointTimeoutMs) {
            if (route.map_id != 0u && MapMgr::GetMapId() != route.map_id) {
                LogArachnisMain("bundle-carry abort map-change route=%s context=%s index=%d expectedMap=%u currentMap=%u",
                                route.name,
                                context != nullptr ? context : "",
                                index,
                                route.map_id,
                                MapMgr::GetMapId());
                result.map_changed = true;
                return result;
            }
            me = AgentMgr::GetMyAgent();
            if (me == nullptr) {
                LogArachnisMain("bundle-carry abort missing-player route=%s context=%s index=%d map=%u",
                                route.name,
                                context != nullptr ? context : "",
                                index,
                                MapMgr::GetMapId());
                result.failed_index = index;
                return result;
            }
            if (PartyMgr::GetIsPartyDefeated()) {
                LogArachnisMain("bundle-carry abort party-defeated route=%s context=%s index=%d map=%u player=(%.0f, %.0f) hp=%.2f",
                                route.name,
                                context != nullptr ? context : "",
                                index,
                                MapMgr::GetMapId(),
                                me->x,
                                me->y,
                                me->hp);
                result.failed_index = index;
                return result;
            }
            if (me->hp <= 0.0f) {
                LogArachnisMain("bundle-carry player down route=%s context=%s index=%d map=%u player=(%.0f, %.0f) partyDefeated=0",
                                route.name,
                                context != nullptr ? context : "",
                                index,
                                MapMgr::GetMapId(),
                                me->x,
                                me->y);
                if (!WaitForPlayerAliveStable(
                        route.map_id,
                        1u,
                        90000u,
                        4000u,
                        "bundle-carry revive",
                        0.70f)) {
                    if (route.map_id != 0u && MapMgr::GetMapId() != route.map_id) {
                        result.map_changed = true;
                        return result;
                    }
                    LogArachnisMain("bundle-carry abort revive-timeout route=%s context=%s index=%d map=%u partyDefeated=%d",
                                    route.name,
                                    context != nullptr ? context : "",
                                    index,
                                    MapMgr::GetMapId(),
                                    PartyMgr::GetIsPartyDefeated() ? 1 : 0);
                    result.failed_index = index;
                    return result;
                }
                WaitMs(500u);
                if (!HasHeldBundle() &&
                    !RecoverFlameStaffAfterRevive(route.name, index, route.map_id, context)) {
                    LogArachnisMain("bundle-carry abort missing-staff-after-revive route=%s context=%s index=%d map=%u",
                                    route.name,
                                    context != nullptr ? context : "",
                                    index,
                                    MapMgr::GetMapId());
                    result.failed_index = index;
                    return result;
                }
                LogArachnisMain("bundle-carry restart-after-revive route=%s context=%s previousIndex=%d map=%u",
                                route.name,
                                context != nullptr ? context : "",
                                index,
                                MapMgr::GetMapId());
                return FollowRouteWhileCarryingBundle(route, context, options);
            }
            if (phase2StaffCarrySegment) {
                PulseArachnisDefensiveHeroShouts(context, index);
            }
            const float lowHpThreshold = dangerousStaffCarrySegment
                ? (finalPhase2StaffCarrySegment ? 0.86f : 0.78f)
                : (staffPickupWaypoint ? 0.58f : (earlyPhase2ClearGateSegment ? 0.88f : (phase2StaffCarrySegment ? 0.78f : (phase3StaffCarrySegment ? 0.70f : 0.35f))));
            if (me->hp > 0.0f && me->hp < lowHpThreshold) {
                LogArachnisMain("bundle-carry low-hp pause route=%s context=%s index=%d phase2=%d danger=%d player=(%.0f, %.0f) hp=%.2f threshold=%.2f",
                                route.name,
                                context != nullptr ? context : "",
                                index,
                                phase2StaffCarrySegment ? 1 : 0,
                                dangerousStaffCarrySegment ? 1 : 0,
                                me->x,
                                me->y,
                                me->hp,
                                lowHpThreshold);
                AgentMgr::Move(me->x, me->y);
                (void)RunStaffSafeCombatAssist(
                    dangerousStaffCarrySegment ? (finalPhase2StaffCarrySegment ? 2200.0f : 2000.0f) : (earlyPhase2ClearGateSegment ? 2400.0f : (phase2StaffCarrySegment ? 1900.0f : 1500.0f)),
                    dangerousStaffCarrySegment ? (finalPhase2StaffCarrySegment ? 14000u : 12000u) : (earlyPhase2ClearGateSegment ? 15000u : (phase2StaffCarrySegment ? 10000u : 6000u)),
                    finalPhase2StaffCarrySegment ? "phase2-final-low-hp" : (dangerousStaffCarrySegment ? "phase2-post-staff-low-hp" : (phase2StaffCarrySegment ? "phase2-low-hp" : "low-hp")));
                const DWORD healStart = GetTickCount();
                const uint32_t healBudgetMs = dangerousStaffCarrySegment ? (finalPhase2StaffCarrySegment ? 20000u : 15000u) : (staffPickupWaypoint ? 6000u : (earlyPhase2ClearGateSegment ? 16000u : (phase2StaffCarrySegment ? 12000u : (phase3StaffCarrySegment ? 12000u : 8000u))));
                const float resumeHp = dangerousStaffCarrySegment ? 0.92f : (staffPickupWaypoint ? 0.78f : (earlyPhase2ClearGateSegment ? 0.96f : (phase2StaffCarrySegment ? 0.88f : (phase3StaffCarrySegment ? 0.85f : 0.70f))));
                while ((GetTickCount() - healStart) < healBudgetMs) {
                    me = AgentMgr::GetMyAgent();
                    if (me == nullptr || me->hp <= 0.0f || me->hp >= resumeHp || PartyMgr::GetIsPartyDefeated()) {
                        break;
                    }
                    WaitMs(500u);
                }
                lastMoveMs = 0u;
                continue;
            }

            const bool earlyPhase2UnderPressure = earlyPhase2StaffCarrySegment && me->hp < 0.75f;
            const float earlyPhase2TriggerDistance =
                earlyPhase2ClearGateSegment ? (earlyPhase2UnderPressure ? 2400.0f : 2200.0f)
                                            : (earlyPhase2UnderPressure ? 1400.0f : 850.0f);
            const uint32_t earlyPhase2AssistCooldownMs =
                earlyPhase2ClearGateSegment ? 1500u : (earlyPhase2UnderPressure ? 3000u : 6500u);
            const uint32_t earlyPhase2AssistBudgetMs =
                earlyPhase2ClearGateSegment ? 11000u : (earlyPhase2UnderPressure ? 7000u : 3500u);
            const float dangerousTriggerDistance = finalPhase2StaffCarrySegment ? 1900.0f : 2200.0f;
            const float enemyScanRange = phase3StaffCarrySegment ? 2200.0f : (dangerousStaffCarrySegment ? 2600.0f : (earlyPhase2ClearGateSegment ? 2600.0f : (phase2StaffCarrySegment ? 2000.0f : 1500.0f)));
            const float enemyTriggerDistance = staffPickupWaypoint ? 850.0f : (phase3StaffCarrySegment ? (latePhase3Choke ? 1600.0f : (tightPhase3Corner ? 1800.0f : 1500.0f)) : (dangerousStaffCarrySegment ? dangerousTriggerDistance : (earlyPhase2StaffCarrySegment ? earlyPhase2TriggerDistance : 1400.0f)));
            const uint32_t assistCooldownMs = staffPickupWaypoint ? 4000u : (phase3StaffCarrySegment ? 2000u : (dangerousStaffCarrySegment ? (finalPhase2StaffCarrySegment ? 1500u : 1000u) : (earlyPhase2StaffCarrySegment ? earlyPhase2AssistCooldownMs : 6000u)));
            const uint32_t assistBudgetMs = staffPickupWaypoint ? 4000u : (phase3StaffCarrySegment ? 10000u : (dangerousStaffCarrySegment ? (finalPhase2StaffCarrySegment ? 14000u : 15000u) : (earlyPhase2StaffCarrySegment ? earlyPhase2AssistBudgetMs : 5000u)));
            const float assistRange = staffPickupWaypoint ? 1200.0f : (phase3StaffCarrySegment ? 2000.0f : (dangerousStaffCarrySegment ? (finalPhase2StaffCarrySegment ? 2200.0f : 2400.0f) : (earlyPhase2ClearGateSegment ? 2400.0f : (phase2StaffCarrySegment ? 1900.0f : 1500.0f))));
            const float nearestEnemyDistance = DungeonCombat::GetNearestLivingEnemyDistance(enemyScanRange);
            const DWORD assistNow = GetTickCount();
            const DWORD waypointElapsedMs = assistNow - start;
            const uint32_t mandatoryClearHoldMs =
                finalPhase2StaffCarrySegment
                    ? 60000u
                    : (earlyPhase2ClearGateSegment ? 45000u : ((dangerousStaffCarrySegment || phase3StaffCarrySegment) ? 30000u : 0u));
            const float pressureMoveHpFloor =
                earlyPhase2ClearGateSegment ? 0.92f : (dangerousStaffCarrySegment ? (finalPhase2StaffCarrySegment ? 0.94f : 0.90f) : (phase3StaffCarrySegment ? 0.88f : 0.0f));
            const bool clearGateSegment =
                earlyPhase2ClearGateSegment || dangerousStaffCarrySegment || phase3StaffCarrySegment;
            const bool holdForLocalClear =
                clearGateSegment && (waypointElapsedMs < mandatoryClearHoldMs || me->hp < pressureMoveHpFloor);
            if (nearestEnemyDistance <= enemyTriggerDistance &&
                (lastAssistMs == 0u || (assistNow - lastAssistMs) >= assistCooldownMs)) {
                LogArachnisMain("bundle-carry enemy-near assist route=%s context=%s index=%d phase2=%d danger=%d player=(%.0f, %.0f) hp=%.2f enemyDistance=%.0f trigger=%.0f",
                                route.name,
                                context != nullptr ? context : "",
                                index,
                                phase2StaffCarrySegment ? 1 : 0,
                                dangerousStaffCarrySegment ? 1 : 0,
                                me->x,
                                me->y,
                                me->hp,
                                nearestEnemyDistance,
                                enemyTriggerDistance);
                AgentMgr::Move(me->x, me->y);
                (void)RunStaffSafeCombatAssist(
                    assistRange,
                    assistBudgetMs,
                    finalPhase2StaffCarrySegment ? "phase2-final-enemy-near" : (dangerousStaffCarrySegment ? "phase2-post-staff-enemy-near" : (phase2StaffCarrySegment ? "phase2-enemy-near" : "enemy-near")));
                lastAssistMs = GetTickCount();
                lastMoveMs = 0u;
                continue;
            }
            if (holdForLocalClear && nearestEnemyDistance <= enemyTriggerDistance) {
                if (lastClearHoldLogMs == 0u || (assistNow - lastClearHoldLogMs) >= 2000u) {
                    LogArachnisMain("bundle-carry hold-for-clear route=%s context=%s index=%d phase2=%d danger=%d player=(%.0f, %.0f) hp=%.2f enemyDistance=%.0f trigger=%.0f cooldownRemaining=%u",
                                    route.name,
                                    context != nullptr ? context : "",
                                    index,
                                    phase2StaffCarrySegment ? 1 : 0,
                                    dangerousStaffCarrySegment ? 1 : 0,
                                    me->x,
                                    me->y,
                                    me->hp,
                                    nearestEnemyDistance,
                                    enemyTriggerDistance,
                                    static_cast<unsigned>(assistCooldownMs > (assistNow - lastAssistMs) ? assistCooldownMs - (assistNow - lastAssistMs) : 0u));
                    lastClearHoldLogMs = assistNow;
                }
                AgentMgr::Move(me->x, me->y);
                WaitMs(250u);
                lastMoveMs = 0u;
                continue;
            }

            const float distance = AgentMgr::GetDistance(me->x, me->y, waypoint.x, waypoint.y);
            if (distance <= tolerance) {
                arrived = true;
                break;
            }

            const DWORD now = GetTickCount();
            if (lastMoveMs == 0u || (now - lastMoveMs) >= options.reissue_ms) {
                const float progress = AgentMgr::GetDistance(lastProgressX, lastProgressY, me->x, me->y);
                if (lastMoveMs != 0u && progress < 25.0f) {
                    ++lowMovementReissues;
                } else {
                    lowMovementReissues = 0;
                }
                lastProgressX = me->x;
                lastProgressY = me->y;

                if (lowMovementReissues >= 8 && index > 0) {
                    const auto& recoveryWaypoint = route.waypoints[index - 1];
                    LogArachnisMain("bundle-carry stuck recovery route=%s context=%s index=%d label=%s recoveryIndex=%d recoveryTarget=(%.0f, %.0f) player=(%.0f, %.0f) progress=%.0f",
                                    route.name,
                                    context != nullptr ? context : "",
                                    index,
                                    waypoint.label != nullptr ? waypoint.label : "",
                                    index - 1,
                                    recoveryWaypoint.x,
                                    recoveryWaypoint.y,
                                    me->x,
                                    me->y,
                                    progress);
                    AgentMgr::Move(recoveryWaypoint.x, recoveryWaypoint.y);
                    lastMoveMs = now;
                    lowMovementReissues = 0;
                    WaitMs(750u);
                    continue;
                }

                LogArachnisMain("bundle-carry move route=%s context=%s index=%d label=%s target=(%.0f, %.0f) player=(%.0f, %.0f) distance=%.0f tolerance=%.0f",
                                route.name,
                                context != nullptr ? context : "",
                                index,
                                waypoint.label != nullptr ? waypoint.label : "",
                                waypoint.x,
                                waypoint.y,
                                me->x,
                                me->y,
                                distance,
                                tolerance);
                AgentMgr::Move(waypoint.x, waypoint.y);
                lastMoveMs = now;
            }

            WaitMs(150u);
        }

        if (!arrived) {
            auto* timeoutMe = AgentMgr::GetMyAgent();
            const float remaining = timeoutMe != nullptr
                ? AgentMgr::GetDistance(timeoutMe->x, timeoutMe->y, waypoint.x, waypoint.y)
                : 0.0f;
            LogArachnisMain("bundle-carry abort timeout route=%s context=%s index=%d label=%s target=(%.0f, %.0f) player=(%.0f, %.0f) remaining=%.0f tolerance=%.0f timeoutMs=%u",
                            route.name,
                            context != nullptr ? context : "",
                            index,
                            waypoint.label != nullptr ? waypoint.label : "",
                            waypoint.x,
                            waypoint.y,
                            timeoutMe ? timeoutMe->x : 0.0f,
                            timeoutMe ? timeoutMe->y : 0.0f,
                            remaining,
                            tolerance,
                            carryWaypointTimeoutMs);
            result.failed_index = index;
            return result;
        }

        if (!HandleArachnisAggroWaypoint(
                waypoint,
                index,
                labelKind,
                DungeonCombat::AggroWaypointPhase::AfterAdvance,
                &trace)) {
            result.failed_index = index;
            return result;
        }
    }

    result.completed = true;
    return result;
}

bool HandleArachnisAggroWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    int waypointIndex,
    DungeonRoute::WaypointLabelKind labelKind,
    DungeonCombat::AggroWaypointPhase phase,
    void* userData) {
    const auto* trace = static_cast<const AggroWaypointTraceContext*>(userData);
    auto* me = AgentMgr::GetMyAgent();
    if (phase == DungeonCombat::AggroWaypointPhase::BeforeAdvance ||
        phase == DungeonCombat::AggroWaypointPhase::AfterAdvance) {
        LogArachnisMain(
            "aggro waypoint route=%s context=%s index=%d label=%s phase=%s target=(%.0f, %.0f) player=(%.0f, %.0f)",
            trace && trace->route_name ? trace->route_name : "",
            trace && trace->context ? trace->context : "",
            waypointIndex,
            waypoint.label != nullptr ? waypoint.label : "",
            phase == DungeonCombat::AggroWaypointPhase::BeforeAdvance ? "before-advance"
                                                                      : "after-advance",
            waypoint.x,
            waypoint.y,
            me ? me->x : 0.0f,
            me ? me->y : 0.0f);
    }

    if (trace && trace->route_name &&
        _stricmp(trace->route_name, "RunMagusToDungeon") == 0 &&
        phase == DungeonCombat::AggroWaypointPhase::BeforeAdvance) {
        PulseArachnisDefensiveHeroShouts("magus-before-advance", waypointIndex);
    }

    switch (labelKind) {
    case DungeonRoute::WaypointLabelKind::AsuraFlameStaff: {
        if (phase != DungeonCombat::AggroWaypointPhase::AfterAdvance) {
            return true;
        }

        LogArachnisMain("aggro waypoint flame-staff index=%d label=%s phase=after-advance",
                        waypointIndex,
                        waypoint.label != nullptr ? waypoint.label : "");
        LogHeldBundleState("flame-staff hook begin", &waypoint, waypointIndex);
        if (HasHeldBundle()) {
            (void)TryDirectFlameStaffPickupNearPoint(
                "flame-staff hook refresh",
                waypoint.x,
                waypoint.y,
                700.0f,
                2,
                750u,
                false);
            LogHeldBundleState("flame-staff hook refresh attempted", &waypoint, waypointIndex);
            WaitMs(1000u);
            LogHeldBundleState("flame-staff hook already holding", &waypoint, waypointIndex);
            return HasHeldBundle();
        }

        const bool pickedDirect = TryDirectFlameStaffPickupNearPoint(
            "flame-staff hook direct",
            waypoint.x,
            waypoint.y,
            600.0f,
            3,
            1000u,
            true);
        if (pickedDirect && HasHeldBundle()) {
            LogHeldBundleState("flame-staff hook direct picked", &waypoint, waypointIndex);
            return true;
        }

        const bool pickedNearby = DungeonBundle::PickUpNearestItemByModelNearPoint(
            waypoint.x,
            waypoint.y,
            kAsuraFlameStaffModelId,
            600.0f,
            3,
            500u);
        const bool pickedFallback = !pickedNearby && DungeonBundle::PickUpNearestItemByModelNearPoint(
                waypoint.x,
                waypoint.y,
                kAsuraFlameStaffModelId,
                18000.0f,
                3,
                500u);
        LogBot("Arachnis: flame-staff hook waypoint %d pickup nearby=%d fallback=%d",
               waypointIndex,
               pickedNearby ? 1 : 0,
               pickedFallback ? 1 : 0);
        if (!pickedNearby && !pickedFallback) {
            LogHeldBundleState("flame-staff hook no pickup", &waypoint, waypointIndex);
            LogBot("Arachnis: no flame staff bundle found at waypoint %d (%.0f, %.0f)",
                   waypointIndex,
                   waypoint.x,
                   waypoint.y);
            return false;
        }

        WaitMs(500u);
        LogHeldBundleState("flame-staff hook end", &waypoint, waypointIndex);
        if (!HasHeldBundle()) {
            LogBot("Arachnis: flame staff pickup at waypoint %d did not result in a held bundle",
                   waypointIndex);
            return false;
        }
        return true;
    }

    case DungeonRoute::WaypointLabelKind::StaffCheck: {
        if (phase != DungeonCombat::AggroWaypointPhase::BeforeAdvance) {
            return true;
        }

        LogArachnisMain("aggro waypoint staff-check index=%d label=%s phase=before-advance",
                        waypointIndex,
                        waypoint.label != nullptr ? waypoint.label : "");
        LogHeldBundleState("staff-check hook begin", &waypoint, waypointIndex);
        if (HasHeldBundle()) {
            LogHeldBundleState("staff-check hook pass", &waypoint, waypointIndex);
            return true;
        }

        LogBot("Arachnis: staff check at waypoint %d missing bundle; attempting reacquire",
               waypointIndex);
        const bool reacquired = EnsureHeldBundleNearPlayer();
        LogHeldBundleState(
            reacquired ? "staff-check hook reacquired" : "staff-check hook failed",
            &waypoint,
            waypointIndex);
        return reacquired;
    }

    default:
        return true;
    }
}

bool PickUpObjectiveBundle(const FlameStaffObjective& objective, uint32_t mapId) {
    auto* me = AgentMgr::GetMyAgent();
    LogArachnisMain("pickup-objective begin name=%s map=%u target=(%.0f, %.0f) player=(%.0f, %.0f)",
                    objective.name != nullptr ? objective.name : "",
                    mapId,
                    objective.pickup_point.x,
                    objective.pickup_point.y,
                    me != nullptr ? me->x : 0.0f,
                    me != nullptr ? me->y : 0.0f);
    LogHeldBundleState("pickup-objective state");

    if (HasHeldBundle()) {
        LogHeldBundleState("pickup-objective already holding");
        return true;
    }

    if (!MoveToTravelPointWithAggro(objective.pickup_point, mapId, 250.0f, 1200.0f, 60000u)) {
        me = AgentMgr::GetMyAgent();
        LogArachnisMain("pickup-objective move failed name=%s map=%u target=(%.0f, %.0f) player=(%.0f, %.0f)",
                        objective.name != nullptr ? objective.name : "",
                        mapId,
                        objective.pickup_point.x,
                        objective.pickup_point.y,
                        me != nullptr ? me->x : 0.0f,
                        me != nullptr ? me->y : 0.0f);
        LogHeldBundleState("pickup-objective move failed state");
        return false;
    }
    LogHeldBundleState("pickup-objective after move");
    if (HasHeldBundle()) {
        LogHeldBundleState("pickup-objective acquired during move");
        return true;
    }
    if (TryDirectFlameStaffPickupNearPoint(
            "pickup-objective direct",
            objective.pickup_point.x,
            objective.pickup_point.y,
            800.0f,
            4,
            1500u,
            true)) {
        LogHeldBundleState("pickup-objective direct picked");
        return true;
    }

    const bool pickedNearby = DungeonBundle::PickUpNearestItemByModelNearPoint(
        objective.pickup_point.x,
        objective.pickup_point.y,
        kAsuraFlameStaffModelId,
        600.0f,
        3,
        500u);
    if (pickedNearby && HasHeldBundle()) {
        LogHeldBundleState("pickup-objective picked nearby");
        return true;
    }
    LogArachnisMain("pickup-objective nearby result name=%s picked=%d hasHeld=%d",
                    objective.name != nullptr ? objective.name : "",
                    pickedNearby ? 1 : 0,
                    HasHeldBundle() ? 1 : 0);

    const bool pickedFallback = DungeonBundle::PickUpNearestItemByModelNearPoint(
        objective.pickup_point.x,
        objective.pickup_point.y,
        kAsuraFlameStaffModelId,
        18000.0f,
        3,
        500u);
    if (!pickedFallback) {
        LogBot("Arachnis: no flame staff bundle found near objective pickup at %.0f, %.0f",
               objective.pickup_point.x,
               objective.pickup_point.y);
        return false;
    }
    WaitMs(500u);
    if (!HasHeldBundle()) {
        LogHeldBundleState("pickup-objective fallback no held");
        LogBot("Arachnis: interacted with flame staff at %.0f, %.0f but did not pick it up",
               objective.pickup_point.x,
               objective.pickup_point.y);
        return false;
    }
    LogHeldBundleState("pickup-objective picked fallback");
    return true;
}

bool FollowRecoveryRouteToWaypoint(
    RouteId routeId,
    int targetWaypointIndex,
    const char* context,
    uint32_t maxAttempts = 1u,
    bool requireHeldBundle = true) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    if (route.waypoints == nullptr ||
        targetWaypointIndex < 0 ||
        targetWaypointIndex >= route.waypoint_count ||
        MapMgr::GetMapId() != route.map_id ||
        !MapMgr::GetIsMapLoaded()) {
        LogArachnisMain("recover-route invalid route=%s context=%s targetIndex=%d map=%u currentMap=%u loaded=%d",
                        route.name,
                        context != nullptr ? context : "",
                        targetWaypointIndex,
                        route.map_id,
                        MapMgr::GetMapId(),
                        MapMgr::GetIsMapLoaded() ? 1 : 0);
        return false;
    }

    int startIndex = GetNearestWaypointIndexForRoute(route);
    if (startIndex < 0) {
        startIndex = 0;
    }
    if (startIndex > targetWaypointIndex) {
        startIndex = targetWaypointIndex;
    }

    DungeonNavigation::RouteFollowOptions options;
    options.use_waypoint_fight_range_as_tolerance = true;
    options.waypoint_timeout_ms = 180000u;
    options.max_backtrack_retries = 3;

    DungeonCombat::AggroAdvanceOptions aggroOptions;
    DungeonBuiltinCombat::ConfigureBuiltinAggroAdvanceOptions(
        aggroOptions,
        options.waypoint_timeout_ms,
        false);

    AggroWaypointTraceContext waypointTrace;
    waypointTrace.route_name = route.name;
    waypointTrace.context = context;

    DungeonCombat::AggroWaypointCallbacks waypointCallbacks;
    waypointCallbacks.on_waypoint = &HandleArachnisAggroWaypoint;
    waypointCallbacks.user_data = &waypointTrace;

    if (maxAttempts == 0u) {
        maxAttempts = 1u;
    }

    for (uint32_t attempt = 1u; attempt <= maxAttempts; ++attempt) {
        if (!WaitForPlayerAliveStable(route.map_id, 1u, 90000u, 3000u, "recover-route alive", 0.70f)) {
            return false;
        }

        startIndex = GetNearestWaypointIndexForRoute(route);
        if (startIndex < 0) {
            startIndex = 0;
        }
        if (startIndex > targetWaypointIndex) {
            startIndex = targetWaypointIndex;
        }

        LogArachnisMain("recover-route start route=%s context=%s attempt=%u/%u startIndex=%d targetIndex=%d currentMap=%u",
                        route.name,
                        context != nullptr ? context : "",
                        attempt,
                        maxAttempts,
                        startIndex,
                        targetWaypointIndex,
                        MapMgr::GetMapId());

        DungeonNavigation::RouteFollowResult result = DungeonCombat::FollowWaypointsWithAggro(
            &route.waypoints[startIndex],
            targetWaypointIndex - startIndex + 1,
            route.map_id,
            DungeonBuiltinCombat::MakeCombatCallbacks(),
            options,
            aggroOptions,
            waypointCallbacks);

        LogArachnisMain("recover-route result route=%s context=%s attempt=%u/%u completed=%d mapChanged=%d failedIndex=%d retries=%d hasHeld=%d",
                        route.name,
                        context != nullptr ? context : "",
                        attempt,
                        maxAttempts,
                        result.completed ? 1 : 0,
                        result.map_changed ? 1 : 0,
                        result.failed_index,
                        result.retries_used,
                        HasHeldBundle() ? 1 : 0);

        if (result.completed && (!requireHeldBundle || HasHeldBundle())) {
            return true;
        }
        if (result.map_changed || MapMgr::GetMapId() != route.map_id) {
            return false;
        }
        if (attempt < maxAttempts) {
            LogArachnisMain("recover-route retry route=%s context=%s nextAttempt=%u currentMap=%u hasHeld=%d",
                            route.name,
                            context != nullptr ? context : "",
                            attempt + 1u,
                            MapMgr::GetMapId(),
                            HasHeldBundle() ? 1 : 0);
        }
    }

    return false;
}

bool RecoverFlameStaffAfterRevive(
    const char* routeName,
    int waypointIndex,
    uint32_t mapId,
    const char* context) {
    if (!WaitForPlayerAliveStable(mapId, 1u, 45000u, 2500u, "flame-staff recover", 0.70f)) {
        return false;
    }

    if (EnsureHeldBundleNearPlayer()) {
        return true;
    }

    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    int objectiveIndex = -1;

    if (mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1) {
        if (routeName != nullptr && std::strcmp(routeName, "Level1Phase1") == 0) {
            objectiveIndex = 0;
        } else if (routeName != nullptr && std::strcmp(routeName, "Level1Phase2") == 0) {
            // Phase 2 starts after the first web has already been burned. If
            // the effect-only staff is lost on death, the consumed first staff
            // is gone; recover by pathing to the next staff instead.
            objectiveIndex = 1;
        } else if (routeName != nullptr &&
                   (std::strcmp(routeName, "Level1Phase3") == 0 ||
                    std::strcmp(routeName, "Level1Phase4") == 0 ||
                    std::strcmp(routeName, "Level1Exit") == 0)) {
            objectiveIndex = 1;
        }
    } else if (mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL2) {
        if (routeName != nullptr && std::strcmp(routeName, "Level2Phase1Transit") == 0) {
            objectiveIndex = 2;
        } else if (routeName != nullptr &&
                   (std::strcmp(routeName, "Level2Phase2") == 0 ||
                    std::strcmp(routeName, "Level2Phase3") == 0)) {
            objectiveIndex = 3;
        }
    }

    if (objectiveIndex < 0 || objectiveIndex >= objectiveCount) {
        LogArachnisMain("flame-staff recover no-objective route=%s context=%s index=%d map=%u objectives=%d",
                        routeName != nullptr ? routeName : "",
                        context != nullptr ? context : "",
                        waypointIndex,
                        mapId,
                        objectiveCount);
        return false;
    }

    LogArachnisMain("flame-staff recover route=%s context=%s index=%d map=%u objective=%s pickup=(%.0f, %.0f)",
                    routeName != nullptr ? routeName : "",
                    context != nullptr ? context : "",
                    waypointIndex,
                    mapId,
                    objectives[objectiveIndex].name != nullptr ? objectives[objectiveIndex].name : "",
                    objectives[objectiveIndex].pickup_point.x,
                    objectives[objectiveIndex].pickup_point.y);
    if (mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1 &&
        routeName != nullptr &&
        std::strcmp(routeName, "Level1Phase1") == 0 &&
        objectiveIndex == 0) {
        const bool reachedFirstStaffArea =
            FollowRecoveryRouteToWaypoint(RouteId::Level1Phase1, 3, "level 1 phase 1 staff recovery", 2u, false);
        if (!reachedFirstStaffArea) {
            LogArachnisMain("flame-staff recover first-staff route incomplete; trying direct pickup anyway");
        }
        if (!WaitForPlayerAliveStable(mapId, 1u, 90000u, 3000u, "flame-staff first direct recovery", 0.70f)) {
            return false;
        }
    }
    if (mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1 &&
        routeName != nullptr &&
        std::strcmp(routeName, "Level1Phase2") == 0 &&
        objectiveIndex == 1) {
        const bool reachedSecondStaffArea =
            FollowRecoveryRouteToWaypoint(RouteId::Level1Phase2, 13, "level 1 phase 2 staff recovery", 2u, false);
        if (!reachedSecondStaffArea) {
            LogArachnisMain("flame-staff recover second-staff route incomplete; trying direct pickup anyway");
        }
        if (!WaitForPlayerAliveStable(mapId, 1u, 90000u, 3000u, "flame-staff direct recovery", 0.70f)) {
            return false;
        }
    }

    return PickUpObjectiveBundle(objectives[objectiveIndex], mapId);
}

bool StabilizeBundleClearLeg(
    const char* objectiveName,
    int legIndex,
    int totalLegCount,
    uint32_t mapId,
    const char* recoveryRouteName,
    int recoveryWaypointIndex) {
    for (int pass = 0; pass < 2; ++pass) {
        auto* me = AgentMgr::GetMyAgent();
        if (me == nullptr || me->hp <= 0.0f || PartyMgr::GetIsPartyDefeated()) {
            if (!RecoverHeldBundleAfterTravelRevive(
                    mapId,
                    "bundle-clear-stabilize",
                    recoveryRouteName,
                    recoveryWaypointIndex)) {
                LogArachnisMain("bundle-clear stabilize abort revive objective=%s leg=%d/%d",
                                objectiveName != nullptr ? objectiveName : "",
                                legIndex,
                                totalLegCount);
                return false;
            }
            continue;
        }

        const float enemyDistance = DungeonCombat::GetNearestLivingEnemyDistance(1600.0f);
        if (me->hp >= 0.70f && enemyDistance > 900.0f) {
            return true;
        }

        LogArachnisMain("bundle-clear stabilize objective=%s leg=%d/%d pass=%d hp=%.2f enemyDistance=%.0f held=%d effectOnly=%d",
                        objectiveName != nullptr ? objectiveName : "",
                        legIndex,
                        totalLegCount,
                        pass + 1,
                        me->hp,
                        enemyDistance,
                        HasHeldBundle() ? 1 : 0,
                        HasEffectOnlyFlameStaffBundle() ? 1 : 0);
        AgentMgr::Move(me->x, me->y);
        (void)RunStaffSafeCombatAssist(1600.0f, enemyDistance <= 300.0f ? 18000u : 12000u, "bundle-clear-stabilize");
        if (!WaitForPlayerAliveStable(mapId, 1u, 45000u, 1500u, "bundle-clear-stabilize", 0.45f)) {
            if (!RecoverHeldBundleAfterTravelRevive(
                    mapId,
                    "bundle-clear-stabilize",
                    recoveryRouteName,
                    recoveryWaypointIndex)) {
                LogArachnisMain("bundle-clear stabilize abort unstable objective=%s leg=%d/%d",
                                objectiveName != nullptr ? objectiveName : "",
                                legIndex,
                                totalLegCount);
                return false;
            }
        }
    }

    return HasHeldBundle();
}

bool ExecuteBundleClearPath(
    const FlameStaffObjective& objective,
    uint32_t mapId,
    bool dropBundle,
    const char* recoveryRouteName,
    int recoveryWaypointIndex) {
    if (!EnsureHeldBundleNearPlayer()) {
        LogArachnisMain("bundle-clear abort missing initial staff objective=%s map=%u",
                        objective.name != nullptr ? objective.name : "",
                        mapId);
        return false;
    }

    const bool hasDropPoint = objective.drop_point.x != 0.0f || objective.drop_point.y != 0.0f;
    const int totalLegCount = objective.web_clear_path_count + ((dropBundle && hasDropPoint) ? 1 : 0);
    LogHeldBundleState("bundle-clear begin");
    LogArachnisMain("bundle-clear start objective=%s map=%u pathCount=%d drop=%d hasDropPoint=%d totalLegs=%d",
                    objective.name != nullptr ? objective.name : "",
                    mapId,
                    objective.web_clear_path_count,
                    dropBundle ? 1 : 0,
                    hasDropPoint ? 1 : 0,
                    totalLegCount);

    for (int i = 0; i < objective.web_clear_path_count; ++i) {
        LogBundleCarryTarget("bundle-clear moving", i + 1, totalLegCount, objective.web_clear_path[i]);
        if (!StabilizeBundleClearLeg(
                objective.name,
                i + 1,
                totalLegCount,
                mapId,
                recoveryRouteName,
                recoveryWaypointIndex)) {
            LogArachnisMain("bundle-clear abort stabilize failed objective=%s leg=%d/%d",
                            objective.name != nullptr ? objective.name : "",
                            i + 1,
                            totalLegCount);
            return false;
        }
        if (!MoveToTravelPointWhileCarryingBundle(
                objective.web_clear_path[i],
                mapId,
                300.0f,
                120000u,
                "bundle-clear-web",
                recoveryRouteName,
                recoveryWaypointIndex)) {
            LogBundleCarryTarget("bundle-clear move failed", i + 1, totalLegCount, objective.web_clear_path[i]);
            LogArachnisMain("bundle-clear abort move failed objective=%s leg=%d/%d held=%d effectOnly=%d hp=%.2f partyDefeated=%d",
                            objective.name != nullptr ? objective.name : "",
                            i + 1,
                            totalLegCount,
                            HasHeldBundle() ? 1 : 0,
                            HasEffectOnlyFlameStaffBundle() ? 1 : 0,
                            AgentMgr::GetMyAgent() != nullptr ? AgentMgr::GetMyAgent()->hp : 0.0f,
                            PartyMgr::GetIsPartyDefeated() ? 1 : 0);
            return false;
        }
        LogBundleCarryTarget("bundle-clear arrived", i + 1, totalLegCount, objective.web_clear_path[i]);
        if (!EnsureHeldBundleNearPlayer()) {
            LogArachnisMain("bundle-clear abort staff lost after leg objective=%s leg=%d/%d",
                            objective.name != nullptr ? objective.name : "",
                            i + 1,
                            totalLegCount);
            return false;
        }
    }

    if (!dropBundle) {
        LogArachnisMain("bundle-clear complete objective=%s map=%u retainedStaff=1",
                        objective.name != nullptr ? objective.name : "",
                        mapId);
        return true;
    }

    if (hasDropPoint) {
        LogBundleCarryTarget(
            "bundle-clear moving",
            objective.web_clear_path_count + 1,
            totalLegCount,
            objective.drop_point);
        if (!StabilizeBundleClearLeg(
                objective.name,
                objective.web_clear_path_count + 1,
                totalLegCount,
                mapId,
                recoveryRouteName,
                recoveryWaypointIndex)) {
            LogArachnisMain("bundle-clear abort drop stabilize failed objective=%s",
                            objective.name != nullptr ? objective.name : "");
            return false;
        }
        if (!MoveToTravelPointWhileCarryingBundle(
                objective.drop_point,
                mapId,
                300.0f,
                120000u,
                "bundle-clear-drop",
                recoveryRouteName,
                recoveryWaypointIndex)) {
            LogBundleCarryTarget(
                "bundle-clear move failed",
                objective.web_clear_path_count + 1,
                totalLegCount,
                objective.drop_point);
            LogArachnisMain("bundle-clear abort drop move failed objective=%s held=%d effectOnly=%d hp=%.2f partyDefeated=%d",
                            objective.name != nullptr ? objective.name : "",
                            HasHeldBundle() ? 1 : 0,
                            HasEffectOnlyFlameStaffBundle() ? 1 : 0,
                            AgentMgr::GetMyAgent() != nullptr ? AgentMgr::GetMyAgent()->hp : 0.0f,
                            PartyMgr::GetIsPartyDefeated() ? 1 : 0);
            return false;
        }
        LogBundleCarryTarget(
            "bundle-clear arrived",
            objective.web_clear_path_count + 1,
            totalLegCount,
            objective.drop_point);
    }

    LogHeldBundleState("bundle-clear before drop");
    AgentMgr::CancelAction();
    WaitMs(100u);
    AgentMgr::ChangeTarget(0u);
    WaitMs(150u);
    if (!WaitForMovementToSettle(2000u, "bundle-clear drop settle")) {
        LogBot("Arachnis: movement did not settle before drop at %.0f, %.0f",
               objective.drop_point.x,
               objective.drop_point.y);
        return false;
    }
    bool dropped = TryDropFlameStaffBundle("bundle-clear");
    if (!dropped && HasEffectOnlyFlameStaffBundle()) {
        LogBot("Arachnis: skipping unsafe effect-only flame staff drop at %.0f, %.0f; continuing to carry staff",
               objective.drop_point.x,
               objective.drop_point.y);
        LogArachnisMain("bundle-clear effect-only drop skipped target=(%.0f, %.0f)",
                        objective.drop_point.x,
                        objective.drop_point.y);
        LogHeldBundleState("bundle-clear after drop-skip");
        return true;
    }
    if (!dropped) {
        dropped = DungeonInteractions::DropHeldBundle(true);
    }
    LogBot("Arachnis: bundle-clear drop call target=(%.0f, %.0f) result=%d",
           objective.drop_point.x,
           objective.drop_point.y,
           dropped ? 1 : 0);
    LogHeldBundleState("bundle-clear after drop");
    if (!dropped) {
        LogBot("Arachnis: expected to drop flame staff bundle at %.0f, %.0f",
               objective.drop_point.x,
               objective.drop_point.y);
        return false;
    }

    if (!WaitForHeldBundleState(false, 3000u, "bundle-clear drop settled")) {
        LogBot("Arachnis: flame staff bundle still held after drop at %.0f, %.0f",
               objective.drop_point.x,
               objective.drop_point.y);
        LogArachnisMain("bundle-clear abort drop unsettled objective=%s",
                        objective.name != nullptr ? objective.name : "");
        return false;
    }
    LogArachnisMain("bundle-clear complete objective=%s map=%u dropped=1",
                    objective.name != nullptr ? objective.name : "",
                    mapId);
    return true;
}

bool LightBrazierNearPoint(
    const DungeonQuest::TravelPoint& point,
    uint32_t mapId,
    const char* clusterName,
    int eggIndex,
    const char* recoveryRouteName = nullptr,
    int recoveryWaypointIndex = 0) {
    for (int pass = 0; pass < 2; ++pass) {
        const char* travelContext = clusterName != nullptr ? clusterName : "brazier";
        const bool moved = HasEffectOnlyFlameStaffBundle()
            ? MoveToTravelPointWhileCarryingBundle(
                  point,
                  mapId,
                  250.0f,
                  30000u,
                  travelContext,
                  recoveryRouteName,
                  recoveryWaypointIndex)
            : MoveToTravelPoint(point, mapId, 250.0f, 30000u);
        if (!moved) {
            LogBot("Arachnis: failed moving to brazier cluster=%s egg=%d pass=%d target=(%.0f, %.0f)",
                   clusterName != nullptr ? clusterName : "",
                   eggIndex,
                   pass + 1,
                   point.x,
                   point.y);
            return false;
        }

        const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(point.x, point.y, 1500.0f);
        auto* signpost = signpostId != 0u ? AgentMgr::GetAgentByID(signpostId) : nullptr;
        const uint32_t gadgetId = signpost && signpost->type == 0x200u
                                      ? static_cast<const AgentGadget*>(signpost)->gadget_id
                                      : 0u;
        LogBot("Arachnis: lighting brazier cluster=%s egg=%d pass=%d signpost=%u gadget=%u target=(%.0f, %.0f)",
               clusterName != nullptr ? clusterName : "",
               eggIndex,
               pass + 1,
               signpostId,
               gadgetId,
               point.x,
               point.y);
        LogArachnisMain("brazier-light cluster=%s egg=%d pass=%d signpost=%u gadget=%u target=(%.0f, %.0f)",
                        clusterName != nullptr ? clusterName : "",
                        eggIndex,
                        pass + 1,
                        signpostId,
                        gadgetId,
                        point.x,
                        point.y);

        if (!DungeonBundle::InteractSignpostNearPoint(point.x, point.y, 1500.0f, 2, 500u)) {
            LogBot("Arachnis: brazier signpost interaction failed cluster=%s egg=%d pass=%d target=(%.0f, %.0f)",
                   clusterName != nullptr ? clusterName : "",
                   eggIndex,
                   pass + 1,
                   point.x,
                   point.y);
            return false;
        }

        if (!HasHeldBundle()) {
            LogHeldBundleState("brazier-light missing staff");
            return false;
        }
        WaitMs(250u);
    }
    return true;
}

bool ExecuteSpiderEggCluster(
    const SpiderEggCluster& cluster,
    uint32_t mapId,
    const char* recoveryRouteName = nullptr,
    int recoveryWaypointIndex = 0) {
    if (!EnsureHeldBundleNearPlayer()) {
        return false;
    }

    for (int i = 0; i < cluster.egg_count; ++i) {
        if (!LightBrazierNearPoint(
                cluster.egg_points[i],
                mapId,
                cluster.name,
                i + 1,
                recoveryRouteName,
                recoveryWaypointIndex)) {
            return false;
        }
    }

    if (cluster.post_cluster_path != nullptr && cluster.post_cluster_path_count > 0) {
        if (HasEffectOnlyFlameStaffBundle() && recoveryRouteName != nullptr) {
            for (int i = 0; i < cluster.post_cluster_path_count; ++i) {
                if (!MoveToTravelPointWhileCarryingBundle(
                        cluster.post_cluster_path[i],
                        mapId,
                        250.0f,
                        30000u,
                        cluster.name != nullptr ? cluster.name : "egg-post-path",
                        recoveryRouteName,
                        recoveryWaypointIndex)) {
                    return false;
                }
            }
            return true;
        }

        return FollowTravelPath(
            cluster.post_cluster_path,
            cluster.post_cluster_path_count,
            mapId,
            250.0f);
    }

    return true;
}

bool ExecuteLevel1KeyObjective() {
    const auto objective = GetLevel1KeyObjective();
    if (!FollowTravelPathWithAggro(
            objective.approach_path,
            objective.approach_path_count,
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            250.0f,
            1200.0f,
            60000u)) {
        return false;
    }

    for (int attempt = 0; attempt < objective.pickup_attempts; ++attempt) {
        if (!MoveToTravelPointWithAggro(
                objective.pickup_point,
                GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
                300.0f,
                1200.0f,
                15000u)) {
            return false;
        }
        (void)DungeonBundle::PickUpNearestItemNearPoint(
            objective.pickup_point.x,
            objective.pickup_point.y,
            18000.0f,
            1,
            500u);
    }

    if (!EnsureHeldBundleNearPlayer()) {
        return false;
    }

    for (int i = 0; i < objective.web_clear_path_count; ++i) {
        if (!MoveToTravelPointWithAggro(
                objective.web_clear_path[i],
                GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
                250.0f,
                1200.0f,
                60000u)) {
            return false;
        }
    }

    if (!MoveToTravelPointWithAggro(
            objective.drop_point,
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            250.0f,
            1200.0f,
            60000u)) {
        return false;
    }
    if (HasEffectOnlyFlameStaffBundle()) {
        LogBot("Arachnis: skipping unsafe effect-only flame staff drop before level 1 door");
        LogArachnisMain("level1-key-objective effect-only drop skipped");
    } else if (!TryDropFlameStaffBundle("level1-key-objective")
        && !DungeonInteractions::DropHeldBundle(true)) {
        LogBot("Arachnis: expected held bundle before level 1 door drop");
        return false;
    }
    WaitMs(500u);
    return true;
}

void ClearTargetAndStopForDoor() {
    AgentMgr::CancelAction();
    WaitMs(100u);
    AgentMgr::ChangeTarget(0u);
    WaitMs(150u);
}

bool SendActionInteractBurst(int interactCount, uint32_t delayMs) {
    if (interactCount <= 0) {
        return false;
    }

    for (int i = 0; i < interactCount; ++i) {
        if (!AgentMgr::ActionInteract()) {
            return false;
        }
        if (delayMs != 0u && i + 1 < interactCount) {
            WaitMs(delayMs);
        }
    }
    return true;
}

bool ExecuteStaffSafeDoorSignpostInteractSequence(const DoorOpenObjective& door) {
    if (!MoveToTravelPoint(door.interact_point, GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 200.0f, 15000u)) {
        return false;
    }

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(
        door.interact_point.x,
        door.interact_point.y,
        2500.0f);
    auto* signpost = signpostId != 0u ? AgentMgr::GetAgentByID(signpostId) : nullptr;
    const uint32_t gadgetId = signpost && signpost->type == 0x200u
                                  ? static_cast<const AgentGadget*>(signpost)->gadget_id
                                  : 0u;
    LogArachnisMain("level1-exit-door staff-safe signpost=%u gadget=%u point=(%.0f, %.0f) heldItem=%u effectOnly=%d",
                    signpostId,
                    gadgetId,
                    door.interact_point.x,
                    door.interact_point.y,
                    DungeonInteractions::GetHeldBundleItemId(),
                    HasEffectOnlyFlameStaffBundle() ? 1 : 0);
    if (signpostId == 0u) {
        return false;
    }

    for (int burst = 0; burst < 3; ++burst) {
        for (int press = 0; press < door.interact_repeats; ++press) {
            AgentMgr::InteractSignpost(signpostId);
            WaitMs(150u);
        }
        WaitMs(500u);
    }

    return true;
}

bool ExecuteLevel1ExitDoorInteractSequence(const DoorOpenObjective& door) {
    if (HasEffectOnlyFlameStaffBundle()) {
        return ExecuteStaffSafeDoorSignpostInteractSequence(door);
    }

    if (!MoveToTravelPoint(door.interact_point, GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 200.0f, 15000u)) {
        return false;
    }
    WaitMs(1000u);

    ClearTargetAndStopForDoor();
    if (!SendActionInteractBurst(2, 100u)) {
        return false;
    }

    WaitMs(500u);
    if (!MoveToTravelPoint(door.interact_point, GWA3::MapIds::ARACHNIS_HAUNT_LVL1, 200.0f, 15000u)) {
        return false;
    }

    WaitMs(1000u);
    ClearTargetAndStopForDoor();
    if (!SendActionInteractBurst(2, 100u)) {
        return false;
    }

    WaitMs(1000u);
    if (!SendActionInteractBurst(2, 100u)) {
        return false;
    }

    return true;
}

bool ExecuteLevel1ExitDoor() {
    if (!ExecuteRoute(RouteId::Level1Exit, "level 1 exit")) {
        return false;
    }

    const auto door = GetLevel1ExitDoorObjective();
    if (!MoveToTravelPointWithAggro(
            door.interact_point,
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            200.0f,
            1200.0f,
            15000u)) {
        return false;
    }

    if (!ExecuteLevel1ExitDoorInteractSequence(door)) {
        LogBot("Arachnis: level 1 exit door interaction failed");
        return false;
    }

    const ZoneTransitionAttemptResult transition = ZoneThroughPointWithTransitionHooks(
        door.zone_point.x,
        door.zone_point.y,
        GWA3::MapIds::ARACHNIS_HAUNT_LVL2,
        "level 1 exit to level 2");
    if (!transition.zoned) {
        LogBot("Arachnis: failed zoning into level 2");
        return false;
    }
    return transition.ready;
}

bool ExecuteRewardChestFlow() {
    const auto reward = GetRewardChestObjective();
    LogArachnisMain("reward flow begin chest=(%.0f, %.0f)",
                    reward.chest_point.x,
                    reward.chest_point.y);
    if (!MoveToTravelPointWithAggro(reward.chest_point, GWA3::MapIds::ARACHNIS_HAUNT_LVL2, 250.0f, 1200.0f, 60000u)) {
        LogArachnisMain("reward flow chest move failed");
        return false;
    }

    (void)RunStaffSafeCombatAssist(1800.0f, 20000u, "reward-chest-pre-open");

    for (int pass = 0; pass < reward.chest_interact_passes; ++pass) {
        LogArachnisMain("reward flow chest interact pass=%d/%d",
                        pass + 1,
                        reward.chest_interact_passes);
        if (!DungeonBundle::InteractSignpostNearPoint(
                reward.chest_point.x,
                reward.chest_point.y,
                1500.0f,
                reward.chest_interact_count,
                reward.chest_interact_delay_ms)) {
            LogArachnisMain("reward flow chest interact failed pass=%d", pass + 1);
            return false;
        }
        (void)DungeonBundle::PickUpNearestItemNearPoint(
            reward.chest_point.x,
            reward.chest_point.y,
            18000.0f,
            reward.chest_loot_attempts,
            reward.chest_loot_delay_ms);
    }

    LogArachnisMain("reward flow completion dialog begin npc=(%.0f, %.0f) radius=%.0f",
                    reward.completion_npc.x,
                    reward.completion_npc.y,
                    reward.completion_npc.search_radius);
    bool completionSent = DungeonQuestRuntime::InteractNearestNpcAndSendDialogPlan(
        reward.completion_npc,
        reward.reward_dialog);
    if (!completionSent) {
        LogArachnisMain("reward flow completion npc interact failed; trying direct dialog plan");
        completionSent = DungeonQuestRuntime::SendDialogPlan(reward.reward_dialog);
    }
    if (!completionSent) {
        LogBot("Arachnis: completion dialog hand-in failed after chest");
        LogArachnisMain("reward flow completion dialog failed");
        return false;
    }

    LogArachnisMain("reward flow completion dialog sent; waiting for Magus Stones");
    const bool returned = DungeonNavigation::WaitForMapId(GWA3::MapIds::MAGUS_STONES, 180000u);
    LogArachnisMain("reward flow wait result returned=%d currentMap=%u",
                    returned ? 1 : 0,
                    MapMgr::GetMapId());
    return returned;
}

bool MoveToQuestNpc(const DungeonQuest::QuestCyclePlan& plan, const char* context) {
    const DungeonQuest::TravelPoint npcPoint = {plan.npc.x, plan.npc.y};
    const float npcDistance = DungeonCombat::DistanceToPoint(npcPoint.x, npcPoint.y);
    constexpr float kQuestNpcReadyDistance = 250.0f;
    if (MapMgr::GetMapId() == plan.start_map_id && npcDistance <= kQuestNpcReadyDistance) {
        LogArachnisMain("quest npc approach already-near context=%s distance=%.0f searchRadius=%.0f",
                        context != nullptr ? context : "",
                        npcDistance,
                        plan.npc.search_radius);
        return true;
    }
    if (plan.start_map_id == GWA3::MapIds::MAGUS_STONES) {
        char approachContext[128];
        snprintf(approachContext,
                 sizeof(approachContext),
                 "quest npc %s",
                 context != nullptr ? context : "");
        if (MoveToTravelPointWithOpportunisticAggro(
                npcPoint,
                plan.start_map_id,
                250.0f,
                900.0f,
                45000u,
                approachContext)) {
            return true;
        }
    }
    if (!MoveToTravelPointWithAggro(
            npcPoint,
            plan.start_map_id,
            300.0f,
            plan.npc.search_radius,
            120000u)) {
        LogBot("Arachnis: failed moving to quest NPC for %s", context);
        return false;
    }
    return true;
}

bool ExecuteQuestDialogs(
    const DungeonQuest::QuestCyclePlan& plan,
    const DungeonQuest::DialogPlan& dialogPlan,
    const char* context) {
    const DungeonQuestRuntime::DialogExecutionOptions dialogOptions = BuildQuestDialogOptions();
    if (!MoveToQuestNpc(plan, context)) {
        return false;
    }
    if (!DungeonQuestRuntime::InteractNearestNpcAndSendDialogPlan(plan.npc, dialogPlan, dialogOptions)) {
        auto directDialogOptions = dialogOptions;
        directDialogOptions.use_direct_npc_interact = true;
        directDialogOptions.interact_count = 2;
        directDialogOptions.interact_delay_ms = 1000u;
        directDialogOptions.post_interact_delay_ms = 1500u;
        directDialogOptions.dialog_wait_timeout_ms = 3500u;
        directDialogOptions.pre_interact_settle_ms = 750u;
        LogArachnisMain("quest dialog native interact failed context=%s; retrying direct NPC packet",
                        context != nullptr ? context : "");
        if (DungeonQuestRuntime::InteractNearestNpcAndSendDialogPlan(plan.npc, dialogPlan, directDialogOptions)) {
            AgentMgr::CancelAction();
            WaitMs(100u);
            AgentMgr::ChangeTarget(0u);
            WaitMs(150u);
            DialogMgr::ClearDialog();
            DialogMgr::ResetHookState();
            DialogMgr::ResetRecentUITrace();
            if (!WaitForMovementToSettle(2000u, context)) {
                LogBot("Arachnis: player state did not settle after %s direct dialogs", context);
                return false;
            }
            return true;
        }
        LogBot("Arachnis: dialog plan failed during %s", context);
        return false;
    }
    AgentMgr::CancelAction();
    WaitMs(100u);
    AgentMgr::ChangeTarget(0u);
    WaitMs(150u);
    DialogMgr::ClearDialog();
    DialogMgr::ResetHookState();
    DialogMgr::ResetRecentUITrace();
    if (!WaitForMovementToSettle(2000u, context)) {
        LogBot("Arachnis: player state did not settle after %s dialogs", context);
        return false;
    }
    return true;
}

bool ExecuteQuestApproach(const DungeonQuest::QuestCyclePlan& plan) {
    if (FollowTravelPathWithAggro(
            plan.approach_path,
            plan.approach_path_count,
            plan.start_map_id,
            300.0f,
            1200.0f,
            60000u)) {
        return true;
    }

    LogBot("Arachnis: quest approach aggro path failed, retrying simple travel");
    LogArachnisMain("quest approach fallback simple-travel map=%u pointCount=%d",
                    plan.start_map_id,
                    plan.approach_path_count);
    return FollowTravelPath(
        plan.approach_path,
        plan.approach_path_count,
        plan.start_map_id,
        300.0f);
}

bool ExecuteQuestReturn() {
    int count = 0;
    const auto* points = GetQuestReturnPath(count);
    if (FollowTravelPathWithAggro(
            points,
            count,
            GWA3::MapIds::MAGUS_STONES,
            300.0f,
            1200.0f,
            60000u)) {
        return true;
    }

    LogBot("Arachnis: quest return aggro path failed, retrying simple travel");
    return FollowTravelPath(
        points,
        count,
        GWA3::MapIds::MAGUS_STONES,
        300.0f);
}

bool HasArachnisQuestForEntry(const char* context) {
    QuestMgr::RequestQuestInfo(kArachnisQuestId);
    WaitMs(250u);

    const auto* quest = QuestMgr::GetQuestById(kArachnisQuestId);
    const uint32_t activeQuest = QuestMgr::GetActiveQuestId();
    const bool ready = quest != nullptr || activeQuest == kArachnisQuestId;
    LogArachnisMain("quest entry-ready check context=%s ready=%d activeQuest=0x%X questPresent=%d questLogSize=%u",
                    context != nullptr ? context : "",
                    ready ? 1 : 0,
                    activeQuest,
                    quest != nullptr ? 1 : 0,
                    QuestMgr::GetQuestLogSize());
    if (quest != nullptr && activeQuest != kArachnisQuestId) {
        QuestMgr::SetActiveQuest(kArachnisQuestId);
        WaitMs(250u);
    }
    return ready;
}

bool ExecuteQuestCycle() {
    const auto plan = GetQuestCyclePlan();
    if (!DungeonQuest::IsValidQuestCyclePlan(plan)) {
        return false;
    }

    LogBot("Arachnis: starting reward/quest bootstrap");
    LogArachnisMain("quest cycle start bootstrap");
    if (!ExecuteQuestDialogs(plan, plan.reward_dialog, "reward bootstrap")) {
        LogArachnisMain("quest cycle bootstrap dialogs failed");
        return false;
    }

    if (!ExecuteQuestApproach(plan)) {
        LogBot("Arachnis: failed on quest approach before reward bounce");
        LogArachnisMain("quest cycle bootstrap approach failed");
        return false;
    }
    LogArachnisMain("quest cycle bootstrap zoning into level1");
    if (!ZoneAtTravelPointWithRetries(
            plan.dungeon_entry,
            plan.start_map_id,
            plan.dungeon_map_id,
            "reward bootstrap entry")) {
        LogBot("Arachnis: failed zoning into reward bootstrap dungeon instance");
        LogArachnisMain("quest cycle bootstrap entry failed");
        return false;
    }
    const ZoneTransitionAttemptResult reverseTransition = ZoneThroughPointWithTransitionHooks(
        plan.dungeon_exit.x,
        plan.dungeon_exit.y,
        plan.start_map_id,
        "reward bootstrap reverse");
    if (!reverseTransition.zoned) {
        LogBot("Arachnis: failed reversing out of reward bootstrap dungeon instance");
        LogArachnisMain("quest cycle bootstrap reverse failed");
        return false;
    }
    if (!reverseTransition.ready) {
        LogBot("Arachnis: Magus Stones did not finish loading after reward bootstrap reversal");
        LogArachnisMain("quest cycle bootstrap reverse ready failed");
        return false;
    }
    const bool heroesRecovered = WaitForPartyRecovery(plan.start_map_id, 1u, 8000u);
    LogBot("Arachnis: reward bootstrap returned to Magus; heroes=%u recovered=%d",
           PartyMgr::CountPartyHeroes(),
           heroesRecovered ? 1 : 0);
    LogArachnisMain("quest cycle bootstrap returned heroes=%u recovered=%d",
                    PartyMgr::CountPartyHeroes(),
                    heroesRecovered ? 1 : 0);

    if (!ExecuteQuestReturn()) {
        LogBot("Arachnis: failed on quest return after reward bootstrap");
        LogArachnisMain("quest cycle return path failed");
        return false;
    }

    LogBot("Arachnis: starting live quest accept flow");
    LogArachnisMain("quest cycle live accept start");
    if (HasArachnisQuestForEntry("before live accept")) {
        LogArachnisMain("quest cycle live accept skipped already-ready");
    } else if (!ExecuteQuestDialogs(plan, plan.accept_dialog, "quest accept")) {
        if (!HasArachnisQuestForEntry("after live accept failure")) {
            LogArachnisMain("quest cycle live accept dialogs failed");
            return false;
        }
        LogArachnisMain("quest cycle live accept recovered quest-ready-after-failure");
    }
    if (!ExecuteQuestApproach(plan)) {
        LogBot("Arachnis: failed on quest approach before live entry");
        LogArachnisMain("quest cycle live approach failed");
        return false;
    }
    LogArachnisMain("quest cycle live entry zoning into level1");
    if (!ZoneAtTravelPointWithRetries(
            plan.dungeon_entry,
            plan.start_map_id,
            plan.dungeon_map_id,
            "level 1 entry")) {
        LogBot("Arachnis: failed zoning into level 1");
        LogArachnisMain("quest cycle live entry failed");
        return false;
    }
    s_runStartTime = GetTickCount();
    LogBot("Arachnis: Run #%u started", s_runCount + 1u);
    LogArachnisMain("quest cycle live entry succeeded");
    return true;
}

bool ExecuteLevel1Phase2();
bool ExecuteLevel1Phase3();
bool ExecuteLevel1Phase4();
bool ExecuteLevel1Exit();
bool ExecuteLevel2Phase2();
bool ExecuteLevel2Phase3();

bool ExecuteLevel1Phase1() {
    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    if (objectiveCount < 2) {
        return false;
    }

    LogArachnisMain("level1-phase1 step route begin");
    if (!ExecuteRoute(RouteId::Level1Phase1, "level 1 phase 1")) {
        LogArachnisMain("level1-phase1 step route failed");
        return false;
    }
    LogArachnisMain("level1-phase1 step pickup begin");
    if (!PickUpObjectiveBundle(objectives[0], GWA3::MapIds::ARACHNIS_HAUNT_LVL1)) {
        LogArachnisMain("level1-phase1 step pickup failed");
        return false;
    }
    LogArachnisMain("level1-phase1 step clear begin");
    if (!ExecuteBundleClearPath(
            objectives[0],
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            true,
            "Level1Phase1",
            3)) {
        LogArachnisMain("level1-phase1 step clear failed");
        return false;
    }
    LogArachnisMain("level1-phase1 step next begin");
    return ExecuteLevel1Phase2();
}

bool ExecuteLevel1Phase2() {
    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    if (objectiveCount < 2) {
        return false;
    }

    LogArachnisMain("level1-phase2 step route begin");
    if (!ExecuteRoute(RouteId::Level1Phase2, "level 1 phase 2")) {
        LogArachnisMain("level1-phase2 step route failed");
        return false;
    }
    LogArachnisMain("level1-phase2 step pickup begin");
    if (!PickUpObjectiveBundle(objectives[1], GWA3::MapIds::ARACHNIS_HAUNT_LVL1)) {
        LogArachnisMain("level1-phase2 step pickup failed");
        return false;
    }
    LogArachnisMain("level1-phase2 step clear begin");
    if (!ExecuteBundleClearPath(
            objectives[1],
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            false,
            "Level1Phase2",
            13)) {
        LogArachnisMain("level1-phase2 step clear failed");
        return false;
    }
    LogArachnisMain("level1-phase2 step next begin");
    return ExecuteLevel1Phase3();
}

bool ExecuteLevel1Phase3() {
    int eggCount = 0;
    const auto* eggs = GetSpiderEggClusters(eggCount);
    if (eggCount < 1) {
        return false;
    }

    LogArachnisMain("level1-phase3 step route begin");
    if (!ExecuteRoute(RouteId::Level1Phase3, "level 1 phase 3")) {
        LogArachnisMain("level1-phase3 step route failed");
        return false;
    }
    LogArachnisMain("level1-phase3 step ensure-staff begin");
    if (!EnsureHeldBundleNearPlayer()) {
        LogArachnisMain("level1-phase3 step ensure-staff failed");
        return false;
    }
    LogArachnisMain("level1-phase3 step eggs begin");
    if (!ExecuteSpiderEggCluster(
            eggs[0],
            GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
            "Level1Phase3")) {
        LogArachnisMain("level1-phase3 step eggs failed");
        return false;
    }
    LogArachnisMain("level1-phase3 step next begin");
    return ExecuteLevel1Phase4();
}

bool ExecuteLevel1Phase4() {
    int eggCount = 0;
    const auto* eggs = GetSpiderEggClusters(eggCount);
    if (eggCount < 2) {
        return false;
    }

    return ExecuteRoute(RouteId::Level1Phase4, "level 1 phase 4") &&
           EnsureHeldBundleNearPlayer() &&
           ExecuteSpiderEggCluster(
               eggs[1],
               GWA3::MapIds::ARACHNIS_HAUNT_LVL1,
               "Level1Phase4") &&
           ExecuteLevel1KeyObjective() &&
           ExecuteLevel1Exit();
}

bool ExecuteLevel1Exit() {
    return ExecuteLevel1ExitDoor();
}

bool ExecuteLevel2Phase1() {
    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    if (objectiveCount < 4) {
        return false;
    }

    return ExecuteRoute(RouteId::Level2Phase1Approach, "level 2 phase 1 pickup") &&
           PickUpObjectiveBundle(objectives[2], GWA3::MapIds::ARACHNIS_HAUNT_LVL2) &&
           ExecuteRoute(RouteId::Level2Phase1Transit, "level 2 phase 1 transit") &&
           ExecuteBundleClearPath(
               objectives[2],
               GWA3::MapIds::ARACHNIS_HAUNT_LVL2,
               true,
               "Level2Phase1Transit",
               3) &&
           ExecuteLevel2Phase2();
}

bool ExecuteLevel2Phase2() {
    int objectiveCount = 0;
    const auto* objectives = GetFlameStaffObjectives(objectiveCount);
    if (objectiveCount < 4) {
        return false;
    }

    return ExecuteRoute(RouteId::Level2Phase2, "level 2 phase 2") &&
           PickUpObjectiveBundle(objectives[3], GWA3::MapIds::ARACHNIS_HAUNT_LVL2) &&
           ExecuteBundleClearPath(
               objectives[3],
               GWA3::MapIds::ARACHNIS_HAUNT_LVL2,
               false,
               "Level2Phase2",
               4) &&
           ExecuteLevel2Phase3();
}

bool ExecuteLevel2Phase3() {
    int eggCount = 0;
    const auto* eggs = GetSpiderEggClusters(eggCount);
    if (eggCount < 6) {
        return false;
    }

    if (!ExecuteRoute(RouteId::Level2Phase3, "level 2 phase 3")) {
        return false;
    }
    if (!EnsureHeldBundleNearPlayer()) {
        return false;
    }
    if (!ExecuteSpiderEggCluster(
            eggs[2],
            GWA3::MapIds::ARACHNIS_HAUNT_LVL2,
            "Level2Phase3") ||
        !ExecuteSpiderEggCluster(
            eggs[3],
            GWA3::MapIds::ARACHNIS_HAUNT_LVL2,
            "Level2Phase3") ||
        !ExecuteSpiderEggCluster(
            eggs[4],
            GWA3::MapIds::ARACHNIS_HAUNT_LVL2,
            "Level2Phase3") ||
        !ExecuteSpiderEggCluster(
            eggs[5],
            GWA3::MapIds::ARACHNIS_HAUNT_LVL2,
            "Level2Phase3")) {
        return false;
    }

    LogArachnisMain("level2-phase3 final eggs lit; executing post-egg boss clear");
    if (!ExecuteRoute(RouteId::Level2Phase3, "level 2 post-egg boss clear")) {
        LogArachnisMain("level2-phase3 post-egg boss clear failed");
        return false;
    }

    return ExecuteRewardChestFlow();
}

bool ExecuteLevel1FromCurrentPosition() {
    const auto& dispatch = GetStageDefinition(StageId::Level1);
    const int nearestIndex = GetNearestWaypointIndexForRoute(dispatch);
    if (nearestIndex < 0) {
        return false;
    }

    LogArachnisMain("level1 dispatch nearestIndex=%d", nearestIndex);

    if (nearestIndex < 2) {
        LogArachnisMain("level1 dispatch selecting phase1");
        return ExecuteLevel1Phase1();
    }
    if (nearestIndex < 10) {
        LogArachnisMain("level1 dispatch selecting phase2");
        return ExecuteLevel1Phase2();
    }
    if (nearestIndex < 12) {
        LogArachnisMain("level1 dispatch selecting phase3");
        return ExecuteLevel1Phase3();
    }
    if (nearestIndex < 14) {
        LogArachnisMain("level1 dispatch selecting phase4");
        return ExecuteLevel1Phase4();
    }
    LogArachnisMain("level1 dispatch selecting exit");
    return ExecuteLevel1Exit();
}

bool ExecuteLevel2FromCurrentPosition() {
    const auto& dispatch = GetStageDefinition(StageId::Level2);
    const int nearestIndex = GetNearestWaypointIndexForRoute(dispatch);
    if (nearestIndex < 0) {
        return false;
    }

    LogArachnisMain("level2 dispatch nearestIndex=%d", nearestIndex);

    if (nearestIndex < 2) {
        LogArachnisMain("level2 dispatch selecting phase1");
        return ExecuteLevel2Phase1();
    }
    if (nearestIndex < 14) {
        LogArachnisMain("level2 dispatch selecting phase2");
        return ExecuteLevel2Phase2();
    }
    LogArachnisMain("level2 dispatch selecting phase3");
    return ExecuteLevel2Phase3();
}

BotState HandleCharSelect(BotConfig&) {
    return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

BotState HandleTownSetup(BotConfig& cfg) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::RATA_SUM) {
        if (!WaitForMapReady(GWA3::MapIds::RATA_SUM, 10000u, "town-rata")) {
            return BotState::Error;
        }
        if (NeedsArachnisMaintenance()) {
            return RunArachnisTownMaintenanceIfNeeded() ? BotState::InTown : BotState::Stopping;
        }
        if (!EnsureArachnisOutpostSetup(cfg)) {
            return BotState::Error;
        }
        UseArachnisConsetsIfEnabled(cfg, "rata-setup");
        return BotState::Traveling;
    }
    if (mapId == GWA3::MapIds::MAGUS_STONES) {
        if (!WaitForMapReady(GWA3::MapIds::MAGUS_STONES, 10000u, "town-magus")) {
            return BotState::Error;
        }
        if (NeedsArachnisMaintenance()) {
            LogBot("Arachnis: maintenance needed after Magus return; leaving loop for upkeep");
            return RunArachnisTownMaintenanceIfNeeded() ? BotState::InTown : BotState::Stopping;
        }
        if (PartyMgr::CountPartyHeroes() < 7u) {
            LogBot("Arachnis: hero party incomplete in Magus (heroes=%u); returning to Rata Sum setup",
                   PartyMgr::CountPartyHeroes());
            MapMgr::Travel(GWA3::MapIds::RATA_SUM);
            if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::RATA_SUM, 60000u) ||
                !WaitForMapReady(GWA3::MapIds::RATA_SUM, 15000u, "magus-to-rata-setup")) {
                return BotState::Error;
            }
            s_outpostSetupApplied = false;
            return BotState::InTown;
        }
        if (!WaitForArachnisLiveHeroSetup(cfg, "magus-town", 20000u)) {
            LogBot("Arachnis: live hero setup invalid in Magus; returning to Rata Sum setup");
            MapMgr::Travel(GWA3::MapIds::RATA_SUM);
            if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::RATA_SUM, 60000u) ||
                !WaitForMapReady(GWA3::MapIds::RATA_SUM, 15000u, "magus-profession-reset")) {
                return BotState::Error;
            }
            s_outpostSetupApplied = false;
            return BotState::InTown;
        }
        return BotState::Traveling;
    }
    if (mapId == GWA3::MapIds::GADDS_ENCAMPMENT) {
        if (NeedsArachnisMaintenance()) {
            if (!RunArachnisTownMaintenanceIfNeeded()) {
                return BotState::Stopping;
            }
            return BotState::InTown;
        }
        LogBot("Arachnis: leaving Gadd's Encampment for Rata Sum setup");
        MapMgr::Travel(GWA3::MapIds::RATA_SUM);
        if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::RATA_SUM, 60000u) ||
            !WaitForMapReady(GWA3::MapIds::RATA_SUM, 15000u, "gadds-to-rata")) {
            return BotState::Error;
        }
        return BotState::InTown;
    }
    if (mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1 || mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL2) {
        return BotState::InDungeon;
    }

    LogBot("Arachnis: traveling to Rata Sum from map %u", mapId);
    MapMgr::Travel(GWA3::MapIds::RATA_SUM);
    if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::RATA_SUM, 60000u)) {
        return BotState::Error;
    }
    if (!WaitForMapReady(GWA3::MapIds::RATA_SUM, 10000u, "town-fallback-rata")) {
        return BotState::Error;
    }
    return BotState::InTown;
}

BotState HandleTravel(BotConfig& cfg) {
    switch (MapMgr::GetMapId()) {
    case GWA3::MapIds::RATA_SUM:
        if (!WaitForMapReady(GWA3::MapIds::RATA_SUM, 10000u, "travel-rata")) {
            return BotState::Error;
        }
        if (!EnsureArachnisOutpostSetup(cfg)) {
            return BotState::Error;
        }
        UseArachnisConsetsIfEnabled(cfg, "travel-rata");
        return ExecuteRoute(RouteId::RunRataSumToMagusStones, "Rata Sum to Magus Stones", true)
                   ? BotState::Traveling
                   : BotState::Error;
    case GWA3::MapIds::MAGUS_STONES:
        if (!WaitForMapReady(GWA3::MapIds::MAGUS_STONES, 10000u, "travel-magus")) {
            return BotState::Error;
        }
        if (NeedsArachnisMaintenance()) {
            LogBot("Arachnis: maintenance became due before entry; leaving Magus loop");
            return BotState::InTown;
        }
        if (!WaitForArachnisLiveHeroSetup(cfg, "magus-pre-entry", 20000u)) {
            LogBot("Arachnis: live hero setup invalid before dungeon entry; returning to Rata Sum setup");
            MapMgr::Travel(GWA3::MapIds::RATA_SUM);
            if (!DungeonNavigation::WaitForMapId(GWA3::MapIds::RATA_SUM, 60000u) ||
                !WaitForMapReady(GWA3::MapIds::RATA_SUM, 15000u, "magus-profession-reset")) {
                return BotState::Error;
            }
            s_outpostSetupApplied = false;
            return BotState::InTown;
        }
        UseArachnisConsetsIfEnabled(cfg, "magus-pre-entry");
        if (!ExecuteRoute(RouteId::RunMagusToDungeon, "Magus Stones dungeon approach")) {
            return BotState::Error;
        }
        return ExecuteQuestCycle() ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::ARACHNIS_HAUNT_LVL1:
    case GWA3::MapIds::ARACHNIS_HAUNT_LVL2:
        return BotState::InDungeon;
    default:
        LogBot("Arachnis: unsupported travel map %u", MapMgr::GetMapId());
        return BotState::Error;
    }
}

BotState HandleDungeon(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL1 || mapId == GWA3::MapIds::ARACHNIS_HAUNT_LVL2) {
        if (!WaitForMapReady(mapId, 10000u)) {
            LogBot("Arachnis: dungeon map %u not ready for route execution", mapId);
            return BotState::Error;
        }
    }

    switch (mapId) {
    case GWA3::MapIds::ARACHNIS_HAUNT_LVL1:
        if (s_runStartTime == 0u) {
            s_runStartTime = GetTickCount();
            LogBot("Arachnis: Run #%u resumed/started from level 1", s_runCount + 1u);
        }
        return ExecuteLevel1FromCurrentPosition() ? BotState::InDungeon : BotState::Error;
    case GWA3::MapIds::ARACHNIS_HAUNT_LVL2: {
        if (s_runStartTime == 0u) {
            s_runStartTime = GetTickCount();
            LogBot("Arachnis: Run #%u resumed/started from level 2", s_runCount + 1u);
        }
        const bool completed = ExecuteLevel2FromCurrentPosition();
        if (!completed) {
            ++s_failCount;
            return BotState::Error;
        }
        ++s_runCount;
        const DWORD now = GetTickCount();
        const DWORD runTime = s_runStartTime != 0u ? now - s_runStartTime : 0u;
        if (runTime != 0u && runTime < s_bestRunTime) {
            s_bestRunTime = runTime;
        }
        s_runStartTime = 0u;
        LogBot("Arachnis: Run #%u complete in %u ms (best: %u ms failures=%u finalMap=%u)",
               s_runCount,
               runTime,
               s_bestRunTime == 0xFFFFFFFFu ? 0u : s_bestRunTime,
               s_failCount,
               MapMgr::GetMapId());
        return BotState::InTown;
    }
    default:
        return BotState::InTown;
    }
}

BotState HandleError(BotConfig&) {
    LogBot("Arachnis: ERROR state - waiting before retry");
    WaitMs(5000u);
    return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

} // namespace

void Register() {
    Bot::RegisterStateHandler(BotState::CharSelect, HandleCharSelect);
    Bot::RegisterStateHandler(BotState::InTown, HandleTownSetup);
    Bot::RegisterStateHandler(BotState::Traveling, HandleTravel);
    Bot::RegisterStateHandler(BotState::InDungeon, HandleDungeon);
    Bot::RegisterStateHandler(BotState::Error, HandleError);

    auto& cfg = Bot::GetConfig();
    cfg.hero_config_file.clear();
    for (uint32_t& hero_id : cfg.hero_ids) {
        hero_id = 0u;
    }
    cfg.hard_mode = true;
    cfg.target_map_id = GWA3::MapIds::ARACHNIS_HAUNT_LVL1;
    cfg.outpost_map_id = GWA3::MapIds::RATA_SUM;
    cfg.bot_module_name = "ArachnisHaunt";

    s_runCount = 0u;
    s_failCount = 0u;
    s_runStartTime = 0u;
    s_bestRunTime = 0xFFFFFFFFu;
    s_outpostSetupApplied = false;

    LogBot("Arachnis Haunt module registered (phase runtime enabled)");
}

} // namespace GWA3::Bot::ArachnisHauntBot
