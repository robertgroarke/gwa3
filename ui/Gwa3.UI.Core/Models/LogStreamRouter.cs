namespace Gwa3.UI.Core.Models;

public static class LogStreamRouter
{
    public static bool ShouldShowInRuntimeTail(string source) =>
        source.Equals("Launcher", StringComparison.OrdinalIgnoreCase) ||
        source.Equals("Bot", StringComparison.OrdinalIgnoreCase) ||
        source.Equals("DLL", StringComparison.OrdinalIgnoreCase) ||
        source.Equals("Bridge", StringComparison.OrdinalIgnoreCase);

    public static bool ShouldShowInLaunchSummary(string source, string message) =>
        IsLaunchSummarySource(source) || IsHighSignalLauncherLine(source, message);

    private static bool IsLaunchSummarySource(string source) =>
        source.Equals("UI", StringComparison.OrdinalIgnoreCase) ||
        source.Equals("Profile", StringComparison.OrdinalIgnoreCase) ||
        source.Equals("Launch", StringComparison.OrdinalIgnoreCase) ||
        source.Equals("Supervisor", StringComparison.OrdinalIgnoreCase) ||
        source.Equals("Session", StringComparison.OrdinalIgnoreCase) ||
        source.Equals("LLM", StringComparison.OrdinalIgnoreCase);

    private static bool IsHighSignalLauncherLine(string source, string message) =>
        source.Equals("Launcher", StringComparison.OrdinalIgnoreCase) &&
        (message.Contains("GWLAUNCHER_PID", StringComparison.OrdinalIgnoreCase) ||
         message.Contains("LOADED_CHARACTER", StringComparison.OrdinalIgnoreCase) ||
         message.Contains("LAUNCH_VERIFIED", StringComparison.OrdinalIgnoreCase) ||
         message.Contains("LAUNCH_FAILED", StringComparison.OrdinalIgnoreCase) ||
         message.Contains("ERROR", StringComparison.OrdinalIgnoreCase) ||
         message.Contains("WARN", StringComparison.OrdinalIgnoreCase));
}
