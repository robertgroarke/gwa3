static bool ReadTextFile(const char* path, std::string& outText) {
    outText.clear();
    FILE* f = nullptr;
    fopen_s(&f, path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    if (len <= 0) {
        fclose(f);
        return false;
    }
    fseek(f, 0, SEEK_SET);

    outText.resize(static_cast<size_t>(len));
    if (fread(outText.data(), 1, static_cast<size_t>(len), f) != static_cast<size_t>(len)) {
        fclose(f);
        outText.clear();
        return false;
    }
    fclose(f);
    return true;
}
