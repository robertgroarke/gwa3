#include <gwa3/managers/MerchantMgr.h>
static void ReportMerchantPreInteractState(const char* label, uint32_t npcId, float npcX, float npcY) {
    float meX = 0.0f, meY = 0.0f;
    TryReadAgentPosition(ReadMyId(), meX, meY);
    const uint32_t currentTarget = AgentMgr::GetTargetId();
    const bool dialogOpen = DialogMgr::IsDialogOpen();
    const uint32_t dialogSender = DialogMgr::GetDialogSenderAgentId();
    const uint32_t dialogButtons = DialogMgr::GetButtonCount();
    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(3613855137u);
    const uint32_t merchantItems = MerchantMgr::GetMerchantItemCount();
    const uint32_t heroCount = PartyMgr::CountPartyHeroes();
    const float dist = AgentMgr::GetDistance(meX, meY, npcX, npcY);
    FroggyFeatureReport("  %s: npc=%u playerPos=(%.0f,%.0f) npcPos=(%.0f,%.0f) dist=%.0f target=%u dialogOpen=%d sender=%u buttons=%u merchantFrame=0x%08X items=%u heroes=%u",
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
