#include "tests.h"
#include "../m3u8.c"

/* Check logical-line boundaries rather than merely successful parsing. */
static void check_m3u8_blob_line(u32 line_length, const char *ending, Bool comment)
{
	static const char header[] = "#EXTM3U\n#EXT-X-TARGETDURATION:1\n#EXTINF:1,\n";
	static const char next_segment[] = "\nsegment.vtt\n#EXT-X-ENDLIST\n";
	static const char segment_url[] = "segment.vtt?pad=";
	MasterPlaylist *master = NULL;
	GF_Blob blob = {0};
	char *blob_url;
	char *line;
	GF_Err e;

	blob.size = sizeof(header)-1 + line_length + (u32) strlen(ending);
	if (comment) blob.size += sizeof(next_segment)-1;
	blob.data = gf_malloc(blob.size + 1);
	assert_not_null(blob.data);
	if (!blob.data) return;
	memcpy(blob.data, header, sizeof(header)-1);
	line = (char *) blob.data + sizeof(header)-1;
	memset(line, 'A', line_length);
	if (comment) line[0] = '#';
	else memcpy(line, segment_url, sizeof(segment_url)-1);
	memcpy(line + line_length, ending, strlen(ending));
	if (comment) memcpy(line + line_length + strlen(ending), next_segment, sizeof(next_segment)-1);
	blob.data[blob.size] = 0;
	blob_url = gf_blob_register(&blob);
	assert_not_null(blob_url);
	if (!blob_url) {
		gf_free(blob.data);
		return;
	}

	e = gf_m3u8_parse_master_playlist(blob_url, &master, "http://localhost/test.m3u8");
	assert_equal(e, GF_OK, "%d");
	assert_not_null(master);
	if (!e && master) {
		Stream *stream = gf_list_get(master->streams, 0);
		PlaylistElement *variant = stream ? gf_list_get(stream->variants, 0) : NULL;
		assert_not_null(variant);
		if (variant) {
			PlaylistElement *segment = gf_list_get(variant->element.playlist.elements, 0);
			assert_equal(gf_list_count(variant->element.playlist.elements), 1, "%u");
			assert_not_null(segment);
			if (segment) {
				line[line_length] = 0;
				assert_equal_str(segment->url, comment ? "segment.vtt" : line);
				assert_equal(segment->duration_info, 1.0, "%g");
			}
		}
	}
	gf_m3u8_master_playlist_del(&master);
	gf_blob_unregister(&blob);
	gf_free(blob_url);
	gf_free(blob.data);
}

unittest(m3u8_blob_line_boundaries)
{
	/* Include the old read/write boundaries, growth, and a final line at EOF. */
	const u32 lengths[] = {M3U8_BUF_SIZE-2, M3U8_BUF_SIZE-1, M3U8_BUF_SIZE, 2*M3U8_BUF_SIZE};
	const char *endings[] = {"\n", "\r\n", ""};
	u32 i, j;
	for (i=0; i<sizeof(lengths)/sizeof(lengths[0]); i++) {
		for (j=0; j<sizeof(endings)/sizeof(endings[0]); j++)
			check_m3u8_blob_line(lengths[i], endings[j], GF_FALSE);
	}
	/* An oversized comment must not turn its continuation into a segment URL. */
	check_m3u8_blob_line(2*M3U8_BUF_SIZE, "\n", GF_TRUE);
}
