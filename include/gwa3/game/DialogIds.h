#pragma once

#include <cstdint>

namespace GWA3::DialogIds {

inline constexpr uint32_t GENERIC_ACCEPT = 0x8101u;
inline constexpr uint32_t NPC_TALK = 0x2AE6u;
inline constexpr uint32_t SHORT_ACCEPT = 0x801u;

namespace ArachnisHaunt {
inline constexpr uint32_t COMPLETION = 0x831A02u;
inline constexpr uint32_t QUEST = 0x831A01u;
inline constexpr uint32_t REWARD = 0x831A07u;
} // namespace ArachnisHaunt

namespace FrostmawsBurrows {
inline constexpr uint32_t QUEST = 0x832A01u;
} // namespace FrostmawsBurrows

namespace Kathandrax {
inline constexpr uint32_t QUEST = 0x832501u;
} // namespace Kathandrax

namespace RavensPoint {
inline constexpr uint32_t QUEST = 0x834301u;
inline constexpr uint32_t REWARD = 0x834307u;
} // namespace RavensPoint

namespace RragarsMenagerie {
inline constexpr uint32_t ACCEPT_VEILED_THREAT = 0x832301u;
inline constexpr uint32_t COMPLETE_OLD_QUEST = 0x832307u;
inline constexpr uint32_t PICK_VEILED_THREAT = 0x832303u;
} // namespace RragarsMenagerie

namespace TekksWar {
inline constexpr uint32_t DUNGEON_ENTRY = 0x833905u;
inline constexpr uint32_t QUEST_ACCEPT = 0x833901u;
inline constexpr uint32_t QUEST_REWARD = 0x833907u;
} // namespace TekksWar

} // namespace GWA3::DialogIds
