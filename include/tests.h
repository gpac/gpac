#pragma once

#include <gpac/tools.h> // Bool
#include <stdio.h>      // printf
#include <stdlib.h>     // NULL
#include <string.h>     // strcmp, memcmp

#ifdef GPAC_UNIT_TEST_LIST
#define unittest(suffix) GPAC_UNIT_TEST_ENTRY(suffix)
#define unittest_disabled(suffix, reason) GPAC_UNIT_TEST_ENTRY(suffix)
#else
#define unittest(suffix) void test_##suffix(void)
#define unittest_disabled(suffix, reason) unittest(suffix) { test_skip(reason " (" __FILE__ ")"); }
#endif

int register_test(const char *name, void (*test_function)(void));
int run_tests(int argc, char *argv[]);

//cleanup need run in reverse order on success, skip, and assertion failure
void test_abort(void);
void test_skip(const char *reason);
void test_defer(void (*cleanup)(void *), void *data);

extern int checks_passed;
extern int checks_failed;

extern Bool verbose_ut;

#define test_check(expr, trace, fatal)                               \
    do {                                                             \
        if (expr) {                                                  \
            if (verbose_ut)                                          \
                printf("\nAssertion passed: \"%s\"\n\tFile: \"%s\"\n\tLine: %d\n\tFunction: \"%s\"\n", #expr, __FILE__, __LINE__, __func__); \
            checks_passed++;                                         \
        } else {                                                     \
            printf("\nAssertion failed: \"%s\"\n", #expr);            \
            trace;                                                   \
            printf("\tFile: \"%s\"\n\tLine: %d\n\tFunction: \"%s\"\n", __FILE__, __LINE__, __func__); \
            checks_failed++;                                         \
            if (fatal) test_abort();                                 \
        }                                                            \
    } while (0)

#define assert_generic(expr, trace) test_check(expr, trace, GF_TRUE)
#define check_true(expr) test_check(expr, {}, GF_FALSE)
#define check_false(expr) check_true(!(expr))

#define assert_true(expr)                assert_generic(expr, {})
#define assert_false(expr)               assert_true(!(expr))

#define SEP "                  "
#define assert_equal_str(str1, str2)     assert_generic(!strcmp((str1), (str2)), printf("\t" #str1"(=\"%s\")\n" SEP #str2 "(=\"%s\")\n", str1, str2))
#define assert_not_equal_str(str1, str2) assert_generic( strcmp((str1), (str2)), printf("\t" #str1"(=\"%s\")\n" SEP #str2 "(=\"%s\")\n", str1, str2))
#define assert_equal_mem(m1, m2, sz)     assert_true(memcmp(m1, m2, sz) == 0)
#define assert_not_null(ptr)             assert_true((ptr) != NULL)

#define assert_equal_op(a, b, op, t)        assert_generic(a op b, { printf("\t" #a "(=" t  ") " #op " "  #b "(=" t ")\n", a, b); })
#define assert_equal(a, b, t)               assert_equal_op(a, b, ==, t)
#define assert_greater(a, b, t)             assert_equal_op(a, b, > , t)
#define assert_greater_equal(a, b, t)       assert_equal_op(a, b, >=, t)
#define assert_less(a, b, t)                assert_equal_op(a, b, < , t)
#define assert_less_equal(a, b, t)          assert_equal_op(a, b, <=, t)
