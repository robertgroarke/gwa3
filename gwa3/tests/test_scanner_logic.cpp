// GWA3-044: Offline tests for scanner pattern logic.
// These tests exercise pattern matching, near-call resolution, and hex parsing
// against synthetic in-memory buffers — no game client needed.

#include <gwa3/core/Scanner.h>
#include <gwa3/testing/TestFramework.h>

#include <cstring>

// ===== FunctionFromNearCall =====

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

GWA3_TEST(near_call_non_e8_returns_zero, {
    // Non-E8 opcode should return 0
    uint8_t code[8] = {0x90, 0x00, 0x00, 0x00, 0x00}; // NOP
    uintptr_t result = GWA3::Scanner::FunctionFromNearCall(
        reinterpret_cast<uintptr_t>(&code[0]));
    GWA3_ASSERT_EQ(result, 0u);
})

GWA3_TEST(near_call_null_returns_zero, {
    uintptr_t result = GWA3::Scanner::FunctionFromNearCall(0);
    GWA3_ASSERT_EQ(result, 0u);
})

// ===== FindInRange (synthetic buffer scan) =====

GWA3_TEST(find_in_range_exact_match, {
    // Search for pattern \x55\x8B\xEC in a known buffer
    uint8_t buf[] = {0x90, 0x90, 0x55, 0x8B, 0xEC, 0x90, 0x90};
    const char pattern[] = "\x55\x8B\xEC";
    const char mask[] = "xxx";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, start + 2);
})

GWA3_TEST(find_in_range_with_wildcard, {
    // \x55\x??\xEC — wildcard on second byte
    uint8_t buf[] = {0x90, 0x55, 0xFF, 0xEC, 0x90};
    const char pattern[] = "\x55\x00\xEC"; // byte at [1] is wildcard
    const char mask[] = "x?x";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, start + 1);
})

GWA3_TEST(find_in_range_with_offset, {
    uint8_t buf[] = {0x55, 0x8B, 0xEC};
    const char pattern[] = "\x55\x8B\xEC";
    const char mask[] = "xxx";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 2, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, start + 2); // found at 0, plus offset 2
})

GWA3_TEST(find_in_range_no_match, {
    uint8_t buf[] = {0x90, 0x90, 0x90, 0x90};
    const char pattern[] = "\x55\x8B\xEC";
    const char mask[] = "xxx";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, 0u);
})

GWA3_TEST(find_in_range_all_wildcards, {
    // All-wildcard mask should match at position 0
    uint8_t buf[] = {0xAA, 0xBB, 0xCC};
    const char pattern[] = "\x00\x00\x00";
    const char mask[] = "???";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, start); // matches immediately
})

GWA3_TEST(find_in_range_empty_mask, {
    uint8_t buf[] = {0x90};
    const char pattern[] = "";
    const char mask[] = "";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, 0u); // empty pattern returns 0
})

GWA3_TEST(find_in_range_pattern_longer_than_buffer, {
    uint8_t buf[] = {0x55, 0x8B};
    const char pattern[] = "\x55\x8B\xEC";
    const char mask[] = "xxx";

    uintptr_t start = reinterpret_cast<uintptr_t>(buf);
    uintptr_t result = GWA3::Scanner::FindInRange(pattern, mask, 0, start, sizeof(buf));
    GWA3_ASSERT_EQ(result, 0u); // pattern longer than buffer
})

// ===== ResolveBranchChain =====

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

GWA3_TEST(resolve_branch_chain_non_branch, {
    // Non-branch instruction returns itself immediately
    uint8_t code[] = {0x55}; // PUSH EBP
    uintptr_t addr = reinterpret_cast<uintptr_t>(&code[0]);
    uintptr_t result = GWA3::Scanner::ResolveBranchChain(addr);
    GWA3_ASSERT_EQ(result, addr);
})
