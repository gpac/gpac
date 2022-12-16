/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / rtsp output filter
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

#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include <gpac/maths.h>

#if !defined(GPAC_DISABLE_STREAMING)

#include <gpac/filters.h>
#include <gpac/ietf.h>
#include <gpac/config_file.h>
#include <gpac/base_coding.h>
#include <gpac/rtp_streamer.h>

//common functions for rtpout and rtspout
#include "out_rtp.h"

#ifdef GPAC_HAS_SSL

void *gf_ssl_new(void *ssl_server_ctx, GF_Socket *client_sock, GF_Err *e);
void *gf_ssl_server_context_new(const char *cert, const char *key);
void gf_ssl_server_context_del(void *ssl_server_ctx);
Bool gf_ssl_init_lib();

#endif

enum
{
	SDP_NONE = 0,
	SDP_WAIT,
	SDP_LOADED
};

enum
{
	MCAST_OFF = 0,
	MCAST_ON,
	MCAST_MIRROR,
	//describe does not ened authenticate but multicast setup does
	MCAST_AUTHENTICATE_SETUP,
};

enum
{
	TRP_BOTH=0,
	TRP_UDP_ONLY,
	TRP_TCP_ONLY
};

typedef struct
{
	char *name;
	u32 name_len;
	char *path;
	char *ru, *rg;
	char *mcast;
} RTSP_DIRInfo;

typedef struct
{
	//options
	char *dst, *user_agent, *cert, *pkey;
	GF_PropStringList mounts;
	u32 port, firstport, timeout;
	Bool xps;
	u32 mtu;
	u32 ttl;
	char *ifce;
	u32 payt, tt;
	s32 delay;
	char *info, *url, *email;
	s32 runfor, tso;
	u32 maxc;
	u32 block_size;
	Bool close, loop, mpeg4, quit, htun, dynurl;
	u32 mcast, trp;
	Bool latm;

	GF_Socket *server_sock;
	GF_List *sessions;

	u32 next_wake_us;
	char *ip;
	Bool done, is_active;

	GF_List *directories;

	void *ssl_ctx;
	u32 ms_timeout;
} GF_RTSPOutCtx;

typedef struct __rtspout_session
{
	GF_RTSPOutCtx *ctx;

	struct __rtspout_session *mcast_mirror;

	GF_RTSPSession *rtsp;
	GF_RTSPCommand *command;
	GF_RTSPResponse *response;
	/*list of streams in session*/
	GF_List *streams;
	Bool loop_disabled, loop;

	char *service_name;
	char *sessionID;
	char peer_address[GF_MAX_IP_NAME_LEN];
	char ctrl_name[10];

	u32 play_state;
	Double start_range;
	u32 last_cseq;
	Bool interleave, is_tunnel;

	/*base stream if this stream contains a media decoding dependency, 0 otherwise*/
	u32 base_pid_id;

	Bool first_RTCP_sent;

	GF_RTPOutStream *active_stream;
	u32 active_stream_idx;
	u64 active_min_ts_microsec;

	/*timeline origin of our session (all tracks) in microseconds*/
	u64 sys_clock_at_init;

	Bool wait_for_loop;
	u64 microsec_ts_init;

	Bool single_session;
	char *server_path;
	GF_List *filter_srcs;

	u32 sdp_state;

	u32 next_stream_id;

	u64 pause_sys_clock;
	Bool request_pending;
	char *multicast_ip;
	u64 sdp_id;
	u32 mcast_mode;
	char *mcast_sname;

	u32 last_active_time;
	char *setup_ctrl;
} GF_RTSPOutSession;

static GF_Err rtspout_process_setup(GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess, char *ctrl);

static void rtspout_send_response(GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	sess->response->User_Agent = ctx->user_agent;
	sess->response->Session = sess->sessionID;
	if (ctx->close && !sess->interleave && !sess->is_tunnel)
		sess->response->Connection = "close";
	gf_rtsp_send_response(sess->rtsp, sess->response);
	sess->response->User_Agent = NULL;
	sess->response->Session = NULL;
	sess->last_active_time = gf_sys_clock();
	if (ctx->close && !sess->interleave && !sess->is_tunnel) {
		sess->response->Connection = NULL;
		gf_rtsp_session_del(sess->rtsp);
		sess->rtsp = NULL;
	}
}

static void rtspout_check_last_sess(GF_RTSPOutCtx *ctx)
{
	if (gf_list_count(ctx->sessions) ) return;
	ctx->is_active = GF_FALSE;
	if (ctx->dst)
		ctx->done = GF_TRUE;
	else if (ctx->runfor>0)
		ctx->done = GF_TRUE;
}

static GF_Err rtspout_send_sdp(GF_RTSPOutSession *sess)
{
	FILE *sdp_out;
	u32 fsize;
	GF_Err e;

	if (sess->setup_ctrl) {
		sess->response->CSeq = sess->last_cseq;
		//setup on resource not loaded, reassign control string based on setup query
		//this typically happen after a seek when we teardown the session right away
		if (!sess->sessionID || !sess->ctrl_name[0]) {
			char *sep = strchr(sess->setup_ctrl, '=');
			if (sep) sep[0] = 0;
			strncpy(sess->ctrl_name, sess->setup_ctrl+1, 9);
			sess->ctrl_name[9] = 0;
			if (sep) sep[0] = '=';
		}
		e = rtspout_process_setup(sess->ctx, sess, sess->setup_ctrl);
		gf_free(sess->setup_ctrl);
		sess->setup_ctrl = NULL;
		return e;
	}

	const char *ip = sess->ctx->ip;
	if (!ip) ip = sess->ctx->ifce;
	if (!ip) ip = "127.0.0.1";

	if (sess->mcast_mirror) {
		ip = sess->mcast_mirror->multicast_ip;
 		e = rtpout_create_sdp(sess->mcast_mirror->streams, GF_FALSE, ip, sess->ctx->info, "livesession", sess->ctx->url, sess->ctx->email, sess->mcast_mirror->base_pid_id, &sdp_out, &sess->sdp_id);
	} else {
 		e = rtpout_create_sdp(sess->streams, GF_TRUE, ip, sess->ctx->info, "livesession", sess->ctx->url, sess->ctx->email, sess->base_pid_id, &sdp_out, &sess->sdp_id);
	}
	if (e) return e;

	fsize = (u32) gf_ftell(sdp_out);
	char *sdp_output = gf_malloc(sizeof(char)*(fsize+1));
	gf_fseek(sdp_out, 0, SEEK_SET);
	u32 read = (u32) gf_fread(sdp_output, fsize, sdp_out);
	sdp_output[read]=0;
	gf_fclose(sdp_out);


	gf_rtsp_response_reset(sess->response);
	sess->response->ResponseCode = NC_RTSP_OK;
	sess->response->CSeq = sess->command->CSeq;
	sess->response->body = sdp_output;

	rtspout_send_response(sess->ctx, sess);
	sess->response->body = NULL;
	gf_free(sdp_output);

	return GF_OK;
}

static void rtspout_del_stream(GF_RTPOutStream *st)
{
	if (st->pid) {
		GF_FilterEvent fevt;
		gf_filter_pid_set_discard(st->pid, GF_TRUE);
		GF_FEVT_INIT(fevt, GF_FEVT_STOP, st->pid);
		gf_filter_pid_send_event(st->pid, &fevt);
		st->has_pck = GF_FALSE;
	}
	rtpout_del_stream(st);
}


static void rtspout_del_session(GF_Filter *filter, GF_RTSPOutSession *sess)
{
	//server mode, cleanup
	while (gf_list_count(sess->streams)) {
		GF_RTPOutStream *stream = gf_list_pop_back(sess->streams);
		rtspout_del_stream(stream);
	}
	gf_list_del(sess->streams);

	if (sess->service_name)
		gf_free(sess->service_name);
	if (sess->sessionID)
		gf_free(sess->sessionID);

	if (filter) {
		while (gf_list_count(sess->filter_srcs)) {
			GF_Filter *fsrc = gf_list_pop_back(sess->filter_srcs);
			gf_filter_remove_src(filter, fsrc);
		}
	}
	gf_list_del(sess->filter_srcs);
	gf_rtsp_session_del(sess->rtsp);
	gf_rtsp_command_del(sess->command);
	gf_rtsp_response_del(sess->response);
	gf_list_del_item(sess->ctx->sessions, sess);
	if (sess->ctx->quit && !gf_list_count(sess->ctx->sessions))
		sess->ctx->done = GF_TRUE;

	if (sess->multicast_ip) gf_free(sess->multicast_ip);
	if (sess->setup_ctrl) gf_free(sess->setup_ctrl);
	gf_free(sess);
}


GF_RTSPOutSession *rtspout_locate_session_for_pid(GF_Filter *filter, GF_RTSPOutCtx *ctx, GF_FilterPid *pid)
{
	u32 i, count = gf_list_count(ctx->sessions);
	if (ctx->dst) {
		for (i=0; i<count; i++) {
			GF_RTSPOutSession *sess = gf_list_get(ctx->sessions, i);
			if (sess->single_session) return sess;
		}
		return NULL;
	}
	for (i=0; i<count; i++) {
		u32 j, nb_filters;
		GF_RTSPOutSession *sess = gf_list_get(ctx->sessions, i);
		if (sess->single_session) continue;
		nb_filters = gf_list_count(sess->filter_srcs);
		for (j=0; j<nb_filters; j++) {
			GF_Filter *srcf = gf_list_get(sess->filter_srcs, j);
			if (gf_filter_pid_is_filter_in_parents(pid, srcf))
				return sess;
		}
	}

	return NULL;
}

static void rtspout_on_rtcp(void *udta)
{
	GF_RTSPOutSession *sess = (GF_RTSPOutSession *) udta;
	sess->last_active_time = gf_sys_clock();
}

static GF_Err rtspout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_RTSPOutCtx *ctx = (GF_RTSPOutCtx *) gf_filter_get_udta(filter);
	GF_Err e = GF_OK;
	GF_RTPOutStream *stream;
	GF_RTSPOutSession *sess;
	u32 streamType, payt;
	const GF_PropertyValue *p;

	sess = rtspout_locate_session_for_pid(filter, ctx, pid);

	if (is_remove) {
		if (!sess) return GF_OK;
		GF_RTPOutStream *t = gf_filter_pid_get_udta(pid);
		if (t) {
			if (sess->active_stream==t) sess->active_stream = NULL;
			gf_list_del_item(sess->streams, t);
			rtspout_del_stream(t);
		}
		if (!gf_list_count(sess->streams)) {
			rtspout_del_session(filter, sess);
		}
		return GF_OK;
	}
	if (!sess) return GF_SERVICE_ERROR;
	stream = gf_filter_pid_get_udta(pid);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	streamType = p ? p->value.uint : 0;

	switch (streamType) {
	case GF_STREAM_VISUAL:
	case GF_STREAM_AUDIO:
		break;
	case GF_STREAM_FILE:
	case GF_STREAM_UNKNOWN:
		if (stream) {
			if (sess->active_stream==stream) sess->active_stream = NULL;
			gf_list_del_item(sess->streams, stream);
			rtspout_del_stream(stream);
		}
		return GF_FILTER_NOT_SUPPORTED;
	default:
		break;
	}
	if (!stream) {
		GF_SAFEALLOC(stream, GF_RTPOutStream);
		if (!stream) return GF_OUT_OF_MEM;
		gf_list_add(sess->streams, stream);
		stream->pid = pid;
		stream->streamtype = streamType;
		stream->min_dts = GF_FILTER_NO_TS;
		stream->on_rtcp = rtspout_on_rtcp;
		stream->on_rtcp_udta = sess;

#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode())
			rtspout_on_rtcp(stream->on_rtcp_udta);
#endif
	 	gf_filter_pid_set_udta(pid, stream);
	}
	stream->ctrl_id = sess->next_stream_id+1;
	sess->next_stream_id++;
	stream->ctrl_name = sess->ctrl_name;

	payt = ctx->payt + gf_list_find(sess->streams, stream);

	e = rtpout_init_streamer(stream, ctx->ifce ? ctx->ifce : "127.0.0.1", ctx->xps, ctx->mpeg4, ctx->latm, payt, ctx->mtu, ctx->ttl, ctx->ifce, GF_TRUE, &sess->base_pid_id, 0);
	if (e) return e;

	if (ctx->loop) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
		if (!p || (p->value.uint<GF_PLAYBACK_MODE_FASTFORWARD)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] PID %s cannot be seek, disabling loop\n", gf_filter_pid_get_name(pid) ));

			sess->loop_disabled = GF_TRUE;
		}
	}
	return GF_OK;
}

static GF_Err rtspout_check_new_session(GF_RTSPOutCtx *ctx, Bool single_session)
{
	GF_RTSPOutSession *sess;
	GF_RTSPSession *new_sess = NULL;

	if (!single_session) {
		new_sess = gf_rtsp_session_new_server(ctx->server_sock, ctx->htun, ctx->ssl_ctx);
		if (!new_sess) return GF_OK;
	}

	GF_SAFEALLOC(sess, GF_RTSPOutSession);
	if (!sess) {
		gf_rtsp_session_del(new_sess);
		return GF_OUT_OF_MEM;
	}
	sess->rtsp = new_sess;
	sess->command = gf_rtsp_command_new();
	sess->response = gf_rtsp_response_new();
	sess->streams = gf_list_new();
	sess->filter_srcs = gf_list_new();
	if (gf_sys_is_test_mode()) {
		strcpy(sess->ctrl_name, "trackID");
	} else {
		u32 seed = gf_rand();
#ifndef GPAC_64_BITS
		seed |= (u32) sess;
#else
		seed |= (u32) (u64) sess;
#endif
		seed |= gf_sys_clock();
		sprintf(sess->ctrl_name, "s%08X", seed);
	}

	if (new_sess) {
		gf_rtsp_set_buffer_size(new_sess, ctx->block_size);
		gf_rtsp_get_remote_address(new_sess, sess->peer_address);
		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSP] Accepting new connection from %s\n", sess->peer_address));
		sess->last_active_time = gf_sys_clock();
	} else {
		sess->single_session = GF_TRUE;
	}
	sess->ctx = ctx;
	gf_list_add(ctx->sessions, sess);
	ctx->is_active = GF_TRUE;
	return GF_OK;
}

static GF_Err rtspout_initialize(GF_Filter *filter)
{
	char szIP[1024];
	GF_Err e;
	u32 i;
	u16 port = 0;
	char *ip;
	GF_RTSPOutCtx *ctx = (GF_RTSPOutCtx *) gf_filter_get_udta(filter);
	if (!ctx->payt) ctx->payt = 96;
	if (!ctx->firstport) ctx->firstport = 7000;
	if (!ctx->mtu) ctx->mtu = 1450;
	if (ctx->payt<96) ctx->payt = 96;
	if (ctx->payt>127) ctx->payt = 127;
	ctx->sessions = gf_list_new();

	ip = ctx->ifce;
	//move to sec
	ctx->ms_timeout = ctx->timeout * 1000;
	if (!ctx->dst) {
		if (! ctx->mounts.nb_items) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] No root dir for server, cannot run\n" ));
			return GF_BAD_PARAM;
		}
		gf_filter_make_sticky(filter);
	} else {
		GF_RTSPOutSession *sess;
		char *sep = strchr(ctx->dst+7, '/');
		if (sep) {
			u32 cplen = (u32) (sep-ctx->dst-7);
			if (cplen>1023) cplen = 1023;
			strncpy(szIP, ctx->dst+7, cplen);
			szIP[1023] = 0;
			sep = strchr(szIP, ':');
			if (sep) {
				port = atoi(sep+1);
				sep[0] = 0;
			}
			if (strlen(szIP)) ip = szIP;
		}
		rtspout_check_new_session(ctx, GF_TRUE);
		sess = gf_list_get(ctx->sessions, 0);
		if (!sess) return GF_SERVICE_ERROR;
		sess->server_path = ctx->dst;
		sess->sdp_state = SDP_LOADED;
	}

	if (ip)
		ctx->ip = gf_strdup(ip);

	if (ctx->cert && !ctx->pkey) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[RTSPOut] missing server private key file\n"));
		return GF_BAD_PARAM;
	}
	if (!ctx->cert && ctx->pkey) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[RTSPOut] missing server certificate file\n"));
		return GF_BAD_PARAM;
	}
	if (ctx->cert && ctx->pkey) {
#ifdef GPAC_HAS_SSL
		if (!gf_file_exists(ctx->cert)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[RTSPOut] Certificate file %s not found\n", ctx->cert));
			return GF_IO_ERR;
		}
		if (!gf_file_exists(ctx->pkey)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[RTSPOut] Private key file %s not found\n", ctx->pkey));
			return GF_IO_ERR;
		}
		if (gf_ssl_init_lib()) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[RTSPOut] Failed to initialize OpenSSL library\n"));
			return GF_IO_ERR;
		}
		ctx->ssl_ctx = gf_ssl_server_context_new(ctx->cert, ctx->pkey);
		if (!ctx->ssl_ctx) return GF_IO_ERR;

		if (!ctx->port || (ctx->port==554))
			ctx->port = 322;
#else
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] TLS key/certificate set but GPAC compiled without TLS support\n"));
		return GF_NOT_SUPPORTED;

#endif
	}

	if (!ctx->port)
		ctx->port = 554;

	if (!port)
		port = ctx->port;

	ctx->server_sock = gf_sk_new(GF_SOCK_TYPE_TCP);
	e = gf_sk_bind(ctx->server_sock, NULL, port, ip, 0, GF_SOCK_REUSE_PORT);
	if (!e) e = gf_sk_listen(ctx->server_sock, ctx->maxc);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] failed to start server on port %d: %s\n", ctx->port, gf_error_to_string(e) ));
		return e;
	}

	gf_sk_server_mode(ctx->server_sock, GF_TRUE);

	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSPOut] Server running on port %d\n", ctx->port));
	gf_filter_post_process_task(filter);

	ctx->directories = gf_list_new();
	for (i=0; i<ctx->mounts.nb_items; i++) {
		char *dpath = ctx->mounts.vals[i];
		if (gf_dir_exists(dpath)) {
			RTSP_DIRInfo *di;
			GF_SAFEALLOC(di, RTSP_DIRInfo);
			di->path = gf_strdup(dpath);
			gf_list_add(ctx->directories, di);
		} else if (gf_file_exists(dpath)) {
			GF_Config *rules = gf_cfg_new(NULL, dpath);
			u32 j, count = gf_cfg_get_section_count(rules);
			for (j=0; j<count; j++) {
				const char *dname = gf_cfg_get_section_name(rules, j);
				if (strcmp(dname, "$dynurl") && !gf_dir_exists(dname)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTSPOut] No such directory %s, ignoring rule\n", dname));
					continue;
				}
				const char *fnames = gf_cfg_get_key(rules, dname, "filters");
				if (fnames && strcmp(fnames, "*") && strcmp(fnames, "all")) {
					if (!strstr(fnames, "rtspout")) continue;
				}
				RTSP_DIRInfo *di;
				GF_SAFEALLOC(di, RTSP_DIRInfo);
				di->path = gf_strdup(dname);
				gf_list_add(ctx->directories, di);
				di->name = (char*)gf_cfg_get_key(rules, dname, "name");
				if (di->name) {
					di->name = gf_strdup(di->name);
					di->name_len = (u32) strlen(di->name);
					if (di->name[di->name_len-1] != '/') {
						gf_dynstrcat(&di->name, "/", NULL);
						di->name_len += 1;
					}
				}
				di->ru = (char*)gf_cfg_get_key(rules, dname, "ru");
				if (di->ru) di->ru = gf_strdup(di->ru);
				di->rg = (char*)gf_cfg_get_key(rules, dname, "rg");
				if (di->rg) di->rg = gf_strdup(di->rg);
				di->mcast = (char*)gf_cfg_get_key(rules, dname, "mcast");
				if (di->mcast) di->mcast = gf_strdup(di->mcast);
			}
			gf_cfg_del(rules);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTSPOut] No such directory %s, ignoring rule\n", dpath));
		}
	}
	if (!gf_list_count(ctx->directories) && !ctx->dst) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] No root dir for server, cannot run\n" ));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

static void rtspout_finalize(GF_Filter *filter)
{
	GF_RTSPOutCtx *ctx = (GF_RTSPOutCtx *) gf_filter_get_udta(filter);

	while (gf_list_count(ctx->sessions)) {
		GF_RTSPOutSession *tmp = gf_list_get(ctx->sessions, 0);
		rtspout_del_session(NULL, tmp);
	}
	gf_list_del(ctx->sessions);

	if (ctx->server_sock) gf_sk_del(ctx->server_sock);
	if (ctx->ip) gf_free(ctx->ip);

#ifdef GPAC_HAS_SSL
	if (ctx->ssl_ctx) {
		gf_ssl_server_context_del(ctx->ssl_ctx);
	}
#endif

	while (gf_list_count(ctx->directories)) {
		RTSP_DIRInfo *di = gf_list_pop_back(ctx->directories);
		gf_free(di->path);
		if (di->name) gf_free(di->name);
		if (di->ru) gf_free(di->ru);
		if (di->rg) gf_free(di->rg);
		if (di->mcast) gf_free(di->mcast);
		gf_free(di);
	}
	gf_list_del(ctx->directories);
}


static Bool rtspout_init_clock(GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	u64 min_dts = GF_FILTER_NO_TS;
	u32 i, count = gf_list_count(sess->streams);

	for (i=0; i<count; i++) {
		u64 dts;
		GF_FilterPacket *pck;
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		if (!stream->selected) continue;

		while (1) {
			pck = gf_filter_pid_get_packet(stream->pid);
			if (!pck) return GF_FALSE;
			if (gf_filter_pck_get_seek_flag(pck)) {
				gf_filter_pid_drop_packet(stream->pid);
				continue;
			}
			break;
		}

		dts = gf_filter_pck_get_dts(pck);
		if (dts==GF_FILTER_NO_TS)
			dts = gf_filter_pck_get_cts(pck);

		if (dts==GF_FILTER_NO_TS) dts=0;

		dts = gf_timestamp_rescale(dts, stream->timescale, 1000000);
		if (min_dts > dts)
			min_dts = dts;

		if (ctx->tso>0) {
			u64 offset = gf_timestamp_rescale(ctx->tso, 1000000, stream->timescale);
			stream->rtp_ts_offset = (u32) offset;
		}
		stream->current_cts = gf_filter_pck_get_cts(pck);
	}
	sess->sys_clock_at_init = gf_sys_clock_high_res();
	sess->microsec_ts_init = min_dts;
	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSPOut] Session %s: RTP clock initialized - time origin set to "LLU" us (sys clock) / "LLU" us (media clock)\n", sess->service_name, sess->sys_clock_at_init, sess->microsec_ts_init));
	if (ctx->tso<0) {
		gf_rand_init(GF_FALSE);
		for (i=0; i<count; i++) {
			GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
			stream->rtp_ts_offset = gf_rand();
			while (stream->rtp_ts_offset>0xFFFFFFF)
				stream->rtp_ts_offset/=2;
			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSPOut] Session %s: RTP stream %d initial RTP TS set to %d\n", sess->service_name, i+1, stream->rtp_ts_offset));
		}
	}


	gf_rtsp_response_reset(sess->response);
	sess->response->ResponseCode = NC_RTSP_OK;
	for (i=0; i<count; i++) {
		GF_RTPInfo *rtpi;
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		if (!stream->selected) continue;
		if (!stream->send_rtpinfo) continue;
		stream->send_rtpinfo = GF_FALSE;

		GF_SAFEALLOC(rtpi, GF_RTPInfo);
		if (rtpi) {
			u32 timescale;
			rtpi->url = gf_malloc(sizeof(char) * (strlen(sess->service_name)+50));
			sprintf(rtpi->url, "%s/%s=%d", sess->service_name, sess->ctrl_name, stream->ctrl_id);
			rtpi->seq = gf_rtp_streamer_get_next_rtp_sn(stream->rtp);
			rtpi->rtp_time = (u32) (stream->current_cts + stream->ts_offset + stream->rtp_ts_offset);

			timescale = gf_rtp_streamer_get_timescale(stream->rtp);
			if (timescale)
				rtpi->rtp_time = (u32) gf_timestamp_rescale(rtpi->rtp_time, stream->timescale, timescale);

			gf_list_add(sess->response->RTP_Infos, rtpi);

#ifdef GPAC_ENABLE_COVERAGE
			if (gf_sys_is_cov_mode()) {
				gf_rtp_streamer_get_ssrc(stream->rtp);
			}
#endif
		}
	}
	GF_SAFEALLOC(sess->response->Range, GF_RTSPRange);
	if (sess->response->Range)
		sess->response->Range->start = sess->start_range;

	sess->response->CSeq = sess->last_cseq;
	rtspout_send_response(ctx, sess);
	sess->request_pending = GF_FALSE;
	return GF_TRUE;
}

static void rtspout_send_event(GF_RTSPOutSession *sess, Bool send_stop, Bool send_play, Double start_range)
{
	GF_FilterEvent fevt;
	u32 i, count = gf_list_count(sess->streams);

	memset(&fevt, 0, sizeof(GF_FilterEvent));

	for (i=0; i<count; i++) {
		GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
		fevt.base.on_pid = stream->pid;
		if (!stream->selected) {
			//send a play/stop event for this pid
			fevt.base.type = GF_FEVT_PLAY;
			gf_filter_pid_send_event(stream->pid, &fevt);
			fevt.base.type = GF_FEVT_STOP;
			gf_filter_pid_send_event(stream->pid, &fevt);
			stream->state = RTPOUT_STREAM_STOP;
			stream->has_pck = GF_FALSE;
			continue;
		}
		if (send_stop && (stream->state!=RTPOUT_STREAM_STOP)) {
			stream->state = RTPOUT_STREAM_STOP;
			fevt.base.type = GF_FEVT_STOP;
			gf_filter_pid_set_discard(stream->pid, GF_TRUE);
			gf_filter_pid_send_event(stream->pid, &fevt);
			stream->has_pck = GF_FALSE;
		}
		if (send_play && (stream->state!=RTPOUT_STREAM_PLAY)) {
			stream->state = RTPOUT_STREAM_PLAY;
			gf_filter_pid_set_discard(stream->pid, GF_FALSE);
			fevt.base.type = GF_FEVT_PLAY;
			fevt.play.start_range = start_range;
			gf_filter_pid_send_event(stream->pid, &fevt);

			stream->send_rtpinfo = GF_TRUE;
		}
	}
}

static GF_Err rtspout_process_rtp(GF_Filter *filter, GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	GF_Err e = GF_OK;
	u32 repost_delay_us=0;

	/*init session timeline - all sessions are sync'ed for packet scheduling purposes*/
	if (!sess->sys_clock_at_init) {
		if (!rtspout_init_clock(ctx, sess)) return GF_OK;
	}

	if (sess->rtsp && sess->interleave) {
		e = gf_rtsp_check_connection(sess->rtsp);
		if (e==GF_IP_NETWORK_EMPTY) {
			ctx->next_wake_us = 100;
			return GF_OK;
		} else if (e) {
			return e;
		}
	}

	e = rtpout_process_rtp(sess->streams, &sess->active_stream, sess->loop, ctx->delay, &sess->active_stream_idx, sess->sys_clock_at_init, &sess->active_min_ts_microsec, sess->microsec_ts_init, &sess->wait_for_loop, &repost_delay_us, &sess->first_RTCP_sent, sess->base_pid_id);

	if (e) {
		if ((e==GF_IP_CONNECTION_CLOSED) || (e==GF_IP_CONNECTION_FAILURE)) {
			sess->play_state = 0;
			rtspout_send_event(sess, GF_TRUE, GF_FALSE, 0);

			rtspout_del_session(filter, sess);
			rtspout_check_last_sess(ctx);
		}
		return e;
	}
	if (ctx->next_wake_us > repost_delay_us)
		ctx->next_wake_us = (u32) repost_delay_us;
	return GF_OK;
}

static GF_Err rtspout_interleave_packet(void *cbk1, void *cbk2, Bool is_rtcp, u8 *pck, u32 pck_size)
{
	GF_RTSPOutSession *sess = (GF_RTSPOutSession *)cbk1;
	GF_RTPOutStream *stream = (GF_RTPOutStream *)cbk2;

	u32 idx = is_rtcp ? stream->rtcp_id : stream->rtp_id;
	if (!sess->rtsp)
		return GF_IP_CONNECTION_CLOSED;
	return gf_rtsp_session_write_interleaved(sess->rtsp, idx, pck, pck_size);
}

Bool rtspout_on_filter_setup_error(GF_Filter *f, void *on_setup_error_udta, GF_Err e)
{
	GF_RTSPOutSession *sess = (GF_RTSPOutSession *)on_setup_error_udta;

	gf_list_del_item(sess->filter_srcs, f);
	//we don't notify error at our session level if the request fails
	if (gf_list_count(sess->filter_srcs)) return GF_TRUE;

	if (sess->sdp_state != SDP_LOADED) {
		sess->sdp_state = SDP_LOADED;
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_Internal_Server_Error;
		if ((e == GF_URL_ERROR) || (e == GF_URL_REMOVED) || (e == GF_IP_CONNECTION_FAILURE)) {
			sess->response->ResponseCode = NC_RTSP_Not_Found;
		}
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(sess->ctx, sess);
	}
	rtspout_del_session(NULL, sess);
	//we don't notify error at our session level if the request fails
	return GF_TRUE;
}

static GF_Err rtspout_load_media_service(GF_Filter *filter, GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess, char *src_url)
{
	GF_Err e;
	Bool found = GF_FALSE;
	u32 i, count = gf_list_count(sess->filter_srcs);
	for (i=0; i<count; i++) {
		GF_Filter *src = gf_list_get(sess->filter_srcs, i);
		const char *url = gf_filter_get_arg_str(src, "src", NULL);
		if (url && !strcmp(src_url, url)) {
			found = GF_TRUE;
			break;
		}
	}
	if (!found) {
		GF_Filter *filter_src = gf_filter_connect_source(filter, src_url, NULL, GF_FALSE, &e);
		if (!filter_src) {
			gf_rtsp_response_reset(sess->response);
			sess->response->ResponseCode = NC_RTSP_Session_Not_Found;
			sess->response->CSeq = sess->command->CSeq;
			rtspout_send_response(ctx, sess);
			return e;
		}
		gf_list_add(sess->filter_srcs, filter_src);
		gf_filter_set_setup_failure_callback(filter, filter_src, rtspout_on_filter_setup_error, sess);
		sess->sdp_state = SDP_WAIT;
	}
	if (sess->sdp_state==SDP_LOADED) {
		//single session, create SDP
		rtspout_send_sdp(sess);
	} else {
		sess->sdp_state = SDP_WAIT;
		sess->request_pending = GF_TRUE;
	}
	return GF_OK;
}

static GF_Err rtspout_check_sdp(GF_Filter *filter, GF_RTSPOutSession *sess)
{
	u32 i, count = gf_list_count(sess->streams);
	u32 j, nb_filters = gf_list_count(sess->filter_srcs);

	for (j=0; j<nb_filters; j++) {
		Bool found = GF_FALSE;
		GF_Filter *srcf = gf_list_get(sess->filter_srcs, j);
		//check we have at least one pid
		for (i=0; i<count; i++) {
			GF_RTPOutStream *stream = gf_list_get(sess->streams, i);
			if (gf_filter_pid_is_filter_in_parents(stream->pid, srcf)) {
				found = GF_TRUE;
				break;
			}
		}
		//not yet connected
		if (!found) return GF_OK;

		//check we don't have other pid connection pendings
		if (gf_filter_has_pid_connection_pending(srcf, filter))
			return GF_OK;
	}
	//all streams should be ready - note that we don't know handle dynamic pid insertion in source service yet
	sess->sdp_state = SDP_LOADED;
	sess->request_pending = GF_FALSE;
	rtspout_send_sdp(sess);
	return GF_OK;
}

static void rtspout_get_next_mcast_port(GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess, u32 *port)
{
	u32 i, count = gf_list_count(ctx->sessions);
	u32 min_port=ctx->firstport;
	for (i=0; i<count; i++) {
		u32 j, count2;
		GF_RTSPOutSession *asess = gf_list_get(ctx->sessions, i);
		if (asess == sess) continue;
		if (!asess->multicast_ip || !sess->multicast_ip) continue;
		//reuse port number if different multicast groups
		if (strcmp(asess->multicast_ip, sess->multicast_ip)) continue;

		count2 = gf_list_count(asess->streams);
		for (j=0; j<count2; j++) {
			GF_RTPOutStream *stream = gf_list_get(asess->streams, j);
			if (stream->mcast_port>min_port) min_port = stream->mcast_port;
			if (stream->mcast_port == *port) *port = 0;
		}
	}
	if (! *port) *port = min_port;
}

static GF_RTSPOutSession *rtspout_locate_mcast(GF_RTSPOutCtx *ctx, char *res_path)
{
	u32 i, count = gf_list_count(ctx->sessions);
	for (i=0; i<count; i++) {
		char *a_sess_path=NULL;
		GF_RTSPOutSession *a_sess = gf_list_get(ctx->sessions, i);
		if (!a_sess->multicast_ip) continue;
		if (a_sess->mcast_sname && !strcmp(a_sess->mcast_sname, res_path))
			return a_sess;

		if (!a_sess->service_name) continue;

		a_sess_path = strstr(a_sess->service_name, "://");
		if (a_sess_path) a_sess_path = strchr(a_sess_path+3, '/');
		if (a_sess_path) a_sess_path++;
		if (a_sess_path && !strcmp(a_sess_path, res_path))
			return a_sess;
	}
	return NULL;
}

static char *rtspout_get_local_res_path(GF_RTSPOutCtx *ctx, char *res_path, GF_RTSPCommand *com, u32 *err_code, u32 *mcast_mode)
{
	u32 i, count, di_len;
	RTSP_DIRInfo *di = NULL;
	char *src_url=NULL;
	count = gf_list_count(ctx->directories);

	*err_code = NC_RTSP_Not_Found;
	if (res_path) {
		for (i=0; i<count; i++) {
			char *res_path_loc = res_path;
			di = gf_list_get(ctx->directories, i);
			char *mpoint = di->path;
			if (di->name) {
				if (strncmp(res_path, di->name, di->name_len)) continue;
				res_path_loc += di->name_len;
			}

			gf_dynstrcat(&src_url, mpoint, NULL);
			gf_dynstrcat(&src_url, res_path_loc, "/");
			if (gf_file_exists(src_url))
				break;
			gf_free(src_url);
			src_url = NULL;
		}
		if (!src_url) return NULL;
	}
	if (src_url || !res_path) {
		di = NULL;
		di_len = 0;
		for (i=0; i<count; i++) {
			RTSP_DIRInfo *adi = gf_list_get(ctx->directories, i);
			u32 plen = (u32) strlen(adi->path);
			if (!src_url) {
				if (strcmp(adi->path, "$dynurl"))
					continue;
			} else {
				if (strncmp(src_url, adi->path, plen)) continue;
			}
			if (!di || (plen > di_len)) {
				di = adi;
				di_len = plen;
			}
		}
	}
	if (!di && !src_url) {
		*err_code = NC_RTSP_Not_Found;
		return NULL;
	}

	if (!di->ru && !di->rg && !com->Authorization) {
		*err_code = NC_RTSP_OK;
		if (di->mcast) *mcast_mode = MCAST_AUTHENTICATE_SETUP;
		return src_url;
	}
	//check access rights and authentication
	GF_Err creds_ok = GF_NOT_FOUND;
	char szUsrPass[200], *user=NULL, *pass=NULL;
	*err_code = NC_RTSP_Unauthorized;

	if (!com->Authorization) {
		if (src_url) gf_free(src_url);
		return NULL;
	}
	if (!strncmp(com->Authorization, "Basic ", 6)) {
		u32 len = gf_base64_decode(com->Authorization+6, (u32) strlen(com->Authorization)-6, szUsrPass, 200);
		szUsrPass[len]=0;
		char *sep = strchr(szUsrPass, ':');
		if (!sep) {
			*err_code = NC_RTSP_Bad_Request;
			if (src_url) gf_free(src_url);
			return NULL;
		}
		pass = sep+1;
		sep[0] = 0;
		user = szUsrPass;
		creds_ok = gf_creds_check_password(user, pass);
	} else {
		if (src_url) gf_free(src_url);
		return NULL;
	}
	//return unauthorized, so that we get a user name
	if (!user) return NULL;

	if (di->ru || di->rg) {
		if (!gf_creds_check_membership(user, di->ru, di->rg)) {
			*err_code = NC_RTSP_Forbidden;
			if (src_url) gf_free(src_url);
			return NULL;
		}
	}
	//if wrong creds - return forbidden if invalid user name
	if (creds_ok) {
		if (creds_ok==GF_NOT_FOUND) *err_code = NC_RTSP_Forbidden;
		if (src_url) gf_free(src_url);
		return NULL;
	}
	*err_code = NC_RTSP_OK;
	*mcast_mode = MCAST_OFF;
	if (di->mcast) {
		if (gf_creds_check_membership(user, di->mcast, di->mcast))
			*mcast_mode = MCAST_ON;
		else
			*mcast_mode = MCAST_MIRROR;
	}
	return src_url;
}

static u32 rtspout_get_ctrl_id(GF_RTSPOutSession *sess, char *ctrl)
{
	u32 stream_ctrl_id=0;
	if (ctrl && (ctrl[0]=='/')) {
		u32 len = (u32) strlen(sess->ctrl_name);
		if (!strncmp(ctrl+1, sess->ctrl_name, len)) {
			if (sscanf(ctrl+1+len, "=%d", &stream_ctrl_id)<1) {
				stream_ctrl_id=0;
			}
		}
	}
	return stream_ctrl_id;
}

static GF_Err rtspout_process_setup(GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess, char *ctrl)
{
	GF_Err e;
	char remoteIP[GF_MAX_IP_NAME_LEN];
	GF_RTPOutStream *stream = NULL;
	GF_RTSPTransport *transport = gf_list_get(sess->command->Transports, 0);
	u32 rsp_code=NC_RTSP_OK;
	Bool enable_multicast = GF_FALSE;
	Bool reset_transport_dest = GF_FALSE;

	u32 stream_ctrl_id = rtspout_get_ctrl_id(sess, ctrl);

	if (ctrl && (ctrl[0]=='/')) {
		u32 len = (u32) strlen(sess->ctrl_name);
		if (!strncmp(ctrl+1, sess->ctrl_name, len)) {
			if (sscanf(ctrl+1+len, "=%d", &stream_ctrl_id)<1) {
				stream_ctrl_id=0;
			}
		}
	}

	if (!ctrl || !transport) {
		rsp_code = NC_RTSP_Bad_Request;
	} else if (sess->sessionID && sess->command->Session && strcmp(sess->sessionID, sess->command->Session)) {
		rsp_code = NC_RTSP_Bad_Request;
	} else if (sess->sessionID && !sess->command->Session) {
		rsp_code = NC_RTSP_Not_Implemented;
	} else {
		u32 i, count = gf_list_count(sess->streams);
		for (i=0; i<count; i++) {
			stream = gf_list_get(sess->streams, i);
			if (stream_ctrl_id==stream->ctrl_id)
				break;
			stream=NULL;
		}
		if (!stream_ctrl_id || !stream)
			rsp_code = NC_RTSP_Not_Found;
	}

	if (!stream) {
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = rsp_code;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);

		gf_rtsp_session_del(sess->rtsp);
		sess->rtsp = NULL;
		rtspout_check_last_sess(ctx);
		return GF_OK;
	}

	gf_rtsp_response_reset(sess->response);
	sess->response->CSeq = sess->command->CSeq;

	stream->selected = GF_TRUE;
	if (transport && (rsp_code==NC_RTSP_OK) ) {
		if (!transport->IsInterleaved) {
			if (ctx->trp == TRP_TCP_ONLY) {
				rsp_code = NC_RTSP_Unsupported_Transport;
			} else {
				u32 st_idx = gf_list_find(sess->streams, stream);
				transport->port_first = ctx->firstport + 2 * st_idx;
				transport->port_last = transport->port_first + 1;
				if (sess->interleave)
					rsp_code = NC_RTSP_Not_Implemented;
			}
		} else {
			if (ctx->trp == TRP_UDP_ONLY) {
				rsp_code = NC_RTSP_Unsupported_Transport;
			} else if (!sess->sessionID) {
				sess->interleave = GF_TRUE;
			} else if (!sess->interleave) {
				rsp_code = NC_RTSP_Not_Implemented;
			}
		}
		transport->SSRC = rand();
		transport->is_sender = GF_TRUE;

		if (transport->IsUnicast) {
			if (transport->destination && gf_sk_is_multicast_address(transport->destination)) {
				rsp_code = NC_RTSP_Bad_Request;
			} else if (!transport->destination) {
				transport->destination = sess->peer_address;
				reset_transport_dest = GF_TRUE;
			}
			if (sess->multicast_ip)
				rsp_code = NC_RTSP_Forbidden;

			if (ctx->dst && (strstr(ctx->dst, "://127.0.0.1") || strstr(ctx->dst, "://localhost") || strstr(ctx->dst, "://::1/128") ) ) {
				if (!reset_transport_dest && transport->destination) gf_free(transport->destination);
				transport->destination = "127.0.0.1";
				reset_transport_dest = GF_TRUE;
			}
		}
		else {
			if (transport->destination && !gf_sk_is_multicast_address(transport->destination)) {
				rsp_code = NC_RTSP_Bad_Request;
			} else {
				u32 mcast_mode = ctx->mcast;
				if (sess->mcast_mode) mcast_mode = sess->mcast_mode;

				if (mcast_mode==MCAST_AUTHENTICATE_SETUP) {
					rtspout_get_local_res_path(ctx, NULL, sess->command, &rsp_code, &sess->mcast_mode);
					mcast_mode = sess->mcast_mode;
					//try once only
					if (rsp_code==NC_RTSP_Forbidden)
						mcast_mode = MCAST_OFF;
				}

				if (mcast_mode == MCAST_AUTHENTICATE_SETUP) {
					rsp_code = NC_RTSP_Unauthorized;
				}
				else if (mcast_mode != MCAST_OFF) {
					enable_multicast = GF_TRUE;
					//we don't allow seting up streams on different mcast addresses
					if (!sess->multicast_ip) {
						sess->multicast_ip = transport->destination; //detach memory
					}
					transport->source = transport->destination = sess->multicast_ip;
					reset_transport_dest = GF_TRUE;

					transport->client_port_first = 0;
					transport->client_port_last = 0;
					if (ctx->ttl) transport->TTL = ctx->ttl;
					if (!transport->TTL) transport->TTL = 1;
					stream->mcast_port = transport->port_first;
					rtspout_get_next_mcast_port(ctx, sess, &stream->mcast_port);
					transport->port_first = stream->mcast_port;
					transport->port_last = stream->mcast_port+1;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] SETUP requests a multicast to %s, not allowed\n", transport->destination));
					rsp_code = NC_RTSP_Forbidden;
				}
			}
		}
	}

	if (rsp_code != NC_RTSP_OK) {
		sess->response->ResponseCode = rsp_code;
		if (rsp_code == NC_RTSP_Unauthorized) {
			sess->response->WWW_Authenticate = gf_strdup("Basic");
			rsp_code = NC_RTSP_OK; //do not delete session
		}
	} else {
		e = gf_rtp_streamer_init_rtsp(stream->rtp, ctx->mtu, transport, ctx->ifce);
		if (e) {
			sess->response->ResponseCode = NC_RTSP_Internal_Server_Error;
		} else {
			if (!sess->sessionID)
				sess->sessionID = gf_rtsp_generate_session_id(sess->rtsp);

			sess->response->ResponseCode = NC_RTSP_OK;
			sess->response->Session = sess->sessionID;
			if (!enable_multicast) {
				gf_rtsp_get_session_ip(sess->rtsp, remoteIP);
				if (!reset_transport_dest && transport->destination) gf_free(transport->destination);
				transport->destination = NULL;
				transport->source = remoteIP;
			}
			gf_list_add(sess->response->Transports, transport);
		}

		if (sess->interleave) {
			stream->rtp_id = transport->rtpID;
			stream->rtcp_id = transport->rtcpID;
			gf_rtp_streamer_set_interleave_callbacks(stream->rtp, rtspout_interleave_packet, sess, stream);
		}
	}

	rtspout_send_response(ctx, sess);
	gf_list_reset(sess->response->Transports);
	sess->response->Session = NULL;
	if (reset_transport_dest)
		transport->destination = NULL;

	transport->source = NULL;

	if (rsp_code != NC_RTSP_OK) {
		gf_rtsp_session_del(sess->rtsp);
		sess->rtsp = NULL;
		rtspout_check_last_sess(ctx);
	}
	return GF_OK;
}


static GF_Err rtspout_process_session_signaling(GF_Filter *filter, GF_RTSPOutCtx *ctx, GF_RTSPOutSession **sess_ptr)
{
	GF_Err e;
	GF_RTSPOutSession *sess = *sess_ptr;
	char *ctrl=NULL;

	//no rtsp connection on this session
	if (!sess->rtsp) return GF_OK;

	if (sess->sdp_state==SDP_WAIT) {
		return rtspout_check_sdp(filter, sess);
	}

	if (sess->request_pending) return GF_OK;

	e = gf_rtsp_get_command(sess->rtsp, sess->command);
	if (e==GF_IP_NETWORK_EMPTY) return GF_OK;

	//delete RTSP session but not the RTP traffic
	if (e==GF_IP_CONNECTION_CLOSED) {
		gf_rtsp_session_del(sess->rtsp);
		sess->rtsp = NULL;
		return GF_OK;
	}
	if (e)
		return e;

	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSP] Got request %s from %s\n", sess->command->method, sess->peer_address));

	//restore session if needed
	if (!sess->service_name) {
		u32 i, count = gf_list_count(ctx->sessions);
		for (i=0; i<count; i++) {
			Bool swap_sess = GF_FALSE;
			GF_RTSPOutSession *a_sess = gf_list_get(ctx->sessions, i);
			if (a_sess->rtsp) continue;

			if (a_sess->sessionID && sess->command->Session && !strcmp(a_sess->sessionID, sess->command->Session) ) {
				swap_sess = GF_TRUE;
			}
			else if (!a_sess->sessionID && !sess->response->Session) {
				char *sname;
				char *cmd_path = strstr(sess->command->service_name, "://");
				if (cmd_path) cmd_path = strchr(cmd_path+3, '/');
				if (cmd_path) cmd_path++;

				sname = a_sess->service_name;
				if (!sname) sname = a_sess->server_path;

				if (sname) sname = strstr(sname, "://");
				if (sname) sname = strchr(sname+3, '/');
				if (sname) sname++;

				if (cmd_path && sname && !strncmp(cmd_path, sname, strlen(sname))) {
					if (a_sess->service_name) {
						//no session ID, match based on peer address
						if (!strcmp(sess->peer_address, a_sess->peer_address))
							swap_sess = GF_TRUE;
					} else {
						swap_sess = GF_TRUE;
					}
				}
			}

			if (!swap_sess) continue;
			gf_rtsp_command_del(a_sess->command);
			a_sess->command = sess->command;
			a_sess->rtsp = sess->rtsp;
			sess->rtsp = NULL;
			sess->command = NULL;
			memcpy(a_sess->peer_address, sess->peer_address, sizeof(char)*GF_MAX_IP_NAME_LEN);
			rtspout_del_session(NULL, sess);
			sess = a_sess;
			*sess_ptr = NULL;
			break;
		}
	}
	if (!sess->sessionID && sess->command->Session) {
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_Session_Not_Found;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);
		return GF_OK;
	}

	//process options
	if (!strcmp(sess->command->method, GF_RTSP_OPTIONS)) {
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_OK;
		sess->response->Public = "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE";
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);
		sess->response->Public = NULL;
		return GF_OK;
	}
	if (!strcmp(sess->command->method, GF_RTSP_GET_PARAMETER)
		|| !strcmp(sess->command->method, GF_RTSP_SET_PARAMETER)
	) {
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_Invalid_parameter;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);
		return GF_OK;
	}
	if (!strcmp(sess->command->method, GF_RTSP_ANNOUNCE) || !strcmp(sess->command->method, GF_RTSP_RECORD)) {
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_Not_Implemented;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);
		return GF_OK;
	}
	if (!strcmp(sess->command->method, GF_RTSP_REDIRECT)) {
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_Method_Not_Allowed;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);
		return GF_OK;
	}


	//process describe
	if (!strcmp(sess->command->method, GF_RTSP_DESCRIBE)
		|| (!sess->service_name && !sess->sessionID && !strcmp(sess->command->method, GF_RTSP_SETUP))
	) {
		u32 rsp_code = NC_RTSP_OK;
		u32 mcast_mode=MCAST_OFF;
		Bool is_setup = !strcmp(sess->command->method, GF_RTSP_SETUP);

		char *res_path = NULL;
		if (sess->command->service_name) {
			res_path = strstr(sess->command->service_name, "://");
			if (res_path) res_path = strchr(res_path+3, '/');
			if (res_path) res_path++;
		}
		if (is_setup && res_path) {
			ctrl = strrchr(res_path, '/');
			if (!ctrl) {
				gf_rtsp_response_reset(sess->response);
				sess->response->ResponseCode = NC_RTSP_Bad_Request;
				sess->response->CSeq = sess->command->CSeq;
				rtspout_send_response(ctx, sess);
				return  GF_OK;
			}
			ctrl[0] = 0;
		}

		if (res_path) {
			mcast_mode = ctx->mcast;
			//check if we have a running multicast for this resource setup after authentication
			GF_RTSPOutSession *a_sess = rtspout_locate_mcast(ctx, res_path);
			if (a_sess && (a_sess->mcast_mode==MCAST_ON)) {
				mcast_mode = MCAST_MIRROR;
			}
			if ((mcast_mode==MCAST_MIRROR) && a_sess) {
				sess->mcast_mirror = a_sess;
				if (is_setup) {
					gf_rtsp_response_reset(sess->response);
					sess->response->ResponseCode = NC_RTSP_Method_Not_Allowed;
					sess->response->CSeq = sess->command->CSeq;
					rtspout_send_response(ctx, sess);
				} else {
					rtspout_send_sdp(sess);
				}
				return GF_OK;
			}
		}

		if (!res_path) {
			rsp_code = NC_RTSP_Not_Found;
		} else if (ctx->dst) {
			if (sess->server_path) {
				char *sepp = strstr(sess->server_path, "://");
				if (sepp) sepp = strchr(sepp+3, '/');
				if (sepp) sepp++;
				if (!sepp || strcmp(sepp, res_path))
					rsp_code = NC_RTSP_Not_Found;
			}
		}
		else if ((res_path[0] == '?') || (res_path[0] == '@') ) {
			Bool ok = GF_FALSE;
			if (ctx->dynurl) {
				ok = GF_TRUE;
			} else {
				rtspout_get_local_res_path(ctx, NULL, sess->command, &rsp_code, &mcast_mode);
				if (rsp_code==NC_RTSP_OK) ok = GF_TRUE;
				else if (rsp_code==NC_RTSP_Not_Found) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTSP] client %s wants dynamic services, not enabled\n", sess->peer_address));
					rsp_code = NC_RTSP_Forbidden;
				}
			}

			if (ok) {
				char *mcast_sname = NULL;
				GF_List *paths = gf_list_new();
				rsp_code = NC_RTSP_OK;
				res_path++;
				while (res_path) {
					char sep_c=0;
					char *src_url = NULL;
					char *sep = strchr(res_path, '&');
					if (!sep) sep = strchr(res_path, '@');
					if (sep) {
						sep_c = sep[0];
						sep[0] = 0;
					}
					if (!strncmp(res_path, "name=", 5)) {
						if (!mcast_sname) mcast_sname = gf_strdup(res_path+5);
					} else if (!strchr(res_path, ':')) {
						src_url = rtspout_get_local_res_path(ctx, res_path, sess->command, &rsp_code, &mcast_mode);
					} else if (gf_filter_is_supported_source(filter, res_path, NULL)) {
						src_url = gf_strdup(res_path);
					} else if (gf_filter_url_is_filter(filter, res_path, NULL)) {
						src_url = gf_strdup(res_path);
					} else {
						rsp_code = NC_RTSP_Service_Unavailable;
					}
					if (src_url) {
						gf_list_add(paths, src_url);
					}

					if (!sep) break;
					sep[0] = sep_c;
					res_path = sep+1;
					if (rsp_code != NC_RTSP_OK)
						break;
				}
				while (gf_list_count(paths)) {
					char *src_url = gf_list_pop_front(paths);
					if (rsp_code == NC_RTSP_OK) {
						//load media service
						e = rtspout_load_media_service(filter, ctx, sess, src_url);
						if (e) {
							rsp_code = NC_RTSP_Service_Unavailable;
						}
					}
					gf_free(src_url);
				}
				gf_list_del(paths);
				sess->mcast_sname = mcast_sname;
			}
		} else {
			char *src_url = rtspout_get_local_res_path(ctx, res_path, sess->command, &rsp_code, &mcast_mode);
			if (src_url) {
				rsp_code = NC_RTSP_OK;
				//load media service
				e = rtspout_load_media_service(filter, ctx, sess, src_url);
				gf_free(src_url);
				if (e) {
					rsp_code = NC_RTSP_Service_Unavailable;
				}
			}
		}

		if (sess->service_name) gf_free(sess->service_name);
		sess->service_name = gf_strdup(sess->command->service_name);

		if (rsp_code != NC_RTSP_OK) {
			gf_rtsp_response_reset(sess->response);
			sess->response->ResponseCode = rsp_code;
			sess->response->CSeq = sess->command->CSeq;
			if (rsp_code==NC_RTSP_Unauthorized) {
				sess->response->WWW_Authenticate = gf_strdup("Basic");
			}
			rtspout_send_response(ctx, sess);
			return GF_OK;
		}
		if (ctrl) ctrl[0] = '/';

		if (is_setup && ctrl) {
			sess->setup_ctrl = gf_strdup(ctrl);
		}

		if (sess->sdp_state==SDP_LOADED) {
			//single session, create SDP
			if (is_setup) {
				return rtspout_process_setup(ctx, sess, ctrl);
			}
			rtspout_send_sdp(sess);
			return GF_OK;
		}
		//store cseq and wait for SDP to be loadable
		sess->last_cseq = sess->command->CSeq;
		sess->request_pending = GF_TRUE;
		sess->mcast_mode = mcast_mode;
		return GF_OK;
	}

	if (!sess->service_name) {
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_Bad_Request;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);
		return GF_OK;
	}

	//forbid any access to the streams, only describe is allowed
	if (sess->mcast_mirror) {
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_Unauthorized;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);
		return GF_OK;
	}

	//extract control string if any
	if (sess->service_name) {
		char *sep = strstr(sess->service_name, "://");
		if (sep) sep = strchr(sep+3, '/');
		if (sep) sep = strstr(sess->command->service_name, sep);

		if (sep) {
			ctrl = strrchr(sess->command->service_name, '/');
		}
	} else {
		ctrl = strrchr(sess->command->service_name, '/');
	}

	//process setup
	if (!strcmp(sess->command->method, GF_RTSP_SETUP)) {
		return rtspout_process_setup(ctx, sess, ctrl);
	}

	//process play
	if (!strcmp(sess->command->method, GF_RTSP_PLAY)) {
		Double start_range=-1;
		u32 rsp_code=NC_RTSP_OK;
		u32 stream_ctrl_id = rtspout_get_ctrl_id(sess, ctrl);
		if (sess->sessionID && sess->command->Session && strcmp(sess->sessionID, sess->command->Session)) {
			rsp_code = NC_RTSP_Bad_Request;
		} else if (!sess->command->Session || !sess->sessionID) {
			rsp_code = NC_RTSP_Bad_Request;
		}
		if (sess->command->Range)
			start_range = sess->command->Range->start;

		if (stream_ctrl_id) {
			rsp_code=NC_RTSP_Only_Aggregate_Operation_Allowed;
		}

		if (rsp_code!=NC_RTSP_OK) {
			gf_rtsp_response_reset(sess->response);
			sess->response->ResponseCode = rsp_code;
			sess->response->CSeq = sess->command->CSeq;
			rtspout_send_response(ctx, sess);
			return GF_OK;
		} else {
			//loop enabled, only if multicast session or single session mode
			if (ctx->loop && !sess->loop_disabled && (sess->single_session || sess->multicast_ip))
				sess->loop = GF_TRUE;

			if ((sess->play_state==2) && !sess->command->Range) {
				u64 elapsed_us = gf_sys_clock_high_res() - sess->pause_sys_clock;
				sess->pause_sys_clock = 0;
				sess->play_state = 1;
				sess->sys_clock_at_init += elapsed_us;

				gf_rtsp_response_reset(sess->response);
				sess->response->ResponseCode = NC_RTSP_OK;
				sess->response->CSeq = sess->command->CSeq;
				rtspout_send_response(ctx, sess);
			} else {
				sess->play_state = 1;
				sess->sys_clock_at_init = 0;
				sess->start_range = start_range;
				sess->last_cseq = sess->command->CSeq;
				sess->request_pending = GF_TRUE;
				rtspout_send_event(sess, GF_FALSE, GF_TRUE, start_range);
			}
		}
		return GF_OK;
	}

	//process pause (we don't implement range on pause yet)
	if (!strcmp(sess->command->method, GF_RTSP_PAUSE)) {
		if (sess->play_state!=2) {
			sess->play_state = 2;
			sess->pause_sys_clock = gf_sys_clock_high_res();
		}
		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_OK;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);
		return GF_OK;
	}
	//process teardown
	if (!strcmp(sess->command->method, GF_RTSP_TEARDOWN)) {
		sess->play_state = 0;
		rtspout_send_event(sess, GF_TRUE, GF_FALSE, 0);

		gf_rtsp_response_reset(sess->response);
		sess->response->ResponseCode = NC_RTSP_OK;
		sess->response->CSeq = sess->command->CSeq;
		rtspout_send_response(ctx, sess);

		rtspout_send_event(sess, GF_TRUE, GF_FALSE, 0);

		if (ctx->dst) {
			rtspout_del_session(filter, sess);
			rtspout_check_last_sess(ctx);
			*sess_ptr = NULL;
		}
		//keep session and setup small timeout - this speeds up seek by avoiding reloading the media source
		else {
			if (sess->sessionID) {
				gf_free(sess->sessionID);
				sess->sessionID = NULL;
			}
			sess->last_active_time = gf_sys_clock() - ctx->ms_timeout + 2000;
		}
		return GF_OK;
	}
	return GF_OK;
}

void rtspout_merge_http_tunnel(GF_Filter *filter, GF_RTSPOutCtx *ctx, GF_RTSPOutSession *sess)
{
	u32 i, count;
	char sip[GF_MAX_IP_NAME_LEN];
	const char *session_cookie = gf_rtsp_get_session_cookie(sess->rtsp);
	gf_rtsp_get_session_ip(sess->rtsp, sip);
	if (!session_cookie) {
		rtspout_del_session(NULL, sess);
		return;
	}
	count = gf_list_count(ctx->sessions);
	for (i=0; i<count; i++) {
		GF_RTSPOutSession *a_sess = gf_list_get(ctx->sessions, i);
		if (!a_sess->rtsp) continue;
		char a_sip[GF_MAX_IP_NAME_LEN];
		const char *a_session_cookie = gf_rtsp_get_session_cookie(sess->rtsp);
		if (!a_session_cookie) continue;
		if (strcmp(a_session_cookie, session_cookie)) continue;
		gf_rtsp_get_session_ip(sess->rtsp, a_sip);
		if (strcmp(a_sip, sip)) continue;

		GF_Err e = gf_rtsp_merge_tunnel(a_sess->rtsp, sess->rtsp);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] Failed to setup HTTP tunnel session from host %s: %s\n", sip, gf_error_to_string(e)));
			rtspout_del_session(filter, a_sess);
		} else {
			a_sess->is_tunnel = GF_TRUE;
		}
		rtspout_del_session(NULL, sess);
		return;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSPOut] Invalid HTTP tunnel session from host %s, discarding session\n", sip));
	rtspout_del_session(NULL, sess);
}

static GF_Err rtspout_process(GF_Filter *filter)
{
	GF_Err e=GF_OK;
	u32 i, count, now;
	GF_RTSPOutCtx *ctx = gf_filter_get_udta(filter);

	if (ctx->done)
		return GF_EOS;

	ctx->next_wake_us = 50000;
	e = rtspout_check_new_session(ctx, GF_FALSE);
	if (e==GF_IP_NETWORK_EMPTY) {
		e = GF_OK;
	}

	now = gf_sys_clock();
 	count = gf_list_count(ctx->sessions);
	for (i=0; i<count; i++) {
		GF_Err sess_err;
		GF_RTSPOutSession *sess = gf_list_get(ctx->sessions, i);
		if (!sess) break;

		sess_err = rtspout_process_session_signaling(filter, ctx, &sess);
		if ((s32)sess_err == GF_RTSP_TUNNEL_POST) {
			gf_list_rem(ctx->sessions, i);
			rtspout_merge_http_tunnel(filter, ctx, sess);
			i--;
			count--;
			continue;
		}
		if (sess_err) e |= sess_err;
		if (!sess) break;

		if (sess->play_state==1) {
			sess_err = rtspout_process_rtp(filter, ctx, sess);
			if (sess_err) e |= sess_err;
		}

		if (ctx->runfor>0) {
			s64 diff = gf_sys_clock_high_res();
			diff -= sess->sys_clock_at_init;
			diff /= 1000;
			if ((s32) diff > ctx->runfor) {
				rtspout_del_session(filter, sess);
				e = GF_EOS;
				ctx->done = GF_TRUE;
				break;
			}
		}

		if (sess->last_active_time && ctx->ms_timeout && (now > sess->last_active_time + ctx->ms_timeout)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSP] Timeout on session %s after %d ms, aborting\n", sess->service_name, now-sess->last_active_time));
			rtspout_del_session(filter, sess);
			i--;
			count--;
		}
	}
	if (!count && ctx->dst)
		return GF_EOS;

	if (e==GF_EOS) {
		if (ctx->dst)
			return GF_EOS;
		e=GF_OK;
	}
	//server mode, post task
	if (!ctx->dst)
		gf_filter_post_process_task(filter);
	if (ctx->next_wake_us) {
		gf_filter_ask_rt_reschedule(filter, ctx->next_wake_us);
	}
	return e;
}

static GF_FilterProbeScore rtspout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "rtsp://", 7)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "rtsph://", 8)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "rtsps://", 8)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static const GF_FilterCapability RTSPOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED | GF_CAPFLAG_LOADED_FILTER,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED | GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_RTSPOutCtx, _n)
static const GF_FilterArgs RTSPOutArgs[] =
{
	{ OFFS(dst), "location of destination resource", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(port), "server port", GF_PROP_UINT, "554", NULL, 0},
	{ OFFS(firstport), "port for first stream in session", GF_PROP_UINT, "6000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mtu), "size of RTP MTU in bytes", GF_PROP_UINT, "1460", NULL, 0},
	{ OFFS(ttl), "time-to-live for multicast packets (a value of 0 uses client requested TTL, or 1)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ifce), "default network interface to use", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(payt), "payload type to use for dynamic decoder configurations", GF_PROP_UINT, "96", "96-127", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mpeg4), "send all streams using MPEG-4 generic payload format if possible", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(delay), "send delay for packet (negative means send earlier)", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(tt), "time tolerance in microsecond (whenever schedule time minus realtime is below this value, the packet is sent right away)", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(runfor), "run the session for the given time in ms. A negative value means run for ever if loop or source duration, value 0 only outputs the sdp", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tso), "set timestamp offset in microseconds (negative value means random initial timestamp)", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(xps), "force parameter set injection at each SAP. If not set, only inject if different from SDP ones", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(latm), "use latm for AAC payload format", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(mounts), "list of directories to expose in server mode", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read TCP socket", GF_PROP_UINT, "10000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(maxc), "maximum number of connections", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timeout), "timeout in seconds for inactive sessions (0 disable timeout)", GF_PROP_UINT, "20", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(user_agent), "user agent string, by default solved from GPAC preferences", GF_PROP_STRING, "$GUA", NULL, 0},
	{ OFFS(close), "close RTSP connection after each request, except when RTP over RTSP is used", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(loop), "loop all streams in session (not always possible depending on source type)", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dynurl), "allow dynamic service assembly", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mcast), "control multicast setup of a session\n"
				"- off: clients are never allowed to create a multicast\n"
				"- on: clients can create multicast sessions\n"
				"- mirror: clients can create a multicast session. Any later request to the same URL will use that multicast session"
		, GF_PROP_UINT, "off", "off|on|mirror", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(quit), "exit server once first session is over (for test purposes)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(htun), "enable RTSP over HTTP tunnel", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(trp), "transport mode\n"
	"- both: allow TCP or UDP traffic\n"
	"- udp: only allow UDP traffic\n"
	"- tcp: only allow TCP traffic", GF_PROP_UINT, "both", "both|udp|tcp", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(cert), "certificate file in PEM format to use for TLS mode", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(pkey), "private key file in PEM format to use for TLS mode", GF_PROP_STRING, NULL, NULL, 0},
	{0}
};


GF_FilterRegister RTSPOutRegister = {
	.name = "rtspout",
	GF_FS_SET_DESCRIPTION("RTSP Server")
	GF_FS_SET_HELP("The RTSP server partially implements RTSP 1.0, with support for OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE and TEARDOWN.\n"
		"Multiple PLAY ranges are not supported, PLAY range end is not supported, PAUSE range is not supported.\n"
		"Only aggregated control is supported for PLAY and PAUSE, PAUSE/PLAY on single stream is not supported.\n"
		"The server only runs on TCP, and handles request in sequence: it will not probe for commands until previous response is sent.\n"
		"The server supports both RTP over UDP delivery and RTP interleaved over RTSP delivery.\n"
		"\n"
		"The scheduling algorithm and RTP options are the same as the RTP output filter, see [gpac -h rtpout](rtpout)\n"
		"The server will disconnect UDP streaming sessions if no RTCP traffic has been received for [-timeout]() seconds.\n"
		"\n"
		"The server can run over TLS by specifying [-cert]() and [-pkey](), in which case the default [-port]() is 322.\n"
		"\n"
		"# Sink mode\n"
		"The filter can work as a simple output filter by specifying the [-dst]() option:\n"
		"EX gpac -i source -o rtsp://myip/sessionname\n"
		"EX gpac -i source -o rtsp://myip/sessionname\n"
		"In this mode, only one session is possible. It is possible to [-loop]() the input source(s).\n"
		"\n"
		"# Server mode\n"
		"The filter can work as a regular RTSP server by specifying the [-mounts]() option to indicate paths of media file to be served:\n"
		"EX gpac rtspout:mounts=mydir1,mydir2\n"
		"In this case, content `RES` from any of the specified directory is exposed as `rtsp://SERVER/RES`\n"
		"\n"
		"The [-mounts]() option can also specify access rule file(s), see `gpac -h creds`. When rules are used:\n"
		"- if a directory has a `name` rule, it will be used in the URL\n"
		"- otherwise, the directory is directly available under server root `/`\n"
		"- only read access and multicast rights are checked\n"
		"EX [foodir]\n"
		"EX name=bar\n"
		"Content `RES` of this directory is exposed as `rtsp://SERVER/bar/RES`.\n"
		"  \n"
		"\n"
		"In this mode, it is possible to load any source supported by gpac by setting the option [-dynurl]().\n"
		"The expected syntax of the dynamic RTSP URLs is `rtsp://servername/?URL1[&URLN]` or `rtsp://servername/@URL1[@URLN]` \n"
		"Each URL can be absolute or local, in which case it is resolved against the mount point(s).\n"
		"EX gpac -i rtsp://localhost/?pipe://mynamepipe&myfile.mp4 [dst filters]\n"
		"The server will resolve this URL in a new session containing streams from `myfile.mp4` and streams from pipe `mynamepipe`.\n"
		"When setting [-runfor]() in server mode, the server will exit at the end of the last session being closed.\n"
		"\n"
		"The parameter `name=VAL` is reserved to assign a session name in case multicast mirroring is used.\n"
		"EX gpac -i rtsp://localhost/?name=live?pipe://mynamepipe&myfile.mp4 [dst filters]\n"
		"\n"
		"Usage of dynamic URLs can also be configured using the specific directory `$dynurl` in an access rule file.\n"
		"EX[$dynurl]\n"
		"ru=foo\n"
		"This will allow dynamic URLs only for `foo` user.\n"
		"\n"
		"Note: If the [-dynurl]() is set, it is enabled for all users, without authentication.\n"
		"\n"
		"# Multicasting\n"
		"In both modes, clients can setup multicast if the [-mcast]() option is `on` or `mirror`.\n"
		"When [-mcast]() is set to `mirror` mode, any DESCRIBE command on a resource already delivered through a multicast session will use that multicast.\n"
		"Consequently, only DESCRIBE methods are processed for such sessions, other methods will return Unauthorized.\n"
		"\n"
		"In server mode, multicast can be enabled per read directory using the `mcast` access rule of the directory configuration - see `gpac -h creds`.\n"
		"\n"
		"# HTTP Tunnel\n"
		"The server mode supports handling RTSP over HTTP tunnel by default. This can be disabled using [-htun]().\n"
		"The tunnel conforms to QT specification, and only HTTP 1.0 and 1.1 tunnels are supported.\n"
	)
	.private_size = sizeof(GF_RTSPOutCtx),
	.max_extra_pids = -1,
	.args = RTSPOutArgs,
	.probe_url = rtspout_probe_url,
	.initialize = rtspout_initialize,
	.finalize = rtspout_finalize,
	SETCAPS(RTSPOutCaps),
	.configure_pid = rtspout_configure_pid,
	.process = rtspout_process
};


const GF_FilterRegister *rtspout_register(GF_FilterSession *session)
{
	if (gf_opts_get_bool("temp", "get_proto_schemes")) {
		gf_opts_set_key("temp_out_proto", RTSPOutRegister.name, "rtsp,rtsph,rtsps");
	}
	return &RTSPOutRegister;
}

#else

const GF_FilterRegister *rtspout_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /* !defined(GPAC_DISABLE_STREAMING) */
