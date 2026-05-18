static std::string WStringToAnsi(const wchar_t* text) {
    if (!text || !*text) return {};
    const int needed = WideCharToMultiByte(CP_ACP, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out;
    out.resize(static_cast<size_t>(needed));
    WideCharToMultiByte(CP_ACP, 0, text, -1, out.data(), needed, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}
