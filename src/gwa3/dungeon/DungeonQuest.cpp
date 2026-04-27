#include <gwa3/dungeon/DungeonQuest.h>

namespace GWA3::DungeonQuest {

bool IsValidDialogPlan(const DialogPlan& plan) {
    return plan.dialog_ids != nullptr
        && plan.dialog_count > 0
        && plan.dialog_repeats > 0;
}

int GetExpandedDialogCount(const DialogPlan& plan) {
    if (!IsValidDialogPlan(plan)) {
        return 0;
    }
    return plan.dialog_count * plan.dialog_repeats;
}

int ExpandDialogSequence(const DialogPlan& plan, uint32_t* outDialogIds, int capacity) {
    const int total = GetExpandedDialogCount(plan);
    if (outDialogIds == nullptr || capacity < total) {
        return 0;
    }

    int index = 0;
    for (int repeat = 0; repeat < plan.dialog_repeats; ++repeat) {
        for (int i = 0; i < plan.dialog_count; ++i) {
            outDialogIds[index++] = plan.dialog_ids[i];
        }
    }
    return index;
}

bool IsValidBootstrapPlan(const BootstrapPlan& plan) {
    if (plan.npc.search_radius <= 0.0f) {
        return false;
    }
    const DialogPlan dialog = {
        plan.dialog_ids,
        plan.dialog_count,
        plan.dialog_repeats,
    };
    if (!IsValidDialogPlan(dialog)) {
        return false;
    }
    if (plan.entry_path == nullptr || plan.entry_path_count <= 0) {
        return false;
    }
    if (plan.target_map_id == 0u) {
        return false;
    }
    if ((plan.reset_path == nullptr) != (plan.reset_path_count == 0)) {
        return false;
    }
    return true;
}

int GetExpandedDialogCount(const BootstrapPlan& plan) {
    if (!IsValidBootstrapPlan(plan)) {
        return 0;
    }
    return GetExpandedDialogCount(DialogPlan{
        plan.dialog_ids,
        plan.dialog_count,
        plan.dialog_repeats,
    });
}

int ExpandDialogSequence(const BootstrapPlan& plan, uint32_t* outDialogIds, int capacity) {
    return ExpandDialogSequence(DialogPlan{
        plan.dialog_ids,
        plan.dialog_count,
        plan.dialog_repeats,
    }, outDialogIds, capacity);
}

bool IsValidQuestCyclePlan(const QuestCyclePlan& plan) {
    if (plan.npc.search_radius <= 0.0f) {
        return false;
    }
    if (!IsValidDialogPlan(plan.reward_dialog) || !IsValidDialogPlan(plan.accept_dialog)) {
        return false;
    }
    if (plan.approach_path == nullptr || plan.approach_path_count <= 0) {
        return false;
    }
    if (plan.start_map_id == 0u || plan.dungeon_map_id == 0u) {
        return false;
    }
    return true;
}

const TravelPoint* GetLastTravelPoint(const TravelPoint* points, int count) {
    if (points == nullptr || count <= 0) {
        return nullptr;
    }
    return &points[count - 1];
}

TravelPoint ResolveBootstrapZonePoint(const BootstrapPlan& plan) {
    if (plan.zone_point.x != 0.0f || plan.zone_point.y != 0.0f) {
        return plan.zone_point;
    }

    const auto* point = GetLastTravelPoint(plan.entry_path, plan.entry_path_count);
    return point ? *point : TravelPoint{};
}

} // namespace GWA3::DungeonQuest
