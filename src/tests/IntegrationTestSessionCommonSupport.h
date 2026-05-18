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

enum class IdentifySalvageIsolationStage {
    Full,
    IdentifyOnly,
    SalvageOpenOnly,
    SingleSalvage,
    NativeSalvage,
    NativeSalvageEnter,
    LegacyBotshubStartOnly,
    LegacyBotshubSalvage,
    LegacyBotshubSalvageEnter,
    LegacyBotshubSalvageDone,
    LegacyBotshubSalvageCancel,
    LegacyBotshubTrackedChain,
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
constexpr uint32_t kMaterialBone = 921u;
constexpr uint32_t kMaterialDust = 929u;
constexpr uint32_t kMaterialFeather = 933u;
constexpr uint32_t kMaterialPlantFiber = 934u;
constexpr uint32_t kMaterialIronIngot = 948u;
constexpr uint32_t kMaterialScale = 953u;
constexpr uint32_t kMaterialGraniteSlab = 955u;
