#pragma once

#include <gpac/tools.h> // Bool

#define unittest(suffix) void test_##suffix(void)

extern int checks_passed;
extern int checks_failed;

static Bool verbose_ut = GF_FALSE;
static Bool fatal_ut = GF_TRUE;

#define assert_generic(expr, trace)                                  \
    do {                                                             \
        if (expr) {                                                  \
            if (verbose_ut)                                          \
                printf("Assertion passed: \"%s\", File: \"%s\", Line: %d, Function: \"%s\"\n", #expr, __FILE__, __LINE__, __func__); \
            checks_passed++;                                         \
        } else {                                                     \
            printf("Assertion failed: \"%s\", File: \"%s\", Line: %d, Function: \"%s\"\n", #expr, __FILE__, __LINE__, __func__); \
            if (verbose_ut) trace;                                   \
            checks_failed++;                                         \
            if (fatal_ut) checks_failed|=0x8000000;                  \
        }                                                            \
    } while (0)

#define assert_true(expr)                assert_generic(expr, {})
#define assert_false(expr)               assert_true(!(expr))

#define SEP "                  "
#define assert_equal_str(str1, str2)     assert_generic(!strcmp((str1), (str2)), printf(SEP #str1"(=\"%s\")\n" SEP #str2 "(=\"%s\")\n", str1, str2))
#define assert_not_equal_str(str1, str2) assert_generic( strcmp((str1), (str2)), printf(SEP #str1"(=\"%s\")\n" SEP #str2 "(=\"%s\")\n", str1, str2))
#define assert_equal_mem(m1, m2, sz)     assert_true(memcmp(m1, m2, sz) == 0)
#define assert_not_null(ptr)             assert_true((ptr) != NULL)

#define assert_equal_op(a, b, op, t)        assert_generic(a op b, { printf(SEP "\"" #a "\"(=" t  ") " #op " \""  #b "\"(=" t ")\n", a, b); })
#define assert_equal(a, b, t)               assert_equal_op(a, b, ==, t)
#define assert_greater(a, b, t)             assert_equal_op(a, b, > , t)
#define assert_greater_equal(a, b, t)       assert_equal_op(a, b, >=, t)
#define assert_less(a, b, t)                assert_equal_op(a, b, < , t)
#define assert_less_equal(a, b, t)          assert_equal_op(a, b, <=, t)
