namespace Gwa3.UI.Core.Validation;

public sealed class ProfileValidationResult
{
    public ProfileValidationResult(IEnumerable<ProfileValidationIssue> issues)
    {
        Issues = issues.ToArray();
    }

    public IReadOnlyList<ProfileValidationIssue> Issues { get; }

    public bool IsValid => Issues.All(issue => issue.Severity != ProfileValidationSeverity.Error);

    public IReadOnlyList<ProfileValidationIssue> Errors =>
        Issues.Where(issue => issue.Severity == ProfileValidationSeverity.Error).ToArray();

    public IReadOnlyList<ProfileValidationIssue> Warnings =>
        Issues.Where(issue => issue.Severity == ProfileValidationSeverity.Warning).ToArray();

    public static ProfileValidationResult Success { get; } = new(Array.Empty<ProfileValidationIssue>());
}
