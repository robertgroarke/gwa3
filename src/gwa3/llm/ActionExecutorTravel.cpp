#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/managers/MapMgr.h>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        ActionResult HandleTravel(const json& p) {
            if (!p.contains("map_id")) return MakeError("missing map_id");
            uint32_t mapId = p["map_id"].get<uint32_t>();
            if (mapId == 0 || mapId > 999) return MakeError("invalid_map_id");
            uint32_t region = p.value("region", 0u);
            uint32_t district = p.value("district", 0u);
            uint32_t language = p.value("language", 0u);
            GWA3::GameThread::Enqueue([mapId, region, district, language]() {
                MapMgr::Travel(mapId, region, district, language);
            });
            return MakeOk();
        }

        ActionResult HandleEnterMission(const json&) {
            GWA3::GameThread::Enqueue([]() { MapMgr::EnterMission(); });
            return MakeOk();
        }

        ActionResult HandleReturnToOutpost(const json&) {
            GWA3::GameThread::Enqueue([]() { MapMgr::ReturnToOutpost(); });
            return MakeOk();
        }

        ActionResult HandleSetHardMode(const json& p) {
            if (!p.contains("enabled")) return MakeError("missing enabled");
            bool enabled = p["enabled"].get<bool>();
            GWA3::GameThread::Enqueue([enabled]() { MapMgr::SetHardMode(enabled); });
            return MakeOk();
        }

        ActionResult HandleSkipCinematic(const json&) {
            GWA3::GameThread::Enqueue([]() { MapMgr::SkipCinematic(); });
            return MakeOk();
        }

    } // namespace

    void RegisterTravelActions(ActionDispatchTable& dispatch) {
        dispatch["travel"] = HandleTravel;
        dispatch["enter_mission"] = HandleEnterMission;
        dispatch["return_to_outpost"] = HandleReturnToOutpost;
        dispatch["set_hard_mode"] = HandleSetHardMode;
        dispatch["skip_cinematic"] = HandleSkipCinematic;
    }

} // namespace GWA3::LLM::ActionExecutor
