#pragma once

#include <gpac/setup.h>

#define unittest(suffix) void test_##suffix(void)

extern int checks_passed;
extern int checks_failed;

static Bool verbose_ut = GF_FALSE;

// provide some flavoured asserts 

#define custom_message_assert(expr, msg)                             \
    do {                                                             \
        if (expr) {                                                  \
            checks_passed++;                                         \
        } else {                                                     \
            checks_failed++;                                         \
        }                                                            \
        ((expr)                                                      \
         ? gf_fatal_assert(0)                                        \
         : printf("Assertion failed: %s\nMessage: %s\n\tFile: %s\nLine: %d\nFunction: %s\n", #expr, msg, __FILE__, __LINE__, __ASSERT_FUNCTION)); \
    } while (0)

#define custom_action_assert(expr, action)                           \
    do {                                                             \
        if (expr) {                                                  \
            checks_passed++;                                         \
        } else {                                                     \
            checks_failed++;                                         \
        }                                                            \
        ((expr)                                                      \
         ? gf_fatal_assert(0)                                        \
         : (action, gf_fatal_assert(0)));                            \
    } while (0)

#define assert_true(expr)                                            \
    do {                                                             \
        if (expr) {                                                  \
            if (verbose_ut) printf("Assertion passed: \"%s\", File: \"%s\", Line: %d, Function: \"%s\"\n", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_passed++;                                         \
        } else {                                                     \
            checks_failed++;                                         \
            gf_assert(0);                                            \
        }                                                            \
    } while (0)

#define assert_false(expr) assert_true(!(expr))

#define assert_equal_str(str1, str2)                                 \
    do {                                                             \
        if (!strcmp(str1, str2)) {                                   \
            if (verbose_ut) printf("Assertion passed: Value: (%s, %s), File: %s, Line: %d, Function: %s\n", #str1, #str2, __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_passed++;                                         \
        } else {                                                     \
            checks_failed++;                                         \
            gf_assert(0);                                            \
        }                                                            \
    } while (0)
