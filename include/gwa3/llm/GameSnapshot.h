#pragma once

#include <cstdint>
#include <string>

namespace GWA3::LLM::GameSnapshot {

    // Serialize current game state to a JSON string.
    // Each tier includes progressively more data:
    //   Tier 1: Player state, skillbar, map, party basics, route status
    //   Tier 2: Delta from Tier 1 + capped nearby agents and rich UI state
    //   Tier 3: Delta from Tier 2 + inventory, effects, storage, titles
    //
    // Returns a heap-allocated JSON string. Caller must delete[].
    // outLength receives the string length (excluding null terminator).

    char* SerializeTier1(uint32_t* outLength);
    char* SerializeTier2(uint32_t* outLength);
    char* SerializeTier3(uint32_t* outLength);

} // namespace GWA3::LLM::GameSnapshot
