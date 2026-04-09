// Session setup, login/bootstrap, and NPC/trader interaction slices.

#include "IntegrationTestInternal.h"

#include <gwa3/core/TraderHook.h>
#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/TradeMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>

namespace GWA3::SmokeTest {

namespace {

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

bool CheckLocalFlagFile(const char* flagFile) {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&CheckLocalFlagFile), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
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

constexpr uint32_t kMerchantRootHash = 3613855137u;
constexpr uint32_t kMapEmbarkBeach = 857u;
constexpr uint32_t kMapGadds = 638u;
constexpr float kEmbarkEyjaX = 3336.0f;
constexpr float kEmbarkEyjaY = 627.0f;
constexpr float kGaddsMerchantX = -8374.0f;
constexpr float kGaddsMerchantY = -22491.0f;

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

    GameThread::Enqueue([targetId]() {
        AgentMgr::InteractNPC(targetId);
    });
    Sleep(1500);
    IntCheck("InteractNPC sent (no crash)", true);

    constexpr uint32_t DIALOG_NPC_TALK = 0x2AE6;
    IntReport("  Sending dialog 0x%X...", DIALOG_NPC_TALK);
    GameThread::Enqueue([=]() {
        QuestMgr::Dialog(DIALOG_NPC_TALK);
    });
    Sleep(1000);
    IntCheck("Dialog sent (no crash)", true);

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
    IntReport("  Merchant interact path: legacy living interact header 0x38");

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

        traderAgentId = FindNearestNpcLikeAgentToCoords(target.x, target.y, 2500.0f);
        if (!traderAgentId) continue;

        for (int attempt = 1; attempt <= 3 && !merchantVisible; ++attempt) {
            traderAgentId = FindNearestNpcLikeAgentToCoords(target.x, target.y, 2500.0f);
            if (!traderAgentId) break;

            float npcX = 0.0f;
            float npcY = 0.0f;
            TryReadAgentPosition(traderAgentId, npcX, npcY);
            float meX = 0.0f;
            float meY = 0.0f;
            TryReadAgentPosition(ReadMyId(), meX, meY);
            IntReport("  Attempt %d: interacting with %s agent %u at (%.0f, %.0f)...",
                      attempt, target.label, traderAgentId, npcX, npcY);

            MovePlayerNear(npcX, npcY, 70.0f, 12000);
            TryReadAgentPosition(ReadMyId(), meX, meY);
            IntReport("    Player pos before interact: (%.0f, %.0f) dist=%.0f",
                      meX, meY, AgentMgr::GetDistance(meX, meY, npcX, npcY));
            ReportMerchantPreInteractState("Merchant harness pre-interact snapshot", traderAgentId, npcX, npcY);

            if (isolationStage == MerchantIsolationStage::ApproachOnly) {
                IntSkip("Merchant target/interact/dialog", "Approach-only isolation stage");
                IntReport("");
                return true;
            }

            AgentMgr::ChangeTarget(traderAgentId);
            Sleep(250);
            IntReport("    step 0 complete: ChangeTarget(%u)", traderAgentId);

            if (isolationStage == MerchantIsolationStage::TargetOnly) {
                IntSkip("Merchant interact/dialog", "Target-only isolation stage");
                IntReport("");
                return true;
            }

            if (isolationStage == MerchantIsolationStage::InteractAgentMgrOnly) {
                IntReport("    step 1: AgentMgr::InteractNPC(%u)", traderAgentId);
                AgentMgr::InteractNPC(traderAgentId);
                Sleep(4000);
                IntReport("    step 1 complete after AgentMgr dwell");
                const uint32_t merchantCountAfterAgentMgr = TradeMgr::GetMerchantItemCount();
                const uintptr_t merchantFrameAfterAgentMgr = UIMgr::GetFrameByHash(kMerchantRootHash);
                IntReport("      Merchant probe after AgentMgr dwell: frame=0x%08X items=%u",
                          merchantFrameAfterAgentMgr,
                          merchantCountAfterAgentMgr);
                IntSkip("Merchant dialog", "AgentMgr-interact-only isolation stage");
                IntReport("");
                return true;
            }

            IntReport("    step 1: SendPacket(2, 0x38, %u)", traderAgentId);
            CtoS::SendPacket(2, 0x38u, traderAgentId);
            Sleep(750);
            IntReport("    step 1 complete");

            if (isolationStage == MerchantIsolationStage::InteractSinglePacketOnly) {
                IntReport("    Single-packet isolation: waiting 4000ms after first raw interact...");
                Sleep(4000);
                IntReport("    Single-packet wait complete");
                const uint32_t merchantCountAfterSingle = TradeMgr::GetMerchantItemCount();
                const uintptr_t merchantFrameAfterSingle = UIMgr::GetFrameByHash(kMerchantRootHash);
                IntReport("      Merchant probe after single-packet dwell: frame=0x%08X items=%u",
                          merchantFrameAfterSingle,
                          merchantCountAfterSingle);
                IntSkip("Merchant dialog", "Interact-single-packet-only isolation stage");
                IntReport("");
                return true;
            }

            IntReport("    step 2: SendPacket(2, 0x38, %u)", traderAgentId);
            CtoS::SendPacket(2, 0x38u, traderAgentId);
            Sleep(250);
            IntReport("    step 2 complete");

            const uintptr_t traderAgentPtr = GetAgentPtrRaw(traderAgentId);
            IntReport("    Agent scalars: id=%u ptr=0x%08X", traderAgentId, traderAgentPtr);

            if (isolationStage == MerchantIsolationStage::InteractPacketOnly) {
                IntSkip("Merchant dialog", "Interact-packet-only isolation stage");
                IntReport("");
                return true;
            }

            if (isolationStage == MerchantIsolationStage::InteractDwellOnly) {
                IntReport("    Interact-dwell isolation: waiting 4000ms after raw interact...");
                Sleep(4000);
                IntReport("    Interact-dwell wait complete");
                const uint32_t merchantCountAfterDwell = TradeMgr::GetMerchantItemCount();
                const uintptr_t merchantFrameAfterDwell = UIMgr::GetFrameByHash(kMerchantRootHash);
                IntReport("      Merchant probe after dwell: frame=0x%08X items=%u",
                          merchantFrameAfterDwell,
                          merchantCountAfterDwell);
                IntSkip("Merchant dialog", "Interact-dwell-only isolation stage");
                IntReport("");
                return true;
            }

            if (isolationStage == MerchantIsolationStage::InteractOnly) {
                IntSkip("Merchant dialog", "Interact-only isolation stage");
                IntReport("");
                return true;
            }

            const uintptr_t dialogScalar = selectedUsesPtr ? traderAgentPtr : traderAgentId;
            if (selectedUsesPtr && dialogScalar <= 0x10000) {
                IntReport("    Selected variant requires live agent pointer but none was available");
                break;
            }

            IntReport("    Sending %s (header=0x%X value=0x%08X)",
                      DescribeMerchantDialogVariant(selectedVariant),
                      selectedHeader,
                      dialogScalar);
            CtoS::SendPacket(2, selectedHeader, static_cast<uint32_t>(dialogScalar));
            Sleep(750);

            const uint32_t merchantCountAfterVariant = TradeMgr::GetMerchantItemCount();
            const uintptr_t merchantFrame = UIMgr::GetFrameByHash(kMerchantRootHash);
            IntReport("      Merchant probe after variant: frame=0x%08X items=%u",
                      merchantFrame,
                      merchantCountAfterVariant);

            merchantVisible = WaitFor("merchant window visible", 2500, []() {
                return UIMgr::GetFrameByHash(kMerchantRootHash) != 0 || TradeMgr::GetMerchantItemCount() > 0;
            });
            if (merchantVisible) {
                quoteExpected = target.expectQuote;
                openedLabel = target.label;
                IntReport("    Merchant context opened via %s", DescribeMerchantDialogVariant(selectedVariant));
            }
            if (merchantVisible) {
                break;
            }

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

} // namespace GWA3::SmokeTest
