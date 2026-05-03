#pragma once

// GWA3 Offline Test Framework
// Lightweight test harness for compile-time and logic tests.

#include <cstdio>
#include <cstdlib>

namespace GWA3::Testing {

struct TestCase {
    const char* name;
    void (*func)();
    TestCase* next;
};

inline TestCase*& GetHead() {
    static TestCase* head = nullptr;
    return head;
}

inline int& GetPassCount() {
    static int count = 0;
    return count;
}

inline int& GetFailCount() {
    static int count = 0;
    return count;
}

struct TestRegistrar {
    explicit TestRegistrar(TestCase* tc) {
        tc->next = GetHead();
        GetHead() = tc;
    }
};

inline int RunAll() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== GWA3 Offline Tests ===\n");

    TestCase* prev = nullptr;
    TestCase* curr = GetHead();
    while (curr) {
        TestCase* next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    GetHead() = prev;

    for (TestCase* tc = GetHead(); tc; tc = tc->next) {
        std::printf("  [RUN ] %s\n", tc->name);
        tc->func();
        std::printf("  [PASS] %s\n", tc->name);
        GetPassCount()++;
    }

    std::printf("=== %d PASSED, %d FAILED ===\n", GetPassCount(), GetFailCount());
    return GetFailCount() > 0 ? 1 : 0;
}

} // namespace GWA3::Testing

#define GWA3_TEST(test_name, ...)                                             \
    static void gwa3_test_func_##test_name();                                 \
    static GWA3::Testing::TestCase gwa3_test_case_##test_name = {             \
        #test_name, gwa3_test_func_##test_name, nullptr                       \
    };                                                                        \
    static GWA3::Testing::TestRegistrar gwa3_test_reg_##test_name(            \
        &gwa3_test_case_##test_name                                           \
    );                                                                        \
    static void gwa3_test_func_##test_name() { __VA_ARGS__ }

#ifdef _MSC_VER
#define GWA3_ASSERT(condition)                                                \
    __pragma(warning(suppress: 4127))                                          \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            GWA3::Testing::GetFailCount()++;                                  \
            std::abort();                                                     \
        }                                                                      \
    } while (0)
#else
#define GWA3_ASSERT(condition)                                                \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            GWA3::Testing::GetFailCount()++;                                  \
            std::abort();                                                     \
        }                                                                      \
    } while (0)
#endif

#ifdef _MSC_VER
#define GWA3_ASSERT_EQ(actual, expected)                                      \
    __pragma(warning(suppress: 4127))                                          \
    do {                                                                       \
        auto _a = (actual);                                                    \
        auto _e = (expected);                                                  \
        if (_a != _e) {                                                        \
            std::printf("  [FAIL] %s:%d: %s == 0x%X, expected 0x%X\n",        \
                        __FILE__, __LINE__, #actual,                          \
                        static_cast<unsigned>(_a), static_cast<unsigned>(_e)); \
            GWA3::Testing::GetFailCount()++;                                  \
            std::abort();                                                     \
        }                                                                      \
    } while (0)
#else
#define GWA3_ASSERT_EQ(actual, expected)                                      \
    do {                                                                       \
        auto _a = (actual);                                                    \
        auto _e = (expected);                                                  \
        if (_a != _e) {                                                        \
            std::printf("  [FAIL] %s:%d: %s == 0x%X, expected 0x%X\n",        \
                        __FILE__, __LINE__, #actual,                          \
                        static_cast<unsigned>(_a), static_cast<unsigned>(_e)); \
            GWA3::Testing::GetFailCount()++;                                  \
            std::abort();                                                     \
        }                                                                      \
    } while (0)
#endif

#define GWA3_CHECK_OFFSET(StructType, field, expected_offset)                 \
    static_assert(                                                             \
        offsetof(StructType, field) == (expected_offset),                     \
        "Offset mismatch: " #StructType "::" #field                           \
    )

#define GWA3_CHECK_SIZE(StructType, expected_size)                            \
    static_assert(                                                             \
        sizeof(StructType) == (expected_size),                                \
        "Size mismatch: " #StructType                                         \
    )
