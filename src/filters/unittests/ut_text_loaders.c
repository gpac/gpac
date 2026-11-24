#include "tests.h"
#include <string.h>
#include <gpac/internal/media_dev.h>

#ifndef GPAC_DISABLE_VTT
typedef struct
{
	char *header;
	u32 header_count;
} WebVTTHeaderTest;

static GF_Err webvtt_header_test_report(void *user, GF_Err error, char *message, const char *line)
{
	(void) user;
	(void) message;
	(void) line;
	return error;
}

static void webvtt_header_test_sample(void *user, GF_WebVTTSample *sample)
{
	(void) user;
	gf_webvtt_sample_del(sample);
}

static void webvtt_header_test_capture(void *user, const char *header)
{
	WebVTTHeaderTest *test = (WebVTTHeaderTest *) user;
	if (test->header) gf_free(test->header);
	test->header = gf_strdup(header);
	test->header_count++;
}

static Bool webvtt_header_test_append(FILE *stream, const char *data)
{
	long position = ftell(stream);
	size_t size = strlen(data);
	if ((position < 0) || (fseek(stream, 0, SEEK_END) != 0)) return GF_FALSE;
	if ((fwrite(data, 1, size, stream) != size) || (fflush(stream) != 0)) return GF_FALSE;
	return (fseek(stream, position, SEEK_SET) == 0) ? GF_TRUE : GF_FALSE;
}

unittest(webvtt_header_across_input_chunks)
{
	WebVTTHeaderTest test = {0};
	GF_WebVTTParser *parser = gf_webvtt_parser_new();
	FILE *stream = tmpfile();

	assert_true(parser != NULL);
	assert_true(stream != NULL);
	if (!parser || !stream) goto exit;

	assert_equal(gf_webvtt_parser_init(parser, &stream, 1, GF_FALSE, &test,
		webvtt_header_test_report, webvtt_header_test_sample, webvtt_header_test_capture), GF_OK, "%d");
	assert_true(webvtt_header_test_append(stream, "WEBVTT\r\n"));
	rewind(stream);
	assert_equal(gf_webvtt_parser_parse_ext(parser, stream, GF_FALSE), GF_OK, "%d");
	assert_equal(test.header_count, 0, "%u");

	assert_true(webvtt_header_test_append(stream, "X-TIMESTAMP-MAP=LOCAL:00:00:00.000,"));
	assert_equal(gf_webvtt_parser_parse_ext(parser, stream, GF_FALSE), GF_OK, "%d");
	assert_equal(test.header_count, 0, "%u");

	assert_true(webvtt_header_test_append(stream, "MPEGTS:324000000\r\n"));
	assert_equal(gf_webvtt_parser_parse_ext(parser, stream, GF_FALSE), GF_OK, "%d");
	assert_equal(test.header_count, 0, "%u");

	assert_true(webvtt_header_test_append(stream, "\r\n"));
	{
		GF_Err e = gf_webvtt_parser_parse_ext(parser, stream, GF_FALSE);
		assert_true((e == GF_OK) || (e == GF_EOS));
	}
	assert_equal(test.header_count, 1, "%u");
	assert_true(test.header && strstr(test.header, "WEBVTT"));
	assert_true(test.header && strstr(test.header, "X-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:324000000"));

exit:
	if (parser) gf_webvtt_parser_del(parser);
	if (stream) fclose(stream);
	if (test.header) gf_free(test.header);
}
#endif
