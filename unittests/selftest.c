//failing fixtures, run separately by selftest.sh
#include "tests.h"

static void cleanup_memory(void *data)
{
	gf_free(data);
	printf("cleanup ran\n");
}

unittest(pass)
{
	assert_equal(gf_opts_set_key("temp", "unit-test-init", "ready"), GF_OK, "%d");
	assert_equal_str(gf_opts_get_key("temp", "unit-test-init"), "ready");
	gf_opts_set_key("temp", "unit-test-init", NULL);
	void *data = gf_malloc(32);
	assert_not_null(data);
	test_defer(cleanup_memory, data);
	assert_true(GF_TRUE);
}

unittest(fatal)
{
	void *data = gf_malloc(32);
	assert_not_null(data);
	test_defer(cleanup_memory, data);
	assert_true(GF_FALSE);
	printf("unreachable after fatal assertion\n");
}

unittest(nonfatal)
{
	check_true(GF_FALSE);
	printf("continued after check\n");
	assert_true(GF_TRUE);
}

unittest(skipped)
{
	char reason[] = "selftest feature unavailable";
	void *data = gf_malloc(32);
	assert_not_null(data);
	test_defer(cleanup_memory, data);
	test_skip(reason);
	printf("unreachable after skip\n");
}

static unsigned int cleanup_expected;

static void cleanup_number(void *data)
{
	unsigned int value = *(unsigned int *) data;
	gf_free(data);
	assert_equal(value, cleanup_expected, "%u");
	cleanup_expected--;
	printf("cleanup ran\n");
}

static void cleanup_done(void *unused)
{
	assert_equal(cleanup_expected, 0U, "%u");
	printf("cleanup order correct\n");
}

unittest(cleanup_order)
{
	unsigned int *first = gf_malloc(sizeof(*first));
	unsigned int *second = gf_malloc(sizeof(*second));
	assert_not_null(first);
	assert_not_null(second);
	*first = 1;
	*second = 2;
	cleanup_expected = 2;
	test_defer(cleanup_done, NULL);
	test_defer(cleanup_number, first);
	test_defer(cleanup_number, second);
}

#ifdef GPAC_MEMORY_TRACKING
unittest(leak)
{
	void *data = gf_malloc(32);
	assert_not_null(data);
	//intentionally not freed: the runner must return failure after shutdown
}

unittest(file_leak)
{
	FILE *file = gf_file_temp(NULL);
	assert_not_null(file);
	//intentionally not closed to exercise the file-handle leak check
}
#endif

int main(int argc, char **argv)
{
	if (register_test("pass", test_pass)
		|| register_test("fatal", test_fatal)
		|| register_test("nonfatal", test_nonfatal)
		|| register_test("skipped", test_skipped)
		|| register_test("cleanup_order", test_cleanup_order)) return EXIT_FAILURE;
#ifdef GPAC_MEMORY_TRACKING
	if (register_test("leak", test_leak)
		|| register_test("file_leak", test_file_leak)) return EXIT_FAILURE;
#endif
	return run_tests(argc, argv);
}
