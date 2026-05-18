#include <gwa3/advanced/Effects.h>

#include <gwa3/core/Log.h>
#include <gwa3/advanced/Dialog.h>
#include <gwa3/advanced/Interactions.h>
#include <gwa3/advanced/Waypoint.h>
#include <gwa3/game/SkillIds.h>
#include <gwa3/game/Title.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/PlayerMgr.h>

#include <Windows.h>

#include <cstddef>

namespace GWA3::AdvancedEffects {

namespace {

uint32_t ResolveAgentId(uint32_t agentId) {
    return agentId != 0u ? agentId : AgentMgr::GetMyId();
}

bool HasAnyEffect(uint32_t agentId, const uint32_t* effectIds, size_t effectCount) {
    const uint32_t resolvedAgentId = ResolveAgentId(agentId);
    if (resolvedAgentId == 0u || effectIds == nullptr) return false;
    for (size_t i = 0u; i < effectCount; ++i) {
        if (EffectMgr::HasEffect(resolvedAgentId, effectIds[i])) {
            return true;
        }
    }
    return false;
}

bool HasRequiredBlessing(uint32_t titleId, bool requireSpecific) {
    return requireSpecific
        ? HasDungeonBlessingForTitle(titleId)
        : HasAnyDungeonBlessing();
}

struct BlessingStopContext {
    uint32_t title_id = 0u;
    bool require_specific = false;
};

bool BlessingStopCondition(void* context) {
    const auto* stop = static_cast<const BlessingStopContext*>(context);
    return stop != nullptr && HasRequiredBlessing(stop->title_id, stop->require_specific);
}

} // namespace

uint32_t GetPlayerEffectCount() {
    auto* effects = EffectMgr::GetPlayerEffects();
    if (!effects) return 0u;
    return effects->effects.size;
}

ActiveTitleEnsureResult EnsureActiveTitle(uint32_t titleId, uint32_t settleDelayMs, WaitFn wait_ms) {
    ActiveTitleEnsureResult result;
    result.previous_title_id = PlayerMgr::GetActiveTitleId();
    result.final_title_id = result.previous_title_id;
    if (titleId == 0u || result.previous_title_id == titleId) {
        result.already_active = (titleId != 0u && result.previous_title_id == titleId);
        return result;
    }

    result.applied = PlayerMgr::SetActiveTitle(titleId);
    if (result.applied && settleDelayMs > 0u) {
        if (wait_ms) {
            wait_ms(settleDelayMs);
        } else {
            Sleep(settleDelayMs);
        }
    }
    result.final_title_id = PlayerMgr::GetActiveTitleId();
    return result;
}

bool HasAnyDungeonBlessing(uint32_t agentId) {
    static constexpr uint32_t kDungeonBlessingEffects[] = {
        GWA3::SkillIds::ASURAN_BODYGUARD,
        GWA3::SkillIds::ASURAN_BODYGUARD_ID_2435,
        GWA3::SkillIds::ASURAN_BODYGUARD_ID_2436,
        GWA3::SkillIds::ASURAN_BODYGUARD_ID_2481,
        GWA3::SkillIds::DWARVEN_RAIDER,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2446,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2447,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2448,
        GWA3::SkillIds::GREAT_DWARFS_BLESSING,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2565,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2566,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2567,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2568,
        GWA3::SkillIds::GREAT_DWARFS_BLESSING_ID_2570,
        GWA3::SkillIds::VANGUARD_PATROL,
        GWA3::SkillIds::VANGUARD_PATROL_ID_2458,
        GWA3::SkillIds::VANGUARD_PATROL_ID_2459,
        GWA3::SkillIds::VANGUARD_PATROL_ID_2460,
        GWA3::SkillIds::VANGUARD_PATROL_ID_2578,
        GWA3::SkillIds::NORN_HUNTING_PARTY,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2470,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2471,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2472,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2591,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2592,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2593,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2594,
        GWA3::SkillIds::VETERAN_ASURAN_BODYGUARD,
        GWA3::SkillIds::VETERAN_DWARVEN_RAIDER,
        GWA3::SkillIds::VETERAN_VANGUARD_PATROL,
        GWA3::SkillIds::VETERAN_NORN_HUNTING_PARTY,
    };
    return HasAnyEffect(agentId, kDungeonBlessingEffects, sizeof(kDungeonBlessingEffects) / sizeof(kDungeonBlessingEffects[0]));
}

bool HasDungeonBlessingForTitle(uint32_t titleId, uint32_t agentId) {
    static constexpr uint32_t kAsuranEffects[] = {
        GWA3::SkillIds::ASURAN_BODYGUARD,
        GWA3::SkillIds::ASURAN_BODYGUARD_ID_2435,
        GWA3::SkillIds::ASURAN_BODYGUARD_ID_2436,
        GWA3::SkillIds::ASURAN_BODYGUARD_ID_2481,
        GWA3::SkillIds::VETERAN_ASURAN_BODYGUARD,
    };
    static constexpr uint32_t kDeldrimorEffects[] = {
        GWA3::SkillIds::DWARVEN_RAIDER,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2446,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2447,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2448,
        GWA3::SkillIds::VETERAN_DWARVEN_RAIDER,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2565,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2566,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2567,
        GWA3::SkillIds::DWARVEN_RAIDER_ID_2568,
        GWA3::SkillIds::GREAT_DWARFS_BLESSING,
        GWA3::SkillIds::GREAT_DWARFS_BLESSING_ID_2570,
    };
    static constexpr uint32_t kVanguardEffects[] = {
        GWA3::SkillIds::VANGUARD_PATROL,
        GWA3::SkillIds::VANGUARD_PATROL_ID_2458,
        GWA3::SkillIds::VANGUARD_PATROL_ID_2459,
        GWA3::SkillIds::VANGUARD_PATROL_ID_2460,
        GWA3::SkillIds::VANGUARD_PATROL_ID_2578,
        GWA3::SkillIds::VETERAN_VANGUARD_PATROL,
    };
    static constexpr uint32_t kNornEffects[] = {
        GWA3::SkillIds::NORN_HUNTING_PARTY,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2470,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2471,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2472,
        GWA3::SkillIds::VETERAN_NORN_HUNTING_PARTY,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2591,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2592,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2593,
        GWA3::SkillIds::NORN_HUNTING_PARTY_ID_2594,
    };

    switch (titleId) {
    case GWA3::TitleID::Asura:
        return HasAnyEffect(agentId, kAsuranEffects, sizeof(kAsuranEffects) / sizeof(kAsuranEffects[0]));
    case GWA3::TitleID::Deldrimor:
        return HasAnyEffect(agentId, kDeldrimorEffects, sizeof(kDeldrimorEffects) / sizeof(kDeldrimorEffects[0]));
    case GWA3::TitleID::Vanguard:
        return HasAnyEffect(agentId, kVanguardEffects, sizeof(kVanguardEffects) / sizeof(kVanguardEffects[0]));
    case GWA3::TitleID::Norn:
        return HasAnyEffect(agentId, kNornEffects, sizeof(kNornEffects) / sizeof(kNornEffects[0]));
    default:
        return HasAnyDungeonBlessing(agentId);
    }
}

bool HasBlessing() {
    return HasAnyDungeonBlessing();
}

bool HasFullConset(uint32_t agentId) {
    const uint32_t resolvedAgentId = ResolveAgentId(agentId);
    if (resolvedAgentId == 0u) return false;
    return EffectMgr::HasEffect(resolvedAgentId, GWA3::SkillIds::ARMOR_OF_SALVATION_ITEM_EFFECT) &&
           EffectMgr::HasEffect(resolvedAgentId, GWA3::SkillIds::ESSENCE_OF_CELERITY_ITEM_EFFECT) &&
           EffectMgr::HasEffect(resolvedAgentId, GWA3::SkillIds::GRAIL_OF_MIGHT_ITEM_EFFECT);
}

BlessingAcquireResult TryAcquireBlessingAt(
    float shrineX,
    float shrineY,
    const BlessingAcquireOptions& options) {
    BlessingAcquireResult result;
    result.final_title_id = PlayerMgr::GetActiveTitleId();

    if (HasBlessing()) {
        result.already_active = true;
        result.confirmed = true;
        return result;
    }

    if (result.final_title_id == 0u && options.required_title_id != 0u) {
        const auto titleResult = EnsureActiveTitle(
            options.required_title_id,
            options.title_settle_delay_ms);
        result.title_applied = titleResult.applied;
        result.final_title_id = titleResult.final_title_id;
    }

    result.npc_id = AdvancedInteractions::FindNearestNpc(shrineX, shrineY, options.npc_search_radius);
    result.npc_found = result.npc_id != 0u;
    if (!result.npc_found) {
        return result;
    }

    if (options.toggle_dialog_hooks) {
        DialogMgr::Shutdown();
        result.dialog_hooks_toggled = true;
    }

    for (int attempt = 0; attempt < options.interact_count; ++attempt) {
        AgentMgr::InteractNPC(result.npc_id);
        result.interacted = true;
        Sleep(options.interact_delay_ms);
    }

    result.dialog_sent = AdvancedDialog::SendDialogWithRetry(
        options.accept_dialog_id,
        options.dialog_retries,
        options.dialog_delay_ms);

    if (options.toggle_dialog_hooks) {
        DialogMgr::Initialize();
    }

    result.confirmed = HasBlessing();
    result.final_title_id = PlayerMgr::GetActiveTitleId();
    return result;
}

BlessingAcquireResult AcquireDungeonBlessingAt(
    float shrineX,
    float shrineY,
    const BlessingInteractionOptions& options) {
    BlessingAcquireResult result;
    result.final_title_id = PlayerMgr::GetActiveTitleId();
    const char* prefix = options.log_prefix != nullptr ? options.log_prefix : "Dungeon blessing";

    if (HasRequiredBlessing(options.required_title_id, options.require_specific_blessing)) {
        result.already_active = true;
        result.confirmed = true;
        Log::Info("%s: already active, skipping", prefix);
        return result;
    }

    const auto titleResult = EnsureActiveTitle(
        options.required_title_id,
        options.title_settle_delay_ms,
        options.wait_ms);
    result.title_applied = titleResult.applied;
    result.final_title_id = titleResult.final_title_id;
    if (titleResult.applied) {
        Log::Info("%s: setting title current=0x%X target=0x%X",
                  prefix,
                  titleResult.previous_title_id,
                  options.required_title_id);
    }

    if (options.signpost_scan_log != nullptr) {
        options.signpost_scan_log(
            shrineX,
            shrineY,
            options.fallback_search_radius,
            "Blessing signposts",
            false);
    }

    AdvancedInteractions::InteractCandidate interactCandidates[2] = {};
    const size_t interactCandidateCount = AdvancedInteractions::CollectNearestInteractCandidates(
        shrineX,
        shrineY,
        options.fallback_search_radius,
        options.fallback_search_radius,
        interactCandidates,
        2u);
    if (interactCandidateCount == 0u) {
        Log::Warn("%s: found no interactable NPC or signpost near shrine=(%.0f, %.0f)",
                  prefix,
                  shrineX,
                  shrineY);
        return result;
    }

    result.npc_found = true;
    for (size_t i = 0; i < interactCandidateCount; ++i) {
        const auto& candidate = interactCandidates[i];
        Log::Info("%s: candidate[%u] kind=%s agent=%u distToShrine=%.0f pos=(%.0f, %.0f)",
                  prefix,
                  static_cast<unsigned>(i),
                  candidate.use_signpost ? "signpost" : "npc",
                  candidate.agent_id,
                  candidate.dist_to_anchor,
                  candidate.x,
                  candidate.y);
        if (options.agent_log != nullptr) {
            options.agent_log(candidate.use_signpost ? "Blessing signpost" : "Blessing NPC", candidate.agent_id);
        }
    }

    for (size_t candidateIndex = 0u;
         candidateIndex < interactCandidateCount && !HasRequiredBlessing(options.required_title_id, options.require_specific_blessing);
         ++candidateIndex) {
        const auto& candidate = interactCandidates[candidateIndex];
        const float moveThreshold =
            candidate.use_signpost ? options.signpost_move_threshold : options.npc_move_threshold;
        if (options.move_to_point != nullptr) {
            (void)options.move_to_point(candidate.x, candidate.y, moveThreshold);
        }
        const bool settled = options.wait_for_position_settle != nullptr
            ? options.wait_for_position_settle(options.settle_timeout_ms, options.settle_distance)
            : AdvancedWaypoint::WaitForLocalPositionSettle(options.settle_timeout_ms, options.settle_distance);
        auto* me = AgentMgr::GetMyAgent();
        const float playerDistance = me != nullptr
            ? AgentMgr::GetDistance(me->x, me->y, candidate.x, candidate.y)
            : -1.0f;
        Log::Info("%s: candidate[%u] approach settled=%d playerDist=%.0f player=(%.0f, %.0f) target=%u",
                  prefix,
                  static_cast<unsigned>(candidateIndex),
                  settled ? 1 : 0,
                  playerDistance,
                  me != nullptr ? me->x : 0.0f,
                  me != nullptr ? me->y : 0.0f,
                  AgentMgr::GetTargetId());

        AdvancedInteractions::CandidateDialogOptions dialogOptions;
        BlessingStopContext stopContext;
        stopContext.title_id = options.required_title_id;
        stopContext.require_specific = options.require_specific_blessing;
        dialogOptions.dialog_id = options.accept_dialog_id;
        dialogOptions.candidate_index = candidateIndex;
        dialogOptions.log_prefix = prefix;
        dialogOptions.wait_ms = options.wait_ms;
        dialogOptions.stop_condition_with_context = &BlessingStopCondition;
        dialogOptions.stop_context = &stopContext;
        dialogOptions.interact_attempts = 8;
        dialogOptions.use_legacy_interact_fallbacks = options.use_legacy_interact_fallbacks;
        dialogOptions.send_dialog_without_ready = options.send_dialog_without_ready;
        const auto dialogResult = AdvancedInteractions::InteractCandidateAndSendDialog(candidate, dialogOptions);
        result.interacted = dialogResult.interacted || result.interacted;
        result.dialog_sent = dialogResult.dialog_sent || result.dialog_sent;
    }

    result.confirmed = HasRequiredBlessing(options.required_title_id, options.require_specific_blessing);
    if (result.confirmed) {
        Log::Info("%s: confirmed active", prefix);
    } else {
        Log::Warn("%s: effect not detected after shrine interaction", prefix);
    }
    result.final_title_id = PlayerMgr::GetActiveTitleId();
    return result;
}

} // namespace GWA3::AdvancedEffects
