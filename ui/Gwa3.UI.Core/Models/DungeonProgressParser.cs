using System.Globalization;

namespace Gwa3.UI.Core.Models;

public sealed record DungeonProgressSnapshot(
    string DungeonLevel,
    int CompletionPercent,
    string CompletionText,
    string ProgressDetail,
    bool IsPrecise);

public static class DungeonProgressParser
{
    private const int SparkflySwampMapId = 558;
    private const int BogrootLevel1MapId = 615;
    private const int BogrootLevel2MapId = 616;
    private const int PostRunOutpostMapId = 638;

    private const int SparkflyWaypointCount = 9;
    private const int BogrootLevel1WaypointCount = 28;
    private const int BogrootLevel2WaypointCount = 35;

    public static DungeonProgressSnapshot? TryParseFroggyProgress(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return null;
        }

        if (Contains(text, "Run #") && Contains(text, "complete"))
        {
            return Snapshot("Complete", 100, "Run complete", isPrecise: true);
        }

        if (Contains(text, "MonitoringStats reason=run-complete"))
        {
            return Snapshot("Complete", 100, "Run complete", isPrecise: true);
        }

        if (Contains(text, "Boss post-reward") ||
            Contains(text, "MonitoringStats reason=waiting-return") ||
            Contains(text, "awaiting return") ||
            Contains(text, "waiting for automatic return"))
        {
            return Snapshot("Bogroot Growths level 2", 100, "Waiting for return", isPrecise: true);
        }

        if (Contains(text, "Boss reward") && Contains(text, "QuestReward cleared"))
        {
            return Snapshot("Bogroot Growths level 2", 100, "Quest reward accepted", isPrecise: true);
        }

        if (Contains(text, "OpenChestAt result") ||
            Contains(text, "Boss chest open succeeded") ||
            Contains(text, "MonitoringStats reason=chest"))
        {
            return Snapshot("Bogroot Growths level 2", 99, "Chest looted", isPrecise: true);
        }

        if (Contains(text, "Froggy transition poll") && Contains(text, "map=616"))
        {
            return Snapshot("Bogroot Growths level 2", 100, "Waiting for return", isPrecise: true);
        }

        if (Contains(text, "State: TownSetup"))
        {
            return Snapshot("Town setup", 0, "Preparing next run", isPrecise: true);
        }

        if (Contains(text, "State transition: InTown -> Traveling"))
        {
            return Snapshot("Traveling to Bogroot", 0, "Leaving outpost", isPrecise: true);
        }

        if (Contains(text, "Sparkfly Swamp - preparing Bogroot entry"))
        {
            return Snapshot("Sparkfly Swamp approach", 1, "Preparing Bogroot entry", isPrecise: true);
        }

        var mapId = TryExtractIntAfter(text, "map=");
        var waypointIndex = TryExtractIntAfter(text, "wp=");
        var waypointLabel = TryExtractWaypointLabel(text);

        if (mapId == PostRunOutpostMapId)
        {
            return null;
        }

        if (mapId == SparkflySwampMapId || Contains(text, "Sparkfly"))
        {
            return CreateRouteSnapshot(
                "Sparkfly Swamp approach",
                "Sparkfly waypoint",
                waypointIndex,
                waypointLabel,
                SparkflyWaypointCount,
                startPercent: 0,
                endPercent: 15);
        }

        if (mapId == BogrootLevel1MapId)
        {
            return CreateRouteSnapshot(
                "Bogroot Growths level 1",
                "Level 1 waypoint",
                waypointIndex,
                waypointLabel,
                BogrootLevel1WaypointCount,
                startPercent: 15,
                endPercent: 70);
        }

        if (mapId == BogrootLevel2MapId)
        {
            return CreateRouteSnapshot(
                "Bogroot Growths level 2",
                "Level 2 waypoint",
                waypointIndex,
                waypointLabel,
                BogrootLevel2WaypointCount,
                startPercent: 70,
                endPercent: 98);
        }

        return null;
    }

    private static DungeonProgressSnapshot CreateRouteSnapshot(
        string dungeonLevel,
        string waypointPrefix,
        int? waypointIndex,
        string? waypointLabel,
        int waypointCount,
        int startPercent,
        int endPercent)
    {
        if (waypointIndex is not int index)
        {
            return Snapshot(dungeonLevel, startPercent, "Map heartbeat", isPrecise: false);
        }

        var boundedIndex = Math.Clamp(index, 0, Math.Max(0, waypointCount - 1));
        var ratio = waypointCount <= 1
            ? 1.0
            : boundedIndex / (double)(waypointCount - 1);
        var percent = (int)Math.Round(startPercent + ((endPercent - startPercent) * ratio), MidpointRounding.AwayFromZero);
        var detail = $"{waypointPrefix} {boundedIndex + 1}/{waypointCount}";
        if (!string.IsNullOrWhiteSpace(waypointLabel))
        {
            detail += $" ({waypointLabel})";
        }

        return Snapshot(dungeonLevel, percent, detail, isPrecise: true);
    }

    private static DungeonProgressSnapshot Snapshot(string dungeonLevel, int percent, string detail, bool isPrecise)
    {
        var boundedPercent = Math.Clamp(percent, 0, 100);
        return new DungeonProgressSnapshot(
            dungeonLevel,
            boundedPercent,
            boundedPercent.ToString(CultureInfo.InvariantCulture) + "%",
            detail,
            isPrecise);
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

    private static string? TryExtractWaypointLabel(string text)
    {
        var wpIndex = text.IndexOf("wp=", StringComparison.OrdinalIgnoreCase);
        if (wpIndex < 0)
        {
            return null;
        }

        var open = text.IndexOf('(', wpIndex);
        var close = open >= 0 ? text.IndexOf(')', open + 1) : -1;
        return open >= 0 && close > open + 1
            ? text[(open + 1)..close]
            : null;
    }

    private static bool Contains(string text, string value) =>
        text.Contains(value, StringComparison.OrdinalIgnoreCase);
}
