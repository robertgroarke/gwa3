#include <gwa3/dungeon/DungeonEffects.h>

#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/game/SkillIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/EffectMgr.h>
#include <gwa3/managers/PlayerMgr.h>

#include <Windows.h>

namespace GWA3::DungeonEffects {

namespace {

uint32_t ResolveAgentId(uint32_t agentId) {
    return agentId != 0u ? agentId : AgentMgr::GetMyId();
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
    const uint32_t resolvedAgentId = ResolveAgentId(agentId);
    if (resolvedAgentId == 0u) return false;

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
    for (const uint32_t effectId : kDungeonBlessingEffects) {
        if (EffectMgr::HasEffect(resolvedAgentId, effectId)) {
            return true;
        }
    }
    return false;
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

    result.npc_id = DungeonInteractions::FindNearestNpc(shrineX, shrineY, options.npc_search_radius);
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

    result.dialog_sent = DungeonDialog::SendDialogWithRetry(
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

} // namespace GWA3::DungeonEffects
