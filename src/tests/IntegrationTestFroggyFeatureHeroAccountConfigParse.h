static bool TryFindHeroConfigBaseNameForPlayer(const std::string& json,
                                               const std::string& playerName,
                                               std::string& outBaseName) {
    outBaseName.clear();
    const std::string quotedName = "\"" + playerName + "\"";
    const size_t namePos = json.find(quotedName);
    if (namePos == std::string::npos) return false;

    const size_t heroConfigPos = json.find("\"hero_config\"", namePos);
    if (heroConfigPos == std::string::npos) return false;

    const size_t colonPos = json.find(':', heroConfigPos);
    if (colonPos == std::string::npos) return false;

    const size_t firstQuote = json.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) return false;

    const size_t secondQuote = json.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos || secondQuote <= firstQuote + 1) return false;

    outBaseName = json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    return !outBaseName.empty();
}

static void SetPreferredHeroTemplateName(const std::string& baseName,
                                         char* outFilename,
                                         size_t outFilenameSize,
                                         char* outLabel,
                                         size_t outLabelSize) {
    snprintf(outLabel, outLabelSize, "%s", baseName.c_str());
    if (baseName.size() >= 4 &&
        _stricmp(baseName.c_str() + static_cast<int>(baseName.size()) - 4, ".txt") == 0) {
        snprintf(outFilename, outFilenameSize, "%s", baseName.c_str());
    } else {
        snprintf(outFilename, outFilenameSize, "%s.txt", baseName.c_str());
    }
}
