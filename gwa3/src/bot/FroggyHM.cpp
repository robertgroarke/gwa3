#include <gwa3/bot/FroggyHM.h>
#include <gwa3/bot/BotFramework.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/SkillMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/GameThread.h>

#include <Windows.h>
#include <cmath>

namespace GWA3::Bot::Froggy {

// ===== Constants =====

static constexpr uint32_t MAP_SPARKFLY_SWAMP    = 558;
static constexpr uint32_t MAP_BOGROOT_LVL1      = 615;
static constexpr uint32_t MAP_BOGROOT_LVL2      = 616;
static constexpr uint32_t MAP_GADDS_ENCAMPMENT  = 638;

static constexpr uint32_t QUEST_TEKKS_WAR       = 0x339;
static constexpr uint32_t DIALOG_QUEST_REWARD   = 0x833907;
static constexpr uint32_t DIALOG_QUEST_ACCEPT   = 0x833905;
static constexpr uint32_t DIALOG_QUEST_BODY     = 0x8101;
static constexpr uint32_t DIALOG_NPC_TALK       = 0x2AE6;

// ===== Waypoint =====

struct Waypoint {
    float x, y;
    float fightRange;
    const char* label;
};

// ===== Routes =====

static const Waypoint SPARKFLY_TO_DUNGEON[] = {
    {-4559,  -14406, 1300, "1"},
    {-5204,  -9831,  1300, "2"},
    {-928,   -8699,  1300, "3"},
    {4200,   -4897,  1500, "4"},
    {6114,   819,    1300, "5"},
    {9500,   2281,   1300, "6"},
    {11570,  6120,   1200, "7"},
    {11025,  11710,  900,  "8"},
    {14624,  19314,  600,  "9"},
    {14650,  19417,  0,    "10"},
    {12280,  22585,  0,    "11"},
};

static const Waypoint BOGROOT_LVL1[] = {
    {17026,  2168,   1200, "0"},
    {19099,  7762,   1200, "Blessing"},
    {17279,  8106,   1200, "1"},
    {14434,  8000,   1200, "Quest Door Checkpoint"},
    {14434,  8000,   1300, "2"},
    {10789,  6433,   1300, "3"},
    {8101,   6800,   1200, "4"},
    {6721,   5340,   1300, "5"},
    {4305,   1078,   1300, "6"},
    {757,    1110,   1200, "7"},
    {1370,   149,    1700, "8"},
    {672,    1105,   2000, "9"},
    {453,    1449,   2000, "10"},
    {504,    -1973,  800,  "11"},
    {-447,   -3014,  800,  "12"},
    {-1055,  -4527,  1000, "13"},
    {-1424,  -6156,  1200, "14"},
    {-475,   -7511,  800,  "15"},
    {265,    -8791,  1400, "16"},
    {1061,   -9443,  1400, "17"},
    {1805,   -10185, 1400, "18"},
    {1665,   -12213, 1400, "19"},
    {3550,   -16052, 1400, "20"},
    {4941,   -16181, 0,    "21"},
    {7360,   -17361, 0,    "22"},
    {7552,   -18776, 0,    "23"},
    {7665,   -19050, 0,    "24"},
    {7665,   -19050, 0,    "Lvl1 to Lvl2"},
};

static const Waypoint BOGROOT_LVL2[] = {
    {-11386, -3871,  400,  "1"},
    {-11132, -2450,  400,  "2"},
    {-8559,  593,    300,  "3"},
    {-4110,  4484,   1200, "4"},
    {-3747,  5068,   1200, "5"},
    {-2597,  5775,   1000, "6"},
    {-2618,  6383,   1100, "7"},
    {-2770,  7571,   1000, "8"},
    {-243,   8364,   1000, "9"},
    {-189,   10499,  1000, "10"},
    {37,     11449,  1400, "11"},
    {3086,   12899,  2000, "12"},
    {4182,   13767,  2000, "13"},
    {7293,   9457,   2000, "14"},
    {8150,   8143,   1500, "15"},
    {8560,   2323,   1500, "16"},
    {9525,   -1153,  1500, "17"},
    {12200,  -6591,  1600, "18"},
    {12200,  -6591,  1600, "19"},
    {17003,  -4906,  1600, "20"},
    {16854,  -5830,  1600, "Dungeon Key"},
    {17925,  -6197,  300,  "Dungeon Door"},
    {17482,  -6661,  300,  "Dungeon Door Checkpoint"},
    {18334,  -8838,  0,    "Boss 1"},
    {16131,  -11510, 0,    "Boss 2"},
    {19009,  -12300, 0,    "Boss 3"},
    {19610,  -11527, 0,    "Boss 4"},
    {18413,  -13924, 0,    "Boss 5"},
    {14188,  -15231, 0,    "Boss 6"},
    {13186,  -17286, 0,    "Boss 7"},
    {14035,  -17800, 0,    "Boss 8"},
    {13583,  -17529, 1100, "Boss 9"},
    {14617,  -18282, 1400, "Boss 10"},
    {15117,  -18582, 1400, "Boss 11"},
    {15117,  -18582, 1400, "Boss 12"},
    {15117,  -18582, 1600, "Boss"},
};

// ===== Run Statistics =====
static uint32_t s_runCount = 0;
static uint32_t s_failCount = 0;
static uint32_t s_wipeCount = 0;
static DWORD s_runStartTime = 0;
static DWORD s_totalStartTime = 0;
static DWORD s_bestRunTime = 0xFFFFFFFF;

// ===== Helpers =====

static float DistanceTo(float x, float y) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 99999.0f;
    return AgentMgr::GetDistance(me->x, me->y, x, y);
}

static int GetNearestWaypointIndex(const Waypoint* wps, int count) {
    auto* me = AgentMgr::GetMyAgent();
    if (!me) return 0;
    float bestDist = 999999.0f;
    int bestIdx = 0;
    for (int i = 0; i < count; i++) {
        float d = AgentMgr::GetSquaredDistance(me->x, me->y, wps[i].x, wps[i].y);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }
    return bestIdx;
}

static bool IsDead() {
    auto* me = AgentMgr::GetMyAgent();
    return !me || me->hp <= 0.0f;
}

static bool IsMapLoaded() {
    return MapMgr::GetIsMapLoaded();
}

static void WaitMs(DWORD ms) {
    Sleep(ms);
}

static void MoveToAndWait(float x, float y, float threshold = 250.0f) {
    AgentMgr::Move(x, y);
    DWORD start = GetTickCount();
    while (DistanceTo(x, y) > threshold && (GetTickCount() - start) < 30000) {
        if (IsDead()) return;
        WaitMs(250);
    }
}

static void AggroMoveToEx(float x, float y, float fightRange = 1350.0f) {
    AgentMgr::Move(x, y);
    DWORD start = GetTickCount();
    while (DistanceTo(x, y) > 250.0f && (GetTickCount() - start) < 240000) {
        if (IsDead()) return;
        if (!IsMapLoaded()) return;

        // Check for enemies in fight range — auto-attack nearest
        auto* agents = AgentMgr::GetAgentArray();
        if (agents && agents->buffer) {
            float bestDist = fightRange * fightRange;
            uint32_t bestId = 0;
            auto* me = AgentMgr::GetMyAgent();
            if (me) {
                for (uint32_t i = 0; i < agents->size; i++) {
                    auto* a = agents->buffer[i];
                    if (!a || a->type != 0xDB) continue;
                    auto* living = static_cast<AgentLiving*>(a);
                    if (living->allegiance != 3) continue; // not foe
                    if (living->hp <= 0.0f) continue;
                    float d = AgentMgr::GetSquaredDistance(me->x, me->y, living->x, living->y);
                    if (d < bestDist) {
                        bestDist = d;
                        bestId = living->agent_id;
                    }
                }
            }
            if (bestId) {
                AgentMgr::ChangeTarget(bestId);
                AgentMgr::Attack(bestId);
                // Wait for combat to resolve
                WaitMs(1000);
                continue;
            }
        }

        AgentMgr::Move(x, y);
        WaitMs(500);
    }
}

static void FollowWaypoints(const Waypoint* wps, int count) {
    int startIdx = GetNearestWaypointIndex(wps, count);
    uint32_t mapId = MapMgr::GetMapId();

    for (int i = startIdx; i < count; i++) {
        if (!Bot::IsRunning()) return;
        if (MapMgr::GetMapId() != mapId) return;
        if (IsDead()) {
            s_wipeCount++;
            LogBot("WIPE detected at waypoint %d (%s)", i, wps[i].label);
            // Wait for resurrection
            DWORD wipeStart = GetTickCount();
            while (IsDead() && (GetTickCount() - wipeStart) < 120000) {
                WaitMs(500);
            }
            if (IsDead()) {
                // Party defeated — return to outpost
                MapMgr::ReturnToOutpost();
                return;
            }
            // Resume from nearest waypoint
            i = GetNearestWaypointIndex(wps, count);
            LogBot("Resuming from waypoint %d after wipe", i);
        }

        LogBot("Moving to waypoint %d: %s (%.0f, %.0f)", i, wps[i].label, wps[i].x, wps[i].y);

        // Special waypoint handling
        if (strcmp(wps[i].label, "Lvl1 to Lvl2") == 0) {
            DWORD start = GetTickCount();
            while ((GetTickCount() - start) < 60000) {
                AgentMgr::Move(7665, -19050);
                WaitMs(250);
                if (MapMgr::GetMapId() == MAP_BOGROOT_LVL2) return;
            }
            return;
        }
        if (strcmp(wps[i].label, "Dungeon Key") == 0) {
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            // Pickup with large radius
            WaitMs(500);
            continue;
        }
        if (strcmp(wps[i].label, "Dungeon Door") == 0) {
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            // Open door interaction
            AgentMgr::InteractSignpost(AgentMgr::GetTargetId());
            WaitMs(2000);
            continue;
        }
        if (strcmp(wps[i].label, "Boss") == 0) {
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
            // Boss encounter — fight then loot
            WaitMs(3000);
            // Open chest
            MoveToAndWait(14876, -19033);
            WaitMs(5000);
            // Talk to Tekk for reward
            MoveToAndWait(14618, -17828);
            QuestMgr::Dialog(DIALOG_QUEST_REWARD);
            WaitMs(1000);
            QuestMgr::Dialog(DIALOG_QUEST_REWARD);
            WaitMs(1000);
            return;
        }

        // Standard waypoint — aggro move
        if (wps[i].fightRange > 0 && IsMapLoaded()) {
            AggroMoveToEx(wps[i].x, wps[i].y, wps[i].fightRange);
        } else {
            MoveToAndWait(wps[i].x, wps[i].y);
        }
    }
}

// ===== State Handlers =====

BotState HandleCharSelect(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: CharSelect");

    // Click Play button
    if (UIMgr::IsFrameVisible(UIMgr::Hashes::PlayButton)) {
        UIMgr::ButtonClickByHash(UIMgr::Hashes::PlayButton);
        WaitMs(5000);
    }

    // Handle reconnect dialog
    if (UIMgr::IsFrameVisible(UIMgr::Hashes::ReconnectYes)) {
        UIMgr::ButtonClickByHash(UIMgr::Hashes::ReconnectYes);
        WaitMs(3000);
    }

    // Wait for map load
    WaitMs(5000);
    uint32_t mapId = MapMgr::GetMapId();
    if (mapId > 0) {
        return BotState::InTown;
    }

    return BotState::CharSelect;
}

BotState HandleTownSetup(BotConfig& cfg) {
    LogBot("State: TownSetup (run #%u)", s_runCount + 1);

    uint32_t mapId = MapMgr::GetMapId();

    // If not at Gadd's Encampment, travel there
    if (mapId != MAP_GADDS_ENCAMPMENT) {
        MapMgr::Travel(MAP_GADDS_ENCAMPMENT);
        WaitMs(10000);
        return BotState::InTown;
    }

    // Add heroes
    for (int i = 0; i < 7; i++) {
        if (cfg.hero_ids[i] > 0) {
            PartyMgr::AddHero(cfg.hero_ids[i]);
            WaitMs(300);
        }
    }

    // Set hard mode
    if (cfg.hard_mode) {
        MapMgr::SetHardMode(true);
        WaitMs(500);
    }

    // Set hero behaviors to Guard
    for (int i = 0; i < 7; i++) {
        PartyMgr::SetHeroBehavior(i + 1, 1); // 1 = Guard
        WaitMs(100);
    }

    return BotState::Traveling;
}

BotState HandleTravel(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Travel to Sparkfly Swamp");

    uint32_t mapId = MapMgr::GetMapId();

    if (mapId == MAP_GADDS_ENCAMPMENT) {
        // Move to exit portal
        MoveToAndWait(-10018, -21892);
        MoveToAndWait(-9550, -20400);
        AgentMgr::Move(-9451, -19766);
        // Wait for Sparkfly Swamp load
        WaitMs(10000);
    }

    mapId = MapMgr::GetMapId();
    if (mapId == MAP_SPARKFLY_SWAMP) {
        return BotState::InDungeon;
    }

    // Fallback
    return BotState::InTown;
}

BotState HandleDungeon(BotConfig& cfg) {
    (void)cfg;
    uint32_t mapId = MapMgr::GetMapId();

    if (mapId == MAP_SPARKFLY_SWAMP) {
        LogBot("State: Sparkfly Swamp — running to dungeon");
        s_runCount++;
        s_runStartTime = GetTickCount();

        // Accept quest from Tekk
        MoveToAndWait(12396, 22407);
        WaitMs(500);
        QuestMgr::Dialog(DIALOG_NPC_TALK);
        WaitMs(500);
        QuestMgr::Dialog(DIALOG_QUEST_ACCEPT);
        WaitMs(500);

        // Move to dungeon entrance
        MoveToAndWait(12228, 22677);
        MoveToAndWait(12470, 25036);
        AgentMgr::Move(12968, 26219);
        WaitMs(1000);
        // Enter dungeon
        DWORD start = GetTickCount();
        while ((GetTickCount() - start) < 60000) {
            AgentMgr::Move(13097, 26393);
            WaitMs(250);
            if (MapMgr::GetMapId() == MAP_BOGROOT_LVL1) break;
        }
        WaitMs(3000);
    }

    mapId = MapMgr::GetMapId();

    if (mapId == MAP_BOGROOT_LVL1) {
        LogBot("State: Bogroot Level 1");
        FollowWaypoints(BOGROOT_LVL1, sizeof(BOGROOT_LVL1) / sizeof(BOGROOT_LVL1[0]));
        WaitMs(3000);
    }

    mapId = MapMgr::GetMapId();

    if (mapId == MAP_BOGROOT_LVL2) {
        LogBot("State: Bogroot Level 2");
        FollowWaypoints(BOGROOT_LVL2, sizeof(BOGROOT_LVL2) / sizeof(BOGROOT_LVL2[0]));

        // Run complete
        DWORD runTime = GetTickCount() - s_runStartTime;
        if (runTime < s_bestRunTime) s_bestRunTime = runTime;
        LogBot("Run #%u complete in %u ms (best: %u ms)", s_runCount, runTime, s_bestRunTime);

        return BotState::Looting;
    }

    // If we ended up back in outpost (wipe/resign), restart
    if (mapId == MAP_GADDS_ENCAMPMENT) {
        s_failCount++;
        LogBot("Run failed (returned to outpost), restarting");
        return BotState::InTown;
    }

    return BotState::InDungeon;
}

BotState HandleLoot(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Loot collection");
    // Loot is handled inline during waypoint traversal
    // This state handles post-boss cleanup
    WaitMs(2000);
    return BotState::Merchant;
}

BotState HandleMerchant(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Merchant (return to outpost)");

    // Return to Gadd's Encampment
    MapMgr::Travel(MAP_GADDS_ENCAMPMENT);
    WaitMs(10000);

    // TODO: sell junk, craft consumables, storage management
    // For now, just proceed to next run
    return BotState::InTown;
}

BotState HandleMaintenance(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: Maintenance");
    // TODO: check consumables, blessings, morale
    return BotState::Traveling;
}

BotState HandleError(BotConfig& cfg) {
    (void)cfg;
    LogBot("State: ERROR — waiting 10s before retry");
    WaitMs(10000);

    // Try to recover by going back to town
    uint32_t mapId = MapMgr::GetMapId();
    if (mapId == 0) {
        return BotState::CharSelect;
    }
    return BotState::InTown;
}

// ===== Registration =====

void Register() {
    Bot::RegisterStateHandler(BotState::CharSelect, HandleCharSelect);
    Bot::RegisterStateHandler(BotState::InTown, HandleTownSetup);
    Bot::RegisterStateHandler(BotState::Traveling, HandleTravel);
    Bot::RegisterStateHandler(BotState::InDungeon, HandleDungeon);
    Bot::RegisterStateHandler(BotState::Looting, HandleLoot);
    Bot::RegisterStateHandler(BotState::Merchant, HandleMerchant);
    Bot::RegisterStateHandler(BotState::Maintenance, HandleMaintenance);
    Bot::RegisterStateHandler(BotState::Error, HandleError);

    // Default hero config (Standard.txt heroes)
    auto& cfg = Bot::GetConfig();
    cfg.hero_ids[0] = 25; // Xandra
    cfg.hero_ids[1] = 14; // Olias
    cfg.hero_ids[2] = 21; // Livia
    cfg.hero_ids[3] = 4;  // Master of Whispers
    cfg.hero_ids[4] = 24; // Gwen
    cfg.hero_ids[5] = 15; // Norgu
    cfg.hero_ids[6] = 1;  // Razah
    cfg.hard_mode = true;
    cfg.target_map_id = MAP_BOGROOT_LVL1;
    cfg.outpost_map_id = MAP_GADDS_ENCAMPMENT;
    cfg.bot_module_name = "FroggyHM";

    s_runCount = 0;
    s_failCount = 0;
    s_wipeCount = 0;
    s_totalStartTime = GetTickCount();

    LogBot("Froggy HM module registered (7 heroes, hard mode)");
}

} // namespace GWA3::Bot::Froggy
