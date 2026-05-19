#include <gwa3/llm/RouteWalker.h>

#include <bots/froggy/FroggyHM.h>
#include <gwa3/game/Agent.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

using json = nlohmann::json;

namespace GWA3::LLM::RouteWalker {

namespace {

struct RouteStep {
    uint32_t map_id;
    int order;
    const char* phase;
    const char* kind;
    const char* label;
    float x;
    float y;
    float tolerance;
};

static constexpr RouteStep kSparkflySteps[] = {
    {MapIds::SPARKFLY_SWAMP, 2, "sparkfly_to_tekks", "route", "sparkfly_1", -4559.0f, -14406.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 3, "sparkfly_to_tekks", "route", "sparkfly_2", -5204.0f, -9831.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 4, "sparkfly_to_tekks", "route", "sparkfly_3", -928.0f, -8699.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 5, "sparkfly_to_tekks", "route", "sparkfly_4", 4200.0f, -4897.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 6, "sparkfly_to_tekks", "route", "sparkfly_5", 6114.0f, 819.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 7, "sparkfly_to_tekks", "route", "sparkfly_6", 9500.0f, 2281.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 8, "sparkfly_to_tekks", "route", "sparkfly_7", 11570.0f, 6120.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 9, "sparkfly_to_tekks", "route", "sparkfly_8", 11025.0f, 11710.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 10, "sparkfly_to_tekks", "route", "sparkfly_9", 14624.0f, 19314.0f, 650.0f},
    {MapIds::SPARKFLY_SWAMP, 11, "tekks", "quest_accept", "Tekks quest accept", 12396.0f, 22407.0f, 850.0f},
    {MapIds::SPARKFLY_SWAMP, 12, "dungeon_entry", "dungeon_entry", "Bogroot entrance", 12968.0f, 26219.0f, 900.0f},
};

static constexpr RouteStep kLvl1Steps[] = {
    {MapIds::BOGROOT_GROWTHS_LVL1, 13, "bogroot_l1", "spawn", "L1 spawn", 17026.0f, 2168.0f, 650.0f},
    {MapIds::BOGROOT_GROWTHS_LVL1, 14, "bogroot_l1", "blessing", "Dwarven blessing", 19099.0f, 7762.0f, 800.0f},
    {MapIds::BOGROOT_GROWTHS_LVL1, 15, "bogroot_l1", "checkpoint", "Quest Door Checkpoint", 14434.0f, 8000.0f, 800.0f},
    {MapIds::BOGROOT_GROWTHS_LVL1, 16, "bogroot_l1", "checkpoint", "mid convergence", 672.0f, 1105.0f, 900.0f},
    {MapIds::BOGROOT_GROWTHS_LVL1, 17, "bogroot_l1", "portal", "Lvl1 to Lvl2", 7665.0f, -19050.0f, 900.0f},
};

static constexpr RouteStep kLvl2Steps[] = {
    {MapIds::BOGROOT_GROWTHS_LVL2, 18, "bogroot_l2", "spawn", "L2 spawn", -11386.0f, -3871.0f, 700.0f},
    {MapIds::BOGROOT_GROWTHS_LVL2, 19, "bogroot_l2", "checkpoint", "L2 route 4", -4110.0f, 4484.0f, 900.0f},
    {MapIds::BOGROOT_GROWTHS_LVL2, 20, "bogroot_l2", "checkpoint", "L2 route 12", 3086.0f, 12899.0f, 1000.0f},
    {MapIds::BOGROOT_GROWTHS_LVL2, 21, "bogroot_l2", "checkpoint", "L2 route 18", 12200.0f, -6591.0f, 1000.0f},
    {MapIds::BOGROOT_GROWTHS_LVL2, 22, "bogroot_l2", "key", "Dungeon Key", 16854.0f, -5830.0f, 700.0f},
    {MapIds::BOGROOT_GROWTHS_LVL2, 23, "bogroot_l2", "door", "Dungeon Door", 17925.0f, -6197.0f, 700.0f},
    {MapIds::BOGROOT_GROWTHS_LVL2, 24, "bogroot_l2", "boss_engagement", "boss gauntlet start", 18334.0f, -8838.0f, 1000.0f},
    {MapIds::BOGROOT_GROWTHS_LVL2, 25, "bogroot_l2", "checkpoint", "final boss approach", 14035.0f, -17800.0f, 1000.0f},
    {MapIds::BOGROOT_GROWTHS_LVL2, 26, "bogroot_l2", "end_chest", "Bogroot end chest", 14876.0f, -19033.0f, 900.0f},
};

static constexpr int kRouteStepCount = 29;

struct RouteView {
    const RouteStep* steps = nullptr;
    int count = 0;
};

template <size_t N>
static constexpr int ArrayCount(const RouteStep (&)[N]) {
    return static_cast<int>(N);
}

static RouteView StepsForMap(uint32_t mapId) {
    switch (mapId) {
        case MapIds::SPARKFLY_SWAMP:
            return {kSparkflySteps, ArrayCount(kSparkflySteps)};
        case MapIds::BOGROOT_GROWTHS_LVL1:
            return {kLvl1Steps, ArrayCount(kLvl1Steps)};
        case MapIds::BOGROOT_GROWTHS_LVL2:
            return {kLvl2Steps, ArrayCount(kLvl2Steps)};
        default:
            return {};
    }
}

static float DistanceTo(const AgentLiving* me, const RouteStep& step) {
    if (!me) return 999999.0f;
    const float dx = me->x - step.x;
    const float dy = me->y - step.y;
    return std::sqrt(dx * dx + dy * dy);
}

static int NearestStepIndex(const RouteView& route, const AgentLiving* me, float* outDistance) {
    int nearest = -1;
    float best = 999999.0f;
    for (int i = 0; i < route.count; ++i) {
        const float d = DistanceTo(me, route.steps[i]);
        if (d < best) {
            best = d;
            nearest = i;
        }
    }
    if (outDistance) *outDistance = best;
    return nearest;
}

static json StepToJson(const RouteStep& step) {
    json j;
    j["kind"] = step.kind;
    j["label"] = step.label;
    j["order"] = step.order;
    j["x"] = step.x;
    j["y"] = step.y;
    j["tolerance"] = step.tolerance;
    return j;
}

static json DeviationJson(const char* reason, float distance) {
    json j;
    j["reason"] = reason;
    if (distance >= 0.0f) {
        j["distance"] = distance;
    }
    j["recovery_actions"] = {"query_state", "wait", "froggy_travel_to_gadds"};
    return j;
}

static void WriteTelemetry(json& out) {
    const auto& telemetry = GWA3::Bot::Froggy::g_dungeonLoopTelemetry;
    json last;
    last["step"] = telemetry.last_waypoint_index;
    last["label"] = telemetry.last_waypoint_label;
    last["waypoint_iterations"] = telemetry.waypoint_iterations;
    last["entered_lvl2"] = telemetry.entered_lvl2;
    last["boss_started"] = telemetry.boss_started;
    last["boss_completed"] = telemetry.boss_completed;
    last["chest_attempts"] = telemetry.chest_attempts;
    last["chest_successes"] = telemetry.chest_successes;
    last["returned_to_sparkfly"] = telemetry.returned_to_sparkfly;
    out["last_outcome"] = last;
}

} // namespace

void WriteRouteJson(json& out) {
    out = json::object();
    out["script_id"] = "bogroot_hm_v1";
    out["step_count"] = kRouteStepCount;

    const uint32_t mapId = MapMgr::GetMapId();
    const bool loading = mapId == 0 || MapMgr::GetLoadingState() != 1 || AgentMgr::GetMyId() == 0;
    out["map_id"] = mapId;
    if (loading) {
        out["phase"] = "loading";
        out["progress"] = "loading";
        out["deviation"] = nullptr;
        WriteTelemetry(out);
        return;
    }

    if (mapId == MapIds::GADDS_ENCAMPMENT) {
        out["phase"] = "town";
        out["step_index"] = 1;
        out["progress"] = "preparing";
        out["next_step"] = {
            {"kind", "travel"},
            {"label", "Sparkfly Swamp"},
            {"map_id", MapIds::SPARKFLY_SWAMP},
            {"order", 1},
        };
        out["deviation"] = nullptr;
        WriteTelemetry(out);
        return;
    }

    const RouteView route = StepsForMap(mapId);
    if (!route.steps || route.count == 0) {
        out["phase"] = "off_route";
        out["progress"] = "unknown_map";
        out["deviation"] = DeviationJson("unsupported_map", -1.0f);
        WriteTelemetry(out);
        return;
    }

    const auto* me = AgentMgr::GetMyAgent();
    float nearestDistance = 999999.0f;
    const int nearest = NearestStepIndex(route, me, &nearestDistance);
    const int clampedNearest = std::max(0, nearest);
    const RouteStep& nearestStep = route.steps[clampedNearest];
    int nextIndex = clampedNearest;
    if (nearestDistance <= nearestStep.tolerance && clampedNearest + 1 < route.count) {
        nextIndex = clampedNearest + 1;
    }

    const RouteStep& next = route.steps[std::min(nextIndex, route.count - 1)];
    out["phase"] = next.phase;
    out["step_index"] = next.order;
    out["nearest_step_index"] = nearestStep.order;
    out["nearest_distance"] = nearestDistance;
    out["next_step"] = StepToJson(next);
    out["progress"] = nearestDistance <= nearestStep.tolerance ? "at_step" : "moving";
    if (nearestDistance > 4500.0f) {
        out["deviation"] = DeviationJson("off_route", nearestDistance);
    } else {
        out["deviation"] = nullptr;
    }
    WriteTelemetry(out);
}

} // namespace GWA3::LLM::RouteWalker
