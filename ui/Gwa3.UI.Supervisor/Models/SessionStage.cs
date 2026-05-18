namespace Gwa3.UI.Supervisor.Models;

public enum SessionStage
{
    Idle = 0,
    Validating,
    Launching,
    WaitingForHealthyClient,
    Injecting,
    StartingBridge,
    Running,
    Stopping,
    Stopped,
    FailedLauncher,
    FailedHealthGate,
    FailedInjector,
    FailedBridge,
    FailedBot,
    FailedValidation,
    Cancelled
}
