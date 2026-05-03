#pragma once

#include <gwa3/advanced/Interactions.h>

namespace GWA3::DungeonInteractions {

using AdvancedInteractions::AgentLogFn;
using AdvancedInteractions::BoolFn;
using AdvancedInteractions::CandidateDialogOptions;
using AdvancedInteractions::CandidateDialogResult;
using AdvancedInteractions::CollectNearestInteractCandidates;
using AdvancedInteractions::CollectNearestNpcs;
using AdvancedInteractions::DirectNpcInteractOptions;
using AdvancedInteractions::DirectNpcInteractResult;
using AdvancedInteractions::DirectNpcInteractStopFn;
using AdvancedInteractions::DoorOpenOptions;
using AdvancedInteractions::DropHeldBundle;
using AdvancedInteractions::FailureProbeFn;
using AdvancedInteractions::FindNearestChestSignpost;
using AdvancedInteractions::FindNearestItem;
using AdvancedInteractions::FindNearestItemByModel;
using AdvancedInteractions::FindNearestNpc;
using AdvancedInteractions::FindNearestSignpost;
using AdvancedInteractions::GetHeldBundleItemId;
using AdvancedInteractions::InteractCandidate;
using AdvancedInteractions::InteractCandidateAndSendDialog;
using AdvancedInteractions::IsChestGadgetId;
using AdvancedInteractions::IsChestStillPresentNear;
using AdvancedInteractions::MakeDoorOpenOptions;
using AdvancedInteractions::MoveToPointResultFn;
using AdvancedInteractions::OpenDoorAt;
using AdvancedInteractions::OpenDoorAtWithProbe;
using AdvancedInteractions::OpenedChestTracker;
using AdvancedInteractions::PulseDirectNpcInteract;
using AdvancedInteractions::ResetOpenedChestTrackerForCurrentMap;
using AdvancedInteractions::ResolveGenericChestFallback;
using AdvancedInteractions::SignpostScanLogFn;
using AdvancedInteractions::WaitFn;

} // namespace GWA3::DungeonInteractions
