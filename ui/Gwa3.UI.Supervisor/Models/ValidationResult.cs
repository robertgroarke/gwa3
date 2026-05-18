namespace Gwa3.UI.Supervisor.Models;

public sealed record ValidationResult
{
    public bool IsValid => Errors.Count == 0;
    public IReadOnlyList<string> Errors { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> Warnings { get; init; } = Array.Empty<string>();

    public static ValidationResult Success(IReadOnlyList<string>? warnings = null) =>
        new() { Warnings = warnings ?? Array.Empty<string>() };

    public static ValidationResult Failure(params string[] errors) =>
        new() { Errors = errors };
}
