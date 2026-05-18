bool TestPersonalDir() {
    IntReport("=== Personal Dir ===");

    wchar_t buf[MAX_PATH] = {};
    bool ok = MemoryMgr::GetPersonalDir(buf, MAX_PATH);
    IntReport("  GetPersonalDir: %s", ok ? "ok" : "failed");
    IntCheck("GetPersonalDir returned true", ok);

    if (ok) {
        char narrow[MAX_PATH] = {};
        for (int i = 0; i < MAX_PATH - 1 && buf[i]; ++i) {
            narrow[i] = (buf[i] < 128) ? static_cast<char>(buf[i]) : '?';
        }
        IntReport("  Path: %s", narrow);
        IntCheck("Path starts with drive letter", buf[0] >= L'A' && buf[0] <= L'Z');
        IntCheck("Path contains Guild Wars directory name", wcsstr(buf, L"Guild Wars") != nullptr);
    }

    IntReport("");
    return true;
}

// ===== CallTarget in Explorable (Sparkfly) =====
