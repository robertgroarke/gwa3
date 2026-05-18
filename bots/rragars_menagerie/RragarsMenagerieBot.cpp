#include <bots/rragars_menagerie/RragarsMenagerieBot.h>

#include <bots/common/BotFramework.h>
#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonCheckpoint.h>
#include <gwa3/dungeon/DungeonCombat.h>
#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonItemActions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/dungeon/DungeonOutpostSetup.h>
#include <gwa3/dungeon/DungeonQuestRuntime.h>
#include <gwa3/dungeon/DungeonVendor.h>
#include <bots/rragars_menagerie/RragarsMenagerie.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Memory.h>
#include <gwa3/game/DialogIds.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/game/QuestIds.h>
#include <gwa3/game/SkillIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>

#include <cmath>

namespace GWA3::Bot::RragarsMenagerieBot {

using namespace GWA3::Bot;
using namespace GWA3::Bot::RragarsMenagerie;

namespace {

constexpr uint32_t LEGACY_AGGRO_MOVE_TIMEOUT_MS = 240000u;
constexpr uint32_t kRragarsTargetStaleTimeoutMs = 45000u;
constexpr float kAggroNearArrivalAfterClearFailureTolerance = 400.0f;
constexpr float kRewardNearArrivalAfterClearFailureTolerance = 1200.0f;
constexpr float kQuestNpcX = -19166.0f;
constexpr float kQuestNpcY = 17980.0f;
constexpr uint32_t kActionInteractCode = 0x80u;
constexpr uint32_t kQuestLogStateCompleted = 0x02u;
constexpr float kGaddsMerchantX = -8374.0f;
constexpr float kGaddsMerchantY = -22491.0f;
constexpr float kGaddsXunlaiX = -10481.0f;
constexpr float kGaddsXunlaiY = -22787.0f;
constexpr float kGaddsMaterialTraderX = -9097.0f;
constexpr float kGaddsMaterialTraderY = -23353.0f;
constexpr uint16_t kGaddsMerchantPlayerNumber = 6060u;
constexpr uint16_t kGaddsMaterialTraderPlayerNumber = 6763u;
constexpr uint16_t kDoomloreMerchantPlayerNumber = 6589u;
constexpr float kEmbarkXunlaiX = 2283.0f;
constexpr float kEmbarkXunlaiY = -2134.0f;
constexpr uint32_t kMaintenanceTravelRegion = 4u;
constexpr uint32_t kMaintenanceTravelDistrict = 99u;
constexpr uint32_t kMaintenanceTravelLanguage = 8u;
constexpr uint32_t kRragarsDpRemovalWipeThreshold = 3u;

uint32_t s_wipeCount = 0u;

void WaitMs(DWORD ms) {
    Sleep(ms);
}

void WaitCombatMs(uint32_t ms) {
    WaitMs(ms);
}

void WaitMaintenanceMs(uint32_t ms) {
    WaitMs(ms);
}

bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs);

void UseDpRemovalIfNeeded() {
    if (s_wipeCount < kRragarsDpRemovalWipeThreshold) {
        return;
    }

    DungeonItemActions::UseItemOptions options;
    options.delay_ms = 5000u;
    const auto result =
        DungeonItemActions::UseDpRemovalSweetIfNeeded(&s_wipeCount, &WaitCombatMs, options);
    if (result.used_model_id != 0u) {
        Log::Info("RragarsDbg: used DP removal sweet model=%u after %u wipes",
                  result.used_model_id,
                  result.previous_wipe_count);
    } else {
        Log::Info("RragarsDbg: DP removal requested after %u wipes but no usable sweet was consumed",
                  result.previous_wipe_count);
    }
}

void RecordRragarsWipeAndRecoverConsumables(const char* context) {
    ++s_wipeCount;
    Log::Info("RragarsDbg: wipe recovery context=%s wipeCount=%u",
              context ? context : "",
              s_wipeCount);
    UseDpRemovalIfNeeded();
}

bool HasRragarsFullConsetActive() {
    const uint32_t myId = AgentMgr::GetMyId();
    return myId != 0u &&
           EffectMgr::HasEffect(myId, GWA3::SkillIds::ARMOR_OF_SALVATION_ITEM_EFFECT) &&
           EffectMgr::HasEffect(myId, GWA3::SkillIds::ESSENCE_OF_CELERITY_ITEM_EFFECT) &&
           EffectMgr::HasEffect(myId, GWA3::SkillIds::GRAIL_OF_MIGHT_ITEM_EFFECT);
}

bool EnsureRragarsConsets(const BotConfig& cfg, const char* context) {
    if (!cfg.use_consets) {
        Log::Info("RragarsDbg: conset attempt context=%s enabled=0 attempted=0 armor=0 essence=0 grail=0 full=1",
                  context ? context : "");
        return true;
    }

    const DWORD start = GetTickCount();
    bool attemptedAny = false;
    bool usedArmor = false;
    bool usedEssence = false;
    bool usedGrail = false;
    bool fullActive = HasRragarsFullConsetActive();
    uint32_t pass = 0u;

    while ((GetTickCount() - start) < 30000u) {
        if (fullActive) {
            Log::Info("RragarsDbg: conset attempt context=%s enabled=1 attempted=%d armor=%d essence=%d grail=%d full=1 pass=%u",
                      context ? context : "",
                      attemptedAny ? 1 : 0,
                      usedArmor ? 1 : 0,
                      usedEssence ? 1 : 0,
                      usedGrail ? 1 : 0,
                      pass);
            return true;
        }

        const auto result = DungeonItemActions::UseConsetsForCurrentPlayerIfEnabled(
            true,
            &WaitCombatMs,
            {},
            "Rragars");
        attemptedAny = attemptedAny || result.attempted;
        usedArmor = usedArmor || result.consets.used_armor;
        usedEssence = usedEssence || result.consets.used_essence;
        usedGrail = usedGrail || result.consets.used_grail;
        fullActive = result.consets.full_active || HasRragarsFullConsetActive();

        if (result.attempted && !fullActive) {
            for (uint32_t settle = 0u; settle < 20u && !fullActive; ++settle) {
                WaitMs(500u);
                fullActive = HasRragarsFullConsetActive();
            }
        } else if (!result.attempted) {
            WaitMs(MapMgr::IsTravelSettling(5000u) ? 750u : 500u);
        }

        ++pass;
    }

    Log::Info("RragarsDbg: conset attempt context=%s enabled=1 attempted=%d armor=%d essence=%d grail=%d full=%d pass=%u loading=%u loaded=%d myId=%u",
              context ? context : "",
              attemptedAny ? 1 : 0,
              usedArmor ? 1 : 0,
              usedEssence ? 1 : 0,
              usedGrail ? 1 : 0,
              fullActive ? 1 : 0,
              pass,
              MapMgr::GetLoadingState(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              AgentMgr::GetMyId());
    return fullActive;
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

uint32_t FindNpcByPlayerNumber(uint16_t playerNumber, float* outX = nullptr, float* outY = nullptr) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    uint32_t bestId = 0u;
    float bestDistSq = 10000000000.0f;
    auto* me = AgentMgr::GetMyAgent();
    const float anchorX = me ? me->x : 0.0f;
    const float anchorY = me ? me->y : 0.0f;

    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) {
            continue;
        }

        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 6u || living->hp <= 0.0f || living->player_number != playerNumber) {
            continue;
        }

        const float distSq = AgentMgr::GetSquaredDistance(anchorX, anchorY, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = living->agent_id;
            if (outX) {
                *outX = living->x;
            }
            if (outY) {
                *outY = living->y;
            }
        }
    }
    return bestId;
}

void LogDoomloreServiceNpcCandidates(const char* label) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    auto* me = AgentMgr::GetMyAgent();
    const float anchorX = me ? me->x : 0.0f;
    const float anchorY = me ? me->y : 0.0f;
    uint32_t emitted = 0u;
    Log::Info("RragarsDbg: Doomlore service NPC numeric scan label=%s map=%u maxAgents=%u player=(%.0f, %.0f)",
              label ? label : "",
              MapMgr::GetMapId(),
              maxAgents,
              anchorX,
              anchorY);

    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDBu) {
            continue;
        }

        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 6u || living->hp <= 0.0f) {
            continue;
        }

        const float distSq = AgentMgr::GetSquaredDistance(anchorX, anchorY, living->x, living->y);
        if (emitted < 48u) {
            Log::Info("RragarsDbg:   npc agent=%u player=%u npc_id=%u effects=0x%08X dist=%.0f pos=(%.0f, %.0f)",
                      living->agent_id,
                      living->player_number,
                      living->transmog_npc_id,
                      living->effects,
                      sqrtf(distSq),
                      living->x,
                      living->y);
            ++emitted;
        }
    }
    Log::Info("RragarsDbg: Doomlore service NPC numeric scan emitted=%u", emitted);
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

void EnableRragarsMapTravelBypasses() {
    static bool enabled = false;
    if (enabled) {
        return;
    }

    bool levelDataEnabled = false;
    bool mapPortEnabled = false;
    auto& levelDataPatch = GWA3::Memory::GetLevelDataBypassPatch();
    if (levelDataPatch.staged) {
        levelDataEnabled = levelDataPatch.Enable();
    }
    auto& mapPortPatch = GWA3::Memory::GetMapPortBypassPatch();
    if (mapPortPatch.staged) {
        mapPortEnabled = mapPortPatch.Enable();
    }
    Log::Info("RragarsDbg: map travel bypass enable levelData=%d mapPort=%d",
              levelDataEnabled ? 1 : 0,
              mapPortEnabled ? 1 : 0);
    enabled = levelDataEnabled || mapPortEnabled;
}

bool TravelOutpostAndWait(
    uint32_t mapId,
    const char* label,
    uint32_t timeoutMs = 90000u,
    uint32_t settleMs = 5000u,
    uint32_t region = 0u,
    uint32_t district = 0u,
    uint32_t language = 0u) {
    const uint32_t currentMap = MapMgr::GetMapId();
    const uint32_t currentRegion = MapMgr::GetRegion();
    const uint32_t currentDistrict = MapMgr::GetDistrict();

    struct TravelAttempt {
        uint32_t region;
        uint32_t district;
        uint32_t language;
        const char* name;
    };

    const TravelAttempt attempts[] = {
        {region, district, language, "requested"},
        {region, 0u, language, "requested-default-district"},
        {currentRegion, 0u, 0u, "current-region-english"},
        {0u, 0u, 0u, "default"},
        {1u, 0u, 0u, "america-english"},
        {4u, 0u, 0u, "asia-japan-english"},
        {4u, 1u, 0u, "asia-japan-district-1-english"},
        {4u, 99u, 8u, "asia-japan-district-99-japanese"},
    };

    auto requestTravel = [&](const TravelAttempt& attempt) -> bool {
        AgentMgr::CancelAction();
        WaitMs(500u);
        Log::Info("RragarsDbg: outpost-travel request label=%s attempt=%s target=%u region=%u district=%u language=%u",
                  label ? label : "",
                  attempt.name,
                  mapId,
                  attempt.region,
                  attempt.district,
                  attempt.language);
        const bool queued = MapMgr::Travel(mapId, attempt.region, attempt.district, attempt.language);
        WaitMs(750u);
        if (MapMgr::GetMapId() == currentMap && MapMgr::GetIsMapLoaded()) {
            Log::Info("RragarsDbg: outpost-travel raw fallback label=%s attempt=%s target=%u current=%u region=%u district=%u language=%u",
                      label ? label : "",
                      attempt.name,
                      mapId,
                      currentMap,
                      attempt.region,
                      attempt.district,
                      attempt.language);
            GameThread::Enqueue([mapId, attempt]() {
                CtoS::MapTravel(mapId, attempt.region, attempt.district, attempt.language);
            });
        }
        return queued;
    };

    Log::Info("RragarsDbg: outpost-travel begin label=%s target=%u current=%u loading=%u currentRegion=%u currentDistrict=%u requestedRegion=%u requestedDistrict=%u requestedLanguage=%u",
              label ? label : "",
              mapId,
              currentMap,
              MapMgr::GetLoadingState(),
              currentRegion,
              currentDistrict,
              region,
              district,
              language);

    if (currentMap == mapId) {
        const bool ready = WaitForMapReady(mapId, timeoutMs);
        const bool stable = ready && WaitForStableOutpostMap(mapId, settleMs);
        Log::Info("RragarsDbg: outpost-travel already-there label=%s target=%u ready=%d stable=%d loading=%u",
                  label ? label : "",
                  mapId,
                  ready ? 1 : 0,
                  stable ? 1 : 0,
                  MapMgr::GetLoadingState());
        return stable;
    }

    if (currentMap != 0u && !MapMgr::GetIsMapLoaded()) {
        const bool settledBeforeTravel = WaitForMapReady(currentMap, 30000u);
        Log::Info("RragarsDbg: outpost-travel pre-settle label=%s current=%u settled=%d loading=%u",
                  label ? label : "",
                  currentMap,
                  settledBeforeTravel ? 1 : 0,
                  MapMgr::GetLoadingState());
        if (!settledBeforeTravel) {
            return false;
        }
    }

    if (!WaitForStableOutpostMap(currentMap, 10000u)) {
        Log::Info("RragarsDbg: outpost-travel current-map did not remain stable before travel label=%s current=%u loading=%u",
                  label ? label : "",
                  currentMap,
                  MapMgr::GetLoadingState());
        return false;
    }
    EnableRragarsMapTravelBypasses();

    const uint32_t perAttemptTimeoutMs =
        timeoutMs < 45000u ? timeoutMs : 45000u;
    DWORD totalElapsed = 0u;
    for (size_t i = 0u; i < (sizeof(attempts) / sizeof(attempts[0])); ++i) {
        const auto& attempt = attempts[i];
        bool duplicate = false;
        for (size_t j = 0u; j < i; ++j) {
            duplicate = attempts[j].region == attempt.region &&
                        attempts[j].district == attempt.district &&
                        attempts[j].language == attempt.language;
            if (duplicate) {
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        const DWORD attemptBudget =
            totalElapsed + perAttemptTimeoutMs > timeoutMs
                ? (timeoutMs > totalElapsed ? timeoutMs - totalElapsed : 0u)
                : perAttemptTimeoutMs;
        if (attemptBudget == 0u) {
            break;
        }

        const DWORD start = GetTickCount();
        if (!requestTravel(attempt)) {
            Log::Info("RragarsDbg: outpost-travel rejected label=%s attempt=%s target=%u current=%u loading=%u",
                      label ? label : "",
                      attempt.name,
                      mapId,
                      MapMgr::GetMapId(),
                      MapMgr::GetLoadingState());
            totalElapsed += GetTickCount() - start;
            continue;
        }

        bool ready = false;
        DWORD lastTravelRequest = start;
        while ((GetTickCount() - start) < attemptBudget) {
            if (MapMgr::GetMapId() == mapId) {
                ready = WaitForMapReady(mapId, 15000u);
                if (ready) {
                    break;
                }
            }

            const DWORD now = GetTickCount();
            if ((now - lastTravelRequest) > 25000u &&
                MapMgr::GetMapId() == currentMap &&
                MapMgr::GetIsMapLoaded()) {
                Log::Info("RragarsDbg: outpost-travel retry label=%s attempt=%s target=%u current=%u elapsed=%lums",
                          label ? label : "",
                          attempt.name,
                          mapId,
                          currentMap,
                          static_cast<unsigned long>(now - start));
                (void)requestTravel(attempt);
                lastTravelRequest = now;
            }
            WaitMs(1000u);
        }

        const bool stable = ready && WaitForStableOutpostMap(mapId, settleMs);
        Log::Info("RragarsDbg: outpost-travel result label=%s attempt=%s target=%u ready=%d stable=%d current=%u loading=%u",
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
        totalElapsed += GetTickCount() - start;
        if (MapMgr::GetMapId() != currentMap) {
            break;
        }
    }

    Log::Info("RragarsDbg: outpost-travel failed label=%s target=%u current=%u loading=%u",
              label ? label : "",
              mapId,
              MapMgr::GetMapId(),
              MapMgr::GetLoadingState());
    return false;
}

bool SkillbarContainsSkill(uint32_t skillId) {
    auto* skillbar = SkillMgr::GetPlayerSkillbar();
    if (!skillbar || skillId == 0u) {
        return false;
    }
    for (const auto& skill : skillbar->skills) {
        if (skill.skill_id == skillId) {
            return true;
        }
    }
    return false;
}

bool HasRragarsPowderKegRelatedEffect() {
    const uint32_t myId = AgentMgr::GetMyId();
    auto* effectArray = myId != 0u ? EffectMgr::GetAgentEffectArray(myId) : nullptr;
    if (!effectArray || !effectArray->buffer) {
        return false;
    }
    for (uint32_t i = 0u; i < effectArray->size; ++i) {
        const Effect& effect = effectArray->buffer[i];
        if (effect.skill_id == GWA3::SkillIds::DWARVEN_POWDER_KEG ||
            effect.skill_id == GWA3::SkillIds::POWDER_KEG_EXPLOSION) {
            return true;
        }
    }
    return false;
}

uint32_t GetRragarsEquippedPowderKegItemId() {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return 0u;
    }

    // Rragars keg stations equip the keg as a bundle-like weapon signal:
    // normal gear is replaced by weapon_type=0 / weapon_item_type=46 with no offhand.
    if (me->weapon_item_id != 0u &&
        me->offhand_item_id == 0u &&
        me->weapon_type == 0u &&
        me->weapon_item_type == 46u) {
        return static_cast<uint32_t>(me->weapon_item_id);
    }
    return 0u;
}

bool HasRragarsPowderKegHeldSignal() {
    return DungeonInteractions::GetHeldBundleItemId() != 0u ||
           SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ||
           GetRragarsEquippedPowderKegItemId() != 0u;
}

bool DropRragarsPowderKeg(bool assumeKegHeld) {
    const uint32_t heldBundle = DungeonInteractions::GetHeldBundleItemId();
    const uint32_t equippedKeg = GetRragarsEquippedPowderKegItemId();
    const bool skillbarKeg = SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG);
    if (heldBundle == 0u && equippedKeg == 0u && !assumeKegHeld) {
        Log::Info("RragarsDbg: powder-keg drop skipped heldBundle=0 equippedKeg=0 skillbarKeg=%d assume=0",
                  skillbarKeg ? 1 : 0);
        return false;
    }

    const bool queued = DungeonInteractions::DropHeldBundle(assumeKegHeld, false);
    Log::Info("RragarsDbg: powder-keg drop queued=%d heldBundle=%u equippedKeg=%u skillbarKeg=%d assume=%d powderKegRelatedEffect=%d",
              queued ? 1 : 0,
              heldBundle,
              equippedKeg,
              skillbarKeg ? 1 : 0,
              assumeKegHeld ? 1 : 0,
              HasRragarsPowderKegRelatedEffect() ? 1 : 0);
    return queued;
}

bool TryPlainSignpostRunForRragarsKeg(float x, float y, float searchRadius, int attempts, uint32_t delayMs) {
    if (attempts <= 0) {
        return false;
    }

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(x, y, searchRadius);
    if (signpostId == 0u) {
        Log::Info("RragarsDbg: plain-signpost keg attempt skipped no signpost center=(%.0f, %.0f) radius=%.0f",
                  x,
                  y,
                  searchRadius);
        return false;
    }

    for (int attempt = 0; attempt < attempts; ++attempt) {
        AgentMgr::InteractSignpost(signpostId);
        Log::Info("RragarsDbg: plain-signpost keg attempt=%d signpost=%u center=(%.0f, %.0f) heldBundle=%u skillbarKeg=%d",
                  attempt + 1,
                  signpostId,
                  x,
                  y,
                  DungeonInteractions::GetHeldBundleItemId(),
                  SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0);
        WaitMs(delayMs);
        if (HasRragarsPowderKegHeldSignal()) {
            return true;
        }
    }

    return HasRragarsPowderKegHeldSignal();
}

void LogPlayerBundleDiagnostics(const char* label) {
    const uint32_t myId = AgentMgr::GetMyId();
    auto* skillbar = SkillMgr::GetPlayerSkillbar();
    const uint32_t heldBundle = DungeonInteractions::GetHeldBundleItemId();
    const uint32_t skill1 = skillbar ? skillbar->skills[0].skill_id : 0u;
    const uint32_t skill2 = skillbar ? skillbar->skills[1].skill_id : 0u;
    const uint32_t skill3 = skillbar ? skillbar->skills[2].skill_id : 0u;
    const uint32_t skill4 = skillbar ? skillbar->skills[3].skill_id : 0u;
    const uint32_t skill5 = skillbar ? skillbar->skills[4].skill_id : 0u;
    const uint32_t skill6 = skillbar ? skillbar->skills[5].skill_id : 0u;
    const uint32_t skill7 = skillbar ? skillbar->skills[6].skill_id : 0u;
    const uint32_t skill8 = skillbar ? skillbar->skills[7].skill_id : 0u;

    auto* effectArray = myId != 0u ? EffectMgr::GetAgentEffectArray(myId) : nullptr;
    auto* buffArray = myId != 0u ? EffectMgr::GetAgentBuffArray(myId) : nullptr;
    const uint32_t effectCount = effectArray ? effectArray->size : 0u;
    const uint32_t buffCount = buffArray ? buffArray->size : 0u;
    const bool skillbarKeg = SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG);
    const bool skillbarExplosion = SkillbarContainsSkill(GWA3::SkillIds::POWDER_KEG_EXPLOSION);
    const bool kegRelatedEffect = HasRragarsPowderKegRelatedEffect();
    auto* me = AgentMgr::GetMyAgent();
    auto* inventory = ItemMgr::GetInventory();
    const uint32_t weaponItemId = me ? me->weapon_item_id : 0u;
    const uint32_t offhandItemId = me ? me->offhand_item_id : 0u;
    const uint32_t activeWeaponSet = inventory && inventory->active_weapon_set < 4u ? inventory->active_weapon_set : 0u;
    const auto* weaponSet = inventory ? &inventory->weapon_sets[activeWeaponSet] : nullptr;
    const uint32_t weaponModelId = weaponSet && weaponSet->weapon ? weaponSet->weapon->model_id : 0u;
    const uint32_t offhandModelId = weaponSet && weaponSet->offhand ? weaponSet->offhand->model_id : 0u;

    Log::Info(
        "RragarsDbg: bundle-state label=%s myId=%u heldBundle=%u "
        "skillbar=[%u,%u,%u,%u,%u,%u,%u,%u] skillbarKeg=%d skillbarExplosion=%d "
        "kegRelatedEffect=%d effectCount=%u buffCount=%u "
        "weaponItem=%u offhandItem=%u activeWeaponSet=%u weaponModel=%u offhandModel=%u weaponType=%u "
        "weaponItemType=%u offhandItemType=%u currentSkill=%u visualEffects=%u",
        label ? label : "",
        myId,
        heldBundle,
        skill1,
        skill2,
        skill3,
        skill4,
        skill5,
        skill6,
        skill7,
        skill8,
        skillbarKeg ? 1 : 0,
        skillbarExplosion ? 1 : 0,
        kegRelatedEffect ? 1 : 0,
        effectCount,
        buffCount,
        weaponItemId,
        offhandItemId,
        activeWeaponSet,
        weaponModelId,
        offhandModelId,
        me ? me->weapon_type : 0u,
        me ? me->weapon_item_type : 0u,
        me ? me->offhand_item_type : 0u,
        me ? me->skill : 0u,
        me ? me->visual_effects : 0u);

    if (effectArray && effectArray->buffer) {
        const uint32_t emit = effectArray->size < 8u ? effectArray->size : 8u;
        for (uint32_t i = 0u; i < emit; ++i) {
            const Effect& effect = effectArray->buffer[i];
            Log::Info(
                "RragarsDbg: bundle-effect label=%s idx=%u skill=%u effectId=%u duration=%.1f",
                label ? label : "",
                i,
                effect.skill_id,
                effect.effect_id,
                effect.duration);
        }
    }

    if (buffArray && buffArray->buffer) {
        const uint32_t emit = buffArray->size < 8u ? buffArray->size : 8u;
        for (uint32_t i = 0u; i < emit; ++i) {
            const Buff& buff = buffArray->buffer[i];
            Log::Info(
                "RragarsDbg: bundle-buff label=%s idx=%u skill=%u buffId=%u",
                label ? label : "",
                i,
                buff.skill_id,
                buff.buff_id);
        }
    }
}

bool TryActionInteractForRragarsKeg(float x, float y, float searchRadius, int attempts, uint32_t delayMs) {
    if (attempts <= 0) {
        return false;
    }

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(x, y, searchRadius);
    if (signpostId == 0u) {
        Log::Info("RragarsDbg: action-interact keg skipped no signpost center=(%.0f, %.0f) radius=%.0f",
                  x,
                  y,
                  searchRadius);
        return false;
    }

    uint32_t gadgetId = 0u;
    if (auto* signpost = AgentMgr::GetAgentByID(signpostId);
        signpost && signpost->type == 0x200u) {
        gadgetId = static_cast<const AgentGadget*>(signpost)->gadget_id;
    }

    const auto approach = DungeonNavigation::MoveToAgent(
        signpostId,
        120.0f,
        8000u,
        500u,
        MapMgr::GetMapId());
    auto* me = AgentMgr::GetMyAgent();
    const float playerDistance = me ? AgentMgr::GetDistance(me->x, me->y, x, y) : -1.0f;
    AgentMgr::ChangeTarget(signpostId);
    WaitMs(150u);
    Log::Info("RragarsDbg: action-interact keg begin signpost=%u gadget=%u center=(%.0f, %.0f) approached=%d dist=%.0f target=%u",
              signpostId,
              gadgetId,
              x,
              y,
              approach.arrived ? 1 : 0,
              playerDistance,
              AgentMgr::GetTargetId());

    bool queuedAny = false;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        const bool worldQueued = AgentMgr::InteractAgentWorldAction(signpostId, true);
        WaitMs(delayMs);
        AgentMgr::ChangeTarget(signpostId);
        WaitMs(150u);
        const bool actionQueued = UIMgr::ActionKeyPress(kActionInteractCode);
        queuedAny = queuedAny || worldQueued || actionQueued;
        WaitMs(delayMs);

        Log::Info("RragarsDbg: action-interact keg attempt=%d signpost=%u gadget=%u worldQueued=%d actionQueued=%d target=%u heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d",
                  attempt + 1,
                  signpostId,
                  gadgetId,
                  worldQueued ? 1 : 0,
                  actionQueued ? 1 : 0,
                  AgentMgr::GetTargetId(),
                  DungeonInteractions::GetHeldBundleItemId(),
                  SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                  HasRragarsPowderKegRelatedEffect() ? 1 : 0);
        LogPlayerBundleDiagnostics("keg-after-action-interact");
        if (HasRragarsPowderKegHeldSignal()) {
            return true;
        }
    }

    return queuedAny && HasRragarsPowderKegHeldSignal();
}

bool IsPlayerOrPartyDead() {
    auto* me = AgentMgr::GetMyAgent();
    return me == nullptr || me->hp <= 0.0f || PartyMgr::GetIsPartyDefeated();
}

bool IsCurrentMapLoaded() {
    return MapMgr::GetMapId() != 0u && MapMgr::GetLoadingState() == 1u;
}

bool IsRragarsDungeonMap(uint32_t mapId) {
    return mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1 ||
           mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL2 ||
           mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3;
}

void QueueAggroMove(float x, float y) {
    AgentMgr::Move(x, y);
}

uint32_t ResolveRragarsTargetTimeout(uint32_t timeoutMs) {
    return timeoutMs < kRragarsTargetStaleTimeoutMs ? timeoutMs : kRragarsTargetStaleTimeoutMs;
}

void ConfigureRragarsClearOptions(
    DungeonCombat::ClearEnemiesOptions& options,
    uint32_t timeoutMs) {
    // Direct target/call/flag packets during movement-adjacent clears can
    // destabilize GW. Keep the proven attack-only flow, but fail stale targets
    // quickly instead of waiting the full route waypoint timeout.
    options.change_target = false;
    options.call_target = false;
    options.flag_heroes = false;
    options.target_timeout_ms = ResolveRragarsTargetTimeout(timeoutMs);
}

void ConfigureLegacyAggroMoveOptions(
    DungeonCombat::AggroAdvanceOptions& options,
    uint32_t timeoutMs) {
    options.timeout_ms = timeoutMs;
    // Match the AutoIt AggroMoveToEX behavior more closely: only care about
    // foes inside the actual waypoint fight range instead of the shared
    // Froggy-style local clear floor.
    options.clear_options.extra_clear_range = 0.0f;
    options.clear_options.minimum_local_clear_range = 0.0f;
    options.clear_options.chase_during_clear = false;
    options.clear_options.hold_movement_for_local_clear = true;
    options.clear_options.quiet_confirmation_ms = 1250u;
    options.clear_options.chase_wait_ms = 350u;
    options.clear_options.pre_clear_cancel_wait_ms = 50u;
    options.clear_options.post_clear_cancel_wait_ms = 150u;
    options.clear_options.idle_wait_ms = 150u;
    options.clear_options.loop_wait_ms = 250u;
    options.clear_options.fight_reissue_ms = 750u;
    options.clear_options.attack_reissue_ms = 750u;
    options.clear_options.timeout_ms = timeoutMs;
    ConfigureRragarsClearOptions(options.clear_options, timeoutMs);
    options.move_wait_ms = 100u;
    options.stuck_recovery_threshold = 30;
    options.stuck_abort_threshold = 240;
    options.stuck_recovery_radius = 900.0f;
}

DungeonCombat::CombatCallbacks MakeRragarsCombatCallbacks() {
    DungeonCombat::CombatCallbacks callbacks;
    callbacks.is_dead = &IsPlayerOrPartyDead;
    callbacks.is_map_loaded = &IsCurrentMapLoaded;
    callbacks.wait_ms = &WaitCombatMs;
    callbacks.queue_move = &QueueAggroMove;
    callbacks.fight_target = &DungeonBuiltinCombat::FightTargetWithPriorityBuiltinCombat;
    return callbacks;
}

bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs = 15000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
            WaitMs(250u);
            continue;
        }

        if (AgentMgr::GetMyId() == 0u) {
            WaitMs(250u);
            continue;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (!me || me->hp <= 0.0f) {
            WaitMs(250u);
            continue;
        }

        if (me->x == 0.0f && me->y == 0.0f) {
            WaitMs(250u);
            continue;
        }

        return true;
    }
    return false;
}

bool WaitForSpawnAwayFromPoint(
    uint32_t mapId,
    float staleX,
    float staleY,
    float minimumDistance = 3000.0f,
    uint32_t timeoutMs = 15000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded() || AgentMgr::GetMyId() == 0u) {
            WaitMs(250u);
            continue;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (!me || me->hp <= 0.0f) {
            WaitMs(250u);
            continue;
        }

        if (me->x == 0.0f && me->y == 0.0f) {
            WaitMs(250u);
            continue;
        }

        if (AgentMgr::GetDistance(me->x, me->y, staleX, staleY) > minimumDistance) {
            return true;
        }

        WaitMs(200u);
    }
    return false;
}

bool WaitForPartyRecovery(uint32_t mapId, uint32_t timeoutMs = 120000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
            WaitMs(500u);
            continue;
        }

        auto* me = AgentMgr::GetMyAgent();
        if (me && me->hp > 0.75f && !PartyMgr::GetIsPartyDefeated()) {
            Log::Info("RragarsDbg: party recovery complete map=%u player=(%.0f, %.0f) hp=%.2f",
                      mapId,
                      me->x,
                      me->y,
                      me->hp);
            WaitMs(1500u);
            return true;
        }
        WaitMs(500u);
    }

    auto* me = AgentMgr::GetMyAgent();
    Log::Info("RragarsDbg: party recovery timed out map=%u currentMap=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d",
              mapId,
              MapMgr::GetMapId(),
              me ? me->x : 0.0f,
              me ? me->y : 0.0f,
              me ? me->hp : 0.0f,
              PartyMgr::GetIsPartyDefeated() ? 1 : 0);
    return false;
}

bool WaitForPartyHeroes(uint32_t minHeroes, uint32_t timeoutMs = 15000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetIsMapLoaded() && PartyMgr::CountPartyHeroes() >= minHeroes) {
            return true;
        }
        WaitMs(250u);
    }

    auto* me = AgentMgr::GetMyAgent();
    Log::Info("RragarsDbg: hero wait timed out minHeroes=%u heroes=%u map=%u loaded=%d myId=%u player=(%.0f, %.0f) hp=%.2f",
              minHeroes,
              PartyMgr::CountPartyHeroes(),
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              AgentMgr::GetMyId(),
              me ? me->x : 0.0f,
              me ? me->y : 0.0f,
              me ? me->hp : 0.0f);
    return false;
}

bool ZoneThroughPoint(float x, float y, uint32_t targetMapId, uint32_t timeoutMs = 60000u) {
    auto* before = AgentMgr::GetMyAgent();
    const bool hasStalePosition = before && (before->x != 0.0f || before->y != 0.0f);
    const float staleX = before ? before->x : 0.0f;
    const float staleY = before ? before->y : 0.0f;
    auto waitForHydratedTarget = [&]() {
        if (!WaitForMapReady(targetMapId, 30000u)) {
            return false;
        }
        if (!hasStalePosition) {
            return true;
        }
        return WaitForSpawnAwayFromPoint(targetMapId, staleX, staleY, 1500.0f, 30000u);
    };

    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        AgentMgr::Move(x, y);
        if (MapMgr::GetMapId() == targetMapId) {
            return waitForHydratedTarget();
        }
        if (DungeonNavigation::WaitForMapId(targetMapId, 250u)) {
            return waitForHydratedTarget();
        }
        WaitMs(250u);
    }
    return false;
}

bool MoveToPointWithAggro(
    float x,
    float y,
    uint32_t mapId,
    float tolerance,
    float fightRange,
    uint32_t timeoutMs) {
    if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
        return false;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (me && AgentMgr::GetDistance(me->x, me->y, x, y) <= tolerance) {
        return true;
    }

    DungeonCombat::AggroAdvanceOptions options;
    options.arrival_threshold = tolerance;
    options.clear_options.pickup_after_clear = false;
    ConfigureLegacyAggroMoveOptions(options, timeoutMs);
    options.clear_options.extra_clear_range = 400.0f;
    options.clear_options.minimum_local_clear_range = fightRange;
    options.clear_options.chase_distance = 1800.0f;
    const bool arrived = DungeonCombat::AdvanceWithAggro(
        x,
        y,
        fightRange,
        MakeRragarsCombatCallbacks(),
        options);
    if (!arrived) {
        auto* after = AgentMgr::GetMyAgent();
        const float remaining = DungeonCombat::DistanceToPoint(x, y);
        const float acceptedRemaining = tolerance > kAggroNearArrivalAfterClearFailureTolerance
            ? tolerance
            : kAggroNearArrivalAfterClearFailureTolerance;
        if (after &&
            after->hp > 0.0f &&
            MapMgr::GetMapId() == mapId &&
            MapMgr::GetIsMapLoaded() &&
            remaining <= acceptedRemaining) {
            Log::Info("RragarsDbg: aggro-move accepted near arrival after local clear failure target=(%.0f, %.0f) map=%u remaining=%.0f tolerance=%.0f acceptTolerance=%.0f fightRange=%.0f timeout=%u",
                      x,
                      y,
                      mapId,
                      remaining,
                      tolerance,
                      acceptedRemaining,
                      fightRange,
                      timeoutMs);
            return true;
        }
        Log::Info("RragarsDbg: aggro-move failed target=(%.0f, %.0f) map=%u currentMap=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d remaining=%.0f fightRange=%.0f timeout=%u",
                  x,
                  y,
                  mapId,
                  MapMgr::GetMapId(),
                  after ? after->x : 0.0f,
                  after ? after->y : 0.0f,
                  after ? after->hp : 0.0f,
                  PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                  remaining,
                  fightRange,
                  timeoutMs);
        LogBot("Rragars: aggro move timed out target=(%.0f, %.0f) map=%u remaining=%.0f fight_range=%.0f timeout=%u",
               x,
               y,
               mapId,
               remaining,
               fightRange,
               timeoutMs);
    }
    return arrived;
}

bool MoveToWaypoint(
    const DungeonRoute::Waypoint& waypoint,
    uint32_t mapId,
    float tolerance = 250.0f,
    uint32_t timeoutMs = LEGACY_AGGRO_MOVE_TIMEOUT_MS) {
    if (mapId != GWA3::MapIds::DOOMLORE_SHRINE) {
        const float fightRange = waypoint.fight_range > 0.0f ? waypoint.fight_range : 1200.0f;
        return MoveToPointWithAggro(
            waypoint.x,
            waypoint.y,
            mapId,
            tolerance,
            fightRange,
            timeoutMs);
    }

    return DungeonNavigation::MoveToAndWait(
        waypoint.x,
        waypoint.y,
        tolerance,
        timeoutMs,
        1000u,
        mapId).arrived;
}

bool MoveToPoint(
    float x,
    float y,
    uint32_t mapId,
    float tolerance = 250.0f,
    uint32_t timeoutMs = LEGACY_AGGRO_MOVE_TIMEOUT_MS,
    float fightRange = 1200.0f) {
    if (mapId != GWA3::MapIds::DOOMLORE_SHRINE) {
        return MoveToPointWithAggro(x, y, mapId, tolerance, fightRange, timeoutMs);
    }

    return DungeonNavigation::MoveToAndWait(
        x,
        y,
        tolerance,
        timeoutMs,
        1000u,
        mapId).arrived;
}

bool ClearRragarsLocalArea(const char* label, float fightRange, uint32_t timeoutMs) {
    DungeonCombat::ClearEnemiesOptions options;
    options.minimum_engage_range = fightRange;
    options.minimum_local_clear_range = fightRange;
    options.extra_clear_range = 400.0f;
    options.chase_distance = 1800.0f;
    options.timeout_ms = timeoutMs;
    ConfigureRragarsClearOptions(options, timeoutMs);
    options.quiet_confirmation_ms = 1500u;
    options.pickup_after_clear = false;

    const bool cleared = DungeonCombat::ClearEnemiesInArea(
        fightRange,
        MakeRragarsCombatCallbacks(),
        options);
    auto* me = AgentMgr::GetMyAgent();
    Log::Info("RragarsDbg: local-clear label=%s cleared=%d fightRange=%.0f player=(%.0f, %.0f) hp=%.2f nearby=%u",
              label ? label : "",
              cleared ? 1 : 0,
              fightRange,
              me ? me->x : 0.0f,
              me ? me->y : 0.0f,
              me ? me->hp : 0.0f,
              DungeonCombat::CountLivingEnemiesInRange(fightRange + 400.0f));
    return cleared;
}

bool MoveRewardPointWithAggro(
    float x,
    float y,
    float tolerance,
    uint32_t timeoutMs,
    float fightRange) {
    if (MapMgr::GetMapId() != GWA3::MapIds::RRAGARS_MENAGERIE_LVL3 || !MapMgr::GetIsMapLoaded()) {
        return false;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (me && AgentMgr::GetDistance(me->x, me->y, x, y) <= tolerance) {
        return true;
    }

    DungeonCombat::AggroAdvanceOptions options;
    ConfigureLegacyAggroMoveOptions(options, timeoutMs);
    options.arrival_threshold = tolerance;
    options.stuck_recovery_threshold = 30;
    options.stuck_abort_threshold = 120;
    options.stuck_recovery_radius = 900.0f;
    options.clear_options.pickup_after_clear = false;
    options.clear_options.minimum_engage_range = fightRange;
    options.clear_options.minimum_local_clear_range = fightRange;
    options.clear_options.extra_clear_range = 400.0f;
    options.clear_options.chase_distance = 1800.0f;

    const bool arrived = DungeonCombat::AdvanceWithAggro(
        x,
        y,
        fightRange,
        MakeRragarsCombatCallbacks(),
        options);
    if (!arrived) {
        auto* after = AgentMgr::GetMyAgent();
        const float remaining = DungeonCombat::DistanceToPoint(x, y);
        Log::Info("RragarsDbg: reward aggro-move failed target=(%.0f, %.0f) currentMap=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d remaining=%.0f fightRange=%.0f timeout=%u",
                  x,
                  y,
                  MapMgr::GetMapId(),
                  after ? after->x : 0.0f,
                  after ? after->y : 0.0f,
                  after ? after->hp : 0.0f,
                  PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                  remaining,
                  fightRange,
                  timeoutMs);
        const float acceptedRemaining = tolerance > kRewardNearArrivalAfterClearFailureTolerance
            ? tolerance
            : kRewardNearArrivalAfterClearFailureTolerance;
        if (after &&
            after->hp > 0.0f &&
            MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3 &&
            MapMgr::GetIsMapLoaded() &&
            remaining <= acceptedRemaining) {
            Log::Info("RragarsDbg: reward aggro-move accepted near live arrival target=(%.0f, %.0f) remaining=%.0f tolerance=%.0f acceptTolerance=%.0f",
                      x,
                      y,
                      remaining,
                      tolerance,
                      acceptedRemaining);
            return true;
        }
    }
    return arrived;
}

bool MoveRewardPointWithRecovery(
    const char* label,
    float x,
    float y,
    float tolerance,
    uint32_t timeoutMs,
    float fightRange = 1800.0f) {
    constexpr int kMaxRewardRecoveryRetries = 6;
    for (int retry = 0; retry <= kMaxRewardRecoveryRetries; ++retry) {
        if (MoveRewardPointWithAggro(
                x,
                y,
                tolerance,
                timeoutMs,
                fightRange)) {
            if (retry > 0) {
                Log::Info("RragarsDbg: reward %s recovered retry=%d playerRemaining=%.0f",
                          label,
                          retry,
                          DungeonCombat::DistanceToPoint(x, y));
            }
            return true;
        }

        auto* me = AgentMgr::GetMyAgent();
        Log::Info("RragarsDbg: reward %s move failed retry=%d player=(%.0f, %.0f) hp=%.2f remaining=%.0f",
                  label,
                  retry,
                  me ? me->x : 0.0f,
                  me ? me->y : 0.0f,
                  me ? me->hp : 0.0f,
                  DungeonCombat::DistanceToPoint(x, y));

        if (!me || retry == kMaxRewardRecoveryRetries) {
            return false;
        }
        if (me->hp > 0.0f) {
            WaitCombatMs(3000u);
            continue;
        }
        if (!WaitForPartyRecovery(GWA3::MapIds::RRAGARS_MENAGERIE_LVL3, 180000u)) {
            return false;
        }
    }
    return false;
}

bool MoveCheckpointWaypointForRoute(const DungeonRoute::Waypoint& waypoint, const void* context) {
    const auto* route = static_cast<const RouteDefinition*>(context);
    return route != nullptr && MoveToWaypoint(waypoint, route->map_id);
}

bool IsExplorableTravelRoute(RouteId routeId) {
    switch (routeId) {
    case RouteId::RunDaladaToGrothmar:
    case RouteId::RunGrothmarToSacnoth:
    case RouteId::RunSacnothToDungeon:
        return true;
    default:
        return false;
    }
}

bool FollowTravelRouteWithRetries(RouteId routeId) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        return false;
    }

    DungeonCombat::AggroAdvanceOptions aggroOptions;
    aggroOptions.clear_options.pickup_after_clear = false;
    aggroOptions.move_wait_ms = 100u;
    uint32_t waypointTimeoutMs = 120000u;

    if (routeId == RouteId::RunSacnothToDungeon) {
        // SAMPLE FIVE cannot reliably sprint the Sacnoth approach. Keep movement
        // held while clearing a modest bubble instead of dragging packed patrols
        // through the narrow waypoint-4-to-7 corridor.
        aggroOptions.clear_options.extra_clear_range = 300.0f;
        aggroOptions.clear_options.minimum_local_clear_range = 1800.0f;
        aggroOptions.clear_options.chase_distance = 1800.0f;
        // Sacnoth has several narrow turns where the generic stuck monitor
        // aborts earlier than the original AutoIt AggroMoveToEX flow.
        // Legacy AggroMoveToEX allows up to four minutes before giving up on
        // a leg. The waypoint-7/8 approach in Sacnoth still needs that headroom.
        waypointTimeoutMs = 240000u;
        aggroOptions.stuck_recovery_threshold = 30;
        aggroOptions.stuck_abort_threshold = 240;
        aggroOptions.stuck_recovery_radius = 900.0f;
    }

    aggroOptions.timeout_ms = waypointTimeoutMs;
    aggroOptions.clear_options.timeout_ms = waypointTimeoutMs;
    ConfigureRragarsClearOptions(aggroOptions.clear_options, waypointTimeoutMs);
    aggroOptions.clear_options.chase_during_clear = false;
    aggroOptions.clear_options.hold_movement_for_local_clear = true;
    aggroOptions.clear_options.quiet_confirmation_ms = 1250u;
    aggroOptions.clear_options.chase_wait_ms = 350u;
    aggroOptions.clear_options.pre_clear_cancel_wait_ms = 50u;
    aggroOptions.clear_options.post_clear_cancel_wait_ms = 150u;
    aggroOptions.clear_options.idle_wait_ms = 150u;
    aggroOptions.clear_options.loop_wait_ms = 250u;
    aggroOptions.clear_options.fight_reissue_ms = 750u;
    aggroOptions.clear_options.attack_reissue_ms = 750u;
    if (routeId == RouteId::RunDaladaToGrothmar) {
        // The first Dalada patrol can kill SAMPLE FIVE if movement continues while
        // the local clear is trying to fight. Hold and clear a wider bubble.
        aggroOptions.clear_options.extra_clear_range = 700.0f;
        aggroOptions.clear_options.minimum_local_clear_range = 1800.0f;
        aggroOptions.clear_options.chase_distance = 1800.0f;
    }

    constexpr int kMaxTravelRecoveryRetries = 8;
    int recoveryRetries = 0;
    const int startIndex = DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);
    for (int i = startIndex; i < route.waypoint_count; ++i) {
        const float fightRange = route.waypoints[i].fight_range > 0.0f
            ? route.waypoints[i].fight_range
            : aggroOptions.clear_options.minimum_engage_range;
        auto waypointOptions = aggroOptions;
        waypointOptions.arrival_threshold = 250.0f;

        const bool arrived = DungeonCombat::AdvanceWithAggro(
            route.waypoints[i].x,
            route.waypoints[i].y,
            fightRange,
            MakeRragarsCombatCallbacks(),
            waypointOptions);
        if (MapMgr::GetMapId() != route.map_id) {
            return true;
        }

        auto* after = AgentMgr::GetMyAgent();
        const bool partyDefeated = PartyMgr::GetIsPartyDefeated();
        const bool playerDead = after == nullptr || after->hp <= 0.0f || partyDefeated;
        const bool mapReady = !playerDead && WaitForMapReady(route.map_id, 10000u);
        if (playerDead || !mapReady) {
            Log::Info("RragarsDbg: travel waypoint interrupted route=%s waypoint=%d label=%s arrived=%d currentMap=%u expectedMap=%u loading=%u loaded=%d myId=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d remaining=%.0f nearby=%u recoveries=%d",
                      route.name,
                      i,
                      route.waypoints[i].label,
                      arrived ? 1 : 0,
                      MapMgr::GetMapId(),
                      route.map_id,
                      MapMgr::GetLoadingState(),
                      MapMgr::GetIsMapLoaded() ? 1 : 0,
                      AgentMgr::GetMyId(),
                      after ? after->x : 0.0f,
                      after ? after->y : 0.0f,
                      after ? after->hp : 0.0f,
                      partyDefeated ? 1 : 0,
                      DungeonCombat::DistanceToPoint(route.waypoints[i].x, route.waypoints[i].y),
                      DungeonCombat::CountLivingEnemiesInRange(fightRange + 600.0f),
                      recoveryRetries);

            if (MapMgr::GetMapId() != route.map_id) {
                return true;
            }

            if (playerDead && recoveryRetries < kMaxTravelRecoveryRetries &&
                WaitForPartyRecovery(route.map_id, 180000u)) {
                ++recoveryRetries;
                auto* recovered = AgentMgr::GetMyAgent();
                int retryIndex = recovered
                    ? DungeonRoute::FindNearestWaypointIndex(
                          route.waypoints,
                          route.waypoint_count,
                          recovered->x,
                          recovered->y)
                    : i;
                if (retryIndex < 0) {
                    retryIndex = i;
                }
                if (retryIndex >= route.waypoint_count) {
                    retryIndex = route.waypoint_count - 1;
                }
                LogBot("Rragars: recovered during %s travel near waypoint %d (%s); retrying from waypoint %d (%s)",
                       route.name,
                       i,
                       route.waypoints[i].label,
                       retryIndex,
                       route.waypoints[retryIndex].label);
                i = retryIndex - 1;
                continue;
            }

            if (!playerDead && recoveryRetries < kMaxTravelRecoveryRetries &&
                MapMgr::GetMapId() == route.map_id && MapMgr::GetIsMapLoaded()) {
                ++recoveryRetries;
                LogBot("Rragars: map-ready wait failed during %s travel waypoint %d (%s); retrying waypoint",
                       route.name,
                       i,
                       route.waypoints[i].label);
                --i;
                WaitMs(2000u);
                continue;
            }

            LogBot("Rragars: travel route %s aborted at waypoint %d (%s) after recovery attempts",
                   route.name,
                   i,
                   route.waypoints[i].label);
            return false;
        }
        if (!arrived) {
            LogBot("Rragars: autoit-style continue after timeout on %s waypoint %d (%s) remaining=%.0f",
                   route.name,
                   i,
                   route.waypoints[i].label,
                   DungeonCombat::DistanceToPoint(route.waypoints[i].x, route.waypoints[i].y));
        }
    }

    return true;
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

DungeonCheckpoint::CheckpointResolution EvaluateRragarsCheckpointResolution(
    WaypointBehavior behavior,
    int currentWaypoint,
    int nearestWaypoint,
    int waypointCount,
    const DungeonCheckpoint::CheckpointRetryPolicy* policy) {
    if (behavior == WaypointBehavior::ValidateRetryCheckpoint &&
        nearestWaypoint >= currentWaypoint) {
        return {};
    }

    return behavior == WaypointBehavior::ValidateRetryCheckpoint
        ? DungeonCheckpoint::EvaluateAdvanceCheckpointResolution(
              currentWaypoint,
              nearestWaypoint,
              waypointCount,
              policy)
        : DungeonCheckpoint::EvaluateCheckpointResolution(
              currentWaypoint,
              nearestWaypoint,
              waypointCount,
              policy);
}

bool ReplayCheckpointBacktrack(const RouteDefinition& route, int currentIndex, int backtrackStart) {
    DungeonCheckpoint::CheckpointBacktrackReplayOptions options;
    options.waypoints = route.waypoints;
    options.waypoint_count = route.waypoint_count;
    options.current_index = currentIndex;
    options.backtrack_start = backtrackStart;
    options.move_waypoint_with_context = &MoveCheckpointWaypointForRoute;
    options.move_context = &route;

    const auto replay = DungeonCheckpoint::ReplayCheckpointBacktrack(options);
    if (!replay.completed) {
        LogBot("Rragars: failed checkpoint backtrack at waypoint %d (%s) on %s",
               replay.failed_index,
               replay.failed_index >= 0 && replay.failed_index < route.waypoint_count
                   ? route.waypoints[replay.failed_index].label
                   : "",
               route.name);
        return false;
    }
    return true;
}

bool HasVeiledThreatQuest() {
    auto* quest = QuestMgr::GetQuestById(GWA3::QuestIds::VEILED_THREAT);
    return quest != nullptr && (quest->log_state & kQuestLogStateCompleted) == 0u;
}

void LogVeiledThreatQuestSnapshot(const char* label) {
    auto* quest = QuestMgr::GetQuestById(GWA3::QuestIds::VEILED_THREAT);
    Log::Info("RragarsDbg: Veiled Threat snapshot label=%s activeQuest=0x%X present=%d questLogSize=%u",
              label ? label : "",
              QuestMgr::GetActiveQuestId(),
              quest != nullptr ? 1 : 0,
              QuestMgr::GetQuestLogSize());
    if (quest) {
        Log::Info("RragarsDbg: Veiled Threat snapshot detail label=%s logState=0x%X completed=%d mapFrom=%u mapTo=%u marker=(%.0f, %.0f)",
                  label ? label : "",
                  quest->log_state,
                  (quest->log_state & kQuestLogStateCompleted) != 0u ? 1 : 0,
                  quest->map_from,
                  quest->map_to,
                  quest->marker_x,
                  quest->marker_y);
    }
}

bool WaitForVeiledThreatReady(const char* label, uint32_t timeoutMs) {
    DungeonQuestRuntime::QuestVerificationOptions options;
    options.timeout_ms = timeoutMs;
    options.refresh_interval_ms = 1000u;
    options.poll_ms = 100u;
    options.post_set_active_delay_ms = 500u;
    options.require_not_completed_when_present = true;
    const bool ready = DungeonQuestRuntime::WaitForQuestState(
        GWA3::QuestIds::VEILED_THREAT,
        true,
        options);
    LogVeiledThreatQuestSnapshot(label);
    return ready && HasVeiledThreatQuest();
}

bool InteractNearestNpcAndSendDialogs(float npcX, float npcY, float searchRadius, const uint32_t* dialogs, int dialogCount) {
    if (dialogs == nullptr || dialogCount <= 0) {
        return false;
    }

    DungeonQuest::QuestNpcAnchor npc = {};
    npc.x = npcX;
    npc.y = npcY;
    npc.search_radius = searchRadius;

    DungeonQuest::DialogPlan plan = {};
    plan.dialog_ids = dialogs;
    plan.dialog_count = dialogCount;
    plan.dialog_repeats = 1;

    DungeonQuestRuntime::DialogExecutionOptions options;
    options.move_to_actual_npc = true;
    options.move_to_npc_tolerance = 120.0f;
    options.move_to_npc_timeout_ms = 20000u;
    options.cancel_action_before_interact = true;
    options.clear_dialog_state_before_interact = true;
    options.require_dialog_before_send = true;
    options.pre_interact_settle_ms = 500u;
    options.change_target_delay_ms = 250u;
    options.interact_count = 3;
    options.interact_delay_ms = 1500u;
    options.post_interact_delay_ms = 1000u;
    options.dialog_wait_timeout_ms = 2500u;
    options.dialog_delay_ms = 500u;
    options.repeat_delay_ms = 750u;
    options.max_retries_per_dialog = 2;
    options.use_direct_npc_interact = true;

    const bool sent = DungeonQuestRuntime::InteractNearestNpcAndSendDialogPlan(
        npc,
        plan,
        options);
    Log::Info("RragarsDbg: quest dialog plan sent=%d npc=(%.0f, %.0f) search=%.0f dialogCount=%d",
              sent ? 1 : 0,
              npcX,
              npcY,
              searchRadius,
              dialogCount);
    return sent;
}

bool EnsureVeiledThreatQuest() {
    if (HasVeiledThreatQuest()) {
        auto* quest = QuestMgr::GetQuestById(GWA3::QuestIds::VEILED_THREAT);
        Log::Info("RragarsDbg: Veiled Threat present activeQuest=%u questLogSize=%u questPtr=%p",
                  QuestMgr::GetActiveQuestId(),
                  QuestMgr::GetQuestLogSize(),
                  quest);
        if (quest) {
            Log::Info("RragarsDbg: Veiled Threat quest state logState=0x%X mapFrom=%u mapTo=%u marker=(%.0f, %.0f)",
                      quest->log_state,
                      quest->map_from,
                      quest->map_to,
                      quest->marker_x,
                      quest->marker_y);
        }
        QuestMgr::SetActiveQuest(GWA3::QuestIds::VEILED_THREAT);
        WaitMs(500u);
        return true;
    }
    if (auto* staleQuest = QuestMgr::GetQuestById(GWA3::QuestIds::VEILED_THREAT)) {
        Log::Info("RragarsDbg: Veiled Threat stale/complete logState=0x%X activeQuest=%u; refreshing quest",
                  staleQuest->log_state,
                  QuestMgr::GetActiveQuestId());
    }
    LogVeiledThreatQuestSnapshot("before-refresh");

    static constexpr uint32_t kCompleteDialogs[] = {
        GWA3::DialogIds::GENERIC_ACCEPT,
        GWA3::DialogIds::RragarsMenagerie::COMPLETE_OLD_QUEST,
        GWA3::DialogIds::GENERIC_ACCEPT,
        GWA3::DialogIds::RragarsMenagerie::COMPLETE_OLD_QUEST,
    };
    static constexpr uint32_t kAcceptDialogs[] = {
        GWA3::DialogIds::GENERIC_ACCEPT,
        GWA3::DialogIds::RragarsMenagerie::PICK_VEILED_THREAT,
        GWA3::DialogIds::GENERIC_ACCEPT,
        GWA3::DialogIds::RragarsMenagerie::ACCEPT_VEILED_THREAT,
    };

    LogBot("Rragars: acquiring Veiled Threat in Doomlore Shrine");
    if (!DungeonNavigation::MoveToAndWait(-17583.0f, 17668.0f, 250.0f, 20000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived ||
        !DungeonNavigation::MoveToAndWait(kQuestNpcX, kQuestNpcY, 250.0f, 20000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived) {
        return false;
    }

    if (!InteractNearestNpcAndSendDialogs(kQuestNpcX, kQuestNpcY, 1500.0f, kCompleteDialogs, 4)) {
        LogVeiledThreatQuestSnapshot("complete-dialog-failed");
        return false;
    }
    LogVeiledThreatQuestSnapshot("after-complete-dialogs");

    if (!TravelOutpostAndWait(GWA3::MapIds::LONGEYES_LEDGE, "veiled-threat-reward-bounce-to-longeye")) {
        LogVeiledThreatQuestSnapshot("longeye-travel-failed");
        return false;
    }
    if (!TravelOutpostAndWait(GWA3::MapIds::DOOMLORE_SHRINE, "veiled-threat-reward-bounce-to-doomlore")) {
        LogVeiledThreatQuestSnapshot("doomlore-return-failed");
        return false;
    }
    LogVeiledThreatQuestSnapshot("after-reward-bounce");

    if (!DungeonNavigation::MoveToAndWait(kQuestNpcX, kQuestNpcY, 250.0f, 20000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived) {
        LogVeiledThreatQuestSnapshot("accept-npc-move-failed");
        return false;
    }
    if (!InteractNearestNpcAndSendDialogs(kQuestNpcX, kQuestNpcY, 1500.0f, kAcceptDialogs, 4)) {
        LogVeiledThreatQuestSnapshot("accept-dialog-failed-attempt1");
        return false;
    }
    if (WaitForVeiledThreatReady("after-accept-attempt1", 8000u)) {
        return true;
    }

    WaitMs(1000);
    if (!InteractNearestNpcAndSendDialogs(kQuestNpcX, kQuestNpcY, 1500.0f, kAcceptDialogs, 4)) {
        LogVeiledThreatQuestSnapshot("accept-dialog-failed-attempt2");
        return false;
    }

    if (!WaitForVeiledThreatReady("after-accept-attempt2", 8000u)) {
        LogBot("Rragars: Veiled Threat was not confirmed in quest log after dialog flow");
        return false;
    }
    return true;
}

bool LeaveDoomloreForDalada() {
    LogBot("Rragars: leaving Doomlore Shrine for Dalada Uplands");
    if (!WaitForMapReady(GWA3::MapIds::DOOMLORE_SHRINE, 15000u) ||
        !DungeonNavigation::MoveToAndWait(-15024.0f, 16571.0f, 250.0f, 15000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived ||
        !DungeonNavigation::MoveToAndWait(-15968.0f, 14434.0f, 250.0f, 15000u, 1000u, GWA3::MapIds::DOOMLORE_SHRINE).arrived) {
        return false;
    }

    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < 60000u) {
        AgentMgr::Move(-15366.0f, 13553.0f);
        if (MapMgr::GetMapId() == GWA3::MapIds::DALADA_UPLANDS ||
            DungeonNavigation::WaitForMapId(GWA3::MapIds::DALADA_UPLANDS, 250u)) {
            if (!WaitForMapReady(GWA3::MapIds::DALADA_UPLANDS, 30000u)) {
                return false;
            }
            if (!WaitForPartyHeroes(7u, 15000u)) {
                LogBot("Rragars: Dalada loaded without full hero party; returning to Doomlore");
                (void)TravelOutpostAndWait(GWA3::MapIds::DOOMLORE_SHRINE, "dalada-missing-heroes-return");
                return false;
            }
            return true;
        }
        WaitMs(250u);
    }
    return false;
}

DungeonVendor::MaintenanceLocation MakeRragarsMaintenanceLocation() {
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

void TuneRragarsMaintenanceConfig(MaintenanceMgr::Config& config) {
    config.minFreeSlots = 8u;
    config.enableConsetRestock = true;
    config.targetCharacterConsetsEach = 2u;
}

MaintenanceMgr::Config MakeRragarsMaintenanceConfig() {
    auto config = DungeonVendor::BuildMaintenanceConfig(
        GWA3::MapIds::GADDS_ENCAMPMENT,
        MakeRragarsMaintenanceLocation());
    TuneRragarsMaintenanceConfig(config);
    return config;
}

bool RestockRragarsConsetsViaEmbark(const MaintenanceMgr::Config& config) {
    if (!MaintenanceMgr::NeedsCharacterConsetRestock(config)) {
        return true;
    }

    LogBot("Rragars: carried consets below target; restocking from Embark Beach Xunlai");
    if (!TravelOutpostAndWait(
            GWA3::MapIds::EMBARK_BEACH,
            "conset-restock-to-embark",
            120000u,
            5000u,
            kMaintenanceTravelRegion,
            kMaintenanceTravelDistrict,
            kMaintenanceTravelLanguage)) {
        LogBot("Rragars: failed reaching Embark Beach for conset restock");
        return false;
    }

    MaintenanceMgr::OpenXunlaiChest(kEmbarkXunlaiX, kEmbarkXunlaiY);
    const uint32_t withdrawn =
        MaintenanceMgr::WithdrawMissingConsetsFromStorage(config.targetCharacterConsetsEach);
    WaitMs(1000u + withdrawn * 250u);

    const bool stocked = MaintenanceMgr::HasCharacterConsetSet(config.targetCharacterConsetsEach);
    Log::Info("RragarsDbg: Embark conset restock result withdrawn=%u stocked=%d consets=%u/%u/%u",
              withdrawn,
              stocked ? 1 : 0,
              MaintenanceMgr::CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT),
              MaintenanceMgr::CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY),
              MaintenanceMgr::CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION));

    if (!TravelOutpostAndWait(
            GWA3::MapIds::DOOMLORE_SHRINE,
            "conset-restock-return-doomlore",
            120000u,
            5000u,
            kMaintenanceTravelRegion,
            kMaintenanceTravelDistrict,
            kMaintenanceTravelLanguage)) {
        LogBot("Rragars: failed returning to Doomlore after Embark conset restock");
        return false;
    }

    return stocked;
}

DungeonVendor::MaintenanceLocation MakeDoomloreMaintenanceLocation() {
    DungeonVendor::MaintenanceLocation location = {};
    location.outpost_map_id = GWA3::MapIds::DOOMLORE_SHRINE;

    auto* me = AgentMgr::GetMyAgent();
    location.merchant_x = me ? me->x : kQuestNpcX;
    location.merchant_y = me ? me->y : kQuestNpcY;
    location.merchant_move_threshold = 350.0f;
    location.merchant_search_radius = 60000.0f;
    location.merchant_player_number = kDoomloreMerchantPlayerNumber;

    float merchantX = 0.0f;
    float merchantY = 0.0f;
    const uint32_t merchantId =
        FindNpcByPlayerNumber(kDoomloreMerchantPlayerNumber, &merchantX, &merchantY);
    if (merchantId != 0u) {
        location.merchant_x = merchantX;
        location.merchant_y = merchantY;
        location.merchant_search_radius = 2500.0f;
        Log::Info("RragarsDbg: Doomlore merchant resolved agent=%u player=%u pos=(%.0f, %.0f)",
                  merchantId,
                  kDoomloreMerchantPlayerNumber,
                  merchantX,
                  merchantY);
    } else {
        Log::Info("RragarsDbg: Doomlore merchant player=%u not found; falling back to broad service scan",
                  kDoomloreMerchantPlayerNumber);
    }

    LogDoomloreServiceNpcCandidates("pre-maintenance");
    Log::Info("RragarsDbg: Doomlore Xunlai unresolved; numeric scan above is for safe service identification");
    return location;
}

MaintenanceMgr::Config MakeDoomloreMaintenanceConfig(const DungeonVendor::MaintenanceLocation& location) {
    auto config = DungeonVendor::BuildMaintenanceConfig(
        GWA3::MapIds::DOOMLORE_SHRINE,
        location);
    TuneRragarsMaintenanceConfig(config);
    return config;
}

bool RunDoomloreLocalMaintenanceIfNeeded() {
    if (MapMgr::GetMapId() != GWA3::MapIds::DOOMLORE_SHRINE) {
        return false;
    }

    Log::Info("RragarsDbg: Doomlore local maintenance disabled; Doomlore service scan finds collectors/quest NPCs, not safe merchant/Xunlai services");
    return false;
}

bool RunRragarsTownMaintenanceIfNeeded() {
    const auto config = MakeRragarsMaintenanceConfig();
    if (!MaintenanceMgr::NeedsMaintenance(config)) {
        return true;
    }

    if (MaintenanceMgr::NeedsCharacterConsetRestock(config)) {
        if (!RestockRragarsConsetsViaEmbark(config)) {
            return false;
        }
        if (!MaintenanceMgr::NeedsMaintenance(config)) {
            return true;
        }
    }

    if (RunDoomloreLocalMaintenanceIfNeeded()) {
        return true;
    }

    LogBot("Rragars: maintenance needed - traveling to Gadd's Encampment");
    if (MapMgr::GetMapId() != GWA3::MapIds::GADDS_ENCAMPMENT) {
        if (!TravelOutpostAndWait(GWA3::MapIds::GADDS_ENCAMPMENT, "maintenance-to-gadds")) {
            LogBot("Rragars: failed reaching Gadd's Encampment for maintenance");
            return false;
        }
    }

    const auto location = MakeRragarsMaintenanceLocation();
    if (DungeonVendor::OpenMaintenanceMerchantContext(
            location,
            &MoveMaintenancePoint,
            &WaitMaintenanceMs,
            "Rragars")) {
        MaintenanceMgr::PerformMaintenance(config);
        WaitMs(500u);
    } else {
        LogBot("Rragars: maintenance merchant failed to open; depositing excess gold only");
        MaintenanceMgr::DepositGold(config.depositKeepOnChar);
    }

    const uint32_t freeSlots = MaintenanceMgr::CountFreeSlots();
    const bool stillNeedsMaintenance = MaintenanceMgr::NeedsMaintenance(config);
    Log::Info("RragarsDbg: maintenance result freeSlots=%u minFreeSlots=%u stillNeeds=%d",
              freeSlots,
              config.minFreeSlots,
              stillNeedsMaintenance ? 1 : 0);

    if (freeSlots < 3u || stillNeedsMaintenance) {
        LogBot("Rragars: maintenance did not clear inventory enough; stopping before next dungeon run");
        return false;
    }

    LogBot("Rragars: returning to Doomlore Shrine after maintenance");
    return TravelOutpostAndWait(GWA3::MapIds::DOOMLORE_SHRINE, "maintenance-return-to-doomlore");
}

bool EnsureRragarsHardModeEnabled() {
    if (PartyMgr::GetIsHardMode()) {
        Log::Info("RragarsDbg: Hard Mode already enabled; skipping SetHardMode");
        return true;
    }

    LogBot("Rragars: enabling Hard Mode for Doomlore setup");
    MapMgr::SetHardMode(true);
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < 5000u) {
        if (PartyMgr::GetIsHardMode()) {
            return true;
        }
        WaitMs(250u);
    }

    LogBot("Rragars: Hard Mode was not confirmed after setup request");
    return false;
}

bool EnsureRragarsOutpostParty(BotConfig& cfg) {
    if (PartyMgr::CountPartyHeroes() >= 7u) {
        LogBot("Rragars: preserving existing full hero party for Doomlore setup");
        return EnsureRragarsHardModeEnabled();
    }

    LogBot("Rragars: applying Doomlore outpost setup heroes=%u", PartyMgr::CountPartyHeroes());
    DungeonOutpostSetup::Options options = {};
    options.default_hero_config_file = "Standard.txt";
    cfg.hero_config_file = "Standard.txt";
    if (!DungeonOutpostSetup::ApplyOutpostSetup(cfg, options)) {
        LogBot("Rragars: Doomlore outpost setup failed");
        return false;
    }

    const uint32_t heroes = PartyMgr::CountPartyHeroes();
    if (heroes < 7u) {
        LogBot("Rragars: refusing to leave Doomlore without full hero party heroes=%u", heroes);
        return false;
    }
    return EnsureRragarsHardModeEnabled();
}

BotState HandleCharSelect(BotConfig&) {
    return MapMgr::GetMapId() == 0u ? BotState::CharSelect : BotState::InTown;
}

bool ExecuteRewardChestFlow() {
    const RewardChestObjective reward = GetRewardChestObjective();
    Log::Info("RragarsDbg: reward flow begin staging=(%.0f, %.0f) chest=(%.0f, %.0f)",
              reward.staging_x,
              reward.staging_y,
              reward.chest_x,
              reward.chest_y);
    const DWORD chestSearchStart = GetTickCount();
    while ((GetTickCount() - chestSearchStart) < 300000u) {
        if (DungeonInteractions::FindNearestSignpost(reward.chest_x, reward.chest_y, 3000.0f) != 0u) {
            break;
        }
        if (!MoveRewardPointWithRecovery("staging", reward.staging_x, reward.staging_y, 500.0f, 180000u)) {
            if (DungeonInteractions::FindNearestSignpost(reward.chest_x, reward.chest_y, 3000.0f) != 0u) {
                Log::Info("RragarsDbg: reward chest appeared after failed staging move; continuing");
                break;
            }
            auto* me = AgentMgr::GetMyAgent();
            Log::Info("RragarsDbg: reward staging move failed player=(%.0f, %.0f) hp=%.2f remaining=%.0f",
                      me ? me->x : 0.0f,
                      me ? me->y : 0.0f,
                      me ? me->hp : 0.0f,
                      DungeonCombat::DistanceToPoint(reward.staging_x, reward.staging_y));
            LogBot("Rragars: failed moving to reward staging point");
            return false;
        }
        if (!ClearRragarsLocalArea("reward-staging", 2200.0f, 180000u)) {
            if (IsPlayerOrPartyDead()) {
                (void)WaitForPartyRecovery(GWA3::MapIds::RRAGARS_MENAGERIE_LVL3, 180000u);
                continue;
            }
        }
        Log::Info("RragarsDbg: reward staging reached signpost=%u",
                  DungeonInteractions::FindNearestSignpost(reward.chest_x, reward.chest_y, 3000.0f));
        WaitMs(1000u);
    }

    const uint32_t chestId = DungeonInteractions::FindNearestSignpost(reward.chest_x, reward.chest_y, 3000.0f);
    if (chestId == 0u) {
        LogBot("Rragars: reward chest signpost never appeared near (%.0f, %.0f)",
               reward.chest_x, reward.chest_y);
        return false;
    }
    Log::Info("RragarsDbg: reward chest signpost found id=%u", chestId);

    if (!MoveRewardPointWithRecovery("chest", reward.chest_x, reward.chest_y, 300.0f, 90000u)) {
        auto* me = AgentMgr::GetMyAgent();
        Log::Info("RragarsDbg: reward chest move failed player=(%.0f, %.0f) hp=%.2f remaining=%.0f",
                  me ? me->x : 0.0f,
                  me ? me->y : 0.0f,
                  me ? me->hp : 0.0f,
                  DungeonCombat::DistanceToPoint(reward.chest_x, reward.chest_y));
        LogBot("Rragars: failed reaching reward chest");
        return false;
    }

    for (int i = 0; i < reward.interact_repeats; ++i) {
        Log::Info("RragarsDbg: reward chest interact attempt=%d signpost=%u", i + 1, chestId);
        AgentMgr::InteractSignpost(chestId);
        WaitMs(5000u);
        DungeonBundle::PickUpNearestItemNearPoint(
            reward.chest_x,
            reward.chest_y,
            5000.0f,
            reward.pickup_attempts,
            500u);
    }

    if (MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3) {
        LogBot("Rragars: returning to Doomlore Shrine after reward chest");
        if (!TravelOutpostAndWait(GWA3::MapIds::DOOMLORE_SHRINE, "reward-return-to-doomlore", 180000u)) {
            LogBot("Rragars: failed returning to Doomlore Shrine after reward chest");
            return false;
        }
        WaitMs(3000u);
    }

    return MapMgr::GetMapId() == GWA3::MapIds::DOOMLORE_SHRINE;
}

bool ExecuteSimpleRoute(RouteId routeId, bool waitForTransition = true) {
    const RouteDefinition& route = GetRouteDefinition(routeId);
    auto* me = AgentMgr::GetMyAgent();
    if (!me) {
        LogBot("Rragars: no player agent available for route %s", route.name);
        return false;
    }

    const int startIndex = DungeonRoute::FindNearestWaypointIndex(
        route.waypoints,
        route.waypoint_count,
        me->x,
        me->y);
    LogBot("Rragars: route %s start index %d player=(%.0f, %.0f)",
           route.name,
           startIndex,
           me->x,
           me->y);
    Log::Info("RragarsDbg: route-start route=%s startIndex=%d map=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d",
              route.name,
              startIndex,
              MapMgr::GetMapId(),
              me->x,
              me->y,
              me->hp,
              PartyMgr::GetIsPartyDefeated() ? 1 : 0);

    if (const BlessingAnchor* blessing = FindBlessingAnchor(routeId, startIndex)) {
        LogBot("Rragars: blessing anchor at route %s index %d -> (%.0f, %.0f)",
               route.name, startIndex, blessing->x, blessing->y);
        DungeonNavigation::MoveToAndWait(blessing->x, blessing->y, 300.0f, 15000u, 1000u, route.map_id);
    }

    if (IsExplorableTravelRoute(routeId)) {
        if (!FollowTravelRouteWithRetries(routeId)) {
            return false;
        }
        if (!waitForTransition) {
            return true;
        }
        const ZoneTransitionPoint* transition = FindZoneTransitionPoint(routeId);
        const auto* fallbackPoint = route.waypoint_count > 0 ? &route.waypoints[route.waypoint_count - 1] : nullptr;
        const float zoneX = transition ? transition->x : (fallbackPoint ? fallbackPoint->x : 0.0f);
        const float zoneY = transition ? transition->y : (fallbackPoint ? fallbackPoint->y : 0.0f);
        if ((!transition && !fallbackPoint) ||
            !ZoneThroughPoint(zoneX, zoneY, route.next_map_id)) {
            return false;
        }
        if (!WaitForMapReady(route.next_map_id, 10000u)) {
            return false;
        }

        if (transition &&
            !WaitForSpawnAwayFromPoint(route.next_map_id, transition->x, transition->y, 3000.0f, 10000u)) {
            LogBot("Rragars: spawn on map %u never moved away from transition point (%.0f, %.0f)",
                   route.next_map_id,
                   transition->x,
                   transition->y);
            return false;
        }
        return true;
    }

    int recoveryRetries = 0;
    int totalRecoveryRetries = 0;
    int checkpointFailureIndex = -1;
    int checkpointFailureAttempts = 0;
    int dungeonKeyPickupMisses = 0;
    int level2DoorCorridorRecoveries = 0;
    bool dungeonKeyPickedThisRoute = false;
    bool assumeKegHeldAfterInteraction = false;
    constexpr int kMaxRecoveryRetries = 6;
    constexpr int kMaxTotalRecoveryRetries = 24;
    constexpr int kMaxCheckpointFailureAttempts = 5;
    constexpr int kMaxDungeonKeyPickupMisses = 2;
    constexpr int kMaxLevel2DoorCorridorRecoveries = 2;
    const auto computeRecoveryRetryIndex = [&](int failedWaypoint, int nearestAfterRez, WaypointBehavior failedBehavior) {
        int retryIndex = nearestAfterRez;
        if (retryIndex < 0) {
            retryIndex = failedWaypoint;
        }
        if (retryIndex > failedWaypoint) {
            if (failedBehavior != WaypointBehavior::StandardMove) {
                return failedWaypoint;
            }
            const int maxForwardIndex = retryIndex < route.waypoint_count ? retryIndex : route.waypoint_count - 1;
            for (int forwardIndex = failedWaypoint + 1; forwardIndex <= maxForwardIndex; ++forwardIndex) {
                const WaypointExecutionPlan forwardPlan = BuildWaypointExecutionPlan(routeId, forwardIndex);
                if (forwardPlan.behavior != WaypointBehavior::StandardMove) {
                    return failedWaypoint;
                }
            }
        }
        if (retryIndex >= route.waypoint_count) {
            retryIndex = route.waypoint_count - 1;
        }
        return retryIndex;
    };
    const auto findPreviousKegIndex = [&](int failedWaypoint) {
        for (int backtrackIndex = failedWaypoint - 1; backtrackIndex >= 0; --backtrackIndex) {
            const WaypointExecutionPlan backtrackPlan = BuildWaypointExecutionPlan(routeId, backtrackIndex);
            if (backtrackPlan.behavior == WaypointBehavior::PickUpKeg) {
                return backtrackIndex;
            }
        }
        return failedWaypoint;
    };
    for (int i = startIndex; i < route.waypoint_count; ++i) {
        const WaypointExecutionPlan plan = BuildWaypointExecutionPlan(routeId, i);
        if (!plan.waypoint) {
            return false;
        }

        switch (plan.behavior) {
        case WaypointBehavior::StandardMove:
            Log::Info("RragarsDbg: standard-move begin route=%s waypoint=%d label=%s target=(%.0f, %.0f) map=%u",
                      route.name,
                      i,
                      plan.waypoint->label,
                      plan.waypoint->x,
                      plan.waypoint->y,
                      route.map_id);
            if (!MoveToWaypoint(*plan.waypoint, route.map_id)) {
                auto* after = AgentMgr::GetMyAgent();
                Log::Info("RragarsDbg: standard-move failed route=%s waypoint=%d label=%s currentMap=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d remaining=%.0f",
                          route.name,
                          i,
                          plan.waypoint->label,
                          MapMgr::GetMapId(),
                          after ? after->x : 0.0f,
                          after ? after->y : 0.0f,
                          after ? after->hp : 0.0f,
                          PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                          DungeonCombat::DistanceToPoint(plan.waypoint->x, plan.waypoint->y));
                if (after && after->hp <= 0.0f && recoveryRetries < kMaxRecoveryRetries &&
                    totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                    WaitForPartyRecovery(route.map_id)) {
                    ++recoveryRetries;
                    ++totalRecoveryRetries;
                    RecordRragarsWipeAndRecoverConsumables("standard-move");
                    auto* recovered = AgentMgr::GetMyAgent();
                    const int nearestAfterRez = recovered
                        ? DungeonRoute::FindNearestWaypointIndex(
                              route.waypoints,
                              route.waypoint_count,
                              recovered->x,
                              recovered->y)
                        : i;
                    const int retryIndex = computeRecoveryRetryIndex(i, nearestAfterRez, plan.behavior);
                    Log::Info("RragarsDbg: standard-move recovery retry route=%s failedWaypoint=%d nearest=%d retryIndex=%d retry=%d total=%d",
                              route.name,
                              i,
                              nearestAfterRez,
                              retryIndex,
                              recoveryRetries,
                              totalRecoveryRetries);
                    i = retryIndex - 1;
                    continue;
                }
                if (routeId == RouteId::Level2 &&
                    i == 19 &&
                    after &&
                    after->hp > 0.0f &&
                    MapMgr::GetMapId() == route.map_id &&
                    MapMgr::GetIsMapLoaded() &&
                    level2DoorCorridorRecoveries < kMaxLevel2DoorCorridorRecoveries) {
                    ++level2DoorCorridorRecoveries;
                    Log::Info("RragarsDbg: level2 door-corridor alive timeout recovery route=%s failedWaypoint=%d retryStart=16 attempt=%d max=%d",
                              route.name,
                              i,
                              level2DoorCorridorRecoveries,
                              kMaxLevel2DoorCorridorRecoveries);
                    (void)ClearRragarsLocalArea("level2-door-corridor-recovery", 2400.0f, 180000u);
                    i = 16 - 1;
                    continue;
                }
                if (after && after->hp <= 0.0f) {
                    Log::Info("RragarsDbg: standard-move recovery skipped route=%s waypoint=%d retry=%d total=%d maxRetry=%d maxTotal=%d",
                              route.name,
                              i,
                              recoveryRetries,
                              totalRecoveryRetries,
                              kMaxRecoveryRetries,
                              kMaxTotalRecoveryRetries);
                }
                LogBot("Rragars: failed moving to waypoint %d (%s) on %s",
                       i, plan.waypoint->label, route.name);
                return false;
            }
            recoveryRetries = 0;
            Log::Info("RragarsDbg: standard-move arrived route=%s waypoint=%d label=%s remaining=%.0f",
                      route.name,
                      i,
                      plan.waypoint->label,
                      DungeonCombat::DistanceToPoint(plan.waypoint->x, plan.waypoint->y));
            break;
        case WaypointBehavior::PickUpKeg: {
            assumeKegHeldAfterInteraction = false;
            Log::Info("RragarsDbg: keg step begin route=%s waypoint=%d label=%s heldBundle=%u",
                      route.name,
                      i,
                      plan.waypoint->label,
                      DungeonInteractions::GetHeldBundleItemId());
            if (!MoveToWaypoint(*plan.waypoint, route.map_id, 250.0f)) {
                auto* after = AgentMgr::GetMyAgent();
                Log::Info("RragarsDbg: keg move failed route=%s waypoint=%d label=%s currentMap=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d remaining=%.0f",
                          route.name,
                          i,
                          plan.waypoint->label,
                          MapMgr::GetMapId(),
                          after ? after->x : 0.0f,
                          after ? after->y : 0.0f,
                          after ? after->hp : 0.0f,
                          PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                          DungeonCombat::DistanceToPoint(plan.waypoint->x, plan.waypoint->y));
                if (after && after->hp <= 0.0f && recoveryRetries < kMaxRecoveryRetries &&
                    totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                    WaitForPartyRecovery(route.map_id)) {
                    ++recoveryRetries;
                    ++totalRecoveryRetries;
                    RecordRragarsWipeAndRecoverConsumables("keg-step");
                    Log::Info("RragarsDbg: keg recovery retry route=%s failedWaypoint=%d retry=%d total=%d",
                              route.name,
                              i,
                              recoveryRetries,
                              totalRecoveryRetries);
                    --i;
                    continue;
                }
                if (after && after->hp <= 0.0f) {
                    Log::Info("RragarsDbg: keg recovery skipped route=%s waypoint=%d retry=%d total=%d maxRetry=%d maxTotal=%d",
                              route.name,
                              i,
                              recoveryRetries,
                              totalRecoveryRetries,
                              kMaxRecoveryRetries,
                              kMaxTotalRecoveryRetries);
                }
                return false;
            }
            recoveryRetries = 0;
            Log::Info("RragarsDbg: keg step arrived route=%s waypoint=%d heldBundle=%u",
                      route.name,
                      i,
                      DungeonInteractions::GetHeldBundleItemId());
            if (routeId == RouteId::Level1 && i == 26) {
                const bool clearedSecondKeg = ClearRragarsLocalArea("level1-second-keg-prepickup", 2200.0f, 240000u);
                auto* afterSecondKegClear = AgentMgr::GetMyAgent();
                const bool diedSecondKegClear = afterSecondKegClear == nullptr ||
                    afterSecondKegClear->hp <= 0.0f ||
                    PartyMgr::GetIsPartyDefeated();
                Log::Info("RragarsDbg: second-keg prep clear route=%s waypoint=%d cleared=%d player=(%.0f, %.0f) hp=%.2f partyDefeated=%d",
                          route.name,
                          i,
                          clearedSecondKeg ? 1 : 0,
                          afterSecondKegClear ? afterSecondKegClear->x : 0.0f,
                          afterSecondKegClear ? afterSecondKegClear->y : 0.0f,
                          afterSecondKegClear ? afterSecondKegClear->hp : 0.0f,
                          PartyMgr::GetIsPartyDefeated() ? 1 : 0);
                if (diedSecondKegClear) {
                    if (recoveryRetries < kMaxRecoveryRetries &&
                        totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                        WaitForPartyRecovery(route.map_id, 180000u)) {
                        ++recoveryRetries;
                        ++totalRecoveryRetries;
                        RecordRragarsWipeAndRecoverConsumables("second-keg-prep");
                        Log::Info("RragarsDbg: second-keg prep recovery route=%s waypoint=%d retry=20 recovery=%d total=%d",
                                  route.name,
                                  i,
                                  recoveryRetries,
                                  totalRecoveryRetries);
                        i = 19;
                        continue;
                    }
                    Log::Info("RragarsDbg: second-keg prep recovery failed route=%s waypoint=%d recovery=%d total=%d",
                              route.name,
                              i,
                              recoveryRetries,
                              totalRecoveryRetries);
                    return false;
                }
            }
            LogPlayerBundleDiagnostics("keg-before");
            bool acquired = TryPlainSignpostRunForRragarsKeg(
                plan.waypoint->x,
                plan.waypoint->y,
                1500.0f,
                2,
                1500u);
            Log::Info("RragarsDbg: keg plain-signpost result route=%s waypoint=%d acquired=%d heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d",
                      route.name,
                      i,
                      acquired ? 1 : 0,
                      DungeonInteractions::GetHeldBundleItemId(),
                      SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                      HasRragarsPowderKegRelatedEffect() ? 1 : 0);
            LogPlayerBundleDiagnostics("keg-after-plain-signpost");
            if (!acquired) {
                acquired = DungeonBundle::AutoItGoToSignpostAndAcquireHeldBundleNearPoint(
                    plan.waypoint->x,
                    plan.waypoint->y,
                    1500.0f,
                    2,
                    100u,
                    1000u);
                acquired = acquired || HasRragarsPowderKegHeldSignal();
            }
            Log::Info("RragarsDbg: keg legacy interact result route=%s waypoint=%d acquired=%d heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d",
                      route.name,
                      i,
                      acquired ? 1 : 0,
                      DungeonInteractions::GetHeldBundleItemId(),
                      SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                      HasRragarsPowderKegRelatedEffect() ? 1 : 0);
            LogPlayerBundleDiagnostics("keg-after-legacy");
            if (!acquired) {
                acquired = DungeonBundle::InteractSignpostAndAcquireHeldBundleNearPoint(
                    plan.waypoint->x,
                    plan.waypoint->y,
                    1500.0f,
                    2,
                    100u,
                    2000u);
                acquired = acquired || HasRragarsPowderKegHeldSignal();
            }
            Log::Info("RragarsDbg: keg interact result route=%s waypoint=%d acquired=%d heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d",
                      route.name,
                      i,
                      acquired ? 1 : 0,
                      DungeonInteractions::GetHeldBundleItemId(),
                      SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                      HasRragarsPowderKegRelatedEffect() ? 1 : 0);
            LogPlayerBundleDiagnostics("keg-after-interact");
            if (!acquired) {
                acquired = TryActionInteractForRragarsKeg(
                    plan.waypoint->x,
                    plan.waypoint->y,
                    1500.0f,
                    3,
                    500u);
                acquired = acquired || HasRragarsPowderKegHeldSignal();
            }
            Log::Info("RragarsDbg: keg action-interact result route=%s waypoint=%d acquired=%d heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d",
                      route.name,
                      i,
                      acquired ? 1 : 0,
                      DungeonInteractions::GetHeldBundleItemId(),
                      SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                      HasRragarsPowderKegRelatedEffect() ? 1 : 0);
            if (!acquired) {
                const bool chestPicked = DungeonBundle::OpenChestAndPickUpBundle(
                    plan.waypoint->x,
                    plan.waypoint->y,
                    1500.0f,
                    1500.0f,
                    2,
                    2,
                    500u,
                    500u);
                acquired = HasRragarsPowderKegHeldSignal();
                Log::Info("RragarsDbg: keg chest-open fallback route=%s waypoint=%d picked=%d acquired=%d heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d",
                          route.name,
                          i,
                          chestPicked ? 1 : 0,
                          acquired ? 1 : 0,
                          DungeonInteractions::GetHeldBundleItemId(),
                          SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                          HasRragarsPowderKegRelatedEffect() ? 1 : 0);
                LogPlayerBundleDiagnostics("keg-after-chest-fallback");
            }
            if (!acquired) {
                const bool pickedGround = DungeonBundle::PickUpNearestItemNearPoint(
                    plan.waypoint->x,
                    plan.waypoint->y,
                    1500.0f,
                    2,
                    500u);
                acquired = HasRragarsPowderKegHeldSignal();
                Log::Info("RragarsDbg: keg ground-pickup fallback route=%s waypoint=%d picked=%d acquired=%d heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d",
                          route.name,
                          i,
                          pickedGround ? 1 : 0,
                          acquired ? 1 : 0,
                          DungeonInteractions::GetHeldBundleItemId(),
                          SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                          HasRragarsPowderKegRelatedEffect() ? 1 : 0);
                LogPlayerBundleDiagnostics("keg-after-ground-fallback");
            }
            if (!acquired) {
                assumeKegHeldAfterInteraction = true;
                LogBot("Rragars: keg pickup near waypoint %d on %s did not expose a held bundle; continuing to blast-door checkpoint",
                       i,
                       route.name);
            }
            WaitMs(500);
            break;
        }
        case WaypointBehavior::DropKegAtBlastDoor: {
            const bool hadKegBeforeMove = HasRragarsPowderKegHeldSignal();
            Log::Info("RragarsDbg: blast-door step begin route=%s waypoint=%d label=%s heldBundle=%u",
                      route.name,
                      i,
                      plan.waypoint->label,
                      DungeonInteractions::GetHeldBundleItemId());
            LogPlayerBundleDiagnostics("blast-door-before");
            if (!MoveToWaypoint(*plan.waypoint, route.map_id, 250.0f)) {
                auto* after = AgentMgr::GetMyAgent();
                Log::Info("RragarsDbg: blast-door move failed route=%s waypoint=%d label=%s currentMap=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d remaining=%.0f",
                          route.name,
                          i,
                          plan.waypoint->label,
                          MapMgr::GetMapId(),
                          after ? after->x : 0.0f,
                          after ? after->y : 0.0f,
                          after ? after->hp : 0.0f,
                          PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                          DungeonCombat::DistanceToPoint(plan.waypoint->x, plan.waypoint->y));
                if (after && after->hp <= 0.0f && recoveryRetries < kMaxRecoveryRetries &&
                    totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                    WaitForPartyRecovery(route.map_id)) {
                    ++recoveryRetries;
                    ++totalRecoveryRetries;
                    RecordRragarsWipeAndRecoverConsumables("blast-door");
                    const int retryIndex = findPreviousKegIndex(i);
                    assumeKegHeldAfterInteraction = false;
                    Log::Info("RragarsDbg: blast-door recovery retry route=%s failedWaypoint=%d retryIndex=%d retry=%d total=%d",
                              route.name,
                              i,
                              retryIndex,
                              recoveryRetries,
                              totalRecoveryRetries);
                    i = retryIndex - 1;
                    continue;
                }
                if (after && after->hp <= 0.0f) {
                    Log::Info("RragarsDbg: blast-door recovery skipped route=%s waypoint=%d retry=%d total=%d maxRetry=%d maxTotal=%d",
                              route.name,
                              i,
                              recoveryRetries,
                              totalRecoveryRetries,
                              kMaxRecoveryRetries,
                              kMaxTotalRecoveryRetries);
                }
                return false;
            }
            recoveryRetries = 0;
            Log::Info("RragarsDbg: blast-door arrived route=%s waypoint=%d heldBundle=%u",
                      route.name,
                      i,
                      DungeonInteractions::GetHeldBundleItemId());
            const bool assumeKegHeld = hadKegBeforeMove || HasRragarsPowderKegHeldSignal() || assumeKegHeldAfterInteraction;
            const bool droppedOnce = DropRragarsPowderKeg(assumeKegHeld);
            assumeKegHeldAfterInteraction = false;
            Log::Info("RragarsDbg: blast-door first-drop route=%s waypoint=%d dropped=%d heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d assumeHeld=%d",
                      route.name,
                      i,
                      droppedOnce ? 1 : 0,
                      DungeonInteractions::GetHeldBundleItemId(),
                      SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                      HasRragarsPowderKegRelatedEffect() ? 1 : 0,
                      assumeKegHeld ? 1 : 0);
            if (!droppedOnce) {
                LogBot("Rragars: blast door waypoint %d on %s reached without a held bundle; deferring to checkpoint retry",
                       i,
                       route.name);
            }
            WaitMs(500);
            if (droppedOnce || HasRragarsPowderKegHeldSignal()) {
                const bool droppedTwice = DropRragarsPowderKeg(true);
                Log::Info("RragarsDbg: blast-door second-drop route=%s waypoint=%d dropped=%d heldBundle=%u skillbarKeg=%d kegRelatedEffect=%d",
                          route.name,
                          i,
                          droppedTwice ? 1 : 0,
                          DungeonInteractions::GetHeldBundleItemId(),
                          SkillbarContainsSkill(GWA3::SkillIds::DWARVEN_POWDER_KEG) ? 1 : 0,
                          HasRragarsPowderKegRelatedEffect() ? 1 : 0);
            } else {
                Log::Info("RragarsDbg: blast-door second-drop skipped route=%s waypoint=%d heldBundle=%u",
                          route.name,
                          i,
                          DungeonInteractions::GetHeldBundleItemId());
            }
            WaitMs(4000);
            if (routeId == RouteId::Level3) {
                (void)WaitForPartyRecovery(route.map_id, 30000u);
                (void)ClearRragarsLocalArea("level3-post-blast-door", 1800.0f, 180000u);
            }
            if (routeId == RouteId::Level1 && i == 30) {
                (void)WaitForPartyRecovery(route.map_id, 30000u);
                const bool clearedSecondDoor = ClearRragarsLocalArea("level1-post-second-blast-door", 2600.0f, 240000u);
                auto* afterSecondDoorClear = AgentMgr::GetMyAgent();
                const bool diedSecondDoorClear = afterSecondDoorClear == nullptr ||
                    afterSecondDoorClear->hp <= 0.0f ||
                    PartyMgr::GetIsPartyDefeated();
                Log::Info("RragarsDbg: second-door post-clear route=%s waypoint=%d cleared=%d player=(%.0f, %.0f) hp=%.2f partyDefeated=%d",
                          route.name,
                          i,
                          clearedSecondDoor ? 1 : 0,
                          afterSecondDoorClear ? afterSecondDoorClear->x : 0.0f,
                          afterSecondDoorClear ? afterSecondDoorClear->y : 0.0f,
                          afterSecondDoorClear ? afterSecondDoorClear->hp : 0.0f,
                          PartyMgr::GetIsPartyDefeated() ? 1 : 0);
                if (diedSecondDoorClear) {
                    if (recoveryRetries < kMaxRecoveryRetries &&
                        totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                        WaitForPartyRecovery(route.map_id, 180000u)) {
                        ++recoveryRetries;
                        ++totalRecoveryRetries;
                        RecordRragarsWipeAndRecoverConsumables("second-door-post-clear");
                        const int retryIndex = findPreviousKegIndex(i);
                        assumeKegHeldAfterInteraction = false;
                        Log::Info("RragarsDbg: second-door post-clear recovery route=%s waypoint=%d retryIndex=%d recovery=%d total=%d",
                                  route.name,
                                  i,
                                  retryIndex,
                                  recoveryRetries,
                                  totalRecoveryRetries);
                        i = retryIndex - 1;
                        continue;
                    }
                    Log::Info("RragarsDbg: second-door post-clear recovery failed route=%s waypoint=%d recovery=%d total=%d",
                              route.name,
                              i,
                              recoveryRetries,
                              totalRecoveryRetries);
                    return false;
                }
            }
            break;
        }
        case WaypointBehavior::PickUpDungeonKey: {
            if (!MoveToWaypoint(*plan.waypoint, route.map_id)) {
                auto* after = AgentMgr::GetMyAgent();
                Log::Info("RragarsDbg: dungeon-key move failed route=%s waypoint=%d label=%s currentMap=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d remaining=%.0f",
                          route.name,
                          i,
                          plan.waypoint->label,
                          MapMgr::GetMapId(),
                          after ? after->x : 0.0f,
                          after ? after->y : 0.0f,
                          after ? after->hp : 0.0f,
                          PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                          DungeonCombat::DistanceToPoint(plan.waypoint->x, plan.waypoint->y));
                if (after && after->hp <= 0.0f && recoveryRetries < kMaxRecoveryRetries &&
                    totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                    WaitForPartyRecovery(route.map_id)) {
                    ++recoveryRetries;
                    ++totalRecoveryRetries;
                    RecordRragarsWipeAndRecoverConsumables("dungeon-key");
                    auto* recovered = AgentMgr::GetMyAgent();
                    const int nearestAfterRez = recovered
                        ? DungeonRoute::FindNearestWaypointIndex(
                              route.waypoints,
                              route.waypoint_count,
                              recovered->x,
                              recovered->y)
                        : i;
                    const int retryIndex = computeRecoveryRetryIndex(i, nearestAfterRez, plan.behavior);
                    Log::Info("RragarsDbg: dungeon-key recovery retry route=%s failedWaypoint=%d nearest=%d retryIndex=%d retry=%d total=%d",
                              route.name,
                              i,
                              nearestAfterRez,
                              retryIndex,
                              recoveryRetries,
                              totalRecoveryRetries);
                    i = retryIndex - 1;
                    continue;
                }
                if (after && after->hp <= 0.0f) {
                    Log::Info("RragarsDbg: dungeon-key recovery skipped route=%s waypoint=%d retry=%d total=%d maxRetry=%d maxTotal=%d",
                              route.name,
                              i,
                              recoveryRetries,
                              totalRecoveryRetries,
                              kMaxRecoveryRetries,
                              kMaxTotalRecoveryRetries);
                }
                return false;
            }
            recoveryRetries = 0;
            const LootObjective* loot = FindLootObjective(routeId);
            const float pickupX = loot ? loot->pickup_x : plan.waypoint->x;
            const float pickupY = loot ? loot->pickup_y : plan.waypoint->y;
            const int pickupRetries = loot ? loot->pickup_retries : 1;
            bool pickedDungeonKey = DungeonBundle::PickUpNearestItemNearPoint(
                    pickupX,
                    pickupY,
                    1200.0f,
                    pickupRetries,
                    500u);
            if (!pickedDungeonKey) {
                (void)ClearRragarsLocalArea("dungeon-key-pickup-retry", 2200.0f, 90000u);
                pickedDungeonKey = DungeonBundle::PickUpNearestItemNearPoint(
                    pickupX,
                    pickupY,
                    2200.0f,
                    pickupRetries + 2,
                    500u);
            }
            if (pickedDungeonKey) {
                dungeonKeyPickedThisRoute = true;
                dungeonKeyPickupMisses = 0;
                Log::Info("RragarsDbg: dungeon-key pickup success route=%s waypoint=%d center=(%.0f, %.0f)",
                          route.name,
                          i,
                          pickupX,
                          pickupY);
            } else if (dungeonKeyPickedThisRoute) {
                Log::Info("RragarsDbg: dungeon-key pickup skipped route=%s waypoint=%d noItemAfterPriorPickup=1 center=(%.0f, %.0f)",
                          route.name,
                          i,
                          pickupX,
                          pickupY);
            } else {
                ++dungeonKeyPickupMisses;
                auto* keyMissPlayer = AgentMgr::GetMyAgent();
                LogBot("Rragars: no nearby dungeon key item found near waypoint %d on %s",
                       i, route.name);
                Log::Info("RragarsDbg: dungeon-key pickup miss route=%s waypoint=%d miss=%d max=%d player=(%.0f, %.0f) center=(%.0f, %.0f)",
                          route.name,
                          i,
                          dungeonKeyPickupMisses,
                          kMaxDungeonKeyPickupMisses,
                          keyMissPlayer ? keyMissPlayer->x : 0.0f,
                          keyMissPlayer ? keyMissPlayer->y : 0.0f,
                          pickupX,
                          pickupY);
                if (dungeonKeyPickupMisses <= kMaxDungeonKeyPickupMisses) {
                    const int retryIndex = i >= 2 ? i - 2 : i;
                    Log::Info("RragarsDbg: dungeon-key pickup backtrack route=%s waypoint=%d retryIndex=%d miss=%d",
                              route.name,
                              i,
                              retryIndex,
                              dungeonKeyPickupMisses);
                    i = retryIndex - 1;
                    continue;
                }
                return false;
            }
            break;
        }
        case WaypointBehavior::ValidateQuestCheckpoint:
        case WaypointBehavior::ValidateRetryCheckpoint: {
            Log::Info("RragarsDbg: checkpoint step begin route=%s waypoint=%d label=%s",
                      route.name,
                      i,
                      plan.waypoint->label);
            const uint32_t checkpointTimeout = routeId == RouteId::Level1 && i == 31
                ? 240000u
                : 90000u;
            const bool checkpointArrived = MoveToWaypoint(*plan.waypoint, route.map_id, 250.0f, checkpointTimeout);
            if (!checkpointArrived) {
                auto* meAfterCheckpointMove = AgentMgr::GetMyAgent();
                const bool checkpointMoveDeath = meAfterCheckpointMove == nullptr ||
                    meAfterCheckpointMove->hp <= 0.0f ||
                    PartyMgr::GetIsPartyDefeated();
                Log::Info("RragarsDbg: checkpoint move did not arrive route=%s waypoint=%d label=%s player=(%.0f, %.0f) remaining=%.0f",
                          route.name,
                          i,
                          plan.waypoint->label,
                          meAfterCheckpointMove ? meAfterCheckpointMove->x : 0.0f,
                          meAfterCheckpointMove ? meAfterCheckpointMove->y : 0.0f,
                          DungeonCombat::DistanceToPoint(plan.waypoint->x, plan.waypoint->y));
                if (checkpointMoveDeath &&
                    recoveryRetries < kMaxRecoveryRetries &&
                    totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                    WaitForPartyRecovery(route.map_id)) {
                    ++recoveryRetries;
                    ++totalRecoveryRetries;
                    RecordRragarsWipeAndRecoverConsumables("checkpoint");
                    auto* recovered = AgentMgr::GetMyAgent();
                    int retryIndex = recovered
                        ? DungeonRoute::FindNearestWaypointIndex(
                              route.waypoints,
                              route.waypoint_count,
                              recovered->x,
                              recovered->y)
                        : i;
                    if (retryIndex < 0) {
                        retryIndex = i;
                    }
                    if (retryIndex > i) {
                        retryIndex = i;
                    }
                    Log::Info("RragarsDbg: checkpoint recovery route=%s waypoint=%d label=%s retryIndex=%d retry=%d total=%d",
                              route.name,
                              i,
                              plan.waypoint->label,
                              retryIndex,
                              recoveryRetries,
                              totalRecoveryRetries);
                    i = retryIndex - 1;
                    continue;
                }
                if (checkpointMoveDeath) {
                    Log::Info("RragarsDbg: checkpoint recovery failed route=%s waypoint=%d label=%s retry=%d total=%d player=(%.0f, %.0f) hp=%.2f partyDefeated=%d",
                              route.name,
                              i,
                              plan.waypoint->label,
                              recoveryRetries,
                              totalRecoveryRetries,
                              meAfterCheckpointMove ? meAfterCheckpointMove->x : 0.0f,
                              meAfterCheckpointMove ? meAfterCheckpointMove->y : 0.0f,
                              meAfterCheckpointMove ? meAfterCheckpointMove->hp : 0.0f,
                              PartyMgr::GetIsPartyDefeated() ? 1 : 0);
                    return false;
                }
            }

            const int nearestWaypoint = GetNearestWaypointIndexForRoute(route);
            Log::Info("RragarsDbg: checkpoint nearest route=%s waypoint=%d label=%s nearest=%d",
                      route.name,
                      i,
                      plan.waypoint->label,
                      nearestWaypoint);
            if (nearestWaypoint < 0) {
                LogBot("Rragars: failed to resolve nearest waypoint after checkpoint %d on %s", i, route.name);
                return false;
            }

            const auto resolution = EvaluateRragarsCheckpointResolution(
                plan.behavior,
                i,
                nearestWaypoint,
                route.waypoint_count,
                plan.checkpoint_policy);
            Log::Info("RragarsDbg: checkpoint resolution route=%s waypoint=%d label=%s passed=%d action=%d backtrackStart=%d retryIndex=%d",
                      route.name,
                      i,
                      plan.waypoint->label,
                      resolution.passed ? 1 : 0,
                      static_cast<int>(resolution.action),
                      resolution.backtrack_start,
                      resolution.retry_index);
            if (resolution.passed) {
                checkpointFailureIndex = -1;
                checkpointFailureAttempts = 0;
                if (routeId == RouteId::Level1 && i == 31) {
                    const bool clearedPostDoor = ClearRragarsLocalArea("level1-post-blast-door-2", 2200.0f, 240000u);
                    auto* afterPostDoorClear = AgentMgr::GetMyAgent();
                    const bool diedPostDoorClear = afterPostDoorClear == nullptr ||
                        afterPostDoorClear->hp <= 0.0f ||
                        PartyMgr::GetIsPartyDefeated();
                    Log::Info("RragarsDbg: post-checkpoint clear route=%s waypoint=%d cleared=%d player=(%.0f, %.0f) hp=%.2f partyDefeated=%d",
                              route.name,
                              i,
                              clearedPostDoor ? 1 : 0,
                              afterPostDoorClear ? afterPostDoorClear->x : 0.0f,
                              afterPostDoorClear ? afterPostDoorClear->y : 0.0f,
                              afterPostDoorClear ? afterPostDoorClear->hp : 0.0f,
                              PartyMgr::GetIsPartyDefeated() ? 1 : 0);
                    if (diedPostDoorClear) {
                        if (recoveryRetries < kMaxRecoveryRetries &&
                            totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                            WaitForPartyRecovery(route.map_id, 180000u)) {
                            ++recoveryRetries;
                            ++totalRecoveryRetries;
                            RecordRragarsWipeAndRecoverConsumables("post-checkpoint-clear");
                            auto* recovered = AgentMgr::GetMyAgent();
                            int retryIndex = recovered
                                ? DungeonRoute::FindNearestWaypointIndex(
                                      route.waypoints,
                                      route.waypoint_count,
                                      recovered->x,
                                      recovered->y)
                                : i;
                            if (retryIndex < 0) {
                                retryIndex = i;
                            }
                            if (retryIndex > i) {
                                retryIndex = i;
                            }
                            Log::Info("RragarsDbg: post-checkpoint clear recovery route=%s waypoint=%d retryIndex=%d retry=%d total=%d",
                                      route.name,
                                      i,
                                      retryIndex,
                                      recoveryRetries,
                                      totalRecoveryRetries);
                            i = retryIndex - 1;
                            continue;
                        }
                        Log::Info("RragarsDbg: post-checkpoint clear recovery failed route=%s waypoint=%d retry=%d total=%d",
                                  route.name,
                                  i,
                                  recoveryRetries,
                                  totalRecoveryRetries);
                        return false;
                    }
                }
                break;
            }

            if (checkpointFailureIndex == i) {
                ++checkpointFailureAttempts;
            } else {
                checkpointFailureIndex = i;
                checkpointFailureAttempts = 1;
            }
            if (checkpointFailureAttempts > kMaxCheckpointFailureAttempts) {
                LogBot("Rragars: checkpoint %s failed %d consecutive times at index %d on %s, aborting route",
                       plan.waypoint->label,
                       checkpointFailureAttempts,
                       i,
                       route.name);
                return false;
            }

            switch (resolution.action) {
            case DungeonCheckpoint::CheckpointFailureAction::AbortRun:
                LogBot("Rragars: checkpoint %s failed at index %d, returning to Doomlore Shrine",
                       plan.waypoint->label, i);
                return TravelOutpostAndWait(GWA3::MapIds::DOOMLORE_SHRINE, "checkpoint-abort-return");
            case DungeonCheckpoint::CheckpointFailureAction::BacktrackRetry:
                LogBot("Rragars: checkpoint %s failed at index %d, backtracking to %d and retrying from loop index %d",
                       plan.waypoint->label, i, resolution.backtrack_start, resolution.retry_index);
                if (!ReplayCheckpointBacktrack(route, i, resolution.backtrack_start)) {
                    return false;
                }
                // Match the AutoIt loop semantics: the enclosing for-loop increments after this case.
                i = resolution.retry_index;
                break;
            default:
                LogBot("Rragars: checkpoint %s failed at index %d without a retry policy",
                       plan.waypoint->label, i);
                return false;
            }
            break;
        }
        case WaypointBehavior::DoubleInteract: {
            if (!MoveToWaypoint(*plan.waypoint, route.map_id, 250.0f)) {
                auto* after = AgentMgr::GetMyAgent();
                Log::Info("RragarsDbg: double-interact move failed route=%s waypoint=%d label=%s currentMap=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d remaining=%.0f",
                          route.name,
                          i,
                          plan.waypoint->label,
                          MapMgr::GetMapId(),
                          after ? after->x : 0.0f,
                          after ? after->y : 0.0f,
                          after ? after->hp : 0.0f,
                          PartyMgr::GetIsPartyDefeated() ? 1 : 0,
                          DungeonCombat::DistanceToPoint(plan.waypoint->x, plan.waypoint->y));
                if (after && after->hp <= 0.0f && recoveryRetries < kMaxRecoveryRetries &&
                    totalRecoveryRetries < kMaxTotalRecoveryRetries &&
                    WaitForPartyRecovery(route.map_id)) {
                    ++recoveryRetries;
                    ++totalRecoveryRetries;
                    RecordRragarsWipeAndRecoverConsumables("double-interact");
                    auto* recovered = AgentMgr::GetMyAgent();
                    const int nearestAfterRez = recovered
                        ? DungeonRoute::FindNearestWaypointIndex(
                              route.waypoints,
                              route.waypoint_count,
                              recovered->x,
                              recovered->y)
                        : i;
                    const int retryIndex = computeRecoveryRetryIndex(i, nearestAfterRez, plan.behavior);
                    Log::Info("RragarsDbg: double-interact recovery retry route=%s failedWaypoint=%d nearest=%d retryIndex=%d retry=%d total=%d",
                              route.name,
                              i,
                              nearestAfterRez,
                              retryIndex,
                              recoveryRetries,
                              totalRecoveryRetries);
                    i = retryIndex - 1;
                    continue;
                }
                if (after && after->hp <= 0.0f) {
                    Log::Info("RragarsDbg: double-interact recovery skipped route=%s waypoint=%d retry=%d total=%d maxRetry=%d maxTotal=%d",
                              route.name,
                              i,
                              recoveryRetries,
                              totalRecoveryRetries,
                              kMaxRecoveryRetries,
                              kMaxTotalRecoveryRetries);
                }
                return false;
            }
            recoveryRetries = 0;
            const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(
                plan.waypoint->x,
                plan.waypoint->y,
                1500.0f);
            if (signpostId == 0u) {
                LogBot("Rragars: no signpost found near waypoint %d label=%s route=%s",
                       i, plan.waypoint->label, route.name);
                return false;
            }
            AgentMgr::InteractSignpost(signpostId);
            WaitMs(500);
            const bool firstActionQueued = UIMgr::ActionKeyPress(kActionInteractCode);
            Log::Info("RragarsDbg: double-interact route=%s waypoint=%d label=%s signpost=%u firstActionQueued=%d",
                      route.name,
                      i,
                      plan.waypoint->label,
                      signpostId,
                      firstActionQueued ? 1 : 0);
            WaitMs(1000);
            AgentMgr::InteractSignpost(signpostId);
            WaitMs(500);
            const bool secondActionQueued = UIMgr::ActionKeyPress(kActionInteractCode);
            Log::Info("RragarsDbg: double-interact route=%s waypoint=%d label=%s signpost=%u secondActionQueued=%d",
                      route.name,
                      i,
                      plan.waypoint->label,
                      signpostId,
                      secondActionQueued ? 1 : 0);
            WaitMs(1000);
            break;
        }
        default:
            return false;
        }
    }

    if (!waitForTransition) {
        return true;
    }
    if (const ZoneTransitionPoint* transition = FindZoneTransitionPoint(routeId)) {
        Log::Info("RragarsDbg: route transition route=%s point=(%.0f, %.0f) nextMap=%u",
                  route.name,
                  transition->x,
                  transition->y,
                  route.next_map_id);
        return ZoneThroughPoint(transition->x, transition->y, route.next_map_id, 60000u);
    }
    return DungeonNavigation::WaitForMapId(route.next_map_id, 60000u);
}

BotState HandleTownSetup(BotConfig& cfg) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::DOOMLORE_SHRINE) {
        if (!WaitForMapReady(GWA3::MapIds::DOOMLORE_SHRINE, 30000u)) {
            return BotState::Error;
        }
        if (!RunRragarsTownMaintenanceIfNeeded()) {
            return BotState::Stopping;
        }
        if (!EnsureRragarsOutpostParty(cfg)) {
            return BotState::Error;
        }
        return BotState::Traveling;
    }
    if (mapId == GWA3::MapIds::DALADA_UPLANDS || mapId == GWA3::MapIds::GROTHMAR_WARDOWNS || mapId == GWA3::MapIds::SACNOTH_VALLEY) {
        return BotState::Traveling;
    }

    LogBot("Rragars: traveling to Doomlore Shrine from map %u", mapId);
    if (!TravelOutpostAndWait(GWA3::MapIds::DOOMLORE_SHRINE, "town-setup-return-to-doomlore")) {
        return BotState::Error;
    }
    return BotState::InTown;
}

BotState HandleTravel(BotConfig& cfg) {
    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId == GWA3::MapIds::DOOMLORE_SHRINE) {
        if (!WaitForMapReady(GWA3::MapIds::DOOMLORE_SHRINE, 15000u)) {
            return BotState::Error;
        }
        if (!EnsureRragarsOutpostParty(cfg)) {
            return BotState::InTown;
        }
        if (!EnsureVeiledThreatQuest() || !LeaveDoomloreForDalada()) {
            return BotState::InTown;
        }
        return BotState::Traveling;
    }

    if ((mapId == GWA3::MapIds::DALADA_UPLANDS ||
         mapId == GWA3::MapIds::GROTHMAR_WARDOWNS ||
         mapId == GWA3::MapIds::SACNOTH_VALLEY) &&
        PartyMgr::CountPartyHeroes() < 7u) {
        if (!WaitForMapReady(mapId, 30000u)) {
            return BotState::Error;
        }
        if (!WaitForPartyHeroes(7u, 15000u)) {
            LogBot("Rragars: missing heroes in overland map %u heroes=%u; returning to Doomlore",
                   mapId,
                   PartyMgr::CountPartyHeroes());
            if (!TravelOutpostAndWait(GWA3::MapIds::DOOMLORE_SHRINE, "overland-missing-heroes-return")) {
                return BotState::Error;
            }
            return BotState::InTown;
        }
    }

    const RouteDefinition* route = FindRouteDefinitionByMapId(mapId);
    if (!route) {
        if (mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL2 || mapId == GWA3::MapIds::RRAGARS_MENAGERIE_LVL3) {
            return BotState::InDungeon;
        }
        LogBot("Rragars: unsupported travel map %u", mapId);
        return BotState::Error;
    }

    if (!WaitForMapReady(mapId, 15000u)) {
        return BotState::Error;
    }

    if (mapId == GWA3::MapIds::DALADA_UPLANDS ||
        mapId == GWA3::MapIds::GROTHMAR_WARDOWNS ||
        mapId == GWA3::MapIds::SACNOTH_VALLEY) {
        if (!EnsureRragarsConsets(cfg, "overland-route")) {
            LogBot("Rragars: consets are required but not active in overland map %u", mapId);
            return BotState::Error;
        }
    }

    const RouteId routeId = static_cast<RouteId>(route - &GetRouteDefinition(RouteId::RunDaladaToGrothmar));
    LogBot("Rragars: executing travel route %s", route->name);
    if (!ExecuteSimpleRoute(routeId)) {
        return BotState::Error;
    }
    return MapMgr::GetMapId() == GWA3::MapIds::RRAGARS_MENAGERIE_LVL1 ? BotState::InDungeon : BotState::Traveling;
}

BotState HandleDungeon(BotConfig& cfg) {
    const uint32_t mapId = MapMgr::GetMapId();
    RouteId routeId = RouteId::Level1;
    switch (mapId) {
    case GWA3::MapIds::RRAGARS_MENAGERIE_LVL1:
        routeId = RouteId::Level1;
        break;
    case GWA3::MapIds::RRAGARS_MENAGERIE_LVL2:
        routeId = RouteId::Level2;
        break;
    case GWA3::MapIds::RRAGARS_MENAGERIE_LVL3:
        routeId = RouteId::Level3;
        break;
    default:
        return BotState::InTown;
    }

    if (!WaitForMapReady(mapId, 15000u)) {
        auto* me = AgentMgr::GetMyAgent();
        Log::Info("RragarsDbg: handle-dungeon map not ready map=%u loading=%u isLoaded=%d myId=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d",
                  mapId,
                  MapMgr::GetLoadingState(),
                  MapMgr::GetIsMapLoaded() ? 1 : 0,
                  AgentMgr::GetMyId(),
                  me ? me->x : 0.0f,
                  me ? me->y : 0.0f,
                  me ? me->hp : 0.0f,
                  PartyMgr::GetIsPartyDefeated() ? 1 : 0);
        return BotState::Error;
    }

    if (!EnsureRragarsConsets(cfg, "dungeon-route")) {
        LogBot("Rragars: consets are required but not active in dungeon");
        return BotState::Error;
    }

    LogBot("Rragars: executing dungeon route on map %u", mapId);
    Log::Info("RragarsDbg: executing dungeon route map=%u route=%d", mapId, static_cast<int>(routeId));
    if (routeId == RouteId::Level3) {
        if (!ExecuteSimpleRoute(routeId, false) || !ExecuteRewardChestFlow()) {
            Log::Info("RragarsDbg: level3 route/reward flow failed map=%u", MapMgr::GetMapId());
            return BotState::Error;
        }
        s_wipeCount = 0u;
        return BotState::InTown;
    }

    if (!ExecuteSimpleRoute(routeId, true)) {
        auto* me = AgentMgr::GetMyAgent();
        Log::Info("RragarsDbg: dungeon route failed route=%d map=%u player=(%.0f, %.0f) hp=%.2f partyDefeated=%d",
                  static_cast<int>(routeId),
                  MapMgr::GetMapId(),
                  me ? me->x : 0.0f,
                  me ? me->y : 0.0f,
                  me ? me->hp : 0.0f,
                  PartyMgr::GetIsPartyDefeated() ? 1 : 0);
        return BotState::Error;
    }
    if (routeId == RouteId::Level2 || routeId == RouteId::Level3) {
        s_wipeCount = 0u;
    }
    return MapMgr::GetMapId() == GWA3::MapIds::DOOMLORE_SHRINE ? BotState::InTown : BotState::InDungeon;
}

BotState HandleError(BotConfig&) {
    const uint32_t mapId = MapMgr::GetMapId();
    auto* me = AgentMgr::GetMyAgent();
    LogBot("Rragars: ERROR state - map=%u loading=%u loaded=%d player=(%.0f, %.0f) hp=%.2f",
           mapId,
           MapMgr::GetLoadingState(),
           MapMgr::GetIsMapLoaded() ? 1 : 0,
           me ? me->x : 0.0f,
           me ? me->y : 0.0f,
           me ? me->hp : 0.0f);
    if (IsRragarsDungeonMap(mapId) || PartyMgr::GetIsPartyDefeated() || (me && me->hp <= 0.0f)) {
        LogBot("Rragars: fatal explorable error state detected, stopping lane for soak relaunch");
        return BotState::Stopping;
    }

    LogBot("Rragars: ERROR state - waiting before retry");
    WaitMs(5000);
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
    cfg.use_consets = true;
    cfg.target_map_id = GWA3::MapIds::RRAGARS_MENAGERIE_LVL1;
    cfg.outpost_map_id = GWA3::MapIds::DOOMLORE_SHRINE;
    cfg.bot_module_name = "RragarsMenagerie";

    LogBot("Rragars Menagerie module registered (in-progress runtime)");
}

} // namespace GWA3::Bot::RragarsMenagerieBot
