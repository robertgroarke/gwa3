static bool ResolvePreferredHeroTemplate(char* outFilename, size_t outFilenameSize,
                                         char* outLabel, size_t outLabelSize) {
    if (!outFilename || outFilenameSize == 0 || !outLabel || outLabelSize == 0) return false;

    snprintf(outFilename, outFilenameSize, "Standard.txt");
    snprintf(outLabel, outLabelSize, "Standard");

    const std::string playerName = WStringToAnsi(PlayerMgr::GetPlayerName(0));
    if (playerName.empty()) return true;

    char configPath[MAX_PATH] = {};
    if (!BuildRepoRelativePath("config\\AccountConfigs.json", configPath, sizeof(configPath))) {
        return true;
    }

    std::string json;
    if (!ReadTextFile(configPath, json)) {
        return true;
    }

    std::string baseName;
    if (!TryFindHeroConfigBaseNameForPlayer(json, playerName, baseName)) {
        return true;
    }

    SetPreferredHeroTemplateName(baseName, outFilename, outFilenameSize, outLabel, outLabelSize);
    return true;
}
