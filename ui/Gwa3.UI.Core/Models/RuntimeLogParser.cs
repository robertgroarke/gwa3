using System.Text.RegularExpressions;

namespace Gwa3.UI.Core.Models;

public sealed record RuntimeLogLineSnapshot
{
    public string? Map { get; init; }
    public string? Health { get; init; }
    public string? Position { get; init; }
    public string? Target { get; init; }
    public string? Pathing { get; init; }
    public string? Casting { get; init; }
    public string? Skillbar { get; init; }
    public string? ActionQueue { get; init; }
    public string? Overwatch { get; init; }
}

public static partial class RuntimeLogParser
{
    public static RuntimeLogLineSnapshot? TryParse(string source, string text)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return null;
        }

        var builder = new RuntimeLogLineSnapshotBuilder();

        if (TrySummarizeActionQueue(text) is { Length: > 0 } actionQueue)
        {
            builder.ActionQueue = actionQueue;
        }

        if (TrySummarizeRouteTelemetry(text) is { } route)
        {
            builder.Map = route.Map;
            builder.Health = route.Health;
            builder.Position = route.Position;
            builder.Target = route.Target;
            builder.Pathing = route.Pathing;
        }

        if (TrySummarizeCasting(text) is { Length: > 0 } casting)
        {
            builder.Casting = casting;
        }

        if (IsSkillbarLoadedLine(text))
        {
            builder.Skillbar = "loaded";
        }

        if (TrySummarizeOverwatch(text) is { Length: > 0 } overwatch)
        {
            builder.Overwatch = overwatch;
            builder.Map ??= ExtractTokenValue(text, "map=") is { Length: > 0 } map ? $"map {map}" : null;
        }

        return builder.Build();
    }

    public static string? TrySummarizeActionQueue(string text)
    {
        if (text.Contains("EnqueueBotshubCommand", StringComparison.OrdinalIgnoreCase))
        {
            return $"queued idx {ExtractTokenValue(text, "idx=") ?? "?"}, exec {ExtractTokenValue(text, "exec=") ?? "?"}, defer {ExtractTokenValue(text, "defer=") ?? "?"}, hb {ExtractTokenValue(text, "hb=") ?? "?"}";
        }

        if (text.Contains("Botshub queue state", StringComparison.OrdinalIgnoreCase))
        {
            return $"pending {ExtractTokenValue(text, "pending=") ?? "?"}, exec {ExtractTokenValue(text, "exec=") ?? "?"}, defer {ExtractTokenValue(text, "defer=") ?? "?"}, suspended {ExtractTokenValue(text, "suspended=") ?? "?"}";
        }

        if (text.Contains("ActionQueueManager", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("ResetAllQueues", StringComparison.OrdinalIgnoreCase))
        {
            return Summarize(text);
        }

        if (text.Contains("queue full", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("lane saturated", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("rejected", StringComparison.OrdinalIgnoreCase))
        {
            return Summarize(text);
        }

        return null;
    }

    private static RuntimeLogLineSnapshot? TrySummarizeRouteTelemetry(string text)
    {
        if (!text.Contains("Froggy:", StringComparison.OrdinalIgnoreCase) ||
            !text.Contains("wp=", StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        var map = ExtractTokenValue(text, "map=");
        var loaded = ExtractTokenValue(text, "loaded=");
        var alive = ExtractTokenValue(text, "alive=");
        var hp = ExtractTokenValue(text, "hp=");
        var pos = ExtractTokenValue(text, "pos=");
        var distance = ExtractTokenValue(text, "distToWp=");
        var nearestEnemy = ExtractTokenValue(text, "nearestEnemy=");
        var nearbyEnemies = ExtractTokenValue(text, "nearbyEnemies=");
        var waypoint = ExtractTokenValue(text, "wp=");
        var stage = ExtractFroggyStage(text);

        var healthParts = new List<string>();
        if (!string.IsNullOrWhiteSpace(alive))
        {
            healthParts.Add(alive == "1" ? "alive" : "dead");
        }

        if (!string.IsNullOrWhiteSpace(hp))
        {
            healthParts.Add($"hp {hp}");
        }

        if (!string.IsNullOrWhiteSpace(loaded))
        {
            healthParts.Add(loaded == "1" ? "loaded" : "loading");
        }

        var positionParts = new List<string>();
        if (!string.IsNullOrWhiteSpace(pos))
        {
            positionParts.Add(pos);
        }

        if (!string.IsNullOrWhiteSpace(distance))
        {
            positionParts.Add($"dist {distance}");
        }

        var targetParts = new List<string>();
        if (!string.IsNullOrWhiteSpace(nearestEnemy))
        {
            targetParts.Add($"nearest {nearestEnemy}");
        }

        if (!string.IsNullOrWhiteSpace(nearbyEnemies))
        {
            targetParts.Add($"nearby {nearbyEnemies}");
        }

        return new RuntimeLogLineSnapshot
        {
            Map = string.IsNullOrWhiteSpace(map) ? null : $"map {map}",
            Health = healthParts.Count > 0 ? string.Join(", ", healthParts) : null,
            Position = positionParts.Count > 0 ? string.Join(", ", positionParts) : null,
            Target = targetParts.Count > 0 ? string.Join(", ", targetParts) : null,
            Pathing = string.IsNullOrWhiteSpace(waypoint)
                ? null
                : $"{stage} waypoint {waypoint}".Trim()
        };
    }

    private static string? TrySummarizeCasting(string text)
    {
        if (!text.Contains("UseSkill", StringComparison.OrdinalIgnoreCase) &&
            !text.Contains("casting", StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        var skill = ExtractTokenValue(text, "skillId=");
        var slot = ExtractTokenValue(text, "slot=");
        var target = ExtractTokenValue(text, "target=");

        if (!string.IsNullOrWhiteSpace(skill))
        {
            var parts = new List<string>();
            if (!string.IsNullOrWhiteSpace(slot))
            {
                parts.Add($"slot {slot}");
            }

            parts.Add($"skill {skill}");
            if (!string.IsNullOrWhiteSpace(target))
            {
                parts.Add($"target {target}");
            }

            return string.Join(", ", parts);
        }

        return Summarize(text);
    }

    private static string? TrySummarizeOverwatch(string text)
    {
        if (text.Contains("[WATCHDOG] runtime alive", StringComparison.OrdinalIgnoreCase))
        {
            var map = ExtractTokenValue(text, "map=");
            var responsive = ExtractTokenValue(text, "gameThreadResponsive=");
            return $"watchdog alive, map {map ?? "?"}, responsive {responsive ?? "?"}";
        }

        if (text.Contains("Overwatch", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("Player died", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("stuck", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("resign", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("resetting run", StringComparison.OrdinalIgnoreCase))
        {
            return Summarize(text);
        }

        return null;
    }

    private static bool IsSkillbarLoadedLine(string text) =>
        text.Contains("SkillBar Loaded", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("Mapping your skill bar - completed", StringComparison.OrdinalIgnoreCase) ||
        text.Contains("CacheSkillBar", StringComparison.OrdinalIgnoreCase);

    private static string ExtractFroggyStage(string text)
    {
        var marker = text.IndexOf("Froggy:", StringComparison.OrdinalIgnoreCase);
        if (marker < 0)
        {
            return "";
        }

        var start = marker + "Froggy:".Length;
        var waypoint = text.IndexOf("wp=", start, StringComparison.OrdinalIgnoreCase);
        if (waypoint < start)
        {
            return "";
        }

        return text[start..waypoint].Trim();
    }

    private static string? ExtractTokenValue(string text, string marker)
    {
        var match = TokenRegex(marker).Match(text);
        return match.Success ? match.Groups["value"].Value : null;
    }

    private static Regex TokenRegex(string marker) =>
        new($"{Regex.Escape(marker)}(?<value>\\([^)]*\\)|[^\\s,]+)", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

    private static string Summarize(string text)
    {
        const int maxLength = 180;
        var normalized = text.Replace('\r', ' ').Replace('\n', ' ').Trim();
        return normalized.Length <= maxLength ? normalized : normalized[..(maxLength - 3)] + "...";
    }

    private sealed class RuntimeLogLineSnapshotBuilder
    {
        public string? Map { get; set; }
        public string? Health { get; set; }
        public string? Position { get; set; }
        public string? Target { get; set; }
        public string? Pathing { get; set; }
        public string? Casting { get; set; }
        public string? Skillbar { get; set; }
        public string? ActionQueue { get; set; }
        public string? Overwatch { get; set; }

        public RuntimeLogLineSnapshot? Build()
        {
            if (Map is null &&
                Health is null &&
                Position is null &&
                Target is null &&
                Pathing is null &&
                Casting is null &&
                Skillbar is null &&
                ActionQueue is null &&
                Overwatch is null)
            {
                return null;
            }

            return new RuntimeLogLineSnapshot
            {
                Map = Map,
                Health = Health,
                Position = Position,
                Target = Target,
                Pathing = Pathing,
                Casting = Casting,
                Skillbar = Skillbar,
                ActionQueue = ActionQueue,
                Overwatch = Overwatch
            };
        }
    }
}
