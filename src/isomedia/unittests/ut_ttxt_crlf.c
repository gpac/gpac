#include "tests.h"
#include <gpac/isomedia.h>

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_ISOM_DUMP)

static void check_ttxt_crlf(u32 crlf_count, GF_TextDumpType dump_type)
{
	GF_ISOFile *movie = NULL;
	GF_TextSample *text_sample = NULL;
	GF_ISOSample *sample = NULL;
	GF_TextSampleDescriptor description = {0};
	GF_FontRecord font = {0};
	GF_StyleRecord style = {0};
	FILE *output = NULL;
	u32 track, description_index, i;
	char text[256], normalized[256], dumped[4096], expected_style[128];
	size_t bytes_read;
	GF_Err e;

	/* Each CRLF becomes one newline. Keep a visible character after every pair. */
	for (i=0; i<crlf_count; i++) {
		memcpy(text + 3*i, "A\r\n", 3);
		memcpy(normalized + 2*i, "A\n", 2);
	}
	text[3*crlf_count] = 'Z';
	normalized[2*crlf_count] = 'Z';
	normalized[2*crlf_count+1] = 0;

	/* WRITE_EDIT stores samples in a temporary file; delete without saving. */
	movie = gf_isom_open("ttxt-crlf.mp4", GF_ISOM_WRITE_EDIT, NULL);
	assert_not_null(movie);
	if (!movie) goto exit;
	track = gf_isom_new_track(movie, 1, GF_ISOM_MEDIA_TEXT, 1000);
	assert_equal(track, 1, "%u");
	if (!track) goto exit;
	description.tag = GF_ODF_TEXT_CFG_TAG;
	font.fontID = 1;
	font.fontName = "Serif";
	description.font_count = 1;
	description.fonts = &font;
	description.default_style.fontID = 1;
	description.default_style.font_size = 18;
	e = gf_isom_new_text_description(movie, track, &description, NULL, NULL, &description_index);
	assert_equal(e, GF_OK, "%d");
	if (e) goto exit;
	text_sample = gf_isom_new_text_sample();
	assert_not_null(text_sample);
	if (!text_sample) goto exit;
	e = gf_isom_text_add_text(text_sample, text, 3*crlf_count+1);
	assert_equal(e, GF_OK, "%d");
	if (e) goto exit;
	style.startCharOffset = 3*crlf_count;
	style.endCharOffset = style.startCharOffset+1;
	style.style_flags = 1;
	style.fontID = 1;
	style.font_size = 18;
	e = gf_isom_text_add_style(text_sample, &style);
	assert_equal(e, GF_OK, "%d");
	if (e) goto exit;
	e = gf_isom_text_add_highlight(text_sample, style.startCharOffset, style.endCharOffset);
	assert_equal(e, GF_OK, "%d");
	if (e) goto exit;
	sample = gf_isom_text_to_sample(text_sample);
	assert_not_null(sample);
	if (!sample) goto exit;
	e = gf_isom_add_sample(movie, track, description_index, sample);
	assert_equal(e, GF_OK, "%d");
	if (e) goto exit;
	output = tmpfile();
	assert_not_null(output);
	if (!output) goto exit;

	e = gf_isom_text_dump(movie, track, output, dump_type);
	assert_equal(e, GF_OK, "%d");
	rewind(output);
	bytes_read = fread(dumped, 1, sizeof(dumped)-1, output);
	dumped[bytes_read] = 0;
	assert_true(strstr(dumped, normalized) != NULL);
	assert_true(strstr(dumped, "</TextSample>") != NULL);
	if (dump_type == GF_TEXTDUMPTYPE_TTXT) {
		/* Preserve the current exporter adjustment for style and highlight offsets. */
		u32 adjustment = crlf_count ? 1 : 0;
		snprintf(expected_style, sizeof(expected_style), "<Style fromChar=\"%u\" toChar=\"%u\" styles=\"Bold ", style.startCharOffset-adjustment, style.endCharOffset-adjustment);
		assert_true(strstr(dumped, expected_style) != NULL);
		snprintf(expected_style, sizeof(expected_style), "<Highlight fromChar=\"%u\" toChar=\"%u\"", style.startCharOffset-adjustment, style.endCharOffset-adjustment);
		assert_true(strstr(dumped, expected_style) != NULL);
	}

exit:
	if (output) fclose(output);
	if (sample) gf_isom_sample_del(&sample);
	if (text_sample) gf_isom_delete_text_sample(text_sample);
	if (movie) gf_isom_delete(movie);
}

#endif

unittest(ttxt_crlf_control)
{
#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_ISOM_DUMP)
	check_ttxt_crlf(0, GF_TEXTDUMPTYPE_TTXT);
	check_ttxt_crlf(1, GF_TEXTDUMPTYPE_TTXT);
	check_ttxt_crlf(20, GF_TEXTDUMPTYPE_TTXT);
#endif
}

unittest(ttxt_crlf_over_twenty)
{
#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_ISOM_DUMP)
	check_ttxt_crlf(21, GF_TEXTDUMPTYPE_TTXT);
	check_ttxt_crlf(79, GF_TEXTDUMPTYPE_TTXT);
#endif
}

unittest(ttxt_crlf_box_dump)
{
#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_ISOM_DUMP)
	check_ttxt_crlf(79, GF_TEXTDUMPTYPE_TTXT_BOXES);
#endif
}
