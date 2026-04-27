#include <gwa3/dungeon/DungeonQuestRuntime.h>

#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>

namespace GWA3::DungeonQuestRuntime {

namespace {

bool HasDialogFromNpc(uint32_t npcId) {
    if (!DialogMgr::IsDialogOpen()) {
        return false;
    }

    const uint32_t senderId = DialogMgr::GetDialogSenderAgentId();
    return senderId == 0u || senderId == npcId;
}

bool WaitForDialogFromNpc(uint32_t npcId, uint32_t timeoutMs) {
    if (HasDialogFromNpc(npcId)) {
        return true;
    }

    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (HasDialogFromNpc(npcId)) {
            return true;
        }
        Sleep(50u);
    }
    return false;
}

} // namespace

bool SendDialogPlan(
    const DungeonQuest::DialogPlan& plan,
    const DialogExecutionOptions& options) {
    if (!DungeonQuest::IsValidDialogPlan(plan)) {
        return false;
    }

    return DungeonDialog::SendDialogSequenceRepeated(
        plan.dialog_ids,
        plan.dialog_count,
        plan.dialog_repeats,
        options.dialog_delay_ms,
        options.repeat_delay_ms,
        options.max_retries_per_dialog);
}

QuestDialogResult SendDialogAndRefreshQuest(
    uint32_t dialogId,
    uint32_t questId,
    const QuestDialogOptions& options) {
    QuestDialogResult result;
    if (dialogId == 0u || questId == 0u) {
        return result;
    }

    const char* prefix = options.log_prefix != nullptr ? options.log_prefix : "DungeonQuestRuntime";
    const char* label = options.label != nullptr ? options.label : "quest dialog";
    Log::Info("%s: %s sending dialog 0x%X quest=0x%X", prefix, label, dialogId, questId);
    DialogHook::RecordDialogSend(dialogId);
    QuestMgr::Dialog(dialogId);
    result.sent = true;
    if (options.post_dialog_wait_ms > 0u) {
        Sleep(options.post_dialog_wait_ms);
    }

    QuestMgr::RequestQuestInfo(questId);
    if (options.refresh_delay_ms > 0u) {
        Sleep(options.refresh_delay_ms);
    }

    result.quest_present = QuestMgr::GetQuestById(questId) != nullptr;
    result.active_quest_id = QuestMgr::GetActiveQuestId();
    result.last_dialog_id = DialogMgr::GetLastDialogId();
    Log::Info("%s: %s refreshed quest=0x%X present=%d active=0x%X lastDialog=0x%X",
              prefix,
              label,
              questId,
              result.quest_present ? 1 : 0,
              result.active_quest_id,
              result.last_dialog_id);
    return result;
}

DungeonEntryReadyResult WaitForDungeonEntryReady(
    const DungeonEntryReadyOptions& options) {
    DungeonEntryReadyResult result;
    if (options.quest_id == 0u || options.entry_dialog_id == 0u) {
        return result;
    }

    const char* prefix = options.log_prefix != nullptr ? options.log_prefix : "DungeonQuestRuntime";
    const char* label = options.label != nullptr ? options.label : "dungeon entry";
    const DWORD start = GetTickCount();
    DWORD lastRefreshTick = start;
    while ((GetTickCount() - start) < options.timeout_ms) {
        result.quest_present = QuestMgr::GetQuestById(options.quest_id) != nullptr;
        result.active_quest = QuestMgr::GetActiveQuestId() == options.quest_id;
        result.entry_dialog_latched = DialogMgr::GetLastDialogId() == options.entry_dialog_id;
        result.sender_agent_id = DialogMgr::GetDialogSenderAgentId();
        const bool dialogSenderMatches = options.npc_id == 0u ||
                                         !DialogMgr::IsDialogOpen() ||
                                         result.sender_agent_id == 0u ||
                                         result.sender_agent_id == options.npc_id;
        result.entry_button_visible = DialogMgr::IsDialogOpen() &&
                                      dialogSenderMatches &&
                                      DungeonDialog::HasDialogButton(options.entry_dialog_id);
        result.ready = result.quest_present ||
                       result.active_quest ||
                       result.entry_dialog_latched ||
                       result.entry_button_visible;
        if (result.ready) {
            Log::Info("%s: %s ready questPresent=%d activeQuest=%d entryLatched=%d entryButtonVisible=%d sender=%u",
                      prefix,
                      label,
                      result.quest_present ? 1 : 0,
                      result.active_quest ? 1 : 0,
                      result.entry_dialog_latched ? 1 : 0,
                      result.entry_button_visible ? 1 : 0,
                      result.sender_agent_id);
            return result;
        }
        if (options.refresh_interval_ms > 0u &&
            (GetTickCount() - lastRefreshTick) >= options.refresh_interval_ms) {
            QuestMgr::RequestQuestInfo(options.quest_id);
            lastRefreshTick = GetTickCount();
        }
        if (options.poll_ms > 0u) {
            Sleep(options.poll_ms);
        }
    }

    Log::Info("%s: %s timeout questPresent=%d activeQuest=%d entryLatched=%d entryButtonVisible=%d sender=%u",
              prefix,
              label,
              result.quest_present ? 1 : 0,
              result.active_quest ? 1 : 0,
              result.entry_dialog_latched ? 1 : 0,
              result.entry_button_visible ? 1 : 0,
              result.sender_agent_id);
    return result;
}

bool InteractNearestNpcAndSendDialogPlan(
    const DungeonQuest::QuestNpcAnchor& npc,
    const DungeonQuest::DialogPlan& plan,
    const DialogExecutionOptions& options) {
    if (!DungeonQuest::IsValidDialogPlan(plan) || npc.search_radius <= 0.0f ||
        options.interact_count <= 0) {
        return false;
    }

    const uint32_t npcId = DungeonInteractions::FindNearestNpc(
        npc.x,
        npc.y,
        npc.search_radius);
    if (npcId == 0u) {
        return false;
    }

    if (options.move_to_actual_npc) {
        auto* npcAgent = AgentMgr::GetAgentByID(npcId);
        if (npcAgent == nullptr) {
            return false;
        }

        const auto moveResult = DungeonNavigation::MoveToAndWait(
            npcAgent->x,
            npcAgent->y,
            options.move_to_npc_tolerance > 0.0f ? options.move_to_npc_tolerance : 120.0f,
            options.move_to_npc_timeout_ms,
            1000u,
            MapMgr::GetMapId());
        if (!moveResult.arrived) {
            Log::Info("DungeonQuestRuntime: failed moving onto NPC agent=%u at (%.0f, %.0f)",
                      npcId,
                      npcAgent->x,
                      npcAgent->y);
            return false;
        }
    }

    if (options.cancel_action_before_interact) {
        AgentMgr::CancelAction();
        if (options.pre_interact_settle_ms > 0u) {
            Sleep(options.pre_interact_settle_ms);
        }
    }

    if (options.clear_dialog_state_before_interact) {
        DialogMgr::ClearDialog();
        DialogMgr::ResetHookState();
    }

    AgentMgr::ChangeTarget(npcId);
    if (options.change_target_delay_ms > 0u) {
        Sleep(options.change_target_delay_ms);
    }

    bool sawDialog = HasDialogFromNpc(npcId);
    for (int attempt = 0; attempt < options.interact_count; ++attempt) {
        if (options.use_direct_npc_interact) {
            CtoS::SendPacketDirect(3, Packets::INTERACT_NPC, npcId, 0u);
        } else {
            AgentMgr::InteractNPC(npcId);
        }
        sawDialog = HasDialogFromNpc(npcId);
        if (sawDialog) {
            break;
        }
        if (attempt + 1 < options.interact_count) {
            Sleep(options.interact_delay_ms);
        }
    }
    Sleep(options.post_interact_delay_ms);

    if (!sawDialog && options.require_dialog_before_send) {
        const uint32_t dialogTimeoutMs =
            options.dialog_wait_timeout_ms > 0u
                ? options.dialog_wait_timeout_ms
                : (options.post_interact_delay_ms > 0u ? options.post_interact_delay_ms : 2000u);
        sawDialog = WaitForDialogFromNpc(npcId, dialogTimeoutMs);
    }

    Log::Info("DungeonQuestRuntime: NPC interaction snapshot npc=%u dialogOpen=%d sender=%u buttons=%u",
              npcId,
              DialogMgr::IsDialogOpen() ? 1 : 0,
              DialogMgr::GetDialogSenderAgentId(),
              DialogMgr::GetButtonCount());

    if (options.require_dialog_before_send && !sawDialog) {
        Log::Info("DungeonQuestRuntime: NPC dialog did not open for npc=%u before sending dialogs", npcId);
        return false;
    }

    AgentMgr::ChangeTarget(npcId);
    if (options.change_target_delay_ms > 0u) {
        Sleep(options.change_target_delay_ms);
    }
    return SendDialogPlan(plan, options);
}

RewardClaimResult TryClaimReward(
    const DungeonQuest::QuestNpcAnchor& npc,
    const DungeonQuest::DialogPlan& plan,
    const RewardClaimOptions& options) {
    RewardClaimResult result;
    result.npc_id = DungeonInteractions::FindNearestNpc(npc.x, npc.y, npc.search_radius);
    result.npc_found = result.npc_id != 0u;
    if (result.npc_found) {
        result.dialog_sent = InteractNearestNpcAndSendDialogPlan(
            npc,
            plan,
            options.execution);
        return result;
    }

    if (options.allow_dialog_without_npc) {
        result.dialog_sent = SendDialogPlan(plan, options.execution);
    }
    return result;
}

BossRewardResult ExecuteBossRewardSequence(
    DungeonInteractions::OpenedChestTracker& tracker,
    const DungeonQuest::QuestNpcAnchor& rewardNpc,
    const DungeonQuest::DialogPlan& rewardDialog,
    const BossRewardOptions& options) {
    BossRewardResult result;
    result.chest_opened = DungeonBundle::TryOpenChestAt(
        options.chest_x,
        options.chest_y,
        options.current_map_id,
        tracker,
        options.chest);

    const RewardClaimResult reward = TryClaimReward(
        rewardNpc,
        rewardDialog,
        options.reward);
    result.reward_dialog_sent = reward.dialog_sent;
    result.reward_npc_found = reward.npc_found;
    return result;
}

bool WaitForQuestState(
    uint32_t questId,
    bool expectPresent,
    const QuestVerificationOptions& options) {
    if (questId == 0u) {
        return !expectPresent;
    }

    auto questMatchesExpectation = [questId, expectPresent, &options]() -> bool {
        auto* quest = QuestMgr::GetQuestById(questId);
        if (!expectPresent) {
            return quest == nullptr;
        }
        if (quest == nullptr) {
            return false;
        }
        if (options.require_not_completed_when_present &&
            (quest->log_state & 0x02u) != 0u) {
            return false;
        }
        return true;
    };

    auto finalizePresence = [&options, questId]() {
        if (!options.set_active_when_present || QuestMgr::GetActiveQuestId() == questId) {
            return;
        }
        QuestMgr::SetActiveQuest(questId);
        if (options.post_set_active_delay_ms > 0u) {
            Sleep(options.post_set_active_delay_ms);
        }
    };

    if (questMatchesExpectation()) {
        if (expectPresent) {
            finalizePresence();
        }
        return true;
    }

    QuestMgr::RequestQuestInfo(questId);
    if (options.refresh_delay_ms > 0u) {
        Sleep(options.refresh_delay_ms);
    }

    const DWORD start = GetTickCount();
    DWORD lastRefreshTick = start;
    while ((GetTickCount() - start) < options.timeout_ms) {
        if (questMatchesExpectation()) {
            if (expectPresent) {
                finalizePresence();
            }
            auto* quest = QuestMgr::GetQuestById(questId);
            Log::Info("DungeonQuestRuntime: WaitForQuestState quest=0x%X expectPresent=%d result=success active=0x%X",
                      questId,
                      expectPresent ? 1 : 0,
                      QuestMgr::GetActiveQuestId());
            if (quest != nullptr) {
                Log::Info("DungeonQuestRuntime: WaitForQuestState detail quest=0x%X logState=%u completed=%d",
                          questId,
                          quest->log_state,
                          (quest->log_state & 0x02u) != 0u ? 1 : 0);
            }
            return true;
        }
        if (options.refresh_interval_ms > 0u &&
            (GetTickCount() - lastRefreshTick) >= options.refresh_interval_ms) {
            QuestMgr::RequestQuestInfo(questId);
            lastRefreshTick = GetTickCount();
        }
        if (options.poll_ms > 0u) {
            Sleep(options.poll_ms);
        }
    }

    auto* quest = QuestMgr::GetQuestById(questId);
    Log::Info("DungeonQuestRuntime: WaitForQuestState quest=0x%X expectPresent=%d result=timeout active=0x%X present=%d",
              questId,
              expectPresent ? 1 : 0,
              QuestMgr::GetActiveQuestId(),
              quest != nullptr ? 1 : 0);
    if (quest != nullptr) {
        Log::Info("DungeonQuestRuntime: WaitForQuestState timeout detail quest=0x%X logState=%u completed=%d requireActive=%d",
                  questId,
                  quest->log_state,
                  (quest->log_state & 0x02u) != 0u ? 1 : 0,
                  options.require_not_completed_when_present ? 1 : 0);
    }
    return false;
}

uint32_t MakeQuestRewardDialogId(uint32_t questId) {
    return 0x00800000u | ((questId & 0x0FFFu) << 8) | 0x07u;
}

bool AcceptQuestRewardWithRetry(uint32_t questId, uint32_t npcId, uint32_t timeoutMs) {
    if (!QuestMgr::GetQuestById(questId)) {
        return false;
    }

    const uint32_t rewardDialogId = MakeQuestRewardDialogId(questId);
    const auto questGoneStable = [questId](uint32_t stableMs) {
        const DWORD stableStart = GetTickCount();
        while ((GetTickCount() - stableStart) < stableMs) {
            QuestMgr::RequestQuestInfo(questId);
            Sleep(250u);
            if (QuestMgr::GetQuestById(questId)) {
                return false;
            }
        }
        return true;
    };
    const DWORD start = GetTickCount();
    do {
        if (npcId != 0u) {
            AgentMgr::ChangeTarget(npcId);
            Sleep(150u);
        }
        DialogHook::RecordDialogSend(rewardDialogId);
        QuestMgr::Dialog(rewardDialogId);
        Sleep(200u);
        if (!QuestMgr::GetQuestById(questId)) {
            break;
        }
    } while ((GetTickCount() - start) < timeoutMs);

    QuestMgr::RequestQuestInfo(questId);
    Sleep(500u);
    if (QuestMgr::GetQuestById(questId) != nullptr) {
        return false;
    }
    return questGoneStable(1000u);
}

bool FollowTravelPath(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t expectedMapId,
    float tolerance,
    uint32_t moveTimeoutMs,
    uint32_t moveReissueMs) {
    if (points == nullptr || count <= 0) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        if (!DungeonNavigation::MoveToAndWait(
                points[i].x,
                points[i].y,
                tolerance,
                moveTimeoutMs,
                moveReissueMs,
                expectedMapId).arrived) {
            return false;
        }
    }

    return true;
}

bool ZoneThroughPoint(
    float x,
    float y,
    uint32_t targetMapId,
    uint32_t timeoutMs,
    uint32_t pollMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        AgentMgr::Move(x, y);
        if (MapMgr::GetMapId() == targetMapId) {
            return true;
        }
        if (DungeonNavigation::WaitForMapId(targetMapId, pollMs)) {
            return true;
        }
    }

    return false;
}

bool ExecuteBootstrapPlan(
    const DungeonQuest::BootstrapPlan& plan,
    const DialogExecutionOptions& dialogOptions,
    const BootstrapExecutionOptions& bootstrapOptions) {
    if (!DungeonQuest::IsValidBootstrapPlan(plan)) {
        return false;
    }

    if (!DungeonNavigation::MoveToAndWait(
            plan.npc.x,
            plan.npc.y,
            bootstrapOptions.npc_tolerance,
            bootstrapOptions.move_timeout_ms,
            bootstrapOptions.move_reissue_ms,
            plan.entry_map_id).arrived) {
        return false;
    }

    const DungeonQuest::DialogPlan dialogPlan = {
        plan.dialog_ids,
        plan.dialog_count,
        plan.dialog_repeats,
    };
    if (!InteractNearestNpcAndSendDialogPlan(plan.npc, dialogPlan, dialogOptions)) {
        return false;
    }

    if (!FollowTravelPath(
            plan.entry_path,
            plan.entry_path_count,
            plan.entry_map_id,
            bootstrapOptions.path_tolerance,
            bootstrapOptions.move_timeout_ms,
            bootstrapOptions.move_reissue_ms)) {
        return false;
    }

    const auto zonePoint = DungeonQuest::ResolveBootstrapZonePoint(plan);
    if (zonePoint.x == 0.0f && zonePoint.y == 0.0f) {
        return false;
    }

    return ZoneThroughPoint(
        zonePoint.x,
        zonePoint.y,
        plan.target_map_id,
        bootstrapOptions.zone_timeout_ms,
        bootstrapOptions.zone_poll_ms);
}

} // namespace GWA3::DungeonQuestRuntime
