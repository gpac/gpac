#include "tests.h"
#include <string.h>
#include <gpac/mpegts.h>

void gf_m2ts_pes_header(GF_M2TS_PES *pes, unsigned char *data, u32 data_size, GF_M2TS_PESHeader *pesh);

/* PES header starting at stream_id (as passed by gf_m2ts_flush_pes):
 * stream_id, PES_packet_length (2), flags (2), PES_header_data_length,
 * then PTS (5 bytes, 90000) and DTS (5 bytes, 45000). */
static const u8 pes_hdr_pts_dts[] = {
	0xE0, 0x00, 0x00, 0x80, 0xC0, 0x0A,
	0x31, 0x00, 0x05, 0xBF, 0x21,
	0x11, 0x00, 0x03, 0x5F, 0x91
};

unittest(m2ts_pes_header_pts_dts)
{
	GF_M2TS_PES pes = {0};
	GF_M2TS_PESHeader pesh;
	u8 buf[sizeof(pes_hdr_pts_dts)];
	memcpy(buf, pes_hdr_pts_dts, sizeof(buf));

	gf_m2ts_pes_header(&pes, buf, sizeof(buf), &pesh);
	assert_true(pesh.has_pts);
	assert_equal(pesh.PTS, (u64) 90000, LLU);
	assert_equal(pesh.DTS, (u64) 45000, LLU);
}

unittest(m2ts_pes_header_truncated_pts)
{
	GF_M2TS_PES pes = {0};
	GF_M2TS_PESHeader pesh;
	u8 buf[sizeof(pes_hdr_pts_dts)];
	u32 old_level = gf_log_get_tool_level(GF_LOG_CONTAINER);
	memcpy(buf, pes_hdr_pts_dts, sizeof(buf));
	gf_log_set_tool_level(GF_LOG_CONTAINER, GF_LOG_QUIET);

	/* the PTS flag is set but only 2 of its 5 bytes are in the PES data:
	   nothing past data_size may be read and no PTS/DTS may be reported */
	gf_m2ts_pes_header(&pes, buf, 8, &pesh);
	assert_false(pesh.has_pts);
	assert_equal(pesh.PTS, (u64) 0, LLU);
	assert_equal(pesh.DTS, (u64) 0, LLU);

	/* PTS complete, DTS flag set but DTS missing: keep the PTS, DTS falls back to it */
	gf_m2ts_pes_header(&pes, buf, 13, &pesh);
	assert_true(pesh.has_pts);
	assert_equal(pesh.PTS, (u64) 90000, LLU);
	assert_equal(pesh.DTS, (u64) 90000, LLU);

	gf_log_set_tool_level(GF_LOG_CONTAINER, old_level);
}
