# Unit tests in GPAC

## Build and run

Enable unit tests with `./configure --unittests`, then run `make -j2`.
The normal build runs the tests before building applications and stops on failure.

To execute the steps manually:
```sh
make unit_tests                         # build and run, including harness checks
make unit_tests_build                   # build without running
make unit_tests TEST_ARGS='--only xml_translate_xml_string'
unittests/launch.sh --list
unittests/launch.sh --only xml_translate_xml_string
```

The runner also accepts `--only INDEX` and `--verbose`. Names are preferable to
indices, which change when tests are added. Unknown names, invalid indices, and
unknown options return failure.

Disabled features are reported as skipped tests.

By design, listing doesn't initialize GPAC. Test execution initializes an in-memory GPAC
profile, so it neither depends on nor saves the user's configuration.

The executable is `bin/gcc/unittests` (`unittests.exe` on Windows). For an
out-of-tree build, run the launcher from the build directory, or set
`GPAC_UNITTEST_BINARY` to that build's executable. No alternate `libgpac` loader
path is needed since we now build from the static library (`libgpac_static.a`).
`make clean` removes test artifacts (including the obsolete
second build left by earlier versions).

These tests complement the separate testsuite. The unit tests in `gpac.c` and
the `units` filter are independent of this harness.

## Build organization

Tests link against the usual `libgpac_static.a`, reusing the same objects, configuration,
and dependency libraries as the production build. Non-static internal functions
can be called through this static link without exporting them from the shared
library.

Tests needing static functions or implementation-private types can include the
implementation `.c`, as the filter tests do. This recompiles only the relevant
implementation for testing. Include the complete implementation: its externally
linked definitions must be supplied by the test object so the linker does not
also extract the original object from the archive. Do not add duplicate stubs for
functions or globals supplied by the library.

Specific compiler flags are in `unittests/unit_tests.mak`.

One will need to use separate build directories for sanitizer or different feature configurations, as all objects linked into a test must use compatible flags.

## Writing a unit test

Include `tests.h` and declare each test with `unittest(name)`:

```c
#include "tests.h"
#include "../xml_parser.c"

static void free_string(void *data)
{
    gf_free(data);
}

unittest(xml_translate_xml_string)
{
    char *str = xml_translate_xml_string("&amp;");
    test_defer(free_string, str);
    assert_not_null(str);
    assert_equal_str(str, "&");
}
```

`assert_*()` records a failure and immediately stops the test, including when
called from a helper or synchronous callback. The runner executes registered
cleanups, stops the suite after a fatal assertion, and returns failure.
`check_true` and `check_false` record failures but continue, for checks where
continuing is safe. Do not rely on code after an assertion to release resources.

The new `test_defer(cleanup, resource)` registers a cleanup that runs in reverse
order on success, skip, or failure. Pass the resource itself or a heap-owned fixture;
never pass the address of a local variable, since the test's stack frame may
already have been unwound. Do not free a deferred resource separately.
Assertions, skips, and cleanup registration must run on the runner thread;
worker threads should report results to that thread after being joined.

Memory-tracking builds will rightfully fail if GPAC allocations or file handles remain after
shutdown. `unittests/selftest.sh` exercises the runner's success, failure, cleanup,
skip, argument-validation, and leak behavior using a separate fixture executable.

For unavailable features, keep a named skipped test rather than an empty body:

```c
#include "tests.h"

#ifndef GPAC_DISABLE_NETWORK
unittest(cache_accepts_valid_content_length)
{
    /* Test the cache. */
}
#else
unittest_disabled(cache_accepts_valid_content_length, "network support disabled")
#endif
```

A test can also call `test_skip("reason")` at runtime. Include `tests.h` before
checking feature macros so the configured feature definitions are available.
