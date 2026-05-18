namespace Gwa3.UI.Core.Models;

public sealed class MaintenanceSettings
{
    public int MinimumFreeSlots { get; set; } = 5;

    public KitMaintenanceSettings Kits { get; set; } = new();

    public GoldMaintenanceSettings Gold { get; set; } = new();

    public ConsetCraftingSettings Consets { get; set; } = new();
}

public sealed class KitMaintenanceSettings
{
    public int MinimumIdentificationKits { get; set; } = 1;

    public int MinimumSalvageKits { get; set; } = 1;

    public int TargetIdentificationKits { get; set; } = 3;

    public int TargetSalvageKits { get; set; } = 8;

    public int TargetExpertSalvageKits { get; set; } = 1;
}

public sealed class GoldMaintenanceSettings
{
    public int KeepOnCharacterGold { get; set; } = 5_000;

    public int DepositWhenCharacterGoldAtLeast { get; set; } = 80_000;
}

public sealed class ConsetCraftingSettings
{
    public bool Enabled { get; set; }

    public bool RestockFromStorage { get; set; } = true;

    public int CraftWhenBelowSets { get; set; } = 5;

    public int TargetStoredSetsEach { get; set; } = 25;

    public int MaterialSlotTrigger { get; set; } = 10;

    public int MaterialPressureFreeSlots { get; set; } = 10;

    public int BatchSets { get; set; } = 10;
}
