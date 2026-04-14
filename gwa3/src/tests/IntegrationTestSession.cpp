// Session setup, login/bootstrap, and NPC/trader interaction slices.

#include "IntegrationTestInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TargetLogHook.h>
#include <gwa3/core/TradePartnerHook.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/packets/CtoSHook.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <string>

namespace GWA3::SmokeTest {

namespace {

void ReportRecentDialogUiTrace(const char* label) {
    uint32_t trace[32] = {};
    const uint32_t count = DialogMgr::GetRecentUITrace(trace, _countof(trace));
    char buf[512] = {};
    size_t used = 0;
    for (uint32_t i = 0; i < count && used + 16 < sizeof(buf); ++i) {
        used += sprintf_s(buf + used, sizeof(buf) - used, "%s0x%X", i == 0 ? "" : " ", trace[i]);
    }
    IntReport("  %s recent UI trace (%u): %s", label, count, count > 0 ? buf : "none");
}

struct MerchantStoCTap {
    StoC::HookEntry entries[0x200]{};
    LONG counts[0x200]{};
    bool active = false;
};

void WriteConsumableHarnessStatus(const char* stage, const char* targetLabel, uint32_t mapId, uint32_t npcId,
                                  uint32_t merchantItemCount, uint32_t targetModelId, uint32_t targetItemId,
                                  uint32_t beforeCount, uint32_t afterCount, uint32_t success,
                                  const char* detail);
uint32_t CountInventoryModelQuantity(uint32_t modelId);

void StartMerchantStoCTap(MerchantStoCTap& tap) {
    if (tap.active) return;
    ZeroMemory(tap.counts, sizeof(tap.counts));
    for (uint32_t header = 0; header < 0x200; ++header) {
        StoC::RegisterPostPacketCallback(&tap.entries[header], header,
            [&tap, header](StoC::HookStatus*, StoC::PacketBase*) {
                InterlockedIncrement(&tap.counts[header]);
            });
    }
    tap.active = true;
}

void StopMerchantStoCTap(MerchantStoCTap& tap) {
    if (!tap.active) return;
    for (uint32_t header = 0; header < 0x200; ++header) {
        StoC::RemoveCallbacks(&tap.entries[header]);
    }
    tap.active = false;
}

void ReportMerchantStoCTap(const char* label, MerchantStoCTap& tap) {
    IntReport("  %s:", label);
    bool any = false;
    for (uint32_t header = 0; header < 0x200; ++header) {
        const LONG count = tap.counts[header];
        if (count > 0) {
            any = true;
            IntReport("    StoC 0x%03X hits=%ld", header, count);
        }
    }
    if (!any) {
        IntReport("    no StoC headers observed in tap window");
    }
}

void FormatMerchantStoCTapSummary(char* out, size_t outSize, MerchantStoCTap& tap) {
    if (!out || !outSize) return;
    out[0] = '\0';
    size_t used = 0;
    bool any = false;
    for (uint32_t header = 0; header < 0x200 && used < outSize; ++header) {
        const LONG count = tap.counts[header];
        if (count <= 0) continue;
        any = true;
        const int written = sprintf_s(out + used, outSize - used, "%s0x%03X=%ld",
                                      used ? " " : "", header, count);
        if (written < 0) break;
        used += written;
    }
    if (!any) {
        sprintf_s(out, outSize, "none");
    }
}

void FormatCtoSPacketTapSummary(char* out, size_t outSize, const CtoS::PacketTapSnapshot& tap) {
    if (!out || !outSize) return;
    out[0] = '\0';
    if (tap.total_packets == 0 || tap.unique_headers == 0) {
        sprintf_s(out, outSize, "none");
        return;
    }

    size_t used = 0;
    int written = sprintf_s(out, outSize, "total=%u", tap.total_packets);
    if (written <= 0) {
        sprintf_s(out, outSize, "format_error");
        return;
    }
    used = static_cast<size_t>(written);

    for (uint32_t i = 0; i < 8 && used < outSize; ++i) {
        if (tap.headers[i] == 0 || tap.counts[i] == 0) continue;
        written = sprintf_s(out + used, outSize - used, "%s0x%03X=%u",
                            used > 0 ? " " : "",
                            tap.headers[i],
                            tap.counts[i]);
        if (written <= 0) break;
        used += static_cast<size_t>(written);
    }
}

enum class MerchantDialogVariant {
    StandardId,
    StandardPtr,
    LegacyId,
    LegacyPtr,
};

enum class MerchantIsolationStage {
    Full,
    TravelOnly,
    ApproachOnly,
    TargetOnly,
    InteractSinglePacketOnly,
    InteractAgentMgrOnly,
    InteractPacketOnly,
    InteractDwellOnly,
    InteractOnly,
};

enum class ConsumableHarnessStage {
    Full,
    TravelOnly,
    OpenOnly,
    ListOnly,
    CraftOnly,
    ConsetCycle,
};

enum class ConsumableHarnessTarget {
    All,
    Grail,
    Essence,
    Armor,
};

enum class ConsumableHarnessClickMode {
    AltOnly,
    PathOnly,
    RowOnly,
    RowChild0Only,
    RowChild1Only,
    RootOnly,
    Both,
};

bool CheckLocalFlagFile(const char* flagFile) {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&CheckLocalFlagFile), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    const size_t baseLen = strlen(path);
    const DWORD pid = GetCurrentProcessId();
    const char* ext = strrchr(flagFile, '.');
    if (ext && _stricmp(ext, ".flag") == 0) {
        std::string stem(flagFile, ext - flagFile);
        snprintf(path + baseLen, MAX_PATH - baseLen, "%s_%lu.flag", stem.c_str(), pid);
        const DWORD scopedAttr = GetFileAttributesA(path);
        if (scopedAttr != INVALID_FILE_ATTRIBUTES) {
            DeleteFileA(path);
            return true;
        }
        path[baseLen] = '\0';
    }
    strcat_s(path, flagFile);
    const DWORD attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        DeleteFileA(path);
        return true;
    }
    return false;
}

bool UseGaddsMerchantTarget() {
    return CheckLocalFlagFile("gwa3_test_merchant_target_gadds.flag");
}

MerchantDialogVariant GetMerchantDialogVariant() {
    if (CheckLocalFlagFile("gwa3_test_merchant_variant_standard_ptr.flag")) {
        return MerchantDialogVariant::StandardPtr;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_variant_legacy_id.flag")) {
        return MerchantDialogVariant::LegacyId;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_variant_legacy_ptr.flag")) {
        return MerchantDialogVariant::LegacyPtr;
    }
    return MerchantDialogVariant::StandardId;
}

const char* DescribeMerchantDialogVariant(MerchantDialogVariant variant) {
    switch (variant) {
    case MerchantDialogVariant::StandardId: return "standard dialog by agent id";
    case MerchantDialogVariant::StandardPtr: return "standard dialog by agent ptr";
    case MerchantDialogVariant::LegacyId: return "legacy dialog by agent id";
    case MerchantDialogVariant::LegacyPtr: return "legacy dialog by agent ptr";
    default: return "unknown";
    }
}

MerchantIsolationStage GetMerchantIsolationStage() {
    if (CheckLocalFlagFile("gwa3_test_merchant_stage_travel_only.flag")) {
        return MerchantIsolationStage::TravelOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_stage_approach_only.flag")) {
        return MerchantIsolationStage::ApproachOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_stage_target_only.flag")) {
        return MerchantIsolationStage::TargetOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_stage_interact_single_packet_only.flag")) {
        return MerchantIsolationStage::InteractSinglePacketOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_stage_interact_agentmgr_only.flag")) {
        return MerchantIsolationStage::InteractAgentMgrOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_stage_interact_packet_only.flag")) {
        return MerchantIsolationStage::InteractPacketOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_stage_interact_dwell_only.flag")) {
        return MerchantIsolationStage::InteractDwellOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_merchant_stage_interact_only.flag")) {
        return MerchantIsolationStage::InteractOnly;
    }
    return MerchantIsolationStage::Full;
}

const char* DescribeMerchantIsolationStage(MerchantIsolationStage stage) {
    switch (stage) {
    case MerchantIsolationStage::Full: return "travel + interact + dialog";
    case MerchantIsolationStage::TravelOnly: return "travel only";
    case MerchantIsolationStage::ApproachOnly: return "travel + approach";
    case MerchantIsolationStage::TargetOnly: return "travel + approach + target";
    case MerchantIsolationStage::InteractSinglePacketOnly: return "travel + approach + target + single raw interact";
    case MerchantIsolationStage::InteractAgentMgrOnly: return "travel + approach + target + AgentMgr::InteractNPC";
    case MerchantIsolationStage::InteractPacketOnly: return "travel + approach + target + raw interact";
    case MerchantIsolationStage::InteractDwellOnly: return "travel + approach + target + raw interact + dwell";
    case MerchantIsolationStage::InteractOnly: return "travel + interact";
    default: return "unknown";
    }
}

ConsumableHarnessStage GetConsumableHarnessStage() {
    if (CheckLocalFlagFile("gwa3_test_consumables_stage_travel_only.flag")) {
        return ConsumableHarnessStage::TravelOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_stage_open_only.flag")) {
        return ConsumableHarnessStage::OpenOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_stage_list_only.flag")) {
        return ConsumableHarnessStage::ListOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_stage_craft_only.flag")) {
        return ConsumableHarnessStage::CraftOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_stage_conset_cycle.flag")) {
        return ConsumableHarnessStage::ConsetCycle;
    }
    return ConsumableHarnessStage::Full;
}

const char* DescribeConsumableHarnessStage(ConsumableHarnessStage stage) {
    switch (stage) {
    case ConsumableHarnessStage::Full: return "travel + open + list + craft";
    case ConsumableHarnessStage::TravelOnly: return "travel only";
    case ConsumableHarnessStage::OpenOnly: return "travel + open";
    case ConsumableHarnessStage::ListOnly: return "travel + open + list";
    case ConsumableHarnessStage::CraftOnly: return "travel + open + list + craft";
    case ConsumableHarnessStage::ConsetCycle: return "full conset cycle: gold + materials + craft all 3";
    default: return "unknown";
    }
}

ConsumableHarnessTarget GetConsumableHarnessTarget() {
    if (CheckLocalFlagFile("gwa3_test_consumables_target_grail.flag")) {
        return ConsumableHarnessTarget::Grail;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_target_essence.flag")) {
        return ConsumableHarnessTarget::Essence;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_target_armor.flag")) {
        return ConsumableHarnessTarget::Armor;
    }
    return ConsumableHarnessTarget::All;
}

const char* DescribeConsumableHarnessTarget(ConsumableHarnessTarget target) {
    switch (target) {
    case ConsumableHarnessTarget::All: return "all consumable crafters";
    case ConsumableHarnessTarget::Grail: return "Eyja / Grail of Might";
    case ConsumableHarnessTarget::Essence: return "Kwat / Essence of Celerity";
    case ConsumableHarnessTarget::Armor: return "Alcus / Armor of Salvation";
    default: return "unknown";
    }
}

ConsumableHarnessClickMode GetConsumableHarnessClickMode() {
    if (CheckLocalFlagFile("gwa3_test_consumables_click_path_only.flag")) {
        return ConsumableHarnessClickMode::PathOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_click_row_only.flag")) {
        return ConsumableHarnessClickMode::RowOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_click_row_child0_only.flag")) {
        return ConsumableHarnessClickMode::RowChild0Only;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_click_row_child1_only.flag")) {
        return ConsumableHarnessClickMode::RowChild1Only;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_click_root_only.flag")) {
        return ConsumableHarnessClickMode::RootOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_consumables_click_both.flag")) {
        return ConsumableHarnessClickMode::Both;
    }
    return ConsumableHarnessClickMode::AltOnly;
}

const char* DescribeConsumableHarnessClickMode(ConsumableHarnessClickMode mode) {
    switch (mode) {
    case ConsumableHarnessClickMode::AltOnly: return "row + action126";
    case ConsumableHarnessClickMode::PathOnly: return "row + actionPath";
    case ConsumableHarnessClickMode::RowOnly: return "row only";
    case ConsumableHarnessClickMode::RowChild0Only: return "row child 0 only";
    case ConsumableHarnessClickMode::RowChild1Only: return "row child 1 only";
    case ConsumableHarnessClickMode::RootOnly: return "row + action125";
    case ConsumableHarnessClickMode::Both: return "row + action125 + action126";
    default: return "unknown";
    }
}

constexpr uint32_t kMerchantRootHash = 3613855137u;
constexpr uint32_t kMerchantActionButtonPrimaryHash = 3422277079u;
constexpr uint32_t kMerchantActionButtonAltHash = 1687064728u;
constexpr uint32_t kMerchantItemRowHash = 1852904459u;
constexpr uint32_t kTradeQuantityPromptChildOffsetId = 2u;
constexpr uint32_t kMapEmbarkBeach = 857u;
constexpr uint32_t kMapGadds = 638u;
constexpr uint32_t kMapLongeyesLedge = 650u;
constexpr uint32_t kTradeTestRegion = 4u;
constexpr uint32_t kTradeTestDistrict = 99u;
constexpr uint32_t kTradeFallbackDistrict = 1u;
constexpr uint32_t kTradeTestLanguage = 8u;
constexpr float kEmbarkEyjaX = 3336.0f;
constexpr float kEmbarkEyjaY = 627.0f;
constexpr float kEmbarkKwatX = 3596.0f;
constexpr float kEmbarkKwatY = 107.0f;
constexpr float kEmbarkAlcusX = 3704.0f;
constexpr float kEmbarkAlcusY = -163.0f;
constexpr float kEmbarkXunlaiX = 2283.0f;
constexpr float kEmbarkXunlaiY = -2134.0f;
constexpr float kGaddsMerchantX = -8374.0f;
constexpr float kGaddsMerchantY = -22491.0f;
constexpr uint32_t kModelArmorSalvation = 24860u;
constexpr uint32_t kModelEssenceCelerity = 24859u;
constexpr uint32_t kModelGrailOfMight = 24861u;
constexpr uint32_t kMaterialBone = 921u;
constexpr uint32_t kMaterialDust = 929u;
constexpr uint32_t kMaterialFeather = 933u;
constexpr uint32_t kMaterialPlantFiber = 934u;
constexpr uint32_t kMaterialIronIngot = 948u;
constexpr uint32_t kMaterialScale = 953u;
constexpr uint32_t kMaterialGraniteSlab = 955u;

struct ConsumableMaterialCounter {
    const char* label;
    uint32_t modelId;
    uint32_t bags14 = 0;
    uint32_t storage = 0;
};

struct ConsumableRecipeMaterial {
    uint32_t modelId;
    uint32_t quantity;
};

struct ConsumableCraftRecipe {
    uint32_t fee = 0;
    uint32_t materialCount = 0;
    ConsumableRecipeMaterial materials[2]{};
};

uint32_t CountBagModelQuantity(uint32_t modelId, uint32_t bagStart, uint32_t bagEnd) {
    uint32_t total = 0;
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;

    for (uint32_t bagIndex = bagStart; bagIndex <= bagEnd; ++bagIndex) {
        Bag* bag = inv->bags[bagIndex];
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (item && item->model_id == modelId) {
                total += item->quantity;
            }
        }
    }
    return total;
}

void FillConsumableMaterialCounters(ConsumableMaterialCounter (&counters)[7]) {
    counters[0] = {"iron", kMaterialIronIngot};
    counters[1] = {"dust", kMaterialDust};
    counters[2] = {"bone", kMaterialBone};
    counters[3] = {"feather", kMaterialFeather};
    counters[4] = {"granite", kMaterialGraniteSlab};
    counters[5] = {"fiber", kMaterialPlantFiber};
    counters[6] = {"scale", kMaterialScale};

    for (auto& counter : counters) {
        counter.bags14 = CountBagModelQuantity(counter.modelId, 1u, 4u);
        counter.storage = CountBagModelQuantity(counter.modelId, 6u, 6u);
    }
}

void FormatConsumableMaterialSnapshot(char* out, size_t outSize,
                                      const ConsumableMaterialCounter (&before)[7],
                                      const ConsumableMaterialCounter (&after)[7]) {
    if (!out || !outSize) return;
    const uint32_t goldChar = ItemMgr::GetGoldCharacter();
    const uint32_t goldStorage = ItemMgr::GetGoldStorage();
    sprintf_s(
        out, outSize,
        "goldChar=%u goldStorage=%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u %s=%u/%u->%u/%u",
        goldChar, goldStorage,
        before[0].label, before[0].bags14, before[0].storage, after[0].bags14, after[0].storage,
        before[1].label, before[1].bags14, before[1].storage, after[1].bags14, after[1].storage,
        before[2].label, before[2].bags14, before[2].storage, after[2].bags14, after[2].storage,
        before[3].label, before[3].bags14, before[3].storage, after[3].bags14, after[3].storage,
        before[4].label, before[4].bags14, before[4].storage, after[4].bags14, after[4].storage,
        before[5].label, before[5].bags14, before[5].storage, after[5].bags14, after[5].storage,
        before[6].label, before[6].bags14, before[6].storage, after[6].bags14, after[6].storage);
}

void FormatPromptChildSnapshot(char* out, size_t outSize, uintptr_t promptFrame, uint32_t maxChildren = 6u) {
    if (!out || !outSize) return;
    if (promptFrame < 0x10000) {
        sprintf_s(out, outSize, "prompt=0x%08X childCount=0", static_cast<unsigned>(promptFrame));
        return;
    }

    const uint32_t childCount = UIMgr::GetChildFrameCount(promptFrame);
    const uint32_t emitCount = childCount < maxChildren ? childCount : maxChildren;
    int written = sprintf_s(out, outSize, "prompt=0x%08X childCount=%u",
                            static_cast<unsigned>(promptFrame), childCount);
    if (written < 0) return;

    for (uint32_t i = 0; i < emitCount && static_cast<size_t>(written) < outSize; ++i) {
        const uintptr_t child = UIMgr::GetChildFrameByIndex(promptFrame, i);
        const int appended = sprintf_s(
            out + written, outSize - written,
            " i%u=0x%08X/h%u/o%u/s%X/c%u",
            i,
            static_cast<unsigned>(child),
            UIMgr::GetFrameHash(child),
            UIMgr::GetChildOffsetId(child),
            UIMgr::GetFrameState(child),
            UIMgr::GetChildFrameCount(child));
        if (appended < 0) break;
        written += appended;
    }
}

bool TryGetConsumableCraftRecipe(uint32_t targetModelId, ConsumableCraftRecipe& recipe) {
    ZeroMemory(&recipe, sizeof(recipe));
    switch (targetModelId) {
    case kModelGrailOfMight:
        recipe.fee = 250u;
        recipe.materialCount = 2u;
        recipe.materials[0] = {kMaterialIronIngot, 50u};
        recipe.materials[1] = {kMaterialDust, 50u};
        return true;
    case kModelEssenceCelerity:
        recipe.fee = 250u;
        recipe.materialCount = 2u;
        recipe.materials[0] = {kMaterialFeather, 50u};
        recipe.materials[1] = {kMaterialDust, 50u};
        return true;
    case kModelArmorSalvation:
        recipe.fee = 250u;
        recipe.materialCount = 2u;
        recipe.materials[0] = {kMaterialIronIngot, 50u};
        recipe.materials[1] = {kMaterialBone, 50u};
        return true;
    default:
        return false;
    }
}

bool CraftConsumableNatively(const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                             uint32_t merchantItemPosition, uint32_t beforeCount,
                             uint32_t& afterCount, char* detail, size_t detailSize) {
    ConsumableCraftRecipe recipe{};
    if (!TryGetConsumableCraftRecipe(targetModelId, recipe) || merchantItemPosition == UINT32_MAX || merchantItemPosition == 0u) {
        return false;
    }

    const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
    MerchantStoCTap tap{};
    StartMerchantStoCTap(tap);
    CtoS::ResetPacketTap();

    // Use the proven working craft path from FroggyHM CraftConsetsIfNeeded / Gemma craft_item:
    // TransactItems(type=3, quantity=1, merchantItemId) — simple TRANSACT_ITEMS packet.
    // No quote, no UIMessage struct, no material arrays.
    // The game resolves materials and gold cost internally from the merchant context.
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "transact_craft_start itemPos=%u itemId=%u gold=%u",
                  merchantItemPosition, targetItemId, goldBefore);
    }
    WriteConsumableHarnessStatus("transact_craft_start", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, detail ? detail : "");

    // Dispatch TransactItems on the game thread — matching FroggyHM's GameThread::Enqueue path.
    // Direct calls to TransactItems from the test thread crash (SendPacket goes through
    // the CtoS sender thread, wrong execution context for merchant transactions).
    struct CraftTransactTask { uint32_t item_id; };
    static auto CraftTransactInvoker = [](void* storage) {
        auto* t = reinterpret_cast<CraftTransactTask*>(storage);
        if (t && t->item_id) TradeMgr::TransactItems(3, 1, t->item_id);
    };
    CraftTransactTask craftTask{targetItemId};
    IntReport("  TransactItems(3, 1, %u) via GameThread — proven FroggyHM/Gemma craft path", targetItemId);
    GameThread::EnqueueRaw(CraftTransactInvoker, &craftTask, sizeof(craftTask));
    const bool craftQueued = true;
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "transact_craft_queued itemPos=%u itemId=%u gold=%u",
                  merchantItemPosition, targetItemId, goldBefore);
    }
    WriteConsumableHarnessStatus("transact_craft_queued", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, detail ? detail : "");
    if (!craftQueued) {
        StopMerchantStoCTap(tap);
        return false;
    }

    CtoS::ResetPacketTap();
    const bool craftObserved = WaitFor("native crafter transaction", 4000, [targetModelId, beforeCount, goldBefore]() {
        return CountInventoryModelQuantity(targetModelId) > beforeCount
            || ItemMgr::GetGoldCharacter() < goldBefore;
    });
    afterCount = CountInventoryModelQuantity(targetModelId);
    const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
    char stoCSummary[256] = {};
    char ctoSSummary[256] = {};
    FormatMerchantStoCTapSummary(stoCSummary, sizeof(stoCSummary), tap);
    FormatCtoSPacketTapSummary(ctoSSummary, sizeof(ctoSSummary), CtoS::GetPacketTapSnapshot());
    StopMerchantStoCTap(tap);
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "native_craft_complete observed=%u before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
                  craftObserved ? 1u : 0u, beforeCount, afterCount, goldBefore, goldAfter, stoCSummary, ctoSSummary);
    }
    WriteConsumableHarnessStatus("native_craft_complete", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, afterCount, afterCount > beforeCount ? 1u : 0u,
                                 detail ? detail : "");
    return afterCount > beforeCount;
}

bool CraftConsumableByPacket(const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                             uint32_t merchantItemPosition, uint32_t beforeCount,
                             uint32_t& afterCount, char* detail, size_t detailSize) {
    // Raw SendPacket(0x4C) and SendPacket(0x4D) crash the GW client in crafter context.
    // Both SendPacketViaGameCommand (PID 37560) and normal SendPacket (PID 30232) caused
    // Gw.exe crash dialogs. The game expects these operations to go through native
    // RequestQuoteFunction / TransactionFunction, not raw packet injection.
    // This path is disabled; CraftConsumableNatively now uses direct function calls.
    IntReport("  CraftConsumableByPacket DISABLED — raw 0x4C/0x4D packets crash the client");
    WriteConsumableHarnessStatus("packet_path_disabled", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 0,
                                 "raw_packet_quote_transact_crashes_client");
    return false;
    ConsumableCraftRecipe recipe{};
    if (!TryGetConsumableCraftRecipe(targetModelId, recipe) || merchantItemPosition == UINT32_MAX || merchantItemPosition == 0u) {
        return false;
    }

    const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
    const uint32_t quoteBefore = TraderHook::GetQuoteId();
    MerchantStoCTap tap{};
    StartMerchantStoCTap(tap);
    CtoS::ResetPacketTap();
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "packet_quote_start itemPos=%u targetModel=%u targetItem=%u quoteBefore=%u",
                  merchantItemPosition, targetModelId, targetItemId, quoteBefore);
    }
    WriteConsumableHarnessStatus("packet_quote_start", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, detail ? detail : "");

    const bool quoteQueued = TradeMgr::RequestCrafterQuoteByPositionPacket(merchantItemPosition);
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "packet_quote_queued=%u itemPos=%u targetItem=%u",
                  quoteQueued ? 1u : 0u, merchantItemPosition, targetItemId);
    }
    WriteConsumableHarnessStatus("packet_quote_queued", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, quoteQueued ? 1u : 0u, detail ? detail : "");
    if (!quoteQueued) {
        StopMerchantStoCTap(tap);
        return false;
    }

    const bool quoteObserved = WaitFor("packet crafter quote response", 3000, [quoteBefore]() {
        return TraderHook::GetQuoteId() != quoteBefore || TraderHook::GetCostValue() > 0;
    });
    const uint32_t quoteAfter = TraderHook::GetQuoteId();
    const uint32_t costItemId = TraderHook::GetCostItemId();
    const uint32_t quotedCost = TraderHook::GetCostValue();
    char stoCSummary[256] = {};
    char ctoSSummary[256] = {};
    FormatMerchantStoCTapSummary(stoCSummary, sizeof(stoCSummary), tap);
    FormatCtoSPacketTapSummary(ctoSSummary, sizeof(ctoSSummary), CtoS::GetPacketTapSnapshot());
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "packet_quote_complete observed=%u quoteBefore=%u quoteAfter=%u costItem=%u costValue=%u stoC=%s ctoS=%s",
                  quoteObserved ? 1u : 0u, quoteBefore, quoteAfter, costItemId, quotedCost, stoCSummary, ctoSSummary);
    }
    WriteConsumableHarnessStatus("packet_quote_complete", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount,
                                 (quoteObserved && costItemId == targetItemId) ? 1u : 0u,
                                 detail ? detail : "");

    CtoS::ResetPacketTap();
    const bool craftQueued = TradeMgr::CraftMerchantItemByPositionPacket(merchantItemPosition, 1u);
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "packet_craft_queued=%u itemPos=%u targetItem=%u",
                  craftQueued ? 1u : 0u, merchantItemPosition, targetItemId);
    }
    WriteConsumableHarnessStatus("packet_craft_queued", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, craftQueued ? 1u : 0u, detail ? detail : "");
    if (!craftQueued) {
        StopMerchantStoCTap(tap);
        return false;
    }

    const bool craftObserved = WaitFor("packet crafter transaction", 4000, [targetModelId, beforeCount, goldBefore]() {
        return CountInventoryModelQuantity(targetModelId) > beforeCount
            || ItemMgr::GetGoldCharacter() < goldBefore;
    });
    afterCount = CountInventoryModelQuantity(targetModelId);
    const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
    FormatMerchantStoCTapSummary(stoCSummary, sizeof(stoCSummary), tap);
    FormatCtoSPacketTapSummary(ctoSSummary, sizeof(ctoSSummary), CtoS::GetPacketTapSnapshot());
    StopMerchantStoCTap(tap);
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "packet_craft_complete observed=%u before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
                  craftObserved ? 1u : 0u, beforeCount, afterCount, goldBefore, goldAfter, stoCSummary, ctoSSummary);
    }
    WriteConsumableHarnessStatus("packet_craft_complete", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, afterCount, afterCount > beforeCount ? 1u : 0u,
                                 detail ? detail : "");
    return afterCount > beforeCount;
}

void FormatPromptNestedChildSnapshot(char* out, size_t outSize, uintptr_t promptFrame,
                                     uint32_t parentChildIndex, uint32_t maxChildren = 6u) {
    if (!out || !outSize) return;
    const uintptr_t parent = UIMgr::GetChildFrameByIndex(promptFrame, parentChildIndex);
    if (parent < 0x10000) {
        sprintf_s(out, outSize, "prompt=0x%08X parentIndex=%u parent=0x%08X",
                  static_cast<unsigned>(promptFrame),
                  parentChildIndex,
                  static_cast<unsigned>(parent));
        return;
    }

    const uint32_t childCount = UIMgr::GetChildFrameCount(parent);
    const uint32_t emitCount = childCount < maxChildren ? childCount : maxChildren;
    int written = sprintf_s(out, outSize,
                            "prompt=0x%08X parentIndex=%u parent=0x%08X hash=%u childCount=%u",
                            static_cast<unsigned>(promptFrame),
                            parentChildIndex,
                            static_cast<unsigned>(parent),
                            UIMgr::GetFrameHash(parent),
                            childCount);
    if (written < 0) return;

    for (uint32_t i = 0; i < emitCount && static_cast<size_t>(written) < outSize; ++i) {
        const uintptr_t child = UIMgr::GetChildFrameByIndex(parent, i);
        const int appended = sprintf_s(
            out + written, outSize - written,
            " i%u=0x%08X/h%u/o%u/s%X/c%u",
            i,
            static_cast<unsigned>(child),
            UIMgr::GetFrameHash(child),
            UIMgr::GetChildOffsetId(child),
            UIMgr::GetFrameState(child),
            UIMgr::GetChildFrameCount(child));
        if (appended < 0) break;
        written += appended;
    }
}

void FormatPromptNestedGrandchildSnapshot(char* out, size_t outSize, uintptr_t promptFrame,
                                          uint32_t parentChildIndex, uint32_t childIndex,
                                          uint32_t maxChildren = 6u) {
    if (!out || !outSize) return;
    const uintptr_t parent = UIMgr::GetChildFrameByIndex(promptFrame, parentChildIndex);
    const uintptr_t child = UIMgr::GetChildFrameByIndex(parent, childIndex);
    if (child < 0x10000) {
        sprintf_s(out, outSize, "prompt=0x%08X parentIndex=%u childIndex=%u child=0x%08X",
                  static_cast<unsigned>(promptFrame), parentChildIndex, childIndex, static_cast<unsigned>(child));
        return;
    }
    size_t used = 0;
    const uint32_t childCount = UIMgr::GetChildFrameCount(child);
    used += sprintf_s(out + used, outSize - used,
                      "prompt=0x%08X parentIndex=%u childIndex=%u node=0x%08X hash=%u childCount=%u",
                      static_cast<unsigned>(promptFrame),
                      parentChildIndex,
                      childIndex,
                      static_cast<unsigned>(child),
                      UIMgr::GetFrameHash(child),
                      childCount);
    const uint32_t capped = childCount < maxChildren ? childCount : maxChildren;
    for (uint32_t i = 0; i < capped && used < outSize; ++i) {
        const uintptr_t nested = UIMgr::GetChildFrameByIndex(child, i);
        used += sprintf_s(out + used, outSize - used,
                          " i%u=0x%08X/h%u/o%u/s%X/c%u",
                          i,
                          static_cast<unsigned>(nested),
                          UIMgr::GetFrameHash(nested),
                          UIMgr::GetChildOffsetId(nested),
                          UIMgr::GetFrameState(nested),
                          UIMgr::GetChildFrameCount(nested));
    }
}

bool ConfirmCrafterQuantityPromptOneDirect(const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                                           uint32_t beforeCount) {
    const uintptr_t promptFrame = TradeMgr::GetTradeQuantityPromptFrame();
    if (promptFrame < 0x10000) return false;

    const uintptr_t promptButtonBar = UIMgr::GetChildFrameByIndex(promptFrame, 5u);
    const uintptr_t promptChild2 = UIMgr::GetChildFrameByIndex(promptFrame, 2u);
    const uintptr_t candidates[] = {
        UIMgr::GetChildFrameByIndex(promptChild2, 0u),
        UIMgr::GetChildFrameByIndex(promptButtonBar, 2u),
        UIMgr::GetChildFrameByIndex(promptButtonBar, 1u),
        UIMgr::GetChildFrameByIndex(promptFrame, 3u),
    };
    const char* labels[] = {
        "child2[0]",
        "bar[5][2]",
        "bar[5][1]",
        "root[3]",
    };

    for (size_t i = 0; i < _countof(candidates); ++i) {
        const uintptr_t candidate = candidates[i];
        if (candidate < 0x10000 || UIMgr::IsFrameHidden(candidate)) continue;

        char detail[192] = {};
        sprintf_s(detail, "direct_prompt_click_start candidate=%s frame=0x%08X", labels[i], static_cast<unsigned>(candidate));
        WriteConsumableHarnessStatus("quantity_prompt_direct_click_start", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), 1, detail);

        const bool clicked = UIMgr::ButtonClick(candidate);
        Sleep(250 + ChatMgr::GetPing());
        const bool closed = !TradeMgr::IsTradeQuantityPromptOpen();
        sprintf_s(detail, "direct_prompt_click_complete candidate=%s frame=0x%08X clicked=%u closed=%u",
                  labels[i], static_cast<unsigned>(candidate), clicked ? 1u : 0u, closed ? 1u : 0u);
        WriteConsumableHarnessStatus("quantity_prompt_direct_click_complete", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), closed ? 1u : 0u, detail);
        if (clicked && closed) return true;
    }

    if (promptChild2 >= 0x10000) {
        const uintptr_t child20 = UIMgr::GetChildFrameByIndex(promptChild2, 0u);
        const uintptr_t child200 = UIMgr::GetChildFrameByIndex(child20, 0u);
        const uintptr_t commitButtons[] = {
            UIMgr::GetChildFrameByIndex(promptButtonBar, 2u),
            UIMgr::GetChildFrameByIndex(promptButtonBar, 1u),
        };
        const char* commitLabels[] = { "bar[5][2]", "bar[5][1]" };
        const wchar_t quantityBuf[] = L"1";

        const auto trySetAndCommit = [&](const char* mode, bool setOk) -> bool {
            char detail[224] = {};
            sprintf_s(detail, "direct_prompt_set_%s child2[0]=0x%08X parent=0x%08X setOk=%u",
                      mode, static_cast<unsigned>(child20), static_cast<unsigned>(promptChild2), setOk ? 1u : 0u);
            WriteConsumableHarnessStatus("quantity_prompt_set_start", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), setOk ? 1u : 0u, detail);
            if (!setOk) return false;

            for (size_t i = 0; i < _countof(commitButtons); ++i) {
                const uintptr_t commit = commitButtons[i];
                if (commit < 0x10000 || UIMgr::IsFrameHidden(commit)) continue;
                const bool clicked = UIMgr::ButtonClick(commit);
                Sleep(250 + ChatMgr::GetPing());
                const bool closed = !TradeMgr::IsTradeQuantityPromptOpen();
                sprintf_s(detail, "direct_prompt_set_%s_commit candidate=%s frame=0x%08X clicked=%u closed=%u",
                          mode, commitLabels[i], static_cast<unsigned>(commit), clicked ? 1u : 0u, closed ? 1u : 0u);
                WriteConsumableHarnessStatus("quantity_prompt_set_commit_complete", targetLabel, ReadMapId(), 0,
                                             TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                             beforeCount, CountInventoryModelQuantity(targetModelId), closed ? 1u : 0u, detail);
                if (clicked && closed) return true;
            }
            return false;
        };

        if (child20 >= 0x10000 && trySetAndCommit("editable", UIMgr::SetEditableTextValue(child20, quantityBuf, promptChild2))) {
            return true;
        }
        if (child20 >= 0x10000 && trySetAndCommit("numeric", UIMgr::SetNumericFrameValue(child20, 1u, promptChild2))) {
            return true;
        }
        if (child200 >= 0x10000 && trySetAndCommit("editable_nested", UIMgr::SetEditableTextValue(child200, quantityBuf, child20))) {
            return true;
        }
        if (child200 >= 0x10000 && trySetAndCommit("numeric_nested", UIMgr::SetNumericFrameValue(child200, 1u, child20))) {
            return true;
        }
    }

    return false;
}

bool WaitForConsumableTravelState(const char* waitStage, const char* targetLabel,
                                  uint32_t expectedMapId, uint32_t expectedRegion, uint32_t expectedDistrict,
                                  uint32_t timeoutMs) {
    const DWORD start = GetTickCount();
    DWORD lastStatusTick = 0;
    DWORD loadedOtherDistrictSince = 0;
    while ((GetTickCount() - start) < timeoutMs) {
        const uint32_t mapId = ReadMapId();
        const uint32_t myId = ReadMyId();
        const uint32_t region = MapMgr::GetRegion();
        const uint32_t district = MapMgr::GetDistrict();
        const uint32_t loading = MapMgr::GetLoadingState();
        const bool matched =
            mapId == expectedMapId &&
            region == expectedRegion &&
            district == expectedDistrict &&
            myId > 0;
        if (matched) return true;

        const bool loadedTargetOtherDistrict =
            mapId == expectedMapId &&
            region == expectedRegion &&
            district != expectedDistrict &&
            myId > 0 &&
            loading == 1;
        if (loadedTargetOtherDistrict) {
            if (loadedOtherDistrictSince == 0) {
                loadedOtherDistrictSince = GetTickCount();
            } else if ((GetTickCount() - loadedOtherDistrictSince) >= 3000) {
                return false;
            }
        } else {
            loadedOtherDistrictSince = 0;
        }

        const DWORD now = GetTickCount();
        if (lastStatusTick == 0 || (now - lastStatusTick) >= 1000) {
            char detail[192] = {};
            sprintf_s(detail,
                      "waiting map=%u region=%u district=%u myId=%u loading=%u expectedMap=%u expectedRegion=%u expectedDistrict=%u elapsedMs=%lu",
                      mapId,
                      region,
                      district,
                      myId,
                      loading,
                      expectedMapId,
                      expectedRegion,
                      expectedDistrict,
                      static_cast<unsigned long>(now - start));
            WriteConsumableHarnessStatus(waitStage, targetLabel, mapId, 0, 0, 0, 0, 0, 0, 0, detail);
            lastStatusTick = now;
        }
        Sleep(250);
    }
    return false;
}

template <typename T>
struct HelperArrayView {
    T* buffer;
    uint32_t capacity;
    uint32_t size;
    uint32_t param;
};

struct HelperTradeContextView {
    struct Item {
        uint32_t item_id;
        uint32_t quantity;
    };
    struct Trader {
        uint32_t gold;
        HelperArrayView<Item> items;
    };
    uint32_t flags;
    uint32_t h0004[3];
    Trader player;
    Trader partner;
};

void WriteTradeHelperStatus(uint32_t mapId, uint32_t region, uint32_t district, uint32_t myId, float x, float y,
                            uint32_t tradeFlags, uint32_t tradeOpenCount, uint32_t lastOpenFlags,
                            uint32_t submitAttemptCount, uint32_t acceptAttemptCount,
                            uint32_t playerGold, uint32_t partnerGold,
                            uint32_t playerItemCount, uint32_t partnerItemCount,
                            uint32_t tradePartnerHookHits, uint32_t tradePartnerLastEax,
                            uint32_t tradePartnerLastEcx, uint32_t tradePartnerLastEdx,
                            uint32_t tradeUiPlayerUpdatedCount, uint32_t tradeUiSessionStartCount,
                            uint32_t tradeUiSessionUpdatedCount, uint32_t tradeUiLastSessionStartState,
                            uint32_t tradeUiLastSessionStartPlayerNumber,
                            const char* partnerItemsJson = "[]") {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&WriteTradeHelperStatus), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "trade_helper_status.json");

    char buf[2048];
    sprintf_s(buf,
              "{\"map_id\":%u,\"region\":%u,\"district\":%u,\"my_id\":%u,\"x\":%.1f,\"y\":%.1f,\"trade_flags\":%u,\"trade_open_count\":%u,\"last_open_flags\":%u,\"submit_attempt_count\":%u,\"accept_attempt_count\":%u,\"player_gold\":%u,\"partner_gold\":%u,\"player_item_count\":%u,\"partner_item_count\":%u,\"partner_items\":%s,\"trade_partner_hook_hits\":%u,\"trade_partner_last_eax\":%u,\"trade_partner_last_ecx\":%u,\"trade_partner_last_edx\":%u,\"trade_ui_player_updated_count\":%u,\"trade_ui_session_start_count\":%u,\"trade_ui_session_updated_count\":%u,\"trade_ui_last_session_start_state\":%u,\"trade_ui_last_session_start_player_number\":%u}\n",
              mapId, region, district, myId, x, y, tradeFlags, tradeOpenCount, lastOpenFlags, submitAttemptCount, acceptAttemptCount, playerGold, partnerGold, playerItemCount, partnerItemCount,
              partnerItemsJson ? partnerItemsJson : "[]",
              tradePartnerHookHits, tradePartnerLastEax, tradePartnerLastEcx, tradePartnerLastEdx,
              tradeUiPlayerUpdatedCount, tradeUiSessionStartCount, tradeUiSessionUpdatedCount,
              tradeUiLastSessionStartState, tradeUiLastSessionStartPlayerNumber);

    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, buf, static_cast<DWORD>(strlen(buf)), &written, nullptr);
    CloseHandle(h);
}

void WriteConsumableHarnessStatus(const char* stage, const char* targetLabel, uint32_t mapId, uint32_t npcId,
                                  uint32_t merchantItemCount, uint32_t targetModelId, uint32_t targetItemId,
                                  uint32_t beforeCount, uint32_t afterCount, uint32_t success,
                                  const char* detail) {
    char dirPath[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&WriteConsumableHarnessStatus), &hSelf);
    GetModuleFileNameA(hSelf, dirPath, MAX_PATH);
    char* slash = strrchr(dirPath, '\\');
    if (slash) *(slash + 1) = '\0';

    const char* safeStage = stage ? stage : "";
    const char* safeTarget = targetLabel ? targetLabel : "";
    const char* safeDetail = detail ? detail : "";
    const DWORD pid = GetCurrentProcessId();
    static const DWORD runTag = GetTickCount();

    char buf[1152];
    sprintf_s(buf,
              "{\"pid\":%lu,\"run_tag\":%lu,\"stage\":\"%s\",\"target\":\"%s\",\"map_id\":%u,\"npc_id\":%u,\"merchant_item_count\":%u,\"target_model_id\":%u,\"target_item_id\":%u,\"before_count\":%u,\"after_count\":%u,\"success\":%u,\"detail\":\"%s\"}\n",
              pid, runTag, safeStage, safeTarget, mapId, npcId, merchantItemCount, targetModelId, targetItemId,
              beforeCount, afterCount, success, safeDetail);

    char statusPath[MAX_PATH];
    strcpy_s(statusPath, dirPath);
    strcat_s(statusPath, "consumable_harness_status.json");

    HANDLE h = CreateFileA(statusPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, buf, static_cast<DWORD>(strlen(buf)), &written, nullptr);
        CloseHandle(h);
    }

    char historyPath[MAX_PATH];
    strcpy_s(historyPath, dirPath);
    strcat_s(historyPath, "consumable_harness_history.jsonl");
    HANDLE hh = CreateFileA(historyPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hh != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hh, buf, static_cast<DWORD>(strlen(buf)), &written, nullptr);
        CloseHandle(hh);
    }
}

static uintptr_t ResolveGameContextForTradeHelper() {
    if (Offsets::BasePointer <= 0x10000) return 0;
    __try {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (ctx <= 0x10000) return 0;
        uintptr_t gc = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (gc <= 0x10000) return 0;
        return gc;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static uint32_t ReadTradeFlagsForHelper() {
    uintptr_t gc = ResolveGameContextForTradeHelper();
    if (!gc) return 0;
    __try {
        uintptr_t trade = *reinterpret_cast<uintptr_t*>(gc + 0x58);
        if (trade <= 0x10000) return 0;
        return *reinterpret_cast<uint32_t*>(trade + 0x0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

struct HelperPartnerItemInfo {
    uint32_t item_id;
    uint32_t model_id;
    uint32_t quantity;
};

static bool ReadTradeStateForHelper(uint32_t& playerGold, uint32_t& partnerGold,
                                    uint32_t& playerItemCount, uint32_t& partnerItemCount) {
    playerGold = 0;
    partnerGold = 0;
    playerItemCount = 0;
    partnerItemCount = 0;
    uintptr_t gc = ResolveGameContextForTradeHelper();
    if (!gc) return false;
    __try {
        uintptr_t trade = *reinterpret_cast<uintptr_t*>(gc + 0x58);
        if (trade <= 0x10000) return false;
        auto* ctx = reinterpret_cast<HelperTradeContextView*>(trade);
        playerGold = ctx->player.gold;
        partnerGold = ctx->partner.gold;
        playerItemCount = ctx->player.items.size;
        partnerItemCount = ctx->partner.items.size;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static size_t ReadTradePartnerItemsForHelper(HelperPartnerItemInfo* out, size_t capacity) {
    if (!out || capacity == 0) return 0;
    uintptr_t gc = ResolveGameContextForTradeHelper();
    if (!gc) return 0;
    __try {
        uintptr_t trade = *reinterpret_cast<uintptr_t*>(gc + 0x58);
        if (trade <= 0x10000) return 0;
        auto* ctx = reinterpret_cast<HelperTradeContextView*>(trade);
        const uint32_t count = ctx->partner.items.size;
        const auto* items = ctx->partner.items.buffer;
        if (!items || count == 0) return 0;
        size_t written = 0;
        for (uint32_t i = 0; i < count && written < capacity; ++i) {
            const uint32_t itemId = items[i].item_id;
            const uint32_t qty = items[i].quantity;
            uint32_t modelId = 0;
            auto* fullItem = ItemMgr::GetItemById(itemId);
            if (fullItem) modelId = fullItem->model_id;
            out[written++] = {itemId, modelId, qty};
        }
        return written;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static uint32_t FindHelperInventoryItemByModel(uint32_t modelId) {
    if (modelId == 0) return 0;
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) return 0;
    for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
        Bag* bag = ItemMgr::GetBag(bagIdx);
        if (!bag || !bag->items.buffer) continue;
        for (uint32_t slot = 0; slot < bag->items.size; ++slot) {
            Item* item = bag->items.buffer[slot];
            if (item && item->model_id == modelId && item->item_id > 0) {
                return item->item_id;
            }
        }
    }
    return 0;
}

static uint32_t ReadTradeHelperSubmitGoldConfig() {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&ReadTradeHelperSubmitGoldConfig), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "trade_helper_config.json");

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;

    char buf[256] = {};
    DWORD read = 0;
    const BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr);
    CloseHandle(h);
    if (!ok || read == 0) return 0;
    buf[read] = '\0';

    const char* key = strstr(buf, "\"submit_gold\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static bool ReadTradeHelperAutoSubmitConfig() {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&ReadTradeHelperAutoSubmitConfig), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "trade_helper_config.json");

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    char buf[256] = {};
    DWORD read = 0;
    const BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr);
    CloseHandle(h);
    if (!ok || read == 0) return false;
    buf[read] = '\0';

    const char* key = strstr(buf, "\"auto_submit\"");
    if (!key) return false;
    const char* colon = strchr(key, ':');
    if (!colon) return false;
    while (*colon == ':' || *colon == ' ' || *colon == '\t') ++colon;
    return _strnicmp(colon, "true", 4) == 0 || *colon == '1';
}

static uint32_t ReadTradeHelperOfferItemModelConfig() {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&ReadTradeHelperOfferItemModelConfig), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "trade_helper_config.json");

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;

    char buf[256] = {};
    DWORD read = 0;
    const BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr);
    CloseHandle(h);
    if (!ok || read == 0) return 0;
    buf[read] = '\0';

    const char* key = strstr(buf, "\"offer_item_model_id\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

uintptr_t GetAgentPtrRaw(uint32_t agentId) {
    if (Offsets::AgentBase <= 0x10000 || agentId == 0 || agentId >= 5000) return 0;

    __try {
        uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
        if (agentArr <= 0x10000) return 0;
        return *reinterpret_cast<uintptr_t*>(agentArr + agentId * 4);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

void ReportMerchantPreInteractState(const char* label, uint32_t npcId, float npcX, float npcY) {
    float meX = 0.0f;
    float meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const uint32_t currentTarget = AgentMgr::GetTargetId();
    const bool dialogOpen = DialogMgr::IsDialogOpen();
    const uint32_t dialogSender = DialogMgr::GetDialogSenderAgentId();
    const uint32_t dialogButtons = DialogMgr::GetButtonCount();
    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(kMerchantRootHash);
    const uint32_t merchantItems = TradeMgr::GetMerchantItemCount();
    const uint32_t heroCount = PartyMgr::CountPartyHeroes();
    const float dist = AgentMgr::GetDistance(meX, meY, npcX, npcY);
    IntReport("  %s: npc=%u playerPos=(%.0f, %.0f) npcPos=(%.0f, %.0f) dist=%.0f target=%u dialogOpen=%d sender=%u buttons=%u merchantFrame=0x%08X items=%u heroes=%u",
              label,
              npcId,
              meX, meY,
              npcX, npcY,
              dist,
              currentTarget,
              dialogOpen ? 1 : 0,
              dialogSender,
              dialogButtons,
              static_cast<unsigned>(merchantFrame),
              merchantItems,
              heroCount);
}

void ReportMerchantRuntimeContext(const char* label) {
    IntReport("  %s: GameThread=%d onGameThread=%d RenderHook=%d hb=%u TraderHook=%d TargetLogHook=%d targetCalls=%u targetStores=%u CtoSHook=%d ctoSHb=%u",
              label,
              GameThread::IsInitialized() ? 1 : 0,
              GameThread::IsOnGameThread() ? 1 : 0,
              RenderHook::IsInitialized() ? 1 : 0,
              RenderHook::GetHeartbeat(),
              TraderHook::IsInitialized() ? 1 : 0,
              TargetLogHook::IsInitialized() ? 1 : 0,
              TargetLogHook::GetCallCount(),
              TargetLogHook::GetStoreCount(),
              CtoSHook::IsInitialized() ? 1 : 0,
              CtoSHook::GetHeartbeat());
}

void EmitMerchantScreenshotMarker(const char* reason) {
    IntReport("MERCHANT_SCREENSHOT_NOW: %s", reason);
}

void ReportMerchantTradeState(const char* label) {
    uintptr_t p0 = 0;
    uintptr_t p1 = 0;
    uintptr_t p2 = 0;
    uintptr_t merchantBase = 0;
    uintptr_t merchantSize = 0;
    bool ok = false;

    __try {
        if (Offsets::BasePointer > 0x10000) {
            p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
            if (p0 > 0x10000) {
                p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
                if (p1 > 0x10000) {
                    p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
                    if (p2 > 0x10000) {
                        merchantBase = *reinterpret_cast<uintptr_t*>(p2 + 0x24);
                        merchantSize = *reinterpret_cast<uintptr_t*>(p2 + 0x28);
                        ok = true;
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }

    IntReport("  %s: tradePtrs ok=%d p0=0x%08X p1=0x%08X p2=0x%08X merchantBase=0x%08X merchantSize=%u quoteId=%u costItem=%u costValue=%u",
              label,
              ok ? 1 : 0,
              static_cast<unsigned>(p0),
              static_cast<unsigned>(p1),
              static_cast<unsigned>(p2),
              static_cast<unsigned>(merchantBase),
              static_cast<unsigned>(merchantSize),
              TraderHook::GetQuoteId(),
              TraderHook::GetCostItemId(),
              TraderHook::GetCostValue());
}

void ReportMerchantInventoryList(const char* label, uint32_t limit = 16) {
    const uint32_t merchantCount = TradeMgr::GetMerchantItemCount();
    IntReport("  %s: merchant item count=%u", label, merchantCount);
    const uint32_t capped = (merchantCount < limit) ? merchantCount : limit;
    for (uint32_t i = 0; i < capped; ++i) {
        Item* item = TradeMgr::GetMerchantItemByPosition(i);
        if (!item) {
            IntReport("    [%u] <null>", i);
            continue;
        }
        IntReport("    [%u] item=%u model=%u type=%u value=%u quantity=%u", i,
                  item->item_id, item->model_id, item->type, item->value, item->quantity);
    }
    if (merchantCount > capped) {
        IntReport("    ... %u more merchant items not shown", merchantCount - capped);
    }
}

void BuildMerchantInventorySummary(char* out, size_t outSize, uint32_t limit = 8) {
    if (!out || outSize == 0) return;
    out[0] = '\0';

    const uint32_t merchantCount = TradeMgr::GetMerchantItemCount();
    char tmp[64];
    sprintf_s(tmp, "count=%u models=", merchantCount);
    strcat_s(out, outSize, tmp);

    const uint32_t capped = (merchantCount < limit) ? merchantCount : limit;
    for (uint32_t i = 0; i < capped; ++i) {
        Item* item = TradeMgr::GetMerchantItemByPosition(i);
        if (!item) {
            strcat_s(out, outSize, "null");
        } else {
            sprintf_s(tmp, "%u", item->model_id);
            strcat_s(out, outSize, tmp);
        }
        if (i + 1 < capped) {
            strcat_s(out, outSize, ",");
        }
    }
}

uint32_t FindNearestNpcLikeAgentToCoords(float targetX, float targetY, float maxDistance) {
    if (Offsets::AgentBase <= 0x10000) return 0;
    const uint32_t myId = ReadMyId();

    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return 0;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    if (maxAgents == 0) return 0;

    const float maxDistSq = maxDistance * maxDistance;
    float bestDistSq = maxDistSq;
    uint32_t bestId = 0;

    for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
        if (i == myId) continue;
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000) continue;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if (base->type != 0xDB) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->hp <= 0.0f) continue;
        if (living->allegiance != 6) continue;

        const float distSq = AgentMgr::GetSquaredDistance(targetX, targetY, living->x, living->y);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestId = i;
        }
    }

    return bestId;
}

uint32_t CountInventoryModelQuantity(uint32_t modelId) {
    return CountBagModelQuantity(modelId, 1u, 4u);
}

uint32_t FindMerchantItemPositionByModelId(uint32_t modelId) {
    const uint32_t merchantCount = TradeMgr::GetMerchantItemCount();
    for (uint32_t i = 0; i < merchantCount; ++i) {
        Item* item = TradeMgr::GetMerchantItemByPosition(i);
        if (item && item->model_id == modelId) return i;
    }
    return UINT32_MAX;
}

uintptr_t ResolveMerchantSortedPathFrame(uintptr_t merchantFrame, const uint32_t* path, uint32_t pathLen, const char* label) {
    const uintptr_t frame = UIMgr::NavigateSortedChildPath(merchantFrame, path, pathLen);
    IntReport("  Merchant path %s: frame=0x%08X hash=%u childOffset=%u childCount=%u context=0x%08X",
              label ? label : "",
              static_cast<unsigned>(frame),
              UIMgr::GetFrameHash(frame),
              UIMgr::GetChildOffsetId(frame),
              UIMgr::GetChildFrameCount(frame),
              static_cast<unsigned>(UIMgr::GetFrameContext(frame)));
    return frame;
}

uintptr_t ResolveMerchantRowClickTarget(uintptr_t itemRowFrame, ConsumableHarnessClickMode clickMode) {
    if (itemRowFrame < 0x10000) return 0;
    if (clickMode == ConsumableHarnessClickMode::RowChild0Only) {
        return UIMgr::GetChildFrameByIndex(itemRowFrame, 0u);
    }
    if (clickMode == ConsumableHarnessClickMode::RowChild1Only) {
        return UIMgr::GetChildFrameByIndex(itemRowFrame, 1u);
    }
    return itemRowFrame;
}

void DumpFrameChildren(uintptr_t frame, const char* label, uint32_t maxChildren = 16) {
    IntReport("  Frame children dump %s: frame=0x%08X hash=%u childCount=%u context=0x%08X",
              label ? label : "",
              static_cast<unsigned>(frame),
              UIMgr::GetFrameHash(frame),
              UIMgr::GetChildFrameCount(frame),
              static_cast<unsigned>(UIMgr::GetFrameContext(frame)));
    const uint32_t childCount = UIMgr::GetChildFrameCount(frame);
    const uint32_t capped = (childCount < maxChildren) ? childCount : maxChildren;
    for (uint32_t i = 0; i < capped; ++i) {
        const uintptr_t child = UIMgr::GetChildFrameByIndex(frame, i);
        IntReport("    [%u] frame=0x%08X hash=%u state=0x%X frameId=%u childOffset=%u context=0x%08X",
                  i,
                  static_cast<unsigned>(child),
                  UIMgr::GetFrameHash(child),
                  UIMgr::GetFrameState(child),
                  UIMgr::GetFrameId(child),
                  UIMgr::GetChildOffsetId(child),
                  static_cast<unsigned>(UIMgr::GetFrameContext(child)));
    }
}

void AppendConsumableFrameDumpLine(const char* line) {
    if (!line) return;
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&AppendConsumableFrameDumpLine), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "consumable_harness_frame_dump.txt");
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
    CloseHandle(h);
}

void AppendConsumableFrameDumpMarker(const char* label) {
    char line[256];
    const DWORD pid = GetCurrentProcessId();
    static const DWORD runTag = GetTickCount();
    sprintf_s(line, "pid=%lu run_tag=%lu marker=%s\r\n", pid, runTag, label ? label : "(null)");
    AppendConsumableFrameDumpLine(line);
}

void DumpConsumableFrameSummary(const char* label, uintptr_t frame) {
    char line[512];
    if (frame < 0x10000) {
        sprintf_s(line, "  %s frame=0x00000000\r\n", label ? label : "(null)");
        AppendConsumableFrameDumpLine(line);
        return;
    }
    const uint32_t childCount = UIMgr::GetChildFrameCount(frame);
    sprintf_s(line,
              "  %s frame=0x%08X hash=%u state=0x%X frameId=%u childOffset=%u childCount=%u context=0x%08X\r\n",
              label ? label : "(null)",
              static_cast<unsigned>(frame),
              UIMgr::GetFrameHash(frame),
              UIMgr::GetFrameState(frame),
              UIMgr::GetFrameId(frame),
              UIMgr::GetChildOffsetId(frame),
              childCount,
              static_cast<unsigned>(UIMgr::GetFrameContext(frame)));
    AppendConsumableFrameDumpLine(line);
}

void DumpConsumableFrameTree(uintptr_t merchantFrame, uint32_t merchantItemPosition, uint32_t targetModelId, uint32_t targetItemId) {
    const DWORD pid = GetCurrentProcessId();
    static const DWORD runTag = GetTickCount();
    char line[512];
    sprintf_s(line, "pid=%lu run_tag=%lu merchant=0x%08X target_model=%u target_item=%u item_pos=%u\r\n",
              pid, runTag, static_cast<unsigned>(merchantFrame), targetModelId, targetItemId, merchantItemPosition);
    AppendConsumableFrameDumpLine(line);

    const uintptr_t merchantContext = UIMgr::GetFrameContext(merchantFrame);
    sprintf_s(line, "  merchant_context=0x%08X\r\n", static_cast<unsigned>(merchantContext));
    AppendConsumableFrameDumpLine(line);

    AppendConsumableFrameDumpMarker("dump_root_0_begin");
    const uintptr_t root0 = UIMgr::GetChildFrameByIndex(merchantFrame, 0u);
    DumpConsumableFrameSummary("root[0]", root0);

    AppendConsumableFrameDumpMarker("dump_root_0_1_begin");
    const uintptr_t root01 = UIMgr::GetChildFrameByIndex(root0, 1u);
    DumpConsumableFrameSummary("root[0][1]", root01);

    const uint32_t branchIndices[] = { 2u, 3u };
    for (uint32_t branchIndex = 0; branchIndex < _countof(branchIndices); ++branchIndex) {
        const uint32_t childIndex = branchIndices[branchIndex];
        sprintf_s(line, "dump_root_0_1_%u_begin", childIndex);
        AppendConsumableFrameDumpMarker(line);

        const uintptr_t branch = UIMgr::GetChildFrameByIndex(root01, childIndex);
        char label[64];
        sprintf_s(label, "root[0][1][%u]", childIndex);
        DumpConsumableFrameSummary(label, branch);
        if (branch < 0x10000) continue;

        const uint32_t branchChildCount = UIMgr::GetChildFrameCount(branch);
        const uint32_t branchChildCapped = (branchChildCount < 8u) ? branchChildCount : 8u;
        for (uint32_t child = 0; child < branchChildCapped; ++child) {
            const uintptr_t row = UIMgr::GetChildFrameByIndex(branch, child);
            sprintf_s(label, "root[0][1][%u][%u]", childIndex, child);
            DumpConsumableFrameSummary(label, row);

            if (row < 0x10000) continue;
            const uint32_t rowChildCount = UIMgr::GetChildFrameCount(row);
            const uint32_t rowChildCapped = (rowChildCount < 4u) ? rowChildCount : 4u;
            for (uint32_t leaf = 0; leaf < rowChildCapped; ++leaf) {
                const uintptr_t leafFrame = UIMgr::GetChildFrameByIndex(row, leaf);
                sprintf_s(label, "root[0][1][%u][%u][%u]", childIndex, child, leaf);
                DumpConsumableFrameSummary(label, leafFrame);
            }

            if (childIndex == 3u && child == 0u) {
                AppendConsumableFrameDumpMarker("dump_root_0_1_3_0_full_begin");
                const uint32_t fullChildCapped = (rowChildCount < 8u) ? rowChildCount : 8u;
                for (uint32_t fullLeaf = rowChildCapped; fullLeaf < fullChildCapped; ++fullLeaf) {
                    const uintptr_t leafFrame = UIMgr::GetChildFrameByIndex(row, fullLeaf);
                    sprintf_s(label, "root[0][1][3][0][%u]", fullLeaf);
                    DumpConsumableFrameSummary(label, leafFrame);
                }
            }
        }
    }
}

bool CraftConsumableViaUiClick(const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                               ConsumableHarnessClickMode clickMode,
                               uint32_t& beforeCount, uint32_t& afterCount,
                                char* detail, size_t detailSize) {
    if (detail && detailSize) detail[0] = '\0';

    WriteConsumableHarnessStatus("ui_probe_start", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 0, 0, 1, "resolving_merchant_frames");

    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(kMerchantRootHash);
    {
        char probeDetail[192] = {};
        sprintf_s(probeDetail, "merchantFrame=0x%08X", static_cast<unsigned>(merchantFrame));
        WriteConsumableHarnessStatus("ui_probe_merchant_frame", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     0, 0, merchantFrame >= 0x10000 ? 1u : 0u, probeDetail);
    }
    const uintptr_t merchantContext = UIMgr::GetFrameContext(merchantFrame);
    const uint32_t merchantItemPosition = FindMerchantItemPositionByModelId(targetModelId);
    {
        char probeDetail[192] = {};
        sprintf_s(probeDetail, "merchantContext=0x%08X itemPos=%u",
                  static_cast<unsigned>(merchantContext),
                  merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition);
        WriteConsumableHarnessStatus("ui_probe_item_position", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     0, 0, merchantItemPosition != UINT32_MAX ? 1u : 0u, probeDetail);
    }
    // AutoIt's proven working item selection: NavigateFramePath("0,0,itemIndex")
    // itemIndex is 0-based. merchantItemPosition is 1-based, so subtract 1.
    const uint32_t autoit_itemIndex = (merchantItemPosition == UINT32_MAX || merchantItemPosition == 0u) ? 0u : (merchantItemPosition - 1u);
    const uint32_t autoitItemPath[] = { 0u, 0u, autoit_itemIndex };
    const uint32_t actionButtonPath[] = { 0u, 1u, 1u };
    uintptr_t itemRowFrame = merchantItemPosition == UINT32_MAX
        ? 0u
        : ResolveMerchantSortedPathFrame(merchantFrame, autoitItemPath, _countof(autoitItemPath), "item[0,0,index] (AutoIt)");
    if (itemRowFrame < 0x10000 && merchantItemPosition != UINT32_MAX) {
        // Fallback to old path
        const uint32_t itemRowPath[] = { 0u, 1u, 3u, 0u, autoit_itemIndex };
        itemRowFrame = ResolveMerchantSortedPathFrame(merchantFrame, itemRowPath, _countof(itemRowPath), "item[0,1,3,0,index-1]-fallback");
    }
    const uintptr_t rowClickFrame = ResolveMerchantRowClickTarget(itemRowFrame, clickMode);
    const uintptr_t pathActionFrame = ResolveMerchantSortedPathFrame(merchantFrame, actionButtonPath, _countof(actionButtonPath), "action[0,1,1]");
    const uintptr_t actionPrimaryByContext = merchantContext >= 0x10000
        ? UIMgr::GetFrameByContextAndChildOffset(merchantContext, 125u, merchantFrame)
        : 0u;
    const uintptr_t actionAltByContext = merchantContext >= 0x10000
        ? UIMgr::GetFrameByContextAndChildOffset(merchantContext, 126u, merchantFrame)
        : 0u;
    // action125 (childOffset 125 in merchant context) produces CtoS 0x049 when clicked.
    // pathActionFrame {0,1,1} has wrong sub-context and crashes on immediate click.
    // Use action125 via GameThread dispatch (ButtonClick, not ButtonClickImmediate).
    const uintptr_t craftButtonFrame = actionPrimaryByContext
        ? actionPrimaryByContext
        : (pathActionFrame
            ? pathActionFrame
            : (actionAltByContext ? actionAltByContext : UIMgr::GetFrameByHash(kMerchantActionButtonAltHash)));
    {
        char probeDetail[256] = {};
        sprintf_s(probeDetail,
                  "row=0x%08X rowClick=0x%08X actionPath=0x%08X action125=0x%08X action126=0x%08X craftButton=0x%08X",
                  static_cast<unsigned>(itemRowFrame),
                  static_cast<unsigned>(rowClickFrame),
                  static_cast<unsigned>(pathActionFrame),
                  static_cast<unsigned>(actionPrimaryByContext),
                  static_cast<unsigned>(actionAltByContext),
                  static_cast<unsigned>(craftButtonFrame));
        WriteConsumableHarnessStatus("ui_probe_frames_resolved", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     0, 0, 1, probeDetail);
    }

    const uintptr_t rowContext = UIMgr::GetFrameContext(itemRowFrame);
    const uintptr_t rowClickContext = UIMgr::GetFrameContext(rowClickFrame);
    IntReport("  UI craft probe for %s: clickMode=%s merchantFrame=0x%08X context=0x%08X row=0x%08X rowHash=%u rowChildOffset=%u rowContext=0x%08X rowClick=0x%08X rowClickHash=%u rowClickChildOffset=%u rowClickContext=0x%08X actionPath=0x%08X action125=0x%08X action126=0x%08X craftButton=0x%08X targetModel=%u item=%u itemPos=%u",
              targetLabel ? targetLabel : "",
              DescribeConsumableHarnessClickMode(clickMode),
              static_cast<unsigned>(merchantFrame),
              static_cast<unsigned>(merchantContext),
              static_cast<unsigned>(itemRowFrame),
              UIMgr::GetFrameHash(itemRowFrame),
              UIMgr::GetChildOffsetId(itemRowFrame),
              static_cast<unsigned>(rowContext),
              static_cast<unsigned>(rowClickFrame),
              UIMgr::GetFrameHash(rowClickFrame),
              UIMgr::GetChildOffsetId(rowClickFrame),
              static_cast<unsigned>(rowClickContext),
              static_cast<unsigned>(pathActionFrame),
              static_cast<unsigned>(actionPrimaryByContext),
              static_cast<unsigned>(actionAltByContext),
              static_cast<unsigned>(craftButtonFrame),
              targetModelId,
              targetItemId,
              merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition);

    // Avoid broad frame-dump traversal during live craft runs; it has been a
    // recurring crash source while the harness is trying to reach the row click.

    ConsumableMaterialCounter materialsBefore[7]{};
    FillConsumableMaterialCounters(materialsBefore);
    beforeCount = CountInventoryModelQuantity(targetModelId);

    char materialDetail[512] = {};
    FormatConsumableMaterialSnapshot(materialDetail, sizeof(materialDetail), materialsBefore, materialsBefore);
    WriteConsumableHarnessStatus("material_snapshot_before", targetLabel, ReadMapId(), 0,
                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, materialDetail);

    bool rowClicked = false;
    if (itemRowFrame >= 0x10000) {
        if (detail && detailSize) {
            sprintf_s(detail, detailSize,
                      "row_click_start mode=%s row=0x%08X rowClick=0x%08X itemPos=%u actionPath=0x%08X action125=0x%08X action126=0x%08X before=%u",
                      DescribeConsumableHarnessClickMode(clickMode),
                      static_cast<unsigned>(itemRowFrame),
                      static_cast<unsigned>(rowClickFrame),
                      merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition,
                      static_cast<unsigned>(pathActionFrame),
                      static_cast<unsigned>(actionPrimaryByContext),
                      static_cast<unsigned>(actionAltByContext),
                      beforeCount);
            WriteConsumableHarnessStatus("row_click_start", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, beforeCount, 1, detail);
        }
        rowClicked = UIMgr::ButtonClick(rowClickFrame ? rowClickFrame : itemRowFrame);
        Sleep(500 + ChatMgr::GetPing());
        const uint32_t afterRowCount = CountInventoryModelQuantity(targetModelId);
        if (detail && detailSize) {
            sprintf_s(detail, detailSize,
                      "row_click_complete rowClicked=%u mode=%s row=0x%08X rowClick=0x%08X afterRow=%u",
                      rowClicked ? 1u : 0u,
                      DescribeConsumableHarnessClickMode(clickMode),
                      static_cast<unsigned>(itemRowFrame),
                      static_cast<unsigned>(rowClickFrame),
                      afterRowCount);
            WriteConsumableHarnessStatus("row_click_complete", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, afterRowCount, rowClicked ? 1u : 0u, detail);
        }
    }
    if (!rowClicked) {
        afterCount = CountInventoryModelQuantity(targetModelId);
        char rowFailDetail[256] = {};
        sprintf_s(rowFailDetail,
                  "row_click_failed mode=%s row=0x%08X rowClick=0x%08X itemPos=%u merchantFrame=0x%08X actionPath=0x%08X action125=0x%08X action126=0x%08X",
                  DescribeConsumableHarnessClickMode(clickMode),
                  static_cast<unsigned>(itemRowFrame),
                  static_cast<unsigned>(rowClickFrame),
                  merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition,
                  static_cast<unsigned>(merchantFrame),
                  static_cast<unsigned>(pathActionFrame),
                  static_cast<unsigned>(actionPrimaryByContext),
                  static_cast<unsigned>(actionAltByContext));
        WriteConsumableHarnessStatus("row_click_failed", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, afterCount, 0, rowFailDetail);

        // Row resolution via NavigateSortedChildPath failed. Try walking the merchant
        // frame's child tree directly (GWA2 style) to find and click the item row,
        // then click the Craft button.
        const bool allowDirectWalkFallback =
            merchantItemPosition != UINT32_MAX
            && merchantFrame >= 0x10000
            && clickMode != ConsumableHarnessClickMode::RowOnly
            && clickMode != ConsumableHarnessClickMode::RowChild0Only
            && clickMode != ConsumableHarnessClickMode::RowChild1Only;
        if (allowDirectWalkFallback) {
            WriteConsumableHarnessStatus("direct_walk_fallback_start", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, afterCount, 1, rowFailDetail);

            // First try the UIMessage TransactItem path directly — this doesn't
            // need row selection or the Craft button. It sends the crafter transaction
            // via the game's internal UIMessage dispatch.
            {
                const bool nativeCrafted = CraftConsumableNatively(
                    targetLabel, targetModelId, targetItemId, merchantItemPosition, beforeCount, afterCount, detail, detailSize);
                if (nativeCrafted) return true;
            }

            // If UIMessage TransactItem failed, try clicking the Craft button directly.
            // Try both action125 and action126 since we're not sure which is Craft vs Goodbye.
            const uintptr_t candidates[] = { actionPrimaryByContext, actionAltByContext };
            const char* candidateLabels[] = { "action125", "action126" };
            for (uint32_t ci = 0; ci < 2; ++ci) {
                const uintptr_t craftBtn = candidates[ci];
                if (craftBtn < 0x10000 || UIMgr::IsFrameHidden(craftBtn)) continue;

                const uint32_t goldBefore2 = ItemMgr::GetGoldCharacter();
                MerchantStoCTap tap2{};
                StartMerchantStoCTap(tap2);
                CtoS::ResetPacketTap();

                const bool craftClicked2 = UIMgr::ButtonClick(craftBtn);
                IntReport("  Fallback %s click: frame=0x%08X hash=%u clicked=%u gold=%u",
                          candidateLabels[ci], static_cast<unsigned>(craftBtn),
                          UIMgr::GetFrameHash(craftBtn), craftClicked2 ? 1u : 0u, goldBefore2);

                if (craftClicked2) {
                    WriteConsumableHarnessStatus("fallback_craft_clicked", targetLabel, ReadMapId(), 0,
                                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                                 beforeCount, beforeCount, 1, candidateLabels[ci]);
                    const bool craftObserved2 = WaitFor("fallback craft result", 3000, [targetModelId, beforeCount, goldBefore2]() {
                        return CountInventoryModelQuantity(targetModelId) > beforeCount
                            || ItemMgr::GetGoldCharacter() < goldBefore2;
                    });
                    afterCount = CountInventoryModelQuantity(targetModelId);
                    const uint32_t goldAfter2 = ItemMgr::GetGoldCharacter();
                    char stoCSummary2[256] = {};
                    char ctoSSummary2[256] = {};
                    FormatMerchantStoCTapSummary(stoCSummary2, sizeof(stoCSummary2), tap2);
                    FormatCtoSPacketTapSummary(ctoSSummary2, sizeof(ctoSSummary2), CtoS::GetPacketTapSnapshot());
                    StopMerchantStoCTap(tap2);
                    if (detail && detailSize) {
                        sprintf_s(detail, detailSize,
                                  "fallback_%s observed=%u before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
                                  candidateLabels[ci], craftObserved2 ? 1u : 0u,
                                  beforeCount, afterCount, goldBefore2, goldAfter2, stoCSummary2, ctoSSummary2);
                    }
                    WriteConsumableHarnessStatus("fallback_craft_result", targetLabel, ReadMapId(), 0,
                                                 TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                                 beforeCount, afterCount, afterCount > beforeCount ? 1u : 0u,
                                                 detail ? detail : "");
                    if (afterCount > beforeCount) {
                        IntReport("  FALLBACK %s CRAFT SUCCESS: before=%u after=%u gold=%u->%u",
                                  candidateLabels[ci], beforeCount, afterCount, goldBefore2, goldAfter2);
                        return true;
                    }
                    IntReport("  Fallback %s: no delta (before=%u after=%u gold=%u->%u stoC=%s ctoS=%s)",
                              candidateLabels[ci], beforeCount, afterCount, goldBefore2, goldAfter2, stoCSummary2, ctoSSummary2);
                } else {
                    StopMerchantStoCTap(tap2);
                }
            }
        }

        if (detail && detailSize) {
            sprintf_s(detail, detailSize,
                      "ui rowClicked=0 craftClicked=0 mode=%s row=0x%08X rowClick=0x%08X itemPos=%u before=%u after=%u",
                      DescribeConsumableHarnessClickMode(clickMode),
                      static_cast<unsigned>(itemRowFrame),
                      static_cast<unsigned>(rowClickFrame),
                      merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition,
                      beforeCount,
                      afterCount);
        }
        return false;
    }

    bool craftClicked = false;
    if (clickMode == ConsumableHarnessClickMode::RowOnly
        || clickMode == ConsumableHarnessClickMode::RowChild0Only
        || clickMode == ConsumableHarnessClickMode::RowChild1Only) {
        afterCount = CountInventoryModelQuantity(targetModelId);
        if (detail && detailSize) {
            sprintf_s(detail, detailSize,
                      "ui rowClicked=%u craftClicked=0 mode=%s frame=0x%08X row=0x%08X rowClick=0x%08X itemPos=%u actionPath=0x%08X action125=0x%08X action126=0x%08X before=%u after=%u",
                      rowClicked ? 1u : 0u,
                      DescribeConsumableHarnessClickMode(clickMode),
                      static_cast<unsigned>(merchantFrame),
                      static_cast<unsigned>(itemRowFrame),
                      static_cast<unsigned>(rowClickFrame),
                      merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition,
                      static_cast<unsigned>(pathActionFrame),
                      static_cast<unsigned>(actionPrimaryByContext),
                      static_cast<unsigned>(actionAltByContext),
                      beforeCount,
                      afterCount);
        }
        return rowClicked;
    }

    // Consumable crafter flow: row is selected, now click the "Craft" button.
    // There is no quote step — the crafter UI is "select item, click Craft".
    // The craft button is at merchant frame path {0,1,1} (same as GWA2 FrameUI).
    {
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        MerchantStoCTap tap{};
        StartMerchantStoCTap(tap);
        CtoS::ResetPacketTap();

        // Click the Craft button — force-click even if marked hidden.
        // AutoIt's ClickFrameByPtr doesn't check hidden state and works.
        // The "hidden" flag may not mean visually hidden in crafter context.
        const uintptr_t craftTarget = craftButtonFrame;
        bool craftBtnClicked = false;
        if (craftTarget >= 0x10000) {
            const bool isHidden = UIMgr::IsFrameHidden(craftTarget);
            IntReport("  Craft button: frame=0x%08X hash=%u hidden=%u — clicking via GameThread",
                      static_cast<unsigned>(craftTarget), UIMgr::GetFrameHash(craftTarget), isHidden ? 1u : 0u);
            craftBtnClicked = UIMgr::ButtonClick(craftTarget);
            IntReport("  Craft button click result: clicked=%u", craftBtnClicked ? 1u : 0u);
        } else {
            IntReport("  Craft button not resolved: frame=0x%08X", static_cast<unsigned>(craftTarget));
        }

        if (craftBtnClicked) {
            WriteConsumableHarnessStatus("craft_button_clicked", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, beforeCount, 1, "craft_button_clicked");

            // Wait for inventory or gold change
            const bool craftObserved = WaitFor("craft button result", 3000, [targetModelId, beforeCount, goldBefore]() {
                return CountInventoryModelQuantity(targetModelId) > beforeCount
                    || ItemMgr::GetGoldCharacter() < goldBefore;
            });
            afterCount = CountInventoryModelQuantity(targetModelId);
            const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
            char stoCSummary[256] = {};
            char ctoSSummary[256] = {};
            FormatMerchantStoCTapSummary(stoCSummary, sizeof(stoCSummary), tap);
            FormatCtoSPacketTapSummary(ctoSSummary, sizeof(ctoSSummary), CtoS::GetPacketTapSnapshot());
            StopMerchantStoCTap(tap);

            if (detail && detailSize) {
                sprintf_s(detail, detailSize,
                          "craft_button_result observed=%u before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
                          craftObserved ? 1u : 0u, beforeCount, afterCount, goldBefore, goldAfter, stoCSummary, ctoSSummary);
            }
            WriteConsumableHarnessStatus("craft_button_result", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, afterCount, afterCount > beforeCount ? 1u : 0u,
                                         detail ? detail : "");

            if (afterCount > beforeCount) {
                IntReport("  CRAFT SUCCESS: before=%u after=%u gold=%u->%u", beforeCount, afterCount, goldBefore, goldAfter);
                return true;
            }
            IntReport("  Craft button clicked but no delta: before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
                      beforeCount, afterCount, goldBefore, goldAfter, stoCSummary, ctoSSummary);
        } else {
            StopMerchantStoCTap(tap);
        }
    }

    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "native_packet_fallback mode=%s actionPath=0x%08X pathHidden=%u action125=0x%08X hidden125=%u action126=0x%08X hidden126=%u",
                  DescribeConsumableHarnessClickMode(clickMode),
                  static_cast<unsigned>(pathActionFrame),
                  (pathActionFrame >= 0x10000 && UIMgr::IsFrameHidden(pathActionFrame)) ? 1u : 0u,
                  static_cast<unsigned>(actionPrimaryByContext),
                  (actionPrimaryByContext >= 0x10000 && UIMgr::IsFrameHidden(actionPrimaryByContext)) ? 1u : 0u,
                  static_cast<unsigned>(actionAltByContext),
                  (actionAltByContext >= 0x10000 && UIMgr::IsFrameHidden(actionAltByContext)) ? 1u : 0u);
        WriteConsumableHarnessStatus("native_packet_fallback", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, beforeCount, 1, detail);
    }

    const auto clickAction = [&](uintptr_t frame, const char* stageLabel) -> bool {
        if (frame < 0x10000 || UIMgr::IsFrameHidden(frame)) {
            char localDetail[192] = {};
            sprintf_s(localDetail, "action_click_skipped stage=%s frame=0x%08X hidden=%u hash=%u state=0x%X mode=%s",
                      stageLabel ? stageLabel : "",
                      static_cast<unsigned>(frame),
                      (frame >= 0x10000 && UIMgr::IsFrameHidden(frame)) ? 1u : 0u,
                      UIMgr::GetFrameHash(frame),
                      UIMgr::GetFrameState(frame),
                      DescribeConsumableHarnessClickMode(clickMode));
            WriteConsumableHarnessStatus("action_click_skipped", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, beforeCount, 0, localDetail);
            return false;
        }
        char localDetail[160] = {};
        sprintf_s(localDetail, "action_click_start stage=%s frame=0x%08X mode=%s", stageLabel,
                  static_cast<unsigned>(frame), DescribeConsumableHarnessClickMode(clickMode));
        WriteConsumableHarnessStatus("action_click_start", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, beforeCount, 1, localDetail);
        const bool clicked = UIMgr::ButtonClick(frame);
        const int postClickDelaysMs[] = { 50, 150, 350 };
        int accumulatedDelay = 0;
        for (int delayMs : postClickDelaysMs) {
            const int sleepMs = delayMs - accumulatedDelay;
            if (sleepMs > 0) Sleep(sleepMs);
            accumulatedDelay = delayMs;

            const uintptr_t merchantFrameNow = UIMgr::GetFrameByHash(kMerchantRootHash);
            const uintptr_t quantityPromptFrame = UIMgr::GetVisibleFrameByChildOffsetAndChildCount(
                kTradeQuantityPromptChildOffsetId, 1u, 16u, merchantFrameNow);
            const uintptr_t altActionNow = UIMgr::GetFrameByContextAndChildOffset(
                merchantFrameNow >= 0x10000 ? UIMgr::GetFrameContext(merchantFrameNow) : 0u,
                126u,
                merchantFrameNow);
            char probeDetail[256] = {};
            sprintf_s(probeDetail,
                      "post_action_probe stage=%s t=%d clicked=%u merchant=0x%08X items=%u qtyPrompt=0x%08X qtyChildCount=%u altAction=0x%08X altHidden=%u",
                      stageLabel ? stageLabel : "",
                      delayMs,
                      clicked ? 1u : 0u,
                      static_cast<unsigned>(merchantFrameNow),
                      TradeMgr::GetMerchantItemCount(),
                      static_cast<unsigned>(quantityPromptFrame),
                      UIMgr::GetChildFrameCount(quantityPromptFrame),
                      static_cast<unsigned>(altActionNow),
                      (altActionNow >= 0x10000 && UIMgr::IsFrameHidden(altActionNow)) ? 1u : 0u);
            WriteConsumableHarnessStatus("post_action_probe", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId),
                                         quantityPromptFrame >= 0x10000 ? 1u : 0u, probeDetail);

            if (quantityPromptFrame >= 0x10000) {
                char dumpLabel[64] = {};
                sprintf_s(dumpLabel, "consumable_post_%s_t%d_qty_prompt", stageLabel ? stageLabel : "action", delayMs);
                UIMgr::DebugDumpChildFrames(quantityPromptFrame, dumpLabel, 12);
            } else {
                char dumpLabel[64] = {};
                sprintf_s(dumpLabel, "consumable_post_%s_t%d_visible_child2", stageLabel ? stageLabel : "action", delayMs);
                UIMgr::DebugDumpVisibleFramesByChildOffset(kTradeQuantityPromptChildOffsetId, dumpLabel, 12);
            }
        }
        const bool quantityPromptOpen = TradeMgr::IsTradeQuantityPromptOpen();
        if (quantityPromptOpen) {
            const uint32_t promptFrameBeforeConfirm = TradeMgr::GetTradeQuantityPromptFrame();
            const uint32_t promptChildCountBeforeConfirm = TradeMgr::GetTradeQuantityPromptChildCount();
            char confirmDetail[256] = {};
            sprintf_s(confirmDetail,
                      "quantity_prompt_confirm_start stage=%s frame=0x%08X childCount=%u",
                      stageLabel ? stageLabel : "",
                      promptFrameBeforeConfirm,
                      promptChildCountBeforeConfirm);
            WriteConsumableHarnessStatus("quantity_prompt_confirm_start", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), 1, confirmDetail);
            char promptDetail[768] = {};
            FormatPromptChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm);
            WriteConsumableHarnessStatus("quantity_prompt_children_before_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);
            FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm, 4u);
            WriteConsumableHarnessStatus("quantity_prompt_child4_before_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);
            FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm, 2u);
            WriteConsumableHarnessStatus("quantity_prompt_child2_before_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);
            FormatPromptNestedGrandchildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm, 2u, 0u);
            WriteConsumableHarnessStatus("quantity_prompt_child2_0_before_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);
            FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm, 5u);
            WriteConsumableHarnessStatus("quantity_prompt_child5_before_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);

            bool confirmed = ConfirmCrafterQuantityPromptOneDirect(targetLabel, targetModelId, targetItemId, beforeCount);
            if (!confirmed) {
                confirmed = TradeMgr::ConfirmTradeQuantityPromptValue(1u);
            }
            sprintf_s(confirmDetail,
                      "quantity_prompt_confirm_value_complete stage=%s confirmed=%u quantity=1 frame=0x%08X childCount=%u inventory=%u",
                      stageLabel ? stageLabel : "",
                      confirmed ? 1u : 0u,
                      TradeMgr::GetTradeQuantityPromptFrame(),
                      TradeMgr::GetTradeQuantityPromptChildCount(),
                      CountInventoryModelQuantity(targetModelId));
            WriteConsumableHarnessStatus("quantity_prompt_confirm_value_complete", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, confirmDetail);
            if (!confirmed) {
                confirmed = TradeMgr::ConfirmTradeQuantityPromptMax();
            }
            Sleep(250 + ChatMgr::GetPing());
            const uint32_t promptFrameAfterConfirm = TradeMgr::GetTradeQuantityPromptFrame();
            const uint32_t promptChildCountAfterConfirm = TradeMgr::GetTradeQuantityPromptChildCount();
            ConsumableMaterialCounter materialsAfterConfirm[7]{};
            FillConsumableMaterialCounters(materialsAfterConfirm);
            sprintf_s(confirmDetail,
                      "quantity_prompt_confirm_complete stage=%s confirmed=%u frameAfter=0x%08X childCountAfter=%u inventory=%u",
                      stageLabel ? stageLabel : "",
                      confirmed ? 1u : 0u,
                      promptFrameAfterConfirm,
                      promptChildCountAfterConfirm,
                      CountInventoryModelQuantity(targetModelId));
            WriteConsumableHarnessStatus("quantity_prompt_confirm_complete", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, confirmDetail);
            FormatPromptChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm);
            WriteConsumableHarnessStatus("quantity_prompt_children_after_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
            FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm, 4u);
            WriteConsumableHarnessStatus("quantity_prompt_child4_after_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
            FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm, 2u);
            WriteConsumableHarnessStatus("quantity_prompt_child2_after_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
            FormatPromptNestedGrandchildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm, 2u, 0u);
            WriteConsumableHarnessStatus("quantity_prompt_child2_0_after_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
            FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm, 5u);
            WriteConsumableHarnessStatus("quantity_prompt_child5_after_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
            FormatConsumableMaterialSnapshot(materialDetail, sizeof(materialDetail), materialsBefore, materialsAfterConfirm);
            WriteConsumableHarnessStatus("material_snapshot_after_confirm", targetLabel, ReadMapId(), 0,
                                         TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, materialDetail);
        }
        Sleep((900 + ChatMgr::GetPing()) - accumulatedDelay);
        const uint32_t observedCount = CountInventoryModelQuantity(targetModelId);
        ConsumableMaterialCounter materialsAfterAction[7]{};
        FillConsumableMaterialCounters(materialsAfterAction);
        FormatConsumableMaterialSnapshot(materialDetail, sizeof(materialDetail), materialsBefore, materialsAfterAction);
        WriteConsumableHarnessStatus("material_snapshot_after_action", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, observedCount, clicked ? 1u : 0u, materialDetail);
        sprintf_s(localDetail, "action_click_complete stage=%s clicked=%u frame=0x%08X mode=%s after=%u",
                  stageLabel, clicked ? 1u : 0u, static_cast<unsigned>(frame),
                  DescribeConsumableHarnessClickMode(clickMode), observedCount);
        WriteConsumableHarnessStatus("action_click_complete", targetLabel, ReadMapId(), 0,
                                     TradeMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, observedCount, clicked ? 1u : 0u, localDetail);
        return clicked;
    };

    if (clickMode == ConsumableHarnessClickMode::RootOnly || clickMode == ConsumableHarnessClickMode::Both) {
        craftClicked = clickAction(actionPrimaryByContext, "action125") || craftClicked;
    }
    if (clickMode == ConsumableHarnessClickMode::PathOnly) {
        craftClicked = clickAction(pathActionFrame, "actionPath") || craftClicked;
    }
    if (clickMode == ConsumableHarnessClickMode::AltOnly || clickMode == ConsumableHarnessClickMode::Both) {
        craftClicked = clickAction(actionAltByContext ? actionAltByContext : craftButtonFrame, "action126") || craftClicked;
    }

    afterCount = CountInventoryModelQuantity(targetModelId);
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "ui rowClicked=%u craftClicked=%u frame=0x%08X row=0x%08X rowClick=0x%08X itemPos=%u actionPath=0x%08X action125=0x%08X action126=0x%08X craftButton=0x%08X before=%u after=%u",
                  rowClicked ? 1u : 0u,
                  craftClicked ? 1u : 0u,
                  static_cast<unsigned>(merchantFrame),
                  static_cast<unsigned>(itemRowFrame),
                  static_cast<unsigned>(rowClickFrame),
                  merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition,
                  static_cast<unsigned>(pathActionFrame),
                  static_cast<unsigned>(actionPrimaryByContext),
                  static_cast<unsigned>(actionAltByContext),
                  static_cast<unsigned>(craftButtonFrame),
                  beforeCount,
                  afterCount);
    }
    return craftClicked;
}

size_t CollectNearestNpcLikeAgentsToCoords(float targetX, float targetY, float maxDistance, uint32_t* outIds, size_t capacity) {
    if (!outIds || capacity == 0 || Offsets::AgentBase <= 0x10000) return 0;

    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return 0;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    if (maxAgents == 0) return 0;

    struct Candidate {
        uint32_t id = 0;
        float distSq = 0.0f;
    };
    Candidate candidates[32]{};
    size_t count = 0;
    const float maxDistSq = maxDistance * maxDistance;
    const uint32_t myId = ReadMyId();

    for (uint32_t i = 1; i < maxAgents && i < 4096; ++i) {
        if (i == myId) continue;
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000) continue;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if (base->type != 0xDB) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->hp <= 0.0f) continue;
        if (living->allegiance != 6) continue;

        const float distSq = AgentMgr::GetSquaredDistance(targetX, targetY, living->x, living->y);
        if (distSq > maxDistSq) continue;
        if (count >= _countof(candidates)) break;

        candidates[count].id = i;
        candidates[count].distSq = distSq;
        count++;
    }

    for (size_t i = 0; i < count; ++i) {
        size_t best = i;
        for (size_t j = i + 1; j < count; ++j) {
            if (candidates[j].distSq < candidates[best].distSq) best = j;
        }
        if (best != i) {
            Candidate tmp = candidates[i];
            candidates[i] = candidates[best];
            candidates[best] = tmp;
        }
    }

    const size_t emit = count < capacity ? count : capacity;
    for (size_t i = 0; i < emit; ++i) {
        outIds[i] = candidates[i].id;
    }
    return emit;
}

void DumpNpcLikeAgentsNearCoords(float targetX, float targetY, float maxDistance, size_t limit) {
    if (Offsets::AgentBase <= 0x10000) return;

    uintptr_t agentArr = *reinterpret_cast<uintptr_t*>(Offsets::AgentBase);
    if (agentArr <= 0x10000) return;

    const uint32_t maxAgents = *reinterpret_cast<uint32_t*>(Offsets::AgentBase + 0x8);
    const float maxDistSq = maxDistance * maxDistance;
    size_t emitted = 0;

    for (uint32_t i = 1; i < maxAgents && i < 4096 && emitted < limit; ++i) {
        uintptr_t agentPtr = *reinterpret_cast<uintptr_t*>(agentArr + i * 4);
        if (agentPtr <= 0x10000) continue;

        auto* base = reinterpret_cast<Agent*>(agentPtr);
        if (base->type != 0xDB) continue;

        auto* living = reinterpret_cast<AgentLiving*>(agentPtr);
        if (living->hp <= 0.0f) continue;
        if (i == ReadMyId()) continue;
        if (living->allegiance != 6) continue;

        const float distSq = AgentMgr::GetSquaredDistance(targetX, targetY, living->x, living->y);
        if (distSq > maxDistSq) continue;

        IntReport("    NPC cand id=%u dist=%.0f allegiance=%u player=%u npc_id=%u pos=(%.0f, %.0f)",
                  i,
                  sqrtf(distSq),
                  living->allegiance,
                  living->player_number,
                  living->transmog_npc_id,
                  living->x,
                  living->y);
        emitted++;
    }
}

} // namespace

bool TestNpcDialog() {
    IntReport("=== GWA3-032 slice: NPC + Dialog ===");

    if (ReadMapId() == 0 || ReadMyId() == 0) {
        IntSkip("NPC dialog", "Not in game");
        return false;
    }

    const uint32_t targetId = FindNearbyNpcLikeAgent(20000.0f);
    if (!targetId) {
        IntSkip("NPC interaction", "No nearby NPC-like living agent found");
        IntReport("");
        return false;
    }

    auto* agent = static_cast<AgentLiving*>(AgentMgr::GetAgentByID(targetId));
    IntReport("  Interacting with agent %u (allegiance=%u, player_number=%u, npc_id=%u)...",
              targetId,
              agent ? agent->allegiance : 0,
              agent ? agent->player_number : 0,
              agent ? agent->transmog_npc_id : 0);
    float npcX = 0.0f;
    float npcY = 0.0f;
    if (TryReadAgentPosition(targetId, npcX, npcY)) {
        const bool nearNpc = MovePlayerNear(npcX, npcY, 120.0f, 12000);
        float meX = 0.0f;
        float meY = 0.0f;
        TryReadAgentPosition(ReadMyId(), meX, meY);
        IntReport("  NPC pre-hook approach: near=%d player=(%.0f, %.0f) npc=(%.0f, %.0f) dist=%.0f",
                  nearNpc ? 1 : 0,
                  meX, meY, npcX, npcY,
                  AgentMgr::GetDistance(meX, meY, npcX, npcY));
    }

    AgentMgr::ChangeTarget(targetId);
    Sleep(250);
    DialogMgr::ResetRecentUITrace();
    const bool dialogUiObserved = DialogMgr::NPCHook(targetId, 2000u);
    Sleep(250);
    IntCheck("NPCHook sent (no crash)", true);
    IntCheck("Dialog UI message observed after NPCHook", dialogUiObserved);
    IntReport("  Dialog hook: lastUi=0x%X armed=0x%X observed=0x%X",
              DialogMgr::GetLastUIMessageId(),
              DialogMgr::GetArmedUIMessageId(),
              DialogMgr::GetObservedUIMessageId());
    ReportRecentDialogUiTrace("NPCHook");

    constexpr uint32_t DIALOG_NPC_TALK = 0x2AE6;
    IntReport("  Sending dialog 0x%X via DialogHook...", DIALOG_NPC_TALK);
    DialogMgr::ResetRecentUITrace();
    const bool talkUiObserved = DialogMgr::DialogHook(DIALOG_NPC_TALK, 2000u);
    Sleep(250);
    IntCheck("DialogHook sent (no crash)", true);
    IntCheck("Dialog hook captured last dialog id", DialogMgr::GetLastDialogId() == DIALOG_NPC_TALK);
    IntReport("  Dialog hook: talkUiObserved=%d lastUi=0x%X armed=0x%X observed=0x%X lastDialogId=0x%X",
              talkUiObserved ? 1 : 0,
              DialogMgr::GetLastUIMessageId(),
              DialogMgr::GetArmedUIMessageId(),
              DialogMgr::GetObservedUIMessageId(),
              DialogMgr::GetLastDialogId());
    ReportRecentDialogUiTrace("DialogHook");

    GameThread::Enqueue([]() {
        AgentMgr::CancelAction();
    });
    Sleep(500);
    IntCheck("CancelAction sent after dialog (no crash)", true);

    IntReport("");
    return true;
}

bool TestMerchantQuote() {
    IntReport("=== GWA3-032 slice: Merchant + Trader Quote ===");

    if (ReadMapId() == 0 || ReadMyId() == 0) {
        IntSkip("Merchant quote", "Not in game");
        IntReport("");
        return false;
    }

    const bool useGaddsTarget = UseGaddsMerchantTarget();
    const uint32_t targetMapId = useGaddsTarget ? kMapGadds : kMapEmbarkBeach;
    const char* targetMapLabel = useGaddsTarget ? "Gadd's Encampment" : "Embark Beach";

    if (ReadMapId() != targetMapId) {
        IntReport("  Traveling to %s (%u) for merchant-open trace...", targetMapLabel, targetMapId);
        MapMgr::Travel(targetMapId);

        const bool atTargetMap = WaitFor("MapID changes to target merchant map", 60000, [targetMapId]() {
            return ReadMapId() == targetMapId;
        });
        IntCheck("Reached merchant-open trace map", atTargetMap);
        if (!atTargetMap) {
            IntReport("");
            return false;
        }

        const bool myIdReady = WaitFor("MyID valid after travel to target merchant map", 30000, []() {
            return ReadMyId() > 0;
        });
        IntCheck("MyID valid after trader travel", myIdReady);
        if (!myIdReady) {
            IntReport("");
            return false;
        }
    }

    if (!WaitForPlayerWorldReady(10000)) {
        IntSkip("Merchant quote", "Player world state not ready");
        IntReport("");
        return false;
    }

    struct MerchantTarget {
        const char* label;
        float x;
        float y;
        bool expectQuote;
    };

    const MerchantTarget targets[] = {
        {"Gadd's merchant", kGaddsMerchantX, kGaddsMerchantY, false},
        {"consumable trader Eyja", kEmbarkEyjaX, kEmbarkEyjaY, false},
    };
    const size_t targetCount = useGaddsTarget ? 1u : _countof(targets);

    bool merchantVisible = false;
    uint32_t traderAgentId = 0;
    bool quoteExpected = false;
    const char* openedLabel = nullptr;

    const MerchantDialogVariant selectedVariant = GetMerchantDialogVariant();
    const MerchantIsolationStage isolationStage = GetMerchantIsolationStage();
    const uint32_t selectedHeader =
        (selectedVariant == MerchantDialogVariant::LegacyId || selectedVariant == MerchantDialogVariant::LegacyPtr)
            ? 0x3Au
            : 0x3Bu;
    const bool selectedUsesPtr =
        (selectedVariant == MerchantDialogVariant::StandardPtr || selectedVariant == MerchantDialogVariant::LegacyPtr);
    IntReport("  Merchant open variant: %s", DescribeMerchantDialogVariant(selectedVariant));
    IntReport("  Merchant isolation stage: %s", DescribeMerchantIsolationStage(isolationStage));
    IntReport("  Merchant interact path: native AgentMgr::InteractNPC() only (no explicit dialog)");

    for (size_t targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
        const auto& target = targets[targetIndex];
        const bool nearTarget = MovePlayerNear(target.x, target.y, 350.0f, 25000);
        IntCheck(target.expectQuote ? "Reached basic material trader area" : "Reached merchant area", nearTarget);
        if (!nearTarget) continue;

        IntReport("  Nearby NPC candidates around %s coordinates:", target.label);
        DumpNpcLikeAgentsNearCoords(target.x, target.y, 900.0f, 8);

        if (isolationStage == MerchantIsolationStage::TravelOnly) {
            IntSkip("Merchant interact/dialog", "Travel-only isolation stage");
            IntReport("");
            return true;
        }

        uint32_t candidateIds[8]{};
        size_t candidateCount = CollectNearestNpcLikeAgentsToCoords(target.x, target.y, 2500.0f, candidateIds, _countof(candidateIds));
        if (candidateCount == 0) continue;

        for (size_t candidateIndex = 0; candidateIndex < candidateCount && !merchantVisible; ++candidateIndex) {
            traderAgentId = candidateIds[candidateIndex];
            if (!traderAgentId) continue;

            float npcX = 0.0f;
            float npcY = 0.0f;
            TryReadAgentPosition(traderAgentId, npcX, npcY);
            float meX = 0.0f;
            float meY = 0.0f;
            TryReadAgentPosition(ReadMyId(), meX, meY);
            IntReport("  Candidate %u/%u: interacting with %s agent %u at (%.0f, %.0f)...",
                      static_cast<unsigned>(candidateIndex + 1),
                      static_cast<unsigned>(candidateCount),
                      target.label, traderAgentId, npcX, npcY);

            MovePlayerNear(npcX, npcY, 70.0f, 12000);
            TryReadAgentPosition(ReadMyId(), meX, meY);
            IntReport("    Player pos before interact: (%.0f, %.0f) dist=%.0f",
                      meX, meY, AgentMgr::GetDistance(meX, meY, npcX, npcY));
            ReportMerchantPreInteractState("Merchant harness pre-interact snapshot", traderAgentId, npcX, npcY);
            ReportMerchantRuntimeContext("Merchant harness runtime context");
            ReportMerchantTradeState("Merchant harness pre-interact trade state");

            if (isolationStage == MerchantIsolationStage::ApproachOnly) {
                IntSkip("Merchant target/interact/dialog", "Approach-only isolation stage");
                IntReport("");
                return true;
            }

            AgentMgr::ChangeTarget(traderAgentId);
            Sleep(250);
            IntReport("    step 0 complete: ChangeTarget(%u)", traderAgentId);
            ReportMerchantPreInteractState("Merchant harness post-target snapshot", traderAgentId, npcX, npcY);
            ReportMerchantRuntimeContext("Merchant harness post-target runtime context");
            ReportMerchantTradeState("Merchant harness post-target trade state");

            if (isolationStage == MerchantIsolationStage::TargetOnly) {
                IntSkip("Merchant interact/dialog", "Target-only isolation stage");
                IntReport("");
                return true;
            }

            if (isolationStage == MerchantIsolationStage::InteractAgentMgrOnly) {
                MerchantStoCTap tap{};
                StartMerchantStoCTap(tap);
                IntReport("    step 1: AgentMgr::InteractNPC(%u) x3 with dwell", traderAgentId);
                for (int nativeAttempt = 1; nativeAttempt <= 3; ++nativeAttempt) {
                    IntReport("      native interact attempt %d", nativeAttempt);
                    AgentMgr::InteractNPC(traderAgentId);
                    Sleep(500);
                }
                Sleep(2500);
                StopMerchantStoCTap(tap);
                IntReport("    step 1 complete after AgentMgr dwell");
                const uint32_t merchantCountAfterAgentMgr = TradeMgr::GetMerchantItemCount();
                const uintptr_t merchantFrameAfterAgentMgr = UIMgr::GetFrameByHash(kMerchantRootHash);
                IntReport("      Merchant probe after AgentMgr dwell: frame=0x%08X items=%u",
                          merchantFrameAfterAgentMgr,
                          merchantCountAfterAgentMgr);
                ReportMerchantTradeState("Merchant harness post-AgentMgr trade state");
                ReportMerchantStoCTap("Merchant harness post-AgentMgr StoC tap", tap);
                if (merchantFrameAfterAgentMgr != 0 || merchantCountAfterAgentMgr > 0) {
                    IntSkip("Merchant dialog", "AgentMgr-interact-only isolation stage");
                    IntReport("");
                    return true;
                }
                IntReport("      No merchant state observed for candidate %u; trying next candidate", traderAgentId);
                continue;
            }

            if (isolationStage == MerchantIsolationStage::InteractSinglePacketOnly ||
                isolationStage == MerchantIsolationStage::InteractPacketOnly ||
                isolationStage == MerchantIsolationStage::InteractDwellOnly) {
                MerchantStoCTap tap{};
                StartMerchantStoCTap(tap);

                const int packetAttempts =
                    (isolationStage == MerchantIsolationStage::InteractSinglePacketOnly) ? 1 : 3;
                IntReport("    step 1: raw legacy GoNPC packet 0x39 (%d attempt%s)", packetAttempts, packetAttempts == 1 ? "" : "s");
                for (int packetAttempt = 1; packetAttempt <= packetAttempts; ++packetAttempt) {
                    IntReport("      raw interact attempt %d: SendPacket(3, 0x39, %u, 0)", packetAttempt, traderAgentId);
                    CtoS::SendPacket(3, Packets::INTERACT_NPC, traderAgentId, 0u);
                    Sleep(500);
                }

                IntReport("    Raw GoNPC dwell: waiting 2500ms after legacy interact...");
                Sleep(2500);
                StopMerchantStoCTap(tap);

                const uint32_t merchantCountAfterPacket = TradeMgr::GetMerchantItemCount();
                const uintptr_t merchantFrameAfterPacket = UIMgr::GetFrameByHash(kMerchantRootHash);
                IntReport("      Merchant probe after raw GoNPC dwell: frame=0x%08X items=%u",
                          merchantFrameAfterPacket,
                          merchantCountAfterPacket);
                ReportMerchantTradeState("Merchant harness post-raw-GoNPC trade state");
                ReportMerchantStoCTap("Merchant harness post-raw-GoNPC StoC tap", tap);

                if (merchantFrameAfterPacket != 0 || merchantCountAfterPacket > 0) {
                    IntSkip("Merchant dialog", "Raw-GoNPC isolation stage");
                    IntReport("");
                    return true;
                }

                if (isolationStage == MerchantIsolationStage::InteractSinglePacketOnly) {
                    IntSkip("Merchant dialog", "Interact-single-packet-only isolation stage");
                    IntReport("");
                    return true;
                }
                if (isolationStage == MerchantIsolationStage::InteractPacketOnly) {
                    IntSkip("Merchant dialog", "Interact-packet-only isolation stage");
                    IntReport("");
                    return true;
                }
                IntSkip("Merchant dialog", "Interact-dwell-only isolation stage");
                IntReport("");
                return true;
            }

            IntReport("    step 1: AgentMgr::InteractNPC(%u) [native path]", traderAgentId);
            AgentMgr::InteractNPC(traderAgentId);
            Sleep(750);
            IntReport("    step 1 complete");

            IntReport("    Native-interact dwell: waiting 2500ms after native interact...");
            Sleep(2500);
            const uint32_t merchantCountAfterNativeInteract = TradeMgr::GetMerchantItemCount();
            const uintptr_t merchantFrameAfterNativeInteract = UIMgr::GetFrameByHash(kMerchantRootHash);
            IntReport("      Merchant probe after native-interact dwell: frame=0x%08X items=%u",
                      merchantFrameAfterNativeInteract,
                      merchantCountAfterNativeInteract);
            EmitMerchantScreenshotMarker("merchant_harness_after_native_interact_dwell");
            merchantVisible = WaitFor("merchant window visible after native interact dwell", 2500, []() {
                return UIMgr::GetFrameByHash(kMerchantRootHash) != 0 || TradeMgr::GetMerchantItemCount() > 0;
            });
            if (merchantVisible) {
                quoteExpected = target.expectQuote;
                openedLabel = target.label;
                IntReport("    Merchant context opened via native AgentMgr::InteractNPC path");
                break;
            }

            const uintptr_t traderAgentPtr = GetAgentPtrRaw(traderAgentId);
            IntReport("    Agent scalars: id=%u ptr=0x%08X", traderAgentId, traderAgentPtr);

            if (isolationStage == MerchantIsolationStage::InteractOnly) {
                IntSkip("Merchant dialog", "Interact-only isolation stage");
                IntReport("");
                return true;
            }

            IntReport("    No explicit dialog send in this experiment; re-approaching target");

            MovePlayerNear(target.x, target.y, 250.0f, 4000);
            Sleep(500);
        }

        if (merchantVisible) break;
    }

    const uint32_t merchantCount = TradeMgr::GetMerchantItemCount();
    IntReport("  Merchant window visible=%s, item count=%u",
              merchantVisible ? "true" : "false",
              merchantCount);
    const bool merchantReady = merchantVisible || merchantCount > 0;
    IntCheck("Merchant context available", merchantReady);
    if (!merchantReady) {
        IntReport("");
        return false;
    }
    IntReport("  Opened merchant context via: %s", openedLabel ? openedLabel : "unknown");
    IntCheck("Merchant item list populated", merchantCount > 0);
    if (merchantCount == 0) {
        IntReport("");
        return false;
    }

    if (!quoteExpected) {
        IntSkip("Trader quote", "Opened regular merchant instead of trader");
        IntReport("");
        return true;
    }

    static constexpr uint32_t kQuoteModels[] = {921u, 929u, 933u, 934u, 945u, 948u, 955u};
    uint32_t selectedModelId = 0;
    uint32_t selectedItemId = 0;
    for (uint32_t modelId : kQuoteModels) {
        selectedItemId = TradeMgr::GetMerchantItemIdByModelId(modelId);
        if (selectedItemId != 0) {
            selectedModelId = modelId;
            break;
        }
    }

    if (!selectedItemId) {
        IntSkip("Merchant quote", "No known basic material model found in trader list");
        IntReport("");
        return false;
    }

    const uint32_t quoteBefore = TraderHook::GetQuoteId();
    IntReport("  Requesting trader quote for model=%u item=%u...", selectedModelId, selectedItemId);
    const bool requestQueued = TradeMgr::RequestTraderQuoteByItemId(selectedItemId);
    IntCheck("Trader quote request queued", requestQueued);
    if (!requestQueued) {
        IntReport("");
        return false;
    }

    const bool quoteObserved = WaitFor("trader quote response", 5000, [quoteBefore]() {
        return TraderHook::GetQuoteId() != quoteBefore || TraderHook::GetCostValue() > 0;
    });

    const uint32_t quoteAfter = TraderHook::GetQuoteId();
    const uint32_t costItemId = TraderHook::GetCostItemId();
    const uint32_t costValue = TraderHook::GetCostValue();
    IntReport("  Quote observed: quoteId=%u costItem=%u costValue=%u",
              quoteAfter, costItemId, costValue);

    IntCheck("Trader quote response observed", quoteObserved);
    IntCheck("Trader quote item matches request", costItemId == selectedItemId);
    IntCheck("Trader quote cost value positive", costValue > 0);

    IntReport("");
    return quoteObserved && costItemId == selectedItemId && costValue > 0;
}

bool TestMapTravel() {
    IntReport("=== GWA3-033 slice: Outpost Travel ===");

    constexpr uint32_t MAP_GADDS_ENCAMPMENT = 638;
    constexpr uint32_t MAP_LONGEYES_LEDGE = 650;

    const uint32_t startMapId = ReadMapId();
    if (startMapId == 0) {
        IntSkip("Map travel", "Not in game");
        return false;
    }

    const uint32_t targetMapId = (startMapId == MAP_GADDS_ENCAMPMENT) ? MAP_LONGEYES_LEDGE : MAP_GADDS_ENCAMPMENT;
    IntReport("  Traveling from map %u to map %u...", startMapId, targetMapId);

    IntReport("  Calling MapMgr::Travel...");
    MapMgr::Travel(targetMapId);
    IntReport("  MapMgr::Travel returned, waiting for transition...");

    const bool transitioned = WaitFor("MapID changes to target outpost", 60000, [startMapId, targetMapId]() {
        const uint32_t mapId = ReadMapId();
        return mapId != 0 && mapId != startMapId && mapId == targetMapId;
    });
    IntCheck("Outpost travel reached target map", transitioned);

    if (!transitioned) {
        IntReport("");
        return false;
    }

    const bool myIdReady = WaitFor("MyID valid after outpost travel", 30000, []() {
        return ReadMyId() > 0;
    });
    IntCheck("MyID valid after travel", myIdReady);

    const uint32_t endMapId = ReadMapId();
    const uint32_t endMyId = ReadMyId();
    IntReport("  After travel: MapID=%u, MyID=%u", endMapId, endMyId);

    IntReport("");
    return transitioned && myIdReady;
}

// ===== CONSET CRAFT CYCLE =====

static bool ConsetMoveToNPC(float x, float y, const char* label) {
    IntReport("  Moving to %s at (%.0f, %.0f)...", label, x, y);
    // Use MovePlayerNear — the proven working movement function.
    // Dispatches AgentMgr::Move via GameThread::EnqueuePost every 500ms.
    const bool arrived = MovePlayerNear(x, y, 250.0f, 30000);
    if (!arrived) IntReport("  Failed to reach %s", label);
    else IntReport("  Arrived at %s", label);
    return arrived;
}

// Open an NPC dialog using the proven AutoIt GoNPC + Dialog packet sequence.
// ChangeTarget → GoNPC (0x39) → wait → Dialog (0x3A) → wait → verify merchant open.
// This matches GWA2's GoToNPC + Dialog flow and GWA3 MaintenanceMgr's Xunlai open.
static bool ConsetOpenNPCDialog(uint32_t npcId, const char* label) {
    IntReport("  Opening %s NPC %u via GoNPC packet sequence...", label, npcId);
    for (int attempt = 0; attempt < 3; ++attempt) {
        // Target the NPC first
        AgentMgr::ChangeTarget(npcId);
        Sleep(250);
        // GoNPC packet (0x39) — initiates interaction and may open merchant directly
        CtoS::SendPacket(3, Packets::INTERACT_NPC, npcId, 0u);
        Sleep(2000);
        if (TradeMgr::GetMerchantItemCount() > 0) {
            IntReport("  %s opened on attempt %d (GoNPC only): %u items",
                      label, attempt + 1, TradeMgr::GetMerchantItemCount());
            return true;
        }
        // If GoNPC alone didn't work, try InteractNPC native as fallback
        if (attempt == 1) {
            IntReport("  GoNPC alone didn't work, trying native InteractNPC...");
            AgentMgr::InteractNPC(npcId);
            Sleep(2000);
            if (TradeMgr::GetMerchantItemCount() > 0) {
                IntReport("  %s opened via native InteractNPC: %u items",
                          label, TradeMgr::GetMerchantItemCount());
                return true;
            }
        }
        IntReport("  %s not open after attempt %d, retrying...", label, attempt + 1);
        Sleep(500);
    }
    IntReport("  Failed to open %s after 3 attempts", label);
    return false;
}

static uint32_t ConsetFindNearestNPC(float x, float y, float maxDist = 500.0f) {
    const uint32_t maxAgents = AgentMgr::GetMaxAgents();
    uint32_t bestId = 0;
    float bestDistSq = maxDist * maxDist;
    for (uint32_t i = 1; i < maxAgents; ++i) {
        auto* agent = AgentMgr::GetAgentByID(i);
        if (!agent || agent->type != 0xDB) continue;
        auto* living = static_cast<AgentLiving*>(agent);
        if (living->allegiance != 6) continue;
        const float distSq = AgentMgr::GetSquaredDistance(x, y, living->x, living->y);
        if (distSq < bestDistSq) { bestDistSq = distSq; bestId = i; }
    }
    return bestId;
}

// Find a trader virtual item by model ID in the global item array.
// AutoIt's TraderRequest scans [BasePtr]+0x18+0x40+0xB8 for items with
// bag==nullptr and agent_id==0 — these are merchant offering slots.
static uint32_t FindTraderVirtualItemId(uint32_t modelId) {
    // Scan global item array for virtual merchant items (bag==nullptr, agent_id==0)
    // Read item array size from [BasePtr]+0x18+0x40+0xC0 (same as AutoIt)
    uint32_t arraySize = 0;
    __try {
        uintptr_t p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
        uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x40);
        arraySize = *reinterpret_cast<uint32_t*>(p2 + 0xC0);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (arraySize == 0 || arraySize > 8192) return 0;
    IntReport("  FindTraderVirtualItemId: model=%u arraySize=%u", modelId, arraySize);
    uint32_t virtualCount = 0;
    for (uint32_t id = 1; id < arraySize; ++id) {
        __try {
            // Use the base pointer path: [BasePtr]+0x18+0x40+0xB8+[id*4]
            uintptr_t p0 = 0, p1 = 0, p2 = 0, p3 = 0, itemPtr = 0;
            if (!Offsets::BasePointer) break;
            p0 = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
            if (p0 < 0x10000) break;
            p1 = *reinterpret_cast<uintptr_t*>(p0 + 0x18);
            if (p1 < 0x10000) break;
            p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x40);
            if (p2 < 0x10000) break;
            p3 = *reinterpret_cast<uintptr_t*>(p2 + 0xB8);
            if (p3 < 0x10000) break;
            itemPtr = *reinterpret_cast<uintptr_t*>(p3 + id * 4);
            if (itemPtr < 0x10000) continue;
            auto* item = reinterpret_cast<Item*>(itemPtr);
            if (item->bag == nullptr && item->agent_id == 0) {
                ++virtualCount;
                if (item->model_id == modelId) {
                    IntReport("  Found virtual item: id=%u model=%u at index=%u (scanned %u virtual items)",
                              item->item_id, item->model_id, id, virtualCount);
                    return item->item_id;
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
    }
    IntReport("  Virtual item model=%u NOT FOUND (scanned %u items, %u virtual)", modelId, arraySize, virtualCount);
    return 0;
}

// Request a trader quote via GameThread — calls the native RequestQuote
// function with type=0xC (TraderBuy). This avoids RenderHook shellcode
// which may not be active after bootstrap.
struct TraderQuoteTask { uint32_t itemId; };
static uint32_t s_traderQuoteItemId = 0;

static void __cdecl TraderQuoteInvoker(void* storage) {
    auto* t = reinterpret_cast<TraderQuoteTask*>(storage);
    if (!t || !t->itemId || !Offsets::RequestQuote) {
        Log::Warn("[INTG] TraderQuoteInvoker: bad args item=%u fn=0x%08X",
                  t ? t->itemId : 0, static_cast<unsigned>(Offsets::RequestQuote));
        return;
    }

    s_traderQuoteItemId = t->itemId;
    uint32_t* itemIdPtr = &s_traderQuoteItemId;
    const uintptr_t fn = Offsets::RequestQuote;
    Log::Info("[INTG] TraderQuoteInvoker: calling RequestQuote(0xC) item=%u itemAddr=0x%08X fn=0x%08X",
              t->itemId, reinterpret_cast<uintptr_t>(itemIdPtr), static_cast<unsigned>(fn));
    __asm {
        mov eax, itemIdPtr
        push eax        // recv.item_ids
        push 1          // recv.item_count
        push 0          // recv.unknown
        push 0          // give.item_ids
        push 0          // give.item_count
        push 0          // give.unknown
        push 0          // unknown arg2
        push 0xC        // type = TraderBuy (0xC)
        xor ecx, ecx
        mov edx, 2
        mov eax, fn
        call eax
        add esp, 0x20
    }
    Log::Info("[INTG] TraderQuoteInvoker: returned from RequestQuote — quoteId=%u costItem=%u costValue=%u",
              TraderHook::GetQuoteId(), TraderHook::GetCostItemId(), TraderHook::GetCostValue());
}

// Send trader quote request via UIMessage (same path as GWCA/GWToolbox).
// UIMessage kSendMerchantRequestQuote = 0x30000006 with struct:
// { type=0xC, unknown=0, give={0,0,nullptr}, recv={0,1,&itemId} }
struct TraderQuoteUIMsg {
    uint32_t type;
    uint32_t unknown;
    struct { uint32_t unknown; uint32_t item_count; uint32_t* item_ids; } give;
    struct { uint32_t unknown; uint32_t item_count; uint32_t* item_ids; } recv;
};
static uint32_t s_uiMsgItemId = 0;

static void __cdecl TraderQuoteUIInvoker(void* storage) {
    auto* t = reinterpret_cast<TraderQuoteTask*>(storage);
    if (!t || !t->itemId) return;

    s_uiMsgItemId = t->itemId;

    TraderQuoteUIMsg msg{};
    msg.type = 0xC; // TraderBuy
    msg.unknown = 0;
    msg.give = {0, 0, nullptr};
    msg.recv = {0, 1, &s_uiMsgItemId};

    Log::Info("[INTG] TraderQuoteUI: SendUIMessage(0x30000006) type=0xC item=%u", t->itemId);
    UIMgr::SendUIMessageAsm(0x30000006u, &msg, nullptr);
    Log::Info("[INTG] TraderQuoteUI: returned — quoteId=%u costItem=%u costValue=%u",
              TraderHook::GetQuoteId(), TraderHook::GetCostItemId(), TraderHook::GetCostValue());
}

static bool RequestTraderQuoteViaGameThread(uint32_t itemId) {
    if (!itemId) return false;
    TraderHook::Reset();
    CtoS::ResetPacketTap();

    // Dispatch via Engine hook command queue — native RequestQuote fires TraderHook
    TraderQuoteTask task{itemId};
    CtoS::EnqueueGameCommand(&TraderQuoteInvoker, &task, sizeof(task));

    // Wait for dispatch + response
    Sleep(500);
    auto snap = CtoS::GetPacketTapSnapshot();
    Log::Info("[INTG] RequestTraderQuote: item=%u quoteId=%u costItem=%u costValue=%u ctoS_total=%u debugEbx=0x%08X",
              itemId, TraderHook::GetQuoteId(), TraderHook::GetCostItemId(), TraderHook::GetCostValue(),
              snap.total_packets, static_cast<unsigned>(TraderHook::GetDebugEbx()));
    return true;
}

// Buy material packs from the material trader at Embark Beach.
// Reproduces AutoIt BuyMaterialIfMissing: quote → buy → re-quote → buy loop.
// Native TransactionFunction invoker for TraderBuy (type=0xC).
// Must be a standalone __cdecl function (not lambda) for inline ASM.
struct TraderTransactTask { uint32_t itemId; uint32_t cost; };
static uint32_t s_traderRecvItemId = 0;
static uint32_t s_traderRecvQty = 1;

static void __cdecl TraderTransactInvoker(void* storage) {
    auto* t = reinterpret_cast<TraderTransactTask*>(storage);
    if (!t || !t->itemId || !Offsets::Transaction) return;

    s_traderRecvItemId = t->itemId;
    s_traderRecvQty = 1;
    uint32_t* recvIds = &s_traderRecvItemId;
    uint32_t* recvQtys = &s_traderRecvQty;
    uint32_t goldGive = t->cost;
    const uintptr_t fn = Offsets::Transaction;
    __asm {
        push recvQtys      // recv.item_quantities
        push recvIds       // recv.item_ids
        push 1             // recv.item_count
        push 0             // gold_recv
        push 0             // give.item_quantities
        push 0             // give.item_ids
        push 0             // give.item_count
        push goldGive      // gold_give
        push 0xC           // type = TraderBuy
        mov eax, fn
        call eax
        add esp, 0x24
    }
    Log::Info("[INTG] TraderTransact: returned (item=%u cost=%u)", t->itemId, t->cost);
}

// Find the position (1-based) of an item in the merchant list by scanning
// the global item array for a virtual item matching the model, then finding
// that item_id in the merchant list.
static uint32_t FindMerchantPositionForTraderItem(uint32_t modelId) {
    const uint32_t virtualItemId = FindTraderVirtualItemId(modelId);
    if (!virtualItemId) return UINT32_MAX;

    // Scan merchant list for this item_id
    const uint32_t merchantCount = TradeMgr::GetMerchantItemCount();
    for (uint32_t pos = 1; pos <= merchantCount; ++pos) {
        Item* item = TradeMgr::GetMerchantItemByPosition(pos);
        if (item && item->item_id == virtualItemId) return pos;
    }
    // Fallback: try FindMerchantItemPositionByModelId
    return FindMerchantItemPositionByModelId(modelId);
}

static uint32_t ConsetBuyMaterial(uint32_t modelId, uint32_t neededTotal) {
    const uint32_t have = CountInventoryModelQuantity(modelId);
    if (have >= neededTotal) return have;
    const uint32_t missing = neededTotal - have;
    const uint32_t packs = (missing + 9) / 10;
    IntReport("  Buying material model=%u: have=%u need=%u missing=%u packs=%u",
              modelId, have, neededTotal, missing, packs);

    // Find the item position in the merchant list
    const uint32_t itemPosition = FindMerchantPositionForTraderItem(modelId);
    if (itemPosition == UINT32_MAX || itemPosition == 0) {
        IntReport("  Material model=%u not found in merchant list", modelId);
        return have;
    }
    IntReport("  Material model=%u at merchant position %u", modelId, itemPosition);

    // Resolve UI frames
    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(kMerchantRootHash);
    const uintptr_t merchantContext = UIMgr::GetFrameContext(merchantFrame);
    if (merchantFrame < 0x10000 || merchantContext < 0x10000) {
        IntReport("  Merchant frame not available (frame=0x%08X context=0x%08X)",
                  static_cast<unsigned>(merchantFrame), static_cast<unsigned>(merchantContext));
        return have;
    }

    // Find the item row frame
    const uint32_t itemIndex = itemPosition - 1u;
    const uint32_t itemPath[] = { 0u, 0u, itemIndex };
    const uintptr_t itemRowFrame = ResolveMerchantSortedPathFrame(merchantFrame, itemPath, 3, "trader-item[0,0,index]");

    // Find the Buy button (same approach as crafter Craft button)
    const uintptr_t buyBtn = UIMgr::GetFrameByContextAndChildOffset(merchantContext, 125u, merchantFrame);
    IntReport("  UI frames: merchantFrame=0x%08X row=0x%08X buyBtn(125)=0x%08X",
              static_cast<unsigned>(merchantFrame), static_cast<unsigned>(itemRowFrame),
              static_cast<unsigned>(buyBtn));

    // Use native RequestQuote function (type=0xC) via Engine hook command queue,
    // then native TransactionFunction via GameThread.
    // NEVER use raw CtoS::SendPacket for 0x4C/0x4D — crashes the client.
    // The native functions go through the game's internal dispatch which is safe.
    uint32_t bought = 0;
    for (uint32_t p = 0; p < packs; ++p) {
        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        const uint32_t matBefore = CountInventoryModelQuantity(modelId);

        // Find the virtual item ID for this material
        const uint32_t traderItemId = FindTraderVirtualItemId(modelId);
        if (!traderItemId) {
            IntReport("  Virtual item not found for model=%u on pack %u", modelId, p + 1);
            break;
        }

        // Request quote via native RequestQuote (type=0xC) — this tells the server
        // we want to buy this item. The server responds with the price.
        TraderHook::Reset();
        IntReport("  Requesting quote for item=%u (pack %u/%u)...", traderItemId, p + 1, packs);
        TraderQuoteTask quoteTask{traderItemId};
        CtoS::EnqueueGameCommand(&TraderQuoteInvoker, &quoteTask, sizeof(quoteTask));

        // Wait for kVendorQuote UIMessage OR TraderHook response (whichever works)
        const bool quoteArrived = WaitFor("trader quote", 5000, []() {
            const uint32_t v = TraderHook::GetCostValue();
            return v > 0 && v < 100000; // sane gold range
        });
        const uint32_t quotedCost = TraderHook::GetCostValue();
        const uint32_t quotedItemId = TraderHook::GetCostItemId();
        IntReport("  Quote: arrived=%u item=%u price=%u", quoteArrived, quotedItemId, quotedCost);

        // Check if the trader has supply (price 0 = out of stock) or if we can afford it
        if (!quoteArrived || quotedCost == 0) {
            IntReport("  Material out of stock or no quote — stopping buy for model=%u", modelId);
            break;
        }
        if (goldBefore < quotedCost + 1000) { // keep 1000g reserve for craft fees
            IntReport("  Low gold (%u) — stopping buy for model=%u", goldBefore, modelId);
            break;
        }

        // After RequestQuote, the game caches the quote. The AutoIt TraderBuy
        // passes TraderCostValue (from hook) as goldGive. Since we can't read the
        // Use the REAL quoted price from kVendorQuote UIMessage callback
        uint32_t itemForBuy = TraderHook::GetCostItemId();
        uint32_t goldForBuy = quotedCost;
        IntReport("  TransactItem(0xC): item=%u goldGive=%u", itemForBuy, goldForBuy);
        TraderTransactTask txTask{itemForBuy, goldForBuy};
        CtoS::EnqueueGameCommand(&TraderTransactInvoker, &txTask, sizeof(txTask));

        // Wait for gold decrease or inventory increase
        const bool buyOk = WaitFor("material buy", 5000, [goldBefore, modelId, matBefore]() {
            return ItemMgr::GetGoldCharacter() < goldBefore
                || CountInventoryModelQuantity(modelId) > matBefore;
        });
        const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
        const uint32_t matAfter = CountInventoryModelQuantity(modelId);
        if (buyOk) {
            ++bought;
            IntReport("  Bought pack %u/%u: gold=%u->%u mat=%u->%u",
                      p + 1, packs, goldBefore, goldAfter, matBefore, matAfter);
        } else {
            IntReport("  Buy failed at pack %u (gold=%u->%u mat=%u->%u)",
                      p + 1, goldBefore, goldAfter, matBefore, matAfter);
            break;
        }
        Sleep(ChatMgr::GetPing() + 500);
        // Every 20 packs, pause longer to let the game catch up
        if ((p + 1) % 20 == 0) {
            IntReport("  Pausing after %u packs to let game settle...", p + 1);
            Sleep(2000);
        }
    }
    const uint32_t finalCount = CountInventoryModelQuantity(modelId);
    IntReport("  Material model=%u: bought=%u packs, final count=%u", modelId, bought, finalCount);
    return finalCount;
}

static bool ConsetCraftOneItem(const char* traderLabel, float traderX, float traderY, uint32_t targetModelId) {
    IntReport("  --- Crafting at %s (model=%u) ---", traderLabel, targetModelId);
    if (!ConsetMoveToNPC(traderX, traderY, traderLabel)) {
        IntReport("  Failed to reach %s", traderLabel);
        return false;
    }
    uint32_t npc = ConsetFindNearestNPC(traderX, traderY);
    if (!npc) { IntReport("  No NPC near %s", traderLabel); return false; }
    if (!ConsetOpenNPCDialog(npc, traderLabel)) return false;
    IntReport("  %s merchant open: %u items", traderLabel, TradeMgr::GetMerchantItemCount());

    const uint32_t itemPosition = FindMerchantItemPositionByModelId(targetModelId);
    if (itemPosition == UINT32_MAX) { IntReport("  Model %u not found at %s", targetModelId, traderLabel); return false; }
    Item* merchantItem = TradeMgr::GetMerchantItemByPosition(itemPosition);
    if (!merchantItem) return false;

    const uint32_t beforeCount = CountInventoryModelQuantity(targetModelId);
    const uint32_t goldBefore = ItemMgr::GetGoldCharacter();

    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(kMerchantRootHash);
    const uintptr_t merchantContext = UIMgr::GetFrameContext(merchantFrame);
    const uint32_t itemIndex = itemPosition - 1u;
    const uint32_t itemPath[] = { 0u, 0u, itemIndex };
    uintptr_t itemRowFrame = ResolveMerchantSortedPathFrame(merchantFrame, itemPath, 3, "item[0,0,index]");
    if (itemRowFrame < 0x10000) { IntReport("  Item row not found"); return false; }

    UIMgr::ButtonClick(itemRowFrame);
    Sleep(500 + ChatMgr::GetPing());

    const uintptr_t craftBtn = merchantContext >= 0x10000
        ? UIMgr::GetFrameByContextAndChildOffset(merchantContext, 125u, merchantFrame) : 0u;
    if (craftBtn < 0x10000) { IntReport("  Craft button not found"); return false; }

    UIMgr::ButtonClick(craftBtn);
    const bool crafted = WaitFor("craft completion", 5000, [targetModelId, beforeCount, goldBefore]() {
        return CountInventoryModelQuantity(targetModelId) > beforeCount || ItemMgr::GetGoldCharacter() < goldBefore;
    });
    const uint32_t afterCount = CountInventoryModelQuantity(targetModelId);
    IntReport("  Result: before=%u after=%u gold=%u->%u", beforeCount, afterCount, goldBefore, ItemMgr::GetGoldCharacter());
    return afterCount > beforeCount;
}

bool TestConsetCraftCycle() {
    IntReport("=== CONSET CRAFT CYCLE TEST (100k budget) ===");
    StartWatchdog();
    WriteConsumableHarnessStatus("conset_cycle_start", "conset", ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "starting");

    // 1. Travel to Embark Beach
    if (ReadMapId() != 857u) {
        CtoS::SuspendEngineHook();
        CtoS::MapTravel(857u, 4u, 1u, 0u);
        Sleep(1000);
        CtoS::ResumeEngineHook();
        WaitFor("Embark Beach", 30000, []() { return ReadMapId() == 857u && MapMgr::GetLoadingState() == 0; });
    }
    if (!WaitForPlayerWorldReady(15000)) { IntReport("  Failed to reach Embark"); return false; }
    const bool agentReady = WaitFor("player agent", 10000, []() {
        auto* me = AgentMgr::GetMyAgent();
        return me && me->x != 0.0f;
    });
    if (!agentReady) { IntReport("  Player agent not ready"); return false; }
    {
        auto* me = AgentMgr::GetMyAgent();
        IntReport("  In Embark Beach district=%u pos=(%.0f, %.0f)", MapMgr::GetDistrict(), me ? me->x : 0.0f, me ? me->y : 0.0f);
    }

    // 2. Withdraw gold to reach 100k on character
    constexpr uint32_t kTargetGold = 100000u;
    uint32_t gold = ItemMgr::GetGoldCharacter();
    uint32_t storageGold = ItemMgr::GetGoldStorage();
    IntReport("  Step 2: Gold — char=%u storage=%u target=%u", gold, storageGold, kTargetGold);
    if (gold < kTargetGold && storageGold > 0) {
        IntReport("  Withdrawing gold from Xunlai to reach %u...", kTargetGold);
        if (ConsetMoveToNPC(kEmbarkXunlaiX, kEmbarkXunlaiY, "Xunlai Chest")) {
            uint32_t npc = ConsetFindNearestNPC(kEmbarkXunlaiX, kEmbarkXunlaiY);
            if (npc) {
                AgentMgr::ChangeTarget(npc);
                Sleep(250);
                CtoS::SendPacket(3, Packets::INTERACT_NPC, npc, 0u);
                Sleep(2000);
                uint32_t toWithdraw = kTargetGold - gold;
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

    // 3. Open material trader and get prices
    constexpr float kMaterialTraderX = 2933.0f;
    constexpr float kMaterialTraderY = -2236.0f;
    IntReport("  Step 3: Opening material trader for price check...");
    if (!ConsetMoveToNPC(kMaterialTraderX, kMaterialTraderY, "Material Trader")) {
        IntReport("  Failed to reach material trader");
        return false;
    }
    uint32_t traderNpc = ConsetFindNearestNPC(kMaterialTraderX, kMaterialTraderY);
    if (!traderNpc || !ConsetOpenNPCDialog(traderNpc, "Material Trader")) {
        IntReport("  Failed to open material trader");
        return false;
    }
    IntReport("  Material trader open: %u items", TradeMgr::GetMerchantItemCount());

    // Get current prices by requesting a single quote for each material
    auto getPrice = [](uint32_t modelId) -> uint32_t {
        uint32_t itemId = FindTraderVirtualItemId(modelId);
        if (!itemId) return 0;
        TraderHook::Reset();
        TraderQuoteTask q{itemId};
        CtoS::EnqueueGameCommand(&TraderQuoteInvoker, &q, sizeof(q));
        WaitFor("price check", 3000, []() {
            return TraderHook::GetCostValue() > 0 && TraderHook::GetCostValue() < 100000;
        });
        return TraderHook::GetCostValue();
    };
    uint32_t priceIron = getPrice(kMaterialIronIngot);
    uint32_t priceDust = getPrice(kMaterialDust);
    uint32_t priceBone = getPrice(kMaterialBone);
    uint32_t priceFeather = getPrice(kMaterialFeather);
    IntReport("  Prices: Iron=%u Dust=%u Bone=%u Feather=%u", priceIron, priceDust, priceBone, priceFeather);

    if (!priceIron || !priceDust || !priceBone || !priceFeather) {
        IntReport("  Failed to get all material prices");
        return false;
    }

    // Calculate how many consets we can afford with ~90k budget
    // Per conset: 10 packs Iron + 10 packs Dust + 5 packs Bone + 5 packs Feather + 750g craft fees
    // Material prices rise as we buy — estimate ~50% markup over starting price
    constexpr uint32_t kMaterialBudget = 90000u;
    uint32_t costPerConset = 10u * priceIron + 10u * priceDust + 5u * priceBone + 5u * priceFeather + 750u;
    // Add 50% buffer for price increases
    uint32_t estimatedCostPerConset = costPerConset + costPerConset / 2;
    uint32_t numConsets = kMaterialBudget / estimatedCostPerConset;
    if (numConsets < 1) numConsets = 1;
    if (numConsets > 25) numConsets = 25; // cap to avoid very long runs
    IntReport("  Cost per conset (base): %u  Estimated (1.5x): %u  Budget: %u  Consets to craft: %u",
              costPerConset, estimatedCostPerConset, kMaterialBudget, numConsets);

    // Calculate total materials needed (including what we already have)
    uint32_t haveIron = CountInventoryModelQuantity(kMaterialIronIngot);
    uint32_t haveDust = CountInventoryModelQuantity(kMaterialDust);
    uint32_t haveBone = CountInventoryModelQuantity(kMaterialBone);
    uint32_t haveFeather = CountInventoryModelQuantity(kMaterialFeather);
    uint32_t needIron = numConsets * 100;
    uint32_t needDust = numConsets * 100;
    uint32_t needBone = numConsets * 50;
    uint32_t needFeather = numConsets * 50;
    IntReport("  Have: Iron=%u Dust=%u Bone=%u Feather=%u", haveIron, haveDust, haveBone, haveFeather);
    IntReport("  Need: Iron=%u Dust=%u Bone=%u Feather=%u", needIron, needDust, needBone, needFeather);

    // 4. Buy all materials (material trader is already open)
    IntReport("  Step 4: Buying materials...");
    if (haveIron < needIron)    ConsetBuyMaterial(kMaterialIronIngot, needIron);
    if (haveDust < needDust)    ConsetBuyMaterial(kMaterialDust, needDust);
    if (haveBone < needBone)    ConsetBuyMaterial(kMaterialBone, needBone);
    if (haveFeather < needFeather) ConsetBuyMaterial(kMaterialFeather, needFeather);

    haveIron = CountInventoryModelQuantity(kMaterialIronIngot);
    haveDust = CountInventoryModelQuantity(kMaterialDust);
    haveBone = CountInventoryModelQuantity(kMaterialBone);
    haveFeather = CountInventoryModelQuantity(kMaterialFeather);
    gold = ItemMgr::GetGoldCharacter();
    IntReport("  After buying: Iron=%u Dust=%u Bone=%u Feather=%u Gold=%u",
              haveIron, haveDust, haveBone, haveFeather, gold);

    // 5. Craft consets in a loop until we run out of materials or gold
    IntReport("  Step 5: Crafting consets...");
    uint32_t grailsCrafted = 0, essencesCrafted = 0, armorsCrafted = 0;
    for (uint32_t c = 0; c < numConsets; ++c) {
        uint32_t curIron = CountInventoryModelQuantity(kMaterialIronIngot);
        uint32_t curDust = CountInventoryModelQuantity(kMaterialDust);
        uint32_t curBone = CountInventoryModelQuantity(kMaterialBone);
        uint32_t curFeather = CountInventoryModelQuantity(kMaterialFeather);
        uint32_t curGold = ItemMgr::GetGoldCharacter();

        bool canGrail = curIron >= 50 && curDust >= 50 && curGold >= 250;
        bool canEssence = curFeather >= 50 && curDust >= 50 && curGold >= 250;
        bool canArmor = curIron >= 50 && curBone >= 50 && curGold >= 250;

        if (!canGrail && !canEssence && !canArmor) {
            IntReport("  Stopping at conset %u — insufficient materials/gold (Iron=%u Dust=%u Bone=%u Feather=%u Gold=%u)",
                      c, curIron, curDust, curBone, curFeather, curGold);
            break;
        }

        IntReport("  --- Conset %u/%u (Iron=%u Dust=%u Bone=%u Feather=%u Gold=%u) ---",
                  c + 1, numConsets, curIron, curDust, curBone, curFeather, curGold);

        if (canGrail && ConsetCraftOneItem("Eyja", kEmbarkEyjaX, kEmbarkEyjaY, kModelGrailOfMight)) {
            ++grailsCrafted;
        }
        if (canEssence && ConsetCraftOneItem("Kwat", kEmbarkKwatX, kEmbarkKwatY, kModelEssenceCelerity)) {
            ++essencesCrafted;
        }
        if (canArmor && ConsetCraftOneItem("Alcus", kEmbarkAlcusX, kEmbarkAlcusY, kModelArmorSalvation)) {
            ++armorsCrafted;
        }
    }

    uint32_t totalSets = (grailsCrafted < essencesCrafted ? grailsCrafted : essencesCrafted);
    totalSets = (totalSets < armorsCrafted ? totalSets : armorsCrafted);
    uint32_t finalGold = ItemMgr::GetGoldCharacter();
    uint32_t goldSpent = (gold > finalGold) ? (gold - finalGold) : 0;
    // Use starting gold before any buying for total spent calculation
    char result[256];
    sprintf_s(result, "grails=%u essences=%u armors=%u completeSets=%u goldSpent=%u goldRemaining=%u",
              grailsCrafted, essencesCrafted, armorsCrafted, totalSets, goldSpent, finalGold);
    WriteConsumableHarnessStatus("conset_cycle_complete", "conset", ReadMapId(), 0, 0, 0, 0,
                                 0, totalSets, totalSets > 0 ? 1u : 0u, result);
    IntReport("=== CONSET RESULT: %u complete sets — %s ===", totalSets, result);
    return totalSets > 0;
}

bool TestConsumableCrafting() {
    IntReport("=== GWA3 Consumable Crafting Harness ===");

    if (ReadMapId() == 0 || ReadMyId() == 0) {
        IntSkip("Consumable crafting", "Not in game");
        IntReport("");
        return false;
    }

    const ConsumableHarnessStage stage = GetConsumableHarnessStage();
    IntReport("  Consumable harness stage: %s", DescribeConsumableHarnessStage(stage));

    if (stage == ConsumableHarnessStage::ConsetCycle) {
        return TestConsetCraftCycle();
    }

    const ConsumableHarnessTarget selectedTarget = GetConsumableHarnessTarget();
    const ConsumableHarnessClickMode clickMode = GetConsumableHarnessClickMode();
    char detail[160] = {};
    IntReport("  Consumable harness target: %s", DescribeConsumableHarnessTarget(selectedTarget));
    IntReport("  Consumable harness click mode: %s", DescribeConsumableHarnessClickMode(clickMode));
    sprintf_s(detail, "map=%u region=%u district=%u",
              ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
    WriteConsumableHarnessStatus("starting", DescribeConsumableHarnessTarget(selectedTarget),
                                 ReadMapId(), 0, 0, 0, 0, 0, 0, 0, detail);

    if (ReadMapId() == kMapEmbarkBeach && MapMgr::GetRegion() == kTradeTestRegion && MapMgr::GetDistrict() == UINT32_MAX) {
        IntReport("  Embark district is unresolved; waiting briefly for world state to settle before forcing travel");
        const bool districtResolved = WaitFor("Embark district resolves before quiet-district travel", 10000, []() {
            return ReadMyId() > 0 && MapMgr::GetDistrict() != UINT32_MAX;
        });
        IntReport("  Embark district settle result: resolved=%d district=%u", districtResolved ? 1 : 0, MapMgr::GetDistrict());
    }

    const bool alreadyInQuietEmbarkDistrict =
        ReadMapId() == kMapEmbarkBeach &&
        MapMgr::GetRegion() == kTradeTestRegion &&
        MapMgr::GetDistrict() == kTradeTestDistrict;
    if (!alreadyInQuietEmbarkDistrict) {
        IntReport("  Traveling to Embark Beach (%u) in Asia/Japan district %u for consumable crafter diagnostics...",
                  kMapEmbarkBeach, kTradeTestDistrict);
        WriteConsumableHarnessStatus("traveling", DescribeConsumableHarnessTarget(selectedTarget),
                                     ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "requesting_quiet_embark_travel");
        MapMgr::Travel(kMapEmbarkBeach, kTradeTestRegion, kTradeTestDistrict, kTradeTestLanguage);

        bool atTargetMap = WaitForConsumableTravelState(
            "travel_wait_preferred",
            DescribeConsumableHarnessTarget(selectedTarget),
            kMapEmbarkBeach,
            kTradeTestRegion,
            kTradeTestDistrict,
            60000);
        if (!atTargetMap) {
            IntReport("  Preferred quiet district did not load; falling back to Asia/Japan district %u", kTradeFallbackDistrict);
            char fallbackDetail[160] = {};
            sprintf_s(fallbackDetail, "preferred_failed map=%u region=%u district=%u myId=%u loading=%u",
                      ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict(), ReadMyId(), MapMgr::GetLoadingState());
            WriteConsumableHarnessStatus("travel_fallback_start", DescribeConsumableHarnessTarget(selectedTarget),
                                         ReadMapId(), 0, 0, 0, 0, 0, 0, 0, fallbackDetail);
            if (ReadMapId() == kMapEmbarkBeach &&
                MapMgr::GetRegion() == kTradeTestRegion &&
                MapMgr::GetDistrict() == kTradeFallbackDistrict &&
                ReadMyId() > 0 &&
                MapMgr::GetLoadingState() == 1) {
                WriteConsumableHarnessStatus("travel_fallback_already_loaded", DescribeConsumableHarnessTarget(selectedTarget),
                                             ReadMapId(), 0, 0, 0, 0, 0, 0, 1, "already_loaded_in_fallback_district");
                atTargetMap = true;
            } else {
                MapMgr::Travel(kMapEmbarkBeach, kTradeTestRegion, kTradeFallbackDistrict, kTradeTestLanguage);
                atTargetMap = WaitForConsumableTravelState(
                    "travel_wait_fallback",
                    DescribeConsumableHarnessTarget(selectedTarget),
                    kMapEmbarkBeach,
                    kTradeTestRegion,
                    kTradeFallbackDistrict,
                    60000);
            }
        }
        IntCheck("Reached quiet Embark Beach district", atTargetMap);
        if (!atTargetMap) {
            char failDetail[192] = {};
            sprintf_s(failDetail, "failed_to_reach_quiet_embark_district map=%u region=%u district=%u myId=%u loading=%u",
                      ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict(), ReadMyId(), MapMgr::GetLoadingState());
            WriteConsumableHarnessStatus("travel_failed", DescribeConsumableHarnessTarget(selectedTarget),
                                         ReadMapId(), 0, 0, 0, 0, 0, 0, 0, failDetail);
            IntReport("");
            return false;
        }

        const bool myIdReady = WaitFor("MyID valid after travel to quiet Embark district", 30000, []() {
            return ReadMyId() > 0;
        });
        IntCheck("MyID valid after quiet Embark travel", myIdReady);
        if (!myIdReady) {
            WriteConsumableHarnessStatus("travel_failed", DescribeConsumableHarnessTarget(selectedTarget),
                                         ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "myid_not_ready_after_quiet_travel");
            IntReport("");
            return false;
        }
    }

    sprintf_s(detail, "map=%u region=%u district=%u waiting_for_world_ready",
              ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
    WriteConsumableHarnessStatus("world_wait", DescribeConsumableHarnessTarget(selectedTarget),
                                 ReadMapId(), 0, 0, 0, 0, 0, 0, 0, detail);
    if (!WaitForPlayerWorldReady(10000)) {
        WriteConsumableHarnessStatus("world_not_ready", DescribeConsumableHarnessTarget(selectedTarget),
                                     ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "player_world_state_not_ready");
        IntSkip("Consumable crafting", "Player world state not ready");
        IntReport("");
        return false;
    }
    sprintf_s(detail, "map=%u region=%u district=%u world_ready",
              ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
    WriteConsumableHarnessStatus("world_ready", DescribeConsumableHarnessTarget(selectedTarget),
                                 ReadMapId(), 0, 0, 0, 0, 0, 0, 1, detail);

    struct ConsumableTarget {
        const char* label;
        float x;
        float y;
        uint32_t modelId;
    };
    const ConsumableTarget targets[] = {
        {"Eyja", kEmbarkEyjaX, kEmbarkEyjaY, kModelGrailOfMight},
        {"Kwat", kEmbarkKwatX, kEmbarkKwatY, kModelEssenceCelerity},
        {"Alcus", kEmbarkAlcusX, kEmbarkAlcusY, kModelArmorSalvation},
    };

    bool anySuccess = false;
    for (const auto& target : targets) {
        if (selectedTarget == ConsumableHarnessTarget::Grail && target.modelId != kModelGrailOfMight) continue;
        if (selectedTarget == ConsumableHarnessTarget::Essence && target.modelId != kModelEssenceCelerity) continue;
        if (selectedTarget == ConsumableHarnessTarget::Armor && target.modelId != kModelArmorSalvation) continue;

        IntReport("  --- %s crafter probe (model=%u) ---", target.label, target.modelId);
        sprintf_s(detail, "approaching_target x=%.0f y=%.0f map=%u region=%u district=%u",
                  target.x, target.y, ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
        WriteConsumableHarnessStatus("approaching_target", target.label, ReadMapId(), 0, 0,
                                     target.modelId, 0, 0, 0, 0, detail);
        const bool nearTarget = MovePlayerNear(target.x, target.y, 350.0f, 25000);
        IntCheck("Reached consumable crafter area", nearTarget);
        if (!nearTarget) {
            WriteConsumableHarnessStatus("approach_failed", target.label, ReadMapId(), 0, 0,
                                         target.modelId, 0, 0, 0, 0, "failed_to_reach_crafter_area");
            continue;
        }

        IntReport("  Nearby NPC candidates around %s:", target.label);
        DumpNpcLikeAgentsNearCoords(target.x, target.y, 900.0f, 8);

        if (stage == ConsumableHarnessStage::TravelOnly) {
            WriteConsumableHarnessStatus("travel_only_complete", target.label, ReadMapId(), 0, 0,
                                         target.modelId, 0, 0, 0, 1, "travel_only_stage_complete");
            IntSkip("Consumable crafter open", "Travel-only consumable isolation stage");
            IntReport("");
            return true;
        }

        uint32_t candidateIds[8]{};
        const size_t candidateCount = CollectNearestNpcLikeAgentsToCoords(target.x, target.y, 900.0f, candidateIds, _countof(candidateIds));
        IntCheck("Found consumable crafter candidates", candidateCount > 0);
        sprintf_s(detail, "candidate_count=%u map=%u region=%u district=%u",
                  static_cast<unsigned>(candidateCount), ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
        WriteConsumableHarnessStatus("candidate_scan", target.label, ReadMapId(), 0, 0,
                                     target.modelId, 0, 0, 0, candidateCount > 0 ? 1u : 0u, detail);
        if (candidateCount == 0) {
            WriteConsumableHarnessStatus("candidate_failed", target.label, ReadMapId(), 0, 0,
                                         target.modelId, 0, 0, 0, 0, "no_crafter_candidates_found");
            continue;
        }

        bool merchantReady = false;
        bool targetInventoryReady = false;
        uint32_t crafterAgentId = 0;
        uint32_t merchantItemCount = 0;
        uint32_t crafterItemId = 0;
        for (size_t i = 0; i < candidateCount && !targetInventoryReady; ++i) {
            crafterAgentId = candidateIds[i];
            if (!crafterAgentId) continue;

            float npcX = 0.0f;
            float npcY = 0.0f;
            TryReadAgentPosition(crafterAgentId, npcX, npcY);
            IntReport("    Candidate %u/%u: agent=%u pos=(%.0f, %.0f)",
                      static_cast<unsigned>(i + 1),
                      static_cast<unsigned>(candidateCount),
                      crafterAgentId, npcX, npcY);
            sprintf_s(detail, "candidate=%u/%u npc=(%.0f,%.0f)",
                      static_cast<unsigned>(i + 1), static_cast<unsigned>(candidateCount), npcX, npcY);
            WriteConsumableHarnessStatus("candidate_selected", target.label, ReadMapId(), crafterAgentId, 0,
                                         target.modelId, 0, 0, 0, 1, detail);

            const bool nearNpc = MovePlayerNear(npcX, npcY, 120.0f, 12000);
            sprintf_s(detail, "near_npc=%u npc=(%.0f,%.0f)", nearNpc ? 1u : 0u, npcX, npcY);
            WriteConsumableHarnessStatus("candidate_approach", target.label, ReadMapId(), crafterAgentId, 0,
                                         target.modelId, 0, 0, 0, nearNpc ? 1u : 0u, detail);
            if (!nearNpc) {
                IntReport("      Could not get close enough to candidate %u; trying next candidate", crafterAgentId);
                continue;
            }
            ReportMerchantPreInteractState("Consumable harness pre-interact snapshot", crafterAgentId, npcX, npcY);
            ReportMerchantRuntimeContext("Consumable harness runtime context");
            ReportMerchantTradeState("Consumable harness pre-interact trade state");

            AgentMgr::ChangeTarget(crafterAgentId);
            Sleep(250);
            MerchantStoCTap tap{};
            StartMerchantStoCTap(tap);
            WriteConsumableHarnessStatus("interacting", target.label, ReadMapId(), crafterAgentId, 0,
                                         target.modelId, 0, 0, 0, 1, "sending_interact_attempts");
            for (int nativeAttempt = 1; nativeAttempt <= 3; ++nativeAttempt) {
                IntReport("      AgentMgr::InteractNPC attempt %d", nativeAttempt);
                AgentMgr::InteractNPC(crafterAgentId);
                Sleep(500);
            }
            Sleep(2500);
            StopMerchantStoCTap(tap);

            ReportMerchantTradeState("Consumable harness post-interact trade state");
            ReportMerchantStoCTap("Consumable harness StoC tap", tap);
            merchantReady = WaitFor("consumable crafter merchant context", 2000, []() {
                return UIMgr::GetFrameByHash(kMerchantRootHash) != 0 || TradeMgr::GetMerchantItemCount() > 0;
            });
            sprintf_s(detail, "merchant_ready=%u frame=0x%08X items=%u",
                      merchantReady ? 1u : 0u,
                      static_cast<unsigned>(UIMgr::GetFrameByHash(kMerchantRootHash)),
                      TradeMgr::GetMerchantItemCount());
            WriteConsumableHarnessStatus("open_probe", target.label, ReadMapId(), crafterAgentId,
                                         TradeMgr::GetMerchantItemCount(), target.modelId, 0, 0, 0,
                                         merchantReady ? 1u : 0u, detail);

            if (!merchantReady) {
                MerchantStoCTap rawTap{};
                StartMerchantStoCTap(rawTap);
                for (int packetAttempt = 1; packetAttempt <= 3; ++packetAttempt) {
                    IntReport("      Raw GoNPC attempt %d", packetAttempt);
                    CtoS::SendPacket(3, Packets::INTERACT_NPC, crafterAgentId, 0u);
                    Sleep(500);
                }
                Sleep(2500);
                StopMerchantStoCTap(rawTap);
                ReportMerchantTradeState("Consumable harness post-raw-GoNPC trade state");
                ReportMerchantStoCTap("Consumable harness raw-GoNPC StoC tap", rawTap);
                merchantReady = WaitFor("consumable crafter merchant context after raw packet", 1500, []() {
                    return UIMgr::GetFrameByHash(kMerchantRootHash) != 0 || TradeMgr::GetMerchantItemCount() > 0;
                });
                sprintf_s(detail, "merchant_ready=%u after_raw_packet frame=0x%08X items=%u",
                          merchantReady ? 1u : 0u,
                          static_cast<unsigned>(UIMgr::GetFrameByHash(kMerchantRootHash)),
                          TradeMgr::GetMerchantItemCount());
                WriteConsumableHarnessStatus("open_probe_raw", target.label, ReadMapId(), crafterAgentId,
                                             TradeMgr::GetMerchantItemCount(), target.modelId, 0, 0, 0,
                                             merchantReady ? 1u : 0u, detail);
            }

            if (!merchantReady) {
                IntReport("      No merchant context observed for candidate %u; trying next candidate", crafterAgentId);
                continue;
            }

            merchantItemCount = TradeMgr::GetMerchantItemCount();
            ReportMerchantInventoryList("Consumable crafter inventory");
            crafterItemId = TradeMgr::GetMerchantItemIdByModelId(target.modelId);
            if (stage == ConsumableHarnessStage::ListOnly) {
                WriteConsumableHarnessStatus("frame_dump_begin", target.label, ReadMapId(), crafterAgentId,
                                             merchantItemCount, target.modelId, crafterItemId, 0, 0, 1,
                                             "dumping_target_candidate_frame_tree");
                DumpConsumableFrameTree(UIMgr::GetFrameByHash(kMerchantRootHash),
                                        FindMerchantItemPositionByModelId(target.modelId),
                                        target.modelId,
                                        crafterItemId);
                WriteConsumableHarnessStatus("frame_dump_complete", target.label, ReadMapId(), crafterAgentId,
                                             merchantItemCount, target.modelId, crafterItemId, 0, 0, 1,
                                             "target_candidate_frame_tree_dumped");
            }
            IntCheck("Consumable target model present in crafter inventory", crafterItemId != 0);
            if (crafterItemId == 0) {
                BuildMerchantInventorySummary(detail, sizeof(detail));
                WriteConsumableHarnessStatus("list_failed", target.label, ReadMapId(), crafterAgentId,
                                             merchantItemCount, target.modelId, 0, 0, 0, 0, detail);
                merchantReady = false;
                continue;
            }

            targetInventoryReady = true;
        }

        IntCheck("Consumable crafter merchant context available", targetInventoryReady);
        if (!targetInventoryReady) {
            WriteConsumableHarnessStatus("open_failed", target.label, ReadMapId(), crafterAgentId, 0,
                                         target.modelId, 0, 0, 0, 0, "merchant_context_not_observed");
            continue;
        }

        anySuccess = true;

        if (stage == ConsumableHarnessStage::OpenOnly) {
            WriteConsumableHarnessStatus("open_complete", target.label, ReadMapId(), crafterAgentId,
                                         merchantItemCount, target.modelId, crafterItemId, 0, 0, 1,
                                         "open_only_stage_complete");
            IntSkip("Consumable inventory list", "Open-only consumable isolation stage");
            IntReport("");
            return true;
        }

        if (stage == ConsumableHarnessStage::ListOnly) {
            WriteConsumableHarnessStatus("list_complete", target.label, ReadMapId(), crafterAgentId,
                                         merchantItemCount, target.modelId, crafterItemId, 0, 0, 1,
                                         "list_only_stage_complete");
            IntSkip("Consumable craft transact", "List-only consumable isolation stage");
            IntReport("");
            return true;
        }

        uint32_t beforeInventoryCount = 0;
        uint32_t afterInventoryCount = 0;
        char uiDetail[160] = {};
        IntReport("  Crafting one item for model=%u item=%u via UI row-selection path...", target.modelId, crafterItemId);
        const bool uiClicked = CraftConsumableViaUiClick(target.label, target.modelId, crafterItemId, clickMode,
                                                         beforeInventoryCount, afterInventoryCount,
                                                         uiDetail, sizeof(uiDetail));
        sprintf_s(detail, "uiClicked=%u legacyFallback=0 before=%u after=%u %s",
                  uiClicked ? 1u : 0u,
                  beforeInventoryCount,
                  afterInventoryCount,
                  uiDetail);

        IntReport("  Consumable craft inventory delta for model=%u: before=%u after=%u detail=%s",
                  target.modelId, beforeInventoryCount, afterInventoryCount, detail);
        IntCheck("Consumable craft increased inventory count", afterInventoryCount > beforeInventoryCount);
        WriteConsumableHarnessStatus("craft_complete", target.label, ReadMapId(), crafterAgentId,
                                     merchantItemCount, target.modelId, crafterItemId,
                                     beforeInventoryCount, afterInventoryCount,
                                     afterInventoryCount > beforeInventoryCount ? 1u : 0u,
                                     detail);

        if (stage == ConsumableHarnessStage::CraftOnly || stage == ConsumableHarnessStage::Full) {
            break;
        }
    }

    if (!anySuccess) {
        WriteConsumableHarnessStatus("complete", DescribeConsumableHarnessTarget(selectedTarget),
                                     ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "no_consumable_targets_succeeded");
    }

    IntReport("");
    return anySuccess;
}

int RunTradeHelperMode() {
    IntReport("=== GWA3 Player Trade Helper Mode ===");
    // Helper mode is a long-lived idle/rendezvous loop rather than an assert-heavy
    // integration suite. The generic watchdog's hung-window heuristic can false-fire
    // here during map travel/load and kill the helper before the trade harness ever
    // sees it. The harness already verifies helper liveness explicitly via PID and
    // the status file, so keep helper mode watchdog-free.

    const uint32_t startMapId = ReadMapId();
    if (startMapId == 0) {
        IntSkip("Trade helper", "Not in game");
        return 1;
    }

    if (!TradePartnerHook::Initialize()) {
        IntReport("  TradePartnerHook unavailable; helper will continue without partner-event telemetry");
    } else {
        TradePartnerHook::Reset();
    }

    if (startMapId != kMapLongeyesLedge || MapMgr::GetRegion() != kTradeTestRegion || MapMgr::GetDistrict() != kTradeTestDistrict) {
        IntReport("  Traveling helper to Longeye's Ledge Asia/Japan district %u...", kTradeTestDistrict);
        MapMgr::Travel(kMapLongeyesLedge, kTradeTestRegion, kTradeTestDistrict, kTradeTestLanguage);
        bool traveled = WaitFor("Helper reaches Longeye's Ledge preferred district", 20000, []() {
            return ReadMapId() == kMapLongeyesLedge
                && ReadMyId() > 0
                && MapMgr::GetRegion() == kTradeTestRegion
                && MapMgr::GetDistrict() == kTradeTestDistrict;
        });
        if (!traveled) {
            IntReport("  Preferred quiet Asia/Japan district did not load; falling back to Asia/Japan district %u", kTradeFallbackDistrict);
            MapMgr::Travel(kMapLongeyesLedge, kTradeTestRegion, kTradeFallbackDistrict, kTradeTestLanguage);
            traveled = WaitFor("Helper reaches Longeye's Ledge fallback district", 60000, []() {
                return ReadMapId() == kMapLongeyesLedge
                    && ReadMyId() > 0
                    && MapMgr::GetRegion() == kTradeTestRegion
                    && MapMgr::GetDistrict() == kTradeFallbackDistrict;
            });
        }
        IntCheck("Helper reached Longeye's Ledge", traveled);
        if (!traveled) {
            return 1;
        }
    }
    IntReport("  Helper staying near spawn in quiet trade district");

    const DWORD start = GetTickCount();
    DWORD lastLog = 0;
    DWORD tradeOpenedAt = 0;
    DWORD lastSubmitAttemptAt = 0;
    DWORD lastAcceptAttemptAt = 0;
    uint32_t tradeOpenCount = 0;
    uint32_t lastOpenFlags = 0;
    uint32_t submitAttemptCount = 0;
    uint32_t acceptAttemptCount = 0;
    bool submittedThisOpen = false;
    bool acceptedThisOpen = false;
    bool offeredItemThisOpen = false;
    uint32_t submitGoldThisOpen = 0;
    uint32_t offerModelThisOpen = 0;

    while (GetTickCount() - start < 10 * 60 * 1000) {
        const DWORD now = GetTickCount();
        const uint32_t tradeFlags = ReadTradeFlagsForHelper();
        const bool tradeOpen = tradeFlags != 0;

        if (tradeOpen && tradeOpenedAt == 0) {
            tradeOpenedAt = now;
            lastSubmitAttemptAt = 0;
            lastAcceptAttemptAt = 0;
            ++tradeOpenCount;
            lastOpenFlags = tradeFlags;
            submittedThisOpen = false;
            acceptedThisOpen = false;
            offeredItemThisOpen = false;
            submitGoldThisOpen = ReadTradeHelperSubmitGoldConfig();
            offerModelThisOpen = ReadTradeHelperOfferItemModelConfig();
            IntReport("  Helper observed player trade open (flags=%u offerModel=%u)", tradeFlags, offerModelThisOpen);
        }

        if (!tradeOpen) {
            tradeOpenedAt = 0;
            lastSubmitAttemptAt = 0;
            lastAcceptAttemptAt = 0;
            submittedThisOpen = false;
            acceptedThisOpen = false;
            offeredItemThisOpen = false;
            submitGoldThisOpen = 0;
            offerModelThisOpen = 0;
        }

        const bool autoSubmitEnabled = ReadTradeHelperAutoSubmitConfig();

        // Auto-offer item if configured, before submitting.
        if (tradeOpen && autoSubmitEnabled && !offeredItemThisOpen && offerModelThisOpen > 0
            && tradeOpenedAt != 0 && now - tradeOpenedAt > 500) {
            const uint32_t itemId = FindHelperInventoryItemByModel(offerModelThisOpen);
            if (itemId > 0) {
                IntReport("  Helper auto-offering item=%u model=%u (flags=%u)", itemId, offerModelThisOpen, tradeFlags);
                GameThread::Enqueue([itemId]() { TradeMgr::OfferItemPromptMax(itemId); });
                offeredItemThisOpen = true;
            } else {
                IntReport("  Helper offer_item_model_id=%u not found in inventory", offerModelThisOpen);
                offeredItemThisOpen = true;
            }
        }

        if (tradeOpen && autoSubmitEnabled && !submittedThisOpen && tradeOpenedAt != 0 && now - tradeOpenedAt > 750
            && (offerModelThisOpen == 0 || offeredItemThisOpen)) {
            const uint32_t gold = submitGoldThisOpen;
            IntReport("  Helper auto-submitting offer gold=%u (flags=%u)", gold, tradeFlags);
            GameThread::Enqueue([gold]() { TradeMgr::SubmitOffer(gold); });
            ++submitAttemptCount;
            submittedThisOpen = true;
            lastSubmitAttemptAt = now;
        }

        if (tradeOpen && autoSubmitEnabled && submittedThisOpen && tradeOpenedAt != 0 && now - tradeOpenedAt > 1500) {
            if (!acceptedThisOpen || now - lastAcceptAttemptAt > 1500) {
                IntReport("  Helper auto-accepting incoming trade (flags=%u)", tradeFlags);
                GameThread::Enqueue([]() { TradeMgr::AcceptTrade(); });
                ++acceptAttemptCount;
                acceptedThisOpen = true;
                lastAcceptAttemptAt = now;
            }
        }

        if (tradeOpen && tradeOpenedAt != 0 && now - tradeOpenedAt > 20000) {
            IntReport("  Helper auto-canceling stale trade after 20s (flags=%u)", tradeFlags);
            GameThread::Enqueue([]() { TradeMgr::CancelTrade(); });
            tradeOpenedAt = 0;
        }

        float x = 0.0f, y = 0.0f;
        TryReadAgentPosition(ReadMyId(), x, y);
        const uint32_t mapId = ReadMapId();
        const uint32_t region = MapMgr::GetRegion();
        const uint32_t district = MapMgr::GetDistrict();
        uint32_t playerGold = 0;
        uint32_t partnerGold = 0;
        uint32_t playerItemCount = 0;
        uint32_t partnerItemCount = 0;
        ReadTradeStateForHelper(playerGold, partnerGold, playerItemCount, partnerItemCount);
        HelperPartnerItemInfo partnerItems[8] = {};
        const size_t partnerItemDetailCount = ReadTradePartnerItemsForHelper(partnerItems, 8);
        const uint32_t tradePartnerHookHits = TradePartnerHook::GetHitCount();
        const uint32_t tradePartnerLastEax = TradePartnerHook::GetLastEax();
        const uint32_t tradePartnerLastEcx = TradePartnerHook::GetLastEcx();
        const uint32_t tradePartnerLastEdx = TradePartnerHook::GetLastEdx();
        const uint32_t tradeUiPlayerUpdatedCount = TradeMgr::GetTradeUiPlayerUpdatedCount();
        const uint32_t tradeUiSessionStartCount = TradeMgr::GetTradeUiSessionStartCount();
        const uint32_t tradeUiSessionUpdatedCount = TradeMgr::GetTradeUiSessionUpdatedCount();
        const uint32_t tradeUiLastSessionStartState = TradeMgr::GetTradeUiLastSessionStartState();
        const uint32_t tradeUiLastSessionStartPlayerNumber = TradeMgr::GetTradeUiLastSessionStartPlayerNumber();
        // Build partner items JSON fragment for status
        char partnerItemsJson[512] = "[]";
        if (partnerItemDetailCount > 0) {
            char* p = partnerItemsJson;
            *p++ = '[';
            for (size_t pi = 0; pi < partnerItemDetailCount; ++pi) {
                if (pi > 0) *p++ = ',';
                p += sprintf_s(p, static_cast<size_t>(partnerItemsJson + sizeof(partnerItemsJson) - p),
                               "{\"item_id\":%u,\"model_id\":%u,\"quantity\":%u}",
                               partnerItems[pi].item_id, partnerItems[pi].model_id, partnerItems[pi].quantity);
            }
            *p++ = ']';
            *p = '\0';
        }
        WriteTradeHelperStatus(mapId, region, district, ReadMyId(), x, y, tradeFlags, tradeOpenCount, lastOpenFlags,
                             submitAttemptCount, acceptAttemptCount, playerGold, partnerGold, playerItemCount, partnerItemCount,
                             tradePartnerHookHits, tradePartnerLastEax, tradePartnerLastEcx, tradePartnerLastEdx,
                             tradeUiPlayerUpdatedCount, tradeUiSessionStartCount, tradeUiSessionUpdatedCount,
                             tradeUiLastSessionStartState, tradeUiLastSessionStartPlayerNumber,
                             partnerItemsJson);

        if (now - lastLog >= 3000) {
            IntReport("  Helper heartbeat: map=%u region=%u district=%u myId=%u pos=(%.1f, %.1f) tradeFlags=%u",
                      mapId, region, district, ReadMyId(), x, y, tradeFlags);
            IntReport("    TradePartnerHook: hits=%u eax=%u ecx=%u edx=%u",
                      tradePartnerHookHits, tradePartnerLastEax, tradePartnerLastEcx, tradePartnerLastEdx);
            IntReport("    TradeUI: playerUpdated=%u sessionStart=%u sessionUpdated=%u lastState=%u lastPlayer=%u",
                      tradeUiPlayerUpdatedCount, tradeUiSessionStartCount, tradeUiSessionUpdatedCount,
                      tradeUiLastSessionStartState, tradeUiLastSessionStartPlayerNumber);
            lastLog = now;
        }

        Sleep(250);
    }

    TradePartnerHook::Shutdown();
    IntReport("  Helper timeout reached; exiting helper mode");
    return 0;
}

int RunConsumableCraftingTest() {
    int failures = 0;
    if (!TestConsumableCrafting()) ++failures;
    return failures;
}

#if 0
int RunFroggySparkflyRouteTest() {
    IntReport("RunFroggySparkflyRouteTest: stub — not implemented in this file");
    return 0;
}
#endif

} // namespace GWA3::SmokeTest
