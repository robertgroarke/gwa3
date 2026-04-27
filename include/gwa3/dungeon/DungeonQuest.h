#pragma once

#include <cstdint>

namespace GWA3::DungeonQuest {

struct QuestNpcAnchor {
    float x = 0.0f;
    float y = 0.0f;
    float search_radius = 0.0f;
};

struct TravelPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct DialogPlan {
    const uint32_t* dialog_ids = nullptr;
    int dialog_count = 0;
    int dialog_repeats = 0;
};

struct BootstrapPlan {
    QuestNpcAnchor npc = {};
    const uint32_t* dialog_ids = nullptr;
    int dialog_count = 0;
    int dialog_repeats = 0;
    const TravelPoint* entry_path = nullptr;
    int entry_path_count = 0;
    TravelPoint zone_point = {};
    const TravelPoint* reset_path = nullptr;
    int reset_path_count = 0;
    uint32_t entry_map_id = 0;
    uint32_t reset_map_id = 0;
    uint32_t target_map_id = 0;
};

struct QuestCyclePlan {
    QuestNpcAnchor npc = {};
    DialogPlan reward_dialog = {};
    DialogPlan accept_dialog = {};
    const TravelPoint* approach_path = nullptr;
    int approach_path_count = 0;
    TravelPoint dungeon_entry = {};
    TravelPoint dungeon_exit = {};
    uint32_t start_map_id = 0;
    uint32_t dungeon_map_id = 0;
};

bool IsValidDialogPlan(const DialogPlan& plan);
int GetExpandedDialogCount(const DialogPlan& plan);
int ExpandDialogSequence(const DialogPlan& plan, uint32_t* outDialogIds, int capacity);
bool IsValidBootstrapPlan(const BootstrapPlan& plan);
int GetExpandedDialogCount(const BootstrapPlan& plan);
int ExpandDialogSequence(const BootstrapPlan& plan, uint32_t* outDialogIds, int capacity);
bool IsValidQuestCyclePlan(const QuestCyclePlan& plan);
const TravelPoint* GetLastTravelPoint(const TravelPoint* points, int count);
TravelPoint ResolveBootstrapZonePoint(const BootstrapPlan& plan);

} // namespace GWA3::DungeonQuest
