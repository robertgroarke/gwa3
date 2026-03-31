#include <gwa3/testing/TestFramework.h>

// Verify the test framework itself works
GWA3_TEST(framework_self_test, {
    GWA3_ASSERT(true);
    GWA3_ASSERT(1 + 1 == 2);
})

// Verify GWA3_ASSERT_EQ works
GWA3_TEST(assert_eq_self_test, {
    GWA3_ASSERT_EQ(0x3Eu, 0x3Eu);
    GWA3_ASSERT_EQ(42, 42);
})

// GWA3-044: Packet header constant validation
#include "test_headers.cpp"

// GWA3-044: Scanner pattern logic tests
#include "test_scanner_logic.cpp"

int main() {
    return GWA3::Testing::RunAll();
}
