using System.Text.RegularExpressions;
using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed partial class LauncherService : ILauncherService
{
    private const int DryRunPid = 42_000;
    private readonly IProcessRunner _processRunner;

    public LauncherService(IProcessRunner processRunner)
    {
        _processRunner = processRunner;
    }

    public ProcessCommand BuildCommand(LaunchPlan plan) =>
        new()
        {
            FileName = plan.AutoItExecutablePath,
            Arguments = new[] { plan.LauncherScriptPath },
            WorkingDirectory = plan.LauncherWorkingDirectory
        };

    public async Task<LauncherResult> LaunchAsync(LaunchPlan plan, CancellationToken cancellationToken = default)
    {
        var command = BuildCommand(plan);
        var processResult = plan.DryRun
            ? ProcessCommandResult.DryRunSuccess(command, DryRunPid)
            : await _processRunner.RunAsync(command, cancellationToken).ConfigureAwait(false);

        var pid = plan.DryRun
            ? DryRunPid
            : TryParseLauncherPid(GetLauncherOutput(plan, processResult));

        if (pid is null or <= 0)
        {
            return LauncherResult.Failed("Launcher did not emit a valid GWLAUNCHER_PID.", processResult);
        }

        return new LauncherResult
        {
            Succeeded = processResult.Succeeded,
            DryRun = processResult.DryRun,
            GuildWarsProcessId = pid,
            Process = processResult,
            Message = processResult.Succeeded
                ? "Launcher returned an exact Guild Wars PID."
                : "Launcher process failed."
        };
    }

    private static int? TryParseLauncherPid(string output)
    {
        var match = LauncherPidRegex().Match(output ?? string.Empty);
        return match.Success && int.TryParse(match.Groups["pid"].Value, out var pid) ? pid : null;
    }

    private static string GetLauncherOutput(LaunchPlan plan, ProcessCommandResult processResult)
    {
        var output = string.Join(
            Environment.NewLine,
            processResult.StandardOutput,
            processResult.StandardError,
            TryReadLauncherLog(plan.LauncherScriptPath));

        return output;
    }

    private static string TryReadLauncherLog(string launcherScriptPath)
    {
        if (string.IsNullOrWhiteSpace(launcherScriptPath))
        {
            return string.Empty;
        }

        var directory = Path.GetDirectoryName(launcherScriptPath);
        if (string.IsNullOrWhiteSpace(directory))
        {
            return string.Empty;
        }

        var candidates = new[] { Path.ChangeExtension(launcherScriptPath, ".log") };

        foreach (var candidate in candidates.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            try
            {
                if (File.Exists(candidate))
                {
                    return File.ReadAllText(candidate);
                }
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }

        return string.Empty;
    }

    [GeneratedRegex(@"GWLAUNCHER_PID\s*=\s*(?<pid>\d+)", RegexOptions.IgnoreCase)]
    private static partial Regex LauncherPidRegex();
}
