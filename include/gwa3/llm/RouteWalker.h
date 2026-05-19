#pragma once

#include <nlohmann/json.hpp>

namespace GWA3::LLM::RouteWalker {

    // Write the current Bogroot HM route-progress view into a snapshot JSON
    // object. The implementation is read-only and derives progress from the
    // current map/player position plus Froggy telemetry.
    void WriteRouteJson(nlohmann::json& out);

} // namespace GWA3::LLM::RouteWalker
