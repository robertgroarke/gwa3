#include <gwa3/dungeon/DungeonBundle.h>

#include <gwa3/dungeon/DungeonBuiltinCombat.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonLoot.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/Agent.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>

namespace GWA3::DungeonBundle {

namespace {

constexpr float kPickupItemTolerance = 120.0f;
constexpr uint32_t kPickupMoveTimeoutMs = 5000u;
constexpr uint32_t kPickupMoveReissueMs = 250u;
constexpr float kSignpostInteractTolerance = 90.0f;
constexpr float kLegacySignpostStandOffDistance = 60.0f;
constexpr float kLegacySignpostStandOffTolerance = 15.0f;
constexpr uint32_t kLegacySignpostMoveTimeoutMs = 5000u;
constexpr uint32_t kSignpostMoveTimeoutMs = 15000u;
constexpr uint32_t kChestPickupMoveTimeoutMs = 15000u;
constexpr float kChestPickupFightRange = 1200.0f;
constexpr uint32_t kChestInteractSettleMs = 5000u;
constexpr float kBundleChestExactSignpostTolerance = 150.0f;
constexpr float kBundleChestFollowupTolerance = 250.0f;
constexpr uint32_t kMaxLoggedNearbyItems = 12u;
constexpr uint32_t kMaxLoggedNearbyGadgets = 12u;
constexpr uint32_t kMaxLoggedPlayerEffects = 12u;
constexpr uint32_t kMaxLoggedPlayerBuffs = 8u;

struct TrackedEffectProbe {
    uint32_t skill_id = 0u;
    const char* label = "";
};

constexpr TrackedEffectProbe kTrackedBundleEffectProbes[] = {
    {984u, "torch_enchantment"},
    {998u, "torch_hex"},
    {999u, "torch_degeneration"},
    {2429u, "asura_flame_staff"},
    {2545u, "lit_torch"},
};

void LogHeldBundleSnapshot(const char* label) {
    const auto* inventory = ItemMgr::GetInventory();
    const Item* heldBundle = inventory ? inventory->bundle : nullptr;
    Log::Info(
        "DungeonBundle: %s heldBundle itemId=%u model=%u type=%u bag=%p",
        label,
        heldBundle ? heldBundle->item_id : 0u,
        heldBundle ? heldBundle->model_id : 0u,
        heldBundle ? heldBundle->type : 0u,
        heldBundle ? static_cast<const void*>(heldBundle->bag) : nullptr);
}

void LogPlayerEffectSnapshot(const char* label) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t myId = me ? me->agent_id : AgentMgr::GetMyId();

    Log::Info(
        "DungeonBundle: %s player agent=%u pos=(%.0f, %.0f) hp=%.2f energy=%.2f "
        "livingEffects=0x%08X visual=0x%04X skill=%u weaponType=%u weaponItem=%u offhandItem=%u",
        label,
        myId,
        me ? me->x : 0.0f,
        me ? me->y : 0.0f,
        me ? me->hp : 0.0f,
        me ? me->energy : 0.0f,
        me ? me->effects : 0u,
        me ? me->visual_effects : 0u,
        me ? me->skill : 0u,
        me ? me->weapon_type : 0u,
        me ? me->weapon_item_id : 0u,
        me ? me->offhand_item_id : 0u);

    auto* agentEffects = myId != 0u ? EffectMgr::GetAgentEffects(myId) : nullptr;
    auto* effectArr = myId != 0u ? EffectMgr::GetAgentEffectArray(myId) : nullptr;
    auto* buffArr = myId != 0u ? EffectMgr::GetAgentBuffArray(myId) : nullptr;

    Log::Info(
        "DungeonBundle: %s playerEffects agent=%u agentEffects=%p effectCount=%u buffCount=%u",
        label,
        myId,
        static_cast<void*>(agentEffects),
        effectArr ? effectArr->size : 0u,
        buffArr ? buffArr->size : 0u);

    for (uint32_t i = 0u; effectArr != nullptr && i < effectArr->size && i < kMaxLoggedPlayerEffects; ++i) {
        const Effect& effect = effectArr->buffer[i];
        Log::Info(
            "DungeonBundle: %s playerEffect[%u] skill=%u attr=%u effectId=%u srcAgent=%u duration=%.1f remaining=%.1f",
            label,
            i + 1u,
            effect.skill_id,
            effect.attribute_level,
            effect.effect_id,
            effect.agent_id,
            effect.duration,
            EffectMgr::GetEffectTimeRemaining(myId, effect.skill_id));
    }

    for (uint32_t i = 0u; buffArr != nullptr && i < buffArr->size && i < kMaxLoggedPlayerBuffs; ++i) {
        const Buff& buff = buffArr->buffer[i];
        Log::Info(
            "DungeonBundle: %s playerBuff[%u] skill=%u buffId=%u target=%u",
            label,
            i + 1u,
            buff.skill_id,
            buff.buff_id,
            buff.target_agent_id);
    }

    for (const auto& probe : kTrackedBundleEffectProbes) {
        const Effect* effect = myId != 0u ? EffectMgr::GetEffectBySkillId(myId, probe.skill_id) : nullptr;
        const Buff* buff = myId != 0u ? EffectMgr::GetBuffBySkillId(myId, probe.skill_id) : nullptr;
        Log::Info(
            "DungeonBundle: %s trackedEffect %s(%u) effect=%d buff=%d remaining=%.1f effectId=%u srcAgent=%u buffId=%u",
            label,
            probe.label,
            probe.skill_id,
            effect ? 1 : 0,
            buff ? 1 : 0,
            effect ? EffectMgr::GetEffectTimeRemaining(myId, probe.skill_id) : 0.0f,
            effect ? effect->effect_id : 0u,
            effect ? effect->agent_id : 0u,
            buff ? buff->buff_id : 0u);
    }
}

void LogTargetSnapshot(const char* label) {
    const uint32_t targetId = AgentMgr::GetTargetId();
    if (targetId == 0u) {
        Log::Info("DungeonBundle: %s target=0", label);
        return;
    }

    auto* target = AgentMgr::GetAgentByID(targetId);
    if (!target) {
        Log::Info("DungeonBundle: %s target=%u missing", label, targetId);
        return;
    }

    if (target->type == 0x400u) {
        const auto* itemAgent = static_cast<const AgentItem*>(target);
        const auto* item = ItemMgr::GetItemById(itemAgent->item_id);
        Log::Info(
            "DungeonBundle: %s targetItem agent=%u itemId=%u model=%u type=%u owner=%u extra=%u pos=(%.0f, %.0f)",
            label,
            targetId,
            itemAgent->item_id,
            item ? item->model_id : 0u,
            item ? item->type : 0u,
            itemAgent->owner,
            itemAgent->extra_type,
            itemAgent->x,
            itemAgent->y);
        return;
    }

    if (target->type == 0x200u) {
        const auto* gadget = static_cast<const AgentGadget*>(target);
        Log::Info(
            "DungeonBundle: %s targetGadget agent=%u gadget=%u extra=%u pos=(%.0f, %.0f)",
            label,
            targetId,
            gadget->gadget_id,
            gadget->extra_type,
            gadget->x,
            gadget->y);
        return;
    }

    if (target->type == 0xDBu) {
        const auto* living = static_cast<const AgentLiving*>(target);
        Log::Info(
            "DungeonBundle: %s targetLiving agent=%u player=%u allegiance=%u hp=%.2f effects=0x%08X skill=%u pos=(%.0f, %.0f)",
            label,
            targetId,
            living->player_number,
            living->allegiance,
            living->hp,
            living->effects,
            living->skill,
            living->x,
            living->y);
        return;
    }

    Log::Info(
        "DungeonBundle: %s targetAgent agent=%u type=0x%X pos=(%.0f, %.0f)",
        label,
        targetId,
        target->type,
        target->x,
        target->y);
}

void LogNearbyGroundItems(
    const char* label,
    float centerX,
    float centerY,
    float searchRadius,
    uint32_t desiredModelId) {
    auto* me = AgentMgr::GetMyAgent();
    const uint32_t myId = me ? me->agent_id : 0u;
    const float meX = me ? me->x : 0.0f;
    const float meY = me ? me->y : 0.0f;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();

    uint32_t totalNearby = 0u;
    uint32_t matchingModel = 0u;
    uint32_t bundleType = 0u;
    uint32_t lootable = 0u;
    uint32_t logged = 0u;

    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x400u) {
            continue;
        }

        const float distFromCenter = AgentMgr::GetDistance(centerX, centerY, agent->x, agent->y);
        if (distFromCenter > searchRadius) {
            continue;
        }

        ++totalNearby;
        auto* itemAgent = static_cast<const AgentItem*>(agent);
        auto* item = ItemMgr::GetItemById(itemAgent->item_id);
        if (!item) {
            if (logged < kMaxLoggedNearbyItems) {
                Log::Info(
                    "DungeonBundle: %s nearbyItem[%u] agent=%u itemId=%u itemRecord=missing owner=%u "
                    "pos=(%.0f, %.0f) distCenter=%.0f",
                    label,
                    logged + 1u,
                    agent->agent_id,
                    itemAgent->item_id,
                    itemAgent->owner,
                    agent->x,
                    agent->y,
                    distFromCenter);
                ++logged;
            }
            continue;
        }

        const bool modelMatch = desiredModelId != 0u && item->model_id == desiredModelId;
        const bool canPick = itemAgent->owner == 0u || itemAgent->owner == myId;
        if (modelMatch) {
            ++matchingModel;
        }
        if (item->type == DungeonLoot::TYPE_BUNDLE) {
            ++bundleType;
        }
        if (canPick) {
            ++lootable;
        }

        if (logged < kMaxLoggedNearbyItems) {
            const float distFromPlayer = me ? AgentMgr::GetDistance(meX, meY, agent->x, agent->y) : -1.0f;
            Log::Info(
                "DungeonBundle: %s nearbyItem[%u] agent=%u itemId=%u model=%u type=%u owner=%u bag=%p "
                "pos=(%.0f, %.0f) distCenter=%.0f distPlayer=%.0f modelMatch=%d lootable=%d",
                label,
                logged + 1u,
                agent->agent_id,
                itemAgent->item_id,
                item->model_id,
                item->type,
                itemAgent->owner,
                static_cast<const void*>(item->bag),
                agent->x,
                agent->y,
                distFromCenter,
                distFromPlayer,
                modelMatch ? 1 : 0,
                canPick ? 1 : 0);
            ++logged;
        }
    }

    Log::Info(
        "DungeonBundle: %s summary center=(%.0f, %.0f) radius=%.0f desiredModel=%u totalNearby=%u "
        "matchingModel=%u bundleType=%u lootable=%u player=(%.0f, %.0f) map=%u",
        label,
        centerX,
        centerY,
        searchRadius,
        desiredModelId,
        totalNearby,
        matchingModel,
        bundleType,
        lootable,
        meX,
        meY,
        MapMgr::GetMapId());
}

void LogNearbyGadgets(
    const char* label,
    float centerX,
    float centerY,
    float searchRadius) {
    auto* me = AgentMgr::GetMyAgent();
    const float meX = me ? me->x : 0.0f;
    const float meY = me ? me->y : 0.0f;
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();

    uint32_t totalNearby = 0u;
    uint32_t chestNearby = 0u;
    uint32_t logged = 0u;

    for (uint32_t i = 1u; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0x200u) {
            continue;
        }

        const auto* gadget = static_cast<const AgentGadget*>(agent);
        const float distFromCenter = AgentMgr::GetDistance(centerX, centerY, gadget->x, gadget->y);
        if (distFromCenter > searchRadius) {
            continue;
        }

        ++totalNearby;
        const bool chestMatch = DungeonInteractions::IsChestGadgetId(gadget->gadget_id);
        if (chestMatch) {
            ++chestNearby;
        }

        if (logged < kMaxLoggedNearbyGadgets) {
            const float distFromPlayer = me ? AgentMgr::GetDistance(meX, meY, gadget->x, gadget->y) : -1.0f;
            Log::Info(
                "DungeonBundle: %s nearbyGadget[%u] agent=%u gadget=%u pos=(%.0f, %.0f) "
                "distCenter=%.0f distPlayer=%.0f chestMatch=%d",
                label,
                logged + 1u,
                gadget->agent_id,
                gadget->gadget_id,
                gadget->x,
                gadget->y,
                distFromCenter,
                distFromPlayer,
                chestMatch ? 1 : 0);
            ++logged;
        }
    }

    Log::Info(
        "DungeonBundle: %s gadgetSummary center=(%.0f, %.0f) radius=%.0f totalNearby=%u "
        "chestNearby=%u player=(%.0f, %.0f) map=%u",
        label,
        centerX,
        centerY,
        searchRadius,
        totalNearby,
        chestNearby,
        meX,
        meY,
        MapMgr::GetMapId());
}

void LogBundleAcquireSnapshot(
    const char* label,
    float centerX,
    float centerY,
    float searchRadius,
    uint32_t desiredModelId) {
    LogHeldBundleSnapshot(label);
    LogPlayerEffectSnapshot(label);
    LogTargetSnapshot(label);
    LogNearbyGroundItems(label, centerX, centerY, searchRadius, desiredModelId);
}

void LogResolvedSignpost(
    const char* label,
    uint32_t signpostId,
    float centerX,
    float centerY,
    float searchRadius) {
    LogNearbyGadgets(label, centerX, centerY, searchRadius);
    if (signpostId == 0u) {
        Log::Info(
            "DungeonBundle: %s resolvedSignpost=0 center=(%.0f, %.0f) radius=%.0f",
            label,
            centerX,
            centerY,
            searchRadius);
        return;
    }

    auto* agent = AgentMgr::GetAgentByID(signpostId);
    if (!agent || agent->type != 0x200u) {
        Log::Info(
            "DungeonBundle: %s resolvedSignpost=%u invalidOrNonGadget type=%u",
            label,
            signpostId,
            agent ? agent->type : 0u);
        return;
    }

    const auto* gadget = static_cast<const AgentGadget*>(agent);
    Log::Info(
        "DungeonBundle: %s resolvedSignpost=%u gadget=%u pos=(%.0f, %.0f) chestMatch=%d",
        label,
        signpostId,
        gadget->gadget_id,
        gadget->x,
        gadget->y,
        DungeonInteractions::IsChestGadgetId(gadget->gadget_id) ? 1 : 0);
}

void ClearTargetAndStop() {
    AgentMgr::CancelAction();
    Sleep(100u);
    AgentMgr::ChangeTarget(0u);
    Sleep(150u);
}

void MoveNearSignpostIfPossible(uint32_t signpostId) {
    if (signpostId == 0u || AgentMgr::GetMyAgent() == nullptr) {
        return;
    }

    const uint32_t mapId = MapMgr::GetMapId();
    (void)DungeonNavigation::MoveToAgent(
        signpostId,
        kSignpostInteractTolerance,
        kSignpostMoveTimeoutMs,
        kPickupMoveReissueMs,
        mapId);
}

bool MoveToDistanceAwayFromSignpostIfPossible(
    uint32_t signpostId,
    float desiredDistance = kLegacySignpostStandOffDistance,
    float tolerance = kLegacySignpostStandOffTolerance) {
    auto* me = AgentMgr::GetMyAgent();
    auto* signpost = AgentMgr::GetAgentByID(signpostId);
    if (!me || !signpost) {
        return false;
    }

    const float currentDistance = AgentMgr::GetDistance(me->x, me->y, signpost->x, signpost->y);
    if (currentDistance <= desiredDistance + tolerance) {
        Log::Info(
            "DungeonBundle: legacy-signpost-stand-off already-in-range signpost=%u current=%.0f desired=%.0f tolerance=%.0f",
            signpostId,
            currentDistance,
            desiredDistance,
            tolerance);
        return true;
    }

    const float dx = me->x - signpost->x;
    const float dy = me->y - signpost->y;
    const float length = currentDistance > 1.0f ? currentDistance : 1.0f;
    const float targetX = signpost->x + ((dx / length) * desiredDistance);
    const float targetY = signpost->y + ((dy / length) * desiredDistance);
    const bool arrived = DungeonNavigation::MoveToAndWait(
        targetX,
        targetY,
        tolerance,
        kLegacySignpostMoveTimeoutMs,
        kPickupMoveReissueMs,
        MapMgr::GetMapId()).arrived;
    Log::Info(
        "DungeonBundle: legacy-signpost-stand-off move signpost=%u current=%.0f desired=%.0f target=(%.0f, %.0f) arrived=%d",
        signpostId,
        currentDistance,
        desiredDistance,
        targetX,
        targetY,
        arrived ? 1 : 0);
    return arrived;
}

bool MoveToPickupPointIfPossible(float x, float y);

uint32_t ResolveChestSignpostNearPoint(float x, float y, float searchRadius) {
    uint32_t signpostId = DungeonInteractions::FindNearestChestSignpost(x, y, searchRadius);
    if (signpostId != 0u) {
        LogResolvedSignpost("resolve-chest-signpost chest-only", signpostId, x, y, searchRadius);
        return signpostId;
    }
    signpostId = DungeonInteractions::FindNearestSignpost(x, y, searchRadius);
    LogResolvedSignpost("resolve-chest-signpost generic-fallback", signpostId, x, y, searchRadius);
    return signpostId;
}

uint32_t ResolveBundleChestSignpostNearPoint(
    float x,
    float y,
    float searchRadius,
    bool preferLooseChestSignpost = false) {
    const uint32_t chestSignpostId = DungeonInteractions::FindNearestChestSignpost(x, y, searchRadius);
    LogResolvedSignpost("resolve-bundle-signpost chest-only", chestSignpostId, x, y, searchRadius);
    if (chestSignpostId != 0u) {
        auto* chestAgent = AgentMgr::GetAgentByID(chestSignpostId);
        if (chestAgent && chestAgent->type == 0x200u) {
            const float chestDistToPoint = AgentMgr::GetDistance(x, y, chestAgent->x, chestAgent->y);
            if (chestDistToPoint <= kBundleChestExactSignpostTolerance) {
                Log::Info(
                    "DungeonBundle: resolve-bundle-signpost exact chest signpost=%u distToPoint=%.0f tolerance=%.0f",
                    chestSignpostId,
                    chestDistToPoint,
                    kBundleChestExactSignpostTolerance);
                return chestSignpostId;
            }
            if (preferLooseChestSignpost) {
                Log::Info(
                    "DungeonBundle: resolve-bundle-signpost chest-preferred signpost=%u distToPoint=%.0f searchRadius=%.0f",
                    chestSignpostId,
                    chestDistToPoint,
                    searchRadius);
                return chestSignpostId;
            }
            Log::Info(
                "DungeonBundle: resolve-bundle-signpost rejected chest signpost=%u distToPoint=%.0f tolerance=%.0f",
                chestSignpostId,
                chestDistToPoint,
                kBundleChestExactSignpostTolerance);
        }
    }

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(x, y, searchRadius);
    LogResolvedSignpost("resolve-bundle-signpost generic", signpostId, x, y, searchRadius);
    if (signpostId == 0u) {
        return 0u;
    }

    auto* agent = AgentMgr::GetAgentByID(signpostId);
    if (!agent || agent->type != 0x200u) {
        return 0u;
    }

    const float distToPoint = AgentMgr::GetDistance(x, y, agent->x, agent->y);
    if (distToPoint > kBundleChestExactSignpostTolerance) {
        Log::Info(
            "DungeonBundle: resolve-bundle-signpost rejected generic signpost=%u distToPoint=%.0f tolerance=%.0f",
            signpostId,
            distToPoint,
            kBundleChestExactSignpostTolerance);
        return 0u;
    }

    Log::Info(
        "DungeonBundle: resolve-bundle-signpost exact generic signpost=%u distToPoint=%.0f tolerance=%.0f",
        signpostId,
        distToPoint,
        kBundleChestExactSignpostTolerance);
    return signpostId;
}

bool IsResolvedChestSignpost(uint32_t signpostId) {
    if (signpostId == 0u) {
        return false;
    }

    auto* agent = AgentMgr::GetAgentByID(signpostId);
    if (!agent || agent->type != 0x200u) {
        return false;
    }

    const auto* gadget = static_cast<const AgentGadget*>(agent);
    return DungeonInteractions::IsChestGadgetId(gadget->gadget_id);
}

uint32_t WaitForPreferredChestSignpostNearPoint(
    float x,
    float y,
    float signpostSearchRadius,
    float itemSearchRadius,
    uint32_t modelId,
    uint32_t chestDelayMs) {
    constexpr int kPreferredChestSettlePasses = 3;

    for (int settlePass = 0; settlePass < kPreferredChestSettlePasses; ++settlePass) {
        (void)MoveToPickupPointIfPossible(x, y);
        Sleep(chestDelayMs);
        Log::Info(
            "DungeonBundle: bundle-acquire settle probe pass=%d/%d center=(%.0f, %.0f)",
            settlePass + 1,
            kPreferredChestSettlePasses,
            x,
            y);
        LogBundleAcquireSnapshot("bundle-acquire after-approach", x, y, itemSearchRadius, modelId);
        LogNearbyGadgets("bundle-acquire after-approach", x, y, signpostSearchRadius);

        const uint32_t signpostId =
            ResolveBundleChestSignpostNearPoint(x, y, signpostSearchRadius, true);
        const bool chestVisible = IsResolvedChestSignpost(signpostId);
        Log::Info(
            "DungeonBundle: bundle-acquire settle probe result pass=%d signpost=%u chestVisible=%d center=(%.0f, %.0f)",
            settlePass + 1,
            signpostId,
            chestVisible ? 1 : 0,
            x,
            y);
        if (chestVisible) {
            return signpostId;
        }
    }

    return 0u;
}

uint32_t ResolveBundleFollowupChestSignpostNearPoint(float x, float y, float searchRadius) {
    const uint32_t chestSignpostId = DungeonInteractions::FindNearestChestSignpost(x, y, searchRadius);
    LogResolvedSignpost("resolve-bundle-followup chest-only", chestSignpostId, x, y, searchRadius);
    if (chestSignpostId == 0u) {
        return 0u;
    }

    auto* chestAgent = AgentMgr::GetAgentByID(chestSignpostId);
    if (!chestAgent || chestAgent->type != 0x200u) {
        return 0u;
    }

    const float chestDistToPoint = AgentMgr::GetDistance(x, y, chestAgent->x, chestAgent->y);
    if (chestDistToPoint > kBundleChestFollowupTolerance) {
        Log::Info(
            "DungeonBundle: resolve-bundle-followup rejected chest signpost=%u distToPoint=%.0f tolerance=%.0f",
            chestSignpostId,
            chestDistToPoint,
            kBundleChestFollowupTolerance);
        return 0u;
    }

    Log::Info(
        "DungeonBundle: resolve-bundle-followup exact chest signpost=%u distToPoint=%.0f tolerance=%.0f",
        chestSignpostId,
        chestDistToPoint,
        kBundleChestFollowupTolerance);
    return chestSignpostId;
}

uint32_t ResolveBundleCenterGenericSignpostNearPoint(float x, float y, float searchRadius) {
    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(x, y, searchRadius);
    LogResolvedSignpost("resolve-bundle-center-signpost generic", signpostId, x, y, searchRadius);
    if (signpostId == 0u) {
        return 0u;
    }

    auto* agent = AgentMgr::GetAgentByID(signpostId);
    if (!agent || agent->type != 0x200u) {
        return 0u;
    }

    const auto* gadget = static_cast<const AgentGadget*>(agent);
    if (DungeonInteractions::IsChestGadgetId(gadget->gadget_id)) {
        Log::Info(
            "DungeonBundle: resolve-bundle-center-signpost rejected chest signpost=%u gadget=%u",
            signpostId,
            gadget->gadget_id);
        return 0u;
    }

    const float distToPoint = AgentMgr::GetDistance(x, y, agent->x, agent->y);
    if (distToPoint > kBundleChestExactSignpostTolerance) {
        Log::Info(
            "DungeonBundle: resolve-bundle-center-signpost rejected generic signpost=%u distToPoint=%.0f tolerance=%.0f",
            signpostId,
            distToPoint,
            kBundleChestExactSignpostTolerance);
        return 0u;
    }

    Log::Info(
        "DungeonBundle: resolve-bundle-center-signpost exact generic signpost=%u gadget=%u distToPoint=%.0f tolerance=%.0f",
        signpostId,
        gadget->gadget_id,
        distToPoint,
        kBundleChestExactSignpostTolerance);
    return signpostId;
}

bool MoveToPickupPointIfPossible(float x, float y) {
    if (AgentMgr::GetMyAgent() == nullptr) {
        return false;
    }

    const uint32_t mapId = MapMgr::GetMapId();
    if (mapId != 0u && MapMgr::GetIsMapLoaded()) {
        return DungeonBuiltinCombat::MoveToPointWithAggro(
            x,
            y,
            mapId,
            kPickupItemTolerance,
            kChestPickupFightRange,
            kChestPickupMoveTimeoutMs);
    }

    return DungeonNavigation::MoveToAndWait(
        x,
        y,
        kPickupItemTolerance,
        kChestPickupMoveTimeoutMs,
        kPickupMoveReissueMs,
        mapId).arrived;
}

bool WaitForHeldBundle(uint32_t timeoutMs, uint32_t pollMs = 100u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
            return true;
        }
        Sleep(pollMs);
    }
    return DungeonInteractions::GetHeldBundleItemId() != 0u;
}

bool InteractResolvedSignpost(uint32_t signpostId, int interactCount, uint32_t delayMs) {
    if (signpostId == 0u || interactCount <= 0) {
        return false;
    }

    ClearTargetAndStop();
    MoveNearSignpostIfPossible(signpostId);
    for (int i = 0; i < interactCount; ++i) {
        AgentMgr::InteractSignpost(signpostId);
        Sleep(delayMs);
    }
    return true;
}

bool OpenChestAtPoint(uint32_t signpostId, float x, float y, int interactCount, uint32_t delayMs) {
    if (interactCount <= 0) {
        return false;
    }

    if (signpostId != 0u) {
        MoveNearSignpostIfPossible(signpostId);
        ClearTargetAndStop();
        bool interacted = false;
        for (int i = 0; i < interactCount; ++i) {
            interacted = AgentMgr::InteractAgentWorldAction(signpostId) || interacted;
            Sleep(delayMs);
        }
        if (interacted) {
            return true;
        }
    }

    (void)MoveToPickupPointIfPossible(x, y);
    ClearTargetAndStop();
    for (int i = 0; i < interactCount; ++i) {
        (void)AgentMgr::ActionInteract();
        Sleep(delayMs);
    }

    if (signpostId != 0u) {
        MoveNearSignpostIfPossible(signpostId);
        for (int i = 0; i < interactCount; ++i) {
            AgentMgr::InteractSignpost(signpostId);
            Sleep(delayMs);
        }
    }

    return true;
}

bool OpenBundleChestAtPoint(uint32_t signpostId, float x, float y, int interactCount, uint32_t delayMs) {
    if (signpostId == 0u || interactCount <= 0) {
        return false;
    }

    (void)MoveToPickupPointIfPossible(x, y);
    MoveNearSignpostIfPossible(signpostId);
    ClearTargetAndStop();
    AgentMgr::ChangeTarget(signpostId);
    Sleep(150u);

    bool interacted = false;
    for (int i = 0; i < interactCount; ++i) {
        const bool queued = AgentMgr::InteractAgentWorldAction(signpostId);
        Log::Info(
            "DungeonBundle: bundle-open world-action signpost=%u press=%d queued=%d center=(%.0f, %.0f)",
            signpostId,
            i + 1,
            queued ? 1 : 0,
            x,
            y);
        interacted = queued || interacted;
        Sleep(delayMs);
    }

    if (!interacted) {
        MoveNearSignpostIfPossible(signpostId);
        ClearTargetAndStop();
        AgentMgr::ChangeTarget(signpostId);
        Sleep(150u);
        for (int i = 0; i < interactCount; ++i) {
            AgentMgr::InteractSignpost(signpostId);
            Log::Info(
                "DungeonBundle: bundle-open signpost-fallback signpost=%u press=%d center=(%.0f, %.0f)",
                signpostId,
                i + 1,
                x,
                y);
            interacted = true;
            Sleep(delayMs);
        }
    }

    CtoS::SendPacket(2, Packets::OPEN_CHEST, 2u);
    Log::Info(
        "DungeonBundle: bundle-open open-chest signpost=%u target=%u header=0x%X mode=%u center=(%.0f, %.0f)",
        signpostId,
        AgentMgr::GetTargetId(),
        Packets::OPEN_CHEST,
        2u,
        x,
        y);
    Sleep(delayMs);

    (void)MoveToPickupPointIfPossible(x, y);
    return interacted;
}

bool PickUpResolvedItem(uint32_t itemAgentId, int pickupAttempts, uint32_t delayMs) {
    if (itemAgentId == 0u || pickupAttempts <= 0) {
        return false;
    }

    ClearTargetAndStop();
    (void)DungeonNavigation::MoveToAgent(
        itemAgentId,
        kPickupItemTolerance,
        kPickupMoveTimeoutMs,
        kPickupMoveReissueMs,
        MapMgr::GetMapId());

    bool pickedAny = false;
    for (int i = 0; i < pickupAttempts; ++i) {
        if (!AgentMgr::GetAgentExists(itemAgentId)) {
            return pickedAny;
        }

        ClearTargetAndStop();
        ItemMgr::PickUpItem(itemAgentId);
        pickedAny = true;
        Sleep(delayMs);
    }

    return pickedAny;
}

bool PickUpHeldBundleByModelNearPoint(
    float x,
    float y,
    uint32_t modelId,
    float searchRadius,
    int pickupAttempts,
    uint32_t delayMs) {
    if (pickupAttempts <= 0 || modelId == 0u) {
        return false;
    }

    for (int i = 0; i < pickupAttempts; ++i) {
        if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
            return true;
        }

        (void)MoveToPickupPointIfPossible(x, y);

        const uint32_t itemAgentId =
            DungeonInteractions::FindNearestItemByModel(x, y, searchRadius, modelId);
        if (itemAgentId == 0u) {
            Sleep(delayMs);
            continue;
        }

        if (PickUpResolvedItem(itemAgentId, 1, delayMs) &&
            DungeonInteractions::GetHeldBundleItemId() != 0u) {
            return true;
        }
    }

    return DungeonInteractions::GetHeldBundleItemId() != 0u;
}

int PickUpNearbyLootAtPoint(float x, float y, float searchRadius, uint32_t delayMs) {
    (void)MoveToPickupPointIfPossible(x, y);

    DungeonLoot::LootPickupOptions options;
    options.interact_threshold = 200.0f;
    options.move_timeout_ms = kPickupMoveTimeoutMs;
    options.move_poll_ms = 100u;
    options.pickup_delay_ms = delayMs;
    options.global_timeout_ms = kChestPickupMoveTimeoutMs;
    return DungeonLoot::PickUpNearbyLoot(
        searchRadius,
        &DungeonBuiltinCombat::WaitMs,
        &DungeonBuiltinCombat::IsPlayerOrPartyDead,
        options);
}

} // namespace

bool InteractSignpostNearPoint(
    float x,
    float y,
    float searchRadius,
    int interactCount,
    uint32_t delayMs) {
    if (interactCount <= 0) {
        return false;
    }

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(x, y, searchRadius);
    if (signpostId == 0u) {
        return false;
    }

    return InteractResolvedSignpost(signpostId, interactCount, delayMs);
}

bool AutoItGoToSignpostAndAcquireHeldBundleNearPoint(
    float x,
    float y,
    float searchRadius,
    int passes,
    uint32_t signpostDelayMs,
    uint32_t acquireTimeoutMs) {
    if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
        return true;
    }
    if (passes <= 0) {
        return false;
    }

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(x, y, searchRadius);
    if (signpostId == 0u) {
        return false;
    }
    LogResolvedSignpost("legacy-bundle-acquire generic", signpostId, x, y, searchRadius);
    const uint32_t interactionDelayMs = signpostDelayMs == 0u ? 100u : signpostDelayMs;

    for (int pass = 0; pass < passes; ++pass) {
        ClearTargetAndStop();
        const bool inRange = MoveToDistanceAwayFromSignpostIfPossible(signpostId);
        Log::Info(
            "DungeonBundle: legacy-bundle-acquire pass=%d signpost=%u center=(%.0f, %.0f) inRange=%d delay=%u",
            pass + 1,
            signpostId,
            x,
            y,
            inRange ? 1 : 0,
            interactionDelayMs);
        for (int press = 0; press < 2; ++press) {
            AgentMgr::InteractSignpostLegacy(signpostId);
            Log::Info(
                "DungeonBundle: legacy-bundle-acquire signpost pass=%d press=%d signpost=%u center=(%.0f, %.0f)",
                pass + 1,
                press + 1,
                signpostId,
                x,
                y);
            Sleep(interactionDelayMs);
            if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
                return true;
            }
        }
        if (WaitForHeldBundle(acquireTimeoutMs)) {
            return true;
        }
    }

    return DungeonInteractions::GetHeldBundleItemId() != 0u;
}

bool InteractSignpostAndAcquireHeldBundleNearPoint(
    float x,
    float y,
    float searchRadius,
    int interactCount,
    uint32_t interactDelayMs,
    uint32_t acquireTimeoutMs) {
    if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
        return true;
    }
    if (interactCount <= 0) {
        return false;
    }

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(x, y, searchRadius);
    if (signpostId == 0u) {
        return false;
    }

    for (int pass = 0; pass < 2; ++pass) {
        MoveNearSignpostIfPossible(signpostId);
        AgentMgr::CancelAction();
        Sleep(100u);
        bool queuedAny = false;
        for (int i = 0; i < interactCount; ++i) {
            const bool queued = AgentMgr::InteractAgentWorldAction(signpostId);
            Log::Info(
                "DungeonBundle: bundle-acquire world-action pass=%d press=%d signpost=%u center=(%.0f, %.0f) queued=%d",
                pass + 1,
                i + 1,
                signpostId,
                x,
                y,
                queued ? 1 : 0);
            queuedAny = queued || queuedAny;
            Sleep(interactDelayMs);
            if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
                return true;
            }
        }

        if (queuedAny && WaitForHeldBundle(acquireTimeoutMs)) {
            return true;
        }

        MoveNearSignpostIfPossible(signpostId);
        AgentMgr::CancelAction();
        Sleep(100u);
        for (int i = 0; i < interactCount; ++i) {
            AgentMgr::InteractSignpost(signpostId);
            Log::Info(
                "DungeonBundle: bundle-acquire signpost pass=%d press=%d signpost=%u center=(%.0f, %.0f)",
                pass + 1,
                i + 1,
                signpostId,
                x,
                y);
            Sleep(interactDelayMs);
            if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
                return true;
            }
        }

        if (WaitForHeldBundle(acquireTimeoutMs)) {
            return true;
        }
    }

    return false;
}

bool InteractSignpostAndPickUpLootNearPoint(
    float x,
    float y,
    float signpostSearchRadius,
    int interactCount,
    uint32_t interactDelayMs,
    float lootSearchRadius,
    uint32_t lootDelayMs) {
    const bool interacted =
        InteractSignpostNearPoint(x, y, signpostSearchRadius, interactCount, interactDelayMs);
    const int pickedCount = PickUpNearbyLootAtPoint(x, y, lootSearchRadius, lootDelayMs);
    return interacted || pickedCount > 0;
}

bool PickUpNearestItemNearPoint(
    float x,
    float y,
    float searchRadius,
    int pickupAttempts,
    uint32_t delayMs) {
    if (pickupAttempts <= 0) {
        return false;
    }

    bool pickedAny = false;
    for (int i = 0; i < pickupAttempts; ++i) {
        const uint32_t itemAgentId = DungeonInteractions::FindNearestItem(x, y, searchRadius);
        if (itemAgentId == 0u) {
            continue;
        }
        if (PickUpResolvedItem(itemAgentId, 1, delayMs)) {
            pickedAny = true;
        }
    }
    return pickedAny;
}

bool PickUpNearestItemByModelNearPoint(
    float x,
    float y,
    uint32_t modelId,
    float searchRadius,
    int pickupAttempts,
    uint32_t delayMs) {
    if (pickupAttempts <= 0 || modelId == 0u) {
        return false;
    }

    bool pickedAny = false;
    for (int i = 0; i < pickupAttempts; ++i) {
        const uint32_t itemAgentId =
            DungeonInteractions::FindNearestItemByModel(x, y, searchRadius, modelId);
        if (itemAgentId == 0u) {
            continue;
        }
        if (PickUpResolvedItem(itemAgentId, 1, delayMs)) {
            pickedAny = true;
        }
    }
    return pickedAny;
}

bool OpenChestAndPickUpBundle(
    float x,
    float y,
    float signpostSearchRadius,
    float itemSearchRadius,
    int interactCount,
    int pickupAttempts,
    uint32_t interactDelayMs,
    uint32_t pickupDelayMs) {
    const uint32_t signpostId = ResolveChestSignpostNearPoint(x, y, signpostSearchRadius);
    if (signpostId == 0u) {
        return false;
    }

    const uint32_t chestDelayMs = interactDelayMs == 0u
        ? 0u
        : (interactDelayMs > kChestInteractSettleMs ? interactDelayMs : kChestInteractSettleMs);

    if (!OpenChestAtPoint(signpostId, x, y, interactCount, interactDelayMs)) {
        return false;
    }
    Sleep(chestDelayMs);

    for (int attempt = 0; attempt < pickupAttempts; ++attempt) {
        (void)MoveToPickupPointIfPossible(x, y);
        if (PickUpNearestItemNearPoint(x, y, itemSearchRadius, 1, pickupDelayMs)) {
            return true;
        }
        if (PickUpNearbyLootAtPoint(x, y, itemSearchRadius, pickupDelayMs) > 0) {
            return true;
        }
    }

    return false;
}

bool TryOpenChestAt(
    float x,
    float y,
    uint32_t currentMapId,
    DungeonInteractions::OpenedChestTracker& tracker,
    const ChestOpenOptions& options) {
    tracker.ResetForMap(currentMapId);

    const uint32_t signpostId = DungeonInteractions::FindNearestSignpost(
        x,
        y,
        options.signpost_search_radius);
    if (signpostId == 0u || tracker.IsOpened(signpostId)) {
        return false;
    }

    tracker.MarkOpened(signpostId);
    return OpenChestAndPickUpBundle(
        x,
        y,
        options.signpost_search_radius,
        options.item_search_radius,
        options.interact_count,
        options.pickup_attempts,
        options.interact_delay_ms,
        options.pickup_delay_ms);
}

bool TryOpenDoorAt(float x, float y, const DoorOpenOptions& options) {
    return InteractSignpostNearPoint(
        x,
        y,
        options.signpost_search_radius,
        options.interact_count,
        options.interact_delay_ms);
}

bool ExecuteDoorOpenSequence(
    float x,
    float y,
    MoveToPointFn move_to_point,
    const DoorOpenOptions& options,
    float settle_threshold,
    uint32_t settle_delay_ms) {
    if (!TryOpenDoorAt(x, y, options)) {
        return false;
    }

    if (move_to_point != nullptr) {
        move_to_point(x, y, settle_threshold);
    }
    Sleep(settle_delay_ms);
    return true;
}

bool OpenChestAndAcquireHeldBundleByModelImpl(
    float x,
    float y,
    uint32_t modelId,
    float signpostSearchRadius,
    float itemSearchRadius,
    int interactCount,
    int pickupAttempts,
    uint32_t interactDelayMs,
    uint32_t pickupDelayMs,
    bool legacyFirst,
    bool preferLooseChestSignpost) {
    if (modelId == 0u || pickupAttempts <= 0) {
        return false;
    }

    const uint32_t chestDelayMs = interactDelayMs == 0u
        ? 0u
        : (interactDelayMs > kChestInteractSettleMs ? interactDelayMs : kChestInteractSettleMs);
    const uint32_t legacyDelayMs = interactDelayMs == 0u
        ? 0u
        : (interactDelayMs > 250u ? interactDelayMs : 250u);
    const uint32_t legacyAcquireTimeoutMs = pickupDelayMs == 0u
        ? 0u
        : (pickupDelayMs > 1000u ? pickupDelayMs : 1000u);

    if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
        LogBundleAcquireSnapshot("bundle-acquire already-held", x, y, itemSearchRadius, modelId);
        return true;
    }

    LogBundleAcquireSnapshot("bundle-acquire before-open", x, y, itemSearchRadius, modelId);
    LogNearbyGadgets("bundle-acquire before-open", x, y, signpostSearchRadius);

    uint32_t settledPreferredSignpostId = 0u;
    if (preferLooseChestSignpost) {
    // Some bundle chests can stream in after the player settles at the
        // chest point, so give the local gadget set a few short re-resolve
        // passes before falling back to the center generic signpost.
        settledPreferredSignpostId = WaitForPreferredChestSignpostNearPoint(
            x,
            y,
            signpostSearchRadius,
            itemSearchRadius,
            modelId,
            chestDelayMs);
    }

    if (legacyFirst) {
        (void)AutoItGoToSignpostAndAcquireHeldBundleNearPoint(
            x,
            y,
            signpostSearchRadius,
            1,
            legacyDelayMs,
            legacyAcquireTimeoutMs);
        if (legacyDelayMs > 0u) {
            Sleep(legacyDelayMs);
        }
        LogBundleAcquireSnapshot("bundle-acquire after-legacy-open", x, y, itemSearchRadius, modelId);
        LogNearbyGadgets("bundle-acquire after-legacy-open", x, y, signpostSearchRadius);
        if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
            return true;
        }
        if (PickUpHeldBundleByModelNearPoint(
                x,
                y,
                modelId,
                itemSearchRadius,
                pickupAttempts,
                pickupDelayMs)) {
            Log::Info(
                "DungeonBundle: bundle-acquire legacy pickup succeeded heldBundle=%u",
                DungeonInteractions::GetHeldBundleItemId());
            return true;
        }
        const int legacyNearbyPicked = PickUpNearbyLootAtPoint(x, y, itemSearchRadius, pickupDelayMs);
        Log::Info(
            "DungeonBundle: bundle-acquire legacy nearby loot result picked=%d heldBundle=%u",
            legacyNearbyPicked,
            DungeonInteractions::GetHeldBundleItemId());
        LogBundleAcquireSnapshot("bundle-acquire after-legacy-pickup", x, y, itemSearchRadius, modelId);
        if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
            return true;
        }
    }

    const uint32_t signpostId = settledPreferredSignpostId != 0u
        ? settledPreferredSignpostId
        : ResolveBundleChestSignpostNearPoint(x, y, signpostSearchRadius, preferLooseChestSignpost);
    if (!OpenBundleChestAtPoint(signpostId, x, y, interactCount, interactDelayMs)) {
        return false;
    }
    Sleep(chestDelayMs);
    LogBundleAcquireSnapshot("bundle-acquire after-open", x, y, itemSearchRadius, modelId);
    LogNearbyGadgets("bundle-acquire after-open", x, y, signpostSearchRadius);

    if (DungeonInteractions::GetHeldBundleItemId() == 0u &&
        DungeonInteractions::FindNearestItemByModel(x, y, itemSearchRadius, modelId) == 0u) {
        if (preferLooseChestSignpost) {
            const uint32_t centerSignpostId =
                ResolveBundleCenterGenericSignpostNearPoint(x, y, signpostSearchRadius);
            if (centerSignpostId != 0u && centerSignpostId != signpostId) {
                (void)MoveToPickupPointIfPossible(x, y);
                MoveNearSignpostIfPossible(centerSignpostId);
                ClearTargetAndStop();
                const bool queued = AgentMgr::InteractAgentWorldAction(centerSignpostId, true);
                Log::Info(
                    "DungeonBundle: bundle-acquire center-followup world-action signpost=%u queued=%d center=(%.0f, %.0f)",
                    centerSignpostId,
                    queued ? 1 : 0,
                    x,
                    y);
                Sleep(chestDelayMs);
                LogBundleAcquireSnapshot("bundle-acquire after-center-followup", x, y, itemSearchRadius, modelId);
                LogNearbyGadgets("bundle-acquire after-center-followup", x, y, signpostSearchRadius);
            }
        }

        const uint32_t followupSignpostId =
            ResolveBundleFollowupChestSignpostNearPoint(x, y, signpostSearchRadius);
        if (followupSignpostId != 0u && followupSignpostId != signpostId) {
            Log::Info(
                "DungeonBundle: bundle-acquire followup chest signpost=%u initial=%u center=(%.0f, %.0f)",
                followupSignpostId,
                signpostId,
                x,
                y);
            if (OpenBundleChestAtPoint(followupSignpostId, x, y, interactCount, interactDelayMs)) {
                Sleep(chestDelayMs);
                LogBundleAcquireSnapshot("bundle-acquire after-followup-open", x, y, itemSearchRadius, modelId);
                LogNearbyGadgets("bundle-acquire after-followup-open", x, y, signpostSearchRadius);
            }
        }
    }

    for (int attempt = 0; attempt < pickupAttempts; ++attempt) {
        (void)MoveToPickupPointIfPossible(x, y);
        if (PickUpHeldBundleByModelNearPoint(x, y, modelId, itemSearchRadius, 1, pickupDelayMs)) {
            Log::Info(
                "DungeonBundle: bundle-acquire direct pickup succeeded attempt=%d heldBundle=%u",
                attempt + 1,
                DungeonInteractions::GetHeldBundleItemId());
            return true;
        }
        Log::Info(
            "DungeonBundle: bundle-acquire direct pickup failed attempt=%d heldBundle=%u",
            attempt + 1,
            DungeonInteractions::GetHeldBundleItemId());
        LogBundleAcquireSnapshot("bundle-acquire after-direct-fail", x, y, itemSearchRadius, modelId);

        const int nearbyPicked = PickUpNearbyLootAtPoint(x, y, itemSearchRadius, pickupDelayMs);
        if (nearbyPicked > 0 && DungeonInteractions::GetHeldBundleItemId() != 0u) {
            Log::Info(
                "DungeonBundle: bundle-acquire nearby loot succeeded attempt=%d picked=%d heldBundle=%u",
                attempt + 1,
                nearbyPicked,
                DungeonInteractions::GetHeldBundleItemId());
            return true;
        }
        Log::Info(
            "DungeonBundle: bundle-acquire nearby loot result attempt=%d picked=%d heldBundle=%u",
            attempt + 1,
            nearbyPicked,
            DungeonInteractions::GetHeldBundleItemId());
        LogBundleAcquireSnapshot("bundle-acquire after-nearby-pass", x, y, itemSearchRadius, modelId);
    }

    LogBundleAcquireSnapshot("bundle-acquire final", x, y, itemSearchRadius, modelId);
    return DungeonInteractions::GetHeldBundleItemId() != 0u;
}

bool OpenChestAndAcquireHeldBundleByModel(
    float x,
    float y,
    uint32_t modelId,
    float signpostSearchRadius,
    float itemSearchRadius,
    int interactCount,
    int pickupAttempts,
    uint32_t interactDelayMs,
    uint32_t pickupDelayMs) {
    return OpenChestAndAcquireHeldBundleByModelImpl(
        x,
        y,
        modelId,
        signpostSearchRadius,
        itemSearchRadius,
        interactCount,
        pickupAttempts,
        interactDelayMs,
        pickupDelayMs,
        false,
        false);
}

bool OpenChestAndAcquireHeldBundleByModelChestPreferred(
    float x,
    float y,
    uint32_t modelId,
    float signpostSearchRadius,
    float itemSearchRadius,
    int interactCount,
    int pickupAttempts,
    uint32_t interactDelayMs,
    uint32_t pickupDelayMs) {
    return OpenChestAndAcquireHeldBundleByModelImpl(
        x,
        y,
        modelId,
        signpostSearchRadius,
        itemSearchRadius,
        interactCount,
        pickupAttempts,
        interactDelayMs,
        pickupDelayMs,
        false,
        true);
}

bool OpenChestAndAcquireHeldBundleByModelActionInteract(
    float x,
    float y,
    uint32_t modelId,
    float itemSearchRadius,
    int interactCount,
    int pickupAttempts,
    uint32_t interactDelayMs,
    uint32_t pickupDelayMs) {
    if (modelId == 0u || interactCount <= 0 || pickupAttempts <= 0) {
        return false;
    }

    if (DungeonInteractions::GetHeldBundleItemId() != 0u) {
        LogBundleAcquireSnapshot("bundle-acquire action-interact already-held", x, y, itemSearchRadius, modelId);
        return true;
    }

    LogBundleAcquireSnapshot("bundle-acquire action-interact before-open", x, y, itemSearchRadius, modelId);
    LogNearbyGadgets("bundle-acquire action-interact before-open", x, y, 1500.0f);

    (void)MoveToPickupPointIfPossible(x, y);
    if (interactDelayMs > 0u) {
        Sleep(interactDelayMs > 1000u ? interactDelayMs : 1000u);
    }
    ClearTargetAndStop();

    for (int press = 0; press < interactCount; ++press) {
        const bool queued = AgentMgr::ActionInteract();
        Log::Info(
            "DungeonBundle: bundle-acquire action-interact open press=%d center=(%.0f, %.0f) queued=%d",
            press + 1,
            x,
            y,
            queued ? 1 : 0);
        Sleep(interactDelayMs);
        if (queued) {
            break;
        }
    }

    for (int attempt = 0; attempt < pickupAttempts; ++attempt) {
        (void)MoveToPickupPointIfPossible(x, y);
        LogBundleAcquireSnapshot(
            "bundle-acquire action-interact after-move",
            x,
            y,
            itemSearchRadius,
            modelId);

        if (PickUpHeldBundleByModelNearPoint(x, y, modelId, itemSearchRadius, 1, pickupDelayMs)) {
            Log::Info(
                "DungeonBundle: bundle-acquire action-interact direct pickup succeeded attempt=%d heldBundle=%u",
                attempt + 1,
                DungeonInteractions::GetHeldBundleItemId());
            return true;
        }

        Log::Info(
            "DungeonBundle: bundle-acquire action-interact direct pickup failed attempt=%d heldBundle=%u",
            attempt + 1,
            DungeonInteractions::GetHeldBundleItemId());

        const int nearbyPicked = PickUpNearbyLootAtPoint(x, y, itemSearchRadius, pickupDelayMs);
        if (nearbyPicked > 0 && DungeonInteractions::GetHeldBundleItemId() != 0u) {
            Log::Info(
                "DungeonBundle: bundle-acquire action-interact nearby loot succeeded attempt=%d picked=%d heldBundle=%u",
                attempt + 1,
                nearbyPicked,
                DungeonInteractions::GetHeldBundleItemId());
            return true;
        }

        Log::Info(
            "DungeonBundle: bundle-acquire action-interact nearby loot result attempt=%d picked=%d heldBundle=%u",
            attempt + 1,
            nearbyPicked,
            DungeonInteractions::GetHeldBundleItemId());
    }

    LogBundleAcquireSnapshot("bundle-acquire action-interact final", x, y, itemSearchRadius, modelId);
    return DungeonInteractions::GetHeldBundleItemId() != 0u;
}

bool OpenChestAndAcquireHeldBundleByModelLegacy(
    float x,
    float y,
    uint32_t modelId,
    float signpostSearchRadius,
    float itemSearchRadius,
    int interactCount,
    int pickupAttempts,
    uint32_t interactDelayMs,
    uint32_t pickupDelayMs) {
    return OpenChestAndAcquireHeldBundleByModelImpl(
        x,
        y,
        modelId,
        signpostSearchRadius,
        itemSearchRadius,
        interactCount,
        pickupAttempts,
        interactDelayMs,
        pickupDelayMs,
        true,
        false);
}

} // namespace GWA3::DungeonBundle
