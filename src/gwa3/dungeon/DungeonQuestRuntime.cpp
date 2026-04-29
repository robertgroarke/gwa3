#include <gwa3/dungeon/DungeonQuestRuntime.h>

#include <gwa3/dungeon/DungeonBundle.h>
#include <gwa3/dungeon/DungeonDiagnostics.h>
#include <gwa3/dungeon/DungeonDialog.h>
#include <gwa3/dungeon/DungeonInteractions.h>
#include <gwa3/dungeon/DungeonNavigation.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ChatMgr.h>
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

const char* PrefixOrDefault(const char* prefix) {
    return prefix != nullptr ? prefix : "DungeonQuestRuntime";
}

const char* LabelOrDefault(const char* label, const char* fallback) {
    return label != nullptr ? label : fallback;
}

bool IsQuestReadyForDungeonEntry(uint32_t questId, bool questPresent, uint32_t activeQuestId) {
    return questPresent || (questId != 0u && activeQuestId == questId);
}

void LogQuestSnapshot(uint32_t questId, const char* prefix, const char* label) {
    Quest* quest = QuestMgr::GetQuestById(questId);
    Log::Info("%s: %s activeQuest=0x%X questPresent=%d questLogSize=%u lastDialog=0x%X",
              prefix,
              label,
              QuestMgr::GetActiveQuestId(),
              quest != nullptr ? 1 : 0,
              QuestMgr::GetQuestLogSize(),
              DialogMgr::GetLastDialogId());
    if (quest != nullptr) {
        Log::Info("%s: %s quest: id=0x%X logState=%u map_from=%u map_to=%u marker=(%.0f, %.0f)",
                  prefix,
                  label,
                  quest->quest_id,
                  quest->log_state,
                  quest->map_from,
                  quest->map_to,
                  quest->marker_x,
                  quest->marker_y);
    }
}

DungeonInteractions::DirectNpcInteractOptions MakeDirectInteractOptions(
    uint32_t targetWaitMs,
    uint32_t passWaitMs,
    int passes,
    const char* prefix,
    const char* label) {
    DungeonInteractions::DirectNpcInteractOptions interactOptions = {};
    interactOptions.target_wait_ms = targetWaitMs;
    interactOptions.pass_wait_ms = passWaitMs;
    interactOptions.passes = passes;
    interactOptions.log_prefix = prefix;
    interactOptions.label = label;
    return interactOptions;
}

QuestGiverDialogSnapshot CaptureQuestGiverDialogSnapshot(
    uint32_t questId,
    const QuestGiverDialogIds& dialogs) {
    QuestGiverDialogSnapshot snapshot;
    snapshot.ping = ChatMgr::GetPing();
    snapshot.quest_present = QuestMgr::GetQuestById(questId) != nullptr;
    snapshot.dialog = DungeonDialog::CaptureDialogSnapshot();
    snapshot.has_dungeon_entry = DungeonDialog::HasDialogButton(dialogs.dungeon_entry);
    snapshot.has_talk = DungeonDialog::HasDialogButton(dialogs.talk);
    snapshot.has_accept = DungeonDialog::HasDialogButton(dialogs.accept);
    snapshot.has_reward = DungeonDialog::HasDialogButton(dialogs.reward);
    return snapshot;
}

void LogQuestGiverDialogSnapshot(
    const QuestGiverDialogSnapshot& snapshot,
    const char* prefix,
    const char* label) {
    Log::Info("%s: %s dialog snapshot visible=%d sender=%u buttons=%u hasReward=%d hasAccept=%d hasTalk=%d hasDungeonEntry=%d",
              prefix,
              label,
              snapshot.dialog.dialog_open ? 1 : 0,
              snapshot.dialog.sender_agent_id,
              snapshot.dialog.button_count,
              snapshot.has_reward ? 1 : 0,
              snapshot.has_accept ? 1 : 0,
              snapshot.has_talk ? 1 : 0,
              snapshot.has_dungeon_entry ? 1 : 0);
}

QuestDialogResult SendQuestGiverDialog(
    uint32_t dialogId,
    uint32_t questId,
    const char* label,
    uint32_t postDialogWaitMs,
    uint32_t refreshDelayMs,
    const char* prefix) {
    QuestDialogOptions dialogOptions = {};
    dialogOptions.post_dialog_wait_ms = postDialogWaitMs;
    dialogOptions.refresh_delay_ms = refreshDelayMs;
    dialogOptions.log_prefix = prefix;
    dialogOptions.label = label;
    return SendDialogAndRefreshQuest(dialogId, questId, dialogOptions);
}

void AdvancePostRewardDialog(
    uint32_t npcId,
    uint32_t acceptDialogId,
    uint32_t ping,
    const QuestGiverEntryOptions& options,
    const char* prefix,
    const char* label) {
    DungeonDialog::DialogAdvanceOptions advanceOptions = {};
    advanceOptions.sender_agent_id = npcId;
    advanceOptions.post_dialog_wait_ms = options.post_reward_wait_base_ms + ping;
    advanceOptions.max_buttons_per_pass = options.post_reward_max_buttons_per_pass;
    advanceOptions.max_passes = options.post_reward_max_passes;
    advanceOptions.log_prefix = prefix;
    advanceOptions.label = label;
    (void)DungeonDialog::AdvanceDialogToButton(acceptDialogId, advanceOptions);
}

void ReopenAcceptAfterReward(
    uint32_t npcId,
    const QuestGiverDialogIds& dialogs,
    uint32_t ping,
    const QuestGiverEntryOptions& options,
    const char* prefix,
    const char* label) {
    auto acceptInteract = MakeDirectInteractOptions(
        options.reopen_accept_target_wait_base_ms + ping,
        options.reopen_accept_pass_wait_ms,
        options.reopen_accept_interact_passes,
        prefix,
        label);
    for (int attempt = 0; attempt < options.reopen_accept_attempts; ++attempt) {
        (void)DungeonInteractions::PulseDirectNpcInteract(npcId, acceptInteract);
        if (!(DialogMgr::IsDialogOpen() && DialogMgr::GetDialogSenderAgentId() == npcId)) {
            continue;
        }
        if (DungeonDialog::HasDialogButton(dialogs.accept)) {
            break;
        }
        AdvancePostRewardDialog(npcId, dialogs.accept, ping, options, prefix, "post-reward");
        if (DungeonDialog::HasDialogButton(dialogs.accept)) {
            break;
        }
    }
}

bool VerifyDungeonEntryReady(
    uint32_t questId,
    uint32_t entryDialogId,
    uint32_t npcId,
    uint32_t timeoutMs,
    const QuestGiverEntryOptions& options,
    const char* prefix,
    const char* label) {
    DungeonEntryReadyOptions readyOptions = {};
    readyOptions.quest_id = questId;
    readyOptions.entry_dialog_id = entryDialogId;
    readyOptions.npc_id = npcId;
    readyOptions.timeout_ms = timeoutMs;
    readyOptions.refresh_interval_ms = options.entry_verify_refresh_interval_ms;
    readyOptions.poll_ms = options.entry_verify_poll_ms;
    readyOptions.log_prefix = prefix;
    readyOptions.label = label;
    return WaitForDungeonEntryReady(readyOptions).ready;
}

bool StopWhenBossRewardDialogReady(uint32_t npcId, void* context) {
    const auto* options = static_cast<const BossRewardClaimOptions*>(context);
    if (options == nullptr || options->reward_dialog_id == 0u) {
        return false;
    }
    return DungeonDialog::IsDialogOpenFromSenderWithButton(npcId, options->reward_dialog_id);
}

QuestGiverEntryResult AcceptQuestAndEnter(
    uint32_t npcId,
    const QuestGiverEntryPlan& plan,
    uint32_t ping,
    const QuestGiverEntryOptions& options,
    const char* prefix,
    const char* label) {
    QuestGiverEntryResult result;
    result.npc_found = true;
    result.npc_id = npcId;

    Log::Info("%s: %s sending accept dialog 0x%X", prefix, label, plan.dialogs.accept);
    DialogHook::RecordDialogSend(plan.dialogs.accept);
    QuestMgr::Dialog(plan.dialogs.accept);
    Sleep(options.accept_wait_base_ms + ping);

    QuestVerificationOptions acceptVerify = options.accept_verify;
    if (acceptVerify.refresh_delay_ms == 0u) {
        acceptVerify.refresh_delay_ms = options.dialog_refresh_delay_ms;
    }
    const bool acceptedQuestPresent = WaitForQuestState(plan.quest_id, true, acceptVerify);
    if (acceptedQuestPresent && QuestMgr::GetQuestById(plan.quest_id) != nullptr) {
        QuestMgr::SetActiveQuest(plan.quest_id);
        Sleep(options.post_set_active_delay_ms);
    }
    LogQuestSnapshot(plan.quest_id, prefix, "accept snapshot");

    QuestReadyOptions readyOptions = {};
    readyOptions.refresh_delay_ms = options.dialog_refresh_delay_ms;
    readyOptions.post_set_active_delay_ms = options.post_set_active_delay_ms;
    readyOptions.log_prefix = prefix;
    readyOptions.label = "accept verification";
    const QuestReadyResult readyAfterAccept = RefreshQuestReadyForDungeonEntry(plan.quest_id, readyOptions);
    if (!readyAfterAccept.ready) {
        Log::Info("%s: %s accept did not place quest in log; refusing dungeon-entry dialog", prefix, label);
        result.quest_present = readyAfterAccept.quest_present;
        result.active_quest_id = readyAfterAccept.active_quest_id;
        result.last_dialog_id = DialogMgr::GetLastDialogId();
        return result;
    }

    Log::Info("%s: %s sending talk dialog 0x%X", prefix, label, plan.dialogs.talk);
    DialogHook::RecordDialogSend(plan.dialogs.talk);
    QuestMgr::Dialog(plan.dialogs.talk);
    Sleep(options.talk_wait_base_ms + ping);
    Log::Info("%s: %s after Dialog(0x%X) lastDialog=0x%X",
              prefix,
              label,
              plan.dialogs.talk,
              DialogMgr::GetLastDialogId());

    (void)SendQuestGiverDialog(
        plan.dialogs.dungeon_entry,
        plan.quest_id,
        "dungeon-entry",
        options.entry_dialog_wait_base_ms + ping,
        options.dialog_refresh_delay_ms,
        prefix);
    LogQuestSnapshot(plan.quest_id, prefix, "dungeon-entry complete snapshot");

    result.quest_present = QuestMgr::GetQuestById(plan.quest_id) != nullptr;
    result.active_quest_id = QuestMgr::GetActiveQuestId();
    result.last_dialog_id = DialogMgr::GetLastDialogId();
    result.entry_ready = VerifyDungeonEntryReady(
        plan.quest_id,
        plan.dialogs.dungeon_entry,
        npcId,
        options.entry_verify_wait_base_ms + ping,
        options,
        prefix,
        "entry verify");
    result.confirmed = result.entry_ready &&
                       IsQuestReadyForDungeonEntry(plan.quest_id, result.quest_present, result.active_quest_id);
    Log::Info("%s: %s dungeon entry sequence complete questPresent=%d activeQuest=0x%X lastDialog=0x%X entryReady=%d confirmed=%d",
              prefix,
              label,
              result.quest_present ? 1 : 0,
              result.active_quest_id,
              result.last_dialog_id,
              result.entry_ready ? 1 : 0,
              result.confirmed ? 1 : 0);
    return result;
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

QuestReadyResult RefreshQuestReadyForDungeonEntry(
    uint32_t questId,
    const QuestReadyOptions& options) {
    QuestReadyResult result;
    if (questId == 0u) {
        return result;
    }

    const char* prefix = PrefixOrDefault(options.log_prefix);
    const char* label = LabelOrDefault(options.label, "quest ready");
    QuestMgr::RequestQuestInfo(questId);
    if (options.refresh_delay_ms > 0u) {
        Sleep(options.refresh_delay_ms);
    }

    Quest* quest = QuestMgr::GetQuestById(questId);
    result.active_quest_id = QuestMgr::GetActiveQuestId();
    if (options.set_active_when_present && quest != nullptr && result.active_quest_id != questId) {
        QuestMgr::SetActiveQuest(questId);
        if (options.post_set_active_delay_ms > 0u) {
            Sleep(options.post_set_active_delay_ms);
        }
        result.active_quest_id = QuestMgr::GetActiveQuestId();
        quest = QuestMgr::GetQuestById(questId);
    }

    result.quest_present = quest != nullptr;
    result.quest_log_size = QuestMgr::GetQuestLogSize();
    result.ready = IsQuestReadyForDungeonEntry(questId, result.quest_present, result.active_quest_id);
    Log::Info("%s: %s quest ready=%d activeQuest=0x%X questPresent=%d questLogSize=%u",
              prefix,
              label,
              result.ready ? 1 : 0,
              result.active_quest_id,
              result.quest_present ? 1 : 0,
              result.quest_log_size);
    return result;
}

QuestGiverEntryResult PrepareDungeonEntryFromQuestGiver(
    const QuestGiverEntryPlan& plan,
    const QuestGiverEntryOptions& options) {
    QuestGiverEntryResult result;
    if (plan.quest_id == 0u || plan.dialogs.accept == 0u || plan.dialogs.dungeon_entry == 0u ||
        plan.npc.search_radius <= 0.0f) {
        return result;
    }

    const char* prefix = PrefixOrDefault(options.log_prefix);
    const char* label = LabelOrDefault(options.label, "quest giver");
    Log::Info("%s: Preparing %s dungeon entry sequence", prefix, label);

    (void)DungeonNavigation::MoveToAndWait(
        plan.npc.x,
        plan.npc.y,
        options.anchor_move_threshold,
        DungeonNavigation::MOVE_TO_TIMEOUT_MS,
        DungeonNavigation::MOVE_TO_POLL_MS,
        MapMgr::GetMapId());
    if (options.pre_interact_dwell_ms > 0u) {
        Sleep(options.pre_interact_dwell_ms);
    }
    AgentMgr::CancelAction();
    if (options.cancel_dwell_ms > 0u) {
        Sleep(options.cancel_dwell_ms);
    }

    const uint32_t npcId = DungeonInteractions::FindNearestNpc(
        plan.npc.x,
        plan.npc.y,
        plan.npc.search_radius);
    if (npcId == 0u) {
        Log::Info("%s: %s NPC not found near (%.0f, %.0f)", prefix, label, plan.npc.x, plan.npc.y);
        return result;
    }

    result.npc_found = true;
    result.npc_id = npcId;
    Log::Info("%s: %s NPC found agent=%u", prefix, label, npcId);
    LogQuestSnapshot(plan.quest_id, prefix, "pre-interact snapshot");

    auto* npc = AgentMgr::GetAgentByID(npcId);
    if (npc != nullptr) {
        (void)DungeonNavigation::MoveToAndWait(
            npc->x,
            npc->y,
            options.npc_move_threshold,
            DungeonNavigation::MOVE_TO_TIMEOUT_MS,
            DungeonNavigation::MOVE_TO_POLL_MS,
            MapMgr::GetMapId());
        DungeonNavigation::WaitForLocalPositionSettle(
            options.npc_settle_timeout_ms,
            options.npc_settle_distance);
    }

    {
        auto* me = AgentMgr::GetMyAgent();
        Log::Info("%s: %s PRE-GoNPC pos=(%.0f, %.0f) npc=(%.0f, %.0f) dist=%.0f",
                  prefix,
                  label,
                  me ? me->x : 0.0f,
                  me ? me->y : 0.0f,
                  npc ? npc->x : 0.0f,
                  npc ? npc->y : 0.0f,
                  (me != nullptr && npc != nullptr) ? AgentMgr::GetDistance(me->x, me->y, npc->x, npc->y) : -1.0f);
    }

    auto initialInteract = MakeDirectInteractOptions(
        options.initial_interact_target_wait_ms,
        options.initial_interact_pass_wait_ms,
        options.initial_interact_passes,
        prefix,
        label);
    (void)DungeonInteractions::PulseDirectNpcInteract(npcId, initialInteract);

    if (options.post_interact_dwell_ms > 0u) {
        Sleep(options.post_interact_dwell_ms);
    }

    {
        auto* me = AgentMgr::GetMyAgent();
        npc = AgentMgr::GetAgentByID(npcId);
        const float dist = (me != nullptr && npc != nullptr)
            ? AgentMgr::GetDistance(me->x, me->y, npc->x, npc->y)
            : -1.0f;
        Log::Info("%s: %s POST-GoNPC pos=(%.0f, %.0f) dist=%.0f dialogOpen=%d buttons=%u sender=%u lastDialog=0x%X",
                  prefix,
                  label,
                  me ? me->x : 0.0f,
                  me ? me->y : 0.0f,
                  dist,
                  DialogMgr::IsDialogOpen() ? 1 : 0,
                  DialogMgr::GetButtonCount(),
                  DialogMgr::GetDialogSenderAgentId(),
                  DialogMgr::GetLastDialogId());
    }

    const QuestGiverDialogSnapshot snapshot = CaptureQuestGiverDialogSnapshot(plan.quest_id, plan.dialogs);
    const uint32_t ping = snapshot.ping;
    LogQuestGiverDialogSnapshot(snapshot, prefix, label);

    if (snapshot.dialog.dialog_open &&
        snapshot.dialog.sender_agent_id == npcId &&
        snapshot.has_dungeon_entry) {
        QuestReadyOptions readyOptions = {};
        readyOptions.refresh_delay_ms = options.dialog_refresh_delay_ms;
        readyOptions.post_set_active_delay_ms = options.post_set_active_delay_ms;
        readyOptions.log_prefix = prefix;
        readyOptions.label = "direct dungeon-entry precheck";
        const QuestReadyResult questReady = RefreshQuestReadyForDungeonEntry(plan.quest_id, readyOptions);
        if (!questReady.ready) {
            Log::Info("%s: %s direct dungeon-entry button visible without active quest; retrying quest flow later",
                      prefix,
                      label);
            result.quest_present = questReady.quest_present;
            result.active_quest_id = questReady.active_quest_id;
            result.last_dialog_id = DialogMgr::GetLastDialogId();
            return result;
        }

        const QuestDialogResult entryResult = SendQuestGiverDialog(
            plan.dialogs.dungeon_entry,
            plan.quest_id,
            "direct dungeon-entry",
            options.direct_entry_wait_base_ms + ping,
            options.dialog_refresh_delay_ms,
            prefix);
        LogQuestSnapshot(plan.quest_id, prefix, "direct dungeon-entry snapshot");
        result.quest_present = entryResult.quest_present;
        result.active_quest_id = entryResult.active_quest_id;
        result.last_dialog_id = entryResult.last_dialog_id;
        result.entry_ready = entryResult.last_dialog_id == plan.dialogs.dungeon_entry;
        result.confirmed = result.entry_ready &&
                           IsQuestReadyForDungeonEntry(plan.quest_id, result.quest_present, result.active_quest_id);
        Log::Info("%s: %s direct dungeon-entry complete questPresent=%d activeQuest=0x%X lastDialog=0x%X confirmed=%d",
                  prefix,
                  label,
                  result.quest_present ? 1 : 0,
                  result.active_quest_id,
                  result.last_dialog_id,
                  result.confirmed ? 1 : 0);
        return result;
    }

    if (snapshot.quest_present && plan.dialogs.reward != 0u) {
        const QuestDialogResult rewardResult = SendQuestGiverDialog(
            plan.dialogs.reward,
            plan.quest_id,
            "reward-first",
            options.reward_first_wait_base_ms + ping,
            options.dialog_refresh_delay_ms,
            prefix);
        const bool clearedAfterReward = !rewardResult.quest_present;
        Log::Info("%s: %s reward-first snapshot clearedAfterReward=%d",
                  prefix,
                  label,
                  clearedAfterReward ? 1 : 0);
        if (clearedAfterReward) {
            ReopenAcceptAfterReward(
                npcId,
                plan.dialogs,
                ping,
                options,
                prefix,
                "re-interact for new accept");
        }
    }

    return AcceptQuestAndEnter(npcId, plan, ping, options, prefix, label);
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

RewardNpcStageResult StageRewardNpcInteraction(
    const DungeonQuest::QuestNpcAnchor& rewardNpc,
    const RewardNpcStageOptions& options) {
    RewardNpcStageResult result = {};
    const char* prefix = options.log_prefix != nullptr ? options.log_prefix : "DungeonQuestRuntime";
    DungeonNavigation::LoggedMoveOptions moveOptions;
    moveOptions.log_prefix = prefix;
    moveOptions.is_dead = options.is_dead;
    result.reached = DungeonNavigation::MoveToAndWaitLogged(
        rewardNpc.x,
        rewardNpc.y,
        options.move_threshold,
        moveOptions);
    if (!result.reached) {
        Log::Warn("%s: Boss reward staging failed to reach target before reward interaction", prefix);
        return result;
    }

    DungeonNavigation::WaitForLocalPositionSettle(
        options.settle_timeout_ms,
        options.settle_distance);
    auto* me = AgentMgr::GetMyAgent();
    result.player_x = me ? me->x : 0.0f;
    result.player_y = me ? me->y : 0.0f;
    result.distance_to_anchor = me
        ? AgentMgr::GetDistance(me->x, me->y, rewardNpc.x, rewardNpc.y)
        : -1.0f;
    result.map_id = MapMgr::GetMapId();
    result.map_loaded = MapMgr::GetIsMapLoaded();

    Log::Info("%s: Boss reward staging player=(%.0f, %.0f) target=(%.0f, %.0f) dist=%.0f map=%u loaded=%d",
              prefix,
              result.player_x,
              result.player_y,
              rewardNpc.x,
              rewardNpc.y,
              result.distance_to_anchor,
              result.map_id,
              result.map_loaded ? 1 : 0);
    return result;
}

RewardNpcResolveResult ResolveRewardNpc(
    const DungeonQuest::QuestNpcAnchor& rewardNpc,
    const RewardNpcResolveOptions& options) {
    RewardNpcResolveResult result = {};
    const char* label = options.label != nullptr ? options.label : "Boss reward";

    DungeonDiagnostics::NearbyNpcCandidate rewardCandidates[8] = {};
    const std::size_t rewardCandidateCount = DungeonDiagnostics::CollectNearbyNpcCandidates(
        rewardNpc.x,
        rewardNpc.y,
        rewardNpc.search_radius,
        rewardCandidates,
        sizeof(rewardCandidates) / sizeof(rewardCandidates[0]));
    DungeonDiagnostics::LogNearbyNpcCandidates(
        label,
        rewardNpc.x,
        rewardNpc.y,
        rewardNpc.search_radius,
        rewardCandidates,
        rewardCandidateCount);

    if (rewardCandidateCount > 0u) {
        result.npc_id = rewardCandidates[0].agentId;
        result.found_at_anchor = true;
        return result;
    }

    result.npc_id = DungeonInteractions::FindNearestNpc(
        rewardNpc.x,
        rewardNpc.y,
        rewardNpc.search_radius);
    if (result.npc_id != 0u) {
        result.found_at_anchor = true;
        return result;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me || options.local_search_radius <= 0.0f) {
        return result;
    }

    DungeonDiagnostics::NearbyNpcCandidate localCandidates[8] = {};
    const std::size_t localCandidateCount = DungeonDiagnostics::CollectNearbyNpcCandidates(
        me->x,
        me->y,
        options.local_search_radius,
        localCandidates,
        sizeof(localCandidates) / sizeof(localCandidates[0]));
    DungeonDiagnostics::LogNearbyNpcCandidates(
        "Boss reward local",
        me->x,
        me->y,
        options.local_search_radius,
        localCandidates,
        localCandidateCount);
    if (localCandidateCount > 0u) {
        result.npc_id = localCandidates[0].agentId;
        result.found_near_player = true;
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

BossRewardClaimResult ClaimBossReward(uint32_t npcId, const BossRewardClaimOptions& options) {
    BossRewardClaimResult result;
    result.npc_id = npcId;
    result.npc_found = npcId != 0u;
    const char* prefix = PrefixOrDefault(options.log_prefix);
    const char* label = LabelOrDefault(options.label, "Boss reward");
    if (options.quest_id == 0u || options.reward_dialog_id == 0u) {
        return result;
    }

    if (npcId == 0u) {
        result.used_fallback = true;
        Log::Info("%s: %s NPC not found near staging coords; sending reward dialog directly",
                  prefix,
                  label);
        result.reward_cleared = AcceptQuestRewardWithRetry(
            options.quest_id,
            0u,
            options.accept_retry_timeout_ms);
        result.final_quest_present = QuestMgr::GetQuestById(options.quest_id) != nullptr;
        Log::Info("%s: %s fallback cleared=%d questPresent=%d",
                  prefix,
                  label,
                  result.reward_cleared ? 1 : 0,
                  result.final_quest_present ? 1 : 0);
        DungeonDialog::SendDialogWithRetry(
            options.reward_dialog_id,
            options.fallback_send_attempts,
            options.fallback_send_delay_ms);
        QuestMgr::RequestQuestInfo(options.quest_id);
        if (options.fallback_refresh_delay_ms > 0u) {
            Sleep(options.fallback_refresh_delay_ms);
        }
        result.final_quest_present = QuestMgr::GetQuestById(options.quest_id) != nullptr;
        result.last_dialog_id = DialogMgr::GetLastDialogId();
        return result;
    }

    auto* npc = AgentMgr::GetAgentByID(npcId);
    Log::Info("%s: %s NPC agent=%u present=%d",
              prefix,
              label,
              npcId,
              npc != nullptr ? 1 : 0);
    if (npc != nullptr) {
        (void)DungeonNavigation::MoveToAndWait(
            npc->x,
            npc->y,
            options.npc_move_threshold,
            DungeonNavigation::MOVE_TO_TIMEOUT_MS,
            DungeonNavigation::MOVE_TO_POLL_MS,
            MapMgr::GetMapId());
    }

    DialogMgr::ClearDialog();
    DialogMgr::ResetHookState();

    DungeonInteractions::DirectNpcInteractOptions interactOptions = {};
    interactOptions.target_wait_ms = options.interact_target_wait_ms;
    interactOptions.pass_wait_ms = options.interact_pass_wait_ms;
    interactOptions.passes = options.interact_passes;
    interactOptions.log_prefix = prefix;
    interactOptions.label = label;
    interactOptions.stop_condition = &StopWhenBossRewardDialogReady;
    interactOptions.stop_context = const_cast<BossRewardClaimOptions*>(&options);
    (void)DungeonInteractions::PulseDirectNpcInteract(npcId, interactOptions);

    if (options.dialog_dwell_ms > 0u) {
        Sleep(options.dialog_dwell_ms);
    }
    AgentMgr::ChangeTarget(npcId);
    if (options.target_settle_ms > 0u) {
        Sleep(options.target_settle_ms);
    }

    DungeonDialog::LogDialogButtons(prefix, label);
    DungeonDialog::DialogAdvanceOptions advanceOptions = {};
    advanceOptions.sender_agent_id = npcId;
    advanceOptions.request_quest_id_after_dialog = options.quest_id;
    advanceOptions.max_passes = options.advance_max_passes;
    advanceOptions.change_target_before_dialog = true;
    advanceOptions.log_prefix = prefix;
    advanceOptions.label = label;
    result.reward_button_ready = DungeonDialog::AdvanceDialogToButton(
        options.reward_dialog_id,
        advanceOptions);

    Log::Info("%s: %s prep target=%u sender=%u buttons=%u dialogOpen=%d",
              prefix,
              label,
              AgentMgr::GetTargetId(),
              DialogMgr::GetDialogSenderAgentId(),
              DialogMgr::GetButtonCount(),
              DialogMgr::IsDialogOpen() ? 1 : 0);
    Log::Info("%s: %s button ready=%d",
              prefix,
              label,
              result.reward_button_ready ? 1 : 0);
    DungeonDialog::LogDialogButtons(prefix, label);

    result.reward_cleared = AcceptQuestRewardWithRetry(
        options.quest_id,
        npcId,
        options.accept_retry_timeout_ms);
    result.final_quest_present = QuestMgr::GetQuestById(options.quest_id) != nullptr;
    Log::Info("%s: %s QuestReward cleared=%d questPresent=%d",
              prefix,
              label,
              result.reward_cleared ? 1 : 0,
              result.final_quest_present ? 1 : 0);

    if (!result.reward_cleared &&
        DialogMgr::IsDialogOpen() &&
        DialogMgr::GetDialogSenderAgentId() == npcId &&
        DungeonDialog::HasDialogButton(options.reward_dialog_id)) {
        AgentMgr::ChangeTarget(npcId);
        if (options.target_settle_ms > 0u) {
            Sleep(options.target_settle_ms);
        }
        Log::Info("%s: %s retrying direct dialog button 0x%X",
                  prefix,
                  label,
                  options.reward_dialog_id);
        QuestDialogOptions retryOptions = {};
        retryOptions.post_dialog_wait_ms = options.retry_post_dialog_wait_ms;
        retryOptions.refresh_delay_ms = options.retry_refresh_delay_ms;
        retryOptions.log_prefix = prefix;
        retryOptions.label = label;
        (void)SendDialogAndRefreshQuest(options.reward_dialog_id, options.quest_id, retryOptions);
        result.retry_sent = true;
        result.final_quest_present = QuestMgr::GetQuestById(options.quest_id) != nullptr;
    }

    result.last_dialog_id = DialogMgr::GetLastDialogId();
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
