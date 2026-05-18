// Consolidated test module generated from small test files.
// --- merged from tests/test_core_encoding_framework_suite.cpp ---
#include <gwa3/testing/TestFramework.h>
#include "StringEncodingTestSupport.h"

// --- tests/test_assert_eq_self.cpp ---
namespace GWA3::Tests::Consolidated::test_assert_eq_self {

GWA3_TEST(assert_eq_self_test, {
    GWA3_ASSERT_EQ(0x3Eu, 0x3Eu);
    GWA3_ASSERT_EQ(42, 42);
})
} // namespace GWA3::Tests::Consolidated::test_assert_eq_self

// --- tests/test_enc_str_empty.cpp ---
namespace GWA3::Tests::Consolidated::test_enc_str_empty {

GWA3_TEST(enc_str_empty, {
    wchar_t str[] = { 0x0000 };
    GWA3_ASSERT(!GWA3::StringEncoding::IsValidEncStr(str));
})

// ===== UInt32ToEncStr =====

} // namespace GWA3::Tests::Consolidated::test_enc_str_empty

// --- tests/test_enc_str_invalid_low.cpp ---
namespace GWA3::Tests::Consolidated::test_enc_str_invalid_low {

GWA3_TEST(enc_str_invalid_low, {
    wchar_t str[] = { 0x0041, 0x0000 };
    GWA3_ASSERT(!GWA3::StringEncoding::IsValidEncStr(str));
})

} // namespace GWA3::Tests::Consolidated::test_enc_str_invalid_low

// --- tests/test_enc_str_null_check.cpp ---
namespace GWA3::Tests::Consolidated::test_enc_str_null_check {

GWA3_TEST(enc_str_null_check, {
    GWA3_ASSERT(!GWA3::StringEncoding::IsValidEncStr(nullptr));
})

} // namespace GWA3::Tests::Consolidated::test_enc_str_null_check

// --- tests/test_enc_str_valid_check.cpp ---
namespace GWA3::Tests::Consolidated::test_enc_str_valid_check {

GWA3_TEST(enc_str_valid_check, {
    wchar_t enc[] = { 0x0108, 0x0000 };
    GWA3_ASSERT(GWA3::StringEncoding::IsValidEncStr(enc));
})

} // namespace GWA3::Tests::Consolidated::test_enc_str_valid_check

// --- tests/test_enc_valid_after_encode.cpp ---
namespace GWA3::Tests::Consolidated::test_enc_valid_after_encode {

GWA3_TEST(enc_valid_after_encode, {
    wchar_t buf[16];
    buf[0] = 0;
    GWA3::StringEncoding::UInt32ToEncStr(42, buf, 16);
    GWA3_ASSERT(GWA3::StringEncoding::IsValidEncStr(buf));
})

} // namespace GWA3::Tests::Consolidated::test_enc_valid_after_encode

// --- tests/test_encode_128_two_words.cpp ---
namespace GWA3::Tests::Consolidated::test_encode_128_two_words {

GWA3_TEST(encode_128_two_words, {
    wchar_t buf[16];
    buf[0] = 0;
    uint32_t count = GWA3::StringEncoding::UInt32ToEncStr(128, buf, 16);
    GWA3_ASSERT_EQ(count, 2u);
})

} // namespace GWA3::Tests::Consolidated::test_encode_128_two_words

// --- tests/test_encode_null_buffer.cpp ---
namespace GWA3::Tests::Consolidated::test_encode_null_buffer {

GWA3_TEST(encode_null_buffer, {
    GWA3_ASSERT_EQ(GWA3::StringEncoding::UInt32ToEncStr(5, nullptr, 8), 0u);
})

} // namespace GWA3::Tests::Consolidated::test_encode_null_buffer

// --- tests/test_encode_small.cpp ---
namespace GWA3::Tests::Consolidated::test_encode_small {

GWA3_TEST(encode_small, {
    wchar_t buf[16];
    buf[0] = 0;
    uint32_t count = GWA3::StringEncoding::UInt32ToEncStr(5, buf, 16);
    GWA3_ASSERT_EQ(count, 1u);
})

} // namespace GWA3::Tests::Consolidated::test_encode_small

// --- tests/test_encode_zero.cpp ---
namespace GWA3::Tests::Consolidated::test_encode_zero {

GWA3_TEST(encode_zero, {
    wchar_t buf[16];
    buf[0] = 0;
    uint32_t count = GWA3::StringEncoding::UInt32ToEncStr(0, buf, 16);
    GWA3_ASSERT_EQ(count, 1u);
    GWA3_ASSERT(GWA3::StringEncoding::IsValidEncStr(buf));
})

} // namespace GWA3::Tests::Consolidated::test_encode_zero

// --- tests/test_framework_self.cpp ---
namespace GWA3::Tests::Consolidated::test_framework_self {

GWA3_TEST(framework_self_test, {
    GWA3_ASSERT(true);
    GWA3_ASSERT(1 + 1 == 2);
})
} // namespace GWA3::Tests::Consolidated::test_framework_self

// --- merged from tests/test_core_scanner_branch_suite.cpp ---
#include "ScannerLogicTestSupport.h"

// --- tests/test_find_in_range_all_wildcards.cpp ---
namespace GWA3::Tests::Consolidated::test_find_in_range_all_wildcards {

GWA3_TEST(find_in_range_all_wildcards, {
    // All-wildcard mask should match at position 0
    uint8_t buf[] = {0xAA, 0xBB, 0xCC};
    const char pattern[] = "\x00\x00\x00";
    const char mask[] = "???";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, start); // matches immediately
})

} // namespace GWA3::Tests::Consolidated::test_find_in_range_all_wildcards

// --- tests/test_find_in_range_empty_mask.cpp ---
namespace GWA3::Tests::Consolidated::test_find_in_range_empty_mask {

GWA3_TEST(find_in_range_empty_mask, {
    uint8_t buf[] = {0x90};
    const char pattern[] = "";
    const char mask[] = "";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, 0u); // empty pattern returns 0
})

} // namespace GWA3::Tests::Consolidated::test_find_in_range_empty_mask

// --- tests/test_find_in_range_exact_match.cpp ---
namespace GWA3::Tests::Consolidated::test_find_in_range_exact_match {

GWA3_TEST(find_in_range_exact_match, {
    // Search for pattern \x55\x8B\xEC in a known buffer
    uint8_t buf[] = {0x90, 0x90, 0x55, 0x8B, 0xEC, 0x90, 0x90};
    const char pattern[] = "\x55\x8B\xEC";
    const char mask[] = "xxx";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, start + 2);
})

} // namespace GWA3::Tests::Consolidated::test_find_in_range_exact_match

// --- tests/test_find_in_range_no_match.cpp ---
namespace GWA3::Tests::Consolidated::test_find_in_range_no_match {

GWA3_TEST(find_in_range_no_match, {
    uint8_t buf[] = {0x90, 0x90, 0x90, 0x90};
    const char pattern[] = "\x55\x8B\xEC";
    const char mask[] = "xxx";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, 0u);
})

} // namespace GWA3::Tests::Consolidated::test_find_in_range_no_match

// --- tests/test_find_in_range_pattern_longer_than_buffer.cpp ---
namespace GWA3::Tests::Consolidated::test_find_in_range_pattern_longer_than_buffer {

GWA3_TEST(find_in_range_pattern_longer_than_buffer, {
    uint8_t buf[] = {0x55, 0x8B};
    const char pattern[] = "\x55\x8B\xEC";
    const char mask[] = "xxx";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, 0u); // pattern longer than buffer
})

// ===== ResolveBranchChain =====

} // namespace GWA3::Tests::Consolidated::test_find_in_range_pattern_longer_than_buffer

// --- tests/test_find_in_range_with_offset.cpp ---
namespace GWA3::Tests::Consolidated::test_find_in_range_with_offset {

GWA3_TEST(find_in_range_with_offset, {
    uint8_t buf[] = {0x55, 0x8B, 0xEC};
    const char pattern[] = "\x55\x8B\xEC";
    const char mask[] = "xxx";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 2, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, start + 2); // found at 0, plus offset 2
})

} // namespace GWA3::Tests::Consolidated::test_find_in_range_with_offset

// --- tests/test_find_in_range_with_wildcard.cpp ---
namespace GWA3::Tests::Consolidated::test_find_in_range_with_wildcard {

GWA3_TEST(find_in_range_with_wildcard, {
    // \x55\x??\xEC — wildcard on second byte
    uint8_t buf[] = {0x90, 0x55, 0xFF, 0xEC, 0x90};
    const char pattern[] = "\x55\x00\xEC"; // byte at [1] is wildcard
    const char mask[] = "x?x";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, start + 1);
})

} // namespace GWA3::Tests::Consolidated::test_find_in_range_with_wildcard

// --- tests/test_near_call_negative_rel32.cpp ---
namespace GWA3::Tests::Consolidated::test_near_call_negative_rel32 {

GWA3_TEST(near_call_negative_rel32, {
    // Test backward (negative) relative call
    uint8_t code[8] = {};
    code[0] = 0xE8;
    int32_t rel = -0x100; // call backward
    memcpy(&code[1], &rel, 4);

    uintptr_t callAddr = reinterpret_cast<uintptr_t>(&code[0]);
    uintptr_t expected = callAddr + 5 + rel;
    uintptr_t result = GWA3::Scanner::FunctionFromNearCall(callAddr);
    GWA3_ASSERT_EQ(result, expected);
})

} // namespace GWA3::Tests::Consolidated::test_near_call_negative_rel32

// --- tests/test_near_call_non_e8_returns_zero.cpp ---
namespace GWA3::Tests::Consolidated::test_near_call_non_e8_returns_zero {

GWA3_TEST(near_call_non_e8_returns_zero, {
    // Non-E8 opcode should return 0
    uint8_t code[8] = {0x90, 0x00, 0x00, 0x00, 0x00}; // NOP
    uintptr_t result = GWA3::Scanner::FunctionFromNearCall(
        reinterpret_cast<uintptr_t>(&code[0]));
    GWA3_ASSERT_EQ(result, 0u);
})

} // namespace GWA3::Tests::Consolidated::test_near_call_non_e8_returns_zero

// --- tests/test_near_call_null_returns_zero.cpp ---
namespace GWA3::Tests::Consolidated::test_near_call_null_returns_zero {

GWA3_TEST(near_call_null_returns_zero, {
    uintptr_t result = GWA3::Scanner::FunctionFromNearCall(0);
    GWA3_ASSERT_EQ(result, 0u);
})

// ===== FindInRange (synthetic buffer scan) =====

} // namespace GWA3::Tests::Consolidated::test_near_call_null_returns_zero

// --- tests/test_near_call_resolution.cpp ---
namespace GWA3::Tests::Consolidated::test_near_call_resolution {

GWA3_TEST(near_call_resolution, {
    // Build a fake E8 (CALL rel32) instruction at a known address.
    // E8 <rel32> at address A calls target = A + 5 + rel32.
    // We'll place the instruction in a local buffer and use its actual address.
    uint8_t code[8] = {};
    code[0] = 0xE8; // CALL rel32

    // Target = &code[0] + 5 + rel32
    // We want target = &code[0] + 0x100 (arbitrary)
    // So rel32 = 0x100 - 5 = 0xFB
    int32_t rel = 0xFB;
    memcpy(&code[1], &rel, 4);

    uintptr_t callAddr = reinterpret_cast<uintptr_t>(&code[0]);
    uintptr_t expected = callAddr + 5 + rel;
    uintptr_t result = GWA3::Scanner::FunctionFromNearCall(callAddr);
    GWA3_ASSERT_EQ(result, expected);
})

} // namespace GWA3::Tests::Consolidated::test_near_call_resolution

// --- tests/test_resolve_branch_chain_e9.cpp ---
namespace GWA3::Tests::Consolidated::test_resolve_branch_chain_e9 {

GWA3_TEST(resolve_branch_chain_e9, {
    // JMP rel32 (E9) chain: hop once to a NOP
    // Layout: [E9 rel32] ... [0x90 at target]
    uint8_t code[16] = {};
    code[0] = 0xE9; // JMP rel32
    int32_t rel = 5;  // jump over remaining 4 bytes of operand + land at code[10]
    memcpy(&code[1], &rel, 4);
    code[10] = 0x90; // NOP (non-branch, chain terminates)

    uintptr_t start = reinterpret_cast<uintptr_t>(&code[0]);
    uintptr_t result = GWA3::Scanner::ResolveBranchChain(start);
    GWA3_ASSERT_EQ(result, start + 10);
})

} // namespace GWA3::Tests::Consolidated::test_resolve_branch_chain_e9

// --- tests/test_resolve_branch_chain_eb.cpp ---
namespace GWA3::Tests::Consolidated::test_resolve_branch_chain_eb {

GWA3_TEST(resolve_branch_chain_eb, {
    // JMP rel8 (EB) short jump
    uint8_t code[8] = {};
    code[0] = 0xEB; // JMP rel8
    code[1] = 0x02; // skip 2 bytes forward from code[2]
    code[4] = 0x90; // NOP at target

    uintptr_t start = reinterpret_cast<uintptr_t>(&code[0]);
    uintptr_t result = GWA3::Scanner::ResolveBranchChain(start);
    GWA3_ASSERT_EQ(result, start + 4);
})

} // namespace GWA3::Tests::Consolidated::test_resolve_branch_chain_eb

// --- tests/test_resolve_branch_chain_non_branch.cpp ---
namespace GWA3::Tests::Consolidated::test_resolve_branch_chain_non_branch {

GWA3_TEST(resolve_branch_chain_non_branch, {
    // Non-branch instruction returns itself immediately
    uint8_t code[] = {0x55}; // PUSH EBP
    uintptr_t addr = reinterpret_cast<uintptr_t>(&code[0]);
    uintptr_t result = GWA3::Scanner::ResolveBranchChain(addr);
    GWA3_ASSERT_EQ(result, addr);
})

} // namespace GWA3::Tests::Consolidated::test_resolve_branch_chain_non_branch
