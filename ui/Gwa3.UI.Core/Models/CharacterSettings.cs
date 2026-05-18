namespace Gwa3.UI.Core.Models;

public sealed class CharacterSettings
{
    public int? AccountIndex { get; set; }

    public string CharacterName { get; set; } = "";

    public string AccountLabel { get; set; } = "";

    public string PreferredMap { get; set; } = "";

    public DistrictSettings PreferredDistrict { get; set; } = new();
}

public sealed class DistrictSettings
{
    public int Region { get; set; } = 4;

    public int District { get; set; } = 99;

    public int Language { get; set; } = 8;

    public string Label { get; set; } = "Asia/Japan 99";
}
