#pragma once

#define unittest(suffix) int test_##suffix(void)
extern int checks_passed;
extern int checks_failed;
// provide some flovoured asserts 

#define verbose_assert(expr)                                        \
    do {                                                             \
        if (expr) {                                                  \
            printf("Assertion passed: %s\nValue: %d\nFile: %s\nLine: %d\nFunction: %s\n", #expr, (expr), __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_passed++;                                         \
        } else {                                                     \
            printf("Assertion failed: %s\nValue: %d\nFile: %s\nLine: %d\nFunction: %s\n", #expr, (expr), __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_failed++;                                         \
            __ASSERT_VOID_CAST (0);                                  \
        }                                                            \
    } while (0)

#define custom_message_assert(expr, msg)                            \
    do {                                                             \
        if (expr) {                                                  \
            checks_passed++;                                         \
        } else {                                                     \
            checks_failed++;                                         \
        }                                                            \
        ((expr)                                                      \
         ? __ASSERT_VOID_CAST (0)                                    \
         : printf("Assertion failed: %s\nMessage: %s\nFile: %s\nLine: %d\nFunction: %s\n", #expr, msg, __FILE__, __LINE__, __ASSERT_FUNCTION)); \
    } while (0)

#define custom_action_assert(expr, action)                          \
    do {                                                             \
        if (expr) {                                                  \
            checks_passed++;                                         \
        } else {                                                     \
            checks_failed++;                                         \
        }                                                            \
        ((expr)                                                      \
         ? __ASSERT_VOID_CAST (0)                                    \
         : (action, __ASSERT_VOID_CAST (0)));                        \
    } while (0)
#define assert_true_(expr)                                           \
    do {                                                             \
        if (expr) {                                                  \
            printf("Assertion passed: %s\nValue: %d\nFile: %s\nLine: %d\nFunction: %s\n", #expr, (expr), __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_passed++;                                         \
        } else {                                                     \
            printf("Assertion failed: %s\nValue: %d\nFile: %s\nLine: %d\nFunction: %s\n", #expr, (expr), __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_failed++;                                         \
            __ASSERT_VOID_CAST (0);                                  \
        }                                                            \
    } while (0)

#define assert_false_(expr)                                          \
    do {                                                             \
        if (!(expr)) {                                               \
            printf("Assertion passed: !(%s)\nValue: %d\nFile: %s\nLine: %d\nFunction: %s\n", #expr, !(expr), __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_passed++;                                         \
        } else {                                                     \
            printf("Assertion failed: !(%s)\nValue: %d\nFile: %s\nLine: %d\nFunction: %s\n", #expr, !(expr), __FILE__, __LINE__, __ASSERT_FUNCTION); \
            checks_failed++;                                         \
            __ASSERT_VOID_CAST (0);                                  \
        }                                                            \
    } while (0)
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
