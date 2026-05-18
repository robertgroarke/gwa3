namespace Gwa3.UI.Supervisor.Models;

public static class CommandLineBuilder
{
    public static string Quote(string? value)
    {
        if (string.IsNullOrEmpty(value))
        {
            return "\"\"";
        }

        if (value.All(c => !char.IsWhiteSpace(c) && c != '"'))
        {
            return value;
        }

        return "\"" + value.Replace("\\", "\\\\", StringComparison.Ordinal).Replace("\"", "\\\"", StringComparison.Ordinal) + "\"";
    }
}
