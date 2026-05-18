namespace Gwa3.UI.Core.Validation;

public sealed record ProfileValidationIssue(
    ProfileValidationSeverity Severity,
    string Path,
    string Message)
{
    public static ProfileValidationIssue Error(string path, string message)
    {
        return new ProfileValidationIssue(ProfileValidationSeverity.Error, path, message);
    }

    public static ProfileValidationIssue Warning(string path, string message)
    {
        return new ProfileValidationIssue(ProfileValidationSeverity.Warning, path, message);
    }
}

public enum ProfileValidationSeverity
{
    Warning,
    Error
}
