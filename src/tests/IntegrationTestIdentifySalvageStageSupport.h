struct IdentifySalvageManualContext {
    IdentifySalvageCandidate candidate{};
    uint32_t salvageKitId = 0;
    uint32_t salvageKitModel = 0;
};

void ConfigureIdentifySalvageRuntimeOverrides() {
    const bool useLegacyAutoItSalvageContext = false;
    const bool forceAutoItSalvageEntry = false;
    CtoS::SetIdentifySalvageRuntimeOverrides(useLegacyAutoItSalvageContext, forceAutoItSalvageEntry);
    IntReport("  Identify/salvage runtime override: defer=%u autoItEntry=%u",
              useLegacyAutoItSalvageContext ? 1u : 0u,
              forceAutoItSalvageEntry ? 1u : 0u);
}

bool EnsureIdentifySalvageHarnessReady() {
    if (ReadMapId() != MapIds::GADDS_ENCAMPMENT) {
        IntReport("  Traveling to Gadd's Encampment (%u) for safe outpost salvage repro...", MapIds::GADDS_ENCAMPMENT);
        MapMgr::Travel(MapIds::GADDS_ENCAMPMENT);
        const bool atTargetMap = WaitFor("MapID changes to Gadd's", 60000, []() {
            return ReadMapId() == MapIds::GADDS_ENCAMPMENT;
        });
        IntCheck("Reached Gadd's Encampment", atTargetMap);
        if (!atTargetMap) {
            IntReport("");
            return false;
        }

        const bool myIdReady = WaitFor("MyID valid after travel to Gadd's", 30000, []() {
            return ReadMyId() > 0;
        });
        IntCheck("MyID valid after Gadd's travel", myIdReady);
        if (!myIdReady) {
            IntReport("");
            return false;
        }
    }

    if (!WaitForPlayerWorldReady(10000)) {
        IntSkip("Identify/salvage isolation", "Player world state not ready");
        IntReport("");
        return false;
    }
    return true;
}

bool RunIdentifyOnlyStage(uint32_t identifyCandidatesBefore) {
    const Item* idKit = FindHarnessIdKit();
    IntCheck("ID kit present", idKit != nullptr || identifyCandidatesBefore == 0);
    const uint32_t identified = MaintenanceMgr::IdentifyAllItems();
    IntReport("  Identify-only result: identified=%u candidatesBefore=%u candidatesAfter=%u",
              identified, identifyCandidatesBefore, CountIdentifyCandidates());
    ReportIdentifySalvageSummary("Post-identify summary");
    ReportGoldSalvageCandidates("Post-identify gold salvage candidates");
    IntCheck("Identify-only run completed", true);
    IntReport("");
    return true;
}

bool RunFullIdentifySalvageStage() {
    const uint32_t salvaged = MaintenanceMgr::IdentifyAndSalvageGoldItems();
    IntReport("  Full identify/salvage result: salvaged=%u", salvaged);
    ReportIdentifySalvageSummary("Post-full summary");
    ReportGoldSalvageCandidates("Post-full gold salvage candidates");
    IntCheck("Full identify/salvage run completed", true);
    IntReport("");
    return true;
}

void RunIdentifySalvagePrepPass() {
    const uint32_t identified = MaintenanceMgr::IdentifyAllItems();
    IntReport("  Prep identify pass before manual salvage stage: identified=%u", identified);
    ReportIdentifySalvageSummary("Post-prep-identify summary");
    ReportGoldSalvageCandidates("Post-prep-identify gold salvage candidates");
}

bool LoadIdentifySalvageManualContext(IdentifySalvageManualContext& context) {
    const Item* salvageKit = FindHarnessSalvageKit();
    IntCheck("Salvage kit present", salvageKit != nullptr);
    if (!salvageKit) {
        IntReport("");
        return false;
    }
    context.salvageKitId = salvageKit->item_id;
    context.salvageKitModel = salvageKit->model_id;

    const bool foundCandidate = FindGoldSalvageCandidate(context.candidate);
    IntCheck("Found salvageable gold candidate", foundCandidate);
    if (!foundCandidate) {
        IntSkip("Identify/salvage isolation", "No eligible gold salvage item found in bags 1-4");
        IntReport("");
        return false;
    }

    IntReport("  Selected salvage candidate: bag=%u slot=%u item=%u model=%u type=%u qty=%u rarity=%u id=%u salvageable=%u req=%u formula=%u kit=%u kitModel=%u",
              context.candidate.bagIndex,
              context.candidate.slotIndex,
              context.candidate.itemId,
              context.candidate.modelId,
              context.candidate.type,
              context.candidate.quantity,
              context.candidate.rarity,
              (context.candidate.interaction & 0x1u) ? 1u : 0u,
              context.candidate.salvageable,
              context.candidate.requirement,
              context.candidate.formula,
              context.salvageKitId,
              context.salvageKitModel);
    return true;
}

uint32_t ReadLegacyBotshubSalvageSessionId() {
    if (!Offsets::BasePointer) return 0u;
    __try {
        uintptr_t ctx = *reinterpret_cast<uintptr_t*>(Offsets::BasePointer);
        if (!ctx) return 0u;
        uintptr_t p1 = *reinterpret_cast<uintptr_t*>(ctx + 0x18);
        if (!p1) return 0u;
        uintptr_t p2 = *reinterpret_cast<uintptr_t*>(p1 + 0x2C);
        if (!p2) return 0u;
        return *reinterpret_cast<uint32_t*>(p2 + 0x690);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0u;
    }
}

uint32_t ReadTrackedItemIdSafe(Item* item) {
    if (!item) return 0u;
    __try {
        return item->item_id;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0u;
    }
}

bool RunLegacyBotshubTrackedChainStage(const IdentifySalvageManualContext& context) {
    const uint32_t initialSessionId = ReadLegacyBotshubSalvageSessionId();
    IntReport("  Tracked-chain initial salvage session=%u", initialSessionId);
    IntCheck("Tracked-chain initial salvage session valid", initialSessionId != 0u);
    if (!initialSessionId) {
        IntReport("");
        return false;
    }

    IdentifySalvageCandidate chain[2] = {};
    const uint32_t chainCount = CollectGoldSalvageCandidates(chain, 2u);
    IntCheck("Found two salvageable gold candidates", chainCount >= 2u);
    if (chainCount < 2u) {
        IntSkip("Identify/salvage isolation", "Need two eligible gold salvage items for tracked chain stage");
        IntReport("");
        return false;
    }

    for (uint32_t i = 0; i < chainCount; ++i) {
        IntReport("  Tracked-chain candidate [%u/%u]: bag=%u slot=%u item=%u model=%u type=%u tracked=0x%08X",
                  i + 1u,
                  chainCount,
                  chain[i].bagIndex,
                  chain[i].slotIndex,
                  chain[i].itemId,
                  chain[i].modelId,
                  chain[i].type,
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(chain[i].trackedPtr)));
    }

    IntReport("  Tracked-chain salvage [1/2]: SalvageItemLegacyBotshubTracked(kit=%u, item=%u, tracked=0x%08X)",
              context.salvageKitId,
              chain[0].itemId,
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(chain[0].trackedPtr)));
    const bool firstConsumed = MaintenanceMgr::SalvageItemLegacyBotshubTracked(
        context.salvageKitId, chain[0].itemId, chain[0].trackedPtr, false, 0u);

    uint32_t sessionAfterFirst = ReadLegacyBotshubSalvageSessionId();
    DWORD sessionRestoreMs = 0u;
    if (!sessionAfterFirst) {
        const DWORD restoreStart = GetTickCount();
        while ((GetTickCount() - restoreStart) < 5000u) {
            Sleep(25);
            sessionAfterFirst = ReadLegacyBotshubSalvageSessionId();
            if (sessionAfterFirst != 0u) {
                sessionRestoreMs = GetTickCount() - restoreStart;
                break;
            }
        }
    }
    IntReport("  Tracked-chain session after [1/2]=%u restoredAfterMs=%u",
              sessionAfterFirst,
              sessionRestoreMs);

    Item* secondTracked = ResolveCandidateItemFromCachedBag(chain[1]);
    uint32_t secondItemId = chain[1].itemId;
    const uint32_t secondTrackedId = ReadTrackedItemIdSafe(secondTracked);
    if (secondTrackedId != 0u) {
        secondItemId = secondTrackedId;
    }
    const uint32_t secondSessionId = sessionAfterFirst ? sessionAfterFirst : initialSessionId;
    const bool allowZeroSessionSecond = (secondSessionId == 0u);
    if (allowZeroSessionSecond) {
        IntReport("  Tracked-chain salvage [2/2] will proceed with zero session id");
    } else if (sessionAfterFirst == 0u) {
        IntReport("  Tracked-chain salvage [2/2] reusing initial session id=%u after live session collapsed", secondSessionId);
    }
    IntReport("  Tracked-chain salvage [2/2]: SalvageItemLegacyBotshubTracked(kit=%u, item=%u, tracked=0x%08X fromBag=0x%08X)",
              context.salvageKitId,
              secondItemId,
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(chain[1].trackedPtr)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(secondTracked)));
    const bool secondConsumed = MaintenanceMgr::SalvageItemLegacyBotshubTracked(
        context.salvageKitId,
        secondItemId,
        secondTracked ? secondTracked : chain[1].trackedPtr,
        false,
        sessionAfterFirst ? 0u : secondSessionId,
        allowZeroSessionSecond);

    IntReport("  Tracked-chain result: consumed1=%u tracked1Now=%u consumed2=%u tracked2Now=%u",
              firstConsumed ? 1u : 0u,
              ReadTrackedItemIdSafe(chain[0].trackedPtr),
              secondConsumed ? 1u : 0u,
              ReadTrackedItemIdSafe(chain[1].trackedPtr));
    ReportIdentifySalvageSummary("Post-legacy-botshub-tracked-chain summary");
    ReportGoldSalvageCandidates("Post-legacy-botshub-tracked-chain gold salvage candidates");
    IntCheck("Tracked-chain salvage consumed item 1", firstConsumed);
    IntCheck("Tracked-chain salvage consumed item 2", secondConsumed);
    IntReport("");
    return firstConsumed && secondConsumed;
}

bool FinishQueuedSalvageStage(const char* resultLabel, const char* summaryLabel,
                              const char* candidatesLabel, const char* checkLabel,
                              bool queued, uint32_t itemId) {
    Sleep(1500);
    Item* itemAfter = ItemMgr::GetItemById(itemId);
    IntReport("  %s result: queued=%u itemStillPresent=%u freeSlots=%u charGold=%u storageGold=%u",
              resultLabel,
              queued ? 1u : 0u,
              itemAfter ? 1u : 0u,
              MaintenanceMgr::CountFreeSlots(),
              ItemMgr::GetGoldCharacter(),
              ItemMgr::GetGoldStorage());
    ReportIdentifySalvageSummary(summaryLabel);
    ReportGoldSalvageCandidates(candidatesLabel);
    IntCheck(checkLabel, queued);
    IntReport("");
    return queued;
}

bool RunQueuedIdentifySalvageStage(IdentifySalvageIsolationStage stage,
                                  const IdentifySalvageManualContext& context) {
    const uint32_t kitId = context.salvageKitId;
    const uint32_t itemId = context.candidate.itemId;
    switch (stage) {
    case IdentifySalvageIsolationStage::NativeSalvage: {
        IntReport("  Native salvage: SalvageItemNative(kit=%u, item=%u)", kitId, itemId);
        const bool queued = MaintenanceMgr::SalvageItemNative(kitId, itemId);
        return FinishQueuedSalvageStage("Native-salvage", "Post-native-salvage summary",
                                        "Post-native-salvage gold salvage candidates",
                                        "Native-salvage command queued", queued, itemId);
    }
    case IdentifySalvageIsolationStage::NativeSalvageEnter: {
        IntReport("  Native salvage + Enter confirm: SalvageItemNative(kit=%u, item=%u, confirmByEnter=1)",
                  kitId, itemId);
        const bool queued = MaintenanceMgr::SalvageItemNative(kitId, itemId, true);
        return FinishQueuedSalvageStage("Native-salvage-enter", "Post-native-salvage-enter summary",
                                        "Post-native-salvage-enter gold salvage candidates",
                                        "Native-salvage-enter command queued", queued, itemId);
    }
    case IdentifySalvageIsolationStage::LegacyBotshubStartOnly: {
        IntReport("  Legacy botshub start-only: SalvageItemLegacyBotshub(kit=%u, item=%u, sendMaterials=0)",
                  kitId, itemId);
        const bool queued = MaintenanceMgr::SalvageItemLegacyBotshub(kitId, itemId, 0u, false, false);
        return FinishQueuedSalvageStage("Legacy-botshub-start-only", "Post-legacy-botshub-start-only summary",
                                        "Post-legacy-botshub-start-only gold salvage candidates",
                                        "Legacy-botshub-start-only command queued", queued, itemId);
    }
    case IdentifySalvageIsolationStage::LegacyBotshubSalvage: {
        IntReport("  Legacy botshub salvage: SalvageItemLegacyBotshub(kit=%u, item=%u)",
                  kitId, itemId);
        const bool queued = MaintenanceMgr::SalvageItemLegacyBotshub(kitId, itemId);
        return FinishQueuedSalvageStage("Legacy-botshub-salvage", "Post-legacy-botshub-salvage summary",
                                        "Post-legacy-botshub-salvage gold salvage candidates",
                                        "Legacy-botshub-salvage command queued", queued, itemId);
    }
    case IdentifySalvageIsolationStage::LegacyBotshubSalvageEnter: {
        IntReport("  Legacy botshub salvage + Enter: SalvageItemLegacyBotshub(kit=%u, item=%u, postConsumeEnter=1)",
                  kitId, itemId);
        const bool queued = MaintenanceMgr::SalvageItemLegacyBotshub(kitId, itemId, 0u, false, true, true);
        return FinishQueuedSalvageStage("Legacy-botshub-salvage-enter", "Post-legacy-botshub-salvage-enter summary",
                                        "Post-legacy-botshub-salvage-enter gold salvage candidates",
                                        "Legacy-botshub-salvage-enter command queued", queued, itemId);
    }
    default:
        return false;
    }
}

bool RunLegacyBotshubFollowupStage(IdentifySalvageIsolationStage stage,
                                  const IdentifySalvageManualContext& context) {
    constexpr uint32_t kLegacyFroggySessionCancel = 0x77u;
    constexpr uint32_t kLegacyFroggySessionDone = 0x78u;
    const uint32_t followupHeader =
        (stage == IdentifySalvageIsolationStage::LegacyBotshubSalvageDone)
            ? kLegacyFroggySessionDone
            : kLegacyFroggySessionCancel;
    IntReport("  Legacy botshub salvage + followup: SalvageItemLegacyBotshub(kit=%u, item=%u, followup=0x%X)",
              context.salvageKitId, context.candidate.itemId, followupHeader);
    const bool queued = MaintenanceMgr::SalvageItemLegacyBotshub(
        context.salvageKitId, context.candidate.itemId, followupHeader, true);
    return FinishQueuedSalvageStage("Legacy-botshub-salvage-followup",
                                    "Post-legacy-botshub-salvage-followup summary",
                                    "Post-legacy-botshub-salvage-followup gold salvage candidates",
                                    "Legacy-botshub-salvage-followup command queued",
                                    queued, context.candidate.itemId);
}

bool RunManualSingleSalvageStage(IdentifySalvageIsolationStage stage,
                                const IdentifySalvageManualContext& context) {
    IntReport("  Manual salvage: SalvageSessionOpen(kit=%u, item=%u)",
              context.salvageKitId, context.candidate.itemId);
    ItemMgr::SalvageSessionOpen(context.salvageKitId, context.candidate.itemId);
    Sleep(2000);

    if (stage == IdentifySalvageIsolationStage::SalvageOpenOnly) {
        IntCheck("Salvage session open survived", true);
        ReportIdentifySalvageSummary("Post-salvage-open summary");
        IntReport("");
        return true;
    }

    IntReport("  Manual salvage: SalvageMaterials()");
    ItemMgr::SalvageMaterials();
    Sleep(800);
    IntReport("  Manual salvage: SalvageSessionDone()");
    ItemMgr::SalvageSessionDone();
    Sleep(500);

    Item* itemAfter = ItemMgr::GetItemById(context.candidate.itemId);
    IntReport("  Single-salvage result: itemStillPresent=%u freeSlots=%u charGold=%u storageGold=%u",
              itemAfter ? 1u : 0u,
              MaintenanceMgr::CountFreeSlots(),
              ItemMgr::GetGoldCharacter(),
              ItemMgr::GetGoldStorage());
    ReportGoldSalvageCandidates("Post-single-salvage gold salvage candidates");
    IntCheck("Single-salvage sequence completed", true);
    IntReport("");
    return true;
}
