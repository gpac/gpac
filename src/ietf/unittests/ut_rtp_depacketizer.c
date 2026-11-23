#include "tests.h"
#include <gpac/ietf.h>

#ifndef GPAC_DISABLE_STREAMING

/*
 * The payload under test is the first `size` bytes of a larger buffer: bytes past the
 * payload are set by each test, so a depacketizer reading past the end of the payload
 * shows up as callbacks with ranges outside of it instead of depending on a sanitizer.
 * Callbacks pointing outside of the buffer (e.g. H.264 start codes) are not checked.
 */
typedef struct
{
	const u8 *start;
	u32 size, buf_size;
	u32 nb_cbk;
	Bool out_of_payload;
} UTRtpCtx;

static void ut_rtp_on_packet(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	UTRtpCtx *ctx = (UTRtpCtx *) udta;
	ctx->nb_cbk++;
	if ((payload >= ctx->start) && (payload < ctx->start + ctx->buf_size) && (payload + size > ctx->start + ctx->size))
		ctx->out_of_payload = GF_TRUE;
}

static void ut_rtp_process(const char *sdp, u8 pt, u8 *buf, u32 buf_size, u32 size, UTRtpCtx *ctx)
{
	GF_SDPInfo *info = gf_sdp_info_new();
	GF_RTPDepacketizer *rtpd;
	GF_RTPHeader hdr;
	u32 old_level = gf_log_get_tool_level(GF_LOG_RTP);

	memset(ctx, 0, sizeof(UTRtpCtx));
	ctx->start = buf;
	ctx->size = size;
	ctx->buf_size = buf_size;
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
	ut_rtp_process(ut_sdp_amr, 100, buf, sizeof(buf), 33, &ctx);
	assert_equal(ctx.nb_cbk, 2, "%u");
	assert_false(ctx.out_of_payload);

	//TOC entries all with F=1: the TOC must not be read past the 3-byte payload
	memset(buf, 0xbc, sizeof(buf));
	buf[0] = 0xf0;
	buf[10] = 0x3c;
	ut_rtp_process(ut_sdp_amr, 100, buf, sizeof(buf), 3, &ctx);
	assert_false(ctx.out_of_payload);

	//mode 7 TOC entry but only 5 bytes of frame: the frame must not be passed past the payload
	memset(buf, 0x11, sizeof(buf));
	buf[0] = 0xf0; buf[1] = 0x3c;
	ut_rtp_process(ut_sdp_amr, 100, buf, sizeof(buf), 7, &ctx);
	assert_false(ctx.out_of_payload);
}

static const char *ut_sdp_h264 =
	"v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=v\r\nt=0 0\r\n"
	"m=video 0 RTP/AVP 97\r\na=rtpmap:97 H264/90000\r\na=fmtp:97 profile-level-id=42e01e;packetization-mode=1\r\n";

unittest(rtp_h264_stapa_units_stay_in_payload)
{
	UTRtpCtx ctx;
	u8 buf[64];
	static const u8 good[] = {0x18, 0x00, 0x02, 0x67, 0x42, 0x00, 0x02, 0x68, 0xce};

	//two aggregated NAL units
	memset(buf, 0, sizeof(buf));
	memcpy(buf, good, sizeof(good));
	ut_rtp_process(ut_sdp_h264, 97, buf, sizeof(buf), sizeof(good), &ctx);
	assert_greater(ctx.nb_cbk, 0, "%u");
	assert_false(ctx.out_of_payload);

	//NAL size field (9) larger than what is left in the 7-byte payload
	memset(buf, 0x41, sizeof(buf));
	buf[0] = 0x18; buf[1] = 0x00; buf[2] = 0x09;
	ut_rtp_process(ut_sdp_h264, 97, buf, sizeof(buf), 7, &ctx);
	assert_false(ctx.out_of_payload);

	//a 2-byte NAL unit, then a single byte: the next size field is not in the payload
	memset(buf, 0, sizeof(buf));
	buf[0] = 0x18; buf[1] = 0x00; buf[2] = 0x02; buf[3] = 0x67; buf[4] = 0x42; buf[5] = 0x00;
	buf[6] = 0x08; buf[7] = 0x67;
	ut_rtp_process(ut_sdp_h264, 97, buf, sizeof(buf), 6, &ctx);
	assert_false(ctx.out_of_payload);
}

#endif
