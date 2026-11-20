#include "tests.h"
#include <gpac/ietf.h>

#ifndef GPAC_DISABLE_STREAMING

/*
 * The payload under test is the first `size` bytes of a larger buffer: bytes past the
 * payload are set by each test, so a depacketizer reading past the end of the payload
 * shows up as callbacks with ranges outside of it instead of depending on a sanitizer.
 */
typedef struct
{
	const u8 *start;
	u32 size;
	u32 nb_cbk;
	Bool out_of_payload;
} UTRtpCtx;

static void ut_rtp_on_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	UTRtpCtx *ctx = (UTRtpCtx *) udta;
	ctx->nb_cbk++;
	if ((payload < ctx->start) || (payload + size > ctx->start + ctx->size))
		ctx->out_of_payload = GF_TRUE;
}

static void ut_rtp_process(const char *sdp, u8 pt, u8 *buf, u32 size, UTRtpCtx *ctx)
{
	GF_SDPInfo *info = gf_sdp_info_new();
	GF_RTPDepacketizer *rtpd;
	GF_RTPHeader hdr;
	u32 old_level = gf_log_get_tool_level(GF_LOG_RTP);

	memset(ctx, 0, sizeof(UTRtpCtx));
	ctx->start = buf;
	ctx->size = size;
	gf_log_set_tool_level(GF_LOG_RTP, GF_LOG_QUIET);
	gf_sdp_info_parse(info, (char *) sdp, (u32) strlen(sdp));
	rtpd = gf_rtp_depacketizer_new((GF_SDPMedia *) gf_list_get(info->media_desc, 0), 0, ut_rtp_on_packet, ctx);
	if (rtpd) {
		memset(&hdr, 0, sizeof(GF_RTPHeader));
		hdr.Version = 2;
		hdr.PayloadType = pt;
		hdr.Marker = 1;
		hdr.TimeStamp = 160;
		gf_rtp_depacketizer_process(rtpd, &hdr, buf, size);
		gf_rtp_depacketizer_del(rtpd);
	}
	gf_sdp_info_del(info);
	gf_log_set_tool_level(GF_LOG_RTP, old_level);
}

static const char *ut_sdp_amr =
	"v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=a\r\nt=0 0\r\n"
	"m=audio 0 RTP/AVP 100\r\na=rtpmap:100 AMR/8000/1\r\na=fmtp:100 octet-align=1\r\n";

unittest(rtp_amr_toc_and_frames_stay_in_payload)
{
	UTRtpCtx ctx;
	u8 buf[64];

	//CMR, one TOC entry for mode 7 (31-byte frame) and the frame: one callback per TOC entry and frame
	buf[0] = 0xf0; buf[1] = 0x3c;
	memset(buf+2, 0x11, 31);
	ut_rtp_process(ut_sdp_amr, 100, buf, 33, &ctx);
	assert_equal(ctx.nb_cbk, 2, "%u");
	assert_false(ctx.out_of_payload);

	//TOC entries all with F=1: the TOC must not be read past the 3-byte payload
	memset(buf, 0xbc, sizeof(buf));
	buf[0] = 0xf0;
	buf[10] = 0x3c;
	ut_rtp_process(ut_sdp_amr, 100, buf, 3, &ctx);
	assert_false(ctx.out_of_payload);

	//mode 7 TOC entry but only 5 bytes of frame: the frame must not be passed past the payload
	memset(buf, 0x11, sizeof(buf));
	buf[0] = 0xf0; buf[1] = 0x3c;
	ut_rtp_process(ut_sdp_amr, 100, buf, 7, &ctx);
	assert_false(ctx.out_of_payload);
}

#endif
