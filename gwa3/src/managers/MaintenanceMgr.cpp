// MaintenanceMgr — inventory maintenance between dungeon runs.
// Mirrors AutoIt Utils-Maintenance.au3 PerformMaintenance() flow.

#include <gwa3/managers/MaintenanceMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>
#include <gwa3/game/Item.h>

#include <Windows.h>

namespace GWA3::MaintenanceMgr {

// ===== Known Item Model IDs =====
static constexpr uint32_t MODEL_CHEAP_SALVAGE_KIT = 2992;
static constexpr uint32_t MODEL_SALVAGE_KIT       = 5900;
static constexpr uint32_t MODEL_CHEAP_ID_KIT      = 2989;
static constexpr uint32_t MODEL_SUP_ID_KIT        = 5899;
static constexpr uint32_t MODEL_ALT_ID_KIT        = 235;
static constexpr uint32_t MODEL_ALT_SALVAGE_KIT   = 243;
static constexpr uint32_t MODEL_EXPERT_SALVAGE_KIT = 2991;
static constexpr uint32_t MODEL_RARE_SALVAGE_KIT   = 2993;
static constexpr uint32_t MODEL_ARMOR_OF_SALVATION = 24860;
static constexpr uint32_t MODEL_ESSENCE_OF_CELERITY = 24859;
static constexpr uint32_t MODEL_GRAIL_OF_MIGHT = 24861;
static constexpr uint32_t MODEL_BIRTHDAY_CUPCAKE = 22269;
static constexpr uint32_t MODEL_SLICE_BIRTHDAY = 28436;
static constexpr uint32_t MODEL_CANDY_CORN = 28431;
static constexpr uint32_t MODEL_VICTORY_TOKEN = 18345;
static constexpr uint32_t MODEL_BOGROOT_STAFF = 26983;
static constexpr uint32_t MODEL_BOGROOT_FOCUS = 26984;

static constexpr uint32_t MAP_EMBARK_BEACH = 857;
static constexpr uint32_t MAP_GADDS = 638;
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
static constexpr float kGaddsXunlaiX = -10481.0f;
static constexpr float kGaddsXunlaiY = -22787.0f;
static constexpr float kGaddsMaterialTraderX = -9097.0f;
static constexpr float kGaddsMaterialTraderY = -23353.0f;
static constexpr uint16_t kGaddsMaterialTraderNpcId = 6763;
static constexpr uint16_t kEmbarkMaterialTraderNpcId = 3285;

// ===== Rare Skin Detection (GWA3-176) =====
// Ported from AutoIt RareSkins.au3 — ~200 model IDs that should never be sold or salvaged.
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
    return modelId == MODEL_SALVAGE_KIT ||
           modelId == MODEL_CHEAP_SALVAGE_KIT ||
           modelId == MODEL_EXPERT_SALVAGE_KIT ||
           modelId == MODEL_RARE_SALVAGE_KIT ||
           modelId == MODEL_SUP_ID_KIT ||
           modelId == MODEL_ALT_ID_KIT ||
           modelId == MODEL_ALT_SALVAGE_KIT ||
           modelId == MODEL_CHEAP_ID_KIT;
}

// Basic material model IDs — from AutoIt GWA2_ID.au3
static constexpr uint32_t MAT_BONE              = 921;
static constexpr uint32_t MAT_IRON_INGOT        = 948;
static constexpr uint32_t MAT_TANNED_HIDE       = 940;
static constexpr uint32_t MAT_SCALE             = 953;
static constexpr uint32_t MAT_CHITIN            = 954;
static constexpr uint32_t MAT_BOLT_OF_CLOTH     = 925;
static constexpr uint32_t MAT_WOOD_PLANK        = 946;
static constexpr uint32_t MAT_GRANITE_SLAB      = 955;
static constexpr uint32_t MAT_GLITTERING_DUST   = 929;
static constexpr uint32_t MAT_PLANT_FIBER       = 934;
static constexpr uint32_t MAT_FEATHER           = 933;

// Materials to KEEP for conset crafting (AutoIt $KEEP_MATERIALS)
static bool IsKeepMaterial(uint32_t modelId) {
    return modelId == MAT_IRON_INGOT ||
           modelId == MAT_GLITTERING_DUST ||
           modelId == MAT_BONE ||
           modelId == MAT_FEATHER ||
           modelId == MAT_GRANITE_SLAB ||
           modelId == MAT_PLANT_FIBER;
}

// Materials to always SELL (AutoIt $SELL_MATERIALS)
static bool IsSellMaterial(uint32_t modelId) {
    return modelId == MAT_BOLT_OF_CLOTH ||
           modelId == MAT_TANNED_HIDE ||
           modelId == MAT_WOOD_PLANK ||
           modelId == MAT_CHITIN;
}

static bool IsEventStorageItem(uint32_t modelId) {
    return modelId == MODEL_BIRTHDAY_CUPCAKE ||
           modelId == MODEL_SLICE_BIRTHDAY ||
           modelId == MODEL_CANDY_CORN ||
           modelId == MODEL_VICTORY_TOKEN;
}

static bool IsForcedVendorSellModel(uint32_t modelId) {
    return modelId == MODEL_BOGROOT_STAFF ||
           modelId == MODEL_BOGROOT_FOCUS;
}

// Check if an item is a basic material (type 11 = materials in GW)
static bool IsBasicMaterial(Item* item) {
    if (!item) return false;
    return item->type == 11; // GW material type
}

// ===== Helpers =====

static void WaitMs(uint32_t ms) { Sleep(ms); }

static bool WaitForMapReady(uint32_t mapId, uint32_t timeoutMs) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MapMgr::GetMapId() == mapId && MapMgr::GetLoadingState() == 1 && AgentMgr::GetMyId() > 0) {
            auto* me = AgentMgr::GetMyAgent();
            if (me && me->x != 0.0f && me->y != 0.0f) {
                return true;
            }
        }
        WaitMs(250);
    }
    return false;
}

static bool MoveNear(float x, float y, float tolerance, uint32_t timeoutMs, const char* label) {
    const DWORD start = GetTickCount();
    DWORD lastMove = 0;
    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = AgentMgr::GetMyAgent();
        if (me) {
            const float dist = AgentMgr::GetDistance(me->x, me->y, x, y);
            if (dist <= tolerance) {
                return true;
            }
            const DWORD now = GetTickCount();
            if (lastMove == 0 || (now - lastMove) >= 1500) {
                AgentMgr::Move(x, y);
                lastMove = now;
            }
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
        if (!requireMerchantItems || TradeMgr::GetMerchantItemCount() > 0) {
            Log::Info("MaintenanceMgr: Opened %s NPC=%u attempt=%d items=%u",
                      label ? label : "NPC", npcId, attempt, TradeMgr::GetMerchantItemCount());
            return true;
        }
        if (attempt == 2) {
            AgentMgr::InteractNPC(npcId);
            WaitMs(2000);
            if (!requireMerchantItems || TradeMgr::GetMerchantItemCount() > 0) {
                Log::Info("MaintenanceMgr: Opened %s NPC=%u via native interact items=%u",
                          label ? label : "NPC", npcId, TradeMgr::GetMerchantItemCount());
                return true;
            }
        }
        WaitMs(500);
    }
    Log::Warn("MaintenanceMgr: Failed to open %s NPC=%u", label ? label : "NPC", npcId);
    return false;
}

static bool HasOpenMaterialTraderInventory() {
    if (TradeMgr::GetMerchantItemCount() == 0) return false;
    static const uint32_t kTraderMaterialModels[] = {
        MAT_BONE,
        MAT_IRON_INGOT,
        MAT_TANNED_HIDE,
        MAT_SCALE,
        MAT_CHITIN,
        MAT_BOLT_OF_CLOTH,
        MAT_WOOD_PLANK,
        MAT_GRANITE_SLAB,
        MAT_GLITTERING_DUST,
        MAT_PLANT_FIBER,
        MAT_FEATHER
    };
    for (uint32_t modelId : kTraderMaterialModels) {
        if (TradeMgr::GetMerchantItemIdByModelId(modelId) != 0) {
            return true;
        }
    }
    return false;
}

static bool OpenGaddsMaterialTrader(uint32_t& traderNpc) {
    if (HasOpenMaterialTraderInventory()) return true;
    if (!MoveNear(kGaddsMaterialTraderX, kGaddsMaterialTraderY, 250.0f, 12000, "Gadd's Material Trader")) {
        return false;
    }
    traderNpc = FindNpcByPlayerNumberNearCoords(kGaddsMaterialTraderNpcId,
                                                kGaddsMaterialTraderX,
                                                kGaddsMaterialTraderY,
                                                1600.0f);
    if (traderNpc == 0) {
        traderNpc = FindNearestNpcByAllegiance(kGaddsMaterialTraderX, kGaddsMaterialTraderY, 1400.0f);
    }
    if (traderNpc == 0) return false;
    for (uint32_t attempt = 1; attempt <= 3; ++attempt) {
        if (OpenNpcDialog(traderNpc, "Gadd's Material Trader", true) && HasOpenMaterialTraderInventory()) {
            return true;
        }
        Log::Warn("MaintenanceMgr: Gadd's material trader attempt=%u opened non-trader inventory items=%u",
                  attempt, TradeMgr::GetMerchantItemCount());
        AgentMgr::CancelAction();
        WaitMs(400);
    }
    return false;
}

static uint32_t SellScalesToMaterialTrader() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t scalePacks = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (!item || item->model_id != MAT_SCALE || item->item_id == 0) continue;
            const uint32_t packs = item->quantity / 10u;
            if (packs == 0) continue;

            uint32_t traderNpc = 0;
            if (!OpenGaddsMaterialTrader(traderNpc)) {
                Log::Warn("MaintenanceMgr: Failed to open Gadd's material trader for scales");
                return scalePacks;
            }

            Log::Info("MaintenanceMgr: Selling scales to material trader item=%u qty=%u packs=%u",
                      item->item_id, item->quantity, packs);
            if (!TradeMgr::SellMaterialsToTrader(item->item_id, packs)) {
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

uint32_t CountFreeSlots() {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t freeSlots = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            if (!bag->items.buffer[i]) freeSlots++;
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

static uint32_t CountAllSalvageKits() {
    return CountItemByModel(MODEL_SALVAGE_KIT) +
           CountItemByModel(MODEL_CHEAP_SALVAGE_KIT) +
           CountItemByModel(MODEL_EXPERT_SALVAGE_KIT) +
           CountItemByModel(MODEL_RARE_SALVAGE_KIT) +
           CountItemByModel(MODEL_ALT_SALVAGE_KIT);
}

static bool IsSalvageKitModel(uint32_t modelId) {
    return modelId == MODEL_SALVAGE_KIT ||
           modelId == MODEL_CHEAP_SALVAGE_KIT ||
           modelId == MODEL_EXPERT_SALVAGE_KIT ||
           modelId == MODEL_RARE_SALVAGE_KIT ||
           modelId == MODEL_ALT_SALVAGE_KIT;
}

static uint32_t CountAllIdKits() {
    return CountItemByModel(MODEL_SUP_ID_KIT) +
           CountItemByModel(MODEL_ALT_ID_KIT) +
           CountItemByModel(MODEL_CHEAP_ID_KIT);
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
                case MAT_IRON_INGOT:
                case MAT_GLITTERING_DUST:
                case MAT_FEATHER:
                case MAT_BONE:
                    ++stacks;
                    break;
                default:
                    break;
            }
        }
    }
    return stacks;
}

// ===== Diagnostics =====

bool NeedsMaintenance(const Config& cfg) {
    uint32_t freeSlots = CountFreeSlots();
    if (freeSlots < cfg.minFreeSlots) {
        Log::Info("MaintenanceMgr: Needs maintenance — freeSlots=%u < %u", freeSlots, cfg.minFreeSlots);
        return true;
    }

    uint32_t idKits = CountItemByModel(MODEL_SUP_ID_KIT);
    if (idKits < cfg.targetIdKits) {
        Log::Info("MaintenanceMgr: Needs maintenance — superiorIdKits=%u < target %u", idKits, cfg.targetIdKits);
        return true;
    }

    uint32_t salvKits = CountAllSalvageKits();
    if (salvKits < cfg.targetSalvageKits) {
        Log::Info("MaintenanceMgr: Needs maintenance — salvageKits=%u < target %u", salvKits, cfg.targetSalvageKits);
        return true;
    }

    uint32_t charGold = ItemMgr::GetGoldCharacter();
    if (charGold >= cfg.maxCharacterGold) {
        Log::Info("MaintenanceMgr: Needs maintenance — gold=%u >= %u", charGold, cfg.maxCharacterGold);
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

    const uint32_t consetMatStacks = CountInventoryConsetCraftMaterialStacks();
    if (cfg.enableConsetRestock &&
        consetMatStacks > cfg.consetMaterialStackTrigger &&
        freeSlots <= cfg.consetMaterialPressureFreeSlots) {
        Log::Info("MaintenanceMgr: Needs maintenance - conset mat stacks=%u > %u with freeSlots=%u <= %u",
                  consetMatStacks, cfg.consetMaterialStackTrigger,
                  freeSlots, cfg.consetMaterialPressureFreeSlots);
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

    // Use raw GoNPC packet (0x39) like AutoIt — matches the proven merchant interaction.
    // AgentMgr::InteractNPC uses the native function which opens a UI dialog that
    // disrupts player agent state (causes position reads to return 0,0).
    Log::Info("MaintenanceMgr: Opening Xunlai chest via GoNPC (agent=%u)", chestId);
    AgentMgr::ChangeTarget(chestId);
    WaitMs(250);
    CtoS::SendPacket(3, Packets::INTERACT_NPC, chestId, 0u);
    WaitMs(2000);

    // Close the storage dialog — we only need the chest "activated" for MoveItem packets.
    // The dialog itself blocks agent reads if left open.
    AgentMgr::CancelAction();
    WaitMs(500);
}

// ===== Material Storage Deposit =====

// Material storage slot mapping (bag 6) — from AutoIt MATERIALS_DOUBLE_ARRAY
// Maps model_id → slot index in material storage bag.
static int GetMaterialStorageSlot(uint32_t modelId) {
    switch (modelId) {
        case MAT_BONE:            return 1;
        case MAT_IRON_INGOT:      return 2;
        case MAT_TANNED_HIDE:     return 3;
        case MAT_SCALE:           return 4;
        case MAT_CHITIN:          return 5;
        case MAT_BOLT_OF_CLOTH:   return 6;
        case MAT_WOOD_PLANK:      return 7;
        // slot 8 = not used for basic materials
        case MAT_GRANITE_SLAB:    return 9;
        case MAT_GLITTERING_DUST: return 10;
        case MAT_PLANT_FIBER:     return 11;
        case MAT_FEATHER:         return 12;
        default: return -1; // not a basic material with known slot
    }
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

            // Only deposit basic materials (type 11 = material)
            if (item->type != 11) continue;

            int slot = GetMaterialStorageSlot(item->model_id);
            if (slot < 0) continue; // not a known basic material

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

// ===== Item Classification (matches AutoIt GWA2_ID_Items.au3) =====

// Rarity constants from name_enc first ushort (AutoIt GWA2_ID_Items.au3)
static constexpr uint16_t RARITY_WHITE  = 2621;
static constexpr uint16_t RARITY_GRAY   = 2622;
static constexpr uint16_t RARITY_BLUE   = 2623;
static constexpr uint16_t RARITY_GOLD   = 2624;
static constexpr uint16_t RARITY_PURPLE = 2626;
static constexpr uint16_t RARITY_GREEN  = 2627;

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
    // AutoIt: MemoryRead(GetItemPtr($aItem) + 56, "ptr") → name string → ushort
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

// IsArmor: checks if item type is an armor type
static bool IsArmor(Item* item) {
    if (!item) return false;
    switch (item->type) {
        case TYPE_FOOT: case TYPE_CHEST: case TYPE_HAND: case TYPE_HEAD: case TYPE_LEG:
            return true;
        default: return false;
    }
}

// ===== Sell Items =====

// ShouldSellItemForMaintenance — mirrors AutoIt Utils-Maintenance.au3 line 750
// Sells: identified weapons of white/blue/purple/gold rarity (not rare skins)
//        + materials in the SELL list
// Keeps: kits, unidentified items, green/red items, rare skins
bool ShouldSellItem(const Item* item) {
    if (!item || item->item_id == 0 || item->model_id == 0) return false;
    if (item->equipped || item->customized) return false;

    const uint16_t rarity = GetRarity(const_cast<Item*>(item));
    if (rarity == RARITY_GREEN) return false;
    if (IsEventStorageItem(item->model_id)) return false;
    if (IsForcedVendorSellModel(item->model_id)) {
        if ((rarity == RARITY_GOLD || rarity == RARITY_PURPLE) &&
            !IsIdentified(const_cast<Item*>(item))) {
            return false;
        }
        return true;
    }

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

    // Preserve low-requirement or perfect-stat collector items that Froggy keeps.
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

    uint32_t merchantItems = TradeMgr::GetMerchantItemCount();
    if (merchantItems == 0) {
        Log::Warn("MaintenanceMgr: SellJunkItems — merchant not open");
        return 0;
    }

    uint32_t soldCount = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size; i++) {
            Item* item = bag->items.buffer[i];
            if (!item || item->model_id == 0) continue;
            if (item->value == 0) continue;
            if (!ShouldSellItem(item)) continue;

            uint32_t qty = (item->quantity > 0) ? item->quantity : 1;
            Log::Info("MaintenanceMgr: Selling item=%u model=%u value=%u qty=%u type=%u rarity=%u",
                      item->item_id, item->model_id, item->value, qty, item->type, GetRarity(item));
            TradeMgr::SellInventoryItem(item->item_id, qty);
            WaitMs(300);
            soldCount++;
        }
    }
    Log::Info("MaintenanceMgr: Sold %u items", soldCount);
    return soldCount;
}

// ===== Item Identification (GWA3-178) =====
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
                if (item->model_id == MODEL_SUP_ID_KIT || item->model_id == MODEL_CHEAP_ID_KIT ||
                    item->model_id == MODEL_ALT_ID_KIT) return item;
            }
        }
    }
    Item* item = ItemMgr::FindItemByModelId(MODEL_SUP_ID_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(MODEL_CHEAP_ID_KIT);
    if (item) return item;
    return ItemMgr::FindItemByModelId(MODEL_ALT_ID_KIT);
}

static Item* FindSalvageKit() {
    // Match the GWA2 default salvage flow: prefer basic kits first.
    Item* item = ItemMgr::FindItemByModelId(MODEL_CHEAP_SALVAGE_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(MODEL_RARE_SALVAGE_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(MODEL_ALT_SALVAGE_KIT);
    if (item) return item;
    item = ItemMgr::FindItemByModelId(MODEL_EXPERT_SALVAGE_KIT);
    if (item) return item;
    return ItemMgr::FindItemByModelId(MODEL_SALVAGE_KIT);
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
            // Skip rare skins — don't identify, preserves value
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

// ===== Salvage (GWA3-179) =====
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

static bool CallNativeZeroArgFunction(uintptr_t fn, const char* label) {
    if (!fn) {
        Log::Warn("MaintenanceMgr: Native call skipped for %s -- fn=0x00000000",
                  label ? label : "native-call");
        return false;
    }

    __try {
        __asm {
            call fn
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

static uint32_t CountInventoryBagHits(uintptr_t invStruct) {
    if (invStruct <= 0x10000) return 0u;
    uint32_t hits = 0u;
    __try {
        for (uint32_t idx = 1; idx <= 4; ++idx) {
            uintptr_t bagPtr = *reinterpret_cast<uintptr_t*>(invStruct + 4u * idx);
            if (bagPtr <= 0x10000) continue;
            auto* bag = reinterpret_cast<Bag*>(bagPtr);
            const bool indexMatches = (bag->index == idx) || (bag->index + 1u == idx);
            if (!indexMatches) continue;
            if (bag->items.size > 512u) continue;
            ++hits;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0u;
    }
    return hits;
}

static void DumpInventoryRootSweep(const char* label) {
    __try {
        uintptr_t bp = Offsets::BasePointer;
        uintptr_t ctx = bp ? *reinterpret_cast<uintptr_t*>(bp) : 0;
        uintptr_t p1 = ctx ? *reinterpret_cast<uintptr_t*>(ctx + 0x18) : 0;
        if (p1 <= 0x10000) {
            Log::Info("MaintenanceMgr: %s sweep p1=0x00000000", label ? label : "inventory-root-sweep");
            return;
        }

        for (uintptr_t off = 0x20; off <= 0x60; off += 4) {
            uintptr_t root = *reinterpret_cast<uintptr_t*>(p1 + off);
            if (root <= 0x10000) continue;

            uintptr_t itemsBase = 0;
            uint32_t itemsSize = 0;
            uintptr_t bagsBase = 0;
            __try {
                itemsBase = *reinterpret_cast<uintptr_t*>(root + 0xB8);
                itemsSize = *reinterpret_cast<uint32_t*>(root + 0xC0);
                bagsBase = *reinterpret_cast<uintptr_t*>(root + 0xF8);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                continue;
            }

            const uint32_t bagHits = CountInventoryBagHits(bagsBase);
            if (itemsBase > 0x10000 || bagsBase > 0x10000 || bagHits > 0u || itemsSize != 0u) {
                Log::Info("MaintenanceMgr: %s sweep [+0x%X] root=0x%08X items=0x%08X size=%u bags=0x%08X bagHits=%u",
                          label ? label : "inventory-root-sweep",
                          static_cast<unsigned>(off),
                          static_cast<unsigned>(root),
                          static_cast<unsigned>(itemsBase),
                          itemsSize,
                          static_cast<unsigned>(bagsBase),
                          bagHits);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("MaintenanceMgr: %s sweep read failed", label ? label : "inventory-root-sweep");
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

static uint32_t ReadTrackedItemId(Item* item) {
    if (!item) return 0u;
    __try {
        return item->item_id;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0u;
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

static bool SendForegroundEnterInput() {
    HWND hwnd = static_cast<HWND>(MemoryMgr::GetGWWindowHandle());
    if (!hwnd || !IsWindow(hwnd)) {
        Log::Warn("MaintenanceMgr: SendForegroundEnterInput missing GW hwnd");
        return false;
    }

    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    Sleep(40);

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_RETURN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    const UINT sent = SendInput(2, inputs, sizeof(INPUT));
    Log::Info("MaintenanceMgr: SendForegroundEnterInput hwnd=0x%08X sent=%u",
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(hwnd)),
              sent);
    return sent == 2;
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
        constexpr uint32_t kLegacyFroggySalvageMaterials = 0x7Au;
        if (cfg.sendMaterialsViaBotshub) {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending SALVAGE_MATERIALS item=%u kit=%u hdr=0x%X via %s botshub queue",
                      itemId, kitId, kLegacyFroggySalvageMaterials, cfg.useHookedBotshubPackets ? "hooked" : "raw");
            const bool materialsQueued = cfg.useHookedBotshubPackets
                ? CtoS::SendPacketBotshubHooked(1, kLegacyFroggySalvageMaterials)
                : CtoS::SendPacketBotshub(1, kLegacyFroggySalvageMaterials);
            Log::Info("MaintenanceMgr: Legacy botshub salvage SALVAGE_MATERIALS queued=%u", materialsQueued ? 1u : 0u);
            CtoS::DumpBotshubQueueState("legacy-botshub-after-materials-enqueue");
            if (cfg.waitForMaterialsQueueIdle) {
                WaitForBotshubQueueIdle("legacy-botshub-salvage-materials");
            }
            WaitMs(250u);
            CtoS::DumpBotshubQueueState("legacy-botshub-250ms-after-materials");
        } else {
            Log::Info("MaintenanceMgr: Legacy botshub salvage sending SALVAGE_MATERIALS item=%u kit=%u hdr=0x%X via post-dispatch packet transport",
                      itemId, kitId, kLegacyFroggySalvageMaterials);
            SendLegacySalvagePacketThreaded(kLegacyFroggySalvageMaterials, "SALVAGE_MATERIALS");
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

    // AutoIt upstream (BotsHub lib/Utils.au3 SalvageItem and the Censured
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

    Log::Info("MaintenanceMgr: Native salvage stabilized item=%u stillPresent=%u bagsRestored=%u",
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
    // AutoIt Boss()->SalvageItems() identifies first, then salvages only
    // gold weapon-like items that are safe to liquidate.
    IdentifyAllItems();

    Item* kit = FindSalvageKit();
    if (!kit) {
        Log::Warn("MaintenanceMgr: No salvage kit found for gold salvage pass");
        return 0;
    }
    const uint32_t kitId = kit->item_id;
    const uint32_t batchSessionId = GetSalvageSessionId();
    if (!batchSessionId) {
        Log::Warn("MaintenanceMgr: No salvage session available for Froggy gold salvage");
        return 0;
    }

    struct GoldSalvageBatchEntry {
        uint32_t itemId;
        uint32_t modelId;
        uint8_t type;
        Item* trackedPtr;
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
                    Log::Info("MaintenanceMgr: Froggy salvage scan bag=%u slot=%u item=%u model=%u type=%u identified=%d isKit=%d isRareSkin=%d shouldSell=%d matSalv=%u",
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
                if (!ShouldSellItem(item)) continue;
                if (!weapon) continue;
                if (rarity != RARITY_GOLD) continue;
                if (item->is_material_salvageable == 0) continue;
                toSalvage[toSalvageCount++] = {item->item_id, item->model_id, item->type, item};
            }
        }
    }

    if (toSalvageCount == 0) {
        Log::Info("MaintenanceMgr: No gold items eligible for Froggy salvage");
        return 0;
    }

    Log::Info("MaintenanceMgr: Froggy post-run salvaging %u gold items session=%u", toSalvageCount, batchSessionId);

    // The legacy botshub queue stalls after the first salvage (tail never
    // advances), so items after the first aren't actually salvaged.
    // SalvageItemNative uses EnqueueGameCommand (a separate queue) for the
    // native salvage start and WaitForBagsPointerRestore to survive the
    // post-salvage inventory-root collapse correctly.
    uint32_t salvaged = 0;
    for (uint32_t i = 0; i < toSalvageCount; i++) {
        const GoldSalvageBatchEntry& entry = toSalvage[i];

        Log::Info("MaintenanceMgr: Froggy salvage [%u/%u] item=%u model=%u type=%u kit=%u (native path)",
                  i + 1, toSalvageCount, entry.itemId, entry.modelId, entry.type, kitId);

        if (SalvageItemNative(kitId, entry.itemId, false)) {
            salvaged++;
        } else {
            Log::Warn("MaintenanceMgr: Froggy native salvage failed item=%u kit=%u", entry.itemId, kitId);
        }
    }

    Log::Info("MaintenanceMgr: Froggy post-run salvaged %u gold items in batch session=%u", salvaged, batchSessionId);
    return salvaged;
}

// ===== Kit Management =====

static uint32_t SellExcessSalvageKits(uint32_t targetSalvageKits) {
    uint32_t currentSalvKits = CountAllSalvageKits();
    if (currentSalvKits <= targetSalvageKits) return 0;

    uint32_t merchantItems = TradeMgr::GetMerchantItemCount();
    if (merchantItems == 0) {
        Log::Warn("MaintenanceMgr: SellExcessSalvageKits - merchant not open");
        return 0;
    }

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    uint32_t soldCount = 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4 && currentSalvKits > targetSalvageKits; bagIdx++) {
        Bag* bag = inv->bags[bagIdx];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t i = 0; i < bag->items.size && currentSalvKits > targetSalvageKits; i++) {
            Item* item = bag->items.buffer[i];
            if (!item || item->item_id == 0 || !IsSalvageKitModel(item->model_id)) continue;

            const uint32_t qty = (item->quantity > 0) ? item->quantity : 1u;
            Log::Info("MaintenanceMgr: Selling excess salvage kit item=%u model=%u qty=%u count=%u target=%u",
                      item->item_id, item->model_id, qty, currentSalvKits, targetSalvageKits);
            TradeMgr::SellInventoryItem(item->item_id, qty);
            WaitMs(300);
            soldCount++;
            currentSalvKits = CountAllSalvageKits();
        }
    }

    Log::Info("MaintenanceMgr: Sold %u excess salvage kits; salvage count now %u/%u",
              soldCount, currentSalvKits, targetSalvageKits);
    return soldCount;
}

void BuyKitsToTarget(const Config& cfg) {
    uint32_t currentIdKits = CountItemByModel(MODEL_SUP_ID_KIT);
    uint32_t currentSalvKits = CountAllSalvageKits();

    // Check merchant is open
    uint32_t merchantItems = TradeMgr::GetMerchantItemCount();
    if (merchantItems == 0) {
        Log::Warn("MaintenanceMgr: BuyKitsToTarget — merchant not open");
        return;
    }

    if (currentSalvKits > cfg.targetSalvageKits) {
        SellExcessSalvageKits(cfg.targetSalvageKits);
        currentSalvKits = CountAllSalvageKits();
    }

    if (currentIdKits >= cfg.targetIdKits && currentSalvKits >= cfg.targetSalvageKits) {
        Log::Info("MaintenanceMgr: Kits sufficient (id=%u/%u salv=%u/%u)",
                  currentIdKits, cfg.targetIdKits, currentSalvKits, cfg.targetSalvageKits);
        return;
    }

    // Buy ID kits if needed
    if (currentIdKits < cfg.targetIdKits) {
        uint32_t need = cfg.targetIdKits - currentIdKits;
        // Froggy uses merchant slot 6 for superior ID kits at 500g each.
        bool bought = TradeMgr::BuyMerchantItemByPosition(6, need, 500);
        if (bought) {
            Log::Info("MaintenanceMgr: Bought %u superior ID kits", need);
            WaitMs(1000);
        } else {
            Log::Warn("MaintenanceMgr: Could not buy superior ID kits");
        }
    }

    // Buy salvage kits if needed
    if (currentSalvKits < cfg.targetSalvageKits) {
        uint32_t need = cfg.targetSalvageKits - currentSalvKits;
        // Froggy uses merchant slot 4 for salvage kits at 2000g each.
        bool bought = TradeMgr::BuyMerchantItemByPosition(4, need, 2000);
        if (bought) {
            Log::Info("MaintenanceMgr: Bought %u salvage kits", need);
            WaitMs(1000);
        } else {
            Log::Warn("MaintenanceMgr: Could not buy salvage kits");
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
};

static const ConsetCraftSpec kConsetSpecs[3] = {
    {"Grail of Might", kEmbarkEyjaX, kEmbarkEyjaY, MODEL_GRAIL_OF_MIGHT, {MAT_IRON_INGOT, MAT_GLITTERING_DUST}},
    {"Essence of Celerity", kEmbarkKwatX, kEmbarkKwatY, MODEL_ESSENCE_OF_CELERITY, {MAT_FEATHER, MAT_GLITTERING_DUST}},
    {"Armor of Salvation", kEmbarkAlcusX, kEmbarkAlcusY, MODEL_ARMOR_OF_SALVATION, {MAT_IRON_INGOT, MAT_BONE}},
};

static bool EnsureMap(uint32_t mapId) {
    if (MapMgr::GetMapId() == mapId && MapMgr::GetLoadingState() == 1) {
        return true;
    }
    MapMgr::Travel(mapId);
    return WaitForMapReady(mapId, 60000);
}

static uint32_t CountTotalConsetsStored() {
    return CountItemByModelInStorage(MODEL_GRAIL_OF_MIGHT)
        + CountItemByModelInStorage(MODEL_ESSENCE_OF_CELERITY)
        + CountItemByModelInStorage(MODEL_ARMOR_OF_SALVATION);
}

static bool OpenEmbarkNpc(float x, float y, const char* label, uint32_t& npcId) {
    npcId = 0;
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

    uint32_t crafted = 0;
    while (crafted < desiredSets) {
        const uint32_t remaining = desiredSets - crafted;
        const uint32_t batch = remaining > 5u ? 5u : remaining;
        const uint32_t before = CountItemByModel(spec.modelId);
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        const uint32_t qtys[2] = {50u, 50u};
        const uint32_t totalValue = 250u * batch;
        if (!TradeMgr::CraftMerchantItemByModelId(spec.modelId, batch, totalValue, spec.materials, qtys, 2)) {
            Log::Warn("MaintenanceMgr: CraftMerchantItemByModelId rejected for %s batch=%u", spec.label, batch);
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
            Log::Warn("MaintenanceMgr: Timed out waiting for %s craft completion", spec.label);
            break;
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
                  attempt, TradeMgr::GetMerchantItemCount());
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
        TradeMgr::BuyMaterials(modelId, remaining);
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
                      modelId, remaining, label ? label : "material", TradeMgr::GetMerchantItemCount());
        }

        WaitMs(400);
    }

    Log::Warn("MaintenanceMgr: Material buy incomplete model=%u requestedQty=%u remaining=%u label=%s",
              modelId, quantity, remaining, label ? label : "material");
    return false;
}

static uint32_t ComputeBalancedConsetSetsAvailable(uint32_t requestedSets) {
    const uint32_t iron = CountItemByModel(MAT_IRON_INGOT);
    const uint32_t dust = CountItemByModel(MAT_GLITTERING_DUST);
    const uint32_t feather = CountItemByModel(MAT_FEATHER);
    const uint32_t bones = CountItemByModel(MAT_BONE);

    uint32_t balanced = requestedSets;
    if ((iron / 100u) < balanced) balanced = iron / 100u;
    if ((dust / 100u) < balanced) balanced = dust / 100u;
    if ((feather / 50u) < balanced) balanced = feather / 50u;
    if ((bones / 50u) < balanced) balanced = bones / 50u;
    return balanced;
}

static uint32_t ComputeAvailableSetsForSpec(const ConsetCraftSpec& spec, uint32_t requestedSets) {
    if (requestedSets == 0) return 0;

    uint32_t available = requestedSets;
    for (uint32_t i = 0; i < 2; ++i) {
        const uint32_t have = CountItemByModel(spec.materials[i]);
        const uint32_t sets = have / 50u;
        if (sets < available) {
            available = sets;
        }
    }
    return available;
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
                case MAT_IRON_INGOT:
                case MAT_GLITTERING_DUST:
                case MAT_FEATHER:
                case MAT_BONE:
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
    const uint32_t iron = CountItemByModel(MAT_IRON_INGOT);
    const uint32_t dust = CountItemByModel(MAT_GLITTERING_DUST);
    const uint32_t feather = CountItemByModel(MAT_FEATHER);
    const uint32_t bones = CountItemByModel(MAT_BONE);
    return (iron >= 50u && dust >= 50u) ||
           (dust >= 50u && feather >= 50u) ||
           (iron >= 50u && bones >= 50u);
}

static bool RestockConsetsOnce(const Config& cfg) {
    if (!EnsureMap(MAP_EMBARK_BEACH)) {
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
    const uint32_t storedArmor = CountItemByModelInStorage(MODEL_ARMOR_OF_SALVATION);
    const uint32_t storedEssence = CountItemByModelInStorage(MODEL_ESSENCE_OF_CELERITY);
    const uint32_t storedGrail = CountItemByModelInStorage(MODEL_GRAIL_OF_MIGHT);
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
        {MAT_IRON_INGOT, 100u * setsToCraft, "Conset iron"},
        {MAT_GLITTERING_DUST, 100u * setsToCraft, "Conset dust"},
        {MAT_FEATHER, 50u * setsToCraft, "Conset feather"},
        {MAT_BONE, 50u * setsToCraft, "Conset bone"},
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
                  CountItemByModel(MAT_IRON_INGOT),
                  CountItemByModel(MAT_GLITTERING_DUST),
                  CountItemByModel(MAT_FEATHER),
                  CountItemByModel(MAT_BONE),
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
        MODEL_GRAIL_OF_MIGHT,
        MODEL_ESSENCE_OF_CELERITY,
        MODEL_ARMOR_OF_SALVATION,
    };
    DepositItemModelsToStorage(consetModels, _countof(consetModels));
    if (ItemMgr::GetGoldCharacter() > cfg.depositKeepOnChar) {
        DepositGold(cfg.depositKeepOnChar);
    }
    return true;
}

static bool PressureCraftConsetsOnce(const Config& cfg) {
    if (!EnsureMap(MAP_EMBARK_BEACH)) {
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

    struct PressureCandidate {
        const ConsetCraftSpec* spec = nullptr;
        uint32_t desiredSets = 0u;
        uint32_t missingTotal = 0u;
        uint32_t missingFirst = 0u;
        uint32_t missingSecond = 0u;
    } best;

    for (const auto& spec : kConsetSpecs) {
        const uint32_t haveFirst = CountItemByModel(spec.materials[0]);
        const uint32_t haveSecond = CountItemByModel(spec.materials[1]);
        uint32_t desiredSets = (haveFirst > haveSecond ? haveFirst : haveSecond) / 50u;
        if (desiredSets == 0u) continue;
        if (cfg.consetBatchSets > 0u && desiredSets > cfg.consetBatchSets) {
            desiredSets = cfg.consetBatchSets;
        }
        const uint32_t targetQty = desiredSets * 50u;
        const uint32_t missingFirst = targetQty > haveFirst ? (targetQty - haveFirst) : 0u;
        const uint32_t missingSecond = targetQty > haveSecond ? (targetQty - haveSecond) : 0u;
        const uint32_t missingTotal = missingFirst + missingSecond;

        if (!best.spec ||
            missingTotal < best.missingTotal ||
            (missingTotal == best.missingTotal && desiredSets > best.desiredSets)) {
            best.spec = &spec;
            best.desiredSets = desiredSets;
            best.missingTotal = missingTotal;
            best.missingFirst = missingFirst;
            best.missingSecond = missingSecond;
        }
    }

    if (!best.spec || best.desiredSets == 0u) {
        Log::Warn("MaintenanceMgr: Pressure conset crafting found no usable recipe");
        return false;
    }

    Log::Info("MaintenanceMgr: Pressure conset crafting selected %s desiredSets=%u missing=%u/%u",
              best.spec->label, best.desiredSets, best.missingFirst, best.missingSecond);

    uint32_t traderNpc = 0;
    if ((best.missingFirst > 0u || best.missingSecond > 0u) &&
        !EnsureEmbarkMaterialTraderOpen(traderNpc)) {
        Log::Warn("MaintenanceMgr: Pressure conset crafting could not open Embark material trader");
        return false;
    }

    if (best.missingFirst > 0u &&
        !BuyConsetMaterialVerified(best.spec->materials[0], best.missingFirst, best.spec->label, traderNpc)) {
        Log::Warn("MaintenanceMgr: Pressure conset crafting failed to top off first material for %s",
                  best.spec->label);
    }
    if (best.missingSecond > 0u &&
        !BuyConsetMaterialVerified(best.spec->materials[1], best.missingSecond, best.spec->label, traderNpc)) {
        Log::Warn("MaintenanceMgr: Pressure conset crafting failed to top off second material for %s",
                  best.spec->label);
    }

    const uint32_t craftableSets = ComputeAvailableSetsForSpec(*best.spec, best.desiredSets);
    if (craftableSets == 0u) {
        Log::Warn("MaintenanceMgr: Pressure conset crafting still has zero craftable sets for %s",
                  best.spec->label);
        return false;
    }

    const uint32_t crafted = CraftConsetAtNpc(*best.spec, craftableSets);
    if (crafted == 0u) {
        Log::Warn("MaintenanceMgr: Pressure conset crafting made no progress for %s", best.spec->label);
        return false;
    }

    OpenXunlaiChest(kEmbarkXunlaiX, kEmbarkXunlaiY);
    const uint32_t consetModels[3] = {
        MODEL_GRAIL_OF_MIGHT,
        MODEL_ESSENCE_OF_CELERITY,
        MODEL_ARMOR_OF_SALVATION,
    };
    DepositItemModelsToStorage(consetModels, _countof(consetModels));
    if (ItemMgr::GetGoldCharacter() > cfg.depositKeepOnChar) {
        DepositGold(cfg.depositKeepOnChar);
    }
    return true;
}

static bool ConvertInventoryCraftMaterialsToConsets(const Config& cfg) {
    if (!cfg.enableConsetRestock) return false;

    uint32_t freeSlots = CountFreeSlots();
    uint32_t materialStacks = CountConsetCraftMaterialStacks();
    const bool severePressure = freeSlots < cfg.minFreeSlots;
    const bool triggerByStacks = (freeSlots <= cfg.consetMaterialPressureFreeSlots &&
                                  materialStacks > cfg.consetMaterialStackTrigger);
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

    Log::Info("MaintenanceMgr: Inventory craft pressure detected freeSlots=%u threshold=%u consetMatStacks=%u trigger=%u severePressure=%u craftable=%u anyMat=%u",
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
        const bool passTriggerByStacks = (freeSlots <= cfg.consetMaterialPressureFreeSlots &&
                                          materialStacks > cfg.consetMaterialStackTrigger);
        const bool passTriggerByCraftablePressure = passSeverePressure && materialStacks > 0u;
        if (!passTriggerByStacks && !passTriggerByCraftablePressure) {
            break;
        }
        const bool pressurePass = passSeverePressure && materialStacks > 0u;
        const bool passConverted = pressurePass
            ? PressureCraftConsetsOnce(cfg)
            : RestockConsetsOnce(cfg);
        if (!passConverted) {
            break;
        }
        converted = true;
        if (pressurePass) {
            Log::Info("MaintenanceMgr: Pressure conset crafting pass completed; stopping after one pass to avoid oversized top-off loops");
            break;
        }
        WaitMs(1000);
    }

    if (MapMgr::GetMapId() != cfg.maintenanceTown) {
        EnsureMap(cfg.maintenanceTown);
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

    if (MapMgr::GetMapId() != cfg.maintenanceTown) {
        EnsureMap(cfg.maintenanceTown);
    }

    Log::Info("MaintenanceMgr: Conset conversion complete converted=%d storageGold=%u storedConsets=%u",
              converted ? 1 : 0, ItemMgr::GetGoldStorage(), CountTotalConsetsStored());
    return converted;
}

// ===== Full Maintenance =====

void PerformMaintenance(const Config& cfg) {
    Log::Info("MaintenanceMgr: Starting maintenance (freeSlots=%u superiorIdKits=%u salvKits=%u targets=%u/%u gold=%u/%u consetTrigger=%u floor=%u)",
              CountFreeSlots(), CountItemByModel(MODEL_SUP_ID_KIT), CountAllSalvageKits(),
              cfg.targetIdKits, cfg.targetSalvageKits,
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

    // Step 4: Salvage — DISABLED.
    // The Salvage() function frees and reallocates the inventory bags array.
    // The old bags pointer at p2+0xF8 is NULLed because the memory is freed.
    // The game rebuilds it via StoC response, but that response never arrives
    // because our PacketSend dispatch path doesn't trigger the correct
    // server-side processing. Requires AutoIt SafeEnqueue (GWA3-184/185).

    // Step 5: Buy kits to target while the merchant is still open.
    BuyKitsToTarget(cfg);

    // Step 6: Sell common materials that are worth more at the material trader.
    uint32_t soldScalePacks = SellScalesToMaterialTrader();
    if (soldScalePacks > 0) WaitMs(500);

    // Step 6b: If bags are nearly full of conset mats, convert those stacks
    // into stored consets before falling back to chest deposits.
    ConvertInventoryCraftMaterialsToConsets(cfg);

    // Step 7: If space is still tight, move to Xunlai and deposit storage-safe
    // items. This runs after merchant work because walking to the chest closes
    // the merchant context.
    static constexpr float kXunlaiX = -10481.0f;
    static constexpr float kXunlaiY = -22787.0f;

    bool hasMaterials = false;
    bool hasLooseConsets = false;
    bool hasEventStorageItems = false;
    {
        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (uint32_t b = 1; b <= 4 && !(hasMaterials && hasLooseConsets && hasEventStorageItems); b++) {
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
                        (item->model_id == MODEL_GRAIL_OF_MIGHT ||
                         item->model_id == MODEL_ESSENCE_OF_CELERITY ||
                         item->model_id == MODEL_ARMOR_OF_SALVATION)) {
                        hasLooseConsets = true;
                    }
                    if (!hasEventStorageItems && IsEventStorageItem(item->model_id)) {
                        hasEventStorageItems = true;
                    }
                    if (hasMaterials && hasLooseConsets && hasEventStorageItems) break;
                }
            }
        }
    }

    uint32_t currentFreeSlots = CountFreeSlots();
    if ((currentFreeSlots < cfg.minFreeSlots && (hasMaterials || hasLooseConsets || hasEventStorageItems)) ||
        hasEventStorageItems) {
        OpenXunlaiChest(kXunlaiX, kXunlaiY);

        uint32_t deposited = 0;
        if (hasMaterials) {
            deposited += DepositMaterialsToStorage();
        }
        if (hasLooseConsets) {
            const uint32_t consetModels[3] = {
                MODEL_GRAIL_OF_MIGHT,
                MODEL_ESSENCE_OF_CELERITY,
                MODEL_ARMOR_OF_SALVATION,
            };
            deposited += DepositItemModelsToStorage(consetModels, _countof(consetModels));
        }
        if (hasEventStorageItems) {
            const uint32_t eventModels[] = {
                MODEL_BIRTHDAY_CUPCAKE,
                MODEL_SLICE_BIRTHDAY,
                MODEL_CANDY_CORN,
                MODEL_VICTORY_TOKEN,
            };
            deposited += DepositItemModelsToStorage(eventModels, _countof(eventModels));
        }

        if (deposited > 0) {
            WaitMs(1000 + deposited * 200);
            Log::Info("MaintenanceMgr: After Xunlai deposit: freeSlots=%u", CountFreeSlots());
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
    }

    // Step 8: Final gold deposit
    charGold = ItemMgr::GetGoldCharacter();
    if (charGold > cfg.depositKeepOnChar) {
        DepositGold(cfg.depositKeepOnChar);
    }

    // Step 9: Convert excess Xunlai gold into stored consets.
    ConvertExcessStorageGoldToConsets(cfg);

    Log::Info("MaintenanceMgr: Maintenance complete (freeSlots=%u superiorIdKits=%u salvKits=%u targets=%u/%u gold=%u/%u storedConsets=%u/%u/%u)",
              CountFreeSlots(), CountItemByModel(MODEL_SUP_ID_KIT), CountAllSalvageKits(),
              cfg.targetIdKits, cfg.targetSalvageKits,
              ItemMgr::GetGoldCharacter(), ItemMgr::GetGoldStorage(),
              CountItemByModelInStorage(MODEL_GRAIL_OF_MIGHT),
              CountItemByModelInStorage(MODEL_ESSENCE_OF_CELERITY),
              CountItemByModelInStorage(MODEL_ARMOR_OF_SALVATION));
}

} // namespace GWA3::MaintenanceMgr
