/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019
 *					All rights reserved
 *
 *  This file is part of GPAC / http server and output filter
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

#include <gpac/filters.h>
#include <gpac/ietf.h>
#include <gpac/config_file.h>
#include <gpac/base_coding.h>
#include <gpac/network.h>

GF_DownloadSession *gf_dm_sess_new_server(GF_Socket *server, gf_dm_user_io user_io, void *usr_cbk, GF_Err *e);

typedef struct
{
	//options
	char *dst, *user_agent;
	GF_List *mounts;
	u32 port;
	char *ifce;
	char *cache_control;
	Bool close;
	u32 block_size, maxc, maxp, timeout;
	Bool smode, hold;

	GF_Filter *filter;
	GF_Socket *server_sock;
	GF_List *sessions, *active_sessions;
	GF_List *inputs;

	u32 next_wake_us;
	char *ip;
	Bool done;

	GF_SockGroup *sg;
	Bool no_etag;

	u32 nb_connections;

} GF_HTTPOutCtx;

typedef struct
{
	GF_FilterPid *ipid;
	char *path;
	Bool dash_mode;
	char *mime;
	u32 nb_dest;

	//for PUT mode, NULL in server mode
	GF_Socket *socket;
	u64 offset_at_seg_start;
	u64 nb_write;
} GF_HTTPOutInput;


typedef struct __httpout_session
{
	GF_HTTPOutCtx *ctx;

	GF_Socket *socket;
	GF_DownloadSession *http_sess;
	char peer_address[GF_MAX_IP_NAME_LEN];

	u32 play_state;
	Double start_range;

	FILE *resource;
	char *path, *mime;
	u64 file_size, file_pos;
	u8 *buffer;
	Bool done;
	u64 last_file_modif;
	u32 retry_block;
	u32 last_active_time;
	Bool is_head;

	GF_HTTPOutInput *in_source;
} GF_HTTPOutSession;

static void httpout_reset_socket(GF_HTTPOutSession *sess)
{
	gf_sk_group_unregister(sess->ctx->sg, sess->socket);
	gf_sk_del(sess->socket);
	if (sess->in_source) sess->in_source->nb_dest--;
	sess->socket = NULL;
	sess->ctx->nb_connections--;
}

static void httpout_insert_date(u64 time, char **headers)
{
	char szDate[200];
	time_t gtime;
	struct tm *t;
	u32 sec, ms;
	gtime = time / 1000;
	sec = (u32)(time / 1000);
	ms = (u32)(time - ((u64)sec) * 1000);

	t = gf_gmtime(&gtime);
	sec = t->tm_sec;
	//see issue #859, no clue how this happened...
	if (sec > 60)
		sec = 60;
	sprintf(szDate, "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, sec, ms);

	gf_dynstrcat(headers, "Date: ", NULL);
	gf_dynstrcat(headers, szDate, NULL);
	gf_dynstrcat(headers, "\r\n", NULL);
}
static void httpout_sess_io(void *usr_cbk, GF_NETIO_Parameter *parameter)
{
	char *rsp_buf = NULL;
	const char *url;
	char *full_path=NULL;
	char *response = NULL;
	char szFmt[100];
	u8 probe_buf[5001];
	char szETag[100];
	u64 modif_time;
	const char *etag;
	GF_Err e;
	Bool not_modified = GF_FALSE;
	u32 i, count;
	GF_HTTPOutInput *source_pid = NULL;
	GF_HTTPOutSession *sess = usr_cbk;

	if (parameter->msg_type != GF_NETIO_PARSE_REPLY) {
		parameter->error = GF_BAD_PARAM;
		return;
	}
	if ((parameter->reply == GF_HTTP_GET) || (parameter->reply == GF_HTTP_HEAD)) {

	} else {
		response = "HTTP/1.1 501 Not Implemented\r\n";
		goto exit;
	}
	url = gf_dm_sess_get_resource_name(sess->http_sess);
	if (!url) e = GF_BAD_PARAM;
	if (url[0]!='/') e = GF_BAD_PARAM;

	count = gf_list_count(sess->ctx->mounts);
	for (i=0; i<count; i++) {
		char *mdir = gf_list_get(sess->ctx->mounts, i);
		full_path = gf_url_concatenate(mdir, url+1);
		if (!full_path) continue;
		if (gf_file_exists(full_path))
			break;
		gf_free(full_path);
		full_path = NULL;
	}
	if (!full_path) {
		count = gf_list_count(sess->ctx->inputs);
		for (i=0; i<count; i++) {
			GF_HTTPOutInput *in = gf_list_get(sess->ctx->inputs, i);

			if (strcmp(in->path, url)) {
				source_pid = in;
				break;
			}
		}
	}

	if (!full_path && !source_pid) {
		response = "HTTP/1.1 404 Not Found\r\n";
		goto exit;
	}
	//check ETag
	if (full_path) {
		modif_time = gf_file_modification_time(full_path);
		sprintf(szETag, LLU, modif_time);
		etag = gf_dm_sess_get_header(sess->http_sess, "If-None-Match");
	}

	if (sess->in_source) {
		sess->in_source->nb_dest--;
		sess->in_source = NULL;
	}

	if (source_pid) {
		if (sess->path) gf_free(sess->path);
		sess->path = NULL;
		sess->in_source = source_pid;
		sess->in_source->nb_dest++;
		sess->file_pos = sess->file_size = 0;
	} else if (etag && !strcmp(etag, szETag) && !sess->ctx->no_etag) {
		if (sess->path) gf_free(sess->path);
		sess->path = full_path;
		not_modified = GF_TRUE;
	} else if (sess->path && !strcmp(sess->path, full_path) && (sess->last_file_modif == modif_time) ) {
		gf_free(full_path);
		sess->file_pos = 0;
	} else {
		if (sess->path) gf_free(sess->path);
		sess->path = full_path;
		sess->resource = gf_fopen(full_path, "r");
		if (!sess->resource) {
			response = "HTTP/1.1 500 Internal Server Error\r\n";
			goto exit;
		}

		u32 read = fread(probe_buf, 1, 5000, sess->resource);
		if ((s32) read< 0) {
			response = "HTTP/1.1 500 Internal Server Error\r\n";
			goto exit;
		}
		probe_buf[read] = 0;

		gf_fseek(sess->resource, 0, SEEK_END);
		sess->file_size = gf_ftell(sess->resource);
		gf_fseek(sess->resource, 0, SEEK_SET);
		sess->file_pos = 0;

		const char * mime = gf_filter_probe_data(sess->ctx->filter, probe_buf, read);
		if (sess->mime) gf_free(sess->mime);
		sess->mime = mime ? gf_strdup(mime) : NULL;
		sess->last_file_modif = gf_file_modification_time(full_path);
	}

	if (not_modified) {
		gf_dynstrcat(&rsp_buf, "HTTP/1.1 304 Not Modified\r\n", NULL);
	} else {
		gf_dynstrcat(&rsp_buf, "HTTP/1.1 200 OK\r\n", NULL);
	}
	gf_dynstrcat(&rsp_buf, "Server: ", NULL);
	gf_dynstrcat(&rsp_buf, sess->ctx->user_agent, NULL);
	gf_dynstrcat(&rsp_buf, "\r\n", NULL);
	httpout_insert_date(gf_net_get_utc(), &rsp_buf);

	if (sess->ctx->close) {
		gf_dynstrcat(&rsp_buf, "Connection: close\r\n", NULL);
	} else {
		gf_dynstrcat(&rsp_buf, "Connection: keep-alive\r\n", NULL);
	}
	if (!not_modified) {
		if (!sess->in_source && !sess->ctx->no_etag) {
			gf_dynstrcat(&rsp_buf, "ETag: ", NULL);
			gf_dynstrcat(&rsp_buf, szETag, NULL);
			gf_dynstrcat(&rsp_buf, "\r\n", NULL);
			if (sess->ctx->cache_control) {
				gf_dynstrcat(&rsp_buf, "Cache-Control: ", NULL);
				gf_dynstrcat(&rsp_buf, sess->ctx->cache_control, NULL);
				gf_dynstrcat(&rsp_buf, "\r\n", NULL);
			}
		}
		if (sess->file_size) {
			sprintf(szFmt, LLU, sess->file_size);
			gf_dynstrcat(&rsp_buf, "Content-Length: ", NULL);
			gf_dynstrcat(&rsp_buf, szFmt, NULL);
			gf_dynstrcat(&rsp_buf, "\r\n", NULL);
		}
		if (sess->mime || sess->in_source->mime) {
			gf_dynstrcat(&rsp_buf, "Content-Type: ", NULL);
			gf_dynstrcat(&rsp_buf, sess->mime ? sess->mime : sess->in_source->mime, NULL);
			gf_dynstrcat(&rsp_buf, "\r\n", NULL);
		}

		if (sess->in_source) {
			gf_dynstrcat(&rsp_buf, "Transfer-Encoding: chunked\r\n", NULL);

		}
	}

	gf_dynstrcat(&rsp_buf, "\r\n", NULL);
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTPOut] Sending response %s to %s\n", rsp_buf, sess->peer_address));
	gf_sk_send(sess->socket, rsp_buf, strlen(rsp_buf));
	gf_free(rsp_buf);
	if (!sess->buffer) {
		sess->buffer = gf_malloc(sizeof(u8)*sess->ctx->block_size);
	}
	if (parameter->reply == GF_HTTP_HEAD) {
		sess->done = GF_FALSE;
		sess->file_pos = sess->file_size;
		sess->is_head = GF_TRUE;
	} else {
		sess->done = GF_FALSE;
		if (gf_list_find(sess->ctx->active_sessions, sess)<0) {
			gf_list_add(sess->ctx->active_sessions, sess);
			gf_sk_group_register(sess->ctx->sg, sess->socket);
		}
		sess->is_head = GF_FALSE;
	}
	sess->last_active_time = gf_sys_clock();
	return;

exit:
	gf_dynstrcat(&rsp_buf, response, NULL);
	gf_dynstrcat(&rsp_buf, "HTTP/1.1 501 Not Implemented\r\n", NULL);
	gf_dynstrcat(&rsp_buf, "Server: ", NULL);
	gf_dynstrcat(&rsp_buf, sess->ctx->user_agent, NULL);
	gf_dynstrcat(&rsp_buf, "\r\n", NULL);
	gf_dynstrcat(&rsp_buf, "Connection: close\r\n", NULL);
	gf_dynstrcat(&rsp_buf, "\r\n", NULL);
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTPOut] Sending response %s to %s\n", rsp_buf, sess->peer_address));
	gf_sk_send(sess->socket, rsp_buf, strlen(rsp_buf));
	gf_free(rsp_buf);
	httpout_reset_socket(sess);
	return;

}

static GF_Err httpout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_HTTPOutInput *pctx;
	GF_HTTPOutCtx *ctx = (GF_HTTPOutCtx *) gf_filter_get_udta(filter);

	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	pctx = gf_filter_pid_get_udta(pid);
	if (!pctx) {
		const GF_PropertyValue *p;
		GF_FilterEvent evt;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLE_PROGRESSIVE);
		if (p && p->value.boolean) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPOut] Cannot process PIDs width progressive download disabled\n"));
			return GF_BAD_PARAM;
		}

		GF_SAFEALLOC(pctx, GF_HTTPOutInput);
		pctx->ipid = pid;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);
		if (p && p->value.uint) pctx->dash_mode = GF_TRUE;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
		if (p && p->value.string) pctx->mime = gf_strdup(p->value.string);

		gf_filter_pid_set_udta(pid, pctx);
		gf_list_add(ctx->inputs, pctx);

		gf_filter_pid_init_play_event(pid, &evt, 0.0, 1.0, "HTTPOut");
		gf_filter_pid_send_event(pid, &evt);

		if (ctx->dst) {
			char *path = strstr(ctx->dst, "://");
			if (path) path = strchr(path+2, '/');
			if (path) pctx->path = gf_strdup(path);
		}
	}

	if (is_remove) {
		return GF_OK;
	}

	//we act as a server
	if (ctx->smode) {



	} else {
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}


static void httpout_check_new_session(GF_HTTPOutCtx *ctx)
{
	char peer_address[GF_MAX_IP_NAME_LEN];
	GF_HTTPOutSession *sess;
	GF_Err e;
	GF_Socket *new_conn=NULL;

	e = gf_sk_accept(ctx->server_sock, &new_conn);
	if (e==GF_IP_SOCK_WOULD_BLOCK) return;
	else if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPOut] Accept failure %s\n", gf_error_to_string(e) ));
		return;
	}
	//check max connections
	if (ctx->maxc && (ctx->nb_connections>=ctx->maxc)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[HTTPOut] Connection rejected due to too many connections\n"));
		gf_sk_del(new_conn);
		return;
	}
	gf_sk_get_remote_address(new_conn, peer_address);
	if (ctx->maxp) {
		u32 i, nb_conn=0, count = gf_list_count(ctx->sessions);
		for (i=0; i<count; i++) {
			GF_HTTPOutSession *sess = gf_list_get(ctx->sessions, i);
			if (!strcmp(sess->peer_address, peer_address)) nb_conn++;
		}
		if (nb_conn>=ctx->maxp) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[HTTPOut] Connection rejected due to too many connections from peer %s\n", peer_address));
			gf_sk_del(new_conn);
			return;
		}
	}
	GF_SAFEALLOC(sess, GF_HTTPOutSession);
	sess->socket = new_conn;
	sess->ctx = ctx;
	sess->http_sess = gf_dm_sess_new_server(new_conn, httpout_sess_io, sess, &e);
	if (!sess->http_sess) {
		gf_sk_del(new_conn);
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTPOut] Failed to create HTTP server session from %s: %s\n", sess->peer_address, gf_error_to_string(e) ));
		gf_free(sess);
		return;
	}
	ctx->nb_connections++;

	gf_list_add(ctx->sessions, sess);
	gf_list_add(ctx->active_sessions, sess);
	gf_sk_group_register(ctx->sg, sess->socket);
	
	gf_sk_set_buffer_size(new_conn, GF_FALSE, ctx->block_size);
	gf_sk_set_buffer_size(new_conn, GF_TRUE, ctx->block_size);
	strcpy(sess->peer_address, peer_address);

//	gf_sk_set_block_mode(sess->socket, GF_FALSE);

	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTPOut] Accepting new connection from %s\n", sess->peer_address));
	ctx->next_wake_us = 0;
}

static GF_Err httpout_initialize(GF_Filter *filter)
{
	char szIP[1024];
	GF_Err e;
	u16 port;
	char *ip;
	GF_HTTPOutCtx *ctx = (GF_HTTPOutCtx *) gf_filter_get_udta(filter);

	ctx->sessions = gf_list_new();
	ctx->active_sessions = gf_list_new();
	ctx->inputs = gf_list_new();
	ctx->filter = filter;
	ctx->sg = gf_sk_group_new();

	port = ctx->port;
	ip = ctx->ifce;

	if (!ctx->dst) {
		if (!ctx->mounts) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPOut] No root dir(s) for server, cannot run\n" ));
			return GF_BAD_PARAM;
		}
		gf_filter_make_sticky(filter);
	} else {
		char *sep = NULL;
		if (!strncmp(ctx->dst, "http://", 7)) {
			sep = strchr(ctx->dst+7, '/');
		} else if (!strncmp(ctx->dst, "https://", 8)) {
			sep = strchr(ctx->dst+8, '/');
		}
		if (sep) {
			strncpy(szIP, ctx->dst+7, sep-ctx->dst-7);
			sep = strchr(szIP, ':');
			if (sep) {
				port = atoi(sep+1);
				if (!port) port = ctx->port;
				sep[0] = 0;
			}
			if (strlen(szIP)) ip = szIP;
		}
	}

	if (ip)
		ctx->ip = gf_strdup(ip);

	if (ctx->cache_control) {
		if (!strcmp(ctx->cache_control, "none")) ctx->no_etag = GF_TRUE;
	}
	ctx->server_sock = gf_sk_new(GF_SOCK_TYPE_TCP);
	e = gf_sk_bind(ctx->server_sock, NULL, port, ip, 0, GF_SOCK_REUSE_PORT);
	if (!e) e = gf_sk_listen(ctx->server_sock, ctx->maxc);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPOut] failed to start server on port %d: %s\n", ctx->port, gf_error_to_string(e) ));
		return e;
	}
	gf_sk_group_register(ctx->sg, ctx->server_sock);

	gf_sk_server_mode(ctx->server_sock, GF_TRUE);
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTPOut] Server running on port %d\n", ctx->port));
	gf_filter_post_process_task(filter);
	return GF_OK;
}

static void httpout_del_session(GF_HTTPOutSession *s)
{
	gf_list_del_item(s->ctx->active_sessions, s);
	gf_list_del_item(s->ctx->sessions, s);
	if (s->socket) gf_sk_del(s->socket);
	if (s->buffer) gf_free(s->buffer);
	if (s->path) gf_free(s->path);
	if (s->mime) gf_free(s->mime);
	if (s->http_sess) gf_dm_sess_del(s->http_sess);
	if (s->resource) gf_fclose(s->resource);
	gf_free(s);
}

static void httpout_finalize(GF_Filter *filter)
{
	GF_HTTPOutCtx *ctx = (GF_HTTPOutCtx *) gf_filter_get_udta(filter);

	while (gf_list_count(ctx->sessions)) {
		GF_HTTPOutSession *tmp = gf_list_get(ctx->sessions, 0);
		httpout_del_session(tmp);
	}
	gf_list_del(ctx->sessions);

	gf_sk_del(ctx->server_sock);
	if (ctx->ip) gf_free(ctx->ip);	
}

static void httpout_process_session(GF_Filter *filter, GF_HTTPOutCtx *ctx, GF_HTTPOutSession *sess)
{
	u32 read;
	GF_Err e = GF_OK;
	//read request and process headers
	if (sess->http_sess) {
		if (!gf_sk_group_sock_is_set(ctx->sg, sess->socket, GF_SK_SELECT_READ))
			return;

		e = gf_dm_sess_process(sess->http_sess);
		//no support for request pipeline yet, just remove the downloader session until done
		gf_dm_sess_del(sess->http_sess);
		sess->http_sess = NULL;
		if (e==GF_IP_CONNECTION_CLOSED) {
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTPOut] Connection to %s closed\n", sess->peer_address));
			httpout_reset_socket(sess);
			return;
		}
		ctx->next_wake_us = 0;
	}

	if (!sess->socket) return;
	if (sess->done) return;

	if (!gf_sk_group_sock_is_set(ctx->sg, sess->socket, GF_SK_SELECT_WRITE))
		return;

	if (sess->file_pos < sess->file_size) {
		ctx->next_wake_us = 0;
		if (sess->retry_block) {
			read = sess->retry_block;
		} else {
			read = fread(sess->buffer, 1, sess->ctx->block_size, sess->resource);
		}

		e = gf_sk_send(sess->socket, sess->buffer, read);
		sess->last_active_time = gf_sys_clock();

		if (e==GF_IP_SOCK_WOULD_BLOCK) {
			sess->retry_block = read;
			return;
		}

		sess->retry_block = 0;
		sess->file_pos += read;
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPOut] Error sending data to %s: %s\n", sess->peer_address, gf_error_to_string(e) ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTPOut] sending data to %s: "LLU" / "LLU" bytes\n", sess->peer_address, sess->file_pos, sess->file_size ));

		}
		return;
	}

	if (!sess->is_head) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTPOut] Done sending %s to %s ("LLU"/"LLU" bytes)\n", sess->path, sess->peer_address, sess->file_pos, sess->file_size));
	}

	sess->file_pos = sess->file_size;
	//keep resource active
	sess->done = GF_TRUE;

	if (ctx->close) {
		httpout_reset_socket(sess);
	} else {
		//we keep alive, recreate an dm sess
		sess->http_sess = gf_dm_sess_new_server(sess->socket, httpout_sess_io, sess, &e);
		if (e) {
			httpout_reset_socket(sess);
		}
	}
}

static void httpout_open_input(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in, const char *name)
{
	if (in->socket) {

	} else {
	}
}

static void httpout_close_input(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in)
{
	if (in->socket) {
		gf_sk_send(in->socket, "0\r\n\r\n", 5);
	} else {
		u32 i, count = gf_list_count(ctx->active_sessions);
		for (i=0; i<count; i++) {
			GF_HTTPOutSession *sess = gf_list_get(ctx->active_sessions, i);
			if (sess->in_source != in) continue;

			gf_sk_send(sess->socket, "0\r\n\r\n", 5);
		}
	}
	in->nb_write = 0;
}
u32 httpout_write_input(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in, const u8 *pck_data, u32 pck_size)
{
	char szHdr[100];
	u32 len, out=0;
	sprintf(szHdr, "%X\r\n", pck_size);
	len = strlen(szHdr);

	if (in->socket) {
		out = pck_size;
		gf_sk_send(in->socket, szHdr, len);
		gf_sk_send(in->socket, pck_data, pck_size);
		gf_sk_send(in->socket, "\r\n", 2);
	} else {
		u32 i, count = gf_list_count(ctx->active_sessions);
		for (i=0; i<count; i++) {
			GF_HTTPOutSession *sess = gf_list_get(ctx->active_sessions, i);
			if (sess->in_source != in) continue;
			out = pck_size;

			gf_sk_send(sess->socket, szHdr, len);
			gf_sk_send(sess->socket, pck_data, pck_size);
			gf_sk_send(sess->socket, "\r\n", 2);
		}
	}
	ctx->next_wake_us = 0;
	return out;
}

static void httpout_process_inputs(GF_HTTPOutCtx *ctx)
{
	u32 i, nb_eos=0, count = gf_list_count(ctx->inputs);
	for (i=0; i<count; i++) {
		Bool start, end;
		const u8 *pck_data;
		u32 pck_size, nb_write;
		GF_FilterPacket *pck;
		GF_HTTPOutInput *in = gf_list_get(ctx->inputs, i);
		if (ctx->smode && ctx->hold && !in->nb_dest) continue;

		pck = gf_filter_pid_get_packet(in->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(in->ipid)) nb_eos++;
			continue;
		}

		//check if end of pid
		if (!pck) {
			if (gf_filter_pid_is_eos(in->ipid)) {
				nb_eos++;
				if (in->dash_mode && in->socket) {
					GF_FilterEvent evt;
					GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, in->ipid);
					evt.seg_size.seg_url = NULL;

					if (in->dash_mode==1) {
						evt.seg_size.is_init = GF_TRUE;
						in->dash_mode = 2;
						evt.seg_size.media_range_start = 0;
						evt.seg_size.media_range_end = 0;
						gf_filter_pid_send_event(in->ipid, &evt);
					} else {
						evt.seg_size.is_init = GF_FALSE;
						evt.seg_size.media_range_start = in->offset_at_seg_start;
						evt.seg_size.media_range_end = in->nb_write;
						gf_filter_pid_send_event(in->ipid, &evt);
					}
				}
				httpout_close_input(ctx, in);
			}
			continue;
		}

		gf_filter_pck_get_framing(pck, &start, &end);

		if (in->dash_mode) {
			const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p) {
				GF_FilterEvent evt;

				GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, in->ipid);
				evt.seg_size.seg_url = NULL;

				if (in->dash_mode==1) {
					evt.seg_size.is_init = GF_TRUE;
					in->dash_mode = 2;
					evt.seg_size.media_range_start = 0;
					evt.seg_size.media_range_end = 0;
					gf_filter_pid_send_event(in->ipid, &evt);
				} else {
					evt.seg_size.is_init = GF_FALSE;
					evt.seg_size.media_range_start = in->offset_at_seg_start;
					evt.seg_size.media_range_end = in->nb_write;
					in->offset_at_seg_start = evt.seg_size.media_range_end;
					gf_filter_pid_send_event(in->ipid, &evt);
				}
				if ( gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME))
					start = GF_TRUE;
			}
		}

		if (start) {
			const GF_PropertyValue *ext, *fnum, *fname;
			Bool explicit_overwrite = GF_FALSE;
			const char *name = NULL;
			fname = ext = NULL;
			//file num increased per packet, open new file
			fnum = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (fnum) {
				fname = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_OUTPATH);
				ext = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_FILE_EXT);
				if (!fname) name = ctx->dst;
			}
			//filename change at packet start, open new file
			if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
			if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PID_OUTPATH);
			if (!ext) ext = gf_filter_pck_get_property(pck, GF_PROP_PID_FILE_EXT);
			if (fname) name = fname->value.string;

			if (end && gf_filter_pck_get_seek_flag(pck))
				explicit_overwrite = GF_TRUE;

			httpout_open_input(ctx, in, name);
		}

		pck_data = gf_filter_pck_get_data(pck, &pck_size);
		if (in->socket || ctx->smode) {
			GF_FilterFrameInterface *hwf = gf_filter_pck_get_frame_interface(pck);
			if (pck_data) {
				nb_write = httpout_write_input(ctx, in, pck_data, pck_size);
				if (nb_write!=pck_size) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[HTTPOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
				}
				in->nb_write += nb_write;
			} else if (hwf) {
				u32 w, h, stride, stride_uv, pf;
				u32 nb_planes, uv_height;
				const GF_PropertyValue *p;
				p = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_WIDTH);
				w = p ? p->value.uint : 0;
				p = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_HEIGHT);
				h = p ? p->value.uint : 0;
				p = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_PIXFMT);
				pf = p ? p->value.uint : 0;

				stride = stride_uv = 0;

				if (gf_pixel_get_size_info(pf, w, h, NULL, &stride, &stride_uv, &nb_planes, &uv_height) == GF_TRUE) {
					u32 i;
					for (i=0; i<nb_planes; i++) {
						u32 j, write_h, lsize;
						const u8 *out_ptr;
						u32 out_stride = i ? stride_uv : stride;
						GF_Err e = hwf->get_plane(hwf, i, &out_ptr, &out_stride);
						if (e) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[HTTPOut] Failed to fetch plane data from hardware frame, cannot write\n"));
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
							nb_write = (u32) httpout_write_input(ctx, in, out_ptr, lsize);
							if (nb_write!=lsize) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[HTTPOut] Write error, wrote %d bytes but had %d to write\n", nb_write, lsize));
							}
							in->nb_write += nb_write;
							out_ptr += out_stride;
						}

					}
				}
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[HTTPOut] No data associated with packet, cannot write\n"));
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[HTTPOut] output file handle is not opened, discarding %d bytes\n", pck_size));
		}
		gf_filter_pid_drop_packet(in->ipid);
		if (end) {
			httpout_close_input(ctx, in);
		}
	}

	if (count && (nb_eos==count)) {
		ctx->done = GF_TRUE;
	}
}

static GF_Err httpout_process(GF_Filter *filter)
{
	GF_Err e=GF_OK;
	u32 i, count;
	GF_HTTPOutCtx *ctx = gf_filter_get_udta(filter);

	if (ctx->done)
		return GF_EOS;

	//wakeup every 50ms when inactive
	ctx->next_wake_us = 50000;

	e = gf_sk_group_select(ctx->sg, 10);
	if (e==GF_OK) {
		if (gf_sk_group_sock_is_set(ctx->sg, ctx->server_sock, GF_SK_SELECT_READ)) {
			httpout_check_new_session(ctx);
		}

		count = gf_list_count(ctx->active_sessions);
		for (i=0; i<count; i++) {
			GF_HTTPOutSession *sess = gf_list_get(ctx->active_sessions, i);
			//push
			if (sess->in_source) continue;

			//regular download
			httpout_process_session(filter, ctx, sess);
			//closed, remove
			if (! sess->socket) {
				httpout_del_session(sess);
				i--;
				count--;
			}
		}
	}

	httpout_process_inputs(ctx);

	if (ctx->timeout) {
		count = gf_list_count(ctx->active_sessions);
		for (i=0; i<count; i++) {
			u32 diff_sec;
			GF_HTTPOutSession *sess = gf_list_get(ctx->active_sessions, i);
			if (!sess->done) continue;

			diff_sec = (u32) (gf_sys_clock() - sess->last_active_time)/1000;
			if (diff_sec>ctx->timeout) {
				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTPOut] Timeout for peer %s after %d sec, closing connection\n", sess->peer_address, diff_sec ));

				httpout_reset_socket(sess);

				httpout_del_session(sess);
				i--;
				count--;
			}
		}
	}

	if (e==GF_EOS) {
		if (ctx->dst) return GF_EOS;
		e=GF_OK;
	}

	if (ctx->next_wake_us)
		gf_filter_ask_rt_reschedule(filter, ctx->next_wake_us);

	return e;
}

static GF_FilterProbeScore httpout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "http://", 7)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static const GF_FilterCapability HTTPOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_FILE_EXT, "*"),
};


#define OFFS(_n)	#_n, offsetof(GF_HTTPOutCtx, _n)
static const GF_FilterArgs HTTPOutArgs[] =
{
	{ OFFS(dst), "location of destination file - see filter help ", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(port), "server port", GF_PROP_UINT, "80", NULL, 0},
	{ OFFS(ifce), "default network inteface to use", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mounts), "list of directories to expose in server mode", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read TCP socket", GF_PROP_UINT, "4096", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(user_agent), "user agent string, by default solved from GPAC preferences", GF_PROP_STRING, "$GUA", NULL, 0},
	{ OFFS(close), "close HTTP connection after each request", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(maxc), "maximum number of connections from all user", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(maxp), "maximum number of connections for one peer, 0 is unlimited", GF_PROP_UINT, "6", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(cache_control), "Cache-Control string to add; `none` disable ETag", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(smode), "single mode HTTP output", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(hold), "hold packets until clients connect in single mode", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timeout), "timeout in seconds for persistent connections; 0 disable timeout", GF_PROP_UINT, "4", NULL, GF_FS_ARG_HINT_ADVANCED},

	{0}
};


GF_FilterRegister HTTPOutRegister = {
	.name = "httpout",
	GF_FS_SET_DESCRIPTION("HTTP output or server")
	GF_FS_SET_HELP("The HTTP output filter can act as both an HTTP server or as a PUT/POST HTTP client\n"\
	)
	.private_size = sizeof(GF_HTTPOutCtx),
	.max_extra_pids = -1,
	.args = HTTPOutArgs,
	.probe_url = httpout_probe_url,
	.initialize = httpout_initialize,
	.finalize = httpout_finalize,
	SETCAPS(HTTPOutCaps),
	.configure_pid = httpout_configure_pid,
	.process = httpout_process
};


const GF_FilterRegister *httpout_register(GF_FilterSession *session)
{
	return &HTTPOutRegister;
}

