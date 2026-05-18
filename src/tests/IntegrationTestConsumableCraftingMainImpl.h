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
    IntReport("  Consumable harness target: %s", DescribeConsumableHarnessTarget(selectedTarget));
    IntReport("  Consumable harness click mode: %s", DescribeConsumableHarnessClickMode(clickMode));

    char detail[160] = {};
    sprintf_s(detail, "map=%u region=%u district=%u",
              ReadMapId(), MapMgr::GetRegion(), MapMgr::GetDistrict());
    WriteConsumableHarnessStatus("starting", DescribeConsumableHarnessTarget(selectedTarget),
                                 ReadMapId(), 0, 0, 0, 0, 0, 0, 0, detail);

    if (!EnsureQuietEmbarkForConsumableHarness(selectedTarget) ||
        !EnsureConsumableHarnessWorldReady(selectedTarget)) {
        IntReport("");
        return false;
    }

    bool anySuccess = false;
    for (const auto& target : kConsumableTargets) {
        if (!ConsumableTargetMatchesSelection(selectedTarget, target)) continue;
        if (!ApproachConsumableTarget(target)) continue;

        if (stage == ConsumableHarnessStage::TravelOnly) {
            WriteConsumableHarnessStatus("travel_only_complete", target.label, ReadMapId(), 0, 0,
                                         target.modelId, 0, 0, 0, 1, "travel_only_stage_complete");
            IntSkip("Consumable crafter open", "Travel-only consumable isolation stage");
            IntReport("");
            return true;
        }

        const ConsumableCrafterProbe probe = OpenConsumableCrafterForTarget(target, stage);
        if (!probe.inventoryReady) continue;

        anySuccess = true;
        if (stage == ConsumableHarnessStage::OpenOnly) {
            WriteConsumableHarnessStatus("open_complete", target.label, ReadMapId(), probe.crafterAgentId,
                                         probe.merchantItemCount, target.modelId, probe.crafterItemId, 0, 0, 1,
                                         "open_only_stage_complete");
            IntSkip("Consumable inventory list", "Open-only consumable isolation stage");
            IntReport("");
            return true;
        }

        if (stage == ConsumableHarnessStage::ListOnly) {
            WriteConsumableHarnessStatus("list_complete", target.label, ReadMapId(), probe.crafterAgentId,
                                         probe.merchantItemCount, target.modelId, probe.crafterItemId, 0, 0, 1,
                                         "list_only_stage_complete");
            IntSkip("Consumable craft transact", "List-only consumable isolation stage");
            IntReport("");
            return true;
        }

        CraftConsumableTargetViaUi(target, probe, clickMode);
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
