namespace Gwa3.UI.Core.Models;

public sealed class BuildSettings
{
    public PlayerBuildSettings Player { get; set; } = new();

    public string HeroConfigFile { get; set; } = "";

    public List<HeroLoadout> Heroes { get; set; } = new();
}

public sealed class PlayerBuildSettings
{
    public string PrimaryProfession { get; set; } = "";

    public string SecondaryProfession { get; set; } = "";

    public SkillbarSettings Skillbar { get; set; } = new();

    public List<AttributeAllocation> Attributes { get; set; } = new();
}

public sealed class HeroLoadout
{
    public int Slot { get; set; }

    public bool Enabled { get; set; } = true;

    public string HeroName { get; set; } = "";

    public int? HeroId { get; set; }

    public HeroBehavior Behavior { get; set; } = HeroBehavior.Guard;

    public SkillbarSettings Skillbar { get; set; } = new();

    public List<AttributeAllocation> Attributes { get; set; } = new();
}

public sealed class SkillbarSettings
{
    public string TemplateCode { get; set; } = "";

    public List<SkillSlot> Skills { get; set; } = new();
}

public sealed class SkillSlot
{
    public int Slot { get; set; }

    public int? SkillId { get; set; }

    public string Name { get; set; } = "";
}

public sealed class AttributeAllocation
{
    public string Name { get; set; } = "";

    public int Rank { get; set; }

    public bool HasSuperiorRune { get; set; }

    public string Source { get; set; } = "";
}

public enum HeroBehavior
{
    Passive,
    Guard,
    Fight,
    AvoidCombat
}
