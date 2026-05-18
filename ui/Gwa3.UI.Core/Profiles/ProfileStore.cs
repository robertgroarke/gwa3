using System.Text.Json;
using Gwa3.UI.Core.Models;
using Gwa3.UI.Core.Validation;

namespace Gwa3.UI.Core.Profiles;

public sealed class ProfileStore
{
    private readonly ProfileValidator _validator;

    public ProfileStore(ProfileValidator? validator = null)
    {
        _validator = validator ?? new ProfileValidator();
    }

    public async Task<Gwa3Profile> LoadAsync(string path, CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);

        await using var stream = File.OpenRead(path);
        var profile = await JsonSerializer.DeserializeAsync<Gwa3Profile>(stream, ProfileJson.Options, cancellationToken)
            .ConfigureAwait(false);

        return profile ?? throw new InvalidDataException($"Profile '{path}' did not contain a profile object.");
    }

    public Gwa3Profile Load(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);

        using var stream = File.OpenRead(path);
        var profile = JsonSerializer.Deserialize<Gwa3Profile>(stream, ProfileJson.Options);
        return profile ?? throw new InvalidDataException($"Profile '{path}' did not contain a profile object.");
    }

    public async Task<ProfileLoadResult> LoadAndValidateAsync(
        string path,
        CancellationToken cancellationToken = default)
    {
        var profile = await LoadAsync(path, cancellationToken).ConfigureAwait(false);
        return new ProfileLoadResult(path, profile, _validator.Validate(profile));
    }

    public async Task SaveAsync(
        string path,
        Gwa3Profile profile,
        bool requireValid = true,
        CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        ArgumentNullException.ThrowIfNull(profile);

        var validation = _validator.Validate(profile);
        if (requireValid && !validation.IsValid)
        {
            throw new ProfileValidationException(validation);
        }

        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        await using var stream = File.Create(path);
        await JsonSerializer.SerializeAsync(stream, profile, ProfileJson.Options, cancellationToken)
            .ConfigureAwait(false);
    }

    public void Save(string path, Gwa3Profile profile, bool requireValid = true)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        ArgumentNullException.ThrowIfNull(profile);

        var validation = _validator.Validate(profile);
        if (requireValid && !validation.IsValid)
        {
            throw new ProfileValidationException(validation);
        }

        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        using var stream = File.Create(path);
        JsonSerializer.Serialize(stream, profile, ProfileJson.Options);
    }

    public async Task<IReadOnlyList<ProfileSummary>> ListProfilesAsync(
        string directory,
        string searchPattern = "*.json",
        CancellationToken cancellationToken = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(directory);

        if (!Directory.Exists(directory))
        {
            return Array.Empty<ProfileSummary>();
        }

        var summaries = new List<ProfileSummary>();
        foreach (var path in Directory.EnumerateFiles(directory, searchPattern, SearchOption.AllDirectories))
        {
            cancellationToken.ThrowIfCancellationRequested();

            try
            {
                var result = await LoadAndValidateAsync(path, cancellationToken).ConfigureAwait(false);
                summaries.Add(ProfileSummary.From(path, result.Profile, result.Validation));
            }
            catch (Exception ex) when (ex is IOException or JsonException or InvalidDataException)
            {
                summaries.Add(ProfileSummary.Unreadable(path, ex.Message));
            }
        }

        return summaries
            .OrderBy(summary => summary.ProfileName, StringComparer.OrdinalIgnoreCase)
            .ThenBy(summary => summary.Path, StringComparer.OrdinalIgnoreCase)
            .ToArray();
    }
}

public sealed record ProfileLoadResult(
    string Path,
    Gwa3Profile Profile,
    ProfileValidationResult Validation);

public sealed record ProfileSummary(
    string Path,
    string ProfileName,
    string BotModule,
    string CharacterName,
    string LaneTag,
    bool IsReadable,
    bool IsValid,
    IReadOnlyList<ProfileValidationIssue> Issues)
{
    public static ProfileSummary From(string path, Gwa3Profile profile, ProfileValidationResult validation)
    {
        return new ProfileSummary(
            path,
            profile.ProfileName,
            profile.Bot?.ModuleId ?? "",
            profile.Character?.CharacterName ?? "",
            profile.Launch?.LaneTag ?? "",
            IsReadable: true,
            validation.IsValid,
            validation.Issues);
    }

    public static ProfileSummary Unreadable(string path, string message)
    {
        return new ProfileSummary(
            path,
            System.IO.Path.GetFileNameWithoutExtension(path),
            "",
            "",
            "",
            IsReadable: false,
            IsValid: false,
            new[] { ProfileValidationIssue.Error("$", message) });
    }
}
