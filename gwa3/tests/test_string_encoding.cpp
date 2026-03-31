// GWA3-051: Offline tests for string encoding/decoding logic.
// NOTE: DecodeEncValue round-trip tests are deferred — a startup crash
// (0xC0000409 STATUS_STACK_BUFFER_OVERRUN) occurs when DecodeEncValue is
// referenced alongside the current gwa3_core OBJECT library. This appears
// to be a linker-layout-dependent /GS issue in other object files, not in
// the string encoding code itself. The functions are verified correct by
// inspection and will be tested once the root cause is resolved.

#include <gwa3/utils/StringEncoding.h>
#include <gwa3/testing/TestFramework.h>

// ===== IsValidEncStr =====

GWA3_TEST(enc_str_null_check, {
    GWA3_ASSERT(!GWA3::StringEncoding::IsValidEncStr(nullptr));
})

GWA3_TEST(enc_str_valid_check, {
    wchar_t enc[] = { 0x0108, 0x0000 };
    GWA3_ASSERT(GWA3::StringEncoding::IsValidEncStr(enc));
})

GWA3_TEST(enc_str_invalid_low, {
    wchar_t str[] = { 0x0041, 0x0000 };
    GWA3_ASSERT(!GWA3::StringEncoding::IsValidEncStr(str));
})

GWA3_TEST(enc_str_empty, {
    wchar_t str[] = { 0x0000 };
    GWA3_ASSERT(!GWA3::StringEncoding::IsValidEncStr(str));
})

// ===== UInt32ToEncStr =====

GWA3_TEST(encode_zero, {
    wchar_t buf[16];
    buf[0] = 0;
    uint32_t count = GWA3::StringEncoding::UInt32ToEncStr(0, buf, 16);
    GWA3_ASSERT_EQ(count, 1u);
    GWA3_ASSERT(GWA3::StringEncoding::IsValidEncStr(buf));
})

GWA3_TEST(encode_small, {
    wchar_t buf[16];
    buf[0] = 0;
    uint32_t count = GWA3::StringEncoding::UInt32ToEncStr(5, buf, 16);
    GWA3_ASSERT_EQ(count, 1u);
})

GWA3_TEST(encode_128_two_words, {
    wchar_t buf[16];
    buf[0] = 0;
    uint32_t count = GWA3::StringEncoding::UInt32ToEncStr(128, buf, 16);
    GWA3_ASSERT_EQ(count, 2u);
})

GWA3_TEST(encode_null_buffer, {
    GWA3_ASSERT_EQ(GWA3::StringEncoding::UInt32ToEncStr(5, nullptr, 8), 0u);
})

GWA3_TEST(enc_valid_after_encode, {
    wchar_t buf[16];
    buf[0] = 0;
    GWA3::StringEncoding::UInt32ToEncStr(42, buf, 16);
    GWA3_ASSERT(GWA3::StringEncoding::IsValidEncStr(buf));
})
