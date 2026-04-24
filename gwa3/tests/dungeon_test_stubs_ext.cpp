#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/PlayerMgr.h>

#include <algorithm>
#include <cwchar>

namespace {

bool g_test_hard_mode_enabled = false;
uint32_t g_test_party_hero_ids[16] = {};
uint32_t g_test_party_hero_count = 0u;
uint32_t g_test_party_hero_behaviors[16] = {};
wchar_t g_test_player_name[64] = {};

} // namespace

namespace GWA3::MapMgr {

void SetHardMode(bool enabled) {
    g_test_hard_mode_enabled = enabled;
}

} // namespace GWA3::MapMgr

namespace GWA3::TestStubs::MapMgr {

void SetHardModeEnabled(bool enabled) {
    g_test_hard_mode_enabled = enabled;
}

bool HardModeEnabled() {
    return g_test_hard_mode_enabled;
}

} // namespace GWA3::TestStubs::MapMgr

namespace GWA3::PartyMgr {

void AddHero(uint32_t heroId) {
    if (heroId == 0u || g_test_party_hero_count >= 16u) {
        return;
    }
    g_test_party_hero_ids[g_test_party_hero_count++] = heroId;
}

void KickHero(uint32_t heroId) {
    for (uint32_t i = 0u; i < g_test_party_hero_count; ++i) {
        if (g_test_party_hero_ids[i] != heroId) {
            continue;
        }
        for (uint32_t j = i + 1u; j < g_test_party_hero_count; ++j) {
            g_test_party_hero_ids[j - 1u] = g_test_party_hero_ids[j];
        }
        g_test_party_hero_ids[g_test_party_hero_count - 1u] = 0u;
        if (g_test_party_hero_count > 0u) {
            --g_test_party_hero_count;
        }
        break;
    }
}

void KickAllHeroes() {
    for (uint32_t i = 0u; i < 16u; ++i) {
        g_test_party_hero_ids[i] = 0u;
        g_test_party_hero_behaviors[i] = 0u;
    }
    g_test_party_hero_count = 0u;
}

void SetHeroBehavior(uint32_t heroIndex, uint32_t behavior) {
    if (heroIndex == 0u || heroIndex > 16u) {
        return;
    }
    g_test_party_hero_behaviors[heroIndex - 1u] = behavior;
}

uint32_t CountPartyHeroes() {
    return g_test_party_hero_count;
}

size_t GetPartyHeroIds(uint32_t* out, size_t maxCount) {
    if (!out || maxCount == 0u) {
        return 0u;
    }

    const size_t count = std::min<std::size_t>(g_test_party_hero_count, maxCount);
    for (size_t i = 0; i < count; ++i) {
        out[i] = g_test_party_hero_ids[i];
    }
    return count;
}

void DebugDumpPartyState(const char*) {
}

} // namespace GWA3::PartyMgr

namespace GWA3::TestStubs::PartyMgr {

void ResetHeroes() {
    GWA3::PartyMgr::KickAllHeroes();
}

uint32_t HeroIdAt(uint32_t index) {
    return index < 16u ? g_test_party_hero_ids[index] : 0u;
}

uint32_t HeroBehaviorAt(uint32_t index) {
    return index < 16u ? g_test_party_hero_behaviors[index] : 0u;
}

} // namespace GWA3::TestStubs::PartyMgr

namespace GWA3::PlayerMgr {

wchar_t* GetPlayerName(uint32_t playerId) {
    return playerId == 0u ? g_test_player_name : nullptr;
}

} // namespace GWA3::PlayerMgr

namespace GWA3::MaintenanceMgr {

bool IsRareSkin(uint32_t) {
    return false;
}

} // namespace GWA3::MaintenanceMgr

namespace GWA3::TestStubs::PlayerMgr {

void SetPlayerName(const wchar_t* name) {
    g_test_player_name[0] = L'\0';
    if (!name) {
        return;
    }
    wcsncpy_s(g_test_player_name, name, _TRUNCATE);
}

} // namespace GWA3::TestStubs::PlayerMgr
