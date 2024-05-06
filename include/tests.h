#pragma once

#include <gpac/setup.h>

#define unittest(suffix) void test_##suffix(void)

extern int checks_passed;
extern int checks_failed;

static Bool verbose_ut = GF_FALSE;
static Bool fatal_ut = GF_TRUE;

#define assert_true(expr)                                            \
    do {                                                             \
        if (expr) {                                                  \
            if (verbose_ut) printf("Assertion passed: \"%s\", File: \"%s\", Line: %d, Function: \"%s\"\n", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_passed++;                                         \
        } else {                                                     \
            checks_failed++;                                         \
            if (fatal_ut) checks_failed|=0x8000000;                                            \
        }                                                            \
    } while (0)

#define assert_false(expr)                assert_true(!(expr))
#define assert_equal_str(str1, str2)      assert_true(!strcmp((str1), (str2)))
#define assert_not_equal_str(str1, str2)  assert_true(strcmp((str1), (str2)))
#define assert_equal(a, b)                assert_true((a) == (b))
#define assert_greater(a, b)              assert_true((a) > (b))
#define assert_greater_equal(a, b)        assert_true((a) >= (b))
#define assert_less(a, b)                 assert_true((a) < (b))
#define assert_less_equal(a, b)           assert_true((a) <= (b))
#define assert_not_null(ptr)              assert_true((ptr) != NULL)
