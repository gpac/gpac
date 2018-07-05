/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / generic TCP/UDP input filter
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/network.h>

#ifndef GPAC_DISABLE_STREAMING
#include <gpac/internal/ietf_dev.h>
#endif

typedef struct
{
	//options
	const char *src;
	u32 block_size;
	u32 port;
	char *mcast_ifce;
	const char *ext;
	const char *mime;
	Bool np;
#ifndef GPAC_DISABLE_STREAMING
	u32 reorder_pck;
	u32 reorder_delay;
#endif

	//only one output pid declared
	GF_FilterPid *pid;
	GF_Socket *socket;
	Bool pck_out;

	char *block;
	u32 bsize;

#ifndef GPAC_DISABLE_STREAMING
	GF_RTPReorder *rtp_reorder;
#else
	Bool is_rtp;
#endif

	Bool is_gsf;
} GF_SockInCtx;



static GF_Err sockin_setup_socket(GF_Filter *filter, GF_SockInCtx *ctx)
{
	GF_Err e = GF_OK;
	char *str, *url;
	u16 port;
	u32 sock_type = 0;

	if (!ctx || !ctx->src) return GF_BAD_PARAM;
	if (ctx->socket) return GF_OK;

	if (!strnicmp(ctx->src, "udp://", 6) || !strnicmp(ctx->src, "mpegts-udp://", 13)) {
		sock_type = GF_SOCK_TYPE_UDP;
	} else if (!strnicmp(ctx->src, "tcp://", 6) || !strnicmp(ctx->src, "mpegts-tcp://", 13) ) {
		sock_type = GF_SOCK_TYPE_TCP;
	} else {
		return GF_NOT_SUPPORTED;
	}

	url = strchr(ctx->src, ':');
	url += 3;

	ctx->socket = gf_sk_new(sock_type);
	if (! ctx->socket ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[SockIn] Failed to open socket for %s\n", ctx->src));
		return GF_IO_ERR;
	}

	/*setup port and src*/
	port = ctx->port;
	str = strrchr(url, ':');
	/*take care of IPv6 address*/
	if (str && strchr(str, ']')) str = strchr(url, ':');
	if (str) {
		port = atoi(str+1);
		str[0] = 0;
	}

	/*do we have a source ?*/
	if (strlen(url) && strcmp(url, "localhost") ) {
		if (gf_sk_is_multicast_address(url)) {
			e = gf_sk_setup_multicast(ctx->socket, url, port, 0, 0, ctx->mcast_ifce);
		} else if (sock_type==GF_SOCK_TYPE_UDP) {
			e = gf_sk_bind(ctx->socket, ctx->mcast_ifce, port, url, 0, GF_SOCK_REUSE_PORT);
		} else {
			e = gf_sk_connect(ctx->socket, url, port, ctx->mcast_ifce);
		}
		
		if (e) {
			gf_sk_del(ctx->socket);
			ctx->socket = NULL;
			return e;
		}
	}
	if (str) str[0] = ':';

	gf_sk_set_buffer_size(ctx->socket, 0, ctx->block_size);
	gf_sk_set_block_mode(ctx->socket, 0);
	return e;
}

static GF_Err sockin_initialize(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_SockInCtx *ctx = (GF_SockInCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->src) return GF_BAD_PARAM;
	e = sockin_setup_socket(filter, ctx);

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[SockIn] Failed to open %s\n", ctx->src));
		gf_filter_setup_failure(filter, e);
		return GF_URL_ERROR;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("[SockIn] opening %s\n", ctx->src));

	ctx->bsize = 10000;
	ctx->block = gf_malloc(ctx->bsize+1);
	if (ctx->ext || ctx->mime) ctx->np = GF_TRUE;
	return GF_OK;
}

static void sockin_finalize(GF_Filter *filter)
{
	GF_SockInCtx *ctx = (GF_SockInCtx *) gf_filter_get_udta(filter);
	if (ctx->socket) gf_sk_del(ctx->socket);
#ifndef GPAC_DISABLE_STREAMING
	if (ctx->rtp_reorder) gf_rtp_reorderer_del(ctx->rtp_reorder);
#endif
	if (ctx->block) gf_free(ctx->block);
}

static GF_FilterProbeScore sockin_probe_url(const char *url, const char *mime_type)
{
	if (!strnicmp(url, "udp://", 6)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "tcp://", 6)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "mpegts-udp://", 13)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "mpegts-tcp://", 13)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static void sockin_rtp_destructor(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	u32 size;
	char *data;
	GF_SockInCtx *ctx = (GF_SockInCtx *) gf_filter_get_udta(filter);
	ctx->pck_out = GF_FALSE;
	data = (char *) gf_filter_pck_get_data(pck, &size);
	if (data) gf_free(data);
}

static Bool sockin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_SockInCtx *ctx = (GF_SockInCtx *) gf_filter_get_udta(filter);

	if (!evt->base.on_pid) return GF_FALSE;
	if (evt->base.on_pid != ctx->pid) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		sockin_setup_socket(filter, ctx);
		return GF_TRUE;
	case GF_FEVT_STOP:
		gf_sk_del(ctx->socket);
		ctx->socket = NULL;
#ifndef GPAC_DISABLE_STREAMING
		gf_rtp_reorderer_del(ctx->rtp_reorder);
		ctx->rtp_reorder = NULL;
#endif
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

static GF_Err sockin_process(GF_Filter *filter)
{
	u32 nb_read;
	GF_Err e;
	GF_FilterPacket *dst_pck;
	char *out_data, *in_data;
	GF_SockInCtx *ctx = (GF_SockInCtx *) gf_filter_get_udta(filter);

	if (!ctx->socket)
		return GF_EOS;
	if (ctx->pck_out)
		return GF_OK;

	if (ctx->pid && gf_filter_pid_would_block(ctx->pid)) {
		assert(0);
		return GF_OK;
	}

	e = gf_sk_receive(ctx->socket, ctx->block, ctx->bsize, 0, &nb_read);
	if (!nb_read || e)
		return GF_OK;

	//should not happen since we start with 10k read buffer !
	if (nb_read>=ctx->bsize) {
		u32 offset = nb_read;
		while (1) {
			ctx->bsize *= 2;
			ctx->block = gf_realloc(ctx->block, ctx->bsize+1);
			e = gf_sk_receive(ctx->socket, ctx->block, ctx->bsize, offset, &nb_read);
			if (e || !nb_read) break;
			offset += nb_read;
			if (offset < ctx->bsize) break;
			//keep reallocating
		}
		nb_read = offset;
	}
	//we allocated one more byte for that
	ctx->block[nb_read] = 0;

	//first run, probe data
	if (!ctx->pid) {
		const char *mime = ctx->mime;
		//probe MPEG-2
		if (!ctx->np) {
			/*TS over RTP signaled as udp */
			if ((ctx->block[0] != 0x47) && ((ctx->block[1] & 0x7F) == 33) ) {
#ifndef GPAC_DISABLE_STREAMING
				ctx->rtp_reorder = gf_rtp_reorderer_new(ctx->reorder_pck, ctx->reorder_delay);
#else
			ctx-	>is_rtp = GF_TRUE;
#endif
				mime = "video/mp2t";
			} else if (ctx->block[0] == 0x47) {
				mime = "video/mp2t";
			}
		}

		ctx->pid = gf_filter_pid_raw_new(filter, ctx->src, ctx->src, mime, ctx->ext, ctx->block, nb_read, &e);
		if (e) return e;
		if (!mime) {
			const GF_PropertyValue *p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_MIME);
			if (p) mime = p->value.string;
		}
	}

	in_data = ctx->block;

#ifndef GPAC_DISABLE_STREAMING
	if (ctx->rtp_reorder) {
		char *pck;
		u16 seq_num = ((ctx->block[2] << 8) & 0xFF00) | (ctx->block[3] & 0xFF);
		gf_rtp_reorderer_add(ctx->rtp_reorder, (void *) ctx->block, nb_read, seq_num);

		pck = (char *) gf_rtp_reorderer_get(ctx->rtp_reorder, &nb_read);
		if (pck) {
			dst_pck = gf_filter_pck_new_shared(ctx->pid, pck+12, nb_read-12, sockin_rtp_destructor);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
			gf_filter_pck_send(dst_pck);
		}
		return GF_OK;
	}
#else
	if (ctx->is_rtp) {
		in_data = ctx->block + 12;
		nb_read -= 12;
	}
#endif

	dst_pck = gf_filter_pck_new_alloc(ctx->pid, nb_read, &out_data);
	memcpy(out_data, in_data, nb_read);
	gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_send(dst_pck);
	return GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_SockInCtx, _n)

static const GF_FilterArgs SockInArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(block_size), "block size used to read file", GF_PROP_UINT, "65536", NULL, GF_FALSE},
	{ OFFS(port), "default port if not specified", GF_PROP_UINT, "1234", NULL, GF_FALSE},
	{ OFFS(mcast_ifce), "default multicast interface", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(np), "disables TS probe on input data. Implied if mime or ext are given", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(ext), "indicates file extension of udp data", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(mime), "indicates mime type of udp data", GF_PROP_STRING, NULL, NULL, GF_FALSE},
#ifndef GPAC_DISABLE_STREAMING
	{ OFFS(reorder_pck), "number of packets delay for RTP reordering", GF_PROP_UINT, "100", NULL, GF_FALSE},
	{ OFFS(reorder_delay), "number of ms delay for RTP reordering", GF_PROP_UINT, "10", NULL, GF_FALSE},
#endif
	{0}
};

static const GF_FilterCapability SockInCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
};

GF_FilterRegister SockInRegister = {
	.name = "sockin",
	.description = "TCP/UDP socket input",
	.comment = "This filter handles generic TCP and UDP input sockets. It can also probe for MPEG-2 TS over RTP input. Probing of MPEG-2 TS over UDP/RTP is enabled by default but can be turned off.\n"\
	"\nFormat of data can be specified by setting either ext or mime option. If not set, the format will be guessed by probing the first data packet",
	.private_size = sizeof(GF_SockInCtx),
	.args = SockInArgs,
	SETCAPS(SockInCaps),
	.initialize = sockin_initialize,
	.finalize = sockin_finalize,
	.process = sockin_process,
	.process_event = sockin_process_event,
	.probe_url = sockin_probe_url
};


const GF_FilterRegister *sockin_register(GF_FilterSession *session)
{
	return &SockInRegister;
}

