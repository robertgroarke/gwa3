namespace Gwa3.UI.Core.Models;

public sealed class LlmSettings
{
    public LlmMode Mode { get; set; } = LlmMode.Off;

    public string Endpoint { get; set; } = "";

    public string Model { get; set; } = "";

    public int HourlyTokenCap { get; set; } = 10_000_000;

    public bool AllowRemote { get; set; }

    public LlmAutonomyLevel AutonomyLevel { get; set; } = LlmAutonomyLevel.Advisory;

    public List<string> AllowedActions { get; set; } = new();

    public LlmChatSettings Chat { get; set; } = new();
}

public sealed class LlmChatSettings
{
    public ChatMode Mode { get; set; } = ChatMode.OperatorSteered;

    public string PersonaName { get; set; } = "GWA3 Operator Copilot";

    public string SystemPrompt { get; set; } = "";

    public string SteeringInstructions { get; set; } = "";

    public int MaxHistoryMessages { get; set; } = 50;

    public double Temperature { get; set; } = 0.2;
}

public enum LlmMode
{
    Off,
    ChatOnly,
    Advisory,
    Autonomous
}

public enum LlmAutonomyLevel
{
    ObserveOnly,
    Advisory,
    ConfirmBeforeAction,
    LimitedAutonomy,
    FullAutonomy
}

public enum ChatMode
{
    OperatorSteered,
    RunCoach,
    DebugAssistant,
    AutonomousSupervisor
}
