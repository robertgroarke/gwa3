namespace Gwa3.UI.Core.Models;

public sealed class InventoryPolicy
{
    public PolicyRuleSet Pickup { get; set; } = new() { Mode = PolicyMode.All };

    public PolicyRuleSet Identify { get; set; } = new() { Mode = PolicyMode.All };

    public PolicyRuleSet SalvageMaterials { get; set; } = new();

    public PolicyRuleSet SalvageUpgrades { get; set; } = new();

    public PolicyRuleSet Sell { get; set; } = new();

    public PolicyRuleSet Store { get; set; } = new();

    public PolicyRuleSet KeepComponents { get; set; } = new();

    public List<UpgradeSalvageRule> UpgradeSalvageRules { get; set; } = new();
}

public sealed class PolicyRuleSet
{
    public PolicyMode Mode { get; set; } = PolicyMode.Disabled;

    public List<string> CategoryTags { get; set; } = new();

    public List<ItemRule> Rules { get; set; } = new();
}

public sealed class ItemRule
{
    public bool Enabled { get; set; } = true;

    public string Name { get; set; } = "";

    public int? ModelId { get; set; }

    public string ItemType { get; set; } = "";

    public ItemRarity Rarity { get; set; } = ItemRarity.Any;

    public string Material { get; set; } = "";

    public string ModifierPattern { get; set; } = "";

    public int? MinimumValueGold { get; set; }

    public string Notes { get; set; } = "";
}

public sealed class UpgradeSalvageRule
{
    public bool Enabled { get; set; } = true;

    public string Group { get; set; } = "";

    public string Name { get; set; } = "";

    public string ModifierPattern { get; set; } = "";

    public int SalvageIndex { get; set; } = 2;

    public List<string> ItemTypes { get; set; } = new();

    public UpgradeSalvageAction Action { get; set; } = UpgradeSalvageAction.Salvage;

    public ItemRarity MinimumRarity { get; set; } = ItemRarity.Gold;
}

public enum PolicyMode
{
    Disabled,
    All,
    IncludeRules,
    ExcludeRules
}

public enum ItemRarity
{
    Any,
    White,
    Blue,
    Purple,
    Gold,
    Green
}

public enum UpgradeSalvageAction
{
    Ignore,
    Salvage,
    Keep,
    Sell
}
