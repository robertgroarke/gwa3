using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed class BridgeService : IBridgeService
{
    private readonly IProcessRunner _processRunner;

    public BridgeService(IProcessRunner processRunner)
    {
        _processRunner = processRunner;
    }

    public ProcessCommand BuildCommand(LaunchPlan plan)
    {
        if (!plan.Bridge.IsEnabled(plan.Mode))
        {
            return new ProcessCommand();
        }

        var python = string.IsNullOrWhiteSpace(plan.Bridge.PythonExecutablePath)
            ? "python"
            : plan.Bridge.PythonExecutablePath;

        var arguments = new List<string>
        {
            "-m",
            "bridge",
            "--pipe",
            plan.Bridge.PipeName
        };

        if (!string.IsNullOrWhiteSpace(plan.Bridge.Endpoint))
        {
            arguments.Add("--llm-url");
            arguments.Add(plan.Bridge.Endpoint);
        }

        if (!string.IsNullOrWhiteSpace(plan.Bridge.Model))
        {
            arguments.Add("--model");
            arguments.Add(plan.Bridge.Model);
        }

        if (plan.Bridge.HourlyTokenCap is int hourlyTokenCap && hourlyTokenCap > 0)
        {
            arguments.Add("--llm-hourly-token-cap");
            arguments.Add(hourlyTokenCap.ToString(System.Globalization.CultureInfo.InvariantCulture));
        }

        if (plan.Bridge.AllowRemote)
        {
            arguments.Add("--allow-remote-llm");
        }

        if (plan.Mode == LaunchMode.Advisory)
        {
            arguments.Add("--advisory");
        }

        return new ProcessCommand
        {
            FileName = python,
            Arguments = arguments,
            WorkingDirectory = plan.Bridge.BridgeWorkingDirectory,
            Environment = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["GWA3_PIPE_NAME"] = plan.Bridge.PipeName
            }
        };
    }

    public async Task<BridgeResult> StartAsync(LaunchPlan plan, CancellationToken cancellationToken = default)
    {
        if (!plan.Bridge.IsEnabled(plan.Mode))
        {
            return BridgeResult.Disabled();
        }

        var command = BuildCommand(plan);
        var result = plan.DryRun
            ? ProcessCommandResult.DryRunSuccess(command)
            : await _processRunner.StartAsync(command, cancellationToken).ConfigureAwait(false);

        return new BridgeResult
        {
            Enabled = true,
            Succeeded = result.Succeeded,
            DryRun = result.DryRun,
            Process = result,
            Message = result.Succeeded ? "Bridge started." : "Bridge failed to start."
        };
    }
}
