using Gwa3.UI.Core.Models;

namespace Gwa3.UI.Core.Validation;

public sealed class ProfileValidator
{
    public ProfileValidationResult Validate(Gwa3Profile profile)
    {
        ArgumentNullException.ThrowIfNull(profile);

        var issues = new List<ProfileValidationIssue>();

        ValidateRoot(profile, issues);
        ValidateBot(profile.Bot, issues);
        ValidateCharacter(profile.Character, issues);
        ValidateBuild(profile.Build, issues);
        ValidateRuntime(profile.Runtime, issues);
        ValidateMaintenance(profile.Maintenance, issues);
        ValidateInventory(profile.InventoryPolicy, issues);
        ValidateLaunch(profile.Launch, profile.Llm, issues);
        ValidateLlm(profile.Llm, profile.Launch, issues);

        return issues.Count == 0
            ? ProfileValidationResult.Success
            : new ProfileValidationResult(issues);
    }

    private static void ValidateRoot(Gwa3Profile profile, ICollection<ProfileValidationIssue> issues)
    {
        if (profile.SchemaVersion != Gwa3Profile.CurrentSchemaVersion)
        {
            AddError(issues, "schemaVersion", $"Expected schema version {Gwa3Profile.CurrentSchemaVersion}.");
        }

        RequireText(issues, "profileName", profile.ProfileName, "Give the profile a name before saving or launching.");
    }

    private static void ValidateBot(BotSelection? bot, ICollection<ProfileValidationIssue> issues)
    {
        if (bot is null)
        {
            AddError(issues, "bot", "Choose the bot module this profile should run.");
            return;
        }

        RequireText(issues, "bot.moduleId", bot.ModuleId, "Choose a bot module.");
        if (!string.IsNullOrWhiteSpace(bot.ModuleId) && !KnownBotModules.All.Contains(bot.ModuleId))
        {
            AddWarning(issues, "bot.moduleId", $"'{bot.ModuleId}' is not one of the built-in bot modules; the UI should treat it as a custom module.");
        }
    }

    private static void ValidateCharacter(CharacterSettings? character, ICollection<ProfileValidationIssue> issues)
    {
        if (character is null)
        {
            AddError(issues, "character", "Choose the account and character for the launch.");
            return;
        }

        if (character.AccountIndex is < 0)
        {
            AddError(issues, "character.accountIndex", "Account index cannot be negative.");
        }

        RequireText(issues, "character.characterName", character.CharacterName, "Choose the Guild Wars character for this profile.");

        if (character.PreferredDistrict is null)
        {
            AddWarning(issues, "character.preferredDistrict", "No preferred district was configured; the supervisor will need an explicit fallback.");
            return;
        }

        ValidateRange(issues, "character.preferredDistrict.region", character.PreferredDistrict.Region, 0, 7);
        ValidateRange(issues, "character.preferredDistrict.district", character.PreferredDistrict.District, 1, 200);
        ValidateRange(issues, "character.preferredDistrict.language", character.PreferredDistrict.Language, 0, 10);
    }

    private static void ValidateBuild(BuildSettings? build, ICollection<ProfileValidationIssue> issues)
    {
        if (build is null)
        {
            AddError(issues, "build", "Build settings are required.");
            return;
        }

        if (build.Player is null)
        {
            AddError(issues, "build.player", "Player build settings are required.");
        }
        else
        {
            ValidateSkillbar(build.Player.Skillbar, "build.player.skillbar", issues);
            ValidateAttributes(build.Player.Attributes, "build.player.attributes", issues);
            var playerSkillbar = build.Player.Skillbar;
            if (playerSkillbar is not null &&
                string.IsNullOrWhiteSpace(playerSkillbar.TemplateCode) &&
                (playerSkillbar.Skills?.Count ?? 0) == 0)
            {
                AddWarning(issues, "build.player.skillbar", "Player skillbar is empty; the bot will rely on current in-game skills.");
            }
        }

        if (build.Heroes is null)
        {
            AddError(issues, "build.heroes", "Hero loadouts must be an array, even when empty.");
            return;
        }

        var usedSlots = new HashSet<int>();
        for (var index = 0; index < build.Heroes.Count; index++)
        {
            var hero = build.Heroes[index];
            var path = $"build.heroes[{index}]";

            if (hero is null)
            {
                AddError(issues, path, "Hero entry cannot be null.");
                continue;
            }

            ValidateRange(issues, $"{path}.slot", hero.Slot, 1, 7);
            if (!usedSlots.Add(hero.Slot))
            {
                AddError(issues, $"{path}.slot", $"Hero slot {hero.Slot} is configured more than once.");
            }

            if (hero.Enabled && string.IsNullOrWhiteSpace(hero.HeroName) && hero.HeroId is null)
            {
                AddWarning(issues, $"{path}.heroName", "Enabled hero has no name or ID; the UI may not be able to show who will be added.");
            }

            ValidateSkillbar(hero.Skillbar, $"{path}.skillbar", issues);
            ValidateAttributes(hero.Attributes, $"{path}.attributes", issues);
        }
    }

    private static void ValidateRuntime(RuntimeSettings? runtime, ICollection<ProfileValidationIssue> issues)
    {
        if (runtime is null)
        {
            AddError(issues, "runtime", "Runtime settings are required.");
        }
    }

    private static void ValidateMaintenance(MaintenanceSettings? maintenance, ICollection<ProfileValidationIssue> issues)
    {
        if (maintenance is null)
        {
            AddError(issues, "maintenance", "Maintenance settings are required.");
            return;
        }

        ValidateRange(issues, "maintenance.minimumFreeSlots", maintenance.MinimumFreeSlots, 0, 45);

        if (maintenance.Kits is null)
        {
            AddError(issues, "maintenance.kits", "Kit maintenance settings are required.");
        }
        else
        {
            ValidateNonNegative(issues, "maintenance.kits.minimumIdentificationKits", maintenance.Kits.MinimumIdentificationKits);
            ValidateNonNegative(issues, "maintenance.kits.minimumSalvageKits", maintenance.Kits.MinimumSalvageKits);
            ValidateNonNegative(issues, "maintenance.kits.targetIdentificationKits", maintenance.Kits.TargetIdentificationKits);
            ValidateNonNegative(issues, "maintenance.kits.targetSalvageKits", maintenance.Kits.TargetSalvageKits);
            ValidateNonNegative(issues, "maintenance.kits.targetExpertSalvageKits", maintenance.Kits.TargetExpertSalvageKits);

            if (maintenance.Kits.TargetIdentificationKits < maintenance.Kits.MinimumIdentificationKits)
            {
                AddError(issues, "maintenance.kits.targetIdentificationKits", "Target ID kits must be greater than or equal to the minimum ID kits.");
            }

            if (maintenance.Kits.TargetSalvageKits < maintenance.Kits.MinimumSalvageKits)
            {
                AddError(issues, "maintenance.kits.targetSalvageKits", "Target salvage kits must be greater than or equal to the minimum salvage kits.");
            }
        }

        if (maintenance.Gold is null)
        {
            AddError(issues, "maintenance.gold", "Gold maintenance settings are required.");
        }
        else
        {
            ValidateRange(issues, "maintenance.gold.keepOnCharacterGold", maintenance.Gold.KeepOnCharacterGold, 0, 100_000);
            ValidateRange(issues, "maintenance.gold.depositWhenCharacterGoldAtLeast", maintenance.Gold.DepositWhenCharacterGoldAtLeast, 0, 100_000);

            if (maintenance.Gold.DepositWhenCharacterGoldAtLeast < maintenance.Gold.KeepOnCharacterGold)
            {
                AddError(issues, "maintenance.gold.depositWhenCharacterGoldAtLeast", "Deposit threshold must be greater than or equal to gold kept on the character.");
            }
        }

        if (maintenance.Consets is null)
        {
            AddError(issues, "maintenance.consets", "Conset crafting settings are required.");
        }
        else
        {
            ValidateNonNegative(issues, "maintenance.consets.craftWhenBelowSets", maintenance.Consets.CraftWhenBelowSets);
            ValidateNonNegative(issues, "maintenance.consets.targetStoredSetsEach", maintenance.Consets.TargetStoredSetsEach);
            ValidateNonNegative(issues, "maintenance.consets.materialSlotTrigger", maintenance.Consets.MaterialSlotTrigger);
            ValidateNonNegative(issues, "maintenance.consets.materialPressureFreeSlots", maintenance.Consets.MaterialPressureFreeSlots);
            ValidateNonNegative(issues, "maintenance.consets.batchSets", maintenance.Consets.BatchSets);

            if (maintenance.Consets.Enabled && maintenance.Consets.BatchSets == 0)
            {
                AddError(issues, "maintenance.consets.batchSets", "Conset crafting is enabled, so batch size must be at least 1.");
            }

            if (maintenance.Consets.Enabled && maintenance.Consets.MaterialSlotTrigger == 0)
            {
                AddError(issues, "maintenance.consets.materialSlotTrigger", "Conset material-slot trigger must be at least 1 when crafting is enabled.");
            }
        }
    }

    private static void ValidateInventory(InventoryPolicy? inventory, ICollection<ProfileValidationIssue> issues)
    {
        if (inventory is null)
        {
            AddError(issues, "inventoryPolicy", "Inventory policy settings are required.");
            return;
        }

        ValidateRuleSet(inventory.Pickup, "inventoryPolicy.pickup", issues);
        ValidateRuleSet(inventory.Identify, "inventoryPolicy.identify", issues);
        ValidateRuleSet(inventory.SalvageMaterials, "inventoryPolicy.salvageMaterials", issues);
        ValidateRuleSet(inventory.SalvageUpgrades, "inventoryPolicy.salvageUpgrades", issues);
        ValidateRuleSet(inventory.Sell, "inventoryPolicy.sell", issues);
        ValidateRuleSet(inventory.Store, "inventoryPolicy.store", issues);
        ValidateRuleSet(inventory.KeepComponents, "inventoryPolicy.keepComponents", issues);

        if (inventory.UpgradeSalvageRules is null)
        {
            AddError(issues, "inventoryPolicy.upgradeSalvageRules", "Upgrade salvage rules must be an array, even when empty.");
            return;
        }

        for (var index = 0; index < inventory.UpgradeSalvageRules.Count; index++)
        {
            var rule = inventory.UpgradeSalvageRules[index];
            var path = $"inventoryPolicy.upgradeSalvageRules[{index}]";

            if (rule is null)
            {
                AddError(issues, path, "Upgrade salvage rule cannot be null.");
                continue;
            }

            if (rule.Enabled && string.IsNullOrWhiteSpace(rule.Name) && string.IsNullOrWhiteSpace(rule.ModifierPattern))
            {
                AddError(issues, $"{path}.modifierPattern", "Enabled upgrade salvage rule needs a name or modifier pattern.");
            }

            if (rule.SalvageIndex is < 0 or > 2)
            {
                AddError(issues, $"{path}.salvageIndex", "Upgrade salvage index must be 0 (prefix), 1 (suffix/rune), or 2 (inscription).");
            }

            if (rule.Enabled && rule.Action == UpgradeSalvageAction.Salvage && rule.ItemTypes.Count == 0)
            {
                AddWarning(issues, $"{path}.itemTypes", "Enabled upgrade salvage rule applies to every item type; add itemTypes to make it GWA2-matrix-specific.");
            }
        }
    }

    private static void ValidateLaunch(
        LaunchSettings? launch,
        LlmSettings? llm,
        ICollection<ProfileValidationIssue> issues)
    {
        if (launch is null)
        {
            AddError(issues, "launch", "Launch settings are required.");
            return;
        }

        RequireText(issues, "launch.laneTag", launch.LaneTag, "Choose the isolated lane tag for this profile.");
        RequireText(issues, "launch.buildDirectory", launch.BuildDirectory, "Choose the lane build directory.");
        RequireText(issues, "launch.dllName", launch.DllName, "Choose the lane DLL name.");
        RequireText(issues, "launch.pipeName", launch.PipeName, "Choose the lane named pipe.");

        if (!string.IsNullOrWhiteSpace(launch.DllName) && !launch.DllName.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
        {
            AddError(issues, "launch.dllName", "DLL name must end with '.dll'.");
        }

        if (!string.IsNullOrWhiteSpace(launch.PipeName) && !launch.PipeName.StartsWith(@"\\.\pipe\", StringComparison.OrdinalIgnoreCase))
        {
            AddError(issues, "launch.pipeName", @"Pipe name must start with '\\.\pipe\'.");
        }

        if (launch.HealthGate is null)
        {
            AddError(issues, "launch.healthGate", "Health gate settings are required.");
        }
        else
        {
            ValidateRange(issues, "launch.healthGate.timeoutSeconds", launch.HealthGate.TimeoutSeconds, 1, 300);
            ValidateRange(issues, "launch.healthGate.pollIntervalMilliseconds", launch.HealthGate.PollIntervalMilliseconds, 50, 10_000);
            ValidateRange(issues, "launch.healthGate.requiredWorkingSetKilobytes", launch.HealthGate.RequiredWorkingSetKilobytes, 1, 2_000_000);
            ValidateRange(issues, "launch.healthGate.postHealthySettleMilliseconds", launch.HealthGate.PostHealthySettleMilliseconds, 0, 60_000);
        }

        if (launch.ExtraInjectorArguments is null)
        {
            AddError(issues, "launch.extraInjectorArguments", "Extra injector arguments must be an array, even when empty.");
        }

        if ((launch.LaunchMode == LaunchMode.Llm || launch.LaunchMode == LaunchMode.Advisory) && llm?.Mode == LlmMode.Off)
        {
            AddError(issues, "llm.mode", "LLM or advisory launch mode requires LLM mode to be enabled.");
        }
    }

    private static void ValidateLlm(
        LlmSettings? llm,
        LaunchSettings? launch,
        ICollection<ProfileValidationIssue> issues)
    {
        if (llm is null)
        {
            AddError(issues, "llm", "LLM settings are required, even when mode is off.");
            return;
        }

        if (llm.Mode != LlmMode.Off)
        {
            RequireText(issues, "llm.endpoint", llm.Endpoint, "LLM mode requires an endpoint.");
            RequireText(issues, "llm.model", llm.Model, "LLM mode requires a model.");

            if (!string.IsNullOrWhiteSpace(llm.Endpoint) &&
                (!Uri.TryCreate(llm.Endpoint, UriKind.Absolute, out var endpoint) ||
                 (endpoint.Scheme != Uri.UriSchemeHttp && endpoint.Scheme != Uri.UriSchemeHttps)))
            {
                AddError(issues, "llm.endpoint", "Endpoint must be an absolute HTTP or HTTPS URI.");
            }
        }

        if (launch?.LaunchMode == LaunchMode.Bot && llm.Mode is LlmMode.Advisory or LlmMode.Autonomous)
        {
            AddWarning(issues, "launch.launchMode", "Profile enables LLM behavior but launch mode is Bot; the bridge will not start unless the supervisor overrides launch mode.");
        }

        ValidateRange(issues, "llm.hourlyTokenCap", llm.HourlyTokenCap, 1, 100_000_000);

        if (llm.AllowedActions is null)
        {
            AddError(issues, "llm.allowedActions", "Allowed actions must be an array, even when empty.");
        }
        else if (llm.AutonomyLevel == LlmAutonomyLevel.FullAutonomy && llm.AllowedActions.Count == 0)
        {
            AddWarning(issues, "llm.allowedActions", "Full autonomy has no explicit allowed action list; the bridge should keep permissions locked down.");
        }

        if (llm.Chat is null)
        {
            AddError(issues, "llm.chat", "Chat metadata is required.");
            return;
        }

        ValidateRange(issues, "llm.chat.maxHistoryMessages", llm.Chat.MaxHistoryMessages, 1, 500);
        if (llm.Chat.Temperature < 0 || llm.Chat.Temperature > 2)
        {
            AddError(issues, "llm.chat.temperature", "Temperature must be between 0 and 2.");
        }
    }

    private static void ValidateRuleSet(
        PolicyRuleSet? ruleSet,
        string path,
        ICollection<ProfileValidationIssue> issues)
    {
        if (ruleSet is null)
        {
            AddError(issues, path, "Policy rule set is required.");
            return;
        }

        if (ruleSet.CategoryTags is null)
        {
            AddError(issues, $"{path}.categoryTags", "Category tags must be an array, even when empty.");
        }

        if (ruleSet.Rules is null)
        {
            AddError(issues, $"{path}.rules", "Rules must be an array, even when empty.");
            return;
        }

        for (var index = 0; index < ruleSet.Rules.Count; index++)
        {
            var rule = ruleSet.Rules[index];
            var rulePath = $"{path}.rules[{index}]";

            if (rule is null)
            {
                AddError(issues, rulePath, "Item rule cannot be null.");
                continue;
            }

            if (rule.ModelId is < 0)
            {
                AddError(issues, $"{rulePath}.modelId", "Model ID cannot be negative.");
            }

            if (rule.MinimumValueGold is < 0)
            {
                AddError(issues, $"{rulePath}.minimumValueGold", "Minimum value cannot be negative.");
            }

            if (rule.Enabled &&
                rule.ModelId is null &&
                string.IsNullOrWhiteSpace(rule.Name) &&
                string.IsNullOrWhiteSpace(rule.ItemType) &&
                string.IsNullOrWhiteSpace(rule.Material) &&
                string.IsNullOrWhiteSpace(rule.ModifierPattern))
            {
                AddError(issues, rulePath, "Enabled item rule must identify items by model ID, name, type, material, or modifier pattern.");
            }
        }
    }

    private static void ValidateSkillbar(
        SkillbarSettings? skillbar,
        string path,
        ICollection<ProfileValidationIssue> issues)
    {
        if (skillbar is null)
        {
            AddError(issues, path, "Skillbar settings are required.");
            return;
        }

        if (skillbar.Skills is null)
        {
            AddError(issues, $"{path}.skills", "Skills must be an array, even when empty.");
            return;
        }

        var usedSlots = new HashSet<int>();
        for (var index = 0; index < skillbar.Skills.Count; index++)
        {
            var skill = skillbar.Skills[index];
            var skillPath = $"{path}.skills[{index}]";

            if (skill is null)
            {
                AddError(issues, skillPath, "Skill entry cannot be null.");
                continue;
            }

            ValidateRange(issues, $"{skillPath}.slot", skill.Slot, 1, 8);
            if (!usedSlots.Add(skill.Slot))
            {
                AddError(issues, $"{skillPath}.slot", $"Skill slot {skill.Slot} is configured more than once.");
            }

            if (skill.SkillId is < 0)
            {
                AddError(issues, $"{skillPath}.skillId", "Skill ID cannot be negative.");
            }
        }
    }

    private static void ValidateAttributes(
        IReadOnlyList<AttributeAllocation>? attributes,
        string path,
        ICollection<ProfileValidationIssue> issues)
    {
        if (attributes is null)
        {
            AddError(issues, path, "Attributes must be an array, even when empty.");
            return;
        }

        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        for (var index = 0; index < attributes.Count; index++)
        {
            var attribute = attributes[index];
            var attributePath = $"{path}[{index}]";

            if (attribute is null)
            {
                AddError(issues, attributePath, "Attribute entry cannot be null.");
                continue;
            }

            RequireText(issues, $"{attributePath}.name", attribute.Name, "Attribute name is required.");
            ValidateRange(issues, $"{attributePath}.rank", attribute.Rank, 0, 16);

            if (!string.IsNullOrWhiteSpace(attribute.Name) && !seen.Add(attribute.Name))
            {
                AddError(issues, $"{attributePath}.name", $"Attribute '{attribute.Name}' is configured more than once.");
            }
        }
    }

    private static void RequireText(
        ICollection<ProfileValidationIssue> issues,
        string path,
        string? value,
        string message)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            AddError(issues, path, message);
        }
    }

    private static void ValidateRange(
        ICollection<ProfileValidationIssue> issues,
        string path,
        int value,
        int min,
        int max)
    {
        if (value < min || value > max)
        {
            AddError(issues, path, $"Value must be between {min} and {max}.");
        }
    }

    private static void ValidateRange(
        ICollection<ProfileValidationIssue> issues,
        string path,
        long value,
        long min,
        long max)
    {
        if (value < min || value > max)
        {
            AddError(issues, path, $"Value must be between {min} and {max}.");
        }
    }

    private static void ValidateNonNegative(
        ICollection<ProfileValidationIssue> issues,
        string path,
        int value)
    {
        if (value < 0)
        {
            AddError(issues, path, "Value cannot be negative.");
        }
    }

    private static void AddError(ICollection<ProfileValidationIssue> issues, string path, string message)
    {
        issues.Add(ProfileValidationIssue.Error(path, message));
    }

    private static void AddWarning(ICollection<ProfileValidationIssue> issues, string path, string message)
    {
        issues.Add(ProfileValidationIssue.Warning(path, message));
    }
}
