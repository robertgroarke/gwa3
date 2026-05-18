namespace Gwa3.UI.Core.Models;

public static class KnownBotModules
{
    public const string FroggyHM = "FroggyHM";
    public const string RragarsMenagerie = "RragarsMenagerie";
    public const string Kathandrax = "Kathandrax";
    public const string FrostmawsBurrows = "FrostmawsBurrows";
    public const string RavensPoint = "RavensPoint";
    public const string ArachnisHaunt = "ArachnisHaunt";

    public static readonly IReadOnlySet<string> All = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
    {
        FroggyHM,
        RragarsMenagerie,
        Kathandrax,
        FrostmawsBurrows,
        RavensPoint,
        ArachnisHaunt
    };
}

public sealed class BotSelection
{
    public string ModuleId { get; set; } = KnownBotModules.FroggyHM;

    public string DisplayName { get; set; } = "Froggy HM";

    public string Objective { get; set; } = "";

    public string DefaultOutpost { get; set; } = "";

    public List<string> Tags { get; set; } = new();
}
