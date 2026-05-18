#include <gwa3/managers/MerchantMgr.h>
// Merchant quote integration test body. Included by IntegrationTestSession.cpp
// so it can use the session-local harness helpers while extraction continues.

bool TestMerchantQuote() {
    IntReport("===  Merchant + Trader Quote ===");

    if (ReadMapId() == 0 || ReadMyId() == 0) {
        IntSkip("Merchant quote", "Not in game");
        IntReport("");
        return false;
    }

    const bool useGaddsTarget = UseGaddsMerchantTarget();
    const uint32_t targetMapId = useGaddsTarget ? MapIds::GADDS_ENCAMPMENT : MapIds::EMBARK_BEACH;
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
                const uint32_t merchantCountAfterAgentMgr = MerchantMgr::GetMerchantItemCount();
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

                const uint32_t merchantCountAfterPacket = MerchantMgr::GetMerchantItemCount();
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
            const uint32_t merchantCountAfterNativeInteract = MerchantMgr::GetMerchantItemCount();
            const uintptr_t merchantFrameAfterNativeInteract = UIMgr::GetFrameByHash(kMerchantRootHash);
            IntReport("      Merchant probe after native-interact dwell: frame=0x%08X items=%u",
                      merchantFrameAfterNativeInteract,
                      merchantCountAfterNativeInteract);
            EmitMerchantScreenshotMarker("merchant_harness_after_native_interact_dwell");
            merchantVisible = WaitFor("merchant window visible after native interact dwell", 2500, []() {
                return UIMgr::GetFrameByHash(kMerchantRootHash) != 0 || MerchantMgr::GetMerchantItemCount() > 0;
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

    const uint32_t merchantCount = MerchantMgr::GetMerchantItemCount();
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
        selectedItemId = MerchantMgr::GetMerchantItemIdByModelId(modelId);
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
    const bool requestQueued = MerchantMgr::RequestTraderQuoteByItemId(selectedItemId);
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
