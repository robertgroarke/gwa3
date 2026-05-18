using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed class InjectorService : IInjectorService
{
    private readonly IProcessRunner _processRunner;

    public InjectorService(IProcessRunner processRunner)
    {
        _processRunner = processRunner;
    }

    public ProcessCommand BuildCommand(LaunchPlan plan, int guildWarsProcessId)
    {
        if (guildWarsProcessId <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(guildWarsProcessId), "Injector commands require the exact launcher-returned PID.");
        }

        var dllArgument = string.IsNullOrWhiteSpace(plan.DllPath) ? plan.DllName : plan.DllPath;
        var arguments = new List<string>
        {
            "--pid",
            guildWarsProcessId.ToString(System.Globalization.CultureInfo.InvariantCulture),
            "--dll",
            dllArgument
        };

        if (plan.Mode == LaunchMode.Llm)
        {
            arguments.Add("--llm");
        }
        else if (plan.Mode == LaunchMode.Advisory)
        {
            arguments.Add("--llm-advisory");
        }

        if (!string.IsNullOrWhiteSpace(plan.BotModule))
        {
            arguments.Add("--bot-module");
            arguments.Add(plan.BotModule);
        }

        if (!string.IsNullOrWhiteSpace(plan.ResolvedProfilePath))
        {
            arguments.Add("--profile");
            arguments.Add(plan.ResolvedProfilePath);
        }

        return new ProcessCommand
        {
            FileName = plan.InjectorPath,
            Arguments = arguments,
            WorkingDirectory = string.IsNullOrWhiteSpace(plan.BuildDirectory) ? null : plan.BuildDirectory,
            Environment = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["GWA3_DLL_NAME"] = plan.DllName,
                ["GWA3_PIPE_NAME"] = plan.Bridge.PipeName
            }
        };
    }

    public async Task<InjectionResult> InjectAsync(
        LaunchPlan plan,
        int guildWarsProcessId,
        CancellationToken cancellationToken = default)
    {
        var command = BuildCommand(plan, guildWarsProcessId);
        var result = plan.DryRun
            ? ProcessCommandResult.DryRunSuccess(command)
            : await _processRunner.RunAsync(command, cancellationToken).ConfigureAwait(false);

        return new InjectionResult
        {
            Succeeded = result.Succeeded,
            DryRun = result.DryRun,
            Process = result,
            Message = result.Succeeded ? "DLL injection completed." : "DLL injection failed."
        };
    }
}
