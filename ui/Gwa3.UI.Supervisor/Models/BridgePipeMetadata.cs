namespace Gwa3.UI.Supervisor.Models;

public sealed record BridgePipeMetadata
{
    public string PipeName { get; init; } = string.Empty;
    public string LaneTag { get; init; } = string.Empty;
    public string? ExpectedDllName { get; init; }
    public string? PythonExecutablePath { get; init; }
    public string? BridgeWorkingDirectory { get; init; }
    public string? Endpoint { get; init; }
    public string? Model { get; init; }
    public int? HourlyTokenCap { get; init; }
    public bool AllowRemote { get; init; }

    public bool IsEnabled(LaunchMode mode) => mode is LaunchMode.Llm or LaunchMode.Advisory;
}
