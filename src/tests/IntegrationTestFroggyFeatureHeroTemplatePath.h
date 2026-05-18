static bool BuildHeroTemplatePath(const char* filename, char* outPath, size_t outPathSize) {
    if (!filename || !*filename || !outPath || outPathSize == 0) return false;

    char dllPath[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&BuildHeroTemplatePath), &hSelf);
    GetModuleFileNameA(hSelf, dllPath, MAX_PATH);
    char* slash = strrchr(dllPath, '\\');
    if (slash) *(slash + 1) = '\0';

    constexpr const char* kPrefixes[] = {
        "",
        "..\\..\\..\\",
        "..\\..\\..\\..\\",
        "..\\..\\",
    };

    for (const char* prefix : kPrefixes) {
        snprintf(outPath, outPathSize, "%s%sconfig\\hero_configs\\%s", dllPath, prefix, filename);
        if (GetFileAttributesA(outPath) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }

    snprintf(outPath, outPathSize, "%s%sconfig\\hero_configs\\%s", dllPath, kPrefixes[0], filename);
    return false;
}
