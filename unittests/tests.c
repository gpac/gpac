#include "tests.h"

#include <errno.h>
#include <setjmp.h>

#define MAX_NUM_TESTS 1024
#define MAX_NUM_CLEANUPS 64

int checks_passed = 0;
int checks_failed = 0;
Bool verbose_ut = GF_FALSE;

static struct TestCase {
	const char *name;
	void (*test_function)(void);
} tests[MAX_NUM_TESTS];
static unsigned int test_count;

static struct TestCleanup {
	void (*function)(void *);
	void *data;
} cleanups[MAX_NUM_CLEANUPS];
static unsigned int cleanup_count;
static jmp_buf test_jump;
static Bool test_active;
static Bool test_aborted;
static char skip_reason[256];

int register_test(const char *name, void (*test_function)(void))
{
	unsigned int i;
	if (!name || !test_function || test_count == MAX_NUM_TESTS) {
		fprintf(stderr, "Invalid test registration or too many tests.\n");
		return 1;
	}
	for (i = 0; i < test_count; i++) {
		if (!strcmp(name, tests[i].name)) {
			fprintf(stderr, "Duplicate test: %s\n", name);
			return 1;
		}
	}
	tests[test_count].name = name;
	tests[test_count++].test_function = test_function;
	return 0;
}

void test_abort(void)
{
	if (!test_active) {
		fprintf(stderr, "Assertion outside a running test.\n");
		exit(EXIT_FAILURE);
	}
	test_aborted = GF_TRUE;
	longjmp(test_jump, 1);
}

void test_skip(const char *reason)
{
	//caller stack unwinded by test_abort
	gf_strlcpy(skip_reason, (reason && reason[0]) ? reason : "not available", sizeof(skip_reason));
	test_abort();
}

void test_defer(void (*cleanup)(void *), void *data)
{
	if (!cleanup || !test_active || cleanup_count == MAX_NUM_CLEANUPS) {
		if (cleanup) cleanup(data);
		fprintf(stderr, "Cannot register test cleanup.\n");
		checks_failed++;
		test_abort();
	}
	cleanups[cleanup_count].function = cleanup;
	cleanups[cleanup_count++].data = data;
}

static int select_test(const char *selection)
{
	unsigned int i;
	char *end;
	unsigned long index;
	for (i = 0; i < test_count; i++) {
		if (!strcmp(selection, tests[i].name)) return (int) i;
	}
	if (selection[0] < '0' || selection[0] > '9') return -1;
	errno = 0;
	index = strtoul(selection, &end, 10);
	if (errno || *end || index >= test_count) return -1;
	return (int) index;
}

int run_tests(int argc, char *argv[])
{
	int i, selected_test = -1;
	int ret = EXIT_SUCCESS;
	unsigned int tests_passed = 0, tests_failed = 0, tests_skipped = 0;
	Bool list = GF_FALSE;
	GF_Err e;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--list") || !strcmp(argv[i], "-l")) {
			list = GF_TRUE;
		} else if (!strcmp(argv[i], "--only")) {
			if (++i == argc) {
				fprintf(stderr, "Missing argument for --only\n");
				return EXIT_FAILURE;
			}
			selected_test = select_test(argv[i]);
			if (selected_test < 0) {
				fprintf(stderr, "Test not found: %s\n", argv[i]);
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--verbose")) {
			verbose_ut = GF_TRUE;
		} else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			printf("Usage: %s [--list] [--only NAME|INDEX] [--verbose]\n", argv[0]);
			return EXIT_SUCCESS;
		} else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			return EXIT_FAILURE;
		}
	}
	if (!test_count) {
		fprintf(stderr, "No unit tests registered.\n");
		return EXIT_FAILURE;
	}
	if (list) {
		for (i = 0; i < (int) test_count; i++)
			printf("test[%d]: %s\n", i, tests[i].name);
		return EXIT_SUCCESS;
	}

#ifdef GPAC_MEMORY_TRACKING
	e = gf_sys_init(GF_MemTrackerSimple, "n");
#else
	e = gf_sys_init(GF_MemTrackerNone, "n");
#endif
	if (e != GF_OK) {
		fprintf(stderr, "Cannot initialize GPAC: %s\n", gf_error_to_string(e));
		return EXIT_FAILURE;
	}
	checks_passed = checks_failed = 0;
	for (i = 0; i < (int) test_count; i++) {
		int previous_failures = checks_failed;
		if (selected_test >= 0 && selected_test != i) continue;
		printf("Test %04d: %s... ", i, tests[i].name);
		fflush(stdout);

		cleanup_count = 0;
		skip_reason[0] = 0;
		test_aborted = GF_FALSE;
		test_active = GF_TRUE;
		if (setjmp(test_jump) == 0)
			tests[i].test_function();
		//pop before calling so an assertion in a cleanup cannot call cleanup twice
		while (cleanup_count) {
			struct TestCleanup cleanup = cleanups[--cleanup_count];
			cleanup.function(cleanup.data);
		}
		test_active = GF_FALSE;

		if (checks_failed != previous_failures) {
			printf("\nFailed.\n");
			tests_failed++;
			ret = EXIT_FAILURE;
			/* A fatal assertion can leave library state unsuitable for another test. */
			if (test_aborted) break;
		} else if (skip_reason[0]) {
			printf("Skipped: %s\n", skip_reason);
			tests_skipped++;
		} else {
			printf("Success.\n");
			tests_passed++;
		}
	}
	printf("\nTests passed: %u\nTests failed: %u\nTests skipped: %u\n", tests_passed, tests_failed, tests_skipped);
	printf("Checks passed: %d\nChecks failed: %d\n", checks_passed, checks_failed);
	gf_sys_close();
#ifdef GPAC_MEMORY_TRACKING
	if (gf_memory_size() || gf_file_handles_count()) {
		fprintf(stderr, "Leaked GPAC resources.\n");
		gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
		gf_memory_print();
		ret = EXIT_FAILURE;
	}
#endif
	return ret;
}
