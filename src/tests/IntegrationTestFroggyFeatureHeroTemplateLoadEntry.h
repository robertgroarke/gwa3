static size_t LoadHeroTemplates(const char* filename, HeroTemplateConfig* out, size_t maxCount) {
    if (!out || maxCount == 0) return 0;
    if (!filename || !*filename) return 0;

    char path[MAX_PATH] = {};
    if (!BuildHeroTemplatePath(filename, path, sizeof(path))) return 0;

    FILE* f = nullptr;
    fopen_s(&f, path, "r");
    if (!f) {
        FroggyFeatureReport("  WARN: Could not open hero config at %s", path);
        return 0;
    }

    size_t count = 0;
    char line[512] = {};
    while (count < maxCount && fgets(line, sizeof(line), f)) {
        HeroTemplateConfig parsed = {};
        if (!ParseHeroTemplateLine(line, parsed)) continue;
        out[count++] = parsed;
    }

    fclose(f);
    return count;
}
