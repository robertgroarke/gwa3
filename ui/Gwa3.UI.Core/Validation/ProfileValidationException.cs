namespace Gwa3.UI.Core.Validation;

public sealed class ProfileValidationException : Exception
{
    public ProfileValidationException(ProfileValidationResult result)
        : base(BuildMessage(result))
    {
        Result = result;
    }

    public ProfileValidationResult Result { get; }

    private static string BuildMessage(ProfileValidationResult result)
    {
        var firstError = result.Issues.FirstOrDefault(issue => issue.Severity == ProfileValidationSeverity.Error);
        return firstError is null
            ? "Profile validation failed."
            : $"Profile validation failed at '{firstError.Path}': {firstError.Message}";
    }
}
