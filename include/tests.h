#pragma once

#define unittest(suffix) int test_##suffix(void)

#if 0 // TODO
extern int checks_passed;
extern int checks_failed;

static const int debug = 0;

#define ASSERT_TRUE(expr)                                                                                              \
  do {                                                                                                                 \
    if(!(expr)) {                                                                                                      \
      printf("Assertion failed: %s\n", #expr);                                                                         \
      checks_failed++;                                                                                                 \
    } else {                                                                                                           \
      checks_passed++;                                                                                                 \
    }                                                                                                                  \
  } while(0)

#define ASSERT(expr)                                                                                                   \
  do {                                                                                                                 \
    if(!(expr)) {                                                                                                      \
      printf("Assertion failed: %s\n", #expr);                                                                         \
      checks_failed++;                                                                                                 \
    } else {                                                                                                           \
      if(debug)                                                                                                        \
        fprintf(stderr, "[Debug] Assertion succeeded: %s\n", #expr);                                                   \
      checks_passed++;                                                                                                 \
    }                                                                                                                  \
  } while(0)

#define ASSERT_EQUALS(expected, actual)                                                                                \
  do {                                                                                                                 \
    if((expected) != (actual)) {                                                                                       \
      printf("Assertion failed: expected: %d, actual: %d\n", (int)(expected), (int)(actual));                          \
      checks_failed++;                                                                                                 \
    } else {                                                                                                           \
      checks_passed++;                                                                                                 \
      if(debug)                                                                                                        \
        printf("[Debug] Assertion succeeded. Expected: %d, Actual: %d\n", (int)(expected), (int)(actual));             \
    }                                                                                                                  \
  } while(0)

#define ASSERT_EQUALS_STR(expected, actual)                                                                            \
  do {                                                                                                                 \
    if(strcmp(expected, actual)) {                                                                                     \
      printf("Assertion failed: expected: %s, actual: %s\n", expected, actual);                                        \
      checks_failed++;                                                                                                 \
    } else {                                                                                                           \
      checks_passed++;                                                                                                 \
      if(debug)                                                                                                        \
        fprintf(stderr, "[Debug] Assertion succeeded. Expected: %s, Actual: %s\n", expected, actual);                  \
    }                                                                                                                  \
  } while(0)
#endif
