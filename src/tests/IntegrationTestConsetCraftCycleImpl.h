bool TestConsetCraftCycle() {
    IntReport("=== CONSET CRAFT CYCLE TEST (100k budget) ===");
    StartWatchdog();
    WriteConsumableHarnessStatus("conset_cycle_start", "conset", ReadMapId(), 0, 0, 0, 0, 0, 0, 0, "starting");

    if (!EnsureConsetCycleEmbarkReady()) return false;
    EnsureConsetGoldBudget();

    ConsetMaterialPrices prices{};
    if (!ReadConsetMaterialPrices(prices)) return false;

    const uint32_t targetConsets = ChooseConsetCycleTargetCount(prices);
    const ConsetMaterialCounts needed = BuildConsetMaterialNeed(targetConsets);
    BuyConsetMaterials(needed);

    const ConsetCraftPlan plan = BuildConsetCraftPlan();
    const ConsetCraftResult result = ExecuteConsetCraftPlan(plan);
    return ReportConsetCraftResult(result);
}
