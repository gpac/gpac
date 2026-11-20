#ifndef UT_ROUTE_HELPERS_H
#define UT_ROUTE_HELPERS_H

#include <gpac/route.h>
#include <gpac/network.h>
#include <string.h>

/*
 * Feeds hand-built LCT packets to a non-ATSC ROUTE demuxer listening on 127.0.0.1,
 * through a local UDP socket. The service signaling (S-TSID) declares TSI 10 with the
 * file template a_$TOI$.m4s. Objects are delivered synchronously: each packet is
 * followed by a call to gf_route_dmx_process().
 */
typedef struct
{
	GF_ROUTEDmx *dmx;
	GF_Socket *peer;
	Bool quiet;
	u32 nb_segs, nb_deleted;
	//TOI, total size and blob size of the first 16 dispatched segments
	u32 seg_toi[16], seg_total_size[16], seg_blob_size[16];
	u32 log_level;
} UTRouteCtx;

static void ut_route_on_event(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo)
{
	UTRouteCtx *ctx = (UTRouteCtx *) udta;
	if (ctx->quiet || !finfo) return;
	if (evt == GF_ROUTE_EVT_DYN_SEG) {
		if (ctx->nb_segs < 16) {
			ctx->seg_toi[ctx->nb_segs] = finfo->toi;
			ctx->seg_total_size[ctx->nb_segs] = finfo->total_size;
			ctx->seg_blob_size[ctx->nb_segs] = finfo->blob ? finfo->blob->size : 0;
		}
		ctx->nb_segs++;
	} else if (evt == GF_ROUTE_EVT_FILE_DELETE) {
		ctx->nb_deleted++;
	}
}

static void ut_route_put32(u8 *b, u32 v)
{
	b[0] = (u8) (v >> 24);
	b[1] = (u8) (v >> 16);
	b[2] = (u8) (v >> 8);
	b[3] = (u8) v;
}

/* one LCT packet: V=1, PSI=2, S=1, O=01, H=0, B=close_flag, optional TOL24 extension, then the start offset */
static void ut_route_send(UTRouteCtx *ctx, Bool close_flag, u32 tsi, u32 toi, u32 tol, u32 offset, const char *data, u32 size)
{
	u8 b[2048];
	u32 i = 0;
	b[i++] = 0x12;
	b[i++] = 0xA0 | (close_flag ? 1 : 0);
	b[i++] = tol ? 5 : 4;
	b[i++] = tsi ? 128 : 0;
	ut_route_put32(b+i, 0); i += 4;
	ut_route_put32(b+i, tsi); i += 4;
	ut_route_put32(b+i, toi); i += 4;
	if (tol) {
		b[i++] = 194;
		b[i++] = (u8) (tol >> 16);
		b[i++] = (u8) (tol >> 8);
		b[i++] = (u8) tol;
	}
	ut_route_put32(b+i, offset); i += 4;
	memcpy(b+i, data, size);
	gf_sk_send(ctx->peer, b, i + size);
	gf_route_dmx_process(ctx->dmx);
}

/* a complete object in one packet, with TOL */
static void ut_route_send_object(UTRouteCtx *ctx, u32 toi, const char *text)
{
	ut_route_send(ctx, GF_TRUE, 10, toi, (u32) strlen(text), 0, text, (u32) strlen(text));
}

static Bool ut_route_open(UTRouteCtx *ctx)
{
	const char *sls =
		"Content-Type: multipart/related; type=\"application/mbms-envelope+xml\"; boundary=\"BOUNDARY\"\r\n\r\n"
		"--BOUNDARY\r\nContent-Type: application/route-s-tsid+xml\r\nContent-Location: stsid.xml\r\n\r\n"
		"<S-TSID><RS><LS tsi=\"10\"><SrcFlow rt=\"true\"><EFDT><FDT-Instance fileTemplate=\"a_$TOI$.m4s\"><File Content-Location=\"a_init.mp4\" TOI=\"1000\"/>"
		"</FDT-Instance></EFDT><Payload codePoint=\"128\" formatId=\"1\" frag=\"0\" order=\"true\"/></SrcFlow></LS></RS></S-TSID>\r\n"
		"--BOUNDARY\r\nContent-Type: application/dash+xml\r\nContent-Location: manifest.mpd\r\n\r\n<MPD id=\"m1\"></MPD>\r\n--BOUNDARY--\r\n";
	//an unusual loopback port, the demuxer binds it with port reuse
	u16 port = 30000 + (u16) (gf_sys_clock_high_res() % 20000);

	memset(ctx, 0, sizeof(UTRouteCtx));
	ctx->log_level = gf_log_get_tool_level(GF_LOG_ROUTE);
	gf_log_set_tool_level(GF_LOG_ROUTE, GF_LOG_QUIET);
	ctx->dmx = gf_route_dmx_new("127.0.0.1", port, NULL, 0, ut_route_on_event, ctx);
	if (!ctx->dmx) return GF_FALSE;
	gf_route_dmx_set_reorder(ctx->dmx, GF_FALSE, 0);
	ctx->peer = gf_sk_new(GF_SOCK_TYPE_UDP);
	if (!ctx->peer || gf_sk_bind(ctx->peer, NULL, port, "127.0.0.1", port, GF_SOCK_REUSE_PORT|GF_SOCK_IS_SENDER)) return GF_FALSE;

	//service signaling: TSI 0, TOI with the S-TSID and MPD bits
	ctx->quiet = GF_TRUE;
	ut_route_send(ctx, GF_TRUE, 0, (1<<17) | (1<<18) | 1, (u32) strlen(sls), 0, sls, (u32) strlen(sls));
	ctx->quiet = GF_FALSE;
	return GF_TRUE;
}

static void ut_route_close(UTRouteCtx *ctx)
{
	ctx->quiet = GF_TRUE;
	if (ctx->peer) gf_sk_del(ctx->peer);
	if (ctx->dmx) gf_route_dmx_del(ctx->dmx);
	gf_log_set_tool_level(GF_LOG_ROUTE, ctx->log_level);
}

#endif
