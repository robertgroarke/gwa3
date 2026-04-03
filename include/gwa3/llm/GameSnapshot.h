#pragma once

#include <cstdint>
#include <string>

namespace GWA3::LLM::GameSnapshot {

    // Serialize current game state to a JSON string.
    // Each tier includes progressively more data:
    //   Tier 1: Player state, skillbar, map, party basics (~500 bytes)
    //   Tier 2: Tier 1 + nearby agents within range (~2-4KB)
    //   Tier 3: Tier 2 + inventory, effects, party details (~5-10KB)
    //
    // Returns a heap-allocated JSON string. Caller must delete[].
    // outLength receives the string length (excluding null terminator).

    char* SerializeTier1(uint32_t* outLength);
    char* SerializeTier2(uint32_t* outLength);
    char* SerializeTier3(uint32_t* outLength);

} // namespace GWA3::LLM::GameSnapshot
