using System.Globalization;
using System.Text.Json;

namespace Gwa3.UI.Core.Models;

public sealed record BotshubWebIpcStatus
{
    public string? Character { get; init; }
    public string? Script { get; init; }
    public int? ProcessId { get; init; }
    public long? TimestampUnix { get; init; }
    public int? UptimeSeconds { get; init; }
    public bool? BotRunning { get; init; }
    public int? MapId { get; init; }
    public int? CharacterGold { get; init; }
    public int? StorageGold { get; init; }
    public string? State { get; init; }
    public string? SettingsSummary { get; init; }
    public int? RunCount { get; init; }
    public int? SuccessCount { get; init; }
    public int? FailCount { get; init; }
    public string? SuccessRatio { get; init; }
    public string? BestRunTime { get; init; }
    public string? AverageRunTime { get; init; }
    public string? CurrentRunTime { get; init; }
    public string? TotalTime { get; init; }
    public string? TimePerRun { get; init; }
    public int? Experience { get; init; }
    public int? Chests { get; init; }
    public int? GoldItems { get; init; }
    public string? TitleSummary { get; init; }
    public string? LootSummary { get; init; }
    public string? MaterialSummary { get; init; }
    public string? InventorySummary { get; init; }
    public string? MaintenanceSummary { get; init; }
    public string? LastLogLine { get; init; }
}

public static class BotshubWebIpcStatusParser
{
    public static BotshubWebIpcStatus? TryParse(string json)
    {
        if (string.IsNullOrWhiteSpace(json))
        {
            return null;
        }

        using var document = JsonDocument.Parse(json);
        var root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object)
        {
            return null;
        }

        var gold = TryGetObject(root, "gold");
        var settings = TryGetObject(root, "settings");
        var stats = TryGetObject(root, "stats");

        return new BotshubWebIpcStatus
        {
            Character = GetString(root, "character"),
            Script = GetString(root, "script"),
            ProcessId = GetInt(root, "pid"),
            TimestampUnix = GetLong(root, "timestamp"),
            UptimeSeconds = GetInt(root, "uptime_seconds"),
            BotRunning = GetBool(root, "bot_running"),
            MapId = GetInt(root, "map_id"),
            CharacterGold = gold is JsonElement goldElement ? GetInt(goldElement, "character") : null,
            StorageGold = gold is JsonElement goldElement2 ? GetInt(goldElement2, "storage") : null,
            State = GetString(root, "state"),
            SettingsSummary = settings is JsonElement settingsElement
                ? BuildSettingsSummary(settingsElement)
                : null,
            RunCount = stats is JsonElement statsElement ? GetIntAny(statsElement, "run_count", "runs") : null,
            SuccessCount = stats is JsonElement statsElement2 ? GetIntAny(statsElement2, "success_count", "successes") : null,
            FailCount = stats is JsonElement statsElement3 ? GetIntAny(statsElement3, "fail_count", "failure_count", "failures") : null,
            SuccessRatio = stats is JsonElement statsElement4 ? GetStringAny(statsElement4, "success_ratio", "successRate") : null,
            BestRunTime = stats is JsonElement statsElement5 ? GetStringAny(statsElement5, "best_run_time", "best") : null,
            AverageRunTime = stats is JsonElement statsElement6 ? GetStringAny(statsElement6, "avg_run_time", "average_run_time", "avg") : null,
            CurrentRunTime = stats is JsonElement statsElement7 ? GetStringAny(statsElement7, "current_run_time", "current") : null,
            TotalTime = stats is JsonElement statsElement8 ? GetStringAny(statsElement8, "total_time", "time") : null,
            TimePerRun = stats is JsonElement statsElement9 ? GetStringAny(statsElement9, "time_per_run", "timePerRun") : null,
            Experience = stats is JsonElement statsElement10 ? GetIntAny(statsElement10, "experience", "xp") : null,
            Chests = stats is JsonElement statsElement11 ? GetIntAny(statsElement11, "chests", "chest_count", "chests_opened") : null,
            GoldItems = stats is JsonElement statsElement12 ? GetIntAny(statsElement12, "gold_items", "goldItems") : null,
            TitleSummary = BuildCounterSummary(root, "titles", "title_points"),
            LootSummary = BuildCounterSummary(root, "loot", "items", "drops", "items_collected"),
            MaterialSummary = BuildCounterSummary(root, "materials", "material_counts"),
            InventorySummary = BuildCounterSummary(root, "inventory", "bags"),
            MaintenanceSummary = BuildMaintenanceSummary(root),
            LastLogLine = GetLastLogLine(root)
        };
    }

    private static string? BuildSettingsSummary(JsonElement settings)
    {
        var parts = new List<string>();
        AddEnabledSetting(parts, settings, "add_heroes", "add heroes");
        AddEnabledSetting(parts, settings, "consets", "use consets");
        AddEnabledSetting(parts, settings, "buy_consets", "buy consets");
        AddEnabledSetting(parts, settings, "pick_up_golds", "pick up golds");
        AddEnabledSetting(parts, settings, "auto_salvage", "auto salvage");

        var heroConfig = GetString(settings, "hero_config");
        if (!string.IsNullOrWhiteSpace(heroConfig))
        {
            parts.Add($"heroes: {heroConfig}");
        }

        return parts.Count > 0 ? string.Join(", ", parts) : null;
    }

    private static void AddEnabledSetting(List<string> parts, JsonElement root, string key, string label)
    {
        if (GetBool(root, key) == true)
        {
            parts.Add(label);
        }
    }

    private static JsonElement? TryGetObject(JsonElement root, string propertyName)
    {
        return root.TryGetProperty(propertyName, out var value) && value.ValueKind == JsonValueKind.Object
            ? value
            : null;
    }

    private static string? BuildMaintenanceSummary(JsonElement root)
    {
        var parts = new List<string>();
        AddObjectSummary(parts, root, "maintenance", "maintenance");
        AddObjectSummary(parts, root, "salvage", "salvage");
        AddObjectSummary(parts, root, "sell", "sell");
        AddObjectSummary(parts, root, "crafting", "crafting");
        AddObjectSummary(parts, root, "consets", "consets");
        return parts.Count > 0 ? string.Join("; ", parts) : null;
    }

    private static void AddObjectSummary(List<string> parts, JsonElement root, string propertyName, string label)
    {
        if (!root.TryGetProperty(propertyName, out var value))
        {
            return;
        }

        var summary = SummarizeElement(value);
        if (!string.IsNullOrWhiteSpace(summary))
        {
            parts.Add($"{label}: {summary}");
        }
    }

    private static string? BuildCounterSummary(JsonElement root, params string[] propertyNames)
    {
        foreach (var propertyName in propertyNames)
        {
            if (!root.TryGetProperty(propertyName, out var value))
            {
                continue;
            }

            var summary = SummarizeElement(value);
            if (!string.IsNullOrWhiteSpace(summary))
            {
                return summary;
            }
        }

        return null;
    }

    private static string? SummarizeElement(JsonElement value)
    {
        return value.ValueKind switch
        {
            JsonValueKind.Object => SummarizeObject(value),
            JsonValueKind.Array => SummarizeArray(value),
            JsonValueKind.String => string.IsNullOrWhiteSpace(value.GetString()) ? null : value.GetString(),
            JsonValueKind.Number => value.ToString(),
            JsonValueKind.True => "enabled",
            JsonValueKind.False => "disabled",
            _ => null
        };
    }

    private static string? SummarizeObject(JsonElement root)
    {
        var parts = new List<string>();
        foreach (var property in root.EnumerateObject())
        {
            var value = property.Value;
            if (value.ValueKind == JsonValueKind.Object)
            {
                var nested = SummarizeObject(value);
                if (!string.IsNullOrWhiteSpace(nested))
                {
                    parts.Add($"{HumanizePropertyName(property.Name)} ({nested})");
                }

                continue;
            }

            if (value.ValueKind == JsonValueKind.Array)
            {
                var nested = SummarizeArray(value);
                if (!string.IsNullOrWhiteSpace(nested))
                {
                    parts.Add($"{HumanizePropertyName(property.Name)}: {nested}");
                }

                continue;
            }

            if (!TryFormatScalar(property.Name, value, out var formatted))
            {
                continue;
            }

            parts.Add(formatted);
            if (parts.Count >= 8)
            {
                break;
            }
        }

        return parts.Count > 0 ? string.Join(", ", parts) : null;
    }

    private static string? SummarizeArray(JsonElement root)
    {
        var parts = new List<string>();
        foreach (var entry in root.EnumerateArray())
        {
            var summary = entry.ValueKind == JsonValueKind.Object
                ? SummarizeNamedCounter(entry)
                : SummarizeElement(entry);
            if (!string.IsNullOrWhiteSpace(summary))
            {
                parts.Add(summary);
            }

            if (parts.Count >= 8)
            {
                break;
            }
        }

        return parts.Count > 0 ? string.Join(", ", parts) : null;
    }

    private static string? SummarizeNamedCounter(JsonElement entry)
    {
        var name = GetStringAny(entry, "name", "label", "item", "material", "title", "key");
        var count = GetIntAny(entry, "count", "quantity", "value", "total", "current");
        if (!string.IsNullOrWhiteSpace(name) && count is int countValue)
        {
            return countValue == 0 ? null : $"{HumanizePropertyName(name)} {countValue.ToString("N0", CultureInfo.InvariantCulture)}";
        }

        return SummarizeObject(entry);
    }

    private static bool TryFormatScalar(string propertyName, JsonElement value, out string formatted)
    {
        formatted = "";
        var label = HumanizePropertyName(propertyName);

        switch (value.ValueKind)
        {
            case JsonValueKind.Number:
                if (value.TryGetInt64(out var number))
                {
                    if (number == 0)
                    {
                        return false;
                    }

                    formatted = $"{label} {number.ToString("N0", CultureInfo.InvariantCulture)}";
                    return true;
                }

                formatted = $"{label} {value}";
                return true;

            case JsonValueKind.String:
                var text = value.GetString();
                if (string.IsNullOrWhiteSpace(text) || string.Equals(text, "0", StringComparison.OrdinalIgnoreCase))
                {
                    return false;
                }

                formatted = $"{label} {text}";
                return true;

            case JsonValueKind.True:
                formatted = label;
                return true;

            case JsonValueKind.False:
                return false;

            default:
                return false;
        }
    }

    private static string HumanizePropertyName(string value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return value;
        }

        var chars = new List<char>(value.Length + 8);
        var previous = '\0';
        foreach (var ch in value)
        {
            if (ch is '_' or '-')
            {
                chars.Add(' ');
            }
            else if (char.IsUpper(ch) && previous != '\0' && !char.IsWhiteSpace(previous))
            {
                chars.Add(' ');
                chars.Add(char.ToLowerInvariant(ch));
            }
            else
            {
                chars.Add(char.ToLowerInvariant(ch));
            }

            previous = ch;
        }

        return new string(chars.ToArray()).Trim();
    }

    private static string? GetLastLogLine(JsonElement root)
    {
        if (!root.TryGetProperty("log", out var value) || value.ValueKind != JsonValueKind.Array)
        {
            return null;
        }

        string? last = null;
        foreach (var entry in value.EnumerateArray())
        {
            if (entry.ValueKind == JsonValueKind.String)
            {
                last = entry.GetString();
            }
        }

        return last;
    }

    private static string? GetString(JsonElement root, string propertyName)
    {
        return root.TryGetProperty(propertyName, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString()
            : null;
    }

    private static string? GetStringAny(JsonElement root, params string[] propertyNames)
    {
        foreach (var propertyName in propertyNames)
        {
            if (!root.TryGetProperty(propertyName, out var value))
            {
                continue;
            }

            if (value.ValueKind == JsonValueKind.String)
            {
                return value.GetString();
            }

            if (value.ValueKind == JsonValueKind.Number ||
                value.ValueKind == JsonValueKind.True ||
                value.ValueKind == JsonValueKind.False)
            {
                return value.ToString();
            }
        }

        return null;
    }

    private static int? GetInt(JsonElement root, string propertyName)
    {
        if (!root.TryGetProperty(propertyName, out var value))
        {
            return null;
        }

        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out var intValue))
        {
            return intValue;
        }

        if (value.ValueKind == JsonValueKind.String &&
            int.TryParse(value.GetString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out intValue))
        {
            return intValue;
        }

        return null;
    }

    private static int? GetIntAny(JsonElement root, params string[] propertyNames)
    {
        foreach (var propertyName in propertyNames)
        {
            var value = GetInt(root, propertyName);
            if (value is not null)
            {
                return value;
            }
        }

        return null;
    }

    private static long? GetLong(JsonElement root, string propertyName)
    {
        if (!root.TryGetProperty(propertyName, out var value))
        {
            return null;
        }

        if (value.ValueKind == JsonValueKind.Number && value.TryGetInt64(out var longValue))
        {
            return longValue;
        }

        if (value.ValueKind == JsonValueKind.String &&
            long.TryParse(value.GetString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out longValue))
        {
            return longValue;
        }

        return null;
    }

    private static bool? GetBool(JsonElement root, string propertyName)
    {
        if (!root.TryGetProperty(propertyName, out var value))
        {
            return null;
        }

        return value.ValueKind switch
        {
            JsonValueKind.True => true,
            JsonValueKind.False => false,
            JsonValueKind.String when bool.TryParse(value.GetString(), out var boolValue) => boolValue,
            _ => null
        };
    }
}
