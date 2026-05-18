static char* TrimHeroTemplateField(char* text) {
    while (*text == ' ' || *text == '\t') ++text;
    char* end = text + strlen(text);
    while (end > text && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }
    return text;
}

static bool ParseHeroTemplateLine(char* line, HeroTemplateConfig& out) {
    char* semi = strchr(line, ';');
    if (semi) *semi = '\0';

    char* p = TrimHeroTemplateField(line);
    if (*p == '\0') return false;

    char* comma = strchr(p, ',');
    if (!comma) return false;
    *comma = '\0';

    const uint32_t heroId = static_cast<uint32_t>(atoi(p));
    char* tmpl = TrimHeroTemplateField(comma + 1);
    if (heroId == 0 || *tmpl == '\0') return false;

    HeroTemplateConfig parsed = {};
    parsed.heroId = heroId;
    if (!DecodeSkillTemplateCode(tmpl, parsed.skills)) return false;
    out = parsed;
    return true;
}
