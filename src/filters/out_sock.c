/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019
 *					All rights reserved
 *
 *  This file is part of GPAC / generic socket output filter
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

typedef struct
{
	GF_Socket *socket;
	Bool is_tuned;
	char address[GF_MAX_IP_NAME_LEN];
	Bool pck_pending;
} GF_SockOutClient;

typedef struct
{
	//options
	Double start, speed;
	char *dst, *mime, *ext, *ifce;
	Bool listen;
	u32 maxc, port, sockbuf, ka, kp, rate;
	GF_Fraction pckr, pckd;

	GF_Socket *socket;
	//only one output pid
	GF_FilterPid *pid;

	GF_List *clients;

	Bool pid_started;
	Bool had_clients;
	Bool pck_pending;

	GF_FilterCapability in_caps[2];
	char szExt[10];
	char szFileName[GF_MAX_PATH];

	u32 nb_pck_processed;
	u64 start_time;
	u64 nb_bytes_sent;

	GF_FilterPacket *rev_pck;
	u32 next_pckd_idx, next_pckr_idx;
	u32 nb_pckd_wnd, nb_pckr_wnd;
} GF_SockOutCtx;


static GF_Err sockout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_SockOutCtx *ctx = (GF_SockOutCtx *) gf_filter_get_udta(filter);
	if (is_remove) {
		ctx->pid = NULL;
		gf_sk_del(ctx->socket);
		ctx->socket = NULL;
		return GF_OK;
	}
	gf_filter_pid_check_caps(pid);

	if (!ctx->pid && (!ctx->listen || gf_list_count(ctx->clients) ) ) {
		GF_FilterEvent evt;
		gf_filter_pid_init_play_event(pid, &evt, ctx->start, ctx->speed, "SockOut");
		gf_filter_pid_send_event(pid, &evt);
		ctx->pid_started = GF_TRUE;
	}
	ctx->pid = pid;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLE_PROGRESSIVE);
	if (p && p->value.boolean) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SockOut] Block patching is not supported by socket output\n"));
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

static GF_Err sockout_initialize(GF_Filter *filter)
{
	GF_Err e;
	char *str, *url;
	u16 port;
	char *ext;
	u32 sock_type = 0;
	GF_SockOutCtx *ctx = (GF_SockOutCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_OK;
	e = GF_NOT_SUPPORTED;
	if (!strncmp(ctx->dst, "tcp://", 6)) e = GF_OK;
	else if (!strncmp(ctx->dst, "udp://", 6)) e = GF_OK;
	else if (!strncmp(ctx->dst, "tcpu://", 7)) e = GF_OK;
	else if (!strncmp(ctx->dst, "udpu://", 7)) e = GF_OK;


	if (e)  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	if (ctx->ext) ext = ctx->ext;
	else {
		ext = strrchr(ctx->dst, '.');
		if (ext) ext++;
	}

	if (!ext && !ctx->mime) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SockOut] No extension provided nor mime type for output file %s, cannot infer format\n", ctx->dst));
		return GF_NOT_SUPPORTED;
	}

	if (ctx->listen) {
		ctx->clients = gf_list_new();
		if (!ctx->clients) return GF_OUT_OF_MEM;
	}

	//static cap, streamtype = file
	ctx->in_caps[0].code = GF_PROP_PID_STREAM_TYPE;
	ctx->in_caps[0].val = PROP_UINT(GF_STREAM_FILE);
	ctx->in_caps[0].flags = GF_CAPS_INPUT_STATIC;

	if (ctx->mime) {
		ctx->in_caps[1].code = GF_PROP_PID_MIME;
		ctx->in_caps[1].val = PROP_NAME( ctx->mime );
		ctx->in_caps[1].flags = GF_CAPS_INPUT;
	} else {
		strncpy(ctx->szExt, ext, 9);
		strlwr(ctx->szExt);
		ctx->in_caps[1].code = GF_PROP_PID_FILE_EXT;
		ctx->in_caps[1].val = PROP_NAME( ctx->szExt );
		ctx->in_caps[1].flags = GF_CAPS_INPUT;
	}
	gf_filter_override_caps(filter, ctx->in_caps, 2);

	/*create our ourput socket*/

	if (!strnicmp(ctx->dst, "udp://", 6)) {
		sock_type = GF_SOCK_TYPE_UDP;
		ctx->listen = GF_FALSE;
	} else if (!strnicmp(ctx->dst, "tcp://", 6)) {
		sock_type = GF_SOCK_TYPE_TCP;
#ifdef GPAC_HAS_SOCK_UN
	} else if (!strnicmp(ctx->dst, "udpu://", 7)) {
		sock_type = GF_SOCK_TYPE_UDP_UN;
		ctx->listen = GF_FALSE;
	} else if (!strnicmp(ctx->dst, "tcpu://", 7)) {
		sock_type = GF_SOCK_TYPE_TCP_UN;
#endif
	} else {
		return GF_NOT_SUPPORTED;
	}

	//skip ://
	url = strchr(ctx->dst, ':');
	url += 3;

	ctx->socket = gf_sk_new(sock_type);
	if (! ctx->socket ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[SockOut] Failed to open socket for %s\n", ctx->dst));
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

	if (gf_sk_is_multicast_address(url)) {
		e = gf_sk_setup_multicast(ctx->socket, url, port, 0, 0, ctx->ifce);
		ctx->listen = GF_FALSE;
	} else if ((sock_type == GF_SOCK_TYPE_UDP)
#ifdef GPAC_HAS_SOCK_UN
		|| (sock_type == GF_SOCK_TYPE_UDP_UN)
#endif
	) {
		e = gf_sk_bind(ctx->socket, ctx->ifce, port, url, port, GF_SOCK_REUSE_PORT | GF_SOCK_FAKE_BIND);
		ctx->listen = GF_FALSE;
	} else if (ctx->listen) {
		e = gf_sk_bind(ctx->socket, NULL, port, url, 0, GF_SOCK_REUSE_PORT);
		if (!e) e = gf_sk_listen(ctx->socket, ctx->maxc);
		if (!e) {
			gf_filter_post_process_task(filter);
			gf_sk_server_mode(ctx->socket, GF_TRUE);
		}
	} else {
		e = gf_sk_connect(ctx->socket, url, port, ctx->ifce);
	}

	if (str) str[0] = ':';

	if (e) {
		gf_sk_del(ctx->socket);
		ctx->socket = NULL;
		return e;
	}

	gf_sk_set_buffer_size(ctx->socket, 0, ctx->sockbuf);

	return GF_OK;
}

static void sockout_finalize(GF_Filter *filter)
{
	GF_SockOutCtx *ctx = (GF_SockOutCtx *) gf_filter_get_udta(filter);
	if (ctx->clients) {
		while (gf_list_count(ctx->clients)) {
			GF_SockOutClient *sc = gf_list_pop_back(ctx->clients);
			if (sc->socket) gf_sk_del(sc->socket);
			gf_free(sc);
		}
		gf_list_del(ctx->clients);
	}

	if (ctx->socket) gf_sk_del(ctx->socket);
}

static GF_Err sockout_send_packet(GF_SockOutCtx *ctx, GF_FilterPacket *pck, GF_Socket *dst_sock)
{
	GF_Err e;
	const GF_PropertyValue *p;
	u32 w, h, stride, stride_uv, pf;
	u32 nb_planes, uv_height;
	const char *pck_data;
	u32 pck_size;
	GF_FilterFrameInterface *hwf=NULL;
	if (!dst_sock) return GF_OK;

	pck_data = gf_filter_pck_get_data(pck, &pck_size);
	if (pck_data) {
		e = gf_sk_send(dst_sock, pck_data, pck_size);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SockOut] Write error: %s\n", gf_error_to_string(e) ));
		}
		ctx->nb_bytes_sent += pck_size;
		return e;
	}
	hwf = gf_filter_pck_get_frame_interface(pck);
	if (!hwf) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SockOut] output file handle is not opened, discarding %d bytes\n", pck_size));
		return GF_OK;
	}

	p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_WIDTH);
	w = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_HEIGHT);
	h = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_PIXFMT);
	pf = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(ctx->pid, GF_PROP_PID_STRIDE);
	stride = stride_uv = 0;

	if (gf_pixel_get_size_info(pf, w, h, NULL, &stride, &stride_uv, &nb_planes, &uv_height) == GF_TRUE) {
		u32 i;
		for (i=0; i<nb_planes; i++) {
			u32 j, write_h, lsize;
			const u8 *out_ptr;
			u32 out_stride = i ? stride_uv : stride;
			GF_Err e = hwf->get_plane(hwf, i, &out_ptr, &out_stride);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SockOut] Failed to fetch plane data from hardware frame, cannot write\n"));
				break;
			}
			if (i) {
				write_h = uv_height;
				lsize = stride_uv;
			} else {
				write_h = h;
				lsize = stride;
			}
			for (j=0; j<write_h; j++) {
				e = gf_sk_send(dst_sock, out_ptr, lsize);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SockOut] Write error: %s\n", gf_error_to_string(e) ));
				}
				out_ptr += out_stride;
				ctx->nb_bytes_sent += lsize;
			}
		}
	}
	return GF_OK;
}


static GF_Err sockout_process(GF_Filter *filter)
{
	GF_Err e;
	Bool is_pck_ref = GF_FALSE;

	GF_FilterPacket *pck;
	GF_SockOutCtx *ctx = (GF_SockOutCtx *) gf_filter_get_udta(filter);

	if (!ctx->socket)
		return GF_EOS;

	if (ctx->rate) {
		if (!ctx->start_time) ctx->start_time = gf_sys_clock_high_res();
		else {
			u64 now = gf_sys_clock_high_res() - ctx->start_time;
			if (ctx->nb_bytes_sent*8*1000000 > ctx->rate * now) {
				u64 diff = ctx->nb_bytes_sent*8*1000000 / ctx->rate - now;
				gf_filter_ask_rt_reschedule(filter, (u32) MAX(diff, 1000) );
				return GF_OK;
			} else {
				fprintf(stderr, "[SockOut] Sending at "LLU" kbps                       \r", ctx->nb_bytes_sent*8*1000/now);
			}
		}
	}

	if (ctx->listen) {
		GF_Socket *new_conn=NULL;
		e = gf_sk_accept(ctx->socket, &new_conn);
		if ((e==GF_OK) && new_conn) {
			GF_SockOutClient *sc;
			GF_SAFEALLOC(sc, GF_SockOutClient);
			sc->socket = new_conn;
			strcpy(sc->address, "unknown");
			gf_sk_get_remote_address(new_conn, sc->address);

			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[SockOut] Accepting new connection from %s\n", sc->address));
			gf_list_add(ctx->clients, sc);
			ctx->had_clients = GF_TRUE;
			if (!ctx->pid_started && ctx->pid) {
				GF_FilterEvent evt;
				gf_filter_pid_init_play_event(ctx->pid, &evt, ctx->start, ctx->speed, "SockOut");
				gf_filter_pid_send_event(ctx->pid, &evt);
				ctx->pid_started = GF_TRUE;
			}
			sc->pck_pending = ctx->pck_pending;
			if (!ctx->nb_pck_processed)
				sc->is_tuned = GF_TRUE;
		}
	}
	if (!ctx->pid) {
		if (ctx->listen) gf_filter_post_process_task(filter);
		return GF_OK;
	}

	pck = gf_filter_pid_get_packet(ctx->pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->pid)) {
			if (ctx->rev_pck) {
				is_pck_ref = GF_TRUE;
				pck = ctx->rev_pck;
			} else {
				if (!ctx->listen) {
					gf_sk_del(ctx->socket);
					ctx->socket = NULL;
					return GF_EOS;
				}
				if (!ctx->ka)
					return GF_EOS;
				//keep alive, ask for real-time reschedule of 100 ms - we should use socket groups and selects !
				gf_filter_ask_rt_reschedule(filter, 100000);
			}
		}
		if (!pck)
			return GF_OK;
	}

	if (ctx->pckd.den && !ctx->rev_pck) {
		u32 pck_idx = ctx->pckd.num;
		u32 nb_pck = ctx->nb_pckd_wnd * ctx->pckd.den;

		if (!pck_idx) {
			if (!ctx->next_pckd_idx) ctx->next_pckd_idx = gf_rand() % ctx->pckd.den;
			pck_idx = ctx->next_pckd_idx;
		}
		nb_pck += pck_idx;

		if (nb_pck == 1+ctx->nb_pck_processed) {
			ctx->nb_pck_processed++;
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[SockOut] Droping packet %d per user request\r", ctx->nb_pck_processed));
			gf_filter_pid_drop_packet(ctx->pid);
			ctx->next_pckd_idx = 0;
			ctx->nb_pckd_wnd ++;
			return GF_OK;
		}
	}
	if (ctx->pckr.den && !ctx->rev_pck) {
		u32 pck_idx = ctx->pckr.num;
		u32 nb_pck = ctx->nb_pckr_wnd * ctx->pckr.den;

		if (!pck_idx) {
			if (!ctx->next_pckr_idx) ctx->next_pckr_idx = gf_rand() % ctx->pckr.den;
			pck_idx = ctx->next_pckr_idx;
		}
		nb_pck += pck_idx;

		if (nb_pck == 1+ctx->nb_pck_processed) {
			ctx->rev_pck = pck;
			gf_filter_pck_ref(&ctx->rev_pck);
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[SockOut] Reverting packet %d per user request\r", ctx->nb_pck_processed));
			ctx->nb_pck_processed++;
			gf_filter_pid_drop_packet(ctx->pid);
			pck = gf_filter_pid_get_packet(ctx->pid);
			if (!pck) return GF_OK;
		}
	}

	if (ctx->listen) {
		Bool had_pck_pending = ctx->pck_pending;
		u32 i, nb_clients = gf_list_count(ctx->clients);
		u8 dep_flags = gf_filter_pck_get_dependency_flags(pck);
		if ((dep_flags & 0x3) == 1) {
		}
		ctx->pck_pending = GF_FALSE;

		if (!nb_clients) {
			//client disconnected, drop packet if needed
			if (ctx->had_clients && !ctx->kp) {
				if (is_pck_ref) {
					gf_filter_pck_unref(pck);
					ctx->rev_pck = NULL;
				} else {
					gf_filter_pid_drop_packet(ctx->pid);
				}
			}
			return GF_OK;
		}
		for (i=0; i<nb_clients; i++) {
			GF_SockOutClient *sc = gf_list_get(ctx->clients, i);
			if (!sc->is_tuned) {

			}
			if (had_pck_pending && !sc->pck_pending) {
				continue;
			}
			sc->pck_pending = GF_FALSE;

			e = sockout_send_packet(ctx, pck, sc->socket);
			if (e == GF_BUFFER_TOO_SMALL) {
				sc->pck_pending = GF_TRUE;
				ctx->pck_pending = GF_TRUE;
			}
		}
		if (ctx->pck_pending) return GF_OK;

	} else {
		e = sockout_send_packet(ctx, pck, ctx->socket);
		if (e == GF_BUFFER_TOO_SMALL) return GF_OK;
	}

	ctx->nb_pck_processed++;

	if (is_pck_ref) {
		gf_filter_pck_unref(pck);
		ctx->rev_pck = NULL;
	} else {
		gf_filter_pid_drop_packet(ctx->pid);
		if (ctx->rev_pck) {
			e = sockout_send_packet(ctx, ctx->rev_pck, ctx->socket);
			if (e == GF_BUFFER_TOO_SMALL) return GF_OK;
			gf_filter_pck_unref(ctx->rev_pck);
			ctx->rev_pck = NULL;
			ctx->next_pckr_idx=0;
			ctx->nb_pckr_wnd++;
		}
	}
	return GF_OK;
}

static GF_FilterProbeScore sockout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "tcp://", 6)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "udp://", 6)) return GF_FPROBE_SUPPORTED;
#ifdef GPAC_HAS_SOCK_UN
	if (!strnicmp(url, "tcpu://", 7)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "udpu://", 7)) return GF_FPROBE_SUPPORTED;
#endif
	return GF_FPROBE_NOT_SUPPORTED;
}


#define OFFS(_n)	#_n, offsetof(GF_SockOutCtx, _n)

static const GF_FilterArgs SockOutArgs[] =
{
	{ OFFS(dst), "location of destination file", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(sockbuf), "block size used to read file", GF_PROP_UINT, "65536", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(port), "default port if not specified", GF_PROP_UINT, "1234", NULL, 0},
	{ OFFS(ifce), "default multicast interface", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ext), "file extension of pipe data - see filter help", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(mime), "mime type of pipe data - see filter help", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(listen), "indicate the output socket works in server mode", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(maxc), "max number of concurrent connections", GF_PROP_UINT, "+I", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ka), "keep socket alive if no more connections", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(kp), "keep packets in queue if no more clients", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(start), "set playback start offset. Negative value means percent of media dur with -1 <=> dur", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(speed), "set playback speed. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(rate), "set send rate in bps, disabled by default (as fast as possible)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pckr), "reverse packet every N - see filter help", GF_PROP_FRACTION, "0/0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(pckd), "drop packet every N - see filter help", GF_PROP_FRACTION, "0/0", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability SockOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_MIME, "*"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_FILE_EXT, "*"),
};


GF_FilterRegister SockOutRegister = {
	.name = "sockout",
	GF_FS_SET_DESCRIPTION("UDP/TCP output")
#ifndef GPAC_DISABLE_DOC
	.help = "This filter handles generic output sockets (mono-directionnal) in blocking mode only.\n"
		"The filter can work in server mode, waiting for source connections, or or in client mode, directly connecting.\n"
		"In server mode, the filter can be instructed to keep running at the end of the stream\n"
		"In server mode, the default behaviour is to keep input packets when no more clients are connected; "
		"this can be adjusted though the [-kp]() option, however there is no realtime regulation of how fast packets are droped.\n"
		"If your sources are not real time, consider adding a real-time scheduler in the chain (cf reframer filter), or set the send [-rate]() option\n"
		"\n"
		"UDP sockets are used for destinations URLs formatted as udp://NAME\n"
		"TCP sockets are used for destinations URLs formatted as tcp://NAME\n"
#ifdef GPAC_HAS_SOCK_UN
		"UDP unix domain sockets are used for destinations URLs formatted as udpu://NAME\n"
		"TCP unix domain sockets are used for destinations URLs formatted as tcpu://NAME\n"
#else
		"Your platform does not supports unix domain sockets"
#endif
		"\n"
		"The socket output can be configured to drop or revert packet order for test purposes.\n"
		"For both mode, a window size in packets is specified as the drop/revert fraction denominator, and the index of the packet to drop/revert is given as the numerator\n"
		"If the numerator is 0, a packet is randomly chosen in that window\n"
		"EX :pckd=4/10\n"\
		"This drops every 4th packet of each 10 packet window\n"
		"EX :pckr=0/100\n"\
		"This reverts the send order of one random packet in each 100 packet window\n"
		"\n",
#endif //GPAC_DISABLE_DOC
	.private_size = sizeof(GF_SockOutCtx),
	.args = SockOutArgs,
	SETCAPS(SockOutCaps),
	.probe_url = sockout_probe_url,
	.initialize = sockout_initialize,
	.finalize = sockout_finalize,
	.configure_pid = sockout_configure_pid,
	.process = sockout_process
};


const GF_FilterRegister *sockout_register(GF_FilterSession *session)
{
	return &SockOutRegister;
}

