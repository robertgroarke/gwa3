using System.Text.Json;

namespace Gwa3.UI.Core.Models;

public sealed class UiLiveStatusSnapshotStore
{
    public const string RelativeSnapshotPath = @"ui\runs\latest\ui_live_status.json";

    private static readonly JsonSerializerOptions Options = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };

    private readonly string snapshotPath;

    public UiLiveStatusSnapshotStore(string repositoryRoot)
    {
        snapshotPath = Path.Combine(repositoryRoot, RelativeSnapshotPath);
    }

    private UiLiveStatusSnapshotStore(string snapshotPath, SnapshotPathKind pathKind)
    {
        this.snapshotPath = pathKind == SnapshotPathKind.Direct
            ? snapshotPath
            : throw new ArgumentOutOfRangeException(nameof(pathKind), pathKind, "Unknown status snapshot path kind.");
    }

    public string SnapshotPath => snapshotPath;

    public static UiLiveStatusSnapshotStore FromSnapshotPath(string snapshotPath) =>
        new(snapshotPath, SnapshotPathKind.Direct);

    public UiLiveStatusSnapshot? TryReadLatest()
    {
        if (!File.Exists(snapshotPath))
        {
            return null;
        }

        var json = File.ReadAllText(snapshotPath);
        return JsonSerializer.Deserialize<UiLiveStatusSnapshot>(json, Options);
    }

    public void WriteLatest(UiLiveStatusSnapshot snapshot)
    {
        var directory = Path.GetDirectoryName(snapshotPath);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var json = JsonSerializer.Serialize(snapshot, Options);
        File.WriteAllText(snapshotPath, json);
    }

    private enum SnapshotPathKind
    {
        Direct
    }
}
