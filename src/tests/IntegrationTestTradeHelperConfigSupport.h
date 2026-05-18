static bool ReadTradeHelperConfigFile(char* buf, size_t bufSize) {
    if (!buf || bufSize < 2) return false;
    buf[0] = '\0';

    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&ReadTradeHelperConfigFile), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "trade_helper_config.json");

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD read = 0;
    const BOOL ok = ReadFile(h, buf, static_cast<DWORD>(bufSize - 1), &read, nullptr);
    CloseHandle(h);
    if (!ok || read == 0) return false;
    buf[read] = '\0';
    return true;
}

static uint32_t ReadTradeHelperSubmitGoldConfig() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return 0;

    const char* key = strstr(buf, "\"submit_gold\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static bool ReadTradeHelperAutoSubmitConfig() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return false;

    const char* key = strstr(buf, "\"auto_submit\"");
    if (!key) return false;
    const char* colon = strchr(key, ':');
    if (!colon) return false;
    while (*colon == ':' || *colon == ' ' || *colon == '\t') ++colon;
    return _strnicmp(colon, "true", 4) == 0 || *colon == '1';
}

static bool ReadTradeHelperAutoAcceptConfig() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return false;

    const char* key = strstr(buf, "\"auto_accept\"");
    if (!key) return ReadTradeHelperAutoSubmitConfig();
    const char* colon = strchr(key, ':');
    if (!colon) return ReadTradeHelperAutoSubmitConfig();
    while (*colon == ':' || *colon == ' ' || *colon == '\t') ++colon;
    return _strnicmp(colon, "true", 4) == 0 || *colon == '1';
}

static uint32_t ReadTradeHelperOfferItemModelConfig() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return 0;

    const char* key = strstr(buf, "\"offer_item_model_id\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static uint32_t ReadTradeHelperOfferItemQuantityConfig() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return 0;

    const char* key = strstr(buf, "\"offer_item_quantity\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static uint32_t ReadTradeHelperOfferItemModelConfig2() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return 0;

    const char* key = strstr(buf, "\"offer_item_model_id_2\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static uint32_t ReadTradeHelperOfferItemQuantityConfig2() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return 0;

    const char* key = strstr(buf, "\"offer_item_quantity_2\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static uint32_t ReadTradeHelperMoveSequenceConfig() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return 0;

    const char* key = strstr(buf, "\"move_seq\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static bool ReadTradeHelperMoveTargetConfig(float& outX, float& outY) {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return false;

    const char* xKey = strstr(buf, "\"move_x\"");
    const char* yKey = strstr(buf, "\"move_y\"");
    if (!xKey || !yKey) return false;
    const char* xColon = strchr(xKey, ':');
    const char* yColon = strchr(yKey, ':');
    if (!xColon || !yColon) return false;
    outX = static_cast<float>(strtod(xColon + 1, nullptr));
    outY = static_cast<float>(strtod(yColon + 1, nullptr));
    return true;
}

static bool ReadTradeHelperJsonStringConfig(const char* configKey, char* out, size_t outSize) {
    if (!configKey || !out || outSize < 2) return false;
    out[0] = '\0';

    char buf[1024] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return false;

    char keyPattern[96] = {};
    sprintf_s(keyPattern, "\"%s\"", configKey);
    const char* key = strstr(buf, keyPattern);
    if (!key) return false;
    const char* colon = strchr(key, ':');
    if (!colon) return false;
    const char* p = colon + 1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != '\"') return false;
    ++p;

    size_t written = 0;
    while (*p && *p != '\"' && written + 1 < outSize) {
        if (*p == '\\') {
            ++p;
            if (!*p) break;
            switch (*p) {
            case '\"': out[written++] = '\"'; break;
            case '\\': out[written++] = '\\'; break;
            case '/': out[written++] = '/'; break;
            case 'b': out[written++] = '\b'; break;
            case 'f': out[written++] = '\f'; break;
            case 'n': out[written++] = '\n'; break;
            case 'r': out[written++] = '\r'; break;
            case 't': out[written++] = '\t'; break;
            case 'u':
                if (written + 1 < outSize) out[written++] = '?';
                for (int i = 0; i < 4 && p[1]; ++i) ++p;
                break;
            default:
                out[written++] = *p;
                break;
            }
        } else {
            out[written++] = *p;
        }
        ++p;
    }
    out[written] = '\0';
    return written > 0;
}

static uint32_t ReadTradeHelperChatSendSequenceConfig() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return 0;

    const char* key = strstr(buf, "\"chat_send_seq\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static bool ReadTradeHelperChatSendChannelConfig(char* out, size_t outSize) {
    return ReadTradeHelperJsonStringConfig("chat_send_channel", out, outSize);
}

static bool ReadTradeHelperChatSendMessageConfig(char* out, size_t outSize) {
    return ReadTradeHelperJsonStringConfig("chat_send_message", out, outSize);
}

static uint32_t ReadTradeHelperWhisperSendSequenceConfig() {
    char buf[512] = {};
    if (!ReadTradeHelperConfigFile(buf, sizeof(buf))) return 0;

    const char* key = strstr(buf, "\"whisper_send_seq\"");
    if (!key) return 0;
    const char* colon = strchr(key, ':');
    if (!colon) return 0;
    unsigned long value = strtoul(colon + 1, nullptr, 10);
    return static_cast<uint32_t>(value);
}

static bool ReadTradeHelperWhisperSendRecipientConfig(char* out, size_t outSize) {
    return ReadTradeHelperJsonStringConfig("whisper_send_recipient", out, outSize);
}

static bool ReadTradeHelperWhisperSendMessageConfig(char* out, size_t outSize) {
    return ReadTradeHelperJsonStringConfig("whisper_send_message", out, outSize);
}

static wchar_t MapTradeHelperChatChannelPrefix(const char* channelName) {
    if (!channelName || !channelName[0]) return L'!';
    if (_stricmp(channelName, "team") == 0 || _stricmp(channelName, "party") == 0) return L'#';
    if (_stricmp(channelName, "guild") == 0) return L'@';
    if (_stricmp(channelName, "trade") == 0) return L'$';
    return L'!';
}
