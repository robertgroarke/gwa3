using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;
using System.Diagnostics;
using System.Management;

namespace Gwa3.UI.Supervisor.Services;

public sealed class ProcessHealthGate : IProcessHealthGate
{
    private readonly IProcessProbe _processProbe;

    public ProcessHealthGate(IProcessProbe processProbe)
    {
        _processProbe = processProbe;
    }

    public async Task<HealthGateResult> WaitForHealthyAsync(
        LaunchPlan plan,
        int guildWarsProcessId,
        CancellationToken cancellationToken = default)
    {
        if (guildWarsProcessId <= 0)
        {
            return HealthGateResult.Failed(
                guildWarsProcessId,
                plan.HealthGate.MinimumWorkingSetBytes,
                "Health gate requires the exact launcher-returned PID.");
        }

        if (plan.DryRun && plan.HealthGate.TreatDryRunAsHealthy)
        {
            return new HealthGateResult
            {
                Succeeded = true,
                DryRun = true,
                ProcessId = guildWarsProcessId,
                RequiredWorkingSetBytes = plan.HealthGate.MinimumWorkingSetBytes,
                Message = "Dry-run health gate treated the launcher PID as healthy.",
                LastSample = new ProcessSample
                {
                    ProcessId = guildWarsProcessId,
                    Exists = true,
                    WorkingSetBytes = plan.HealthGate.MinimumWorkingSetBytes
                }
            };
        }

        var deadline = DateTimeOffset.UtcNow + plan.HealthGate.Timeout;
        ProcessSample? last = null;

        while (DateTimeOffset.UtcNow <= deadline)
        {
            cancellationToken.ThrowIfCancellationRequested();
            last = _processProbe.Sample(guildWarsProcessId);

            if (!last.Exists || last.HasExited)
            {
                var resolved = TryResolveHealthyCharacterProcess(plan, guildWarsProcessId);
                if (resolved is not null)
                {
                    return resolved;
                }

                return HealthGateResult.Failed(
                    guildWarsProcessId,
                    plan.HealthGate.MinimumWorkingSetBytes,
                    "Launcher-returned process exited before it became healthy.",
                    last);
            }

            if (last.WorkingSetBytes >= plan.HealthGate.MinimumWorkingSetBytes)
            {
                if (plan.HealthGate.PostHealthySettle > TimeSpan.Zero)
                {
                    await Task.Delay(plan.HealthGate.PostHealthySettle, cancellationToken).ConfigureAwait(false);
                    last = _processProbe.Sample(guildWarsProcessId);
                    if (!last.Exists || last.HasExited)
                    {
                        return HealthGateResult.Failed(
                            guildWarsProcessId,
                            plan.HealthGate.MinimumWorkingSetBytes,
                            "Launcher-returned process exited during the post-health settle window.",
                            last);
                    }
                }

                return new HealthGateResult
                {
                    Succeeded = true,
                    ProcessId = guildWarsProcessId,
                    RequiredWorkingSetBytes = plan.HealthGate.MinimumWorkingSetBytes,
                    LastSample = last,
                    Message = plan.HealthGate.PostHealthySettle > TimeSpan.Zero
                        ? $"Launcher-returned process passed the memory health gate and settled for {plan.HealthGate.PostHealthySettle.TotalSeconds:N0}s."
                        : "Launcher-returned process passed the memory health gate."
                };
            }

            await Task.Delay(plan.HealthGate.PollInterval, cancellationToken).ConfigureAwait(false);
        }

        return HealthGateResult.Failed(
            guildWarsProcessId,
            plan.HealthGate.MinimumWorkingSetBytes,
            "Launcher-returned process did not reach the healthy memory threshold before timeout.",
            last);
    }

    private static HealthGateResult? TryResolveHealthyCharacterProcess(LaunchPlan plan, int launcherProcessId)
    {
        if (string.IsNullOrWhiteSpace(plan.CharacterName))
        {
            return null;
        }

        try
        {
            var candidates = new List<ProcessSample>();
            using var searcher = new ManagementObjectSearcher(
                "SELECT ProcessId, CommandLine FROM Win32_Process WHERE Name = 'Gw.exe'");

            foreach (ManagementObject row in searcher.Get())
            {
                var commandLine = row["CommandLine"]?.ToString() ?? string.Empty;
                if (!commandLine.Contains(plan.CharacterName, StringComparison.OrdinalIgnoreCase))
                {
                    continue;
                }

                var processId = Convert.ToInt32(row["ProcessId"], System.Globalization.CultureInfo.InvariantCulture);
                using var process = Process.GetProcessById(processId);
                var sample = new ProcessSample
                {
                    ProcessId = processId,
                    Exists = true,
                    HasExited = process.HasExited,
                    WorkingSetBytes = process.WorkingSet64,
                    ProcessName = process.ProcessName
                };

                if (!sample.HasExited &&
                    process.MainWindowHandle != IntPtr.Zero &&
                    sample.WorkingSetBytes >= plan.HealthGate.MinimumWorkingSetBytes)
                {
                    candidates.Add(sample);
                }
            }

            if (candidates.Count != 1)
            {
                return null;
            }

            var resolved = candidates[0];
            return new HealthGateResult
            {
                Succeeded = true,
                ProcessId = resolved.ProcessId,
                RequiredWorkingSetBytes = plan.HealthGate.MinimumWorkingSetBytes,
                LastSample = resolved,
                Message = $"Launcher PID {launcherProcessId} handed off to healthy {plan.CharacterName} client PID {resolved.ProcessId}."
            };
        }
        catch (ManagementException)
        {
            return null;
        }
        catch (InvalidOperationException)
        {
            return null;
        }
        catch (ArgumentException)
        {
            return null;
        }
    }
}
