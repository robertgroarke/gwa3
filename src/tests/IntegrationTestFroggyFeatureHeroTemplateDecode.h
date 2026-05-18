static int Base64CharToVal(char c) {
    static const char* kBase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char* p = strchr(kBase64, c);
    return p ? static_cast<int>(p - kBase64) : -1;
}

static bool DecodeSkillTemplateCode(const char* code, uint32_t skillIds[8]) {
    if (!code || !*code || !skillIds) return false;

    uint8_t bits[256] = {};
    int totalBits = 0;
    for (int i = 0; code[i] && totalBits < 240; ++i) {
        const int val = Base64CharToVal(code[i]);
        if (val < 0) continue;
        for (int b = 0; b < 6; ++b) {
            bits[totalBits++] = static_cast<uint8_t>((val >> b) & 1);
        }
    }

    int pos = 0;
    auto readBits = [&](int count) -> uint32_t {
        uint32_t val = 0;
        for (int i = 0; i < count && pos < totalBits; ++i) {
            val |= (bits[pos++] << i);
        }
        return val;
    };

    const uint32_t header = readBits(4);
    if (header == 14) {
        readBits(4);
    } else if (header != 0) {
        return false;
    }

    const uint32_t profBits = readBits(2) * 2 + 4;
    readBits(profBits);
    readBits(profBits);

    const uint32_t attrCount = readBits(4);
    const uint32_t attrBits = readBits(4) + 4;
    for (uint32_t i = 0; i < attrCount; ++i) {
        readBits(attrBits);
        readBits(4);
    }

    const uint32_t skillBits = readBits(4) + 8;
    for (int i = 0; i < 8; ++i) {
        skillIds[i] = readBits(skillBits);
    }
    return true;
}
