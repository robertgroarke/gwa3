using System.Globalization;

namespace Gwa3.UI.Core.Models;

public sealed record FroggyRunStartSnapshot(int RunNumber);

public sealed record FroggyRunCompletionSnapshot(
    int RunNumber,
    int DurationMilliseconds,
    int BestMilliseconds);

public sealed record FroggyMonitoringStatsSnapshot
{
    public string? Reason { get; init; }
    public int DeldrimorPoints { get; init; }
    public int AsuraPoints { get; init; }
    public int NornPoints { get; init; }
    public int VanguardPoints { get; init; }
    public int Lockpicks { get; init; }
    public int Wipes { get; init; }
    public int RareSkins { get; init; }
    public int GoldItems { get; init; }
    public int DroppedLockpicks { get; init; }
    public int ChestsOpened { get; init; }
    public int BlackDyes { get; init; }
    public int Tomes { get; init; }
    public int? ItemsPicked { get; init; }
    public int? SkinsPicked { get; init; }
    public int? LockpicksGained { get; init; }
    public bool? InventoryBaselineReady { get; init; }
    public int? DeldrimorTotal { get; init; }
    public int? AsuraTotal { get; init; }
    public int? NornTotal { get; init; }
    public int? VanguardTotal { get; init; }
    public int? CharacterGold { get; init; }
    public int? StorageGold { get; init; }
    public int? Experience { get; init; }
    public int? MapId { get; init; }
    public int? FreeSlots { get; init; }
    public int? IdentificationKits { get; init; }
    public int? SalvageKits { get; init; }
    public int? RegularSalvageKits { get; init; }
    public int? HighGradeSalvageKits { get; init; }
    public int? ConsetsInventory { get; init; }
    public int? ConsetsStorage { get; init; }
    public int? GrailsInventory { get; init; }
    public int? EssencesInventory { get; init; }
    public int? ArmorsInventory { get; init; }
    public int? GrailsStorage { get; init; }
    public int? EssencesStorage { get; init; }
    public int? ArmorsStorage { get; init; }
    public int? DustInventory { get; init; }
    public int? IronInventory { get; init; }
    public int? BonesInventory { get; init; }
    public int? FeathersInventory { get; init; }
    public int? DustStorage { get; init; }
    public int? IronStorage { get; init; }
    public int? BonesStorage { get; init; }
    public int? FeathersStorage { get; init; }
    public bool? SkillbarReady { get; init; }
    public int? SkillbarNonZero { get; init; }
    public IReadOnlyList<int> SkillbarSkillIds { get; init; } = Array.Empty<int>();
}

public static class FroggyMonitoringParser
{
    public static FroggyRunStartSnapshot? TryParseRunStart(string text)
    {
        if (string.IsNullOrWhiteSpace(text) ||
            !text.Contains("State: TownSetup", StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        var runNumber = TryExtractIntAfter(text, "(run #");
        return runNumber is int value ? new FroggyRunStartSnapshot(value) : null;
    }

    public static FroggyRunCompletionSnapshot? TryParseRunCompletion(string text)
    {
        if (string.IsNullOrWhiteSpace(text) ||
            !text.Contains("Run #", StringComparison.OrdinalIgnoreCase) ||
            !text.Contains("complete in", StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        var runNumber = TryExtractIntAfter(text, "Run #");
        var duration = TryExtractIntAfter(text, "complete in ");
        var best = TryExtractIntAfter(text, "best: ");

        return runNumber is int run && duration is int durationMs
            ? new FroggyRunCompletionSnapshot(run, durationMs, best ?? durationMs)
            : null;
    }

    public static FroggyMonitoringStatsSnapshot? TryParseMonitoringStats(string text)
    {
        if (string.IsNullOrWhiteSpace(text) ||
            !text.Contains("MonitoringStats", StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        var deldrimor = TryExtractKeyValueInt(text, "deldrimor");
        var asura = TryExtractKeyValueInt(text, "asura");
        var norn = TryExtractKeyValueInt(text, "norn");
        var vanguard = TryExtractKeyValueInt(text, "vanguard");
        var lockpicks = TryExtractKeyValueInt(text, "lockpicks");
        var wipes = TryExtractKeyValueInt(text, "wipes");
        var rareSkins = TryExtractKeyValueInt(text, "rareSkins");
        var goldItems = TryExtractKeyValueInt(text, "goldItems");
        var droppedLockpicks = TryExtractKeyValueInt(text, "droppedLockpicks");
        var chestsOpened = TryExtractKeyValueInt(text, "chestsOpened");
        var blackDyes = TryExtractKeyValueInt(text, "blackDyes");
        var tomes = TryExtractKeyValueInt(text, "tomes");

        return deldrimor is int deldrimorValue &&
               asura is int asuraValue &&
               norn is int nornValue &&
               vanguard is int vanguardValue &&
               lockpicks is int lockpickValue &&
               wipes is int wipesValue &&
               rareSkins is int rareSkinsValue &&
               goldItems is int goldItemsValue &&
               droppedLockpicks is int droppedLockpicksValue &&
               chestsOpened is int chestsOpenedValue &&
               blackDyes is int blackDyesValue &&
               tomes is int tomesValue
            ? new FroggyMonitoringStatsSnapshot
            {
                Reason = TryExtractKeyValueText(text, "reason"),
                DeldrimorPoints = deldrimorValue,
                AsuraPoints = asuraValue,
                NornPoints = nornValue,
                VanguardPoints = vanguardValue,
                Lockpicks = lockpickValue,
                Wipes = wipesValue,
                RareSkins = rareSkinsValue,
                GoldItems = goldItemsValue,
                DroppedLockpicks = droppedLockpicksValue,
                ChestsOpened = chestsOpenedValue,
                BlackDyes = blackDyesValue,
                Tomes = tomesValue,
                ItemsPicked = TryExtractKeyValueInt(text, "itemsPicked"),
                SkinsPicked = TryExtractKeyValueInt(text, "skinsPicked"),
                LockpicksGained = TryExtractKeyValueInt(text, "lockpicksGained"),
                InventoryBaselineReady = TryExtractKeyValueInt(text, "inventoryBaselineReady") is int inventoryReady ? inventoryReady != 0 : null,
                DeldrimorTotal = TryExtractKeyValueInt(text, "deldrimorTotal"),
                AsuraTotal = TryExtractKeyValueInt(text, "asuraTotal"),
                NornTotal = TryExtractKeyValueInt(text, "nornTotal"),
                VanguardTotal = TryExtractKeyValueInt(text, "vanguardTotal"),
                CharacterGold = TryExtractKeyValueInt(text, "goldCharacter"),
                StorageGold = TryExtractKeyValueInt(text, "goldStorage"),
                Experience = TryExtractKeyValueInt(text, "experience"),
                MapId = TryExtractKeyValueInt(text, "mapId"),
                FreeSlots = TryExtractKeyValueInt(text, "freeSlots"),
                IdentificationKits = TryExtractKeyValueInt(text, "idKits"),
                SalvageKits = TryExtractKeyValueInt(text, "salvageKits"),
                RegularSalvageKits = TryExtractKeyValueInt(text, "regularSalvageKits"),
                HighGradeSalvageKits = TryExtractKeyValueInt(text, "highGradeSalvageKits"),
                ConsetsInventory = TryExtractKeyValueInt(text, "consetsInventory"),
                ConsetsStorage = TryExtractKeyValueInt(text, "consetsStorage"),
                GrailsInventory = TryExtractKeyValueInt(text, "grailsInventory"),
                EssencesInventory = TryExtractKeyValueInt(text, "essencesInventory"),
                ArmorsInventory = TryExtractKeyValueInt(text, "armorsInventory"),
                GrailsStorage = TryExtractKeyValueInt(text, "grailsStorage"),
                EssencesStorage = TryExtractKeyValueInt(text, "essencesStorage"),
                ArmorsStorage = TryExtractKeyValueInt(text, "armorsStorage"),
                DustInventory = TryExtractKeyValueInt(text, "dustInventory"),
                IronInventory = TryExtractKeyValueInt(text, "ironInventory"),
                BonesInventory = TryExtractKeyValueInt(text, "bonesInventory"),
                FeathersInventory = TryExtractKeyValueInt(text, "feathersInventory"),
                DustStorage = TryExtractKeyValueInt(text, "dustStorage"),
                IronStorage = TryExtractKeyValueInt(text, "ironStorage"),
                BonesStorage = TryExtractKeyValueInt(text, "bonesStorage"),
                FeathersStorage = TryExtractKeyValueInt(text, "feathersStorage"),
                SkillbarReady = TryExtractKeyValueInt(text, "skillbarReady") is int ready ? ready != 0 : null,
                SkillbarNonZero = TryExtractKeyValueInt(text, "skillbarNonZero"),
                SkillbarSkillIds = Enumerable.Range(1, 8)
                    .Select(index => TryExtractKeyValueInt(text, $"skill{index}") ?? 0)
                    .ToArray()
            }
            : null;
    }

    public static int? TryParseOpenedChestIncrement(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return null;
        }

        if (text.Contains("OpenChestAt result", StringComparison.OrdinalIgnoreCase))
        {
            return 1;
        }

        if (text.Contains("Boss chest loot completed", StringComparison.OrdinalIgnoreCase))
        {
            return TryExtractIntAfter(text, "successes=");
        }

        return null;
    }

    public static bool IsWipeLine(string text)
    {
        if (string.IsNullOrWhiteSpace(text) ||
            text.Contains("MonitoringStats", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("wipes=", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        return text.Contains("wipe", StringComparison.OrdinalIgnoreCase) &&
               (text.Contains("detected", StringComparison.OrdinalIgnoreCase) ||
                text.Contains("resign", StringComparison.OrdinalIgnoreCase) ||
                text.Contains("return", StringComparison.OrdinalIgnoreCase));
    }

    private static int? TryExtractIntAfter(string text, string marker)
    {
        var index = text.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return null;
        }

        index += marker.Length;
        var end = index;
        while (end < text.Length && char.IsDigit(text[end]))
        {
            end++;
        }

        return end > index && int.TryParse(text[index..end], NumberStyles.Integer, CultureInfo.InvariantCulture, out var value)
            ? value
            : null;
    }

    private static int? TryExtractKeyValueInt(string text, string key)
    {
        var index = text.IndexOf(key + "=", StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return null;
        }

        index += key.Length + 1;
        var end = index;
        while (end < text.Length && char.IsDigit(text[end]))
        {
            end++;
        }

        return end > index && int.TryParse(text[index..end], NumberStyles.Integer, CultureInfo.InvariantCulture, out var value)
            ? value
            : null;
    }

    private static string? TryExtractKeyValueText(string text, string key)
    {
        var index = text.IndexOf(key + "=", StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return null;
        }

        index += key.Length + 1;
        var end = index;
        while (end < text.Length && !char.IsWhiteSpace(text[end]))
        {
            end++;
        }

        return end > index ? text[index..end].Trim() : null;
    }
}
