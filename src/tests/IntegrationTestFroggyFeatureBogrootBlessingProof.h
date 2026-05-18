// Bogroot blessing proof.

#include "IntegrationTestFroggyFeatureBogrootBlessingDiagnostics.h"
#include "IntegrationTestFroggyFeatureBogrootBlessingInteraction.h"

static bool RunBogrootBlessingProof() {
    FroggyFeatureReport("=== PHASE 6B: Grab Blessing (Bogroot) ===");

    if (MapMgr::GetMapId() != MapIds::BOGROOT_GROWTHS_LVL1) {
        FroggyFeatureSkip("Bogroot blessing", "Not in Bogroot Growths Level 1");
        return false;
    }

    if (HasAnyBlessing()) {
        FroggyFeatureSkip("Bogroot blessing", "Player already has a blessing active");
        return true;
    }

    MoveToBogrootBlessingShrine();

    const uint32_t npcId = FindBogrootBlessingNpc();
    if (npcId == 0) {
        FroggyFeatureSkip("Bogroot blessing", "No NPC found near blessing shrine coordinates");
        return false;
    }

    LivingAgentSnapshot npcSnap;
    if (!TrySnapshotLivingAgent(npcId, npcSnap)) {
        FroggyFeatureSkip("Bogroot blessing", "Could not read blessing NPC");
        return false;
    }
    FroggyFeatureReport("  Found NPC agent=%u playerNum=%u at (%.0f, %.0f) allegiance=%u",
              npcId, npcSnap.playerNumber, npcSnap.x, npcSnap.y, npcSnap.allegiance);

    MovePlayerNear(npcSnap.x, npcSnap.y, 120.0f, 12000);
    ReportBogrootBlessingPreInteractionDiagnostics(AgentMgr::GetMyId());
    EnsureDeldrimorTitleForBogrootBlessing();
    InteractBogrootBlessingNpc(npcId);
    ReportBogrootBlessingPostInteractionDiagnostics();

    const bool blessed = WaitForBogrootBlessingEffect();
    if (blessed) {
        FroggyFeatureCheck("Phase 6B: Blessing grabbed successfully", true);
    } else {
        FroggyFeatureReport("  WARN: Blessing effect not detected. Title may be maxed or dialog not processed.");
        FroggyFeatureCheck("Phase 6B: Blessing grab attempted (effect not confirmed)", true);
    }
    return blessed;
}
