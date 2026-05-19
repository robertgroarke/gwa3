#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/managers/PartyMgr.h>

#include <cmath>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        bool IsCoordinateOutOfRange(float x, float y) {
            return std::abs(x) > 100000 || std::abs(y) > 100000;
        }

        ActionResult HandleAddHero(const json& p) {
            if (!p.contains("hero_id")) return MakeError("missing hero_id");
            uint32_t id = p["hero_id"].get<uint32_t>();
            GWA3::GameThread::Enqueue([id]() { PartyMgr::AddHero(id); });
            return MakeOk();
        }

        ActionResult HandleKickHero(const json& p) {
            if (!p.contains("hero_id")) return MakeError("missing hero_id");
            uint32_t id = p["hero_id"].get<uint32_t>();
            GWA3::GameThread::Enqueue([id]() { PartyMgr::KickHero(id); });
            return MakeOk();
        }

        ActionResult HandleKickAllHeroes(const json&) {
            return MakeError("deprecated_use_kick_hero_individually");
        }

        ActionResult HandleFlagHero(const json& p) {
            if (!p.contains("hero_index") || !p.contains("x") || !p.contains("y"))
                return MakeError("missing hero_index, x, or y");
            uint32_t idx = p["hero_index"].get<uint32_t>();
            float x = p["x"].get<float>();
            float y = p["y"].get<float>();
            if (IsCoordinateOutOfRange(x, y)) return MakeError("coordinates_out_of_range");
            GWA3::GameThread::Enqueue([idx, x, y]() { PartyMgr::FlagHero(idx, x, y); });
            return MakeOk();
        }

        ActionResult HandleFlagAll(const json& p) {
            if (!p.contains("x") || !p.contains("y")) return MakeError("missing x or y");
            float x = p["x"].get<float>();
            float y = p["y"].get<float>();
            if (IsCoordinateOutOfRange(x, y)) return MakeError("coordinates_out_of_range");
            GWA3::GameThread::Enqueue([x, y]() { PartyMgr::FlagAll(x, y); });
            return MakeOk();
        }

        ActionResult HandleUnflagAll(const json&) {
            GWA3::GameThread::Enqueue([]() { PartyMgr::UnflagAll(); });
            return MakeOk();
        }

        ActionResult HandleSetHeroBehavior(const json& p) {
            if (!p.contains("hero_index") || !p.contains("behavior"))
                return MakeError("missing hero_index or behavior");
            uint32_t idx = p["hero_index"].get<uint32_t>();
            uint32_t beh = p["behavior"].get<uint32_t>();
            if (beh > 2) return MakeError("invalid_behavior");
            GWA3::GameThread::Enqueue([idx, beh]() { PartyMgr::SetHeroBehavior(idx, beh); });
            return MakeOk();
        }

        ActionResult HandleLockHeroTarget(const json& p) {
            if (!p.contains("hero_index") || !p.contains("target_id"))
                return MakeError("missing hero_index or target_id");
            uint32_t idx = p["hero_index"].get<uint32_t>();
            uint32_t tid = p["target_id"].get<uint32_t>();
            GWA3::GameThread::Enqueue([idx, tid]() { PartyMgr::LockHeroTarget(idx, tid); });
            return MakeOk();
        }

    } // namespace

    void RegisterPartyActions(ActionDispatchTable& dispatch) {
        dispatch["add_hero"] = HandleAddHero;
        dispatch["kick_hero"] = HandleKickHero;
        dispatch["kick_all_heroes"] = HandleKickAllHeroes;
        dispatch["flag_hero"] = HandleFlagHero;
        dispatch["flag_all"] = HandleFlagAll;
        dispatch["unflag_all"] = HandleUnflagAll;
        dispatch["set_hero_behavior"] = HandleSetHeroBehavior;
        dispatch["lock_hero_target"] = HandleLockHeroTarget;
    }

} // namespace GWA3::LLM::ActionExecutor
