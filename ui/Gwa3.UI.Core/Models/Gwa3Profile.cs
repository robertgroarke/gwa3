using System.Text.Json;

namespace Gwa3.UI.Core.Models;

public sealed class Gwa3Profile
{
    public const int CurrentSchemaVersion = 1;

    public int SchemaVersion { get; set; } = CurrentSchemaVersion;

    public string ProfileName { get; set; } = "";

    public string Description { get; set; } = "";

    public BotSelection Bot { get; set; } = new();

    public CharacterSettings Character { get; set; } = new();

    public BuildSettings Build { get; set; } = new();

    public RuntimeSettings Runtime { get; set; } = new();

    public MaintenanceSettings Maintenance { get; set; } = new();

    public InventoryPolicy InventoryPolicy { get; set; } = new();

    public LaunchSettings Launch { get; set; } = new();

    public LlmSettings Llm { get; set; } = new();

    public Dictionary<string, JsonElement> BotSpecific { get; set; } = new(StringComparer.OrdinalIgnoreCase);
}

public sealed class RuntimeSettings
{
    public bool HardMode { get; set; } = true;

    public bool UseConsets { get; set; }

    public bool UseStones { get; set; }

    public bool DisableRendering { get; set; }

    public bool OpenChests { get; set; } = true;

    public bool PickupGold { get; set; } = true;

    public bool AutoIdentify { get; set; } = true;

    public bool AutoSalvage { get; set; } = true;
}
