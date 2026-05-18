#include <gwa3/managers/MerchantMgr.h>
struct ConsumableTarget {
    const char* label;
    float x;
    float y;
    uint32_t modelId;
};

struct ConsumableCrafterProbe {
    bool inventoryReady = false;
    uint32_t crafterAgentId = 0;
    uint32_t merchantItemCount = 0;
    uint32_t crafterItemId = 0;
};

const ConsumableTarget kConsumableTargets[] = {
    {"Eyja", kEmbarkEyjaX, kEmbarkEyjaY, ItemModelIds::GRAIL_OF_MIGHT},
    {"Kwat", kEmbarkKwatX, kEmbarkKwatY, ItemModelIds::ESSENCE_OF_CELERITY},
    {"Alcus", kEmbarkAlcusX, kEmbarkAlcusY, ItemModelIds::ARMOR_OF_SALVATION},
};

bool ConsumableTargetMatchesSelection(ConsumableHarnessTarget selectedTarget, const ConsumableTarget& target) {
    if (selectedTarget == ConsumableHarnessTarget::Grail) return target.modelId == ItemModelIds::GRAIL_OF_MIGHT;
    if (selectedTarget == ConsumableHarnessTarget::Essence) return target.modelId == ItemModelIds::ESSENCE_OF_CELERITY;
    if (selectedTarget == ConsumableHarnessTarget::Armor) return target.modelId == ItemModelIds::ARMOR_OF_SALVATION;
    return true;
}

bool EnsureQuietEmbarkForConsumableHarness(ConsumableHarnessTarget selectedTarget) {
    if (ReadMapId() == MapIds::EMBARK_BEACH && MapMgr::GetRegion() == kTradeTestRegion && MapMgr::GetDistrict() == UINT32_MAX) {
        IntReport("  Embark district is unresolved; waiting briefly for world state to settle before forcing travel");
        const bool districtResolved = WaitFor("Embark district resolves before quiet-district travel", 10000, []() {
            return ReadMyId() > 0 && MapMgr::GetDistrict() != UINT32_MAX;
        });
        IntReport("  Embark district settle result: resolved=%d district=%u", districtResolved ? 1 : 0, MapMgr::GetDistrict());
    }

    const bool alreadyInQuietEmbarkDistrict =
        ReadMapId() == MapIds::EMBARK_BEACH &&
        MapMgr::GetRegion() == kTradeTestRegion &&
        MapMgr::GetDistrict() == kTradeTestDistrict;
    if (alreadyInQuietEmbarkDistrict) return true;

    const char* targetLabel = DescribeConsumableHarnessTarget(selectedTarget);
    IntReport("  Traveling to Embark Beach (%u) in Asia/Japan district %u for consumable crafter diagnostics...",
              MapIds::EMBARK_BEACH, kTradeTestDistrict);
    WriteConsumableHarnessStatus("traveling", targetLabel,
                                 ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "requesting_quiet_embark_travel");
    MapMgr::Travel(MapIds::EMBARK_BEACH, kTradeTestRegion, kTradeTestDistrict, kTradeTestLanguage);

    bool atTargetMap = WaitForConsumableTravelState(
        "travel_wait_preferred",
        targetLabel,
        MapIds::EMBARK_BEACH,
        kTradeTestRegion,
        kTradeTestDistrict,
        60000);
    if (!atTargetMap) {
        IntReport("  Preferred quiet district did not load; falling back to Asia/Japan district %u", kTradeFallbackDistrict);
        char fallbackDetail[160] = {};
        sprintf_s(fallbackDetail, "preferred_failed map=%u region=%u district=%u myId=%u loading=%u",
                  ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict(), ReadMyId(), MapMgr::GetLoadingState());
        WriteConsumableHarnessStatus("travel_fallback_start", targetLabel,
                                     ReadMapId(), 0, 0, 0, 0, 0, 0, 0, fallbackDetail);
        if (ReadMapId() == MapIds::EMBARK_BEACH &&
            MapMgr::GetRegion() == kTradeTestRegion &&
            MapMgr::GetDistrict() == kTradeFallbackDistrict &&
            ReadMyId() > 0 &&
            MapMgr::GetLoadingState() == 1) {
            WriteConsumableHarnessStatus("travel_fallback_already_loaded", targetLabel,
                                         ReadMapId(), 0, 0, 0, 0, 0, 0, 1, "already_loaded_in_fallback_district");
            atTargetMap = true;
        } else {
            MapMgr::Travel(MapIds::EMBARK_BEACH, kTradeTestRegion, kTradeFallbackDistrict, kTradeTestLanguage);
            atTargetMap = WaitForConsumableTravelState(
                "travel_wait_fallback",
                targetLabel,
                MapIds::EMBARK_BEACH,
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
        WriteConsumableHarnessStatus("travel_failed", targetLabel,
                                     ReadMapId(), 0, 0, 0, 0, 0, 0, 0, failDetail);
        return false;
    }

    const bool myIdReady = WaitFor("MyID valid after travel to quiet Embark district", 30000, []() {
        return ReadMyId() > 0;
    });
    IntCheck("MyID valid after quiet Embark travel", myIdReady);
    if (!myIdReady) {
        WriteConsumableHarnessStatus("travel_failed", targetLabel,
                                     ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "myid_not_ready_after_quiet_travel");
    }
    return myIdReady;
}

bool EnsureConsumableHarnessWorldReady(ConsumableHarnessTarget selectedTarget) {
    const char* targetLabel = DescribeConsumableHarnessTarget(selectedTarget);
    char detail[160] = {};
    sprintf_s(detail, "map=%u region=%u district=%u waiting_for_world_ready",
              ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
    WriteConsumableHarnessStatus("world_wait", targetLabel,
                                 ReadMapId(), 0, 0, 0, 0, 0, 0, 0, detail);
    if (!WaitForPlayerWorldReady(10000)) {
        WriteConsumableHarnessStatus("world_not_ready", targetLabel,
                                     ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "player_world_state_not_ready");
        IntSkip("Consumable crafting", "Player world state not ready");
        return false;
    }
    sprintf_s(detail, "map=%u region=%u district=%u world_ready",
              ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
    WriteConsumableHarnessStatus("world_ready", targetLabel,
                                 ReadMapId(), 0, 0, 0, 0, 0, 0, 1, detail);
    return true;
}

bool ApproachConsumableTarget(const ConsumableTarget& target) {
    char detail[160] = {};
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
        return false;
    }

    IntReport("  Nearby NPC candidates around %s:", target.label);
    DumpNpcLikeAgentsNearCoords(target.x, target.y, 900.0f, 8);
    return true;
}

bool ProbeConsumableCrafterCandidate(const ConsumableTarget& target, uint32_t crafterAgentId,
                                     size_t candidateIndex, size_t candidateCount,
                                     ConsumableHarnessStage stage,
                                     ConsumableCrafterProbe& probe) {
    if (!crafterAgentId) return false;
    probe.crafterAgentId = crafterAgentId;

    char detail[160] = {};
    float npcX = 0.0f;
    float npcY = 0.0f;
    TryReadAgentPosition(crafterAgentId, npcX, npcY);
    IntReport("    Candidate %u/%u: agent=%u pos=(%.0f, %.0f)",
              static_cast<unsigned>(candidateIndex + 1),
              static_cast<unsigned>(candidateCount),
              crafterAgentId, npcX, npcY);
    sprintf_s(detail, "candidate=%u/%u npc=(%.0f,%.0f)",
              static_cast<unsigned>(candidateIndex + 1), static_cast<unsigned>(candidateCount), npcX, npcY);
    WriteConsumableHarnessStatus("candidate_selected", target.label, ReadMapId(), crafterAgentId, 0,
                                 target.modelId, 0, 0, 0, 1, detail);

    const bool nearNpc = MovePlayerNear(npcX, npcY, 120.0f, 12000);
    sprintf_s(detail, "near_npc=%u npc=(%.0f,%.0f)", nearNpc ? 1u : 0u, npcX, npcY);
    WriteConsumableHarnessStatus("candidate_approach", target.label, ReadMapId(), crafterAgentId, 0,
                                 target.modelId, 0, 0, 0, nearNpc ? 1u : 0u, detail);
    if (!nearNpc) {
        IntReport("      Could not get close enough to candidate %u; trying next candidate", crafterAgentId);
        return false;
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
    bool merchantReady = WaitFor("consumable crafter merchant context", 2000, []() {
        return UIMgr::GetFrameByHash(kMerchantRootHash) != 0 || MerchantMgr::GetMerchantItemCount() > 0;
    });
    sprintf_s(detail, "merchant_ready=%u frame=0x%08X items=%u",
              merchantReady ? 1u : 0u,
              static_cast<unsigned>(UIMgr::GetFrameByHash(kMerchantRootHash)),
              MerchantMgr::GetMerchantItemCount());
    WriteConsumableHarnessStatus("open_probe", target.label, ReadMapId(), crafterAgentId,
                                 MerchantMgr::GetMerchantItemCount(), target.modelId, 0, 0, 0,
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
            return UIMgr::GetFrameByHash(kMerchantRootHash) != 0 || MerchantMgr::GetMerchantItemCount() > 0;
        });
        sprintf_s(detail, "merchant_ready=%u after_raw_packet frame=0x%08X items=%u",
                  merchantReady ? 1u : 0u,
                  static_cast<unsigned>(UIMgr::GetFrameByHash(kMerchantRootHash)),
                  MerchantMgr::GetMerchantItemCount());
        WriteConsumableHarnessStatus("open_probe_raw", target.label, ReadMapId(), crafterAgentId,
                                     MerchantMgr::GetMerchantItemCount(), target.modelId, 0, 0, 0,
                                     merchantReady ? 1u : 0u, detail);
    }

    if (!merchantReady) {
        IntReport("      No merchant context observed for candidate %u; trying next candidate", crafterAgentId);
        return false;
    }

    probe.merchantItemCount = MerchantMgr::GetMerchantItemCount();
    ReportMerchantInventoryList("Consumable crafter inventory");
    probe.crafterItemId = MerchantMgr::GetMerchantItemIdByModelId(target.modelId);
    if (stage == ConsumableHarnessStage::ListOnly) {
        WriteConsumableHarnessStatus("frame_dump_begin", target.label, ReadMapId(), crafterAgentId,
                                     probe.merchantItemCount, target.modelId, probe.crafterItemId, 0, 0, 1,
                                     "dumping_target_candidate_frame_tree");
        DumpConsumableFrameTree(UIMgr::GetFrameByHash(kMerchantRootHash),
                                FindMerchantItemPositionByModelId(target.modelId),
                                target.modelId,
                                probe.crafterItemId);
        WriteConsumableHarnessStatus("frame_dump_complete", target.label, ReadMapId(), crafterAgentId,
                                     probe.merchantItemCount, target.modelId, probe.crafterItemId, 0, 0, 1,
                                     "target_candidate_frame_tree_dumped");
    }
    IntCheck("Consumable target model present in crafter inventory", probe.crafterItemId != 0);
    if (probe.crafterItemId == 0) {
        BuildMerchantInventorySummary(detail, sizeof(detail));
        WriteConsumableHarnessStatus("list_failed", target.label, ReadMapId(), crafterAgentId,
                                     probe.merchantItemCount, target.modelId, 0, 0, 0, 0, detail);
        return false;
    }

    probe.inventoryReady = true;
    return true;
}

ConsumableCrafterProbe OpenConsumableCrafterForTarget(const ConsumableTarget& target,
                                                      ConsumableHarnessStage stage) {
    ConsumableCrafterProbe probe{};
    uint32_t candidateIds[8]{};
    const size_t candidateCount = CollectNearestNpcLikeAgentsToCoords(target.x, target.y, 900.0f, candidateIds, _countof(candidateIds));
    IntCheck("Found consumable crafter candidates", candidateCount > 0);

    char detail[160] = {};
    sprintf_s(detail, "candidate_count=%u map=%u region=%u district=%u",
              static_cast<unsigned>(candidateCount), ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
    WriteConsumableHarnessStatus("candidate_scan", target.label, ReadMapId(), 0, 0,
                                 target.modelId, 0, 0, 0, candidateCount > 0 ? 1u : 0u, detail);
    if (candidateCount == 0) {
        WriteConsumableHarnessStatus("candidate_failed", target.label, ReadMapId(), 0, 0,
                                     target.modelId, 0, 0, 0, 0, "no_crafter_candidates_found");
        return probe;
    }

    for (size_t i = 0; i < candidateCount && !probe.inventoryReady; ++i) {
        ProbeConsumableCrafterCandidate(target, candidateIds[i], i, candidateCount, stage, probe);
    }

    IntCheck("Consumable crafter merchant context available", probe.inventoryReady);
    if (!probe.inventoryReady) {
        WriteConsumableHarnessStatus("open_failed", target.label, ReadMapId(), probe.crafterAgentId, 0,
                                     target.modelId, 0, 0, 0, 0, "merchant_context_not_observed");
    }
    return probe;
}

void CraftConsumableTargetViaUi(const ConsumableTarget& target, const ConsumableCrafterProbe& probe,
                                ConsumableHarnessClickMode clickMode) {
    uint32_t beforeInventoryCount = 0;
    uint32_t afterInventoryCount = 0;
    char uiDetail[160] = {};
    char detail[160] = {};
    IntReport("  Crafting one item for model=%u item=%u via UI row-selection path...", target.modelId, probe.crafterItemId);
    const bool uiClicked = CraftConsumableViaUiClick(target.label, target.modelId, probe.crafterItemId, clickMode,
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
    WriteConsumableHarnessStatus("craft_complete", target.label, ReadMapId(), probe.crafterAgentId,
                                 probe.merchantItemCount, target.modelId, probe.crafterItemId,
                                 beforeInventoryCount, afterInventoryCount,
                                 afterInventoryCount > beforeInventoryCount ? 1u : 0u,
                                 detail);
}
