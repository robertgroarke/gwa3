// MaintenanceMgr: inventory maintenance between dungeon runs.
// Mirrors AutoIt Utils-Maintenance.au3 PerformMaintenance() flow.

#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/MerchantMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/game/ItemModelIds.h>
#include <gwa3/game/MapIds.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/Map.h>
#include <gwa3/game/Item.h>

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <cstring>

namespace GWA3::MaintenanceMgr {

static constexpr float kEmbarkEyjaX = 3336.0f;
static constexpr float kEmbarkEyjaY = 627.0f;
static constexpr float kEmbarkKwatX = 3596.0f;
static constexpr float kEmbarkKwatY = 107.0f;
static constexpr float kEmbarkAlcusX = 3704.0f;
static constexpr float kEmbarkAlcusY = -163.0f;
static constexpr float kEmbarkXunlaiX = 2283.0f;
static constexpr float kEmbarkXunlaiY = -2134.0f;
static constexpr float kEmbarkMaterialTraderX = 2933.0f;
static constexpr float kEmbarkMaterialTraderY = -2236.0f;
static constexpr uint16_t kEmbarkMaterialTraderNpcId = 3285;
static constexpr uint32_t kCriticalFreeSlots = 3;
static constexpr uint32_t kQuietAsiaJapanRegion = 4u;
static constexpr uint32_t kQuietAsiaJapanDistrict = 0u;
static constexpr uint32_t kQuietAsiaJapanLanguage = 0u;

// ===== Rare Skin Detection () =====
// Ported from AutoIt RareSkins.au3: model IDs that should never be sold or salvaged.
bool IsRareSkin(uint32_t modelId) {
    // Sorted array of rare skin model IDs for binary search
    static const uint32_t kRareSkins[] = {
        114, 117, 118, 127, 205, 332, 333, 336, 341, 342, 344, 391, 399, 528,
        773, 776, 777, 778, 789, 854, 855, 856, 858, 860, 861, 862, 874, 875,
        928, 942, 943, 944, 945, 947, 949, 951, 952, 953, 954, 955, 956, 958,
        959, 960, 985, 1022, 1052, 1195, 1271, 1315, 1316, 1320, 1321, 1350,
        1452, 1536, 1557, 1953, 1956, 1957, 1958, 1959, 1960, 1961, 1962,
        1963, 1964, 1965, 1966, 1967, 1968, 1969, 1970, 1971, 1972, 1973,
        1974, 1975, 1977, 1985, 1987, 1988, 1989, 1990, 1991, 1992, 1993,
        1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
        2005, 2006, 2007, 2008, 2009, 2039, 2058, 2062, 2129, 2236, 2237,
        2295, 2296, 2331, 2385, 2386, 2387, 2388, 2389, 2421, 2422, 2423,
        2435, 2436, 2437, 2438, 2439, 2440, 2441, 2442, 2443, 2444, 2472,
        2474, 3270, 5901, 5902, 5914, 5916, 5919, 5947, 5949, 6309, 6996,
        7995, 8003, 8084, 8085, 8257, 8431, 8509, 8515, 8547, 8554, 8555,
        8640, 8641, 8785, 11521, 15239, 19219, 19266, 19273, 19286, 19302,
        19310, 19323, 19337, 19338, 19344, 19364, 19379, 19380, 19382, 19385,
        19388, 19400, 19410, 19412, 19413, 19420, 19424, 19429, 21264, 21265,
        21267, 21271, 21272, 21273, 21280, 25918, 26901, 26902, 26910, 26925,
        26930, 26931, 26936, 26956, 26958, 26974, 26986, 26987, 26988, 27001,
        27005, 27006, 27017, 27022, 27030, 27031, 28314, 29110, 29113, 29114,
        29115, 29117, 29119, 30218, 30231, 31167, 35131, 35134, 35136, 35137,
        35139, 35141, 35142, 35143, 35145, 36676, 36985
    };
    static constexpr size_t kCount = sizeof(kRareSkins) / sizeof(kRareSkins[0]);
    // Binary search
    size_t lo = 0, hi = kCount;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (kRareSkins[mid] < modelId) lo = mid + 1;
        else if (kRareSkins[mid] > modelId) hi = mid;
        else return true;
    }
    return false;
}

// Kit and special item model IDs to never sell
static bool IsKit(uint32_t modelId) {
    return modelId == ItemModelIds::SUPERIOR_SALVAGE_KIT ||
           modelId == ItemModelIds::SALVAGE_KIT ||
           modelId == ItemModelIds::EXPERT_SALVAGE_KIT ||
           modelId == ItemModelIds::RARE_SALVAGE_KIT ||
           modelId == ItemModelIds::SUPERIOR_IDENTIFICATION_KIT ||
           modelId == ItemModelIds::ALT_IDENTIFICATION_KIT ||
           modelId == ItemModelIds::ALT_SALVAGE_KIT ||
           modelId == ItemModelIds::IDENTIFICATION_KIT;
}

// Materials to always SELL (AutoIt $SELL_MATERIALS)
static bool IsSellMaterial(uint32_t modelId) {
    return modelId == ItemModelIds::CLOTH ||
           modelId == ItemModelIds::TANNED_HIDE_SQUARE ||
           modelId == ItemModelIds::WOOD_PLANK ||
           modelId == ItemModelIds::CHITIN_FRAGMENT;
}

static bool IsEventStorageItem(uint32_t modelId) {
    switch (modelId) {
    case ItemModelIds::DRAKE_KABOB:
    case ItemModelIds::SCALEFIN_SOUP:
    case ItemModelIds::PAHNAI_SALAD:
    case ItemModelIds::BIRTHDAY_CUPCAKE:
    case ItemModelIds::PUMPKIN_PIE:
    case ItemModelIds::GOLDEN_EGG:
    case ItemModelIds::CANDY_APPLE:
    case ItemModelIds::CANDY_CORN:
    case ItemModelIds::LUNAR_FORTUNE:
    case ItemModelIds::LUNAR_FORTUNE_2:
    case ItemModelIds::WAR_SUPPLIES:
    case ItemModelIds::BLUE_ROCK_CANDY:
    case ItemModelIds::GREEN_ROCK_CANDY:
    case ItemModelIds::RED_ROCK_CANDY:
    case ItemModelIds::FOUR_LEAF_CLOVER:
    case ItemModelIds::HONEYCOMB:
    case ItemModelIds::PUMPKIN_COOKIE:
    case ItemModelIds::SHINING_BLADE_RATION:
    case ItemModelIds::REFINED_JELLY:
    case ItemModelIds::WINTERGREEN_CANDY_CANE:
    case ItemModelIds::RAINBOW_CANDY_CANE:
    case ItemModelIds::PEPPERMINT_CANDY_CANE:
    case ItemModelIds::CHOCOLATE_BUNNY:
    case ItemModelIds::GROG:
    case ItemModelIds::SHAMROCK_ALE:
    case ItemModelIds::TRICK_OR_TREAT_BAG:
    case ItemModelIds::APPLE_CIDER:
    case ItemModelIds::BOTTLE_ROCKET:
    case ItemModelIds::CHAMPAGNE_POPPER:
    case ItemModelIds::SPARKLER:
    case ItemModelIds::VICTORY_TOKEN:
    case ItemModelIds::HUNTERS_ALE:
    case ItemModelIds::KRYTAN_BRANDY:
    case ItemModelIds::BLUE_DRINK:
    case ItemModelIds::FRUITCAKE:
    case ItemModelIds::SPIKED_EGGNOG:
    case ItemModelIds::EGGNOG:
    case ItemModelIds::SNOWMAN_SUMMONER:
    case ItemModelIds::FROSTY_TONIC:
    case ItemModelIds::MISCHIEVOUS_TONIC:
    case ItemModelIds::DELICIOUS_CAKE:
    case ItemModelIds::ICED_TEA:
    case ItemModelIds::PARTY_BEACON:
        return true;
    default:
        return false;
    }
}

static bool IsTomeStorageItem(uint32_t modelId) {
    return modelId >= ItemModelIds::ELITE_WARRIOR_TOME &&
           modelId <= ItemModelIds::DERVISH_TOME;
}

static bool IsRareMaterialStorageItem(uint32_t modelId) {
    switch (modelId) {
    case ItemModelIds::CHARCOAL:
    case ItemModelIds::MONSTROUS_CLAW:
    case ItemModelIds::LINEN:
    case ItemModelIds::DAMASK:
    case ItemModelIds::SILK:
    case ItemModelIds::GLOB_OF_ECTOPLASM:
    case ItemModelIds::MONSTROUS_EYE:
    case ItemModelIds::MONSTROUS_FANG:
    case ItemModelIds::DIAMOND:
    case ItemModelIds::ONYX_GEMSTONE:
    case ItemModelIds::RUBY:
    case ItemModelIds::SAPPHIRE:
    case ItemModelIds::GLASS_VIAL:
    case ItemModelIds::FUR_SQUARE:
    case ItemModelIds::LEATHER_SQUARE:
    case ItemModelIds::ELONIAN_LEATHER_SQUARE:
    case ItemModelIds::VIAL_OF_INK:
    case ItemModelIds::OBSIDIAN_SHARD:
    case ItemModelIds::STEEL_INGOT:
    case ItemModelIds::DELDRIMOR_STEEL_INGOT:
    case ItemModelIds::ROLL_OF_PARCHMENT:
    case ItemModelIds::ROLL_OF_VELLUM:
    case ItemModelIds::SPIRITWOOD_PLANK:
    case ItemModelIds::AMBER_CHUNK:
    case ItemModelIds::JADEITE_SHARD:
        return true;
    default:
        return false;
    }
}

static bool IsPolymockPieceStorageItem(uint32_t modelId) {
    switch (modelId) {
    case ItemModelIds::POLYMOCK_ALOE_SEED:
    case ItemModelIds::POLYMOCK_WIND_RIDER:
    case ItemModelIds::POLYMOCK_EARTH_ELEMENTAL:
    case ItemModelIds::POLYMOCK_FIRE_ELEMENTAL:
    case ItemModelIds::POLYMOCK_FIRE_IMP:
    case ItemModelIds::POLYMOCK_GAKI:
    case ItemModelIds::POLYMOCK_GARGOYLE:
    case ItemModelIds::POLYMOCK_MIRAGE_IBOGA:
    case ItemModelIds::POLYMOCK_ICE_ELEMENTAL:
    case ItemModelIds::POLYMOCK_ICE_IMP:
    case ItemModelIds::POLYMOCK_KAPPA:
    case ItemModelIds::POLYMOCK_MERGOYLE:
    case ItemModelIds::POLYMOCK_MURSAAT_ELEMENTALIST:
    case ItemModelIds::POLYMOCK_RUBY_DJINN:
    case ItemModelIds::POLYMOCK_NAGA_SHAMAN:
    case ItemModelIds::POLYMOCK_SKALE:
    case ItemModelIds::POLYMOCK_STONE_RAIN:
        return true;
    default:
        return false;
    }
}

static bool IsGeneralXunlaiStorageItem(uint32_t modelId) {
    return IsEventStorageItem(modelId) ||
           IsTomeStorageItem(modelId) ||
           IsRareMaterialStorageItem(modelId) ||
           IsPolymockPieceStorageItem(modelId);
}

static bool IsForcedVendorSellModel(uint32_t modelId) {
    return modelId == ItemModelIds::BOGROOT_ROD ||
           modelId == ItemModelIds::BOGROOT_STAFF ||
           modelId == ItemModelIds::BOGROOT_FOCUS;
}

// ===== Helpers =====

static void WaitMs(uint32_t ms) { Sleep(ms); }

static bool HasSaneBackpackInventoryRoot() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return false;

    __try {
        Bag* backpack = inv->bags[1];
        if (!backpack || !backpack->items.buffer) return false;
        if (backpack->items.size == 0u || backpack->items.size > 25u) return false;
        if (backpack->items_count > backpack->items.size) return false;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool WaitForMaintenanceMapRuntimeReady(uint32_t mapId, uint32_t timeoutMs, const char* context) {
    const DWORD start = GetTickCount();
    DWORD stableStart = 0u;
    while ((GetTickCount() - start) < timeoutMs) {
        bool ready = false;
        if (MapMgr::GetMapId() == mapId &&
            MapMgr::GetLoadingState() == 1 &&
            MapMgr::GetIsMapLoaded() &&
            AgentMgr::GetMyId() > 0u) {
            auto* me = AgentMgr::GetMyAgent();
            if (me && me->x != 0.0f && me->y != 0.0f && HasSaneBackpackInventoryRoot()) {
                ready = true;
            }
        }

        if (ready) {
            if (stableStart == 0u) {
                stableStart = GetTickCount();
            }
            if ((GetTickCount() - stableStart) >= 1500u) {
                return true;
            }
        } else {
            stableStart = 0u;
        }
        WaitMs(250);
    }
    Log::Warn("MaintenanceMgr: %s runtime not ready map=%u currentMap=%u loaded=%d myId=%u inventoryReady=%d",
              context ? context : "maintenance map",
              mapId,
              MapMgr::GetMapId(),
              MapMgr::GetIsMapLoaded() ? 1 : 0,
              AgentMgr::GetMyId(),
              HasSaneBackpackInventoryRoot() ? 1 : 0);
    return false;
}

static bool MoveNear(float x, float y, float tolerance, uint32_t timeoutMs, const char* label) {
    constexpr float kStuckProgressThreshold = 25.0f;
    constexpr int kStuckLimit = 4;
    constexpr float kBacktrackDistance = 250.0f;
    constexpr float kSidestepDistance = 350.0f;

    const DWORD start = GetTickCount();
    DWORD lastMove = 0;
    float prevX = 0.0f;
    float prevY = 0.0f;
    bool havePrev = false;
    int stuckCount = 0;
    int lateralSign = 1;

    auto queueMove = [](float moveX, float moveY) {
        if (GameThread::IsInitialized()) {
            GameThread::EnqueuePost([moveX, moveY]() {
                AgentMgr::Move(moveX, moveY);
            });
        } else {
            AgentMgr::Move(moveX, moveY);
        }
    };

    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = AgentMgr::GetMyAgent();
        if (me) {
            const float dist = AgentMgr::GetDistance(me->x, me->y, x, y);
            if (dist <= tolerance) {
                return true;
            }

            if (havePrev) {
                const float moved = AgentMgr::GetDistance(prevX, prevY, me->x, me->y);
                if (moved < kStuckProgressThreshold) {
                    ++stuckCount;
                } else {
                    stuckCount = 0;
                }
            }

            const DWORD now = GetTickCount();
            if (stuckCount >= kStuckLimit) {
                const float dx = x - me->x;
                const float dy = y - me->y;
                const float len = std::sqrt(dx * dx + dy * dy);
                const float ndx = (len > 0.01f) ? dx / len : 1.0f;
                const float ndy = (len > 0.01f) ? dy / len : 0.0f;
                const float backtrackX = me->x - ndx * kBacktrackDistance;
                const float backtrackY = me->y - ndy * kBacktrackDistance;
                const float sidestepX = me->x + (-ndy * lateralSign * kSidestepDistance);
                const float sidestepY = me->y + (ndx * lateralSign * kSidestepDistance);

                Log::Info("MaintenanceMgr: MoveNear unstuck for %s player=(%.0f, %.0f) target=(%.0f, %.0f) backtrack=(%.0f, %.0f) sidestep=(%.0f, %.0f)",
                          label ? label : "target", me->x, me->y, x, y, backtrackX, backtrackY, sidestepX, sidestepY);
                queueMove(backtrackX, backtrackY);
                WaitMs(1200);
                queueMove(sidestepX, sidestepY);
                lateralSign = -lateralSign;
                stuckCount = 0;
                havePrev = false;
                lastMove = now;
            } else if (lastMove == 0 || (now - lastMove) >= 500) {
                queueMove(x, y);
                lastMove = now;
            }

            prevX = me->x;
            prevY = me->y;
            havePrev = true;
        }
        WaitMs(150);
    }
    Log::Warn("MaintenanceMgr: MoveNear failed for %s target=(%.0f, %.0f) tol=%.0f map=%u",
              label ? label : "target", x, y, tolerance, MapMgr::GetMapId());
    return false;
}

static uint32_t FindNearestNpcByAllegiance(float x, float y, float maxDist) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    uint32_t bestId = 0;
    float bestDistSq = maxDist * maxDist;
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 6 || living->hp <= 0.0f) continue;
        const float distSq = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = i;
        }
    }
    return bestId;
}

static uint32_t FindNpcByPlayerNumberNearCoords(uint16_t playerNumber, float x, float y, float maxDist) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    uint32_t bestId = 0;
    float bestDistSq = maxDist * maxDist;
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 6 || living->hp <= 0.0f) continue;
        if (living->player_number != playerNumber) continue;
        const float distSq = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = i;
        }
    }
    return bestId;
}

static bool OpenNpcDialog(uint32_t npcId, const char* label, bool requireMerchantItems = true) {
    for (int attempt = 1; attempt <= 3; ++attempt) {
        AgentMgr::ChangeTarget(npcId);
        WaitMs(250);
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        WaitMs(2000);
        if (!requireMerchantItems || MerchantMgr::GetMerchantItemCount() > 0) {
            Log::Info("MaintenanceMgr: Opened %s NPC=%u attempt=%d items=%u",
                      label ? label : "NPC", npcId, attempt, MerchantMgr::GetMerchantItemCount());
            return true;
        }
        if (attempt == 2) {
            AgentMgr::InteractNPC(npcId);
            WaitMs(2000);
            if (!requireMerchantItems || MerchantMgr::GetMerchantItemCount() > 0) {
                Log::Info("MaintenanceMgr: Opened %s NPC=%u via native interact items=%u",
                          label ? label : "NPC", npcId, MerchantMgr::GetMerchantItemCount());
                return true;
            }
        }
        WaitMs(500);
    }
    Log::Warn("MaintenanceMgr: Failed to open %s NPC=%u", label ? label : "NPC", npcId);
    return false;
}

static bool HasOpenMaterialTraderInventory() {
    if (MerchantMgr::GetMerchantItemCount() == 0) return false;
    static const uint32_t kConsetCrafterProductModels[] = {
        ItemModelIds::GRAIL_OF_MIGHT,
        ItemModelIds::ESSENCE_OF_CELERITY,
        ItemModelIds::ARMOR_OF_SALVATION,
    };
    for (uint32_t modelId : kConsetCrafterProductModels) {
        if (MerchantMgr::GetMerchantItemIdByModelId(modelId) != 0) {
            return false;
        }
    }

    static const uint32_t kTraderMaterialModels[] = {
        ItemModelIds::BONES,
        ItemModelIds::IRON_INGOT,
        ItemModelIds::TANNED_HIDE_SQUARE,
        ItemModelIds::SCALES,
        ItemModelIds::CHITIN_FRAGMENT,
        ItemModelIds::CLOTH,
        ItemModelIds::WOOD_PLANK,
        ItemModelIds::GRANITE_SLAB,
        ItemModelIds::DUST,
        ItemModelIds::PLANT_FIBRES,
        ItemModelIds::FEATHERS
    };
    for (uint32_t modelId : kTraderMaterialModels) {
        if (MerchantMgr::GetMerchantItemIdByModelId(modelId) != 0) {
            return true;
        }
    }
    return false;
}

static bool OpenConfiguredMaterialTrader(const Config& cfg, uint32_t& traderNpc) {
    if (HasOpenMaterialTraderInventory()) return true;
    if (cfg.materialTraderX == 0.0f && cfg.materialTraderY == 0.0f) {
        return false;
    }

    if (!MoveNear(cfg.materialTraderX, cfg.materialTraderY, 250.0f, 12000, "Maintenance Material Trader")) {
        return false;
    }
    if (cfg.materialTraderPlayerNumber != 0) {
        traderNpc = FindNpcByPlayerNumberNearCoords(cfg.materialTraderPlayerNumber,
                                                    cfg.materialTraderX,
                                                    cfg.materialTraderY,
                                                    1600.0f);
    }
    if (traderNpc == 0) {
        traderNpc = FindNearestNpcByAllegiance(cfg.materialTraderX, cfg.materialTraderY, 1400.0f);
    }
    if (traderNpc == 0) return false;
    for (uint32_t attempt = 1; attempt <= 3; ++attempt) {
        if (OpenNpcDialog(traderNpc, "Maintenance Material Trader", true) && HasOpenMaterialTraderInventory()) {
            return true;
        }
        Log::Warn("MaintenanceMgr: material trader attempt=%u opened non-trader inventory items=%u",
                  attempt, MerchantMgr::GetMerchantItemCount());
        AgentMgr::CancelAction();
        WaitMs(400);
    }
    return false;
}

static uint32_t SellScalesToMaterialTrader(const Config& cfg) {
    if (cfg.materialTraderX == 0.0f && cfg.materialTraderY == 0.0f) return 0;

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t scalePacks = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id != ItemModelIds::SCALES || item->item_id == 0) continue;
            const uint32_t packs = item->quantity / 10u;
            if (packs == 0) continue;

            uint32_t traderNpc = 0;
            if (!OpenConfiguredMaterialTrader(cfg, traderNpc)) {
                Log::Warn("MaintenanceMgr: Failed to open configured material trader for scales");
                return scalePacks;
            }

            Log::Info("MaintenanceMgr: Selling scales to material trader item=%u qty=%u packs=%u",
                      item->item_id, item->quantity, packs);
            if (!MerchantMgr::SellMaterialsToTrader(item->item_id, packs)) {
                Log::Warn("MaintenanceMgr: Scale trader sell failed item=%u packs=%u",
                          item->item_id, packs);
                return scalePacks;
            }
            scalePacks += packs;
            WaitMs(400);
        }
    }

    if (scalePacks > 0) {
        Log::Info("MaintenanceMgr: Sold %u scale packs to material trader", scalePacks);
    }
    return scalePacks;
}

static uint32_t CountBagModelQuantityRange(uint32_t modelId, uint32_t bagStart, uint32_t bagEnd) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t total = 0;
    for (uint32_t bagIdx = bagStart; bagIdx <= bagEnd; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (item && item->model_id == modelId) {
                total += (item->quantity > 0) ? item->quantity : 1;
            }
        }
    }
    return total;
}

static bool FindEmptyStorageSlot(uint32_t& bagId, uint32_t& slot) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return false;

    for (uint32_t storageBag = 8; storageBag <= 16; ++storageBag) {
        Bag* bag = inv->bags[storageBag];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            if (!bag->items.buffer[i]) {
                bagId = storageBag;
                slot = i;
                return true;
            }
        }
    }
    return false;
}

static bool IsItemInBagRange(const Item* item, uint32_t firstBag, uint32_t lastBag) {
    if (!item || !item->bag) return false;

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return false;

    for (uint32_t bagIdx = firstBag; bagIdx <= lastBag; ++bagIdx) {
        if (inv->bags[bagIdx] == item->bag) return true;
    }
    return false;
}

uint32_t CountFreeSlots() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t freeSlots = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag) continue;
        if (bag->items_count <= bag->items.size) {
            freeSlots += bag->items.size - bag->items_count;
            continue;
        }

        if (!bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            if (!bag->items.buffer[i]) ++freeSlots;
        }
    }
    return freeSlots;
}

uint32_t CountItemByModel(uint32_t modelId) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t total = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (item && item->model_id == modelId) {
                total += (item->quantity > 0) ? item->quantity : 1;
            }
        }
    }
    return total;
}

uint32_t CountItemByModelInStorage(uint32_t modelId) {
    return CountBagModelQuantityRange(modelId, 8u, 16u);
}

static bool WaitForStorageItemInInventory(
    uint32_t itemId,
    uint32_t modelId,
    uint32_t expectedMinCount,
    uint32_t timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        Item* movedItem = ItemMgr::GetItemById(itemId);
        if (movedItem &&
            movedItem->model_id == modelId &&
            IsItemInBagRange(movedItem, 1u, 4u)) {
            return true;
        }

        if (CountItemByModel(modelId) >= expectedMinCount) {
            return true;
        }

        WaitMs(100);
    }
    return false;
}

struct StorageStackSelection {
    Item* item = nullptr;
    uint32_t bag = 0u;
    uint32_t slot = 0u;
    uint32_t quantity = 0u;
};

static uint32_t ItemStackQuantity(const Item* item) {
    return item && item->quantity > 0u ? item->quantity : 1u;
}

static bool FindLargestStorageStackByModel(uint32_t modelId, StorageStackSelection& selection) {
    selection = {};

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return false;

    for (uint32_t storageBag = 8; storageBag <= 16; ++storageBag) {
        Bag* bag = inv->bags[storageBag];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id != modelId) continue;

            const uint32_t quantity = ItemStackQuantity(item);
            if (!selection.item || quantity > selection.quantity) {
                selection.item = item;
                selection.bag = storageBag;
                selection.slot = slot;
                selection.quantity = quantity;
            }
        }
    }

    return selection.item != nullptr;
}

static bool FindEmptyInventorySlot(uint32_t& bagId, uint32_t& slot) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return false;

    for (uint32_t targetBag = 1u; targetBag <= 4u; ++targetBag) {
        Bag* bag = inv->bags[targetBag];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t targetSlot = 0u; targetSlot < bag->items.size; ++targetSlot) {
            if (bag->items.buffer[targetSlot]) continue;
            bagId = targetBag;
            slot = targetSlot;
            return true;
        }
    }

    return false;
}

bool HasCharacterConsetSet(uint32_t targetEach) {
    if (targetEach == 0u) return true;
    return CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT) >= targetEach &&
           CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY) >= targetEach &&
           CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION) >= targetEach;
}

bool NeedsCharacterConsetRestock(const Config& cfg) {
    if (!cfg.enableConsetRestock || cfg.targetCharacterConsetsEach == 0u) {
        return false;
    }
    return !HasCharacterConsetSet(cfg.targetCharacterConsetsEach);
}

static bool NeedsCharacterConsetRebalance(const Config& cfg) {
    if (!cfg.enableConsetRestock || cfg.targetCharacterConsetsEach == 0u) {
        return false;
    }

    const uint32_t grail = CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT);
    const uint32_t essence = CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY);
    const uint32_t armor = CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION);
    if (grail >= cfg.targetCharacterConsetsEach &&
        essence >= cfg.targetCharacterConsetsEach &&
        armor >= cfg.targetCharacterConsetsEach) {
        return false;
    }

    const uint32_t minCount = (grail < essence ? (grail < armor ? grail : armor) : (essence < armor ? essence : armor));
    const uint32_t maxCount = (grail > essence ? (grail > armor ? grail : armor) : (essence > armor ? essence : armor));
    const uint32_t tolerance = cfg.targetCharacterConsetsEach > 25u ? cfg.targetCharacterConsetsEach : 25u;
    return maxCount > tolerance && minCount + tolerance < maxCount;
}

static uint32_t WithdrawStorageModelToInventory(uint32_t modelId, uint32_t targetEach) {
    uint32_t moved = 0u;

    for (uint32_t guard = 0u; guard < 8u; ++guard) {
        const uint32_t current = CountItemByModel(modelId);
        if (current >= targetEach) break;

        StorageStackSelection selected;
        if (!FindLargestStorageStackByModel(modelId, selected)) {
            Log::Warn("MaintenanceMgr: Missing conset model=%u on character and none found in storage",
                      modelId);
            break;
        }

        uint32_t targetBag = 0u;
        uint32_t targetSlot = 0u;
        if (!FindEmptyInventorySlot(targetBag, targetSlot)) {
            Log::Warn("MaintenanceMgr: No empty inventory slot available to withdraw conset model=%u current=%u target=%u selectedQty=%u",
                      modelId,
                      current,
                      targetEach,
                      selected.quantity);
            break;
        }

        const uint32_t itemId = selected.item->item_id;
        const uint32_t expectedMinCount = current + selected.quantity;
        Inventory* inv = ItemMgr::GetInventory();
        Bag* target = inv ? inv->bags[targetBag] : nullptr;
        Log::Info("MaintenanceMgr: Withdrawing conset item=%u model=%u qty=%u current=%u target=%u storageBag=%u storageSlot=%u to bag=%u slot=%u packetBag=%u",
                  itemId,
                  modelId,
                  selected.quantity,
                  current,
                  targetEach,
                  selected.bag,
                  selected.slot,
                  targetBag,
                  targetSlot,
                  target ? target->h0008 : 0u);
        ItemMgr::MoveItem(itemId, targetBag, targetSlot);

        if (WaitForStorageItemInInventory(itemId, modelId, expectedMinCount, 1500u + ChatMgr::GetPing())) {
            const uint32_t inventoryCount = CountItemByModel(modelId);
            Log::Info("MaintenanceMgr: Verified conset withdrawal item=%u model=%u qty=%u inventoryCount=%u storageCount=%u",
                      itemId,
                      modelId,
                      selected.quantity,
                      inventoryCount,
                      CountItemByModelInStorage(modelId));
            ++moved;
            continue;
        }

        Item* after = ItemMgr::GetItemById(itemId);
        Log::Warn("MaintenanceMgr: Conset withdrawal not verified item=%u model=%u qty=%u targetBag=%u targetSlot=%u itemBagIndex=%u itemPacketBag=%u itemSlot=%u inventoryCount=%u storageCount=%u",
                  itemId,
                  modelId,
                  selected.quantity,
                  targetBag,
                  targetSlot,
                  (after && after->bag) ? after->bag->index : 0u,
                  (after && after->bag) ? after->bag->h0008 : 0u,
                  after ? static_cast<uint32_t>(after->slot) : 0u,
                  CountItemByModel(modelId),
                  CountItemByModelInStorage(modelId));
        break;
    }

    return moved;
}

uint32_t WithdrawMissingConsetsFromStorage(uint32_t targetEach) {
    if (targetEach == 0u) return 0u;
    uint32_t moved = 0u;
    moved += WithdrawStorageModelToInventory(ItemModelIds::GRAIL_OF_MIGHT, targetEach);
    moved += WithdrawStorageModelToInventory(ItemModelIds::ESSENCE_OF_CELERITY, targetEach);
    moved += WithdrawStorageModelToInventory(ItemModelIds::ARMOR_OF_SALVATION, targetEach);
    if (moved > 0u) {
        Log::Info("MaintenanceMgr: Withdrew %u missing conset stack(s) from Xunlai", moved);
    }
    return moved;
}

static uint32_t CountAllSalvageKits() {
    return CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT) +
           CountItemByModel(ItemModelIds::SALVAGE_KIT) +
           CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT) +
           CountItemByModel(ItemModelIds::RARE_SALVAGE_KIT) +
           CountItemByModel(ItemModelIds::ALT_SALVAGE_KIT);
}

static uint32_t CountRegularSalvageKits() {
    return CountItemByModel(ItemModelIds::SALVAGE_KIT);
}

static uint32_t CountHighGradeSalvageKits() {
    return CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT) +
           CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT);
}

static uint32_t CountInventoryEventStorageStacks() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t stacks = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (item && IsEventStorageItem(item->model_id)) {
                ++stacks;
            }
        }
    }
    return stacks;
}

static uint32_t CountInventoryGeneralStorageStacks() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t stacks = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (item && IsGeneralXunlaiStorageItem(item->model_id)) {
                ++stacks;
            }
        }
    }
    return stacks;
}

static uint32_t CountInventoryConsetCraftMaterialStacks() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t stacks = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item) continue;
            switch (item->model_id) {
                case ItemModelIds::IRON_INGOT:
                case ItemModelIds::DUST:
                case ItemModelIds::FEATHERS:
                case ItemModelIds::BONES:
                    ++stacks;
                    break;
                default:
                    break;
            }
        }
    }
    return stacks;
}

static bool ShouldCraftConsetsForMaterialSlots(const Config& cfg, uint32_t materialStacks) {
    const bool hasCraftableRecipe =
        (CountItemByModel(ItemModelIds::IRON_INGOT) >= 50u &&
         CountItemByModel(ItemModelIds::DUST) >= 50u) ||
        (CountItemByModel(ItemModelIds::DUST) >= 50u &&
         CountItemByModel(ItemModelIds::FEATHERS) >= 50u) ||
        (CountItemByModel(ItemModelIds::IRON_INGOT) >= 50u &&
         CountItemByModel(ItemModelIds::BONES) >= 50u);

    return cfg.enableConsetRestock &&
           cfg.consetMaterialStackTrigger > 0u &&
           materialStacks >= cfg.consetMaterialStackTrigger &&
           hasCraftableRecipe;
}

static int GetMaterialStorageSlot(uint32_t modelId);

static uint32_t CountLooseConsetStacks() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t stacks = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (!item) continue;
            if (item->model_id == ItemModelIds::GRAIL_OF_MIGHT ||
                item->model_id == ItemModelIds::ESSENCE_OF_CELERITY ||
                item->model_id == ItemModelIds::ARMOR_OF_SALVATION) {
                ++stacks;
            }
        }
    }
    return stacks;
}

static bool HasMaterialStorageCandidate() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return false;

    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (item && item->type == 11 && GetMaterialStorageSlot(item->model_id) >= 0) {
                return true;
            }
        }
    }
    return false;
}

static uint32_t CountVendorSellableItems() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t count = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; ++i) {
            Item* item = bag->items.buffer[i];
            if (item && item->value > 0 && ShouldSellItem(item)) {
                ++count;
            }
        }
    }
    return count;
}

static bool HasFreeSlotMaintenanceAction(const Config& cfg, uint32_t freeSlots) {
    const uint32_t sellable = CountVendorSellableItems();
    if (sellable > 0) {
        Log::Info("MaintenanceMgr: Free-slot action available - sellableItems=%u", sellable);
        return true;
    }

    uint32_t storageBag = 0;
    uint32_t storageSlot = 0;
    if (FindEmptyStorageSlot(storageBag, storageSlot)) {
        if (HasMaterialStorageCandidate()) {
            Log::Info("MaintenanceMgr: Free-slot action available - material storage slot=%u/%u",
                      storageBag, storageSlot);
            return true;
        }
        const uint32_t looseConsets = CountLooseConsetStacks();
        if (looseConsets > 0) {
            Log::Info("MaintenanceMgr: Free-slot action available - looseConsets=%u storage slot=%u/%u",
                      looseConsets, storageBag, storageSlot);
            return true;
        }
        const uint32_t eventStacks = CountInventoryEventStorageStacks();
        if (eventStacks > 0) {
            Log::Info("MaintenanceMgr: Free-slot action available - eventStacks=%u storage slot=%u/%u",
                      eventStacks, storageBag, storageSlot);
            return true;
        }
        const uint32_t generalStorageStacks = CountInventoryGeneralStorageStacks();
        if (generalStorageStacks > 0) {
            Log::Info("MaintenanceMgr: Free-slot action available - generalStorageStacks=%u storage slot=%u/%u",
                      generalStorageStacks, storageBag, storageSlot);
            return true;
        }
    }

    const uint32_t consetMatStacks = CountInventoryConsetCraftMaterialStacks();
    if (ShouldCraftConsetsForMaterialSlots(cfg, consetMatStacks)) {
        Log::Info("MaintenanceMgr: Free-slot action available - consetMatStacks=%u >= materialSlotTrigger=%u freeSlots=%u",
                  consetMatStacks,
                  cfg.consetMaterialStackTrigger,
                  freeSlots);
        return true;
    }

    return false;
}

// ===== Diagnostics =====

bool NeedsMaintenance(const Config& cfg) {
    uint32_t freeSlots = CountFreeSlots();
    if (freeSlots < kCriticalFreeSlots) {
        if (HasFreeSlotMaintenanceAction(cfg, freeSlots)) {
            Log::Info("MaintenanceMgr: Needs maintenance - freeSlots=%u < %u", freeSlots, cfg.minFreeSlots);
            return true;
        }
        Log::Info("MaintenanceMgr: Critical freeSlots=%u < %u but no actionable safe cleanup remains",
                  freeSlots,
                  kCriticalFreeSlots);
    } else if (freeSlots < cfg.minFreeSlots && HasFreeSlotMaintenanceAction(cfg, freeSlots)) {
        Log::Info("MaintenanceMgr: Needs maintenance - freeSlots=%u < %u", freeSlots, cfg.minFreeSlots);
        return true;
    }
    if (freeSlots < cfg.minFreeSlots) {
        Log::Info("MaintenanceMgr: Low freeSlots=%u < %u but no actionable safe cleanup remains; allowing run",
                  freeSlots, cfg.minFreeSlots);
    }

    const bool hasRoomForKitRestock = freeSlots > cfg.minFreeSlots;
    if (hasRoomForKitRestock) {
        uint32_t idKits = CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT);
        if (idKits < cfg.targetIdKits) {
            Log::Info("MaintenanceMgr: Needs maintenance - superiorIdKits=%u < target %u", idKits, cfg.targetIdKits);
            return true;
        }

        uint32_t salvKits = CountRegularSalvageKits();
        if (salvKits < cfg.targetSalvageKits) {
            Log::Info("MaintenanceMgr: Needs maintenance - salvageKits=%u < target %u", salvKits, cfg.targetSalvageKits);
            return true;
        }

        uint32_t highGradeSalvKits = CountHighGradeSalvageKits();
        if (highGradeSalvKits < cfg.targetExpertSalvageKits) {
            Log::Info("MaintenanceMgr: Needs maintenance - highGradeSalvageKits=%u < target %u",
                      highGradeSalvKits, cfg.targetExpertSalvageKits);
            return true;
        }
    } else {
        Log::Info("MaintenanceMgr: Deferring kit restock; freeSlots=%u <= preferred=%u",
                  freeSlots,
                  cfg.minFreeSlots);
    }
    if (CountItemByModel(ItemModelIds::EXPERT_SALVAGE_KIT) > 0 &&
        CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT) == 0) {
        Log::Info("MaintenanceMgr: Needs maintenance - replacing expert salvage fallback with superior kit");
        return true;
    }

    uint32_t charGold = ItemMgr::GetGoldCharacter();
    if (charGold >= cfg.maxCharacterGold) {
        Log::Info("MaintenanceMgr: Needs maintenance - gold=%u >= %u", charGold, cfg.maxCharacterGold);
        return true;
    }

    const uint32_t eventStacks = CountInventoryEventStorageStacks();
    if (eventStacks > 0) {
        uint32_t storageBag = 0;
        uint32_t storageSlot = 0;
        if (FindEmptyStorageSlot(storageBag, storageSlot)) {
            Log::Info("MaintenanceMgr: Needs maintenance - event storage stacks=%u", eventStacks);
            return true;
        }
        Log::Info("MaintenanceMgr: Event storage stacks present=%u but Xunlai is full; not blocking run start",
                  eventStacks);
    }

    const uint32_t generalStorageStacks = CountInventoryGeneralStorageStacks();
    if (generalStorageStacks > eventStacks) {
        uint32_t storageBag = 0;
        uint32_t storageSlot = 0;
        if (FindEmptyStorageSlot(storageBag, storageSlot)) {
            Log::Info("MaintenanceMgr: Needs maintenance - general storage stacks=%u",
                      generalStorageStacks);
            return true;
        }
        Log::Info("MaintenanceMgr: General storage stacks present=%u but Xunlai is full; not blocking run start",
                  generalStorageStacks);
    }

    const uint32_t consetMatStacks = CountInventoryConsetCraftMaterialStacks();
    if (ShouldCraftConsetsForMaterialSlots(cfg, consetMatStacks)) {
        Log::Info("MaintenanceMgr: Needs maintenance - conset material slots=%u >= trigger=%u freeSlots=%u",
                  consetMatStacks,
                  cfg.consetMaterialStackTrigger,
                  freeSlots);
        return true;
    }
    if (NeedsCharacterConsetRestock(cfg)) {
        Log::Info("MaintenanceMgr: Needs maintenance - character consets below target "
                  "(grail=%u essence=%u armor=%u target=%u stored=%u/%u/%u)",
                  CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT),
                  CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY),
                  CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION),
                  cfg.targetCharacterConsetsEach,
                  CountItemByModelInStorage(ItemModelIds::GRAIL_OF_MIGHT),
                  CountItemByModelInStorage(ItemModelIds::ESSENCE_OF_CELERITY),
                  CountItemByModelInStorage(ItemModelIds::ARMOR_OF_SALVATION));
        return true;
    }
    if (NeedsCharacterConsetRebalance(cfg)) {
        Log::Info("MaintenanceMgr: Needs maintenance - character conset stacks imbalanced "
                  "(grail=%u essence=%u armor=%u target=%u stored=%u/%u/%u)",
                  CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT),
                  CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY),
                  CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION),
                  cfg.targetCharacterConsetsEach,
                  CountItemByModelInStorage(ItemModelIds::GRAIL_OF_MIGHT),
                  CountItemByModelInStorage(ItemModelIds::ESSENCE_OF_CELERITY),
                  CountItemByModelInStorage(ItemModelIds::ARMOR_OF_SALVATION));
        return true;
    }

    return false;
}

// ===== Gold Management =====

void DepositGold(uint32_t keepOnChar) {
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t storageGold = ItemMgr::GetGoldStorage();

    if (charGold <= keepOnChar) return;

    uint32_t deposit = charGold - keepOnChar;
    // Cap at storage limit (1,000,000)
    if (storageGold + deposit > 1000000) {
        deposit = (storageGold < 1000000) ? (1000000 - storageGold) : 0;
    }
    if (deposit == 0) return;

    uint32_t newChar = charGold - deposit;
    uint32_t newStorage = storageGold + deposit;

    Log::Info("MaintenanceMgr: DepositGold %u (char: %u->%u, storage: %u->%u)",
              deposit, charGold, newChar, storageGold, newStorage);
    ItemMgr::ChangeGold(newChar, newStorage);
    WaitMs(500);
}

void WithdrawGold(uint32_t amount) {
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    uint32_t storageGold = ItemMgr::GetGoldStorage();

    if (amount > storageGold) amount = storageGold;
    // Cap character gold at 100,000
    if (charGold + amount > 100000) amount = 100000 - charGold;
    if (amount == 0) return;

    uint32_t newChar = charGold + amount;
    uint32_t newStorage = storageGold - amount;

    Log::Info("MaintenanceMgr: WithdrawGold %u (char: %u->%u, storage: %u->%u)",
              amount, charGold, newChar, storageGold, newStorage);
    ItemMgr::ChangeGold(newChar, newStorage);
    WaitMs(500);
}

// ===== Xunlai Chest Interaction =====

void OpenXunlaiChest(float chestX, float chestY) {
    // Move to the Xunlai chest NPC
    GameThread::EnqueuePost([chestX, chestY]() {
        AgentMgr::Move(chestX, chestY);
    });
    for (int tick = 0; tick < 30; tick++) {
        WaitMs(500);
        auto* me = AgentMgr::GetMyAgent();
        if (me && AgentMgr::GetDistance(me->x, me->y, chestX, chestY) < 350.0f) break;
    }

    // Find the Xunlai NPC
    uint32_t maxAgents = AgentMgr::GetMaxAgents();
    float bestDist = 900.0f * 900.0f;
    uint32_t chestId = 0;
    for (uint32_t i = 1; i < maxAgents; i++) {
        auto* a = AgentMgr::GetAgentByID(i);
        if (!a || a->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(a);
        if (living->allegiance != 6 || living->hp <= 0.0f) continue;
        float d = AgentMgr::GetSquaredDistance(chestX, chestY, living->x, living->y);
        if (d < bestDist) { bestDist = d; chestId = living->agent_id; }
    }

    if (!chestId) {
        Log::Warn("MaintenanceMgr: No Xunlai chest NPC found near (%.0f, %.0f)", chestX, chestY);
        return;
    }

    // Use raw GoNPC packet (0x39), matching the proven AutoIt merchant interaction.
    // AgentMgr::InteractNPC uses the native function which opens a UI dialog that
    // disrupts player agent state (causes position reads to return 0,0).
    Log::Info("MaintenanceMgr: Opening Xunlai chest via GoNPC (agent=%u)", chestId);
    AgentMgr::ChangeTarget(chestId);
    WaitMs(250);
    CtoS::SendPacket(3, Packets::INTERACT_NPC, chestId, 0u);
    ItemMgr::MarkXunlaiOpened();
    WaitMs(2000);

    // Close the storage dialog; we only need the chest activated for MoveItem packets.
    // The dialog itself blocks agent reads if left open.
    AgentMgr::CancelAction();
    WaitMs(500);
}

// ===== Material Storage Deposit =====

// Material storage slot mapping (bag 6), from AutoIt MATERIALS_DOUBLE_ARRAY.
// Maps model_id to slot index in material storage bag.
static int GetMaterialStorageSlot(uint32_t modelId) {
    switch (modelId) {
        case ItemModelIds::BONES:               return 1;
        case ItemModelIds::IRON_INGOT:          return 2;
        case ItemModelIds::TANNED_HIDE_SQUARE:  return 3;
        case ItemModelIds::SCALES:              return 4;
        case ItemModelIds::CHITIN_FRAGMENT:     return 5;
        case ItemModelIds::CLOTH:               return 6;
        case ItemModelIds::WOOD_PLANK:          return 7;
        // slot 8 = not used for basic materials
        case ItemModelIds::GRANITE_SLAB:        return 9;
        case ItemModelIds::DUST:                return 10;
        case ItemModelIds::PLANT_FIBRES:        return 11;
        case ItemModelIds::FEATHERS:            return 12;
        case ItemModelIds::FUR_SQUARE:          return 13;
        case ItemModelIds::LINEN:               return 14;
        case ItemModelIds::DAMASK:              return 15;
        case ItemModelIds::SILK:                return 16;
        case ItemModelIds::GLOB_OF_ECTOPLASM:   return 17;
        case ItemModelIds::STEEL_INGOT:         return 18;
        case ItemModelIds::DELDRIMOR_STEEL_INGOT: return 19;
        case ItemModelIds::MONSTROUS_CLAW:      return 20;
        case ItemModelIds::MONSTROUS_EYE:       return 21;
        case ItemModelIds::MONSTROUS_FANG:      return 22;
        case ItemModelIds::RUBY:                return 23;
        case ItemModelIds::SAPPHIRE:            return 24;
        case ItemModelIds::DIAMOND:             return 25;
        case ItemModelIds::ONYX_GEMSTONE:       return 26;
        case ItemModelIds::CHARCOAL:            return 27;
        case ItemModelIds::OBSIDIAN_SHARD:      return 28;
        // slot 29 = not used in the legacy material map
        case ItemModelIds::GLASS_VIAL:          return 30;
        case ItemModelIds::LEATHER_SQUARE:      return 31;
        case ItemModelIds::ELONIAN_LEATHER_SQUARE: return 32;
        case ItemModelIds::VIAL_OF_INK:         return 33;
        case ItemModelIds::ROLL_OF_PARCHMENT:   return 34;
        case ItemModelIds::ROLL_OF_VELLUM:      return 35;
        case ItemModelIds::SPIRITWOOD_PLANK:    return 36;
        case ItemModelIds::AMBER_CHUNK:         return 37;
        case ItemModelIds::JADEITE_SHARD:       return 38;
        default: return -1;
    }
}

static bool IsMaterialStorageSlotFull(uint32_t modelId, int oneBasedSlot) {
    if (oneBasedSlot <= 0) return true;

    Inventory* inv = ItemMgr::GetInventory();
    Bag* materialStorage = inv ? inv->bags[6] : nullptr;
    if (!materialStorage || !materialStorage->items.buffer) return false;

    const uint32_t packetSlot = static_cast<uint32_t>(oneBasedSlot - 1);
    if (packetSlot >= materialStorage->items.size) return false;

    Item* stored = materialStorage->items.buffer[packetSlot];
    if (!stored || stored->model_id != modelId) return false;

    return stored->quantity >= 250u;
}

uint32_t DepositMaterialsToStorage() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t deposited = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;

            // Only deposit materials (type 11 = material)
            if (item->type != 11) continue;

            int slot = GetMaterialStorageSlot(item->model_id);
            if (slot < 0) continue;

            if (IsMaterialStorageSlotFull(item->model_id, slot)) {
                Log::Info("MaintenanceMgr: Material storage slot full; skipping item=%u model=%u qty=%u slot=%d",
                          item->item_id,
                          item->model_id,
                          item->quantity,
                          slot);
                continue;
            }

            // AutoIt uses 1-based slot, but MoveItem packet needs 0-based (slot - 1)
            int packetSlot = slot - 1;
            Log::Info("MaintenanceMgr: Depositing material=%u model=%u qty=%u to bag 6 slot %d (packet=%d)",
                      item->item_id, item->model_id, item->quantity, slot, packetSlot);
            ItemMgr::MoveItem(item->item_id, 6, packetSlot);
            WaitMs(300);
            deposited++;
        }
    }
    if (deposited > 0) {
        Log::Info("MaintenanceMgr: Deposited %u material stacks to storage", deposited);
    }
    return deposited;
}

uint32_t DepositItemModelsToStorage(const uint32_t* modelIds, uint32_t modelCount) {
    if (!modelIds || modelCount == 0) return 0;

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t moved = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;

        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id == 0) continue;

            bool match = false;
            for (uint32_t i = 0; i < modelCount; ++i) {
                if (item->model_id == modelIds[i]) {
                    match = true;
                    break;
                }
            }
            if (!match) continue;

            uint32_t storageBag = 0;
            uint32_t storageSlot = 0;
            if (!FindEmptyStorageSlot(storageBag, storageSlot)) {
                Log::Warn("MaintenanceMgr: No empty Xunlai slot available for model=%u", item->model_id);
                return moved;
            }

            Log::Info("MaintenanceMgr: Depositing item=%u model=%u to storage bag=%u slot=%u",
                      item->item_id, item->model_id, storageBag, storageSlot);
            ItemMgr::MoveItem(item->item_id, storageBag, storageSlot);
            WaitMs(400);
            moved++;
        }
    }

    if (moved > 0) {
        Log::Info("MaintenanceMgr: Deposited %u matching stacks to Xunlai", moved);
    }
    return moved;
}

static uint32_t DepositExcessConsetsToStorage(uint32_t keepEach) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    const uint32_t consetModels[3] = {
        ItemModelIds::GRAIL_OF_MIGHT,
        ItemModelIds::ESSENCE_OF_CELERITY,
        ItemModelIds::ARMOR_OF_SALVATION,
    };

    uint32_t moved = 0u;
    for (uint32_t modelIdx = 0; modelIdx < _countof(consetModels); ++modelIdx) {
        const uint32_t modelId = consetModels[modelIdx];
        uint32_t current = CountItemByModel(modelId);
        if (current <= keepEach) {
            continue;
        }

        for (uint32_t bagIdx = 1; bagIdx <= 4 && current > keepEach; ++bagIdx) {
            Bag* bag = inv->bags[bagIdx];
            if (!bag || !bag->items.buffer) continue;

            for (uint32_t slot = 0; slot < bag->items.size && current > keepEach; ++slot) {
                Item* item = bag->items.buffer[slot];
                if (!item || item->model_id != modelId) continue;

                const uint32_t quantity = (item->quantity > 0) ? item->quantity : 1u;
                if (current <= quantity || current - quantity < keepEach) {
                    continue;
                }

                uint32_t storageBag = 0u;
                uint32_t storageSlot = 0u;
                if (!FindEmptyStorageSlot(storageBag, storageSlot)) {
                    Log::Warn("MaintenanceMgr: No empty Xunlai slot available for excess conset model=%u", modelId);
                    return moved;
                }

                Log::Info("MaintenanceMgr: Depositing excess conset item=%u model=%u qty=%u keep=%u to storage bag=%u slot=%u",
                          item->item_id, modelId, quantity, keepEach, storageBag, storageSlot);
                ItemMgr::MoveItem(item->item_id, storageBag, storageSlot);
                WaitMs(400);
                current = current > quantity ? (current - quantity) : 0u;
                ++moved;
            }
        }
    }

    if (moved > 0) {
        Log::Info("MaintenanceMgr: Deposited %u excess conset stacks to Xunlai", moved);
    }
    return moved;
}

static uint32_t DepositGeneralStorageItemsToXunlai() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t moved = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;

        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id == 0) continue;
            if (!IsGeneralXunlaiStorageItem(item->model_id)) continue;

            uint32_t storageBag = 0;
            uint32_t storageSlot = 0;
            if (!FindEmptyStorageSlot(storageBag, storageSlot)) {
                Log::Warn("MaintenanceMgr: No empty Xunlai slot available for general storage item model=%u",
                          item->model_id);
                return moved;
            }

            Log::Info("MaintenanceMgr: Depositing general storage item=%u model=%u type=%u to storage bag=%u slot=%u",
                      item->item_id, item->model_id, item->type, storageBag, storageSlot);
            ItemMgr::MoveItem(item->item_id, storageBag, storageSlot);
            WaitMs(400);
            ++moved;
        }
    }

    if (moved > 0) {
        Log::Info("MaintenanceMgr: Deposited %u general storage stacks to Xunlai", moved);
    }
    return moved;
}

// ===== Item Classification (matches AutoIt GWA2_ID_Items.au3) =====

// Rarity constants from name_enc first ushort (AutoIt GWA2_ID_Items.au3)
static constexpr uint16_t RARITY_WHITE  = kItemRarityWhite;
static constexpr uint16_t RARITY_GRAY   = kItemRarityGray;
static constexpr uint16_t RARITY_BLUE   = kItemRarityBlue;
static constexpr uint16_t RARITY_GOLD   = kItemRarityGold;
static constexpr uint16_t RARITY_PURPLE = kItemRarityPurple;
static constexpr uint16_t RARITY_GREEN  = kItemRarityGreen;

// Item type IDs (AutoIt GWA2_ID_Items.au3)
static constexpr uint8_t TYPE_AXE       = 2;
static constexpr uint8_t TYPE_FOOT      = 4;
static constexpr uint8_t TYPE_BOW       = 5;
static constexpr uint8_t TYPE_CHEST     = 7;
static constexpr uint8_t TYPE_MATERIAL  = 11;
static constexpr uint8_t TYPE_OFFHAND   = 12;
static constexpr uint8_t TYPE_HAND      = 13;
static constexpr uint8_t TYPE_HAMMER    = 15;
static constexpr uint8_t TYPE_HEAD      = 16;
static constexpr uint8_t TYPE_RUNE_MOD  = 8;
static constexpr uint8_t TYPE_LEG       = 19;
static constexpr uint8_t TYPE_WAND      = 22;
static constexpr uint8_t TYPE_SHIELD    = 24;
static constexpr uint8_t TYPE_STAFF     = 26;
static constexpr uint8_t TYPE_SWORD     = 27;
static constexpr uint8_t TYPE_DAGGER    = 32;
static constexpr uint8_t TYPE_SCYTHE    = 35;
static constexpr uint8_t TYPE_SPEAR     = 36;

// GetRarity: reads name_enc first ushort (AutoIt GetRarity at offset +56)
// Our Item struct has name_enc at offset +0x34 = 52, but complete_name_enc at +0x38 = 56
// AutoIt reads ptr at offset 56 (complete_name_enc), then reads ushort at that ptr.
static uint16_t GetRarity(Item* item) {
    if (!item) return 0;
    // AutoIt: MemoryRead(GetItemPtr($aItem) + 56, "ptr") name string ushort.
    // In our struct: +56 = 0x38 = complete_name_enc
    wchar_t* nameStr = item->complete_name_enc;
    if (!nameStr) nameStr = item->name_enc; // fallback
    if (!nameStr) return 0;
    return static_cast<uint16_t>(nameStr[0]);
}

// IsIdentified: checks interaction field bit 0 (AutoIt: Interaction & 0x1)
static bool IsIdentified(Item* item) {
    if (!item) return false;
    return (item->interaction & 0x1) != 0;
}

// IsWeapon: checks if item type is a weapon type
static bool IsWeapon(Item* item) {
    if (!item) return false;
    switch (item->type) {
        case TYPE_AXE: case TYPE_BOW: case TYPE_OFFHAND: case TYPE_HAMMER:
        case TYPE_WAND: case TYPE_SHIELD: case TYPE_STAFF: case TYPE_SWORD:
        case TYPE_DAGGER: case TYPE_SCYTHE: case TYPE_SPEAR:
            return true;
        default: return false;
    }
}

static bool IsLowValueGreenWeaponForVendor(const Item* item, uint16_t rarity) {
    if (!item || rarity != RARITY_GREEN) return false;
    if (item->value == 0u || item->value > 100u) return false;
    if (item->equipped || item->customized) return false;
    return IsWeapon(const_cast<Item*>(item));
}

// IsArmor: checks if item type is an armor type
static bool IsArmor(Item* item) {
    if (!item) return false;
    switch (item->type) {
        case TYPE_FOOT: case TYPE_CHEST: case TYPE_HAND: case TYPE_HEAD: case TYPE_LEG:
            return true;
        default: return false;
    }
}

static constexpr uint64_t kStaffMask = UpgradeSalvageItemTypeMask(TYPE_STAFF);
static constexpr uint64_t kWandMask = UpgradeSalvageItemTypeMask(TYPE_WAND);
static constexpr uint64_t kFocusMask = UpgradeSalvageItemTypeMask(TYPE_OFFHAND);
static constexpr uint64_t kShieldMask = UpgradeSalvageItemTypeMask(TYPE_SHIELD);
static constexpr uint64_t kArmorRuneMask =
    UpgradeSalvageItemTypeMask(0) |
    UpgradeSalvageItemTypeMask(TYPE_RUNE_MOD) |
    UpgradeSalvageItemTypeMask(TYPE_FOOT) |
    UpgradeSalvageItemTypeMask(TYPE_CHEST) |
    UpgradeSalvageItemTypeMask(TYPE_HAND) |
    UpgradeSalvageItemTypeMask(TYPE_HEAD) |
    UpgradeSalvageItemTypeMask(TYPE_LEG);

static constexpr UpgradeSalvageRule kDefaultUpgradeSalvageRules[] = {
    {true, "HCT20 [Inscription]", "22500140828", 2, kStaffMask | kWandMask, RARITY_GOLD},
    {true, "HSR20 [Inscription]", "00142828", 2, kFocusMask, RARITY_GOLD},
    {true, "+30 HP", "001E4823", 1, kShieldMask, RARITY_GOLD},
    {true, "+30 HP (staff wrapping)", "9013025001E4823", 1, kStaffMask, RARITY_GOLD},
    {true, "+30 HP (staff head)", "A013025001E4823", 0, kStaffMask, RARITY_GOLD},
    {true, "Rune of Superior Vigor", "C202EA27", 1, kArmorRuneMask, kItemRarityAny},
    {true, "Rune of Major Vigor", "C202E927", 1, kArmorRuneMask, kItemRarityAny},
    {true, "Rune of Clarity", "01087827", 1, kArmorRuneMask, kItemRarityAny},
    {true, "Blessed Insignia", "E9010824", 0, kArmorRuneMask, kItemRarityAny},
};

static uint8_t RarityRank(uint16_t rarity) {
    switch (rarity) {
    case RARITY_WHITE: return 1;
    case RARITY_BLUE: return 2;
    case RARITY_PURPLE: return 3;
    case RARITY_GOLD: return 4;
    case RARITY_GREEN: return 5;
    default: return 0;
    }
}

static bool RarityMatchesMinimum(uint16_t rarity, uint16_t minimumRarity) {
    if (minimumRarity == kItemRarityAny) return true;
    const uint8_t itemRank = RarityRank(rarity);
    const uint8_t minRank = RarityRank(minimumRarity);
    return itemRank != 0 && minRank != 0 && itemRank >= minRank;
}

static int HexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static bool IsPatternSeparator(char c) {
    return c == '|' || c == ',' || c == ';';
}

static bool IsPatternWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool ModStructContainsHexSegment(const Item* item, const char* begin, const char* end) {
    if (!item || !item->mod_struct || !begin || begin >= end) return false;

    uint8_t pattern[64]{};
    size_t patternLen = 0;
    int highNibble = -1;
    bool sawDigit = false;

    for (const char* p = begin; p < end; ++p) {
        if (*p == '0' && p + 1 < end && (p[1] == 'x' || p[1] == 'X') && highNibble < 0) {
            ++p;
            continue;
        }
        const int nibble = HexNibble(*p);
        if (nibble < 0) {
            if (IsPatternWhitespace(*p)) continue;
            return false;
        }

        sawDigit = true;
        if (highNibble < 0) {
            highNibble = nibble;
        } else {
            if (patternLen >= sizeof(pattern)) return false;
            pattern[patternLen++] = static_cast<uint8_t>((highNibble << 4) | nibble);
            highNibble = -1;
        }
    }

    if (!sawDigit || highNibble >= 0 || patternLen == 0) return false;

    const size_t modDwords = item->mod_struct_size;
    if (modDwords == 0 || modDwords > 256) return false;
    const size_t byteCount = modDwords * sizeof(ItemModifier);
    if (patternLen > byteCount) return false;

    __try {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(item->mod_struct);
        for (size_t i = 0; i + patternLen <= byteCount; ++i) {
            if (std::memcmp(bytes + i, pattern, patternLen) == 0) {
                return true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return false;
}

static bool ModStructContainsAnyHexPattern(const Item* item, const char* pattern) {
    if (!item || !pattern || !*pattern) return false;

    const char* segmentStart = pattern;
    for (const char* p = pattern; ; ++p) {
        if (*p == '\0' || IsPatternSeparator(*p)) {
            if (ModStructContainsHexSegment(item, segmentStart, p)) return true;
            if (*p == '\0') break;
            segmentStart = p + 1;
        }
    }

    return false;
}

UpgradeSalvageMatch FindUpgradeSalvageMatch(const Item* item, const Config& cfg) {
    UpgradeSalvageMatch match{};
    if (!item || item->item_id == 0 || item->model_id == 0) return match;
    if (!cfg.salvageMatchedUpgradesBeforeMaterials) return match;
    if (item->equipped || item->customized) return match;
    if (!IsIdentified(const_cast<Item*>(item))) return match;
    if (IsKit(item->model_id) || IsRareSkin(item->model_id)) return match;

    const uint16_t rarity = GetRarity(const_cast<Item*>(item));
    if (rarity == RARITY_GREEN) return match;

    const bool weapon = IsWeapon(const_cast<Item*>(item));
    const bool armorOrRune =
        IsArmor(const_cast<Item*>(item)) ||
        item->type == TYPE_RUNE_MOD ||
        item->type == 0;
    if (weapon && !ShouldSellItem(item)) return match;
    if (!weapon && !armorOrRune && !ShouldSellItem(item)) return match;

    const UpgradeSalvageRule* rules = cfg.upgradeSalvageRules;
    uint32_t ruleCount = cfg.upgradeSalvageRuleCount;
    if (!rules || ruleCount == 0) {
        rules = kDefaultUpgradeSalvageRules;
        ruleCount = static_cast<uint32_t>(_countof(kDefaultUpgradeSalvageRules));
    }

    const uint64_t itemTypeMask = UpgradeSalvageItemTypeMask(item->type);
    for (uint32_t i = 0; i < ruleCount; ++i) {
        const UpgradeSalvageRule& rule = rules[i];
        if (!rule.enabled || !rule.modPatternHex || !*rule.modPatternHex) continue;
        if (rule.salvageIndex > 2) continue;
        if (rule.itemTypeMask != 0 && (rule.itemTypeMask & itemTypeMask) == 0) continue;
        if (!RarityMatchesMinimum(rarity, rule.minimumRarity)) continue;
        if (!ModStructContainsAnyHexPattern(item, rule.modPatternHex)) continue;

        match.matched = true;
        match.salvageIndex = rule.salvageIndex;
        match.ruleName = rule.name;
        return match;
    }

    return match;
}

// ===== Sell Items =====

// ShouldSellItemForMaintenance mirrors AutoIt Utils-Maintenance.au3 line 750.
// Sells: identified weapons of white/blue/purple/gold rarity (not rare skins)
//        + materials in the SELL list
// Keeps: kits, unidentified items, green/red items, rare skins
bool ShouldSellItem(const Item* item) {
    if (!item || item->item_id == 0 || item->model_id == 0) return false;
    if (item->equipped || item->customized) return false;

    if (IsForcedVendorSellModel(item->model_id)) {
        return true;
    }

    const uint16_t rarity = GetRarity(const_cast<Item*>(item));
    if (IsLowValueGreenWeaponForVendor(item, rarity)) return true;
    if (rarity == RARITY_GREEN) return false;
    if (IsEventStorageItem(item->model_id)) return false;
    if (IsGeneralXunlaiStorageItem(item->model_id)) return false;

    switch (item->type) {
    case 8:  // runes and mods
    case 9:  // usable
    case 10: // dye
    case 18: // keys
    case 29: // kits
        return false;
    case 11: // materials
        if (IsSellMaterial(item->model_id)) return true;
        return false;
    case 30: // trophies
        return item->value > 0;
    case 34: // salvage item type in older layouts
        return true;
    default:
        break;
    }

    if (IsKit(item->model_id) || IsRareSkin(item->model_id)) return false;
    if ((rarity == RARITY_GOLD || rarity == RARITY_PURPLE) && !IsIdentified(const_cast<Item*>(item))) {
        return false;
    }

    // Preserve low-requirement or perfect-stat collector items.
    const uint16_t req = item->h0026;
    const uint16_t dmg = item->item_formula;
    switch (item->type) {
    case 24: // shield
        if ((req == 9 && dmg == 16 && IsRareSkin(item->model_id)) ||
            (req == 8 && dmg == 16) ||
            (req == 7 && dmg == 15) ||
            (req == 6 && dmg == 14) ||
            (req == 5 && dmg == 13) ||
            (req == 4 && dmg == 12)) {
            return false;
        }
        break;
    case 12: // offhand
        if ((req == 9 && dmg == 12 && IsRareSkin(item->model_id)) ||
            (req == 8 && dmg == 12)) {
            return false;
        }
        break;
    case 27: // sword
        if ((req == 9 && dmg == 22 && IsRareSkin(item->model_id)) ||
            (req == 8 && dmg == 22)) {
            return false;
        }
        break;
    default:
        break;
    }

    return true;
}

uint32_t SellJunkItems() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t merchantItems = MerchantMgr::GetMerchantItemCount();
    if (merchantItems == 0) {
        Log::Warn("MaintenanceMgr: SellJunkItems - merchant not open");
        return 0;
    }

    struct SellCandidate {
        uint32_t itemId;
        uint32_t modelId;
        uint32_t value;
        uint32_t qty;
        uint8_t type;
        uint16_t rarity;
        bool forcedVendorSell;
        bool lowValueGreenWeapon;
    };

    SellCandidate candidates[128];
    uint32_t candidateCount = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4 && candidateCount < _countof(candidates); bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size && candidateCount < _countof(candidates); i++) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;
            if (!ShouldSellItem(item)) continue;
            const bool forcedVendorSell = IsForcedVendorSellModel(item->model_id);
            const uint16_t rarity = GetRarity(item);
            const bool lowValueGreenWeapon = IsLowValueGreenWeaponForVendor(item, rarity);
            if (item->value == 0 && !forcedVendorSell) continue;

            candidates[candidateCount++] = {
                item->item_id,
                item->model_id,
                item->value,
                item->quantity > 0 ? item->quantity : 1u,
                item->type,
                rarity,
                forcedVendorSell,
                lowValueGreenWeapon,
            };
        }
    }

    uint32_t soldCount = 0;
    for (uint32_t i = 0; i < candidateCount; ++i) {
        const SellCandidate& candidate = candidates[i];
        Item* item = ItemMgr::GetItemById(candidate.itemId);
        if (!item || item->model_id != candidate.modelId) continue;
        if (!ShouldSellItem(item)) continue;

        const uint32_t freeBefore = CountFreeSlots();
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        Log::Info("MaintenanceMgr: Selling item=%u model=%u value=%u qty=%u type=%u rarity=%u forced=%u lowValueGreenWeapon=%u",
                  candidate.itemId, candidate.modelId, candidate.value, candidate.qty,
                  candidate.type, candidate.rarity,
                  candidate.forcedVendorSell ? 1u : 0u,
                  candidate.lowValueGreenWeapon ? 1u : 0u);
        if (!MerchantMgr::SellInventoryItem(candidate.itemId, candidate.qty)) {
            Log::Warn("MaintenanceMgr: SellInventoryItem rejected item=%u model=%u",
                      candidate.itemId, candidate.modelId);
            continue;
        }

        bool observed = false;
        const DWORD start = GetTickCount();
        while ((GetTickCount() - start) < 2500u) {
            WaitMs(100);
            Item* current = ItemMgr::GetItemById(candidate.itemId);
            if (!current || current->model_id != candidate.modelId ||
                CountFreeSlots() > freeBefore ||
                ItemMgr::GetGoldCharacter() > goldBefore) {
                observed = true;
                break;
            }
        }

        if (!observed) {
            Log::Warn("MaintenanceMgr: Sale not observed item=%u model=%u freeSlots=%u->%u gold=%u->%u",
                      candidate.itemId, candidate.modelId, freeBefore, CountFreeSlots(),
                      goldBefore, ItemMgr::GetGoldCharacter());
            continue;
        }

        ++soldCount;
    }
    Log::Info("MaintenanceMgr: Sold %u items", soldCount);
    return soldCount;
}

namespace {

bool IsEmergencySellCandidate(const Item* item) {
    if (!item || item->item_id == 0u || item->model_id == 0u) return false;
    if (item->equipped || item->customized) return false;
    return item->value > 0u;
}

Item* FindEmergencyWhiteWeaponToSell(Inventory* inv) {
    if (!inv) return nullptr;
    for (uint32_t bagIdx = 1u; bagIdx <= 4u; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!IsEmergencySellCandidate(item)) continue;
            if (!IsWeapon(item)) continue;
            if (GetRarity(item) != RARITY_WHITE) continue;
            if (IsRareSkin(item->model_id)) continue;
            return item;
        }
    }
    return nullptr;
}

Item* FindEmergencyLowGradeSalvageKitToSell(Inventory* inv) {
    if (!inv) return nullptr;
    static constexpr uint32_t kEmergencyKitModels[] = {
        ItemModelIds::SALVAGE_KIT,
        ItemModelIds::RARE_SALVAGE_KIT,
        ItemModelIds::ALT_SALVAGE_KIT,
    };
    for (uint32_t modelId : kEmergencyKitModels) {
        for (uint32_t bagIdx = 1u; bagIdx <= 4u; ++bagIdx) {
            Bag* bag = inv->bags[bagIdx];
            if (!bag || !bag->items.buffer) continue;
            for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
                Item* item = bag->items.buffer[slot];
                if (!IsEmergencySellCandidate(item)) continue;
                if (item->model_id != modelId) continue;
                return item;
            }
        }
    }
    return nullptr;
}

Item* FindEmergencyForcedVendorSellItem(Inventory* inv) {
    if (!inv) return nullptr;
    for (uint32_t bagIdx = 1u; bagIdx <= 4u; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0u; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!IsEmergencySellCandidate(item)) continue;
            if (!IsForcedVendorSellModel(item->model_id)) continue;
            return item;
        }
    }
    return nullptr;
}

} // namespace

uint32_t SellEmergencyItemsForFreeSlots(uint32_t minFreeSlots) {
    if (minFreeSlots == 0u) return 0u;
    if (MerchantMgr::GetMerchantItemCount() == 0u) {
        Log::Warn("MaintenanceMgr: SellEmergencyItemsForFreeSlots - merchant not open");
        return 0u;
    }

    uint32_t sold = 0u;
    uint32_t freeSlots = CountFreeSlots();
    while (freeSlots < minFreeSlots) {
        Inventory* inv = ItemMgr::GetInventory();
        Item* item = FindEmergencyForcedVendorSellItem(inv);
        const char* reason = "forced vendor-sell item";
        if (!item) {
            item = FindEmergencyWhiteWeaponToSell(inv);
            reason = "white weapon";
        }
        if (!item) {
            item = FindEmergencyLowGradeSalvageKitToSell(inv);
            reason = "low-grade salvage kit";
        }
        if (!item) {
            Log::Warn("MaintenanceMgr: emergency merchant cleanup stopped at freeSlots=%u/%u; no safe item found",
                      freeSlots,
                      minFreeSlots);
            break;
        }

        const uint32_t itemId = item->item_id;
        const uint32_t modelId = item->model_id;
        const uint32_t quantity = item->quantity > 0u ? item->quantity : 1u;
        const uint8_t type = item->type;
        const uint16_t rarity = GetRarity(item);
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        Log::Warn("MaintenanceMgr: selling emergency %s for critical free slots item=%u model=%u qty=%u type=%u rarity=%u",
                  reason,
                  itemId,
                  modelId,
                  quantity,
                  type,
                  rarity);
        if (!MerchantMgr::SellInventoryItem(itemId, quantity)) {
            Log::Warn("MaintenanceMgr: emergency merchant sale rejected item=%u model=%u",
                      itemId,
                      modelId);
            break;
        }

        uint32_t nextFreeSlots = freeSlots;
        bool freedSlot = false;
        bool goldChanged = false;
        const DWORD start = GetTickCount();
        while ((GetTickCount() - start) < 2500u) {
            WaitMs(100);
            nextFreeSlots = CountFreeSlots();
            Item* current = ItemMgr::GetItemById(itemId);
            freedSlot = nextFreeSlots > freeSlots || !current || current->model_id != modelId;
            goldChanged = ItemMgr::GetGoldCharacter() > goldBefore;
            if (freedSlot) {
                break;
            }
        }
        if (nextFreeSlots <= freeSlots) {
            Log::Warn("MaintenanceMgr: emergency merchant sale did not increase free slots (%u -> %u, goldChanged=%u)",
                      freeSlots,
                      nextFreeSlots,
                      goldChanged ? 1u : 0u);
            break;
        }
        ++sold;
        freeSlots = nextFreeSlots;
    }

    if (sold > 0u) {
        Log::Warn("MaintenanceMgr: emergency merchant cleanup sold %u item(s); freeSlots=%u/%u",
                  sold,
                  CountFreeSlots(),
                  minFreeSlots);
    }
    return sold;
}

// ===== Item Identification () =====
// Mirrors AutoIt IdentifyUnidentifiedItemsForMaintenance():
// scan bags 1-4, skip rare skins, identify with ID kit.

static Item* FindIdKit() {
    Inventory* inv = ItemMgr::GetInventory();
    if (inv) {
        for (uint32_t b = 1; b <= 4; b++) {
            Bag* bag = inv->bags[b];
            if (!bag || !bag->items.buffer) continue;
            for (uint32_t s = 0; s < bag->items.size; s++) {
                Item* item = bag->items.buffer[s];
                if (!item) continue;
                if (item->model_id == ItemModelIds::SUPERIOR_IDENTIFICATION_KIT || item->model_id == ItemModelIds::IDENTIFICATION_KIT ||
                    item->model_id == ItemModelIds::ALT_IDENTIFICATION_KIT) return item;
            }
        }
    }
    Item* item = ItemMgr::FindItemByModelId(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(ItemModelIds::IDENTIFICATION_KIT);
    if (item) return item;
    return ItemMgr::FindItemByModelId(ItemModelIds::ALT_IDENTIFICATION_KIT);
}

static Item* FindSalvageKit() {
    // Match the GWA2 default salvage flow: prefer basic kits first.
    Item* item = ItemMgr::FindItemByModelId(ItemModelIds::SALVAGE_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(ItemModelIds::RARE_SALVAGE_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(ItemModelIds::ALT_SALVAGE_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(ItemModelIds::EXPERT_SALVAGE_KIT);
    if (item) return item;
    return ItemMgr::FindItemByModelId(ItemModelIds::SUPERIOR_SALVAGE_KIT);
}

static Item* FindExpertSalvageKit() {
    Item* item = ItemMgr::FindItemByModelId(ItemModelIds::SUPERIOR_SALVAGE_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(ItemModelIds::EXPERT_SALVAGE_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(ItemModelIds::RARE_SALVAGE_KIT);
    if (item) return item;
    return ItemMgr::FindItemByModelId(ItemModelIds::SALVAGE_KIT);
}

uint32_t IdentifyAllItems() {
    Item* kit = FindIdKit();
    if (!kit) {
        Log::Warn("MaintenanceMgr: No ID kit found in inventory");
        return 0;
    }

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t identified = 0;
    for (uint32_t b = 1; b <= 4; b++) {
        Bag* bag = inv->bags[b];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t s = 0; s < bag->items.size; s++) {
            Item* item = bag->items.buffer[s];
            if (!item || item->model_id == 0) continue;
            if (IsIdentified(item)) continue;
            if (IsKit(item->model_id)) continue;
            // Skip rare skins; do not identify, preserving value.
            if (IsRareSkin(item->model_id)) continue;

            Log::Info("MaintenanceMgr: Identifying item=%u model=%u type=%u with kit=%u",
                      item->item_id, item->model_id, item->type, kit->item_id);
            ItemMgr::IdentifyItem(item->item_id, kit->item_id);
            WaitMs(1000);
            identified++;

            // Re-find kit (it may have been consumed)
            kit = FindIdKit();
            if (!kit) {
                Log::Warn("MaintenanceMgr: ID kit exhausted after %u identifications", identified);
                return identified;
            }
        }
    }
    Log::Info("MaintenanceMgr: Identified %u items", identified);
    return identified;
}

// ===== Salvage () =====
// Uses the native Salvage function directly, matching AutoIt's CommandSalvage shellcode.
// AutoIt: writes item_id + kit_id to SalvageGlobal, calls Salvage(session_id, kit_id, item_id).
// This avoids the kPreStartSalvage UI message which corrupts game state.

// Read the salvage session ID from the game's pointer chain.
// AutoIt: MemoryReadPtr(base_address_ptr, [0, 0x18, 0x2C, 0x690])
static uint32_t GetSalvageSessionId() {
    if (!Offsets::BasePointer) return 0;
    __try {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (!ctx) return 0;
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (!p1) return 0;
        uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
        if (!p2) return 0;
        return *reinterpret_cast<uint32_t*>(p2 + 0x690);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static void DumpCodeBytes(const char* label, uintptr_t addr, size_t count = 32u) {
    if (!addr || count == 0u) {
        Log::Info("MaintenanceMgr: %s addr=0x%08X (skip)", label ? label : "code", static_cast<unsigned>(addr));
        return;
    }
    char buffer[256]{};
    size_t cursor = 0;
    __try {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
        for (size_t i = 0; i < count && cursor + 4 < sizeof(buffer); ++i) {
            int wrote = _snprintf_s(buffer + cursor, sizeof(buffer) - cursor, _TRUNCATE, "%02X ", p[i]);
            if (wrote <= 0) break;
            cursor += static_cast<size_t>(wrote);
        }
        Log::Info("MaintenanceMgr: %s addr=0x%08X bytes=%s",
                  label ? label : "code",
                  static_cast<unsigned>(addr),
                  buffer);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("MaintenanceMgr: %s addr=0x%08X unreadable", label ? label : "code", static_cast<unsigned>(addr));
    }
}

static void DumpSalvageOffsetDiagnostics() {
    static bool s_dumped = false;
    if (s_dumped) return;
    s_dumped = true;

    constexpr const char* kSalvageBytes66 =
        "\x33\xC5\x89\x45\xFC\x8B\x45\x08\x89\x45\xF0\x8B\x45\x0C\x89\x45\xF4\x8B\x45\x10\x89\x45\xF8\x8D\x45\xEC\x50\x6A\x10\xC7\x45\xEC\x66";
    constexpr const char* kSalvageBytes77 =
        "\x33\xC5\x89\x45\xFC\x8B\x45\x08\x89\x45\xF0\x8B\x45\x0C\x89\x45\xF4\x8B\x45\x10\x89\x45\xF8\x8D\x45\xEC\x50\x6A\x10\xC7\x45\xEC\x77";
    constexpr const char* kSalvageMask =
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    constexpr const char* kGlobalBytes =
        "\x8B\x4A\x04\x53\x89\x45\xF4\x8B\x42\x08";
    constexpr const char* kGlobalMask =
        "xxxxxxxxxx";

    const uintptr_t curFn = Offsets::Salvage;
    const uintptr_t curGlobal = Offsets::SalvageGlobal;
    const uintptr_t openFn = Offsets::SalvageSessionOpen;
    const uintptr_t materialsFn = Offsets::SalvageMaterials;
    const uintptr_t fn66b = Scanner::Find(kSalvageBytes66, kSalvageMask, -0xB);
    const uintptr_t fn66a = Scanner::Find(kSalvageBytes66, kSalvageMask, -0xA);
    const uintptr_t fn77b = Scanner::Find(kSalvageBytes77, kSalvageMask, -0xB);
    const uintptr_t fn77a = Scanner::Find(kSalvageBytes77, kSalvageMask, -0xA);
    const uintptr_t rawGlobal0 = Scanner::Find(kGlobalBytes, kGlobalMask, 0x0);
    const uintptr_t rawGlobal1 = Scanner::Find(kGlobalBytes, kGlobalMask, 0x1);
    const uintptr_t fnStart = Scanner::ToFunctionStart(curFn, 0x40);

    uint32_t globalFromRaw0 = 0u;
    uint32_t globalFromRaw1 = 0u;
    __try {
        if (rawGlobal0) globalFromRaw0 = *reinterpret_cast<const uint32_t*>(rawGlobal0 - 0x4);
        if (rawGlobal1) globalFromRaw1 = *reinterpret_cast<const uint32_t*>(rawGlobal1 - 0x4);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("MaintenanceMgr: Salvage diag failed to deref global candidates");
    }

    Log::Info("MaintenanceMgr: Salvage diag currentFn=0x%08X fnStart=0x%08X openFn=0x%08X materialsFn=0x%08X currentGlobal=0x%08X",
              static_cast<unsigned>(curFn),
              static_cast<unsigned>(fnStart),
              static_cast<unsigned>(openFn),
              static_cast<unsigned>(materialsFn),
              static_cast<unsigned>(curGlobal));
    Log::Info("MaintenanceMgr: Salvage diag scan66[-0xB]=0x%08X scan66[-0xA]=0x%08X scan77[-0xB]=0x%08X scan77[-0xA]=0x%08X",
              static_cast<unsigned>(fn66b),
              static_cast<unsigned>(fn66a),
              static_cast<unsigned>(fn77b),
              static_cast<unsigned>(fn77a));
    Log::Info("MaintenanceMgr: Salvage diag global raw[0]=0x%08X raw[1]=0x%08X deref(raw0-4)=0x%08X deref(raw1-4)=0x%08X",
              static_cast<unsigned>(rawGlobal0),
              static_cast<unsigned>(rawGlobal1),
              static_cast<unsigned>(globalFromRaw0),
              static_cast<unsigned>(globalFromRaw1));

    DumpCodeBytes("Salvage current", curFn, 64u);
    if (fnStart && fnStart != curFn) DumpCodeBytes("Salvage fnStart", fnStart, 32u);
    if (fn66a && fn66a != curFn) DumpCodeBytes("Salvage alt66[-0xA]", fn66a, 32u);
    if (fn77a && fn77a != curFn) DumpCodeBytes("Salvage alt77[-0xA]", fn77a, 32u);
    DumpCodeBytes("Salvage session open", openFn, 48u);
    DumpCodeBytes("Salvage materials", materialsFn, 32u);
    DumpCodeBytes("SalvageGlobal current", curGlobal, 16u);
}

static bool CallNativeThreeArgFunction(uintptr_t fn, uint32_t arg0, uint32_t arg1, uint32_t arg2, const char* label) {
    if (!fn) {
        Log::Warn("MaintenanceMgr: Native call skipped for %s -- fn=0x00000000",
                  label ? label : "native-call");
        return false;
    }

    uint32_t a0 = arg0;
    uint32_t a1 = arg1;
    uint32_t a2 = arg2;
    __try {
        __asm {
            push a2
            push a1
            push a0
            call fn
            add esp, 0xC
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Error("MaintenanceMgr: Native call faulted for %s fn=0x%08X code=0x%08X",
                   label ? label : "native-call",
                   static_cast<unsigned>(fn),
                   static_cast<unsigned>(GetExceptionCode()));
        return false;
    }
    return true;
}

// AutoIt/GWA2's salvage entry pattern resolves the native 0x77 session-open
// wrapper, not the Reforged-local 0x66 helper. Keep the local helper only as a
// fallback; the primary path should call the native 0x77 wrapper directly after
// priming SalvageGlobal with the item/kit pair.
static void ExecuteSalvageStartCommand(uint32_t itemId, uint32_t kitId, uint32_t sessionId) {
    if ((!Offsets::SalvageSessionOpen && !Offsets::Salvage) || !Offsets::SalvageGlobal) return;

    Log::Info("MaintenanceMgr: ExecuteSalvageStartCommand start session=%u kit=%u item=%u localFn=0x%08X openFn=0x%08X salvageGlobal=0x%08X",
              sessionId, kitId, itemId, Offsets::Salvage, Offsets::SalvageSessionOpen, Offsets::SalvageGlobal);

    uint32_t* global = reinterpret_cast<uint32_t*>(Offsets::SalvageGlobal);
    global[0] = itemId;
    global[1] = kitId;

    if (Offsets::SalvageSessionOpen) {
        CallNativeThreeArgFunction(Offsets::SalvageSessionOpen, sessionId, kitId, itemId, "salvage-session-open");
    } else if (Offsets::Salvage) {
        CallNativeThreeArgFunction(Offsets::Salvage, sessionId, kitId, itemId, "salvage-local-init-fallback");
    }

    Log::Info("MaintenanceMgr: ExecuteSalvageStartCommand done session=%u kit=%u item=%u", sessionId, kitId, itemId);
}

struct NativeSalvageTask {
    uint32_t item_id;
    uint32_t kit_id;
    uint32_t session_id;
};

static void NativeSalvageInvoker(void* params) {
    auto* task = reinterpret_cast<NativeSalvageTask*>(params);
    if (!task) return;
    Log::Info("MaintenanceMgr: NativeSalvageInvoker item=%u kit=%u session=%u", task->item_id, task->kit_id, task->session_id);
    ExecuteSalvageStartCommand(task->item_id, task->kit_id, task->session_id);
}

static void DumpInventoryRootWindow(const char* label) {
    __try {
        uintptr_t bp = Offsets::BasePointer;
        uintptr_t ctx = bp ? *reinterpret_cast<uintptr_t*>(bp) : 0;
        uintptr_t p1 = ctx ? *reinterpret_cast<uintptr_t*>(ctx + 0x18) : 0;
        uintptr_t p2 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x40) : 0;
        uintptr_t p2Alt = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x44) : 0;
        if (!p2) {
            Log::Info("MaintenanceMgr: %s p2=0x00000000", label ? label : "inventory-root");
            return;
        }

        const uintptr_t vB0 = *reinterpret_cast<uintptr_t*>(p2 + 0xB0);
        const uintptr_t vB4 = *reinterpret_cast<uintptr_t*>(p2 + 0xB4);
        const uintptr_t vB8 = *reinterpret_cast<uintptr_t*>(p2 + 0xB8);
        const uintptr_t vBC = *reinterpret_cast<uintptr_t*>(p2 + 0xBC);
        const uint32_t  vC0 = *reinterpret_cast<uint32_t*>(p2 + 0xC0);
        const uintptr_t vC4 = *reinterpret_cast<uintptr_t*>(p2 + 0xC4);
        const uintptr_t vF0 = *reinterpret_cast<uintptr_t*>(p2 + 0xF0);
        const uintptr_t vF4 = *reinterpret_cast<uintptr_t*>(p2 + 0xF4);
        const uintptr_t vF8 = *reinterpret_cast<uintptr_t*>(p2 + 0xF8);
        const uintptr_t vFC = *reinterpret_cast<uintptr_t*>(p2 + 0xFC);

        const uintptr_t altB8 = p2Alt ? *reinterpret_cast<uintptr_t*>(p2Alt + 0xB8) : 0;
        const uint32_t  altC0 = p2Alt ? *reinterpret_cast<uint32_t*>(p2Alt + 0xC0) : 0;
        const uintptr_t altF8 = p2Alt ? *reinterpret_cast<uintptr_t*>(p2Alt + 0xF8) : 0;

        const uintptr_t p1_30 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x30) : 0;
        const uintptr_t p1_34 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x34) : 0;
        const uintptr_t p1_38 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x38) : 0;
        const uintptr_t p1_3C = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x3C) : 0;
        const uintptr_t p1_40 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x40) : 0;
        const uintptr_t p1_44 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x44) : 0;
        const uintptr_t p1_48 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x48) : 0;
        const uintptr_t p1_4C = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x4C) : 0;
        const uintptr_t p1_50 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x50) : 0;
        const uintptr_t p1_54 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x54) : 0;

        Log::Info("MaintenanceMgr: %s p2=0x%08X [B0]=0x%08X [B4]=0x%08X [B8]=0x%08X [BC]=0x%08X [C0]=%u [C4]=0x%08X [F0]=0x%08X [F4]=0x%08X [F8]=0x%08X [FC]=0x%08X",
                  label ? label : "inventory-root",
                  p2, vB0, vB4, vB8, vBC, vC0, vC4, vF0, vF4, vF8, vFC);
        Log::Info("MaintenanceMgr: %s chain bp=0x%08X ctx=0x%08X p1=0x%08X [30]=0x%08X [34]=0x%08X [38]=0x%08X [3C]=0x%08X [40]=0x%08X [44]=0x%08X [48]=0x%08X [4C]=0x%08X [50]=0x%08X [54]=0x%08X",
                  label ? label : "inventory-root",
                  bp, ctx, p1,
                  p1_30, p1_34, p1_38, p1_3C, p1_40, p1_44, p1_48, p1_4C, p1_50, p1_54);
        Log::Info("MaintenanceMgr: %s sibling p2Alt=0x%08X [B8]=0x%08X [C0]=%u [F8]=0x%08X",
                  label ? label : "inventory-root",
                  p2Alt, altB8, altC0, altF8);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("MaintenanceMgr: %s root-window read failed", label ? label : "inventory-root");
    }
}

static void DumpTrackedItemState(const char* label, Item* item) {
    if (!item) {
        Log::Info("MaintenanceMgr: %s tracked item=<null>", label ? label : "tracked-item");
        return;
    }

    __try {
        Log::Info("MaintenanceMgr: %s tracked item=0x%08X id=%u model=%u bag=0x%08X qty=%u salvageable=%u",
                  label ? label : "tracked-item",
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(item)),
                  item->item_id,
                  item->model_id,
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(item->bag)),
                  item->quantity,
                  item->is_material_salvageable);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("MaintenanceMgr: %s tracked item read faulted ptr=0x%08X",
                  label ? label : "tracked-item",
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(item)));
    }
}

static bool TrackedItemStillRepresentsOriginal(Item* item, uint32_t expectedItemId) {
    if (!item) return false;
    __try {
        // Once salvage completes in our client, the original slot/item pointer may
        // remain readable but no longer describes the original item: item_id
        // mutates, the bag pointer is cleared, or the model disappears. Treat all
        // of those as "consumed" rather than waiting only for a hard zero/fault.
        if (item->item_id == 0u) return false;
        if (item->item_id != expectedItemId) return false;
        if (item->bag == nullptr) return false;
        if (item->model_id == 0u) return false;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Wait for the bags array pointer to become non-null after salvage.
// The Salvage function temporarily zeroes p2+0xF8 during processing.
static bool WaitForBagsPointerRestore(uint32_t timeoutMs = 10000) {
    auto readRoots = [](uintptr_t& p2, uintptr_t& bags, uintptr_t& itemsBase, uint32_t& itemsSize) -> bool {
        p2 = bags = itemsBase = 0;
        itemsSize = 0;
        __try {
            uintptr_t bp = Offsets::BasePointer;
            uintptr_t ctx = bp ? *reinterpret_cast<uintptr_t*>(bp) : 0;
            uintptr_t p1 = ctx ? *reinterpret_cast<uintptr_t*>(ctx + 0x18) : 0;
            p2 = p1 ? *reinterpret_cast<uintptr_t*>(p1 + 0x40) : 0;
            bags = p2 ? *reinterpret_cast<uintptr_t*>(p2 + 0xF8) : 0;
            itemsBase = p2 ? *reinterpret_cast<uintptr_t*>(p2 + 0xB8) : 0;
            itemsSize = p2 ? *reinterpret_cast<uint32_t*>(p2 + 0xC0) : 0;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    };

    uintptr_t p2 = 0, bags = 0, itemsBase = 0;
    uint32_t itemsSize = 0;
    if (!readRoots(p2, bags, itemsBase, itemsSize)) {
        Log::Error("MaintenanceMgr: Exception reading pointer chain after salvage");
        return false;
    }

    Log::Info("MaintenanceMgr: POST-SALVAGE: p2=0x%08X bags=0x%08X items=0x%08X size=%u",
              p2, bags, itemsBase, itemsSize);

    const uint32_t polls = timeoutMs / 100u;
    for (uint32_t wait = 0; wait < polls; wait++) {
        if (readRoots(p2, bags, itemsBase, itemsSize) && bags != 0) {
            Log::Info("MaintenanceMgr: Bags pointer restored after %d ms (p2=0x%08X bags=0x%08X items=0x%08X size=%u)",
                      wait * 100, p2, bags, itemsBase, itemsSize);
            return true;
        }
        if ((wait % 5u) == 4u) {
            Offsets::RefreshBasePointer();
        }
        Sleep(100);
    }

    if (!readRoots(p2, bags, itemsBase, itemsSize)) {
        Log::Error("MaintenanceMgr: Exception reading pointer chain after salvage");
        return false;
    }
    Log::Warn("MaintenanceMgr: Bags pointer still NULL after %u ms (p2=0x%08X items=0x%08X size=%u)",
              timeoutMs, p2, itemsBase, itemsSize);
    DumpInventoryRootWindow("POST-SALVAGE-TAIL");
    return false;
}

static bool SendLegacySalvagePacketThreaded(uint32_t header, const char* label) {
    Log::Info("MaintenanceMgr: Legacy salvage post-dispatch packet %s hdr=0x%X",
              label ? label : "packet",
              header);
    if (GameThread::IsOnGameThread()) {
        CtoS::SendPacketDirect(1, header);
        Log::Info("MaintenanceMgr: Legacy salvage post-dispatch packet executed inline hdr=0x%X", header);
        return true;
    }
    if (!GameThread::IsInitialized()) {
        Log::Warn("MaintenanceMgr: Legacy salvage post-dispatch packet dropped hdr=0x%X -- GameThread not ready", header);
        return false;
    }

    HANDLE done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    GameThread::EnqueuePost([header, done]() {
        CtoS::SendPacketDirectRaw(1, header);
        if (done) {
            SetEvent(done);
        }
    });

    DWORD wait = WAIT_FAILED;
    if (done) {
        wait = WaitForSingleObject(done, 3000);
        if (wait == WAIT_OBJECT_0) {
            CloseHandle(done);
            done = nullptr;
        }
    }
    const bool executed = wait == WAIT_OBJECT_0;
    Log::Info("MaintenanceMgr: Legacy salvage post-dispatch packet executed=%u hdr=0x%X wait=0x%X",
              executed ? 1u : 0u,
              header,
              static_cast<unsigned>(wait));
    return executed;
}

static bool WaitForBotshubQueueIdle(const char* label, uint32_t timeoutMs = 2000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (CtoS::IsBotshubQueueIdle()) {
            Log::Info("MaintenanceMgr: Botshub queue idle after %u ms (%s)",
                      static_cast<unsigned>(GetTickCount() - start),
                      label ? label : "queue");
            return true;
        }
        Sleep(10);
    }
    Log::Warn("MaintenanceMgr: Botshub queue still busy after %u ms (%s)",
              timeoutMs,
              label ? label : "queue");
    CtoS::DumpBotshubQueueState(label ? label : "queue");
    return false;
}

static bool WaitForGameCommandQueueIdle(const char* label, uint32_t timeoutMs = 2000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (CtoS::IsGameCommandQueueIdle()) {
            Log::Info("MaintenanceMgr: Game command queue idle after %u ms (%s)",
                      static_cast<unsigned>(GetTickCount() - start),
                      label ? label : "queue");
            return true;
        }
        Sleep(10);
    }
    Log::Warn("MaintenanceMgr: Game command queue still busy after %u ms (%s)",
              timeoutMs,
              label ? label : "queue");
    return false;
}

static void DumpSalvageUiState(const char* label) {
    const uintptr_t root = UIMgr::GetRootFrame();
    Log::Info("MaintenanceMgr: %s root=0x%08X frameId=%u childCount=%u",
              label ? label : "salvage-ui",
              static_cast<unsigned>(root),
              root > 0x10000 ? UIMgr::GetFrameId(root) : 0u,
              root > 0x10000 ? UIMgr::GetChildFrameCount(root) : 0u);
    if (root > 0x10000) {
        UIMgr::DebugDumpChildFrames(root, label ? label : "salvage-ui", 24);
    }
}

static void DumpSalvageUiStateOnGameThread() {
    if (!GameThread::IsInitialized() || GameThread::IsOnGameThread()) {
        DumpSalvageUiState("salvage-ui-game-thread");
        return;
    }

    HANDLE done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!done) {
        DumpSalvageUiState("salvage-ui-fallback");
        return;
    }

    GameThread::EnqueuePost([done]() {
        DumpSalvageUiState("salvage-ui-game-thread");
        SetEvent(done);
    });

    WaitForSingleObject(done, 1500);
    CloseHandle(done);
}

static bool SendWindowEnterKey() {
    HWND hwnd = static_cast<HWND>(MemoryMgr::GetGWWindowHandle());
    if (!hwnd || !IsWindow(hwnd)) {
        Log::Warn("MaintenanceMgr: SendWindowEnterKey missing GW hwnd");
        return false;
    }

    constexpr LPARAM kEnterDownLParam = 0x001C0001;
    constexpr LPARAM kEnterUpLParam = 0xC01C0001;
    const BOOL downOk = PostMessageA(hwnd, WM_KEYDOWN, VK_RETURN, kEnterDownLParam);
    Sleep(30);
    const BOOL charOk = PostMessageA(hwnd, WM_CHAR, '\r', kEnterDownLParam);
    Sleep(30);
    const BOOL upOk = PostMessageA(hwnd, WM_KEYUP, VK_RETURN, kEnterUpLParam);
    Log::Info("MaintenanceMgr: SendWindowEnterKey hwnd=0x%08X down=%u char=%u up=%u",
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(hwnd)),
              downOk ? 1u : 0u,
              charOk ? 1u : 0u,
              upOk ? 1u : 0u);
    return downOk && upOk;
}

static bool SendRootFrameEnterKey() {
    const uintptr_t root = UIMgr::GetRootFrame();
    if (root < 0x10000) {
        Log::Warn("MaintenanceMgr: SendRootFrameEnterKey missing root frame");
        return false;
    }
    const bool sent = UIMgr::KeyPress(root, VK_RETURN);
    Log::Info("MaintenanceMgr: SendRootFrameEnterKey frame=0x%08X frameId=%u sent=%u",
              static_cast<unsigned>(root),
              UIMgr::GetFrameId(root),
              sent ? 1u : 0u);
    return sent;
}

static bool SendRootFrameEnterKeyOnGameThread() {
    if (!GameThread::IsInitialized() || GameThread::IsOnGameThread()) {
        return SendRootFrameEnterKey();
    }

    HANDLE done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!done) {
        return SendRootFrameEnterKey();
    }

    bool sent = false;
    GameThread::EnqueuePost([&sent, done]() {
        sent = SendRootFrameEnterKey();
        SetEvent(done);
    });

    const DWORD wait = WaitForSingleObject(done, 1500);
    CloseHandle(done);
    if (wait != WAIT_OBJECT_0) {
        Log::Warn("MaintenanceMgr: SendRootFrameEnterKeyOnGameThread timed out");
    }
    return sent;
}

struct LegacyBotshubSalvageConfig {
    uint32_t forcedSessionId = 0u;
    uint32_t followupHeader = 0u;
    bool followupAfterConsume = false;
    bool sendMaterials = true;
    bool applyAutoItPreStartDelay = true;
    bool sendMaterialsViaBotshub = false;
    bool waitForStartQueueIdleBeforeMaterials = true;
    bool waitForMaterialsQueueIdle = true;
    bool followupViaBotshub = false;
    bool useHookedBotshubPackets = false;
    bool sendEnterAfterMaterials = false;
    bool sendEnterAfterConsume = false;
    bool allowZeroSession = false;
    bool allowInventoryFallback = true;
    bool waitForInventoryRestoreAfterConsume = true;
    uint32_t restoreAfterConsumeTimeoutMs = 15000u;
    uint32_t stabilizeRestoreTimeoutMs = 1000u;
    bool applyAutoItPostConsumeDwell = false;
};

struct LegacyBotshubSalvageResult {
    bool queued = false;
    bool consumed = false;
    bool inventoryRestored = false;
};

static LegacyBotshubSalvageResult SalvageItemLegacyBotshubImpl(uint32_t kitId, uint32_t itemId, Item* trackedItem, const LegacyBotshubSalvageConfig& cfg) {
    LegacyBotshubSalvageResult result{};
    if (!kitId || !itemId) {
        Log::Warn("MaintenanceMgr: SalvageItemLegacyBotshubImpl rejected kit=%u item=%u", kitId, itemId);
        return result;
    }
    DumpSalvageOffsetDiagnostics();

    Item* itemBefore = trackedItem ? trackedItem : ItemMgr::GetItemById(itemId);
    DumpTrackedItemState("PRE-LEGACY-BOTSHUB-SALVAGE-ITEM", itemBefore);

    const uint32_t sessionId = cfg.forcedSessionId ? cfg.forcedSessionId : GetSalvageSessionId();
    if (!sessionId && !cfg.allowZeroSession) {
        Log::Warn("MaintenanceMgr: SalvageItemLegacyBotshubImpl missing session id for kit=%u item=%u", kitId, itemId);
        return result;
    }
    if (!sessionId && cfg.allowZeroSession) {
        Log::Warn("MaintenanceMgr: Legacy botshub salvage proceeding with zero session id for kit=%u item=%u", kitId, itemId);
    }

    const uint32_t ping = ChatMgr::GetPing();
    DumpInventoryRootWindow("PRE-LEGACY-BOTSHUB-SALVAGE");
    Log::Info("MaintenanceMgr: Legacy botshub salvage queue item=%u kit=%u session=%u forcedSession=%u allowZeroSession=%u tracked=0x%08X fallback=%u restoreAfterConsume=%u",
              itemId,
              kitId,
              sessionId,
              cfg.forcedSessionId ? 1u : 0u,
              cfg.allowZeroSession ? 1u : 0u,
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(itemBefore)),
              cfg.allowInventoryFallback ? 1u : 0u,
              cfg.waitForInventoryRestoreAfterConsume ? 1u : 0u);
    if (cfg.applyAutoItPreStartDelay) {
        const uint32_t startDelayMs = 40u + ping;
        Log::Info("MaintenanceMgr: Legacy botshub applying AutoIt pre-start delay=%u item=%u", startDelayMs, itemId);
        WaitMs(startDelayMs);
    }
    result.queued = CtoS::SalvageItemBotshub(itemId, kitId, sessionId);
    if (!result.queued) {
        Log::Warn("MaintenanceMgr: Legacy botshub salvage queue failed item=%u kit=%u session=%u", itemId, kitId, sessionId);
        return result;
    }
    CtoS::DumpBotshubQueueState("legacy-botshub-after-start-enqueue");

    if (cfg.waitForStartQueueIdleBeforeMaterials) {
        WaitForBotshubQueueIdle("legacy-botshub-salvage-start");
    }
    WaitMs(ping);

    if (cfg.sendMaterials) {
        // Per AutoIt GWA2_Headers.au3:
        //   0x79 = SALVAGE_SESSION_DONE (close session without salvaging)
        //   0x7A = SALVAGE_MATERIALS    (actually extract materials)
        // We were sending 0x79 for every item, which closed each session
        // without salvaging; the server eventually disconnected us.
        constexpr uint32_t kLegacySalvageMaterials = 0x7Au;
        if (cfg.sendMaterialsViaBotshub) {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending SALVAGE_MATERIALS item=%u kit=%u hdr=0x%X via %s botshub queue",
                      itemId, kitId, kLegacySalvageMaterials, cfg.useHookedBotshubPackets ? "hooked" : "raw");
            const bool materialsQueued = cfg.useHookedBotshubPackets
                ? CtoS::SendPacketBotshubHooked(1, kLegacySalvageMaterials)
                : CtoS::SendPacketBotshub(1, kLegacySalvageMaterials);
            Log::Info("MaintenanceMgr: Legacy botshub salvage SALVAGE_MATERIALS queued=%u", materialsQueued ? 1u : 0u);
            CtoS::DumpBotshubQueueState("legacy-botshub-after-materials-enqueue");
            if (cfg.waitForMaterialsQueueIdle) {
                WaitForBotshubQueueIdle("legacy-botshub-salvage-materials");
            }
            WaitMs(250u);
            CtoS::DumpBotshubQueueState("legacy-botshub-250ms-after-materials");
        } else {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending SALVAGE_MATERIALS item=%u kit=%u hdr=0x%X via post-dispatch packet transport",
                      itemId, kitId, kLegacySalvageMaterials);
            SendLegacySalvagePacketThreaded(kLegacySalvageMaterials, "SALVAGE_MATERIALS");
        }
    } else {
        Log::Info("MaintenanceMgr: Legacy botshub salvage start-only item=%u kit=%u session=%u (no SALVAGE_MATERIALS)",
                  itemId, kitId, sessionId);
        WaitMs(ping);
    }
    if (cfg.followupHeader != 0u && !cfg.followupAfterConsume) {
        if (cfg.followupViaBotshub) {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending followup hdr=0x%X item=%u kit=%u via %s botshub queue",
                      cfg.followupHeader, itemId, kitId, cfg.useHookedBotshubPackets ? "hooked" : "raw");
            const bool followupQueued = cfg.useHookedBotshubPackets
                ? CtoS::SendPacketBotshubHooked(1, cfg.followupHeader)
                : CtoS::SendPacketBotshub(1, cfg.followupHeader);
            Log::Info("MaintenanceMgr: Legacy botshub salvage followup queued=%u", followupQueued ? 1u : 0u);
            WaitForBotshubQueueIdle("legacy-botshub-salvage-followup");
            WaitMs(40 + ping);
        } else {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending followup hdr=0x%X item=%u kit=%u via post-dispatch packet transport",
                      cfg.followupHeader, itemId, kitId);
            SendLegacySalvagePacketThreaded(cfg.followupHeader, "FOLLOWUP");
            WaitMs(40 + ping);
        }
    }
    if (cfg.sendEnterAfterMaterials) {
        Log::Info("MaintenanceMgr: Legacy botshub salvage sending Enter after materials item=%u", itemId);
        DumpSalvageUiStateOnGameThread();
        bool enterSent = SendRootFrameEnterKeyOnGameThread();
        if (!enterSent) {
            enterSent = SendWindowEnterKey();
        }
        Log::Info("MaintenanceMgr: Legacy botshub salvage post-materials Enter sent=%u item=%u",
                  enterSent ? 1u : 0u,
                  itemId);
        WaitMs(250u + ping);
        DumpSalvageUiStateOnGameThread();
    }
    DumpTrackedItemState("POST-LEGACY-BOTSHUB-SALVAGE-MATERIALS-ITEM", itemBefore);

    bool consumedByTrackedPtr = false;
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < (500u + ping)) {
        if (!TrackedItemStillRepresentsOriginal(itemBefore, itemId)) {
            consumedByTrackedPtr = true;
            break;
        }
        WaitMs(10);
    }
    Log::Info("MaintenanceMgr: Legacy botshub tracked-pointer consume wait item=%u consumed=%u elapsed=%u",
              itemId,
              consumedByTrackedPtr ? 1u : 0u,
              static_cast<unsigned>(GetTickCount() - start));

    if (cfg.followupHeader != 0u && cfg.followupAfterConsume) {
        if (cfg.followupViaBotshub) {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending post-consume followup hdr=0x%X item=%u kit=%u via %s botshub queue",
                      cfg.followupHeader, itemId, kitId, cfg.useHookedBotshubPackets ? "hooked" : "raw");
            const bool followupQueued = cfg.useHookedBotshubPackets
                ? CtoS::SendPacketBotshubHooked(1, cfg.followupHeader)
                : CtoS::SendPacketBotshub(1, cfg.followupHeader);
            Log::Info("MaintenanceMgr: Legacy botshub salvage post-consume followup queued=%u", followupQueued ? 1u : 0u);
            WaitForBotshubQueueIdle("legacy-botshub-salvage-post-followup");
            WaitMs(40 + ping);
        } else {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending post-consume followup hdr=0x%X item=%u kit=%u via post-dispatch packet transport",
                      cfg.followupHeader, itemId, kitId);
            SendLegacySalvagePacketThreaded(cfg.followupHeader, "FOLLOWUP");
            WaitMs(40 + ping);
        }
    }

    bool consumedByInventoryFallback = false;
    if (!consumedByTrackedPtr && cfg.allowInventoryFallback) {
        consumedByInventoryFallback = ItemMgr::GetItemById(itemId) == nullptr;
    }
    result.consumed = consumedByTrackedPtr || consumedByInventoryFallback;

    if (result.consumed) {
        Log::Info("MaintenanceMgr: Item %u consumed by legacy botshub salvage tracked=%u fallback=%u",
                  itemId,
                  consumedByTrackedPtr ? 1u : 0u,
                  consumedByInventoryFallback ? 1u : 0u);
        DumpTrackedItemState("POST-LEGACY-BOTSHUB-CONSUME-ITEM", itemBefore);
        DumpInventoryRootWindow("POST-LEGACY-BOTSHUB-CONSUME");
        if (cfg.sendEnterAfterConsume) {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending Enter after consume item=%u", itemId);
            DumpSalvageUiStateOnGameThread();
            bool enterSent = SendRootFrameEnterKeyOnGameThread();
            if (!enterSent) {
                enterSent = SendWindowEnterKey();
            }
            Log::Info("MaintenanceMgr: Legacy botshub salvage post-consume Enter sent=%u item=%u",
                      enterSent ? 1u : 0u,
                      itemId);
            WaitMs(250u + ping);
            DumpSalvageUiStateOnGameThread();
        }
        if (cfg.waitForInventoryRestoreAfterConsume) {
            result.inventoryRestored = WaitForBagsPointerRestore(cfg.restoreAfterConsumeTimeoutMs);
            Log::Info("MaintenanceMgr: Legacy botshub post-consume inventory restore=%u item=%u",
                      result.inventoryRestored ? 1u : 0u,
                      itemId);
            DumpTrackedItemState("POST-LEGACY-BOTSHUB-CONSUME-RESTORE-ITEM", itemBefore);
        } else {
            Log::Info("MaintenanceMgr: Legacy botshub post-consume inventory restore skipped item=%u", itemId);
        }
        if (cfg.applyAutoItPostConsumeDwell) {
            const uint32_t dwellMs = 500u + ping;
            Log::Info("MaintenanceMgr: Legacy botshub applying AutoIt post-consume dwell=%u item=%u", dwellMs, itemId);
            WaitMs(dwellMs);
        }
        return result;
    }

    if (cfg.waitForInventoryRestoreAfterConsume) {
        result.inventoryRestored = WaitForBagsPointerRestore(cfg.stabilizeRestoreTimeoutMs);
    }
    const uint32_t trackedStillPresent = TrackedItemStillRepresentsOriginal(itemBefore, itemId) ? 1u : 0u;
    const uint32_t inventoryStillPresent =
        cfg.allowInventoryFallback ? (ItemMgr::GetItemById(itemId) ? 1u : 0u) : 0u;
    Log::Info("MaintenanceMgr: Legacy botshub salvage stabilized item=%u trackedStillPresent=%u inventoryStillPresent=%u bagsRestored=%u",
              itemId,
              trackedStillPresent,
              inventoryStillPresent,
              result.inventoryRestored ? 1u : 0u);
    DumpTrackedItemState("POST-LEGACY-BOTSHUB-STABILIZE-ITEM", itemBefore);
    DumpInventoryRootWindow("POST-LEGACY-BOTSHUB-STABILIZE");
    return result;
}

bool SalvageItemNative(uint32_t kitId, uint32_t itemId, bool confirmByEnter) {
    if (!kitId || !itemId) {
        Log::Warn("MaintenanceMgr: SalvageItemNative rejected kit=%u item=%u", kitId, itemId);
        return false;
    }
    if (!Offsets::Salvage || !Offsets::SalvageGlobal) {
        Log::Warn("MaintenanceMgr: SalvageItemNative missing local salvage offsets fn=0x%08X global=0x%08X",
                  static_cast<unsigned>(Offsets::Salvage),
                  static_cast<unsigned>(Offsets::SalvageGlobal));
        return false;
    }
    DumpSalvageOffsetDiagnostics();

    Item* itemBefore = ItemMgr::GetItemById(itemId);
    const uint32_t rarity = itemBefore ? GetRarity(itemBefore) : 0u;
    const bool needsConfirm = confirmByEnter || rarity == RARITY_GOLD || rarity == RARITY_PURPLE;
    const uint32_t ping = ChatMgr::GetPing();
    DumpTrackedItemState("PRE-SALVAGE-ITEM", itemBefore);

    const uint32_t sessionId = GetSalvageSessionId();
    if (!sessionId) {
        Log::Warn("MaintenanceMgr: SalvageItemNative missing session id for kit=%u item=%u", kitId, itemId);
        return false;
    }

    Log::Info("MaintenanceMgr: Native salvage queue item=%u kit=%u session=%u", itemId, kitId, sessionId);
    DumpInventoryRootWindow("PRE-SALVAGE");
    {
        const uint32_t startDelayMs = 40u + ping;
        Log::Info("MaintenanceMgr: Native salvage applying AutoIt pre-start delay=%u item=%u", startDelayMs, itemId);
        WaitMs(startDelayMs);
    }
    NativeSalvageTask salvageTask{};
    salvageTask.item_id = itemId;
    salvageTask.kit_id = kitId;
    salvageTask.session_id = sessionId;
    if (!CtoS::EnqueueGameCommand(&NativeSalvageInvoker, &salvageTask, sizeof(salvageTask))) {
        Log::Warn("MaintenanceMgr: Game-command salvage queue failed item=%u kit=%u session=%u", itemId, kitId, sessionId);
        return false;
    }

    WaitForGameCommandQueueIdle("native-salvage-start");

    // AutoIt upstream (BotsHub lib/Utils.au3 SalvageItem and the legacy
    // Utils-Storage.au3 variant) does:
    //   StartSalvage(...)
    //   Sleep(600 + ping)
    //   if gold/purple: ValidateSalvage() (Enter); Sleep(600 + ping)
    //   SendPacket(MATERIALS)
    // The Enter key confirms the "Are you sure you want to salvage this
    // valuable item?" dialog the native Salvage opens locally; sending
    // MATERIALS before the confirm is acknowledged is rejected server-side.
    WaitMs(600 + ping);
    if (needsConfirm) {
        Log::Info("MaintenanceMgr: Native salvage sending Enter (ValidateSalvage) BEFORE materials item=%u", itemId);
        DumpSalvageUiStateOnGameThread();
        bool enterSent = SendRootFrameEnterKeyOnGameThread();
        if (!enterSent) {
            enterSent = SendWindowEnterKey();
        }
        Log::Info("MaintenanceMgr: Native salvage pre-materials Enter sent=%u item=%u",
                  enterSent ? 1u : 0u,
                  itemId);
        WaitMs(600 + ping);
        DumpSalvageUiStateOnGameThread();
    }

    // The native 0x7A wrapper crashes on this Reforged build even with the
    // corrected 0x77 session-open path. Keep the stable raw SendPacket
    // transport here, but only after the native local init + 0x77 open have
    // run.
    Log::Info("MaintenanceMgr: Native salvage sending SALVAGE_MATERIALS item=%u kit=%u needsConfirm=%u hdr=0x%X (regular SendPacket after native open)",
              itemId, kitId, needsConfirm ? 1u : 0u, Packets::SALVAGE_MATERIALS);
    CtoS::SendPacket(1, Packets::SALVAGE_MATERIALS);
    WaitMs(40 + ping);
    DumpTrackedItemState("POST-SALVAGE-MATERIALS-ITEM", itemBefore);

    const bool bagsRestored = WaitForBagsPointerRestore(30000u);

    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < (500u + ping)) {
        if (!ItemMgr::GetItemById(itemId)) {
            Log::Info("MaintenanceMgr: Item %u consumed by Salvage function (auto-complete)", itemId);
            DumpTrackedItemState("POST-CONSUME-ITEM", itemBefore);
            const bool restoredAfterConsume = WaitForBagsPointerRestore(15000);
            Log::Info("MaintenanceMgr: Post-consume inventory restore=%u item=%u",
                      restoredAfterConsume ? 1u : 0u,
                      itemId);
            DumpTrackedItemState("POST-CONSUME-RESTORE-ITEM", itemBefore);
            return true;
        }
        WaitMs(100);
    }

    const bool itemStillPresent = ItemMgr::GetItemById(itemId) != nullptr;
    Log::Info("MaintenanceMgr: Native salvage stabilized item=%u stillPresent=%u bagsRestored=%u",
              itemId,
              itemStillPresent ? 1u : 0u,
              bagsRestored ? 1u : 0u);
    if (itemStillPresent) {
        Log::Warn("MaintenanceMgr: Native material salvage did not consume item=%u; cancelling session to avoid blocking modal",
                  itemId);
        CtoS::SendPacket(1, Packets::SALVAGE_SESSION_CANCEL);
        WaitMs(250u + ping);
        return false;
    }
    return true;
}

bool SalvageUpgradeNative(uint32_t kitId, uint32_t itemId, uint8_t modIndex, bool confirmByEnter) {
    if (!kitId || !itemId || modIndex > 2) {
        Log::Warn("MaintenanceMgr: SalvageUpgradeNative rejected kit=%u item=%u modIndex=%u",
                  kitId, itemId, modIndex);
        return false;
    }
    if (!Offsets::Salvage || !Offsets::SalvageGlobal) {
        Log::Warn("MaintenanceMgr: SalvageUpgradeNative missing local salvage offsets fn=0x%08X global=0x%08X",
                  static_cast<unsigned>(Offsets::Salvage),
                  static_cast<unsigned>(Offsets::SalvageGlobal));
        return false;
    }
    DumpSalvageOffsetDiagnostics();

    Item* itemBefore = ItemMgr::GetItemById(itemId);
    const uint32_t rarity = itemBefore ? GetRarity(itemBefore) : 0u;
    const bool needsConfirm = confirmByEnter || rarity == RARITY_GOLD || rarity == RARITY_PURPLE;
    const uint32_t ping = ChatMgr::GetPing();
    DumpTrackedItemState("PRE-SALVAGE-UPGRADE-ITEM", itemBefore);

    const uint32_t sessionId = GetSalvageSessionId();
    if (!sessionId) {
        Log::Warn("MaintenanceMgr: SalvageUpgradeNative missing session id for kit=%u item=%u modIndex=%u",
                  kitId, itemId, modIndex);
        return false;
    }

    Log::Info("MaintenanceMgr: Native upgrade salvage queue item=%u kit=%u session=%u modIndex=%u",
              itemId, kitId, sessionId, modIndex);
    DumpInventoryRootWindow("PRE-SALVAGE-UPGRADE");
    {
        const uint32_t startDelayMs = 40u + ping;
        Log::Info("MaintenanceMgr: Native upgrade salvage applying AutoIt pre-start delay=%u item=%u",
                  startDelayMs, itemId);
        WaitMs(startDelayMs);
    }

    NativeSalvageTask salvageTask{};
    salvageTask.item_id = itemId;
    salvageTask.kit_id = kitId;
    salvageTask.session_id = sessionId;
    if (!CtoS::EnqueueGameCommand(&NativeSalvageInvoker, &salvageTask, sizeof(salvageTask))) {
        Log::Warn("MaintenanceMgr: Game-command upgrade salvage queue failed item=%u kit=%u session=%u",
                  itemId, kitId, sessionId);
        return false;
    }

    WaitForGameCommandQueueIdle("native-salvage-upgrade-start");

    WaitMs(600 + ping);
    if (needsConfirm) {
        Log::Info("MaintenanceMgr: Native upgrade salvage sending Enter (ValidateSalvage) BEFORE upgrade item=%u modIndex=%u",
                  itemId, modIndex);
        DumpSalvageUiStateOnGameThread();
        bool enterSent = SendRootFrameEnterKeyOnGameThread();
        if (!enterSent) {
            enterSent = SendWindowEnterKey();
        }
        Log::Info("MaintenanceMgr: Native upgrade salvage pre-upgrade Enter sent=%u item=%u",
                  enterSent ? 1u : 0u,
                  itemId);
        WaitMs(600 + ping);
        DumpSalvageUiStateOnGameThread();
    }

    Log::Info("MaintenanceMgr: Native salvage sending SALVAGE_UPGRADE item=%u kit=%u modIndex=%u needsConfirm=%u hdr=0x%X",
              itemId, kitId, modIndex, needsConfirm ? 1u : 0u, Packets::SALVAGE_UPGRADE);
    ItemMgr::SalvageUpgrade(static_cast<uint32_t>(modIndex));
    WaitMs(40 + ping);
    DumpTrackedItemState("POST-SALVAGE-UPGRADE-ITEM", itemBefore);

    const bool bagsRestored = WaitForBagsPointerRestore(30000u);

    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < (500u + ping)) {
        if (!ItemMgr::GetItemById(itemId)) {
            Log::Info("MaintenanceMgr: Item %u consumed by SalvageUpgrade modIndex=%u",
                      itemId, modIndex);
            DumpTrackedItemState("POST-UPGRADE-CONSUME-ITEM", itemBefore);
            const bool restoredAfterConsume = WaitForBagsPointerRestore(15000);
            Log::Info("MaintenanceMgr: Post-upgrade-consume inventory restore=%u item=%u",
                      restoredAfterConsume ? 1u : 0u,
                      itemId);
            DumpTrackedItemState("POST-UPGRADE-CONSUME-RESTORE-ITEM", itemBefore);
            return true;
        }
        WaitMs(100);
    }

    Log::Info("MaintenanceMgr: Native upgrade salvage stabilized item=%u stillPresent=%u bagsRestored=%u",
              itemId,
              ItemMgr::GetItemById(itemId) ? 1u : 0u,
              bagsRestored ? 1u : 0u);
    return true;
}

bool SalvageItemLegacyBotshub(uint32_t kitId, uint32_t itemId, uint32_t followupHeader, bool followupAfterConsume, bool sendMaterials, bool sendEnterAfterConsume) {
    LegacyBotshubSalvageConfig cfg{};
    cfg.followupHeader = followupHeader;
    cfg.followupAfterConsume = followupAfterConsume;
    cfg.sendMaterials = sendMaterials;
    cfg.sendMaterialsViaBotshub = true;
    cfg.waitForStartQueueIdleBeforeMaterials = false;
    cfg.waitForMaterialsQueueIdle = false;
    cfg.followupViaBotshub = true;
    cfg.useHookedBotshubPackets = false;
    cfg.sendEnterAfterConsume = sendEnterAfterConsume;
    cfg.allowInventoryFallback = false;
    cfg.waitForInventoryRestoreAfterConsume = false;
    cfg.stabilizeRestoreTimeoutMs = 0u;
    cfg.applyAutoItPostConsumeDwell = true;
    return SalvageItemLegacyBotshubImpl(kitId, itemId, nullptr, cfg).queued;
}

bool SalvageItemLegacyBotshubTracked(uint32_t kitId, uint32_t itemId, Item* trackedItem, bool waitForInventoryRestoreAfterConsume, uint32_t forcedSessionId, bool allowZeroSession) {
    LegacyBotshubSalvageConfig cfg{};
    cfg.forcedSessionId = forcedSessionId;
    cfg.sendMaterialsViaBotshub = true;
    cfg.waitForStartQueueIdleBeforeMaterials = false;
    cfg.waitForMaterialsQueueIdle = false;
    cfg.useHookedBotshubPackets = false;
    cfg.allowZeroSession = allowZeroSession;
    cfg.allowInventoryFallback = false;
    cfg.waitForInventoryRestoreAfterConsume = waitForInventoryRestoreAfterConsume;
    cfg.stabilizeRestoreTimeoutMs = waitForInventoryRestoreAfterConsume ? 1000u : 0u;
    cfg.applyAutoItPostConsumeDwell = true;
    return SalvageItemLegacyBotshubImpl(kitId, itemId, trackedItem, cfg).consumed;
}

uint32_t SalvageJunkItems() {
    Item* kit = FindSalvageKit();
    if (!kit) {
        Log::Warn("MaintenanceMgr: No salvage kit found in inventory");
        return 0;
    }
    const uint32_t kitId = kit->item_id;
    const uint32_t batchSessionId = GetSalvageSessionId();
    if (!batchSessionId) {
        Log::Warn("MaintenanceMgr: No salvage session available for junk salvage");
        return 0;
    }

    struct SalvageBatchEntry {
        uint32_t itemId;
        uint32_t modelId;
        Item* trackedPtr;
    };

    // Phase 1: Collect salvage targets before the first salvage collapses the
    // live inventory roots. This mirrors AutoIt's bag-scan behavior more
    // closely than re-querying ItemMgr between salvages.
    SalvageBatchEntry toSalvage[64];
    uint32_t toSalvageCount = 0;
    {
        Inventory* inv = ItemMgr::GetInventory();
        if (!inv) return 0;

        for (uint32_t b = 1; b <= 4 && toSalvageCount < 64; b++) {
            Bag* bag = inv->bags[b];
            if (!bag || !bag->items.buffer) continue;
            for (uint32_t s = 0; s < bag->items.size && toSalvageCount < 64; s++) {
                Item* item = bag->items.buffer[s];
                if (!item || item->model_id == 0) continue;
                if (IsKit(item->model_id)) continue;
                if (IsRareSkin(item->model_id)) continue;
                if (!IsIdentified(item)) continue;
                if (!(IsWeapon(item) || IsArmor(item))) continue;

                uint16_t rarity = GetRarity(item);
                if (rarity != RARITY_WHITE && rarity != RARITY_BLUE) continue;
                if (item->is_material_salvageable == 0) continue;

                toSalvage[toSalvageCount++] = {item->item_id, item->model_id, item};
            }
        }
    }

    if (toSalvageCount == 0) {
        Log::Info("MaintenanceMgr: No items to salvage");
        return 0;
    }
    if (toSalvageCount > 10) toSalvageCount = 10;
    Log::Info("MaintenanceMgr: Salvaging %u items (capped at 10) session=%u", toSalvageCount, batchSessionId);

    // Phase 2: Salvage each item via the legacy AutoIt-style botshub path.
    uint32_t salvaged = 0;
    for (uint32_t i = 0; i < toSalvageCount; i++) {
        const SalvageBatchEntry& entry = toSalvage[i];

        Log::Info("MaintenanceMgr: Salvaging [%u/%u] item=%u model=%u kit=%u",
                  i + 1, toSalvageCount, entry.itemId, entry.modelId, kitId);

        if (SalvageItemLegacyBotshubTracked(
                kitId,
                entry.itemId,
                entry.trackedPtr,
                false,
                batchSessionId,
                false)) {
            salvaged++;
        } else {
            Log::Warn("MaintenanceMgr: Legacy botshub salvage failed for item=%u kit=%u", entry.itemId, kitId);
        }
    }
    Log::Info("MaintenanceMgr: Salvaged %u items in AutoIt-style junk batch session=%u", salvaged, batchSessionId);
    return salvaged;
}

uint32_t IdentifyAndSalvageGoldItems() {
    return IdentifyAndSalvageGoldItems(Config{});
}

uint32_t IdentifyAndSalvageGoldItems(const Config& cfg) {
    // AutoIt Boss()->SalvageItems() identifies first, then salvages only
    // gold weapon-like items that are safe to liquidate.
    IdentifyAllItems();

    Item* kit = FindExpertSalvageKit();
    if (!kit) {
        Log::Warn("MaintenanceMgr: No salvage kit found for gold salvage pass");
        return 0;
    }
    const uint32_t kitId = kit->item_id;
    const uint32_t batchSessionId = GetSalvageSessionId();
    if (!batchSessionId) {
        Log::Warn("MaintenanceMgr: No salvage session available for post-run gold salvage");
        return 0;
    }

    struct GoldSalvageBatchEntry {
        uint32_t itemId;
        uint32_t modelId;
        uint8_t type;
        Item* trackedPtr;
        bool salvageUpgrade;
        uint8_t salvageIndex;
        const char* matchedRuleName;
    };

    GoldSalvageBatchEntry toSalvage[64];
    uint32_t toSalvageCount = 0;
    {
        Inventory* inv = ItemMgr::GetInventory();
        if (!inv) return 0;

        for (uint32_t b = 1; b <= 4 && toSalvageCount < _countof(toSalvage); b++) {
            Bag* bag = inv->bags[b];
            if (!bag || !bag->items.buffer) continue;
            for (uint32_t s = 0; s < bag->items.size && toSalvageCount < _countof(toSalvage); s++) {
                Item* item = bag->items.buffer[s];
                if (!item || item->model_id == 0) continue;
                const bool identified = IsIdentified(item);
                const bool weapon = IsWeapon(item);
                const uint16_t rarity = GetRarity(item);
                // Diagnostic: log every gold weapon we evaluate so we can see
                // which filter reason gates the Fire Wand / similar items.
                if (weapon && rarity == RARITY_GOLD) {
                    Log::Info("MaintenanceMgr: gold salvage scan bag=%u slot=%u item=%u model=%u type=%u identified=%d isKit=%d isRareSkin=%d shouldSell=%d matSalv=%u",
                              b, s, item->item_id, item->model_id, item->type,
                              identified ? 1 : 0,
                              IsKit(item->model_id) ? 1 : 0,
                              IsRareSkin(item->model_id) ? 1 : 0,
                              ShouldSellItem(item) ? 1 : 0,
                              item->is_material_salvageable);
                }
                if (!identified) continue;
                // Restore the safe filter: removing these gates caused the
                // server to disconnect us on invalid salvage commands.
                if (IsKit(item->model_id) || IsRareSkin(item->model_id)) continue;
                const UpgradeSalvageMatch upgradeMatch = FindUpgradeSalvageMatch(item, cfg);
                if (upgradeMatch.matched) {
                    Log::Info("MaintenanceMgr: useful upgrade match item=%u model=%u type=%u rarity=%u rule=%s modIndex=%u",
                              item->item_id,
                              item->model_id,
                              item->type,
                              rarity,
                              upgradeMatch.ruleName ? upgradeMatch.ruleName : "(unnamed)",
                              upgradeMatch.salvageIndex);
                    toSalvage[toSalvageCount++] = {
                        item->item_id,
                        item->model_id,
                        item->type,
                        item,
                        true,
                        upgradeMatch.salvageIndex,
                        upgradeMatch.ruleName,
                    };
                    continue;
                }

                if (!cfg.salvageUnmatchedGoldWeaponsForMaterials) continue;
                if (!ShouldSellItem(item)) continue;
                if (!weapon) continue;
                if (rarity != RARITY_GOLD) continue;
                if (item->is_material_salvageable == 0) continue;
                toSalvage[toSalvageCount++] = {item->item_id, item->model_id, item->type, item, false, 0, nullptr};
            }
        }
    }

    if (toSalvageCount == 0) {
        Log::Info("MaintenanceMgr: No gold items eligible for post-run salvage");
        return 0;
    }

    Log::Info("MaintenanceMgr: Post-run salvaging %u gold items session=%u", toSalvageCount, batchSessionId);

    // The legacy botshub queue stalls after the first salvage (tail never
    // advances), so items after the first aren't actually salvaged.
    // SalvageItemNative uses EnqueueGameCommand (a separate queue) for the
    // native salvage start and WaitForBagsPointerRestore to survive the
    // post-salvage inventory-root collapse correctly.
    uint32_t salvaged = 0;
    for (uint32_t i = 0; i < toSalvageCount; i++) {
        const GoldSalvageBatchEntry& entry = toSalvage[i];

        const bool ok = entry.salvageUpgrade
            ? SalvageUpgradeNative(kitId, entry.itemId, entry.salvageIndex, false)
            : SalvageItemNative(kitId, entry.itemId, false);

        Log::Info("MaintenanceMgr: gold salvage [%u/%u] item=%u model=%u type=%u kit=%u action=%s rule=%s",
                  i + 1,
                  toSalvageCount,
                  entry.itemId,
                  entry.modelId,
                  entry.type,
                  kitId,
                  entry.salvageUpgrade ? "upgrade" : "materials",
                  entry.matchedRuleName ? entry.matchedRuleName : "");

        if (ok) {
            salvaged++;
        } else {
            Log::Warn("MaintenanceMgr: Native gold salvage failed item=%u kit=%u action=%s",
                      entry.itemId,
                      kitId,
                      entry.salvageUpgrade ? "upgrade" : "materials");
        }
    }

    Log::Info("MaintenanceMgr: Post-run salvaged %u gold items in batch session=%u", salvaged, batchSessionId);
    return salvaged;
}

// ===== Kit Management =====

static uint32_t SellSalvageKitModelDownTo(uint32_t modelId, uint32_t keepCount, const char* label) {
    uint32_t currentCount = CountItemByModel(modelId);
    if (currentCount <= keepCount) return 0;

    uint32_t merchantItems = MerchantMgr::GetMerchantItemCount();
    if (merchantItems == 0) {
        Log::Warn("MaintenanceMgr: SellSalvageKitModelDownTo - merchant not open for %s", label);
        return 0;
    }

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t soldCount = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4 && currentCount > keepCount; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size && currentCount > keepCount; i++) {
            Item* item = bag->items.buffer[i];
            if (!item || item->item_id == 0 || item->model_id != modelId) continue;

            Log::Info("MaintenanceMgr: Selling excess %s salvage kit item=%u model=%u count=%u keep=%u",
                      label, item->item_id, item->model_id, currentCount, keepCount);
            MerchantMgr::SellInventoryItem(item->item_id, 1u);
            WaitMs(300);
            soldCount++;
            currentCount = CountItemByModel(modelId);
        }
    }

    if (soldCount > 0) {
        Log::Info("MaintenanceMgr: Sold %u excess %s salvage kits; count now %u/%u",
                  soldCount, label, currentCount, keepCount);
    }
    return soldCount;
}

static uint32_t SellExcessSalvageKits(const Config& cfg) {
    uint32_t sold = 0;
    sold += SellSalvageKitModelDownTo(ItemModelIds::RARE_SALVAGE_KIT, 0, "rare");
    sold += SellSalvageKitModelDownTo(ItemModelIds::ALT_SALVAGE_KIT, 0, "alt");
    const bool preferSuperior =
        CountItemByModel(ItemModelIds::SUPERIOR_SALVAGE_KIT) > 0 ||
        MerchantMgr::GetMerchantItemIdByModelId(ItemModelIds::SUPERIOR_SALVAGE_KIT) != 0;
    if (preferSuperior) {
        sold += SellSalvageKitModelDownTo(ItemModelIds::EXPERT_SALVAGE_KIT, 0, "expert");
        sold += SellSalvageKitModelDownTo(ItemModelIds::SUPERIOR_SALVAGE_KIT,
                                          cfg.targetExpertSalvageKits,
                                          "superior");
    } else {
        sold += SellSalvageKitModelDownTo(ItemModelIds::EXPERT_SALVAGE_KIT,
                                          cfg.targetExpertSalvageKits,
                                          "expert");
    }
    sold += SellSalvageKitModelDownTo(ItemModelIds::SALVAGE_KIT, cfg.targetSalvageKits, "regular");
    return sold;
}

static bool BuyMerchantModel(uint32_t modelId, uint32_t quantity, const char* label) {
    const uint32_t itemId = MerchantMgr::GetMerchantItemIdByModelId(modelId);
    if (!itemId) {
        Log::Warn("MaintenanceMgr: Merchant does not sell %s model=%u", label, modelId);
        return false;
    }
    if (!MerchantMgr::BuyMerchantItem(itemId, quantity)) {
        Log::Warn("MaintenanceMgr: Could not buy %u %s model=%u item=%u",
                  quantity, label, modelId, itemId);
        return false;
    }
    Log::Info("MaintenanceMgr: Bought %u %s model=%u item=%u",
              quantity, label, modelId, itemId);
    WaitMs(1000);
    return true;
}

static bool BuyHighGradeSalvageKits(uint32_t quantity) {
    if (MerchantMgr::GetMerchantItemIdByModelId(ItemModelIds::SUPERIOR_SALVAGE_KIT) != 0) {
        return BuyMerchantModel(ItemModelIds::SUPERIOR_SALVAGE_KIT, quantity, "superior salvage kit");
    }
    return BuyMerchantModel(ItemModelIds::EXPERT_SALVAGE_KIT, quantity, "expert salvage kit");
}

static uint32_t ClampKitPurchaseToFreeSlotBudget(uint32_t requested, const Config& cfg, const char* label) {
    if (requested == 0u) return 0u;
    const uint32_t freeSlots = CountFreeSlots();
    if (freeSlots <= cfg.minFreeSlots) {
        Log::Info("MaintenanceMgr: Skipping %s purchase; freeSlots=%u <= preferred=%u",
                  label,
                  freeSlots,
                  cfg.minFreeSlots);
        return 0u;
    }

    const uint32_t spareSlots = freeSlots - cfg.minFreeSlots;
    const uint32_t quantity = requested < spareSlots ? requested : spareSlots;
    if (quantity < requested) {
        Log::Info("MaintenanceMgr: Limiting %s purchase %u -> %u to preserve %u free slots",
                  label,
                  requested,
                  quantity,
                  cfg.minFreeSlots);
    }
    return quantity;
}

void BuyKitsToTarget(const Config& cfg) {
    uint32_t currentIdKits = CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT);
    uint32_t currentRegularSalvKits = CountRegularSalvageKits();
    uint32_t currentHighGradeSalvKits = CountHighGradeSalvageKits();

    // Check merchant is open
    uint32_t merchantItems = MerchantMgr::GetMerchantItemCount();
    if (merchantItems == 0) {
        Log::Warn("MaintenanceMgr: BuyKitsToTarget - merchant not open");
        return;
    }

    SellExcessSalvageKits(cfg);
    currentRegularSalvKits = CountRegularSalvageKits();
    currentHighGradeSalvKits = CountHighGradeSalvageKits();

    if (currentIdKits >= cfg.targetIdKits &&
        currentRegularSalvKits >= cfg.targetSalvageKits &&
        currentHighGradeSalvKits >= cfg.targetExpertSalvageKits) {
        Log::Info("MaintenanceMgr: Kits sufficient (id=%u/%u regularSalv=%u/%u highGradeSalv=%u/%u totalSalv=%u)",
                  currentIdKits, cfg.targetIdKits,
                  currentRegularSalvKits, cfg.targetSalvageKits,
                  currentHighGradeSalvKits, cfg.targetExpertSalvageKits,
                  CountAllSalvageKits());
        return;
    }

    // Buy ID kits if needed
    if (currentIdKits < cfg.targetIdKits) {
        const uint32_t need = cfg.targetIdKits - currentIdKits;
        const uint32_t quantity = ClampKitPurchaseToFreeSlotBudget(need, cfg, "superior ID kit");
        if (quantity > 0u) {
            BuyMerchantModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT, quantity, "superior ID kit");
        }
    }

    currentRegularSalvKits = CountRegularSalvageKits();
    if (currentRegularSalvKits < cfg.targetSalvageKits) {
        const uint32_t need = cfg.targetSalvageKits - currentRegularSalvKits;
        const uint32_t quantity = ClampKitPurchaseToFreeSlotBudget(need, cfg, "regular salvage kit");
        if (quantity > 0u) {
            BuyMerchantModel(ItemModelIds::SALVAGE_KIT, quantity, "regular salvage kit");
        }
    }

    currentHighGradeSalvKits = CountHighGradeSalvageKits();
    if (currentHighGradeSalvKits < cfg.targetExpertSalvageKits) {
        const uint32_t need = cfg.targetExpertSalvageKits - currentHighGradeSalvKits;
        const uint32_t quantity = ClampKitPurchaseToFreeSlotBudget(need, cfg, "high-grade salvage kit");
        if (quantity > 0u) {
            BuyHighGradeSalvageKits(quantity);
        }
    }
}

namespace {

struct ConsetCraftSpec {
    const char* label;
    float x;
    float y;
    uint32_t modelId;
    uint32_t materials[2];
    uint32_t materialQuantities[2];
    uint32_t goldPerSet;
    uint32_t merchantPosition;
};

static const ConsetCraftSpec kConsetSpecs[3] = {
    {"Grail of Might", kEmbarkEyjaX, kEmbarkEyjaY, ItemModelIds::GRAIL_OF_MIGHT,
     {ItemModelIds::IRON_INGOT, ItemModelIds::DUST}, {50u, 50u}, 250u, 1u},
    {"Essence of Celerity", kEmbarkKwatX, kEmbarkKwatY, ItemModelIds::ESSENCE_OF_CELERITY,
     {ItemModelIds::FEATHERS, ItemModelIds::DUST}, {50u, 50u}, 250u, 1u},
    {"Armor of Salvation", kEmbarkAlcusX, kEmbarkAlcusY, ItemModelIds::ARMOR_OF_SALVATION,
     {ItemModelIds::IRON_INGOT, ItemModelIds::BONES}, {50u, 50u}, 250u, 1u},
};

static bool EnsureMap(uint32_t mapId) {
    if (mapId == 0u) {
        return false;
    }
    if (MapMgr::GetMapId() == mapId && MapMgr::GetLoadingState() == 1) {
        return WaitForMaintenanceMapRuntimeReady(mapId, 10000u, "EnsureMap existing");
    }
    if (mapId == MapIds::EMBARK_BEACH) {
        Log::Info("MaintenanceMgr: Traveling to Embark Beach via Asia/Japan default district for maintenance");
        if (MapMgr::Travel(mapId, kQuietAsiaJapanRegion, kQuietAsiaJapanDistrict, kQuietAsiaJapanLanguage)) {
            WaitMs(3000);
            if (WaitForMaintenanceMapRuntimeReady(mapId, 45000u, "EnsureMap Embark Asia/Japan")) {
                Log::Info("MaintenanceMgr: Reached Embark Beach region=%u district=%u",
                          MapMgr::GetRegion(),
                          MapMgr::GetDistrict());
                return true;
            }
        }

        Log::Warn("MaintenanceMgr: Asia/Japan Embark travel did not load; falling back to default map travel");
        if (!MapMgr::Travel(mapId)) {
            return false;
        }
        WaitMs(3000);
        if (WaitForMaintenanceMapRuntimeReady(mapId, 45000u, "EnsureMap Embark default")) {
            Log::Info("MaintenanceMgr: Reached fallback Embark Beach region=%u district=%u",
                      MapMgr::GetRegion(),
                      MapMgr::GetDistrict());
            return true;
        }
        return false;
    }
    if (!MapMgr::Travel(mapId)) {
        return false;
    }
    WaitMs(3000);
    return WaitForMaintenanceMapRuntimeReady(mapId, 60000u, "EnsureMap travel");
}

static uint32_t ResolveMaintenanceTown(const Config& cfg, uint32_t fallbackMapId) {
    return cfg.maintenanceTown != 0u ? cfg.maintenanceTown : fallbackMapId;
}

static uint32_t CountTotalConsetsStored() {
    return CountItemByModelInStorage(ItemModelIds::GRAIL_OF_MIGHT)
        + CountItemByModelInStorage(ItemModelIds::ESSENCE_OF_CELERITY)
        + CountItemByModelInStorage(ItemModelIds::ARMOR_OF_SALVATION);
}

static uint32_t CountCraftableMaterialUnitsByModel(uint32_t modelId, uint32_t quantityPerSet) {
    if (modelId == 0u || quantityPerSet == 0u) return 0u;
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0u;

    uint32_t units = 0u;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id != modelId) continue;
            const uint32_t quantity = item->quantity > 0 ? item->quantity : 1u;
            units += quantity / quantityPerSet;
        }
    }
    return units;
}

static uint32_t LargestSingleStackCraftBatchByModel(uint32_t modelId,
                                                    uint32_t quantityPerSet,
                                                    uint32_t cap) {
    if (modelId == 0u || quantityPerSet == 0u || cap == 0u) return 0u;
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0u;

    uint32_t largest = 0u;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id != modelId) continue;
            const uint32_t quantity = item->quantity > 0 ? item->quantity : 1u;
            const uint32_t craftable = quantity / quantityPerSet;
            if (craftable > largest) {
                largest = craftable;
            }
        }
    }
    return largest > cap ? cap : largest;
}

static uint32_t ComputeMaxSingleStackBatchForSpec(const ConsetCraftSpec& spec, uint32_t cap) {
    uint32_t batch = cap;
    for (uint32_t i = 0; i < _countof(spec.materials); ++i) {
        const uint32_t materialBatch =
            LargestSingleStackCraftBatchByModel(spec.materials[i], spec.materialQuantities[i], cap);
        if (materialBatch < batch) {
            batch = materialBatch;
        }
    }
    return batch;
}

static bool OpenEmbarkNpc(float x, float y, const char* label, uint32_t& npcId) {
    npcId = 0;
    if (MapMgr::GetMapId() == MapIds::EMBARK_BEACH && y > -1000.0f) {
        MoveNear(kEmbarkMaterialTraderX, kEmbarkMaterialTraderY, 350.0f, 30000, "Embark crafter approach");
    }
    if (!MoveNear(x, y, 250.0f, 45000, label)) {
        return false;
    }
    npcId = FindNearestNpcByAllegiance(x, y, 500.0f);
    if (!npcId) {
        Log::Warn("MaintenanceMgr: No NPC found for %s near (%.0f, %.0f)", label, x, y);
        return false;
    }
    auto* npc = AgentMgr::GetAgentByID(npcId);
    if (npc) {
        MoveNear(npc->x, npc->y, 125.0f, 10000, label);
    }
    return OpenNpcDialog(npcId, label, true);
}

static uint32_t CraftConsetAtNpc(const ConsetCraftSpec& spec, uint32_t desiredSets) {
    if (desiredSets == 0) return 0;

    uint32_t npcId = 0;
    if (!OpenEmbarkNpc(spec.x, spec.y, spec.label, npcId)) {
        return 0;
    }

    uint32_t crafterItemId = MerchantMgr::GetMerchantItemIdByModelId(spec.modelId);
    if (crafterItemId == 0u) {
        const uint32_t fallbackItemId = MerchantMgr::GetMerchantItemIdByPosition(spec.merchantPosition);
        if (fallbackItemId != 0u) {
            crafterItemId = fallbackItemId;
            Log::Warn("MaintenanceMgr: Using merchant row fallback for %s model=%u row=%u item=%u merchantItems=%u",
                      spec.label,
                      spec.modelId,
                      spec.merchantPosition,
                      crafterItemId,
                      MerchantMgr::GetMerchantItemCount());
        } else {
            Log::Warn("MaintenanceMgr: Could not resolve crafter item for %s model=%u merchantItems=%u",
                      spec.label,
                      spec.modelId,
                      MerchantMgr::GetMerchantItemCount());
            return 0;
        }
    }

    uint32_t crafted = 0;
    while (crafted < desiredSets) {
        const uint32_t remaining = desiredSets - crafted;
        uint32_t batch = remaining > 5u ? 5u : remaining;
        const uint32_t materialBatch = ComputeMaxSingleStackBatchForSpec(spec, batch);
        if (materialBatch == 0u) {
            Log::Warn("MaintenanceMgr: No single material stack can craft another %s batch "
                      "remaining=%u mats=[%u:%u,%u:%u]",
                      spec.label,
                      remaining,
                      spec.materials[0],
                      CountItemByModel(spec.materials[0]),
                      spec.materials[1],
                      CountItemByModel(spec.materials[1]));
            break;
        }
        if (batch > materialBatch) {
            batch = materialBatch;
        }
        const uint32_t before = CountItemByModel(spec.modelId);
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        const uint32_t totalGold = spec.goldPerSet * batch;
        Log::Info("MaintenanceMgr: Crafting %s item=%u batch=%u gold=%u mats=[%u:%u,%u:%u]",
                  spec.label,
                  crafterItemId,
                  batch,
                  totalGold,
                  spec.materials[0],
                  spec.materialQuantities[0] * batch,
                  spec.materials[1],
                  spec.materialQuantities[1] * batch);
        if (!MerchantMgr::CraftMerchantItem(crafterItemId,
                                            batch,
                                            totalGold,
                                            spec.materials,
                                            spec.materialQuantities,
                                            _countof(spec.materials))) {
            Log::Warn("MaintenanceMgr: native crafter transact rejected for %s item=%u batch=%u",
                      spec.label,
                      crafterItemId,
                      batch);
            break;
        }

        const DWORD start = GetTickCount();
        bool changed = false;
        while ((GetTickCount() - start) < 8000) {
            const uint32_t after = CountItemByModel(spec.modelId);
            if (after > before || ItemMgr::GetGoldCharacter() < goldBefore) {
                crafted += (after > before) ? (after - before) : batch;
                changed = true;
                break;
            }
            WaitMs(200);
        }
        if (!changed) {
            Log::Warn("MaintenanceMgr: Timed out waiting for %s batch craft completion; trying single native craft",
                      spec.label);
            if (batch > 1u &&
                MerchantMgr::CraftMerchantItem(crafterItemId,
                                               1u,
                                               spec.goldPerSet,
                                               spec.materials,
                                               spec.materialQuantities,
                                               _countof(spec.materials))) {
                const DWORD singleStart = GetTickCount();
                while ((GetTickCount() - singleStart) < 5000) {
                    const uint32_t after = CountItemByModel(spec.modelId);
                    if (after > before || ItemMgr::GetGoldCharacter() < goldBefore) {
                        crafted += (after > before) ? (after - before) : 1u;
                        changed = true;
                        break;
                    }
                    WaitMs(200);
                }
            }
            if (!changed) {
                Log::Warn("MaintenanceMgr: Single native craft also made no progress for %s", spec.label);
                break;
            }
        }
        WaitMs(ChatMgr::GetPing() + 300);
    }

    if (crafted > 0) {
        Log::Info("MaintenanceMgr: Crafted %u x %s", crafted, spec.label);
    }
    return crafted;
}

static bool EnsureEmbarkMaterialTraderOpen(uint32_t& traderNpc) {
    if (HasOpenMaterialTraderInventory()) return true;
    if (!MoveNear(kEmbarkMaterialTraderX, kEmbarkMaterialTraderY, 250.0f, 45000, "Material Trader")) {
        return false;
    }
    traderNpc = FindNpcByPlayerNumberNearCoords(kEmbarkMaterialTraderNpcId,
                                                kEmbarkMaterialTraderX,
                                                kEmbarkMaterialTraderY,
                                                900.0f);
    if (traderNpc == 0) {
        traderNpc = FindNearestNpcByAllegiance(kEmbarkMaterialTraderX, kEmbarkMaterialTraderY, 500.0f);
    }
    if (traderNpc == 0) {
        Log::Warn("MaintenanceMgr: No Embark material trader NPC found near (%.0f, %.0f)",
                  kEmbarkMaterialTraderX, kEmbarkMaterialTraderY);
        return false;
    }
    for (uint32_t attempt = 1; attempt <= 3; ++attempt) {
        if (OpenNpcDialog(traderNpc, "Material Trader", true) &&
            HasOpenMaterialTraderInventory()) {
            return true;
        }
        Log::Warn("MaintenanceMgr: Embark material trader attempt=%u opened non-trader inventory items=%u",
                  attempt, MerchantMgr::GetMerchantItemCount());
        AgentMgr::CancelAction();
        WaitMs(400);
    }
    return false;
}

static bool BuyConsetMaterialVerified(uint32_t modelId,
                                      uint32_t quantity,
                                      const char* label,
                                      uint32_t& traderNpc) {
    uint32_t remaining = quantity;
    for (uint32_t attempt = 0; attempt < 3u && remaining > 0; ++attempt) {
        if (!EnsureEmbarkMaterialTraderOpen(traderNpc)) {
            Log::Warn("MaintenanceMgr: Unable to open Material Trader for model=%u (%s) attempt=%u",
                      modelId, label ? label : "material", attempt + 1u);
            return false;
        }

        const uint32_t before = CountItemByModel(modelId);
        Log::Info("MaintenanceMgr: Buying materials model=%u qty=%u for %s attempt=%u",
                  modelId, remaining, label ? label : "material", attempt + 1u);
        MerchantMgr::BuyMaterials(modelId, remaining);
        WaitMs(600);

        const uint32_t after = CountItemByModel(modelId);
        const uint32_t gained = (after > before) ? (after - before) : 0u;
        if (gained >= remaining) {
            return true;
        }

        if (gained > 0) {
            remaining -= gained;
            Log::Warn("MaintenanceMgr: Material buy partial model=%u gained=%u remaining=%u label=%s",
                      modelId, gained, remaining, label ? label : "material");
        } else {
            Log::Warn("MaintenanceMgr: Material buy made no progress model=%u remaining=%u label=%s merchantItems=%u",
                      modelId, remaining, label ? label : "material", MerchantMgr::GetMerchantItemCount());
        }

        WaitMs(400);
    }

    Log::Warn("MaintenanceMgr: Material buy incomplete model=%u requestedQty=%u remaining=%u label=%s",
              modelId, quantity, remaining, label ? label : "material");
    return false;
}

static uint32_t ComputeBalancedConsetSetsAvailable(uint32_t requestedSets) {
    const uint32_t iron = CountCraftableMaterialUnitsByModel(ItemModelIds::IRON_INGOT, 50u);
    const uint32_t dust = CountCraftableMaterialUnitsByModel(ItemModelIds::DUST, 50u);
    const uint32_t feather = CountCraftableMaterialUnitsByModel(ItemModelIds::FEATHERS, 50u);
    const uint32_t bones = CountCraftableMaterialUnitsByModel(ItemModelIds::BONES, 50u);

    uint32_t balanced = requestedSets;
    if ((iron / 2u) < balanced) balanced = iron / 2u;
    if ((dust / 2u) < balanced) balanced = dust / 2u;
    if (feather < balanced) balanced = feather;
    if (bones < balanced) balanced = bones;
    return balanced;
}

static uint32_t ComputeAvailableSetsForSpec(const ConsetCraftSpec& spec, uint32_t requestedSets) {
    if (requestedSets == 0) return 0;

    uint32_t available = requestedSets;
    for (uint32_t i = 0; i < 2; ++i) {
        const uint32_t sets =
            CountCraftableMaterialUnitsByModel(spec.materials[i], spec.materialQuantities[i]);
        if (sets < available) {
            available = sets;
        }
    }
    return available;
}

static const ConsetCraftSpec* FindConsetCraftSpec(uint32_t modelId) {
    for (const auto& spec : kConsetSpecs) {
        if (spec.modelId == modelId) {
            return &spec;
        }
    }
    return nullptr;
}

static bool EnsureCharacterGoldForConsetCrafting(const Config& cfg, uint32_t requiredCraftGold) {
    const uint32_t desiredCharGold = requiredCraftGold > cfg.consetWithdrawGoldTarget
        ? requiredCraftGold
        : cfg.consetWithdrawGoldTarget;
    const uint32_t cappedDesired = desiredCharGold > 100000u ? 100000u : desiredCharGold;
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    if (charGold >= cappedDesired || cappedDesired == 0u) {
        return true;
    }

    OpenXunlaiChest(kEmbarkXunlaiX, kEmbarkXunlaiY);
    const uint32_t storageGold = ItemMgr::GetGoldStorage();
    charGold = ItemMgr::GetGoldCharacter();
    if (charGold >= cappedDesired) {
        return true;
    }

    const uint32_t normalSafeWithdraw = storageGold > cfg.consetStorageGoldFloor
        ? (storageGold - cfg.consetStorageGoldFloor)
        : 0u;
    const uint32_t emergencyFloor = cfg.consetStorageGoldFloor > cappedDesired
        ? (cfg.consetStorageGoldFloor - cappedDesired)
        : 0u;
    const uint32_t emergencySafeWithdraw = storageGold > emergencyFloor
        ? (storageGold - emergencyFloor)
        : 0u;
    uint32_t safeWithdraw = normalSafeWithdraw;
    if (safeWithdraw == 0u && emergencySafeWithdraw > 0u) {
        safeWithdraw = emergencySafeWithdraw;
        Log::Info("MaintenanceMgr: Character conset crafting borrowing below storage floor storageGold=%u floor=%u emergencyFloor=%u",
                  storageGold, cfg.consetStorageGoldFloor, emergencyFloor);
    }

    uint32_t request = cappedDesired > charGold ? (cappedDesired - charGold) : 0u;
    if (request > safeWithdraw) {
        request = safeWithdraw;
    }
    if (request == 0u) {
        Log::Warn("MaintenanceMgr: Character conset crafting cannot withdraw gold char=%u desired=%u storage=%u floor=%u",
                  charGold, cappedDesired, storageGold, cfg.consetStorageGoldFloor);
        return charGold >= requiredCraftGold;
    }

    WithdrawGold(request);
    WaitMs(500);
    return ItemMgr::GetGoldCharacter() >= requiredCraftGold;
}

static uint32_t CraftConsetModelForCharacter(const Config& cfg, uint32_t modelId, uint32_t desiredQuantity) {
    const ConsetCraftSpec* spec = FindConsetCraftSpec(modelId);
    if (!spec || desiredQuantity == 0u) {
        return 0u;
    }

    const uint32_t requiredCraftGold = spec->goldPerSet * desiredQuantity;
    EnsureCharacterGoldForConsetCrafting(cfg, requiredCraftGold);

    uint32_t traderNpc = 0;
    if (!EnsureEmbarkMaterialTraderOpen(traderNpc)) {
        Log::Warn("MaintenanceMgr: Character conset crafting could not open material trader for %s",
                  spec->label);
        return 0u;
    }

    for (uint32_t i = 0; i < _countof(spec->materials); ++i) {
        const uint32_t required = spec->materialQuantities[i] * desiredQuantity;
        const uint32_t current = CountItemByModel(spec->materials[i]);
        const uint32_t deficit = required > current ? (required - current) : 0u;
        if (deficit == 0u) {
            continue;
        }

        if (!BuyConsetMaterialVerified(spec->materials[i], deficit, spec->label, traderNpc)) {
            Log::Warn("MaintenanceMgr: Character conset crafting material top-off incomplete for %s model=%u required=%u current=%u",
                      spec->label,
                      spec->materials[i],
                      required,
                      CountItemByModel(spec->materials[i]));
        }
    }

    const uint32_t craftable = ComputeAvailableSetsForSpec(*spec, desiredQuantity);
    if (craftable == 0u) {
        Log::Warn("MaintenanceMgr: Character conset crafting has zero craftable quantity for %s desired=%u mats=[%u:%u,%u:%u]",
                  spec->label,
                  desiredQuantity,
                  spec->materials[0],
                  CountItemByModel(spec->materials[0]),
                  spec->materials[1],
                  CountItemByModel(spec->materials[1]));
        return 0u;
    }

    if (craftable < desiredQuantity) {
        Log::Warn("MaintenanceMgr: Character conset crafting reduced %s from %u to %u due to material/gold availability",
                  spec->label,
                  desiredQuantity,
                  craftable);
    }

    EnsureCharacterGoldForConsetCrafting(cfg, spec->goldPerSet * craftable);
    const uint32_t before = CountItemByModel(spec->modelId);
    const uint32_t crafted = CraftConsetAtNpc(*spec, craftable);
    const uint32_t after = CountItemByModel(spec->modelId);
    Log::Info("MaintenanceMgr: Character conset crafting result %s requested=%u crafted=%u count=%u->%u",
              spec->label,
              desiredQuantity,
              crafted,
              before,
              after);
    return crafted;
}

static bool CraftCharacterConsetRebalance(const Config& cfg) {
    if (!NeedsCharacterConsetRebalance(cfg)) {
        return false;
    }

    const uint32_t returnMapId = ResolveMaintenanceTown(cfg, MapMgr::GetMapId());
    const uint32_t grail = CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT);
    const uint32_t essence = CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY);
    const uint32_t armor = CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION);
    const uint32_t target = cfg.targetCharacterConsetsEach;

    Log::Info("MaintenanceMgr: Character conset imbalance will be corrected by crafting only to target floor "
              "(grail=%u essence=%u armor=%u target=%u)",
              grail, essence, armor, target);

    if (!EnsureMap(MapIds::EMBARK_BEACH)) {
        Log::Warn("MaintenanceMgr: Failed to reach Embark Beach for character conset rebalance crafting");
        return false;
    }

    uint32_t craftedTotal = 0u;
    if (grail < target) {
        craftedTotal += CraftConsetModelForCharacter(cfg, ItemModelIds::GRAIL_OF_MIGHT, target - grail);
    }
    if (essence < target) {
        craftedTotal += CraftConsetModelForCharacter(cfg, ItemModelIds::ESSENCE_OF_CELERITY, target - essence);
    }
    if (armor < target) {
        craftedTotal += CraftConsetModelForCharacter(cfg, ItemModelIds::ARMOR_OF_SALVATION, target - armor);
    }

    if (returnMapId != 0u && MapMgr::GetMapId() != returnMapId) {
        if (!EnsureMap(returnMapId)) {
            Log::Warn("MaintenanceMgr: Failed to return to maintenance town %u after character conset rebalance crafting",
                      returnMapId);
            return craftedTotal > 0u;
        }
    }

    Log::Info("MaintenanceMgr: Character conset rebalance crafting complete crafted=%u consets=%u/%u/%u",
              craftedTotal,
              CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT),
              CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY),
              CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION));
    return craftedTotal > 0u;
}

static bool CraftCharacterConsetRestock(const Config& cfg) {
    if (!NeedsCharacterConsetRestock(cfg)) {
        return false;
    }

    const uint32_t returnMapId = ResolveMaintenanceTown(cfg, MapMgr::GetMapId());
    const uint32_t target = cfg.targetCharacterConsetsEach;
    const uint32_t grail = CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT);
    const uint32_t essence = CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY);
    const uint32_t armor = CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION);

    Log::Info("MaintenanceMgr: Character conset restock will be corrected by crafting, not Xunlai withdrawal "
              "(grail=%u essence=%u armor=%u target=%u)",
              grail, essence, armor, target);

    if (!EnsureMap(MapIds::EMBARK_BEACH)) {
        Log::Warn("MaintenanceMgr: Failed to reach Embark Beach for character conset restock crafting");
        return false;
    }

    uint32_t craftedTotal = 0u;
    if (grail < target) {
        craftedTotal += CraftConsetModelForCharacter(cfg, ItemModelIds::GRAIL_OF_MIGHT, target - grail);
    }
    if (essence < target) {
        craftedTotal += CraftConsetModelForCharacter(cfg, ItemModelIds::ESSENCE_OF_CELERITY, target - essence);
    }
    if (armor < target) {
        craftedTotal += CraftConsetModelForCharacter(cfg, ItemModelIds::ARMOR_OF_SALVATION, target - armor);
    }

    if (returnMapId != 0u && MapMgr::GetMapId() != returnMapId) {
        if (!EnsureMap(returnMapId)) {
            Log::Warn("MaintenanceMgr: Failed to return to maintenance town %u after character conset restock crafting",
                      returnMapId);
            return craftedTotal > 0u;
        }
    }

    Log::Info("MaintenanceMgr: Character conset restock crafting complete crafted=%u consets=%u/%u/%u",
              craftedTotal,
              CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT),
              CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY),
              CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION));
    return craftedTotal > 0u;
}

static uint32_t CountConsetCraftMaterialStacks() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t stacks = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item) continue;
            switch (item->model_id) {
                case ItemModelIds::IRON_INGOT:
                case ItemModelIds::DUST:
                case ItemModelIds::FEATHERS:
                case ItemModelIds::BONES:
                    ++stacks;
                    break;
                default:
                    break;
            }
        }
    }
    return stacks;
}

static bool HasAnyConsetCraftMaterialStacks() {
    return CountConsetCraftMaterialStacks() > 0u;
}

static bool HasAnyCraftableConsetSet() {
    const uint32_t iron = CountItemByModel(ItemModelIds::IRON_INGOT);
    const uint32_t dust = CountItemByModel(ItemModelIds::DUST);
    const uint32_t feather = CountItemByModel(ItemModelIds::FEATHERS);
    const uint32_t bones = CountItemByModel(ItemModelIds::BONES);
    return (iron >= 50u && dust >= 50u) ||
           (dust >= 50u && feather >= 50u) ||
           (iron >= 50u && bones >= 50u);
}

static bool RestockConsetsOnce(const Config& cfg) {
    if (!EnsureMap(MapIds::EMBARK_BEACH)) {
        Log::Warn("MaintenanceMgr: Failed to reach Embark Beach for conset restock");
        return false;
    }

    uint32_t storageGold = ItemMgr::GetGoldStorage();
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    const uint32_t desiredCharGold = cfg.consetWithdrawGoldTarget > 100000u ? 100000u : cfg.consetWithdrawGoldTarget;
    if (storageGold > cfg.consetStorageGoldFloor && charGold < desiredCharGold) {
        OpenXunlaiChest(kEmbarkXunlaiX, kEmbarkXunlaiY);
        storageGold = ItemMgr::GetGoldStorage();
        charGold = ItemMgr::GetGoldCharacter();
        const uint32_t safeWithdraw = storageGold > cfg.consetStorageGoldFloor
            ? (storageGold - cfg.consetStorageGoldFloor) : 0u;
        uint32_t request = desiredCharGold > charGold ? (desiredCharGold - charGold) : 0u;
        if (request > safeWithdraw) request = safeWithdraw;
        if (request > 0) {
            WithdrawGold(request);
            WaitMs(500);
        }
    }

    uint32_t setsToCraft = cfg.consetBatchSets > 0 ? cfg.consetBatchSets : 1u;
    const uint32_t storedArmor = CountItemByModelInStorage(ItemModelIds::ARMOR_OF_SALVATION);
    const uint32_t storedEssence = CountItemByModelInStorage(ItemModelIds::ESSENCE_OF_CELERITY);
    const uint32_t storedGrail = CountItemByModelInStorage(ItemModelIds::GRAIL_OF_MIGHT);
    uint32_t maxDeficit = 0;
    if (cfg.targetStoredConsetsEach > storedArmor) maxDeficit = cfg.targetStoredConsetsEach - storedArmor;
    if (cfg.targetStoredConsetsEach > storedEssence && (cfg.targetStoredConsetsEach - storedEssence) > maxDeficit) {
        maxDeficit = cfg.targetStoredConsetsEach - storedEssence;
    }
    if (cfg.targetStoredConsetsEach > storedGrail && (cfg.targetStoredConsetsEach - storedGrail) > maxDeficit) {
        maxDeficit = cfg.targetStoredConsetsEach - storedGrail;
    }
    if (maxDeficit > 0 && maxDeficit < setsToCraft) {
        setsToCraft = maxDeficit;
    }

    uint32_t traderNpc = 0;
    if (!EnsureEmbarkMaterialTraderOpen(traderNpc)) {
        return false;
    }

    struct MaterialNeed {
        uint32_t modelId;
        uint32_t required;
        const char* label;
    };
    MaterialNeed needs[] = {
        {ItemModelIds::IRON_INGOT, 100u * setsToCraft, "Conset iron"},
        {ItemModelIds::DUST, 100u * setsToCraft, "Conset dust"},
        {ItemModelIds::FEATHERS, 50u * setsToCraft, "Conset feather"},
        {ItemModelIds::BONES, 50u * setsToCraft, "Conset bone"},
    };

    bool buyPhaseOk = true;
    for (const auto& need : needs) {
        const uint32_t currentQty = CountItemByModel(need.modelId);
        const uint32_t deficit = need.required > currentQty ? (need.required - currentQty) : 0u;
        if (deficit == 0u) {
            Log::Info("MaintenanceMgr: Conset material already sufficient model=%u current=%u required=%u label=%s",
                      need.modelId, currentQty, need.required, need.label);
            continue;
        }
        if (!BuyConsetMaterialVerified(need.modelId, deficit, need.label, traderNpc)) {
            buyPhaseOk = false;
        }
    }

    const uint32_t balancedSets = ComputeBalancedConsetSetsAvailable(setsToCraft);
    if (balancedSets < setsToCraft) {
        Log::Warn("MaintenanceMgr: Reducing conset craft count from %u to %u based on actual materials "
                  "(iron=%u dust=%u feather=%u bones=%u buyPhaseOk=%u)",
                  setsToCraft, balancedSets,
                  CountItemByModel(ItemModelIds::IRON_INGOT),
                  CountItemByModel(ItemModelIds::DUST),
                  CountItemByModel(ItemModelIds::FEATHERS),
                  CountItemByModel(ItemModelIds::BONES),
                  buyPhaseOk ? 1u : 0u);
        setsToCraft = balancedSets;
    }

    uint32_t craftedTotal = 0;
    if (setsToCraft > 0) {
        for (const auto& spec : kConsetSpecs) {
            craftedTotal += CraftConsetAtNpc(spec, setsToCraft);
        }
    } else {
        Log::Warn("MaintenanceMgr: No full balanced conset set available; pressure-crafting from on-hand materials");
        const uint32_t requestedSets = cfg.consetBatchSets > 0 ? cfg.consetBatchSets : 1u;
        for (const auto& spec : kConsetSpecs) {
            const uint32_t availableSets = ComputeAvailableSetsForSpec(spec, requestedSets);
            if (availableSets == 0) continue;
            craftedTotal += CraftConsetAtNpc(spec, availableSets);
        }
    }
    if (craftedTotal == 0) {
        Log::Warn("MaintenanceMgr: No consets crafted during restock pass");
        return false;
    }

    OpenXunlaiChest(kEmbarkXunlaiX, kEmbarkXunlaiY);
    const uint32_t consetModels[3] = {
        ItemModelIds::GRAIL_OF_MIGHT,
        ItemModelIds::ESSENCE_OF_CELERITY,
        ItemModelIds::ARMOR_OF_SALVATION,
    };
    DepositItemModelsToStorage(consetModels, _countof(consetModels));
    if (ItemMgr::GetGoldCharacter() > cfg.depositKeepOnChar) {
        DepositGold(cfg.depositKeepOnChar);
    }
    return true;
}

static bool PressureCraftConsetsOnce(const Config& cfg) {
    if (!EnsureMap(MapIds::EMBARK_BEACH)) {
        Log::Warn("MaintenanceMgr: Failed to reach Embark Beach for pressure conset crafting");
        return false;
    }

    uint32_t storageGold = ItemMgr::GetGoldStorage();
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    const uint32_t desiredCharGold = cfg.consetWithdrawGoldTarget > 100000u ? 100000u : cfg.consetWithdrawGoldTarget;
    if (charGold < desiredCharGold) {
        OpenXunlaiChest(kEmbarkXunlaiX, kEmbarkXunlaiY);
        storageGold = ItemMgr::GetGoldStorage();
        charGold = ItemMgr::GetGoldCharacter();
        const uint32_t normalSafeWithdraw = storageGold > cfg.consetStorageGoldFloor
            ? (storageGold - cfg.consetStorageGoldFloor) : 0u;
        const uint32_t emergencyFloor = cfg.consetStorageGoldFloor > desiredCharGold
            ? (cfg.consetStorageGoldFloor - desiredCharGold) : 0u;
        const uint32_t emergencySafeWithdraw = storageGold > emergencyFloor
            ? (storageGold - emergencyFloor) : 0u;
        uint32_t safeWithdraw = normalSafeWithdraw;
        if (safeWithdraw == 0u && emergencySafeWithdraw > 0u) {
            safeWithdraw = emergencySafeWithdraw;
            Log::Info("MaintenanceMgr: Pressure conset crafting borrowing below storage floor storageGold=%u floor=%u emergencyFloor=%u",
                      storageGold, cfg.consetStorageGoldFloor, emergencyFloor);
        }
        uint32_t request = desiredCharGold > charGold ? (desiredCharGold - charGold) : 0u;
        if (request > safeWithdraw) request = safeWithdraw;
        if (request > 0) {
            WithdrawGold(request);
            WaitMs(500);
        }
    }

    const uint32_t requestedBalancedSets = cfg.consetBatchSets > 0u ? cfg.consetBatchSets : 1u;
    const uint32_t balancedSets = ComputeBalancedConsetSetsAvailable(requestedBalancedSets);
    if (balancedSets > 0u) {
        Log::Info("MaintenanceMgr: Pressure conset crafting selected balanced bundle sets=%u "
                  "mats iron=%u dust=%u feathers=%u bones=%u",
                  balancedSets,
                  CountItemByModel(ItemModelIds::IRON_INGOT),
                  CountItemByModel(ItemModelIds::DUST),
                  CountItemByModel(ItemModelIds::FEATHERS),
                  CountItemByModel(ItemModelIds::BONES));

        uint32_t craftedTotal = 0u;
        for (const auto& spec : kConsetSpecs) {
            craftedTotal += CraftConsetAtNpc(spec, balancedSets);
        }
        if (craftedTotal == 0u) {
            Log::Warn("MaintenanceMgr: Pressure conset balanced bundle made no progress");
            return false;
        }

        OpenXunlaiChest(kEmbarkXunlaiX, kEmbarkXunlaiY);
        DepositExcessConsetsToStorage(cfg.targetCharacterConsetsEach);
        if (ItemMgr::GetGoldCharacter() > cfg.depositKeepOnChar) {
            DepositGold(cfg.depositKeepOnChar);
        }
        return true;
    }

    struct PressureCandidate {
        const ConsetCraftSpec* spec = nullptr;
        uint32_t craftableSets = 0u;
        uint32_t haveFirst = 0u;
        uint32_t haveSecond = 0u;
    } best;

    const uint32_t requestedFallbackSets = cfg.consetBatchSets > 0u ? cfg.consetBatchSets : 1u;
    for (const auto& spec : kConsetSpecs) {
        const uint32_t haveFirst = CountItemByModel(spec.materials[0]);
        const uint32_t haveSecond = CountItemByModel(spec.materials[1]);
        const uint32_t craftableSets = ComputeAvailableSetsForSpec(spec, requestedFallbackSets);
        if (craftableSets == 0u) continue;

        if (!best.spec || craftableSets > best.craftableSets) {
            best.spec = &spec;
            best.craftableSets = craftableSets;
            best.haveFirst = haveFirst;
            best.haveSecond = haveSecond;
        }
    }

    if (!best.spec || best.craftableSets == 0u) {
        Log::Warn("MaintenanceMgr: Pressure conset crafting found no usable on-hand recipe");
        return false;
    }

    Log::Info("MaintenanceMgr: Pressure conset crafting selected %s on-hand sets=%u "
              "mats %u=%u %u=%u",
              best.spec->label,
              best.craftableSets,
              best.spec->materials[0],
              best.haveFirst,
              best.spec->materials[1],
              best.haveSecond);

    const uint32_t crafted = CraftConsetAtNpc(*best.spec, best.craftableSets);
    if (crafted == 0u) {
        Log::Warn("MaintenanceMgr: Pressure conset crafting made no progress for %s", best.spec->label);
        return false;
    }

    OpenXunlaiChest(kEmbarkXunlaiX, kEmbarkXunlaiY);
    DepositExcessConsetsToStorage(cfg.targetCharacterConsetsEach);
    if (ItemMgr::GetGoldCharacter() > cfg.depositKeepOnChar) {
        DepositGold(cfg.depositKeepOnChar);
    }
    return true;
}

static bool ConvertInventoryCraftMaterialsToConsets(const Config& cfg) {
    if (!cfg.enableConsetRestock) return false;
    const uint32_t returnMapId = ResolveMaintenanceTown(cfg, MapMgr::GetMapId());

    uint32_t freeSlots = CountFreeSlots();
    uint32_t materialStacks = CountConsetCraftMaterialStacks();
    const bool severePressure = freeSlots < cfg.minFreeSlots;
    const bool triggerByStacks = ShouldCraftConsetsForMaterialSlots(cfg, materialStacks);
    const bool hasAnyConsetMats = HasAnyConsetCraftMaterialStacks();
    const bool triggerByCraftablePressure = severePressure && hasAnyConsetMats;
    if (!triggerByStacks && !triggerByCraftablePressure) {
        if (severePressure) {
            Log::Info("MaintenanceMgr: Severe inventory pressure but no conset conversion trigger freeSlots=%u minFree=%u consetMatStacks=%u craftable=%u",
                      freeSlots,
                      cfg.minFreeSlots,
                      materialStacks,
                      HasAnyCraftableConsetSet() ? 1u : 0u);
        }
        return false;
    }

    Log::Info("MaintenanceMgr: Inventory conset material slots detected freeSlots=%u pressureHint=%u consetMatStacks=%u slotTrigger=%u severePressure=%u craftable=%u anyMat=%u",
              freeSlots,
              cfg.consetMaterialPressureFreeSlots,
              materialStacks,
              cfg.consetMaterialStackTrigger,
              severePressure ? 1u : 0u,
              HasAnyCraftableConsetSet() ? 1u : 0u,
              hasAnyConsetMats ? 1u : 0u);

    bool converted = false;
    for (uint32_t pass = 0; pass < 4; ++pass) {
        freeSlots = CountFreeSlots();
        materialStacks = CountConsetCraftMaterialStacks();
        const bool passSeverePressure = freeSlots < cfg.minFreeSlots;
        const bool passTriggerByStacks = ShouldCraftConsetsForMaterialSlots(cfg, materialStacks);
        const bool passTriggerByCraftablePressure = passSeverePressure && materialStacks > 0u;
        if (!passTriggerByStacks && !passTriggerByCraftablePressure) {
            break;
        }
        const bool pressurePass = (passSeverePressure && materialStacks > 0u) || passTriggerByStacks;
        const bool passConverted = PressureCraftConsetsOnce(cfg);
        if (!passConverted) {
            break;
        }
        converted = true;
        if (pressurePass) {
            Log::Info("MaintenanceMgr: Inventory material conset crafting pass completed; checking remaining material slots");
        }
        WaitMs(1000);
    }

    if (returnMapId != 0u && MapMgr::GetMapId() != returnMapId) {
        if (!EnsureMap(returnMapId)) {
            Log::Warn("MaintenanceMgr: Failed to return to maintenance town %u after inventory conset conversion",
                      returnMapId);
            return converted;
        }
    }
    if (returnMapId != 0u &&
        !WaitForMaintenanceMapRuntimeReady(returnMapId, 10000u, "post inventory conset conversion return")) {
        Log::Warn("MaintenanceMgr: Skipping inventory conset conversion summary reads; maintenance town runtime is not stable");
        return converted;
    }

    Log::Info("MaintenanceMgr: Inventory material conset conversion complete converted=%d freeSlots=%u consetMatStacks=%u storedConsets=%u",
              converted ? 1 : 0,
              CountFreeSlots(),
              CountConsetCraftMaterialStacks(),
              CountTotalConsetsStored());
    return converted;
}

} // namespace

bool ConvertExcessStorageGoldToConsets(const Config& cfg) {
    if (!cfg.enableConsetRestock) return false;
    if (ItemMgr::GetGoldStorage() <= cfg.consetStorageGoldTrigger) return false;
    const uint32_t storedArmor = CountItemByModelInStorage(ItemModelIds::ARMOR_OF_SALVATION);
    const uint32_t storedEssence = CountItemByModelInStorage(ItemModelIds::ESSENCE_OF_CELERITY);
    const uint32_t storedGrail = CountItemByModelInStorage(ItemModelIds::GRAIL_OF_MIGHT);
    if (storedArmor >= cfg.targetStoredConsetsEach &&
        storedEssence >= cfg.targetStoredConsetsEach &&
        storedGrail >= cfg.targetStoredConsetsEach) {
        Log::Info("MaintenanceMgr: Skipping storage-gold conset conversion; stored consets already above target armor=%u essence=%u grail=%u target=%u",
                  storedArmor,
                  storedEssence,
                  storedGrail,
                  cfg.targetStoredConsetsEach);
        return false;
    }
    const uint32_t returnMapId = ResolveMaintenanceTown(cfg, MapMgr::GetMapId());

    Log::Info("MaintenanceMgr: Excess storage gold detected (%u > %u); converting to consets",
              ItemMgr::GetGoldStorage(), cfg.consetStorageGoldTrigger);

    bool converted = false;
    for (uint32_t pass = 0; pass < 6; ++pass) {
        const uint32_t storageGold = ItemMgr::GetGoldStorage();
        if (storageGold <= cfg.consetStorageGoldFloor) break;
        if (!RestockConsetsOnce(cfg)) break;
        converted = true;
        WaitMs(1000);
    }

    if (returnMapId != 0u && MapMgr::GetMapId() != returnMapId) {
        if (!EnsureMap(returnMapId)) {
            Log::Warn("MaintenanceMgr: Failed to return to maintenance town %u after storage-gold conset conversion",
                      returnMapId);
            return converted;
        }
    }
    if (returnMapId != 0u &&
        !WaitForMaintenanceMapRuntimeReady(returnMapId, 10000u, "post storage-gold conset conversion return")) {
        Log::Warn("MaintenanceMgr: Skipping final gold deposit after conset conversion; maintenance town runtime is not stable");
        return converted;
    }
    if (ItemMgr::GetGoldCharacter() >= cfg.maxCharacterGold ||
        ItemMgr::GetGoldCharacter() >= cfg.depositWhenCharacterGoldAtLeast) {
        DepositGold(cfg.depositKeepOnChar);
    }

    Log::Info("MaintenanceMgr: Conset conversion complete converted=%d storageGold=%u storedConsets=%u",
              converted ? 1 : 0, ItemMgr::GetGoldStorage(), CountTotalConsetsStored());
    return converted;
}

// ===== Full Maintenance =====

void PerformMaintenance(const Config& cfg) {
    Log::Info("MaintenanceMgr: Starting maintenance (freeSlots=%u superiorIdKits=%u regularSalvKits=%u highGradeSalvKits=%u totalSalvKits=%u targets=%u/%u/%u gold=%u/%u consetTrigger=%u floor=%u)",
              CountFreeSlots(),
              CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT),
              CountRegularSalvageKits(),
              CountHighGradeSalvageKits(),
              CountAllSalvageKits(),
              cfg.targetIdKits, cfg.targetSalvageKits, cfg.targetExpertSalvageKits,
              ItemMgr::GetGoldCharacter(), ItemMgr::GetGoldStorage(),
              cfg.consetStorageGoldTrigger, cfg.consetStorageGoldFloor);

    // Step 1: Deposit excess gold
    uint32_t charGold = ItemMgr::GetGoldCharacter();
    if (charGold >= cfg.depositWhenCharacterGoldAtLeast) {
        DepositGold(cfg.depositKeepOnChar);
    }

    // Step 2: Identify unidentified items (needed before sell/salvage decisions)
    uint32_t identified = IdentifyAllItems();
    if (identified > 0) WaitMs(500);

    // Step 3: Sell vendor-safe junk while the merchant context is definitely
    // open. Rare items, kits, and consets are filtered out by ShouldSellItem().
    uint32_t sold = SellJunkItems();
    if (sold > 0) WaitMs(500);

    // Step 4: Salvage is disabled.
    // The Salvage() function frees and reallocates the inventory bags array.
    // The old bags pointer at p2+0xF8 is NULLed because the memory is freed.
    // The game rebuilds it via StoC response, but that response never arrives
    // because our PacketSend dispatch path doesn't trigger the correct
    // server-side processing. Requires AutoIt SafeEnqueue ().

    // Step 5: Buy kits to target while the merchant is still open.
    BuyKitsToTarget(cfg);

    // Step 6: Sell common materials that are worth more at the material trader.
    uint32_t soldScalePacks = SellScalesToMaterialTrader(cfg);
    if (soldScalePacks > 0) WaitMs(500);

    // Step 6b: If bags are nearly full of conset mats, convert those stacks
    // into stored consets before falling back to chest deposits.
    ConvertInventoryCraftMaterialsToConsets(cfg);

    // Step 6c: If carried conset stacks are imbalanced or below the configured
    // character floor, craft the missing consumable type(s). Do not pull stored
    // consets just to make carried stacks look even.
    CraftCharacterConsetRebalance(cfg);
    CraftCharacterConsetRestock(cfg);

    // Step 7: If space is still tight, move to Xunlai and deposit storage-safe
    // items. This runs after merchant work because walking to the chest closes
    // the merchant context.
    bool hasMaterials = false;
    bool hasLooseConsets = false;
    bool hasEventStorageItems = false;
    bool hasGeneralStorageItems = false;
    {
        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (uint32_t b = 1; b <= 4 && !(hasMaterials && hasLooseConsets && hasEventStorageItems && hasGeneralStorageItems); b++) {
                Bag* bag = inv->bags[b];
                if (!bag || !bag->items.buffer) continue;
                for (uint32_t s = 0; s < bag->items.size; s++) {
                    Item* item = bag->items.buffer[s];
                    if (!item) continue;
                    if (!hasMaterials &&
                        item->type == 11 &&
                        GetMaterialStorageSlot(item->model_id) >= 0) {
                        hasMaterials = true;
                    }
                    if (!hasLooseConsets &&
                        (item->model_id == ItemModelIds::GRAIL_OF_MIGHT ||
                         item->model_id == ItemModelIds::ESSENCE_OF_CELERITY ||
                         item->model_id == ItemModelIds::ARMOR_OF_SALVATION)) {
                        hasLooseConsets = true;
                    }
                    if (!hasEventStorageItems && IsEventStorageItem(item->model_id)) {
                        hasEventStorageItems = true;
                    }
                    if (!hasGeneralStorageItems && IsGeneralXunlaiStorageItem(item->model_id)) {
                        hasGeneralStorageItems = true;
                    }
                    if (hasMaterials && hasLooseConsets && hasEventStorageItems && hasGeneralStorageItems) break;
                }
            }
        }
    }

    uint32_t currentFreeSlots = CountFreeSlots();
    const bool shouldDepositToStorage =
        (currentFreeSlots < cfg.minFreeSlots &&
         (hasMaterials || hasLooseConsets || hasEventStorageItems || hasGeneralStorageItems)) ||
        hasEventStorageItems ||
        hasGeneralStorageItems;
    const bool hasConfiguredXunlai = cfg.xunlaiChestX != 0.0f || cfg.xunlaiChestY != 0.0f;
    if (shouldDepositToStorage && hasConfiguredXunlai) {
        OpenXunlaiChest(cfg.xunlaiChestX, cfg.xunlaiChestY);

        uint32_t deposited = 0;
        if (hasMaterials) {
            deposited += DepositMaterialsToStorage();
        }
        if (hasLooseConsets) {
            deposited += DepositExcessConsetsToStorage(cfg.targetCharacterConsetsEach);
        }
        if (hasGeneralStorageItems) {
            deposited += DepositGeneralStorageItemsToXunlai();
        }
        if (deposited > 0) {
            WaitMs(1000 + deposited * 200);
            Log::Info("MaintenanceMgr: After Xunlai maintenance: freeSlots=%u consets=%u/%u/%u",
                      CountFreeSlots(),
                      CountItemByModel(ItemModelIds::GRAIL_OF_MIGHT),
                      CountItemByModel(ItemModelIds::ESSENCE_OF_CELERITY),
                      CountItemByModel(ItemModelIds::ARMOR_OF_SALVATION));
        }

        for (int retry = 0; retry < 10; retry++) {
            auto* me = AgentMgr::GetMyAgent();
            if (me && me->x != 0.0f && me->y != 0.0f) {
                Log::Info("MaintenanceMgr: Agent valid after Xunlai (pos=%.0f, %.0f)", me->x, me->y);
                break;
            }
            Log::Warn("MaintenanceMgr: Agent invalid after Xunlai, waiting...");
            WaitMs(500);
        }
    } else if (shouldDepositToStorage) {
        Log::Warn("MaintenanceMgr: Skipping storage deposit; no maintenance-town Xunlai coordinates configured");
    }

    // Step 8: Final gold deposit
    charGold = ItemMgr::GetGoldCharacter();
    if (charGold > cfg.depositKeepOnChar) {
        DepositGold(cfg.depositKeepOnChar);
    }

    // Step 9: Convert excess Xunlai gold into stored consets.
    ConvertExcessStorageGoldToConsets(cfg);

    Log::Info("MaintenanceMgr: Maintenance complete (freeSlots=%u superiorIdKits=%u regularSalvKits=%u highGradeSalvKits=%u totalSalvKits=%u targets=%u/%u/%u gold=%u/%u storedConsets=%u/%u/%u)",
              CountFreeSlots(),
              CountItemByModel(ItemModelIds::SUPERIOR_IDENTIFICATION_KIT),
              CountRegularSalvageKits(),
              CountHighGradeSalvageKits(),
              CountAllSalvageKits(),
              cfg.targetIdKits, cfg.targetSalvageKits, cfg.targetExpertSalvageKits,
              ItemMgr::GetGoldCharacter(), ItemMgr::GetGoldStorage(),
              CountItemByModelInStorage(ItemModelIds::GRAIL_OF_MIGHT),
              CountItemByModelInStorage(ItemModelIds::ESSENCE_OF_CELERITY),
              CountItemByModelInStorage(ItemModelIds::ARMOR_OF_SALVATION));
}

} // namespace GWA3::MaintenanceMgr
