/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2019
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
	GF_FilterPid *pid;
	GF_Socket *socket;
	Bool pck_out;
#ifndef GPAC_DISABLE_STREAMING
	GF_RTPReorder *rtp_reorder;
#else
	Bool is_rtp;
#endif
	char address[GF_MAX_IP_NAME_LEN];

	u64 start_time;
	u64 nb_bytes;
	Bool done;

} GF_SockInClient;

typedef struct
{
	//options
	const char *src;
	u32 block_size, sockbuf;
	u32 port, maxc;
	char *ifce;
	const char *ext;
	const char *mime;
	Bool tsprobe, listen, ka, block;
	u32 timeout;
#ifndef GPAC_DISABLE_STREAMING
	u32 reorder_pck;
	u32 reorder_delay;
#endif

	GF_SockInClient sock_c;
	GF_List *clients;
	Bool had_clients;
	Bool is_udp;

	char *buffer;

	GF_SockGroup *active_sockets;
	u64 last_rcv_time;
} GF_SockInCtx;



static GF_Err sockin_initialize(GF_Filter *filter)
{
	char *str, *url;
	u16 port;
	u32 sock_type = 0;
	GF_Err e = GF_OK;
	GF_SockInCtx *ctx = (GF_SockInCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->src) return GF_BAD_PARAM;

	ctx->active_sockets = gf_sk_group_new();
	if (!ctx->active_sockets) return GF_OUT_OF_MEM;

	if (!strnicmp(ctx->src, "udp://", 6)) {
		sock_type = GF_SOCK_TYPE_UDP;
		ctx->listen = GF_FALSE;
		ctx->is_udp = GF_TRUE;
	} else if (!strnicmp(ctx->src, "tcp://", 6)) {
		sock_type = GF_SOCK_TYPE_TCP;
#ifdef GPAC_HAS_SOCK_UN
	} else if (!strnicmp(ctx->src, "tcpu://", 7) ) {
		sock_type = GF_SOCK_TYPE_TCP_UN;
	} else if (!strnicmp(ctx->src, "udpu://", 7) ) {
		sock_type = GF_SOCK_TYPE_UDP_UN;
		ctx->listen = GF_FALSE;
#endif
	} else {
		return GF_NOT_SUPPORTED;
	}

	url = strchr(ctx->src, ':');
	url += 3;

	ctx->sock_c.socket = gf_sk_new(sock_type);
	if (! ctx->sock_c.socket ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SockIn] Failed to open socket for %s\n", ctx->src));
		return GF_IO_ERR;
	}
	gf_sk_group_register(ctx->active_sockets, ctx->sock_c.socket);

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
	if (gf_sk_is_multicast_address(url)) {
		e = gf_sk_setup_multicast(ctx->sock_c.socket, url, port, 0, 0, ctx->ifce);
		ctx->listen = GF_FALSE;
	} else if ((sock_type==GF_SOCK_TYPE_UDP) 
#ifdef GPAC_HAS_SOCK_UN 
		|| (sock_type==GF_SOCK_TYPE_UDP_UN)
#endif
		) {
		e = gf_sk_bind(ctx->sock_c.socket, ctx->ifce, port, url, port, GF_SOCK_REUSE_PORT);
		ctx->listen = GF_FALSE;
		e = gf_sk_connect(ctx->sock_c.socket, url, port, NULL);
	} else if (ctx->listen) {
		e = gf_sk_bind(ctx->sock_c.socket, NULL, port, url, 0, GF_SOCK_REUSE_PORT);
		if (!e) e = gf_sk_listen(ctx->sock_c.socket, ctx->maxc);
		if (!e) {
			gf_filter_post_process_task(filter);
			gf_sk_server_mode(ctx->sock_c.socket, GF_TRUE);
		}

	} else {
		e = gf_sk_connect(ctx->sock_c.socket, url, port, ctx->ifce);
	}

	if (str) str[0] = ':';

	if (e) {
		gf_sk_del(ctx->sock_c.socket);
		ctx->sock_c.socket = NULL;
		return e;
	}

	gf_sk_set_buffer_size(ctx->sock_c.socket, 0, ctx->sockbuf);
	gf_sk_set_block_mode(ctx->sock_c.socket, !ctx->block);


	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[SockIn] opening %s%s\n", ctx->src, ctx->listen ? " in server mode" : ""));

	if (ctx->block_size<2000)
		ctx->block_size = 2000;
	ctx->buffer = gf_malloc(ctx->block_size + 1);
	if (!ctx->buffer) return GF_OUT_OF_MEM;
	//ext/mime given and not mpeg2, disable probe
	if (ctx->ext && !strstr("ts|m2t|mts|dmb|trp", ctx->ext)) ctx->tsprobe = GF_FALSE;
	if (ctx->mime && !strstr(ctx->mime, "mpeg-2") && !strstr(ctx->mime, "mp2t")) ctx->tsprobe = GF_FALSE;

	if (ctx->listen) {
		ctx->clients = gf_list_new();
		if (!ctx->clients) return GF_OUT_OF_MEM;
	}
	return GF_OK;
}

static void sockin_client_reset(GF_SockInClient *sc)
{
	if (sc->socket) gf_sk_del(sc->socket);
#ifndef GPAC_DISABLE_STREAMING
	if (sc->rtp_reorder) gf_rtp_reorderer_del(sc->rtp_reorder);
#endif
}

static void sockin_finalize(GF_Filter *filter)
{
	GF_SockInCtx *ctx = (GF_SockInCtx *) gf_filter_get_udta(filter);

	if (ctx->clients) {
		while (gf_list_count(ctx->clients)) {
			GF_SockInClient *sc = gf_list_pop_back(ctx->clients);
			sockin_client_reset(sc);
			gf_free(sc);
		}
		gf_list_del(ctx->clients);
	}
	sockin_client_reset(&ctx->sock_c);
	if (ctx->buffer) gf_free(ctx->buffer);
	if (ctx->active_sockets) gf_sk_group_del(ctx->active_sockets);
}

static GF_FilterProbeScore sockin_probe_url(const char *url, const char *mime_type)
{
	if (!strnicmp(url, "udp://", 6)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "tcp://", 6)) return GF_FPROBE_SUPPORTED;
#ifdef GPAC_HAS_SOCK_UN
	if (!strnicmp(url, "udpu://", 7)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "tcpu://", 7)) return GF_FPROBE_SUPPORTED;
#endif
	return GF_FPROBE_NOT_SUPPORTED;
}

static void sockin_rtp_destructor(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	u32 size;
	char *data;
	GF_SockInClient *sc = (GF_SockInClient *) gf_filter_pid_get_udta(pid);
	sc->pck_out = GF_FALSE;
	data = (char *) gf_filter_pck_get_data(pck, &size);
	if (data) gf_free(data);
}

static Bool sockin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	if (!evt->base.on_pid) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		return GF_TRUE;
	case GF_FEVT_STOP:
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

static GF_Err sockin_read_client(GF_Filter *filter, GF_SockInCtx *ctx, GF_SockInClient *sock_c)
{
	u32 nb_read;
	u64 bitrate;
	GF_Err e;
	GF_FilterPacket *dst_pck;
	u8 *out_data, *in_data;

	if (!sock_c->socket)
		return GF_EOS;
	if (sock_c->pck_out)
		return GF_OK;

	if (sock_c->pid && gf_filter_pid_would_block(sock_c->pid)) {
		return GF_OK;
	}

	if (!sock_c->start_time) sock_c->start_time = gf_sys_clock_high_res();

	e = gf_sk_receive_no_select(sock_c->socket, ctx->buffer, ctx->block_size, &nb_read);
	switch (e) {
	case GF_IP_NETWORK_EMPTY:
		return GF_OK;
	case GF_OK:
		break;
	case GF_IP_CONNECTION_CLOSED:
		if (!sock_c->done) {
			sock_c->done = GF_TRUE;
			gf_filter_pid_set_eos(sock_c->pid);
		}
		return GF_EOS;
	default:
		return e;
	}
	if (!nb_read) return GF_OK;
	sock_c->nb_bytes += nb_read;
	sock_c->done = GF_FALSE;

	//we allocated one more byte for that
	ctx->buffer[nb_read] = 0;

	//first run, probe data
	if (!sock_c->pid) {
		const char *mime = ctx->mime;
		//probe MPEG-2
		if (ctx->tsprobe) {
			/*TS over RTP signaled as udp */
			if ((ctx->buffer[0] != 0x47) && ((ctx->buffer[1] & 0x7F) == 33) ) {
#ifndef GPAC_DISABLE_STREAMING
				sock_c->rtp_reorder = gf_rtp_reorderer_new(ctx->reorder_pck, ctx->reorder_delay);
#else
			ctx-	>is_rtp = GF_TRUE;
#endif
				mime = "video/mp2t";
			} else if (ctx->buffer[0] == 0x47) {
				mime = "video/mp2t";
			}
		}

		e = gf_filter_pid_raw_new(filter, ctx->src, NULL, mime, ctx->ext, ctx->buffer, nb_read, GF_TRUE, &sock_c->pid);
		if (e) return e;
		if (!mime) {
			const GF_PropertyValue *p = gf_filter_pid_get_property(sock_c->pid, GF_PROP_PID_MIME);
			if (p) mime = p->value.string;
		}
//		if (ctx->is_udp) gf_filter_pid_set_property(sock_c->pid, GF_PROP_PID_UDP, &PROP_BOOL(GF_TRUE) );


		gf_filter_pid_set_udta(sock_c->pid, sock_c);
	}

	in_data = ctx->buffer;

#ifndef GPAC_DISABLE_STREAMING
	if (sock_c->rtp_reorder) {
		char *pck;
		u16 seq_num = ((ctx->buffer[2] << 8) & 0xFF00) | (ctx->buffer[3] & 0xFF);
		gf_rtp_reorderer_add(sock_c->rtp_reorder, (void *) ctx->buffer, nb_read, seq_num);

		pck = (char *) gf_rtp_reorderer_get(sock_c->rtp_reorder, &nb_read, GF_FALSE);
		if (pck) {
			dst_pck = gf_filter_pck_new_shared(sock_c->pid, pck+12, nb_read-12, sockin_rtp_destructor);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
			gf_filter_pck_send(dst_pck);
		}
		return GF_OK;
	}
#else
	if (sock_c->is_rtp) {
		in_data = ctx->buffer + 12;
		nb_read -= 12;
	}
#endif

	dst_pck = gf_filter_pck_new_alloc(sock_c->pid, nb_read, &out_data);
	memcpy(out_data, in_data, nb_read);

	gf_filter_pck_set_framing(dst_pck, (sock_c->nb_bytes == nb_read)  ? GF_TRUE : GF_FALSE, GF_FALSE);
	gf_filter_pck_send(dst_pck);

	//send bitrate
	bitrate = ( gf_sys_clock_high_res() - sock_c->start_time );
	if (bitrate) {
		bitrate = (sock_c->nb_bytes * 8 * 1000000) / bitrate;
		gf_filter_pid_set_property(sock_c->pid, GF_PROP_PID_DOWN_RATE, &PROP_UINT((u32) bitrate) );
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[SockIn] Receiving from %s at %d kbps\r", sock_c->address, (u32) (bitrate/10)));
	}

	return GF_OK;
}

static Bool sockin_check_eos(GF_SockInCtx *ctx)
{
	u64 now = gf_sys_clock_high_res();
	if (!ctx->last_rcv_time) {
		ctx->last_rcv_time = now;
		return GF_FALSE;
	}
	if (now - ctx->last_rcv_time < ctx->timeout*1000) {
		return GF_FALSE;
	}
	if (ctx->sock_c.pid && !ctx->sock_c.done) {
		gf_filter_pid_set_eos(ctx->sock_c.pid);
		ctx->sock_c.done = GF_TRUE;
	}
	return GF_TRUE;
}

static GF_Err sockin_process(GF_Filter *filter)
{
	GF_Socket *new_conn=NULL;
	GF_Err e;
	u32 i, count;
	GF_SockInCtx *ctx = (GF_SockInCtx *) gf_filter_get_udta(filter);

	e = gf_sk_group_select(ctx->active_sockets, 10);
	if (e==GF_IP_NETWORK_EMPTY) {
		if (ctx->is_udp) {
			if (sockin_check_eos(ctx) )
				return GF_EOS;
		} else if (!gf_list_count(ctx->clients)) {
			gf_filter_ask_rt_reschedule(filter, 1000);
			return GF_OK;
		}

		gf_filter_ask_rt_reschedule(filter, 1000);
		return GF_OK;
	}
	else if (e) return e;

	if (gf_sk_group_sock_is_set(ctx->active_sockets, ctx->sock_c.socket)) {
		if (!ctx->listen) {
			return sockin_read_client(filter, ctx, &ctx->sock_c);
		}

		if (gf_sk_group_sock_is_set(ctx->active_sockets, ctx->sock_c.socket)) {
			e = gf_sk_accept(ctx->sock_c.socket, &new_conn);
			if ((e==GF_OK) && new_conn) {
				GF_SockInClient *sc;
				GF_SAFEALLOC(sc, GF_SockInClient);
				sc->socket = new_conn;
				strcpy(sc->address, "unknown");
				gf_sk_get_remote_address(new_conn, sc->address);
				gf_sk_set_block_mode(new_conn, !ctx->block);

				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[SockIn] Accepting new connection from %s\n", sc->address));
				gf_list_add(ctx->clients, sc);
				ctx->had_clients = GF_TRUE;
				gf_sk_group_register(ctx->active_sockets, sc->socket);
			}
		}
	}
	if (!ctx->listen) return GF_OK;

	count = gf_list_count(ctx->clients);
	for (i=0; i<count; i++) {
		GF_SockInClient *sc = gf_list_get(ctx->clients, i);

		if (!gf_sk_group_sock_is_set(ctx->active_sockets, sc->socket)) continue;

	 	e = sockin_read_client(filter, ctx, sc);
	 	if (e == GF_IP_CONNECTION_CLOSED) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[SockIn] Connection to %s lost, removing input\n", sc->address));
			if (sc->socket)
				gf_sk_group_unregister(ctx->active_sockets, sc->socket);

	 		sockin_client_reset(sc);
	 		if (sc->pid) {
	 			gf_filter_pid_set_eos(sc->pid);
	 			gf_filter_pid_remove(sc->pid);
	 		}
	 		gf_free(sc);
	 		gf_list_del_item(ctx->clients, sc);
	 		i--;
	 		count--;
		} else {
			if (e) return e;
		}
	}
	if (!ctx->had_clients) {
		//we should use socket groups and selects !
		gf_filter_ask_rt_reschedule(filter, 100000);
		return GF_OK;
	}

	if (!count) {
		if (ctx->ka) {
			//keep alive, ask for real-time reschedule of 100 ms - we should use socket groups and selects !
			gf_filter_ask_rt_reschedule(filter, 100000);
		} else {
			return GF_EOS;
		}
	}
	return GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_SockInCtx, _n)

static const GF_FilterArgs SockInArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read socket", GF_PROP_UINT, "10000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sockbuf), "socket max buffer size", GF_PROP_UINT, "65536", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(port), "default port if not specified", GF_PROP_UINT, "1234", NULL, 0},
	{ OFFS(ifce), "default multicast interface", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(listen), "indicate the input socket works in server mode", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(ka), "keep socket alive if no more connections", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(maxc), "max number of concurrent connections", GF_PROP_UINT, "+I", NULL, 0},
	{ OFFS(tsprobe), "probe for MPEG-2 TS data, either RTP or raw UDP. Disabled if mime or ext are given and do not match MPEG-2 TS mimes/extensions", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ext), "indicate file extension of udp data", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(mime), "indicate mime type of udp data", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(block), "set blocking mode for socket(s)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timeout), "set timeout in ms for UDP socket(s)", GF_PROP_UINT, "5000", NULL, GF_FS_ARG_HINT_ADVANCED},

#ifndef GPAC_DISABLE_STREAMING
	{ OFFS(reorder_pck), "number of packets delay for RTP reordering (M2TS over RTP) ", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(reorder_delay), "number of ms delay for RTP reordering (M2TS over RTP)", GF_PROP_UINT, "10", NULL, GF_FS_ARG_HINT_ADVANCED},
#endif
	{0}
};

static const GF_FilterCapability SockInCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister SockInRegister = {
	.name = "sockin",
	GF_FS_SET_DESCRIPTION("UDP/TCP input")
#ifndef GPAC_DISABLE_DOC
	.help = "This filter handles generic TCP and UDP input sockets. It can also probe for MPEG-2 TS over RTP input. Probing of MPEG-2 TS over UDP/RTP is enabled by default but can be turned off.\n"
		"\nFormat of data can be specified by setting either [-ext]() or [-mime]() options. If not set, the format will be guessed by probing the first data packet\n"
		"\n"
		"UDP sockets are used for source URLs formatted as udp://NAME\n"
		"TCP sockets are used for source URLs formatted as tcp://NAME\n"
#ifdef GPAC_HAS_SOCK_UN
		"UDP unix domain sockets are used for source URLs formatted as udpu://NAME\n"
		"TCP unix domain sockets are used for source URLs formatted as tcpu://NAME\n"
#ifdef GPAC_CONFIG_DARWIN
	"\nOn OSX with VM packet replay you will need to force multicast routing, eg: route add -net 239.255.1.4/32 -interface vboxnet0"
#endif
	""
#else
	"Your platform does not supports unix domain sockets, udpu:// and tcpu:// schemes not supported."
#endif
	,
#endif //GPAC_DISABLE_DOC
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

