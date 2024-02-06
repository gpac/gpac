#!/bin/bash

echo '#include <stdio.h>
#include <stdlib.h> // EXIT_FAILURE

int register_test(const char *name, int (*test_function)(void));
int run_tests(int argc, char *argv[]);

#define unittest(ut)                                                                                                   \
  int test_##ut();                                                                                                     \
  if(register_test(#ut, test_##ut)) {                                                                                  \
    fprintf(stderr, "registration failed for unit test: %s\n", #ut);                                                   \
    return EXIT_FAILURE;                                                                                               \
  }

int main(int argc, char *argv[])
{'

calls=$(find . -path "*/unittests/*.c" | xargs grep unittest | cut -d ":" -f 2)
for call in $calls; do
    echo "  $call;"
done

echo '
  return run_tests(argc, argv);
}
'
