#include <gwa3/managers/MerchantMgr.h>
static constexpr uint32_t kConsetTargetGold = 100000u;
static constexpr uint32_t kConsetMaterialBudget = 90000u;
static constexpr float kConsetMaterialTraderX = 2933.0f;
static constexpr float kConsetMaterialTraderY = -2236.0f;

struct ConsetMaterialCounts {
    uint32_t iron = 0;
    uint32_t dust = 0;
    uint32_t bone = 0;
    uint32_t feather = 0;
};

struct ConsetMaterialPrices {
    uint32_t iron = 0;
    uint32_t dust = 0;
    uint32_t bone = 0;
    uint32_t feather = 0;
};

struct ConsetMaterialStockState {
    bool ironOOS = false;
    bool dustOOS = false;
    bool boneOOS = false;
    bool featherOOS = false;
};

struct ConsetCraftPlan {
    uint32_t equalSets = 0;
    uint32_t grails = 0;
    uint32_t essences = 0;
    uint32_t armors = 0;
    ConsetMaterialCounts materials{};
    uint32_t gold = 0;
};

struct ConsetCraftResult {
    uint32_t grailsCrafted = 0;
    uint32_t essencesCrafted = 0;
    uint32_t armorsCrafted = 0;
    uint32_t completeSets = 0;
    uint32_t goldSpent = 0;
    uint32_t goldRemaining = 0;
};

static ConsetMaterialCounts ReadConsetMaterialCounts() {
    return {
        CountInventoryModelQuantity(kMaterialIronIngot),
        CountInventoryModelQuantity(kMaterialDust),
        CountInventoryModelQuantity(kMaterialBone),
        CountInventoryModelQuantity(kMaterialFeather),
    };
}

static bool EnsureConsetCycleEmbarkReady() {
    if (ReadMapId() != MapIds::EMBARK_BEACH) {
        CtoS::SuspendEngineHook();
        CtoS::MapTravel(MapIds::EMBARK_BEACH, 4u, 1u, 0u);
        Sleep(1000);
        CtoS::ResumeEngineHook();
        WaitFor("Embark Beach", 30000, []() {
            return ReadMapId() == MapIds::EMBARK_BEACH && MapMgr::GetLoadingState() == 0;
        });
    }
    if (!WaitForPlayerWorldReady(15000)) {
        IntReport("  Failed to reach Embark");
        return false;
    }

    const bool agentReady = WaitFor("player agent", 10000, []() {
        auto* me = AgentMgr::GetMyAgent();
        return me && me->x != 0.0f;
    });
    if (!agentReady) {
        IntReport("  Player agent not ready");
        return false;
    }

    auto* me = AgentMgr::GetMyAgent();
    IntReport("  In Embark Beach district=%u pos=(%.0f, %.0f)",
              MapMgr::GetDistrict(), me ? me->x : 0.0f, me ? me->y : 0.0f);
    return true;
}

static void EnsureConsetGoldBudget() {
    uint32_t gold = ItemMgr::GetGoldCharacter();
    uint32_t storageGold = ItemMgr::GetGoldStorage();
    IntReport("  Step 2: Gold - char=%u storage=%u target=%u", gold, storageGold, kConsetTargetGold);
    if (gold < kConsetTargetGold && storageGold > 0) {
        IntReport("  Withdrawing gold from Xunlai to reach %u...", kConsetTargetGold);
        if (ConsetMoveToNPC(kEmbarkXunlaiX, kEmbarkXunlaiY, "Xunlai Chest")) {
            uint32_t npc = ConsetFindNearestNPC(kEmbarkXunlaiX, kEmbarkXunlaiY);
            if (npc) {
                AgentMgr::ChangeTarget(npc);
                Sleep(250);
                CtoS::SendPacket(3, Packets::INTERACT_NPC, npc, 0u);
                Sleep(2000);
                uint32_t toWithdraw = kConsetTargetGold - gold;
                if (toWithdraw > storageGold) toWithdraw = storageGold;
                IntReport("  Withdrawing %u gold...", toWithdraw);
                ItemMgr::ChangeGold(gold + toWithdraw, storageGold - toWithdraw);
                Sleep(500);
                gold = ItemMgr::GetGoldCharacter();
                IntReport("  Gold after withdraw: char=%u storage=%u", gold, ItemMgr::GetGoldStorage());
            }
        }
    } else {
        IntReport("  Already have %u gold, skipping Xunlai", gold);
    }
}

static bool OpenConsetMaterialTrader() {
    if (!ConsetMoveToNPC(kConsetMaterialTraderX, kConsetMaterialTraderY, "Material Trader")) {
        IntReport("  Failed to reach material trader");
        return false;
    }

    uint32_t traderNpc = ConsetFindNearestNPC(kConsetMaterialTraderX, kConsetMaterialTraderY);
    if (!traderNpc || !ConsetOpenNPCDialog(traderNpc, "Material Trader")) {
        IntReport("  Failed to open material trader");
        return false;
    }

    IntReport("  Material trader open: %u items", MerchantMgr::GetMerchantItemCount());
    return true;
}

static uint32_t ReadConsetMaterialPrice(uint32_t modelId) {
    uint32_t itemId = FindTraderVirtualItemId(modelId);
    if (!itemId) return 0;

    TraderHook::Reset();
    TraderQuoteTask quoteTask{itemId};
    CtoS::EnqueueGameCommand(&TraderQuoteInvoker, &quoteTask, sizeof(quoteTask));
    WaitFor("price check", 3000, []() {
        return TraderHook::GetCostValue() > 0 && TraderHook::GetCostValue() < 100000;
    });
    return TraderHook::GetCostValue();
}

static bool ReadConsetMaterialPrices(ConsetMaterialPrices& prices) {
    IntReport("  Step 3: Opening material trader for price check...");
    if (!OpenConsetMaterialTrader()) return false;

    prices.iron = ReadConsetMaterialPrice(kMaterialIronIngot);
    prices.dust = ReadConsetMaterialPrice(kMaterialDust);
    prices.bone = ReadConsetMaterialPrice(kMaterialBone);
    prices.feather = ReadConsetMaterialPrice(kMaterialFeather);
    IntReport("  Prices: Iron=%u Dust=%u Bone=%u Feather=%u",
              prices.iron, prices.dust, prices.bone, prices.feather);

    if (!prices.iron || !prices.dust || !prices.bone || !prices.feather) {
        IntReport("  Failed to get all material prices");
        return false;
    }
    return true;
}

static uint32_t ChooseConsetCycleTargetCount(const ConsetMaterialPrices& prices) {
    const uint32_t costPerConset =
        10u * prices.iron + 10u * prices.dust + 5u * prices.bone + 5u * prices.feather + 750u;
    const uint32_t estimatedCostPerConset = costPerConset + costPerConset / 2;
    uint32_t numConsets = kConsetMaterialBudget / estimatedCostPerConset;
    if (numConsets < 1) numConsets = 1;
    if (numConsets > 25) numConsets = 25;
    IntReport("  Cost per conset (base): %u  Estimated (1.5x): %u  Budget: %u  Consets to craft: %u",
              costPerConset, estimatedCostPerConset, kConsetMaterialBudget, numConsets);
    return numConsets;
}

static ConsetMaterialCounts BuildConsetMaterialNeed(uint32_t numConsets) {
    return {
        numConsets * 100,
        numConsets * 100,
        numConsets * 50,
        numConsets * 50,
    };
}

static void ReopenConsetMaterialTraderIfClosed() {
    if (MerchantMgr::GetMerchantItemCount() != 0) return;

    IntReport("  Re-opening material trader for supplemental buying...");
    if (ConsetMoveToNPC(kConsetMaterialTraderX, kConsetMaterialTraderY, "Material Trader")) {
        uint32_t traderNpc = ConsetFindNearestNPC(kConsetMaterialTraderX, kConsetMaterialTraderY);
        if (traderNpc) ConsetOpenNPCDialog(traderNpc, "Material Trader");
    }
}

static void BuySupplementalConsetMaterials(const ConsetMaterialStockState& stockState) {
    constexpr uint32_t kMinGoldForBuy = 3000u;
    const uint32_t gold = ItemMgr::GetGoldCharacter();
    if (gold <= kMinGoldForBuy) return;

    ReopenConsetMaterialTraderIfClosed();
    const bool canBuyForEssence = !stockState.featherOOS && !stockState.dustOOS;
    const bool canBuyForGrail = !stockState.ironOOS && !stockState.dustOOS;
    const bool canBuyForArmor = !stockState.ironOOS && !stockState.boneOOS;

    const uint32_t extraBudget = gold - kMinGoldForBuy;
    uint32_t extraCrafts = extraBudget / 4000u;
    if (extraCrafts > 25) extraCrafts = 25;
    IntReport("  Extra budget: %u  Estimated extra crafts: %u (Grail=%s Essence=%s Armor=%s)",
              extraBudget, extraCrafts,
              canBuyForGrail ? "yes" : "OOS", canBuyForEssence ? "yes" : "OOS",
              canBuyForArmor ? "yes" : "OOS");

    const uint32_t extraPer = (extraCrafts > 0 && (canBuyForGrail || canBuyForEssence || canBuyForArmor))
        ? extraCrafts : 0u;
    if (extraPer == 0 || MerchantMgr::GetMerchantItemCount() == 0) return;

    if (canBuyForEssence && !stockState.featherOOS) {
        const uint32_t curFeather = CountInventoryModelQuantity(kMaterialFeather);
        ConsetBuyMaterial(kMaterialFeather, curFeather + extraPer * 50);
    }
    if ((canBuyForGrail || canBuyForEssence) && !stockState.dustOOS) {
        const uint32_t curDust = CountInventoryModelQuantity(kMaterialDust);
        ConsetBuyMaterial(kMaterialDust, curDust + extraPer * 50);
    }
    if ((canBuyForGrail || canBuyForArmor) && !stockState.ironOOS) {
        const uint32_t curIron = CountInventoryModelQuantity(kMaterialIronIngot);
        ConsetBuyMaterial(kMaterialIronIngot, curIron + extraPer * 50);
    }
    if (canBuyForArmor && !stockState.boneOOS) {
        const uint32_t curBone = CountInventoryModelQuantity(kMaterialBone);
        ConsetBuyMaterial(kMaterialBone, curBone + extraPer * 50);
    }
}

static ConsetMaterialStockState BuyConsetMaterials(const ConsetMaterialCounts& needed) {
    ConsetMaterialCounts have = ReadConsetMaterialCounts();
    IntReport("  Have: Iron=%u Dust=%u Bone=%u Feather=%u", have.iron, have.dust, have.bone, have.feather);
    IntReport("  Need: Iron=%u Dust=%u Bone=%u Feather=%u", needed.iron, needed.dust, needed.bone, needed.feather);

    ConsetMaterialStockState stockState{};
    IntReport("  Step 4: Buying materials (pass 1)...");
    if (have.iron < needed.iron) {
        auto result = ConsetBuyMaterial(kMaterialIronIngot, needed.iron);
        stockState.ironOOS = result.outOfStock;
    }
    if (have.dust < needed.dust) {
        auto result = ConsetBuyMaterial(kMaterialDust, needed.dust);
        stockState.dustOOS = result.outOfStock;
    }
    if (have.bone < needed.bone) {
        auto result = ConsetBuyMaterial(kMaterialBone, needed.bone);
        stockState.boneOOS = result.outOfStock;
    }
    if (have.feather < needed.feather) {
        auto result = ConsetBuyMaterial(kMaterialFeather, needed.feather);
        stockState.featherOOS = result.outOfStock;
    }

    if (stockState.ironOOS || stockState.dustOOS || stockState.boneOOS || stockState.featherOOS) {
        IntReport("  Out of stock: Iron=%s Dust=%s Bone=%s Feather=%s - redistributing budget",
                  stockState.ironOOS ? "YES" : "no", stockState.dustOOS ? "YES" : "no",
                  stockState.boneOOS ? "YES" : "no", stockState.featherOOS ? "YES" : "no");
        BuySupplementalConsetMaterials(stockState);
    }

    have = ReadConsetMaterialCounts();
    const uint32_t gold = ItemMgr::GetGoldCharacter();
    IntReport("  After buying: Iron=%u Dust=%u Bone=%u Feather=%u Gold=%u",
              have.iron, have.dust, have.bone, have.feather, gold);
    if (stockState.ironOOS || stockState.dustOOS || stockState.boneOOS || stockState.featherOOS) {
        IntReport("  NOTE: Some materials were out of stock. Will craft unequal conset components to burn remaining gold.");
    }
    return stockState;
}

static ConsetCraftPlan BuildConsetCraftPlan() {
    ConsetCraftPlan plan{};
    plan.materials = ReadConsetMaterialCounts();
    plan.gold = ItemMgr::GetGoldCharacter();

    uint32_t equalSets = plan.gold / 250;
    if (equalSets > plan.materials.iron / 100) equalSets = plan.materials.iron / 100;
    if (equalSets > plan.materials.dust / 100) equalSets = plan.materials.dust / 100;
    if (equalSets > plan.materials.bone / 50) equalSets = plan.materials.bone / 50;
    if (equalSets > plan.materials.feather / 50) equalSets = plan.materials.feather / 50;
    if (equalSets > plan.gold / 750) equalSets = plan.gold / 750;

    uint32_t leftIron = plan.materials.iron - equalSets * 100;
    const uint32_t leftDust = plan.materials.dust - equalSets * 100;
    const uint32_t leftBone = plan.materials.bone - equalSets * 50;
    const uint32_t leftFeather = plan.materials.feather - equalSets * 50;
    uint32_t leftGold = plan.gold - equalSets * 750;

    const uint32_t dustForGrails = leftDust / 2;
    const uint32_t dustForEssences = leftDust - dustForGrails;
    uint32_t extraGrails = leftIron / 50;
    if (extraGrails > dustForGrails / 50) extraGrails = dustForGrails / 50;
    if (extraGrails > leftGold / 250) extraGrails = leftGold / 250;
    leftIron -= extraGrails * 50;
    leftGold -= extraGrails * 250;

    uint32_t extraEssences = leftFeather / 50;
    if (extraEssences > dustForEssences / 50) extraEssences = dustForEssences / 50;
    if (extraEssences > leftGold / 250) extraEssences = leftGold / 250;
    leftGold -= extraEssences * 250;

    uint32_t extraArmors = leftIron / 50;
    if (extraArmors > leftBone / 50) extraArmors = leftBone / 50;
    if (extraArmors > leftGold / 250) extraArmors = leftGold / 250;

    plan.equalSets = equalSets;
    plan.grails = equalSets + extraGrails;
    plan.essences = equalSets + extraEssences;
    plan.armors = equalSets + extraArmors;
    IntReport("  Step 5: Craft plan - %u equal sets + extras: Grails=%u Essences=%u Armors=%u",
              plan.equalSets, plan.grails, plan.essences, plan.armors);
    IntReport("  Materials: Iron=%u Dust=%u Bone=%u Feather=%u Gold=%u",
              plan.materials.iron, plan.materials.dust, plan.materials.bone, plan.materials.feather, plan.gold);
    return plan;
}

static ConsetCraftResult ExecuteConsetCraftPlan(const ConsetCraftPlan& plan) {
    const uint32_t grailMats[] = {kMaterialIronIngot, kMaterialDust};
    const uint32_t grailQtys[] = {50u, 50u};
    const uint32_t essenceMats[] = {kMaterialFeather, kMaterialDust};
    const uint32_t essenceQtys[] = {50u, 50u};
    const uint32_t armorMats[] = {kMaterialIronIngot, kMaterialBone};
    const uint32_t armorQtys[] = {50u, 50u};

    ConsetCraftResult result{};
    if (plan.grails > 0) {
        result.grailsCrafted = ConsetCraftAllAtNPC("Eyja", kEmbarkEyjaX, kEmbarkEyjaY,
            ItemModelIds::GRAIL_OF_MIGHT, plan.grails, grailMats, grailQtys, 2);
    }
    if (plan.essences > 0) {
        result.essencesCrafted = ConsetCraftAllAtNPC("Kwat", kEmbarkKwatX, kEmbarkKwatY,
            ItemModelIds::ESSENCE_OF_CELERITY, plan.essences, essenceMats, essenceQtys, 2);
    }
    if (plan.armors > 0) {
        result.armorsCrafted = ConsetCraftAllAtNPC("Alcus", kEmbarkAlcusX, kEmbarkAlcusY,
            ItemModelIds::ARMOR_OF_SALVATION, plan.armors, armorMats, armorQtys, 2);
    }

    result.completeSets = (result.grailsCrafted < result.essencesCrafted ? result.grailsCrafted : result.essencesCrafted);
    result.completeSets = (result.completeSets < result.armorsCrafted ? result.completeSets : result.armorsCrafted);
    result.goldRemaining = ItemMgr::GetGoldCharacter();
    result.goldSpent = (plan.gold > result.goldRemaining) ? (plan.gold - result.goldRemaining) : 0;
    return result;
}

static bool ReportConsetCraftResult(const ConsetCraftResult& result) {
    char detail[256] = {};
    sprintf_s(detail, "grails=%u essences=%u armors=%u completeSets=%u goldSpent=%u goldRemaining=%u",
              result.grailsCrafted, result.essencesCrafted, result.armorsCrafted,
              result.completeSets, result.goldSpent, result.goldRemaining);
    WriteConsumableHarnessStatus("conset_cycle_complete", "conset", ReadMapId(), 0, 0, 0, 0,
                                 0, result.completeSets, result.completeSets > 0 ? 1u : 0u, detail);
    IntReport("=== CONSET RESULT: %u complete sets - %s ===", result.completeSets, detail);
    return result.completeSets > 0;
}
