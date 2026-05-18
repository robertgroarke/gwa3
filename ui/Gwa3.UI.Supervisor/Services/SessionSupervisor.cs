using Gwa3.UI.Supervisor.Abstractions;
using Gwa3.UI.Supervisor.Models;

namespace Gwa3.UI.Supervisor.Services;

public sealed class SessionSupervisor
{
    private readonly ILaunchPlanValidator _validator;
    private readonly ILauncherService _launcherService;
    private readonly IProcessHealthGate _healthGate;
    private readonly IInjectorService _injectorService;
    private readonly IBridgeService _bridgeService;
    private readonly ISessionStatusSink _statusSink;
    private readonly IProcessTerminator _processTerminator;

    public SessionSupervisor(
        ILaunchPlanValidator validator,
        ILauncherService launcherService,
        IProcessHealthGate healthGate,
        IInjectorService injectorService,
        IBridgeService bridgeService,
        ISessionStatusSink statusSink,
        IProcessTerminator? processTerminator = null)
    {
        _validator = validator;
        _launcherService = launcherService;
        _healthGate = healthGate;
        _injectorService = injectorService;
        _bridgeService = bridgeService;
        _statusSink = statusSink;
        _processTerminator = processTerminator ?? new ProcessTerminator();
    }

    public async Task<SessionRunResult> StartAsync(LaunchPlan plan, CancellationToken cancellationToken = default)
    {
        LauncherResult? launcher = null;
        HealthGateResult? health = null;
        int? ownedProcessId = null;

        try
        {
            await PublishAsync(plan, SessionStage.Validating, "Validating launch plan.", cancellationToken).ConfigureAwait(false);
            var validation = _validator.Validate(plan);
            if (!validation.IsValid)
            {
                var message = string.Join(Environment.NewLine, validation.Errors);
                await PublishAsync(plan, SessionStage.FailedValidation, message, cancellationToken, StatusSeverity.Error).ConfigureAwait(false);
                return Failed(plan, SessionStage.FailedValidation, message);
            }

            await PublishAsync(plan, SessionStage.Launching, "Launching Guild Wars through the configured GWLauncher script.", cancellationToken).ConfigureAwait(false);
            launcher = await _launcherService.LaunchAsync(plan, cancellationToken).ConfigureAwait(false);
            if (!launcher.Succeeded || launcher.GuildWarsProcessId is null)
            {
                await PublishAsync(plan, SessionStage.FailedLauncher, launcher.Message, cancellationToken, StatusSeverity.Error).ConfigureAwait(false);
                return Failed(plan, SessionStage.FailedLauncher, launcher.Message, launcher: launcher);
            }
            ownedProcessId = launcher.GuildWarsProcessId.Value;

            await PublishAsync(
                plan,
                SessionStage.WaitingForHealthyClient,
                $"Waiting for launcher PID {launcher.GuildWarsProcessId.Value} to pass the health gate.",
                cancellationToken).ConfigureAwait(false);

            health = await _healthGate.WaitForHealthyAsync(plan, launcher.GuildWarsProcessId.Value, cancellationToken).ConfigureAwait(false);
            if (!health.Succeeded)
            {
                await PublishAsync(plan, SessionStage.FailedHealthGate, health.Message, cancellationToken, StatusSeverity.Error).ConfigureAwait(false);
                await CleanupOwnedClientAsync(plan, ownedProcessId, SessionStage.FailedHealthGate, health.Message).ConfigureAwait(false);
                return Failed(plan, SessionStage.FailedHealthGate, health.Message, launcher: launcher, healthGate: health);
            }

            var injectionProcessId = health.ProcessId > 0
                ? health.ProcessId
                : launcher.GuildWarsProcessId.Value;
            ownedProcessId = injectionProcessId;

            var injectionMessage = injectionProcessId == launcher.GuildWarsProcessId.Value
                ? "Injecting the lane DLL into the exact launcher-returned PID."
                : $"Injecting the lane DLL into the validated live client PID {injectionProcessId} from launcher handoff.";
            await PublishAsync(plan, SessionStage.Injecting, injectionMessage, cancellationToken).ConfigureAwait(false);
            var injection = await _injectorService.InjectAsync(plan, injectionProcessId, cancellationToken).ConfigureAwait(false);
            if (!injection.Succeeded)
            {
                await PublishAsync(plan, SessionStage.FailedInjector, injection.Message, cancellationToken, StatusSeverity.Error).ConfigureAwait(false);
                await CleanupOwnedClientAsync(plan, ownedProcessId, SessionStage.FailedInjector, injection.Message).ConfigureAwait(false);
                return Failed(plan, SessionStage.FailedInjector, injection.Message, launcher: launcher, healthGate: health, injection: injection);
            }

            await PublishAsync(plan, SessionStage.StartingBridge, "Starting bridge services if the launch mode requires them.", cancellationToken).ConfigureAwait(false);
            var bridge = await _bridgeService.StartAsync(plan, cancellationToken).ConfigureAwait(false);
            if (!bridge.Succeeded)
            {
                await PublishAsync(plan, SessionStage.FailedBridge, bridge.Message, cancellationToken, StatusSeverity.Error).ConfigureAwait(false);
                await CleanupOwnedClientAsync(plan, ownedProcessId, SessionStage.FailedBridge, bridge.Message).ConfigureAwait(false);
                return Failed(plan, SessionStage.FailedBridge, bridge.Message, launcher: launcher, healthGate: health, injection: injection, bridge: bridge);
            }

            await PublishAsync(plan, SessionStage.Running, "Session supervisor reached running state.", cancellationToken).ConfigureAwait(false);
            return new SessionRunResult
            {
                SessionId = plan.SessionId,
                FinalStage = SessionStage.Running,
                Succeeded = true,
                DryRun = plan.DryRun,
                GuildWarsProcessId = injectionProcessId,
                Launcher = launcher,
                HealthGate = health,
                Injection = injection,
                Bridge = bridge,
                Message = "Session started."
            };
        }
        catch (OperationCanceledException)
        {
            await PublishAsync(plan, SessionStage.Cancelled, "Session startup was cancelled.", CancellationToken.None, StatusSeverity.Warning).ConfigureAwait(false);
            await CleanupOwnedClientAsync(plan, ownedProcessId, SessionStage.Cancelled, "Session startup was cancelled.").ConfigureAwait(false);
            return Failed(plan, SessionStage.Cancelled, "Session startup was cancelled.", launcher: launcher, healthGate: health);
        }
    }

    private async Task CleanupOwnedClientAsync(LaunchPlan plan, int? processId, SessionStage failureStage, string reason)
    {
        if (plan.DryRun || processId is not int pid || pid <= 0)
        {
            return;
        }

        try
        {
            await _processTerminator.TerminateAsync(pid, CancellationToken.None).ConfigureAwait(false);
            await PublishAsync(
                plan,
                failureStage,
                $"Stopped owned Guild Wars PID {pid} after startup failure: {reason}",
                CancellationToken.None,
                StatusSeverity.Warning).ConfigureAwait(false);
        }
        catch (Exception ex) when (ex is ArgumentException or InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            await PublishAsync(
                plan,
                failureStage,
                $"Could not stop owned Guild Wars PID {pid} after startup failure: {ex.Message}",
                CancellationToken.None,
                StatusSeverity.Warning).ConfigureAwait(false);
        }
    }

    private static SessionRunResult Failed(
        LaunchPlan plan,
        SessionStage finalStage,
        string message,
        LauncherResult? launcher = null,
        HealthGateResult? healthGate = null,
        InjectionResult? injection = null,
        BridgeResult? bridge = null) =>
        new()
        {
            SessionId = plan.SessionId,
            FinalStage = finalStage,
            Succeeded = false,
            DryRun = plan.DryRun,
            GuildWarsProcessId = healthGate?.ProcessId > 0 ? healthGate.ProcessId : launcher?.GuildWarsProcessId,
            Launcher = launcher,
            HealthGate = healthGate,
            Injection = injection,
            Bridge = bridge,
            Message = message
        };

    private ValueTask PublishAsync(
        LaunchPlan plan,
        SessionStage stage,
        string message,
        CancellationToken cancellationToken,
        StatusSeverity severity = StatusSeverity.Info) =>
        _statusSink.PublishAsync(
            new SessionStatusEvent
            {
                SessionId = plan.SessionId,
                Stage = stage,
                Severity = severity,
                Message = message,
                Details = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
                {
                    ["CharacterName"] = plan.CharacterName,
                    ["LaneTag"] = plan.LaneTag,
                    ["LaunchMode"] = plan.Mode.ToString(),
                    ["DryRun"] = plan.DryRun.ToString(System.Globalization.CultureInfo.InvariantCulture)
                }
            },
            cancellationToken);
}
