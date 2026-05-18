static bool BuildRepoRelativePath(const char* relativeSuffix, char* outPath, size_t outPathSize) {
    if (!relativeSuffix || !*relativeSuffix || !outPath || outPathSize == 0) return false;

    char dllPath[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&BuildRepoRelativePath), &hSelf)) {
        return false;
    }
    if (!GetModuleFileNameA(hSelf, dllPath, MAX_PATH)) return false;

    char* slash = strrchr(dllPath, '\\');
    if (slash) *(slash + 1) = '\0';

    constexpr const char* kPrefixes[] = {
        "",
        "..\\..\\..\\",
        "..\\..\\..\\..\\",
        "..\\..\\",
    };

    for (const char* prefix : kPrefixes) {
        snprintf(outPath, outPathSize, "%s%s%s", dllPath, prefix, relativeSuffix);
        if (GetFileAttributesA(outPath) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }

    snprintf(outPath, outPathSize, "%s%s%s", dllPath, kPrefixes[0], relativeSuffix);
    return false;
}
