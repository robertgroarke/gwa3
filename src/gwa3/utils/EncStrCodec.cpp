// Offline-safe string encoding functions (no Windows.h dependency).
// These are separated from StringEncoding.cpp so they can be linked into
// the test binary without pulling in Windows API static initializers.

#include <gwa3/utils/StringEncoding.h>

namespace GWA3::StringEncoding {

static constexpr uint16_t ENC_BASE      = 0x0100;
static constexpr uint16_t ENC_MORE      = 0x8000;
static constexpr uint16_t ENC_MASK      = 0x7F00;

bool IsValidEncStr(const wchar_t* str) {
    if (!str) return false;
    return static_cast<uint16_t>(str[0]) >= ENC_BASE;
}

uint32_t DecodeEncValue(const wchar_t* encStr) {
    if (!encStr) return 0;

    uint32_t value = 0;
    uint32_t shift = 0;

    for (int i = 0; i < 10; i++) {
        uint16_t word = static_cast<uint16_t>(encStr[i]);
        if (word < ENC_BASE) break;

        uint32_t payload = (word & ENC_MASK) >> 8;
        value |= (payload << shift);
        shift += 7;

        if (!(word & ENC_MORE)) break;
    }

    return value;
}

uint32_t UInt32ToEncStr(uint32_t value, wchar_t* outBuf, uint32_t outCount) {
    if (!outBuf || outCount < 2) return 0;

    uint32_t written = 0;
    uint32_t maxWords = outCount - 1;

    do {
        if (written >= maxWords) return 0;

        uint16_t payload = static_cast<uint16_t>((value & 0x7F) << 8);
        value >>= 7;

        uint16_t word = ENC_BASE | payload;
        if (value > 0) {
            word |= ENC_MORE;
        }

        outBuf[written++] = static_cast<wchar_t>(word);
    } while (value > 0);

    outBuf[written] = L'\0';
    return written;
}

} // namespace GWA3::StringEncoding
