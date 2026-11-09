#include "tests.h"
#include <gpac/ietf.h>

/*
 * A NUL byte inside an a=fmtp value used to make gf_sdp_info_parse() spin forever:
 * gf_token_get() is strlen-based and stops at the NUL, so the fmtp loop kept
 * re-reading the same position and allocating an attribute on every iteration.
 *
 * The input is written as an explicit byte array because the length passed to the
 * parser must include the bytes that follow the embedded NUL.
 */
unittest(sdp_fmtp_with_nul_byte_terminates)
{
	const u8 sdp[] = "v=0\r\n"
	                 "o=- 1 1 IN IP4 127.0.0.1\r\n"
	                 "s=Stream\r\n"
	                 "t=0 0\r\n"
	                 "m=video 0 RTP/AVP 96\r\n"
	                 "a=rtpmap:96 H264/90000\r\n"
	                 "a=fmtp:96 packetization-mo\0e=1\r\n";
	GF_SDPInfo *info = gf_sdp_info_new();

	assert_not_null(info);
	/* sizeof(sdp) covers the whole array, including the bytes after the NUL. */
	assert_equal(gf_sdp_info_parse(info, (char *) sdp, (u32) sizeof(sdp) - 1), GF_OK, "%d");

	gf_sdp_info_del(info);
}

unittest(sdp_fmtp_without_nul_byte_parses)
{
	const u8 sdp[] = "v=0\r\n"
	                 "m=video 0 RTP/AVP 96\r\n"
	                 "a=fmtp:96 profile-level-id=42e01f;packetization-mode=1\r\n";
	GF_SDPInfo *info = gf_sdp_info_new();

	assert_not_null(info);
	assert_equal(gf_sdp_info_parse(info, (char *) sdp, (u32) sizeof(sdp) - 1), GF_OK, "%d");

	gf_sdp_info_del(info);
}
