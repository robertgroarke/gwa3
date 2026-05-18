using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed class LaunchPlanValidator : ILaunchPlanValidator
{
    private static readonly IReadOnlyList<KnownLane> KnownLanes =
    [
        new("GWA3 SAMPLE ONE", 0, "sample-one", "gwa3_sample_one.dll", @"\\.\pipe\gwa3_llm_sample-one", "launch_sample_one_via_gwlauncher.au3"),
        new("GWA3 SAMPLE TWO", 1, "sample-two", "gwa3_sample_two.dll", @"\\.\pipe\gwa3_llm_sample-two", "launch_sample_two_via_gwlauncher.au3"),
        new("GWA3 SAMPLE THREE", 2, "trade-helper", "gwa3_trade.dll", @"\\.\pipe\gwa3_llm_trade", "launch_sample_three_via_gwlauncher.au3"),
        new("GWA3 SAMPLE FOUR", 3, "sample-four", "gwa3_sample_four.dll", @"\\.\pipe\gwa3_llm_sample-four", "launch_sample_four_via_gwlauncher.au3"),
        new("GWA3 SAMPLE FIVE", 4, "sample-five", "gwa3_sample_five.dll", @"\\.\pipe\gwa3_llm_sample-five", "launch_sample_five_via_gwlauncher.au3")
    ];

    public ValidationResult Validate(LaunchPlan plan)
    {
        var errors = new List<string>();
        var warnings = new List<string>();

        Required(plan.CharacterName, nameof(plan.CharacterName), errors);
        Required(plan.LaneTag, nameof(plan.LaneTag), errors);
        Required(plan.AutoItExecutablePath, nameof(plan.AutoItExecutablePath), errors);
        Required(plan.LauncherScriptPath, nameof(plan.LauncherScriptPath), errors);
        Required(plan.InjectorPath, nameof(plan.InjectorPath), errors);
        Required(plan.DllName, nameof(plan.DllName), errors);

        if (plan.AccountIndex < 0)
        {
            errors.Add("AccountIndex must be zero or greater.");
        }

        if (plan.Mode is LaunchMode.Llm or LaunchMode.Advisory)
        {
            Required(plan.Bridge.PipeName, "Bridge.PipeName", errors);
        }

        if (!plan.DryRun)
        {
            WarnMissingFile(plan.AutoItExecutablePath, nameof(plan.AutoItExecutablePath), warnings);
            WarnMissingFile(plan.LauncherScriptPath, nameof(plan.LauncherScriptPath), warnings);
            WarnMissingFile(plan.InjectorPath, nameof(plan.InjectorPath), warnings);
        }

        if (!string.IsNullOrWhiteSpace(plan.Bridge.ExpectedDllName)
            && !string.Equals(plan.Bridge.ExpectedDllName, plan.DllName, StringComparison.OrdinalIgnoreCase))
        {
            errors.Add("Bridge.ExpectedDllName must match DllName for lane-safe injection.");
        }

        ValidateKnownLane(plan, errors);

        return new ValidationResult
        {
            Errors = errors,
            Warnings = warnings
        };
    }

    private static void Required(string? value, string name, ICollection<string> errors)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            errors.Add($"{name} is required.");
        }
    }

    private static void WarnMissingFile(string path, string name, ICollection<string> warnings)
    {
        if (!string.IsNullOrWhiteSpace(path) && !File.Exists(path))
        {
            warnings.Add($"{name} does not exist yet: {path}");
        }
    }

    private static void ValidateKnownLane(LaunchPlan plan, ICollection<string> errors)
    {
        var lane = FindKnownLane(plan.CharacterName);
        if (lane is null)
        {
            return;
        }

        if (plan.AccountIndex != lane.AccountIndex)
        {
            errors.Add($"{plan.CharacterName} must launch with account index {lane.AccountIndex}, not {plan.AccountIndex}.");
        }

        if (!string.Equals(plan.LaneTag, lane.LaneTag, StringComparison.OrdinalIgnoreCase))
        {
            errors.Add($"{plan.CharacterName} must launch on lane '{lane.LaneTag}', not '{plan.LaneTag}'.");
        }

        if (!string.Equals(plan.DllName, lane.DllName, StringComparison.OrdinalIgnoreCase))
        {
            errors.Add($"{plan.CharacterName} must inject {lane.DllName}, not {plan.DllName}.");
        }

        if (!string.Equals(plan.Bridge.PipeName, lane.PipeName, StringComparison.OrdinalIgnoreCase))
        {
            errors.Add($"{plan.CharacterName} must use bridge pipe {lane.PipeName}, not {plan.Bridge.PipeName}.");
        }

        var launcherScriptName = Path.GetFileName(plan.LauncherScriptPath);
        if (!string.Equals(launcherScriptName, lane.LauncherScriptName, StringComparison.OrdinalIgnoreCase))
        {
            errors.Add($"{plan.CharacterName} must launch through {lane.LauncherScriptName}, not {launcherScriptName}.");
        }
    }

    private static KnownLane? FindKnownLane(string? characterName)
    {
        var key = NormalizeCharacterKey(characterName);
        return string.IsNullOrWhiteSpace(key)
            ? null
            : KnownLanes.FirstOrDefault(lane =>
                string.Equals(NormalizeCharacterKey(lane.CharacterName), key, StringComparison.OrdinalIgnoreCase));
    }

    private static string NormalizeCharacterKey(string? characterName) =>
        string.IsNullOrWhiteSpace(characterName)
            ? ""
            : new string(characterName.Where(character => !char.IsWhiteSpace(character)).ToArray()).ToLowerInvariant();

    private sealed record KnownLane(
        string CharacterName,
        int AccountIndex,
        string LaneTag,
        string DllName,
        string PipeName,
        string LauncherScriptName);
}
