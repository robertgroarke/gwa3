namespace Gwa3.UI.Supervisor.Models;

public sealed record ProcessCommand
{
    public string FileName { get; init; } = string.Empty;
    public IReadOnlyList<string> Arguments { get; init; } = Array.Empty<string>();
    public string? WorkingDirectory { get; init; }
    public IReadOnlyDictionary<string, string> Environment { get; init; } =
        new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

    public string ToDisplayString() =>
        string.Join(" ", new[] { CommandLineBuilder.Quote(FileName) }
            .Concat(Arguments.Select(CommandLineBuilder.Quote)));
}
