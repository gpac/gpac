/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019-2021
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

//socket and SSL context ownership is transfered to the download session object
GF_DownloadSession *gf_dm_sess_new_server(GF_Socket *server, void *ssl_ctx, gf_dm_user_io user_io, void *usr_cbk, GF_Err *e);
GF_DownloadSession *gf_dm_sess_new_subsession(GF_DownloadSession *sess, u32 stream_id, void *usr_cbk, GF_Err *e);
u32 gf_dm_sess_subsession_count(GF_DownloadSession *);


GF_Err gf_dm_sess_send(GF_DownloadSession *sess, u8 *data, u32 size);
void gf_dm_sess_clear_headers(GF_DownloadSession *sess);
void  gf_dm_sess_set_header(GF_DownloadSession *sess, const char *name, const char *value);

GF_Err gf_dm_sess_send_reply(GF_DownloadSession *sess, u32 reply_code, const char *response_body, Bool no_body);
void gf_dm_sess_server_reset(GF_DownloadSession *sess);
Bool gf_dm_sess_is_h2(GF_DownloadSession *sess);
void gf_dm_sess_flush_h2(GF_DownloadSession *sess);

#ifdef GPAC_HAS_SSL

void *gf_ssl_new(void *ssl_server_ctx, GF_Socket *client_sock, GF_Err *e);
void gf_ssl_del(void *ssl_ctx);
void *gf_ssl_server_context_new(const char *cert, const char *key);
void gf_ssl_server_context_del(void *ssl_server_ctx);
Bool gf_ssl_init_lib();

#endif

enum
{
	MODE_DEFAULT=0,
	MODE_PUSH,
	MODE_SOURCE,
};

enum
{
	CORS_AUTO=0,
	CORS_OFF,
	CORS_ON,
};

typedef struct
{
	//options
	char *dst, *user_agent, *ifce, *cache_control, *ext, *mime, *wdir, *cert, *pkey, *reqlog;
	GF_PropStringList rdirs;
	Bool close, hold, quit, post, dlist, ice;
	u32 port, block_size, maxc, maxp, timeout, hmode, sutc, cors, max_client_errors;

	//internal
	GF_Filter *filter;
	GF_Socket *server_sock;
	GF_List *sessions, *active_sessions;
	GF_List *inputs;

	u32 next_wake_us;
	char *ip;
	Bool done;
	struct __httpout_input *dst_in;

	GF_SockGroup *sg;
	Bool no_etag;

	u32 nb_connections;

	GF_FilterCapability in_caps[2];
	char szExt[10];

	//set to true when no mounted dirs and not push mode
	Bool single_mode;

	void *ssl_ctx;

	u64 req_id;
	Bool log_record;
} GF_HTTPOutCtx;

typedef struct __httpout_input
{
	GF_HTTPOutCtx *ctx;
	GF_FilterPid *ipid;
	char *path;
	Bool dash_mode;
	char *mime;
	u32 nb_dest;
	Bool hold;

	Bool is_open, done, is_delete;
	Bool patch_blocks;
	GF_List *file_deletes;

	//for PUT mode, NULL in server mode
	GF_DownloadSession *upload;
	Bool is_h2;
	u32 cur_header;

	u64 offset_at_seg_start;
	u64 nb_write, write_start_range, write_end_range;
	char range_hdr[100];

	//for server mode, recording
	char *local_path;
	FILE *resource;

	FILE *hls_chunk;
	char *hls_chunk_path, *hls_chunk_local_path;

	u8 *tunein_data;
	u32 tunein_data_size;
    
    Bool force_dst_name;
    Bool in_error;


} GF_HTTPOutInput;

typedef struct
{
	s64 start;
	s64 end;
} HTTByteRange;

typedef struct __httpout_session
{
	GF_HTTPOutCtx *ctx;

	GF_Socket *socket;
	GF_DownloadSession *http_sess;
	char peer_address[GF_MAX_IP_NAME_LEN];

	Bool headers_done;

	u32 play_state;
	Double start_range;

	FILE *resource;
	char *path, *mime;
	u64 file_size, file_pos, nb_bytes, bytes_in_req;
	u8 *buffer;
	Bool done;
	u64 last_file_modif;

	u64 req_start_time;
	u64 last_active_time;
	Bool file_in_progress;
	Bool use_chunk_transfer;
	u32 put_in_progress;
	//for upload only: 0 not an upload, 1 creation, 2: update
	u32 upload_type;
	u64 content_length;
	GF_FilterPid *opid;
	Bool reconfigure_output;

	GF_HTTPOutInput *in_source;
	Bool send_init_data;
	Bool in_source_is_ll_hls_chunk;

	u32 nb_ranges, alloc_ranges, range_idx;
	HTTByteRange *ranges;

	Bool do_log;
	u64 req_id;
	u32 method_type, reply_code, nb_consecutive_errors;

	Bool is_h2;
	Bool sub_sess_pending;
	Bool canceled;

	Bool force_destroy;
} GF_HTTPOutSession;

static void httpout_close_session(GF_HTTPOutSession *sess)
{
	Bool last_connection = GF_TRUE;
	if (!sess->http_sess) return;

	if (sess->is_h2) {
		u32 nb_sub_sess = gf_dm_sess_subsession_count(sess->http_sess);
		if (nb_sub_sess > 1) {
			last_connection = GF_FALSE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPOut] %d sub-sessions still active in connection to %s, keeping alive\n", nb_sub_sess-1, sess->peer_address ));
		}
		else {
			gf_dm_sess_flush_h2(sess->http_sess);
		}
	}
	if (last_connection) {
		assert(sess->ctx->nb_connections);
		sess->ctx->nb_connections--;

		gf_sk_group_unregister(sess->ctx->sg, sess->socket);
	}

	gf_dm_sess_del(sess->http_sess);
	sess->http_sess = NULL;
	sess->socket = NULL;

	if (sess->in_source) sess->in_source->nb_dest--;
}

static void httpout_format_date(u64 time, char szDate[200], Bool for_listing)
{
	time_t gtime;
	struct tm *t;
	const char *wday, *month;
	u32 sec;
	gtime = time / 1000;
	t = gf_gmtime(&gtime);
	sec = t->tm_sec;
	//see issue #859, no clue how this happened...
	if (sec > 60)
		sec = 60;
	switch (t->tm_wday) {
	case 1: wday = "Mon"; break;
	case 2: wday = "Tue"; break;
	case 3: wday = "Wed"; break;
	case 4: wday = "Thu"; break;
	case 5: wday = "Fri"; break;
	case 6: wday = "Sat"; break;
	default: wday = "Sun"; break;
	}
	switch (t->tm_mon) {
	case 1: month = "Feb"; break;
	case 2: month = "Mar"; break;
	case 3: month = "Apr"; break;
	case 4: month = "May"; break;
	case 5: month = "Jun"; break;
	case 6: month = "Jul"; break;
	case 7: month = "Aug"; break;
	case 8: month = "Sep"; break;
	case 9: month = "Oct"; break;
	case 10: month = "Nov"; break;
	case 11: month = "Dec"; break;
	default: month = "Jan"; break;

	}

	if (for_listing) {
		sprintf(szDate, "%02d-%s-%d %02d:%02d:%02d", t->tm_mday, month, 1900 + t->tm_year, t->tm_hour, t->tm_min, sec);
	} else {
		sprintf(szDate, "%s, %02d %s %d %02d:%02d:%02d GMT", wday, t->tm_mday, month, 1900 + t->tm_year, t->tm_hour, t->tm_min, sec);
	}
}

static Bool httpout_dir_file_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info, Bool is_dir)
{
	char szFmt[200];
	u64 size;
	u32 name_len;
	char *unit=NULL;
	char **listing = (char **) cbck;

	if (file_info && (file_info->hidden || file_info->system))
		return GF_FALSE;

	if (is_dir)
		gf_dynstrcat(listing, "+  <a href=\"", NULL);
	else
		gf_dynstrcat(listing, "   <a href=\"", NULL);

	name_len = (u32) strlen(item_name);
	if (is_dir) name_len++;
	gf_dynstrcat(listing, item_name, NULL);
	if (is_dir) gf_dynstrcat(listing, "/", NULL);
	gf_dynstrcat(listing, "\">", NULL);
	gf_dynstrcat(listing, item_name, NULL);
	if (is_dir) gf_dynstrcat(listing, "/", NULL);
	gf_dynstrcat(listing, "</a>", NULL);
	while (name_len<60) {
		name_len++;
		gf_dynstrcat(listing, " ", NULL);
	}
	if (file_info) {
		char szDate[200];
		httpout_format_date(file_info->last_modified*1000, szDate, GF_TRUE);
		gf_dynstrcat(listing, szDate, NULL);
	}

	if (is_dir || !file_info) {
		gf_dynstrcat(listing, "    -\n", NULL);
		return GF_FALSE;
	}
	size = file_info->size;
	if (size<1000) unit="";
	else if (size<1000000) { unit="K"; size/=1000; }
	else if (size<1000000000) { unit="M"; size/=1000000; }
	else if (size<1000000000000) { unit="G"; size/=1000000000; }
	gf_dynstrcat(listing, "    ", NULL);
	sprintf(szFmt, LLU"%s\n", size, unit);
	gf_dynstrcat(listing, szFmt, NULL);

	return GF_FALSE;
}
static Bool httpout_dir_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	return httpout_dir_file_enum(cbck, item_name, item_path, file_info, GF_TRUE);
}
static Bool httpout_file_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	return httpout_dir_file_enum(cbck, item_name, item_path, file_info, GF_FALSE);
}

static char *httpout_create_listing(GF_HTTPOutCtx *ctx, char *full_path)
{
	char szHost[GF_MAX_IP_NAME_LEN];
	char *has_par, *dir;
	u32 i, count = ctx->rdirs.nb_items;
	char *listing = NULL;
	char *name = full_path;

	if (full_path && (full_path[0]=='.'))
		name++;

	gf_dynstrcat(&listing, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n<html>\n<head>\n<title>Index of ", NULL);
	gf_dynstrcat(&listing, name, NULL);
	gf_dynstrcat(&listing, "</title>\n</head>\n<body><h1>Index of ", NULL);
	gf_dynstrcat(&listing, name, NULL);
	gf_dynstrcat(&listing, "</h1>\n<pre>Name                                                                Last modified      Size\n<hr>\n", NULL);

	if (!full_path) {
		has_par=NULL;
	} else {
		u32 len = (u32) strlen(full_path);
		if (len && strchr("/\\", full_path[len-1]))
			full_path[len-1] = 0;
		has_par = strrchr(full_path, '/');
	}
	if (has_par) {
		u8 c = has_par[1];
		has_par[1] = 0;

		//retranslate server root
		gf_dynstrcat(&listing, ".. <a href=\"", NULL);
		for (i=0; i<count; i++) {
			dir = ctx->rdirs.vals[i];
			u32 dlen = (u32) strlen(dir);
			if (!strncmp(dir, name, dlen) && ((name[dlen]=='/') || (name[dlen]==0))) {
				gf_dynstrcat(&listing, "/", NULL);
				if (count==1) name = NULL;
				break;
			}
		}

		if (name)
			gf_dynstrcat(&listing, name, NULL);

		gf_dynstrcat(&listing, "\">Parent Directory</a>\n", NULL);
		has_par[1] = c;
	}

	if (!full_path || !strlen(full_path)) {
		count = ctx->rdirs.nb_items;
		if (count==1) {
			dir = ctx->rdirs.vals[0];
			gf_enum_directory(dir, GF_TRUE, httpout_dir_enum, &listing, NULL);
			gf_enum_directory(dir, GF_FALSE, httpout_file_enum, &listing, NULL);
		} else {
			for (i=0; i<count; i++) {
				dir = ctx->rdirs.vals[i];
				httpout_dir_file_enum(&listing, dir, NULL, NULL, GF_TRUE);
			}
		}
	} else {
		Bool insert_root = GF_FALSE;
		if (count>1) {
			for (i=0; i<count; i++) {
				dir = ctx->rdirs.vals[i];
				if (!strcmp(full_path, dir)) {
					insert_root=GF_TRUE;
					break;
				}
			}
			if (insert_root) {
				gf_dynstrcat(&listing, ".. <a href=\"/\">Parent Directory</a>                                 -\n", NULL);
			}
		}
		gf_enum_directory(full_path, GF_TRUE, httpout_dir_enum, &listing, NULL);
		gf_enum_directory(full_path, GF_FALSE, httpout_file_enum, &listing, NULL);
	}

	gf_dynstrcat(&listing, "\n<hr></pre>\n<address>", NULL);
	gf_dynstrcat(&listing, ctx->user_agent, NULL);
	gf_sk_get_host_name(szHost);
	gf_dynstrcat(&listing, " at ", NULL);
	gf_dynstrcat(&listing, szHost, NULL);
	gf_dynstrcat(&listing, " Port ", NULL);
	sprintf(szHost, "%d", ctx->port);
	gf_dynstrcat(&listing, szHost, NULL);
	gf_dynstrcat(&listing, "</address>\n</body></html>", NULL);
	return listing;
}


static void httpout_set_local_path(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in)
{
	char *dir;
	u32 len;
	assert(in->path);
	//not recording
	if (!ctx->rdirs.nb_items) return;

	dir = ctx->rdirs.vals[0];
	if (!dir) return;
	len = (u32) strlen(dir);
	if (in->local_path) gf_free(in->local_path);
	in->local_path = NULL;
	gf_dynstrcat(&in->local_path, dir, NULL);
	if (!strchr("/\\", dir[len-1]))
		gf_dynstrcat(&in->local_path, "/", NULL);
    if (in->path[0]=='/')
        gf_dynstrcat(&in->local_path, in->path+1, NULL);
    else
        gf_dynstrcat(&in->local_path, in->path, NULL);
}

static Bool httpout_sess_parse_range(GF_HTTPOutSession *sess, char *range)
{
	Bool request_ok = GF_TRUE;;
	u32 i;
	Bool has_open_start=GF_FALSE;
	Bool has_file_end=GF_FALSE;
	u64 known_file_size;
	sess->nb_ranges = 0;
	sess->nb_bytes = 0;
	sess->range_idx = 0;
	if (!range) return GF_TRUE;

	if (sess->in_source && !sess->ctx->rdirs.nb_items)
		return GF_FALSE;

	while (range) {
		char *sep;
		u32 len;
		s64 start, end;
		char *next = strchr(range, ',');
		if (next) next[0] = 0;

		while (range[0] == ' ') range++;

		//unsupported unit
		if (strncmp(range, "bytes=", 6)) {
			return GF_FALSE;
		}
		range += 6;
		sep = strchr(range, '/');
		if (sep) sep[0] = 0;
		len = (u32) strlen(range);
		start = end = -1;
		if (!len) {
			request_ok = GF_FALSE;
		}
		//end range only
		else if (range[0] == '-') {
			if (has_file_end)
				request_ok = GF_FALSE;
			has_file_end = GF_TRUE;
			start = -1;
			if (sscanf(range+1, LLD, &end) != 1)
				request_ok = GF_FALSE;
		}
		//start -> EOF
		else if (range[len-1] == '-') {
			if (has_open_start)
				request_ok = GF_FALSE;
			has_open_start = GF_TRUE;
			end = -1;
			if (sscanf(range, LLD"-", &start) != 1)
				request_ok = GF_FALSE;
		} else {
			if (sscanf(range, LLD"-"LLD, &start, &end) != 2)
				request_ok = GF_FALSE;
		}
		if ((start==-1) && (end==-1)) {
			request_ok = GF_FALSE;
		}

		if (request_ok) {
			if (sess->nb_ranges >= sess->alloc_ranges) {
				sess->alloc_ranges = sess->nb_ranges + 1;
				sess->ranges = gf_realloc(sess->ranges, sizeof(HTTByteRange)*sess->alloc_ranges);
			}
			sess->ranges[sess->nb_ranges].start = start;
			sess->ranges[sess->nb_ranges].end = end;
			sess->nb_ranges++;
		}

		if (sep) sep[0] = '/';
		if (!next) break;
		next[0] = ',';
		range = next+1;
		if (!request_ok) break;
	}
	if (!request_ok) return GF_FALSE;

	if (sess->in_source && !sess->resource) {
		//cannot fetch end of file it is not yet known !
		if (has_file_end) return GF_FALSE;
		known_file_size = sess->in_source->nb_write;
	} else {
		known_file_size = sess->file_size;
	}
	sess->bytes_in_req = 0;
	for (i=0; i<sess->nb_ranges; i++) {
		if (sess->ranges[i].start>=0) {
			//if start, end is a pos in bytes in size (0-based)
			if (sess->ranges[i].end==-1) {
				sess->ranges[i].end = known_file_size-1;
			}

			if (sess->ranges[i].end >= (s64) known_file_size) {
				request_ok = GF_FALSE;
				break;
			}
			if (sess->ranges[i].start >= (s64) known_file_size) {
				request_ok = GF_FALSE;
				break;
			}
		} else {
			//no start, end is a file size
			if (sess->ranges[i].end >= (s64) known_file_size) {
				request_ok = GF_FALSE;
				break;
			}
			sess->ranges[i].start = known_file_size - sess->ranges[i].end;
			sess->ranges[i].end = known_file_size - 1;
		}
		sess->bytes_in_req += (sess->ranges[i].end + 1 - sess->ranges[i].start);
	}
	//if we have a single byte range request covering the entire file, reply 200 OK and not 206 partial
	if ((sess->nb_ranges == 1) && known_file_size && !sess->ranges[0].start && (sess->ranges[0].end==known_file_size-1))
		sess->nb_ranges = 0;

	if (!request_ok) {
		if (!sess->in_source || (sess->nb_ranges>1))
			return GF_FALSE;
		//source in progress, we accept single range - note that this could be further refined by postponing the request until the source
		//is done or has written the requested byte range, however this will delay sending chunk in LL-HLS byterange ...
		//for now, since we use chunk transfer in this case, we will send less data than asked and close resource using last 0-size chunk
	}
	sess->file_pos = sess->ranges[0].start;
	if (sess->resource)
		gf_fseek(sess->resource, sess->file_pos, SEEK_SET);
	return GF_TRUE;
}

static Bool httpout_do_log(GF_HTTPOutSession *sess, u32 method)
{
	if (!sess->ctx->reqlog) return GF_FALSE;

	if (!strcmp(sess->ctx->reqlog, "*")) return GF_TRUE;

	switch (method) {
	case GF_HTTP_GET:
		if (strstr(sess->ctx->reqlog, "GET") || strstr(sess->ctx->reqlog, "get")) return GF_TRUE;
		break;
	case GF_HTTP_PUT:
		if (strstr(sess->ctx->reqlog, "PUT") || strstr(sess->ctx->reqlog, "put")) return GF_TRUE;
		break;
	case GF_HTTP_POST:
		if (strstr(sess->ctx->reqlog, "POST") || strstr(sess->ctx->reqlog, "post")) return GF_TRUE;
		break;
	case GF_HTTP_DELETE:
		if (strstr(sess->ctx->reqlog, "DEL") || strstr(sess->ctx->reqlog, "del")) return GF_TRUE;
		break;
	case GF_HTTP_HEAD:
		if (strstr(sess->ctx->reqlog, "HEAD") || strstr(sess->ctx->reqlog, "head")) return GF_TRUE;
		break;
	case GF_HTTP_OPTIONS:
		if (strstr(sess->ctx->reqlog, "OPT") || strstr(sess->ctx->reqlog, "opt")) return GF_TRUE;
		break;
	default:
		return GF_TRUE;
	}
	return GF_FALSE;
}

#ifndef GPAC_DISABLE_LOG
static const char *get_method_name(u32 method)
{
	switch (method) {
	case GF_HTTP_GET: return "GET";
	case GF_HTTP_HEAD: return "HEAD";
	case GF_HTTP_PUT: return "PUT";
	case GF_HTTP_POST: return "POST";
	case GF_HTTP_DELETE: return "DELETE";
	case GF_HTTP_TRACE: return "TRACE";
	case GF_HTTP_CONNECT: return "CONNECT";
	case GF_HTTP_OPTIONS: return "OPTIONS";
	default: return "UNKNOWN";
	}
}
#endif //GPAC_DISABLE_LOG

GF_Err httpout_new_subsession(GF_HTTPOutSession *sess, u32 stream_id)
{
	GF_HTTPOutSession *sub_sess;
	GF_Err e;
	if (!sess || !sess->http_sess || !sess->is_h2)
		return GF_BAD_PARAM;


	GF_SAFEALLOC(sub_sess, GF_HTTPOutSession);
	if (!sub_sess) return GF_OUT_OF_MEM;
	sub_sess->socket = sess->socket;
	sub_sess->ctx = sess->ctx;
	//mark the subsession as being h2 right away so that we can process it even if no pending data on socket (cf httpout_process_session)
	sub_sess->is_h2 = GF_TRUE;
	strcpy(sub_sess->peer_address, sess->peer_address);
	sub_sess->http_sess = gf_dm_sess_new_subsession(sess->http_sess, stream_id, sub_sess, &e);
	if (!sub_sess->http_sess) {
		gf_free(sub_sess);
		return e;
	}
	gf_list_add(sess->ctx->sessions, sub_sess);
	gf_list_add(sess->ctx->active_sessions, sub_sess);
	sess->sub_sess_pending = GF_TRUE;
	return GF_OK;
}


static void httpout_sess_io(void *usr_cbk, GF_NETIO_Parameter *parameter)
{
	const char *durl="";
	char *url=NULL;
	char *full_path=NULL;
	char szFmt[100];
	char szDate[200];
	char szRange[200];
	char szETag[100];
	u64 modif_time=0;
	u32 body_size=0;
	const char *etag=NULL, *range=NULL;
	const char *mime = NULL;
	char *cors_origin = NULL;
	char *response_body = NULL;
	GF_Err e=GF_OK;
	Bool not_modified = GF_FALSE;
	Bool is_upload = GF_FALSE;
	Bool is_options = GF_FALSE;
	Bool is_head = GF_FALSE;
	Bool no_body = GF_FALSE;
	Bool send_cors;
	u32 i, count;
	GF_HTTPOutInput *source_pid = NULL;
	Bool source_pid_is_ll_hls_chunk = GF_FALSE;
	GF_HTTPOutSession *source_sess = NULL;
	GF_HTTPOutSession *sess = usr_cbk;

	if (parameter->msg_type == GF_NETIO_REQUEST_SESSION) {
		parameter->error = httpout_new_subsession(sess, parameter->reply);
		return;
	}
	if (parameter->msg_type == GF_NETIO_CANCEL_STREAM) {
		sess->canceled = GF_TRUE;
		return;
	}

	if (parameter->msg_type != GF_NETIO_PARSE_REPLY) {
		parameter->error = GF_BAD_PARAM;
		return;
	}

	send_cors = GF_FALSE;
	sess->reply_code = 0;
	switch (parameter->reply) {
	case GF_HTTP_GET:
	case GF_HTTP_HEAD:
		break;
	case GF_HTTP_PUT:
	case GF_HTTP_POST:
	case GF_HTTP_DELETE:
		is_upload = GF_TRUE;
		break;
	case GF_HTTP_OPTIONS:
		is_options = GF_TRUE;
		break;
	default:
		sess->reply_code = 501;
		gf_dynstrcat(&response_body, "Method is not supported by GPAC", NULL);
		goto exit;
	}
	durl = gf_dm_sess_get_resource_name(sess->http_sess);
	if (!durl || (durl[0] != '/')) {
		sess->reply_code = 400;
		goto exit;
	}
	url = gf_url_percent_decode(durl);

	sess->file_pos = 0;
	sess->file_size = 0;
	sess->bytes_in_req = 0;
	sess->nb_ranges = 0;
	sess->do_log = httpout_do_log(sess, parameter->reply);

	//resolve name against upload dir
	if (is_upload) {
		if (!sess->ctx->wdir && (sess->ctx->hmode!=MODE_SOURCE)) {
			sess->reply_code = 405;
			gf_dynstrcat(&response_body, "No write directory enabled on server", NULL);
			goto exit;
		}
		if (sess->ctx->wdir) {
			u32 len = (u32) strlen(sess->ctx->wdir);
			gf_dynstrcat(&full_path, sess->ctx->wdir, NULL);
			if (!strchr("/\\", sess->ctx->wdir[len-1]))
				gf_dynstrcat(&full_path, "/", NULL);
			gf_dynstrcat(&full_path, url+1, NULL);
		} else if (sess->ctx->hmode==MODE_SOURCE) {
			full_path = gf_strdup(url+1);
		}
		if (parameter->reply==GF_HTTP_DELETE)
			is_upload = GF_FALSE;
	}

	if (is_upload) {
		const char *hdr;
		range = gf_dm_sess_get_header(sess->http_sess, "Range");

		if (sess->in_source) {
			sess->in_source->nb_dest--;
			sess->in_source = NULL;
		}
		sess->content_length = 0;
		hdr = gf_dm_sess_get_header(sess->http_sess, "Content-Length");
		if (hdr) {
			sscanf(hdr, LLU, &sess->content_length);
		}
		sess->use_chunk_transfer = GF_FALSE;
		hdr = gf_dm_sess_get_header(sess->http_sess, "Transfer-Encoding");
		if (hdr && !strcmp(hdr, "chunked")) {
			sess->use_chunk_transfer = GF_TRUE;
		} else if (!sess->is_h2) {
			sess->is_h2 = gf_dm_sess_is_h2(sess->http_sess);
		}
		sess->file_in_progress = GF_FALSE;
		sess->nb_bytes = 0;
		sess->done = GF_FALSE;
		assert(full_path);
		if (sess->path) gf_free(sess->path);
		sess->path = full_path;
		if (sess->resource) gf_fclose(sess->resource);
		sess->resource = NULL;

		if (sess->ctx->hmode==MODE_SOURCE) {
			if (range) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTPOut] Cannot handle PUT/POST request as PID output with byte ranges (%s)\n", range));
				sess->reply_code = 416;
				gf_dynstrcat(&response_body, "Server running in source mode - cannot handle PUT/POST request with byte ranges ", NULL);
				gf_dynstrcat(&response_body, range, NULL);
				goto exit;
			}
			sess->upload_type = 1;
			sess->reconfigure_output = GF_TRUE;
		} else {
			if (gf_file_exists(sess->path))
				sess->upload_type = 2;
			else
				sess->upload_type = 1;

			sess->resource = gf_fopen(sess->path, range ? "rb+" : "wb");
			if (!sess->resource) {
				sess->reply_code = 403;
				gf_dynstrcat(&response_body, "File exists but cannot be open", NULL);
				goto exit;
			}
			if (!sess->content_length && !sess->use_chunk_transfer && !sess->is_h2) {
				sess->reply_code = 411;
				gf_dynstrcat(&response_body, "No content length specified and chunked transfer not enabled", NULL);
				goto exit;
			}
			sess->file_size = gf_fsize(sess->resource);
		}
		sess->file_pos = 0;

		range = gf_dm_sess_get_header(sess->http_sess, "Range");
		if (! httpout_sess_parse_range(sess, (char *) range) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTPOut] Unsupported Range format: %s\n", range));
			sess->reply_code = 416;
			gf_dynstrcat(&response_body, "Range format is not supported, only \"bytes\" units allowed: ", NULL);
			gf_dynstrcat(&response_body, range, NULL);
			goto exit;
		}
		if (!sess->buffer) {
			sess->buffer = gf_malloc(sizeof(u8)*sess->ctx->block_size);
		}
		if (gf_list_find(sess->ctx->active_sessions, sess)<0) {
			gf_list_add(sess->ctx->active_sessions, sess);
			gf_sk_group_register(sess->ctx->sg, sess->socket);
		}
		sess->last_active_time = gf_sys_clock_high_res();

		if (sess->do_log) {
			sess->req_id = ++sess->ctx->req_id;
			sess->method_type = parameter->reply;
			if (range) {
				GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] REQ#"LLU" %s %s %s [range: %s] start%s\n", sess->req_id, sess->peer_address, get_method_name(sess->method_type), url+1, range, sess->use_chunk_transfer ? " chunk-transfer" : ""));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] REQ#"LLU" %s %s %s start%s\n", sess->req_id, sess->peer_address, get_method_name(sess->method_type), url+1, sess->use_chunk_transfer ? " chunk-transfer" : ""));
			}
		}
		sess->nb_consecutive_errors = 0;
		sess->req_start_time = gf_sys_clock_high_res();
		if (url) gf_free(url);
		gf_dm_sess_clear_headers(sess->http_sess);
		//send reply once we are done receiving
		return;
	}

	/*first check active inputs*/
	count = gf_list_count(sess->ctx->inputs);
	//delete only accepts local files
	if (parameter->reply == GF_HTTP_DELETE)
		count = 0;

	for (i=0; i<count; i++) {
		GF_HTTPOutInput *in = gf_list_get(sess->ctx->inputs, i);
		assert(in->path[0] == '/');
		//matching name and input pid not done: file has been created and is in progress
		//if input pid done, try from file
		if (!strcmp(in->path, url) && !in->done) {
			source_pid = in;
			break;
		}
		if (in->hls_chunk_path && !strcmp(in->hls_chunk_path, url) && !in->done) {
			source_pid = in;
			source_pid_is_ll_hls_chunk = GF_TRUE;
			break;
		}
	}

	/*not resolved and no source matching, check file on disk*/
	if (!source_pid && !full_path) {
		count = sess->ctx->rdirs.nb_items;
		for (i=0; i<count; i++) {
			char *mdir = sess->ctx->rdirs.vals[i];
			u32 len = (u32) strlen(mdir);
			if (!len) continue;
			if (count==1) {
				gf_dynstrcat(&full_path, mdir, NULL);
				if (!strchr("/\\", mdir[len-1]))
					gf_dynstrcat(&full_path, "/", NULL);
			}
			gf_dynstrcat(&full_path, url+1, NULL);

			if (gf_file_exists(full_path) || gf_dir_exists(full_path) )
				break;
			gf_free(full_path);
			full_path = NULL;
		}
	}

	cors_origin = (char *) gf_dm_sess_get_header(sess->http_sess, "Origin");
	switch (sess->ctx->cors) {
	case CORS_ON:
		send_cors = GF_TRUE;
		break;
	case CORS_AUTO:
		if (cors_origin != NULL) {
			send_cors = GF_TRUE;
			break;
		}
	default:
		send_cors = GF_FALSE;
		break;
	}
	if (is_options && (!url || !strcmp(url, "*"))) {
		sess->reply_code = 204;
		goto exit;
	}

	if (!full_path && !source_pid) {
		if (!sess->ctx->dlist || strcmp(url, "/")) {
			sess->reply_code = 404;
			gf_dynstrcat(&response_body, "Resource ", NULL);
			gf_dynstrcat(&response_body, url, NULL);
			gf_dynstrcat(&response_body, " cannot be resolved", NULL);
			goto exit;
		}
	}

	if (is_options) {
		sess->reply_code = 200;
		goto exit;
	}

	//check if request is HEAD or GET on a file being uploaded
	if (full_path && ((parameter->reply == GF_HTTP_GET) || (parameter->reply == GF_HTTP_HEAD))) {
		count = gf_list_count(sess->ctx->sessions);
		for (i=0; i<count; i++) {
			source_sess = gf_list_get(sess->ctx->sessions, i);
			if ((source_sess != sess) && !source_sess->done && source_sess->upload_type && !strcmp(source_sess->path, full_path)) {
				break;
			}
			source_sess = NULL;
		}
	}

	szETag[0] = 0;

	//session is on an active input being uploaded, always consider as modified
	if (source_pid && !sess->ctx->single_mode) {
		etag = NULL;
	}
	//resource is being uploaded, always consider as modified
	else if (source_sess) {
		etag = NULL;
	}
	//check ETag
	else if (full_path) {
		modif_time = gf_file_modification_time(full_path);
		sprintf(szETag, LLU, modif_time);
		etag = gf_dm_sess_get_header(sess->http_sess, "If-None-Match");
	}

	range = gf_dm_sess_get_header(sess->http_sess, "Range");

	if (sess->in_source) {
		sess->in_source->nb_dest--;
		sess->in_source = NULL;
	}
	sess->file_in_progress = GF_FALSE;
	sess->use_chunk_transfer = GF_FALSE;
	sess->put_in_progress = 0;
	sess->nb_bytes = 0;
	sess->upload_type = 0;

	if (parameter->reply==GF_HTTP_DELETE) {
		no_body = GF_TRUE;
		sess->upload_type = 0;
		if (sess->path) gf_free(sess->path);
		sess->path = full_path;
		if (sess->resource) gf_fclose(sess->resource);
		sess->resource = NULL;
		sess->file_pos = sess->file_size = 0;

		if (gf_file_exists(full_path)) {
			e = gf_file_delete(full_path);

			if (e) {
				sess->reply_code = 500;
				GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTPOut] Error deleting file %s (full path %s)\n", url, full_path));
				sess->reply_code = 500;
				gf_dynstrcat(&response_body, "Error while deleting ", NULL);
				gf_dynstrcat(&response_body, url, NULL);
				gf_dynstrcat(&response_body, ": ", NULL);
				gf_dynstrcat(&response_body, gf_error_to_string(e), NULL);
				goto exit;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPOut] Deleting file %s (full path %s)\n", url, full_path));

			if (sess->do_log) {
				sess->req_id = ++sess->ctx->req_id;
				GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] REQ#"LLU" %s DELETE %s\n", sess->req_id, sess->peer_address, url+1));
				sess->do_log = GF_FALSE;
			}
		} else {
			e = GF_URL_ERROR;
		}
		range = NULL;
	}
	/*we have a source and we are not in record mode */
	else if (source_pid && sess->ctx->single_mode) {
		if (sess->path) gf_free(sess->path);
		sess->path = NULL;
		sess->in_source = source_pid;
		source_pid->nb_dest++;
		sess->send_init_data = GF_TRUE;
		source_pid->hold = GF_FALSE;

		sess->file_pos = sess->file_size = 0;
		sess->use_chunk_transfer = GF_TRUE;
	}
	/*we have matching etag*/
	else if (etag && !strcmp(etag, szETag) && !sess->ctx->no_etag) {
		if (sess->path) gf_free(sess->path);
		sess->path = full_path;
		not_modified = GF_TRUE;
	}
	/*we have the same URL, source file is setup and not modified, no need to resetup - byte-range is setup after */
	else if (!sess->in_source && sess->resource && (sess->last_file_modif == modif_time) && sess->path && full_path && !strcmp(sess->path, full_path) ) {
		gf_free(full_path);
		sess->file_pos = 0;
	}
	else {
		if (sess->path) gf_free(sess->path);
		if (source_pid) {
			//source_pid->resource may be NULL at this point (PID created but no data written yet), typically happens on manifest PIDs or init segments
			sess->in_source = source_pid;
			source_pid->nb_dest++;
			source_pid->hold = GF_FALSE;
			sess->send_init_data = GF_TRUE;
			sess->in_source_is_ll_hls_chunk = source_pid_is_ll_hls_chunk;

			sess->file_in_progress = GF_TRUE;
			assert(!full_path);
			assert(source_pid->local_path);
			if (source_pid_is_ll_hls_chunk)
				full_path = gf_strdup(source_pid->hls_chunk_local_path);
			else
				full_path = gf_strdup(source_pid->local_path);
			sess->use_chunk_transfer = GF_TRUE;
			sess->file_size = 0;
		}
		sess->path = full_path;
		if (!full_path || gf_dir_exists(full_path)) {
			if (sess->ctx->dlist) {
				range = NULL;
				if (!strcmp(url, "/")) {
					response_body = httpout_create_listing(sess->ctx, (char *) url);
				} else {
					response_body = httpout_create_listing(sess->ctx, full_path);
				}
				sess->file_size = sess->file_pos = 0;
			} else {
				sess->reply_code = 403;
				gf_dynstrcat(&response_body, "Directory browsing is not allowed", NULL);
				goto exit;
			}
		} else {
			sess->resource = gf_fopen(full_path, "rb");
			//we may not have the file if it is currently being created
			if (!sess->resource && !sess->in_source) {
				sess->reply_code = 500;
				gf_dynstrcat(&response_body, "File exists but no read access", NULL);
				goto exit;
			}
			//warning, sess->resource may still be NULL here !

			mime = source_pid ? source_pid->mime : NULL;
			//probe for mime
			if (!mime && sess->resource) {
				u8 probe_buf[5001];
				u32 read = (u32) gf_fread(probe_buf, 5000, sess->resource);
				if ((s32) read < 0) {
					if (source_sess) {
						read = 0;
					} else {
						sess->reply_code = 500;
						gf_dynstrcat(&response_body, "File opened but read operation failed", NULL);
						goto exit;
					}
				}
				if (read) {
					probe_buf[read] = 0;
					mime = gf_filter_probe_data(sess->ctx->filter, probe_buf, read);
				}
			}
			if (source_sess) {
				sess->file_size = 0;
				sess->use_chunk_transfer = GF_TRUE;
				sess->put_in_progress = 1;
			} else if (sess->resource) {
				//get file size, might be incomplete if file writing is in progress
				sess->file_size = gf_fsize(sess->resource);
			} else {
				sess->file_size = 0;
			}
		}
		sess->file_pos = 0;
		sess->bytes_in_req = sess->file_size;

		if (sess->mime) gf_free(sess->mime);
		sess->mime = ( mime && strcmp(mime, "*")) ? gf_strdup(mime) : NULL;
		sess->last_file_modif = gf_file_modification_time(full_path);
	}

	//parse byte range except if associated input in single mode where byte ranges are ignored
	if ( (!sess->in_source || !sess->ctx->single_mode) && ! httpout_sess_parse_range(sess, (char *) range) ) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTPOut] Unsupported Range format: %s\n", range));
		sess->reply_code = 416;
		gf_dynstrcat(&response_body, "Range format is not supported, only \"bytes\" units allowed: ", NULL);
		gf_dynstrcat(&response_body, range, NULL);
		goto exit;
	}

	if (not_modified) {
		sess->reply_code = 304;
	} else if (sess->nb_ranges) {
		sess->reply_code = 206;
	} else if ((parameter->reply==GF_HTTP_DELETE) && (e==GF_URL_ERROR)) {
		sess->reply_code = 204;
	} else {
		sess->reply_code = 200;
	}

	//copy range for logs before resetting session headers
	if (sess->do_log && range) {
		strncpy(szRange, range, 199);
		szRange[199] = 0;
	} else {
		szRange[0] = 0;
	}
	//copy cors origin
	if (cors_origin)
		cors_origin = gf_strdup(cors_origin);

	gf_dm_sess_clear_headers(sess->http_sess);

	gf_dm_sess_set_header(sess->http_sess, "Server", sess->ctx->user_agent);

	httpout_format_date(gf_net_get_utc(), szDate, GF_FALSE);
	gf_dm_sess_set_header(sess->http_sess, "Date", szDate);

	if (send_cors) {
		gf_dm_sess_set_header(sess->http_sess, "Access-Control-Allow-Origin", cors_origin ? cors_origin : "*");
		gf_dm_sess_set_header(sess->http_sess, "Access-Control-Expose-Headers", "*");
	}
	if (cors_origin) gf_free(cors_origin);

	if (sess->ctx->sutc) {
		sprintf(szFmt, LLU, gf_net_get_utc() );
		gf_dm_sess_set_header(sess->http_sess, "Server-UTC", szFmt);
	}

	if (parameter->reply == GF_HTTP_HEAD) {
		is_head = GF_TRUE;
		no_body = GF_TRUE;
	}

	if (sess->ctx->close) {
		gf_dm_sess_set_header(sess->http_sess, "Connection", "close");
	} else {
		gf_dm_sess_set_header(sess->http_sess, "Connection", "keep-alive");
		if (sess->ctx->timeout) {
			sprintf(szFmt, "timeout=%d", sess->ctx->timeout);
			gf_dm_sess_set_header(sess->http_sess, "Keep-Alive", szFmt);
		}
	}

	if (response_body) {
		body_size = (u32) strlen(response_body);
		gf_dm_sess_set_header(sess->http_sess, "Content-Type", "text/html");
		sprintf(szFmt, "%d", body_size);
		gf_dm_sess_set_header(sess->http_sess, "Content-Length", szFmt);
	}
	//for HEAD/GET only
	else if (!not_modified && (parameter->reply!=GF_HTTP_DELETE) ) {
		if (!sess->in_source && !sess->ctx->no_etag && szETag[0]) {
			gf_dm_sess_set_header(sess->http_sess, "ETag", szETag);
			if (sess->ctx->cache_control) {
				gf_dm_sess_set_header(sess->http_sess, "Cache-Control", sess->ctx->cache_control);
			}
		} else if (sess->in_source && !sess->ctx->rdirs.nb_items) {
			sess->nb_ranges = 0;
			gf_dm_sess_set_header(sess->http_sess, "Cache-Control", "no-cache, no-store");
		}
		//only put content length if not using chunk transfer - bytes_in_req may be > 0 if we have a byte range on a chunk-transfer session
		if (sess->bytes_in_req && !sess->use_chunk_transfer) {
			sprintf(szFmt, LLU, sess->bytes_in_req);
			gf_dm_sess_set_header(sess->http_sess, "Content-Length", szFmt);
		}
		mime = sess->in_source ? sess->in_source->mime : mime;
		if (mime && !strcmp(mime, "*")) mime = NULL;
		if (mime) {
			gf_dm_sess_set_header(sess->http_sess, "Content-Type", mime);
		}
		//data comes either directly from source pid, or from file written by source pid, we must use chunk transfer
		if (!is_head && sess->use_chunk_transfer) {
			gf_dm_sess_set_header(sess->http_sess, "Transfer-Encoding", "chunked");
		}

		if (!is_head && sess->nb_ranges) {
			char *ranges = NULL;
			gf_dynstrcat(&ranges, "bytes=", NULL);
			for (i=0; i<sess->nb_ranges; i++) {
				if (sess->in_source || !sess->file_size) {
					sprintf(szFmt, LLD"-"LLD"/*", sess->ranges[i].start, sess->ranges[i].end);
				} else {
					sprintf(szFmt, LLD"-"LLD"/"LLU, sess->ranges[i].start, sess->ranges[i].end, sess->file_size);
				}
				gf_dynstrcat(&ranges, szFmt, i ? ", " : NULL);
			}
			gf_dm_sess_set_header(sess->http_sess, "Content-Range", ranges);
			gf_free(ranges);
		}

		if (sess->in_source && sess->ctx->ice) {
			const GF_PropertyValue *p;
			u32 sr=0, br=0, nb_ch=0, p_idx;
			u32 w=0, h=0;
			p = gf_filter_pid_get_property(sess->in_source->ipid, GF_PROP_PID_SAMPLE_RATE);
			if (p) sr = p->value.uint;
			p = gf_filter_pid_get_property(sess->in_source->ipid, GF_PROP_PID_NUM_CHANNELS);
			if (p) nb_ch = p->value.uint;
			p = gf_filter_pid_get_property(sess->in_source->ipid, GF_PROP_PID_BITRATE);
			if (p) br = p->value.uint;

			p = gf_filter_pid_get_property(sess->in_source->ipid, GF_PROP_PID_WIDTH);
			if (p) w = p->value.uint;
			p = gf_filter_pid_get_property(sess->in_source->ipid, GF_PROP_PID_HEIGHT);
			if (p) h = p->value.uint;

			if (sr || br || nb_ch) {
				if (sr && br && nb_ch)
					sprintf(szFmt, "samplerate=%d;channels=%d;bitrate=%d", sr, nb_ch, br);
				else if (sr && nb_ch)
					sprintf(szFmt, "samplerate=%d;channels=%d", sr, nb_ch);
				else if (sr && br)
					sprintf(szFmt, "samplerate=%d;bitrate=%d", sr, br);
				else if (nb_ch && br)
					sprintf(szFmt, "channels=%d;bitrate=%d", nb_ch, br);
				else if (nb_ch)
					sprintf(szFmt, "channels=%d", nb_ch);
				else
					sprintf(szFmt, "bitrate=%d", br);

				gf_dm_sess_set_header(sess->http_sess, "ice-audio-info", szFmt);
			}
			if (w && h) {
				sprintf(szFmt, "width=%d;height=%d", w, h);
				gf_dm_sess_set_header(sess->http_sess, "ice-video-info", szFmt);
			}

			if (br) {
				sprintf(szFmt, "%d", br);
				gf_dm_sess_set_header(sess->http_sess, "icy-br", szFmt);
			}
			gf_dm_sess_set_header(sess->http_sess, "icy-pub", "1");
			p = gf_filter_pid_get_property(sess->in_source->ipid, GF_PROP_PID_SERVICE_NAME);
			if (p && p->value.string) {
				gf_dm_sess_set_header(sess->http_sess, "icy-name", p->value.string);
			}
			p_idx = 0;
			while (1) {
				const char *pname;
				p = gf_filter_pid_enum_properties(sess->in_source->ipid, &p_idx, NULL, &pname);
				if (!p) break;
				if (!pname || strncmp(pname, "ice-", 4)) continue;
				if (!p->value.string) continue;
				gf_dm_sess_set_header(sess->http_sess, pname, p->value.string);
			}
		}
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Sending response to %s\n", sess->peer_address));

	if (sess->do_log) {
		sess->req_id = ++sess->ctx->req_id;
		sess->method_type = parameter->reply;
		sess->req_start_time = gf_sys_clock_high_res();
		if (not_modified) {
			GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] REQ#"LLU" %s %s %s: reply %d\n", sess->req_id, sess->peer_address, get_method_name(sess->method_type), url+1, sess->reply_code));
		} else if (szRange[0]) {
			GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] REQ#"LLU" %s %s %s [range: %s] start%s\n", sess->req_id, sess->peer_address, get_method_name(sess->method_type), url+1, szRange, sess->use_chunk_transfer ? " chunk-transfer" : ""));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] REQ#"LLU" %s %s %s start%s\n", sess->req_id, sess->peer_address, get_method_name(sess->method_type), url+1, sess->use_chunk_transfer ? " chunk-transfer" : ""));
		}
	}

	sess->nb_consecutive_errors = 0;
	sess->canceled = GF_FALSE;
	e = gf_dm_sess_send_reply(sess->http_sess, sess->reply_code, response_body, no_body);
	sess->headers_done = GF_TRUE;
	sess->is_h2 = gf_dm_sess_is_h2(sess->http_sess);

	if (url) gf_free(url);
	if (!sess->buffer) {
		sess->buffer = gf_malloc(sizeof(u8)*sess->ctx->block_size);
	}
	if (response_body) {
		gf_free(response_body);
		sess->done = GF_TRUE;
	} else if (parameter->reply == GF_HTTP_DELETE) {
		sess->done = GF_TRUE;
	} else if (parameter->reply == GF_HTTP_HEAD) {
		sess->done = GF_FALSE;
		sess->file_pos = sess->file_size;
	} else {
		sess->done = GF_FALSE;
		if (gf_list_find(sess->ctx->active_sessions, sess)<0) {
			gf_list_add(sess->ctx->active_sessions, sess);
			gf_sk_group_register(sess->ctx->sg, sess->socket);
		}
		if (not_modified) {
			sess->done = GF_TRUE;
		}
	}

	if (e<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Error sending reply: %s\n", gf_error_to_string(e)));
		sess->done = GF_TRUE;
	}

	if (sess->done)
		sess->headers_done = GF_FALSE;

	gf_dm_sess_clear_headers(sess->http_sess);

	sess->last_active_time = gf_sys_clock_high_res();
	return;

exit:

	if (cors_origin && (sess->reply_code==200))
		send_cors = GF_TRUE;

	gf_dm_sess_clear_headers(sess->http_sess);

	gf_dm_sess_set_header(sess->http_sess, "Server", sess->ctx->user_agent);

	httpout_format_date(gf_net_get_utc(), szDate, GF_FALSE);
	gf_dm_sess_set_header(sess->http_sess, "Date", szDate);

	sess->nb_consecutive_errors++;
	if (sess->nb_consecutive_errors == sess->ctx->max_client_errors) {
		gf_dm_sess_set_header(sess->http_sess, "Connection", "close");
	} else {
		sess->last_active_time = gf_sys_clock_high_res();
	}

	if (is_options) {
		if (sess->ctx->hmode==MODE_SOURCE) {
			gf_dm_sess_set_header(sess->http_sess, "Allow", "POST,PUT,OPTIONS");
		} else if (sess->ctx->wdir) {
			gf_dm_sess_set_header(sess->http_sess, "Allow", "GET,POST,PUT,DELETE,HEAD,OPTIONS");
		} else {
			gf_dm_sess_set_header(sess->http_sess, "Allow", "GET,HEAD,OPTIONS");
		}
	}
	if (send_cors) {
		gf_dm_sess_set_header(sess->http_sess, "Access-Control-Allow-Origin", "*");
		gf_dm_sess_set_header(sess->http_sess, "Access-Control-Expose-Headers", "*");

		if (is_options) {
			gf_dm_sess_set_header(sess->http_sess, "Access-Control-Allow-Methods", "*");
			gf_dm_sess_set_header(sess->http_sess, "Access-Control-Allow-Headers", "*");
		}
	}

	if (response_body) {
		body_size = (u32) strlen(response_body);
		gf_dm_sess_set_header(sess->http_sess, "Content-Type", "text/plain");
		sprintf(szFmt, "%d", body_size);
		gf_dm_sess_set_header(sess->http_sess, "Content-Length", szFmt);
	}

	gf_dm_sess_send_reply(sess->http_sess, sess->reply_code, response_body, GF_FALSE);
	sess->is_h2 = gf_dm_sess_is_h2(sess->http_sess);

	if (response_body) gf_free(response_body);
	gf_dm_sess_clear_headers(sess->http_sess);

	if (sess->do_log) {
		sess->req_id = ++sess->ctx->req_id;
		if (sess->reply_code<204) {
			GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] REQ#"LLU" %s %s %s OK\n", sess->req_id, sess->peer_address, get_method_name(parameter->reply), url ? (url+1) : ""));
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ALL, ("[HTTPOut] REQ#"LLU" %s %s %s error %d\n", sess->req_id, sess->peer_address, get_method_name(parameter->reply), url ? (url+1) : "", sess->reply_code));
		}
	}

	if (url) gf_free(url);
	sess->upload_type = 0;
	sess->done = GF_TRUE;
	sess->canceled = GF_FALSE;
	sess->headers_done = GF_FALSE;

	if (!sess->is_h2 && (sess->ctx->close || (sess->nb_consecutive_errors == sess->ctx->max_client_errors))) {
		sess->force_destroy = GF_TRUE;
	} else if (sess->http_sess) {
		gf_dm_sess_server_reset(sess->http_sess);
	}
	return;
}

enum
{
	HTTP_PUT_HEADER_ENCODING=0,
	HTTP_PUT_HEADER_MIME,
	HTTP_PUT_HEADER_RANGE,
	HTTP_PUT_HEADER_DONE
};

static void httpout_in_io(void *usr_cbk, GF_NETIO_Parameter *parameter)
{
	GF_HTTPOutInput *in =usr_cbk;

	if (parameter->msg_type==GF_NETIO_GET_METHOD) {
		if (in->is_delete)
			parameter->name = "DELETE";
		else
			parameter->name = in->ctx->post ? "POST" : "PUT";
		in->cur_header = HTTP_PUT_HEADER_ENCODING;
		return;
	}
	if (parameter->msg_type==GF_NETIO_GET_HEADER) {
		parameter->name = parameter->value = NULL;

		if (in->is_delete) return;

		switch (in->cur_header) {
		case HTTP_PUT_HEADER_ENCODING:
			parameter->name = "Transfer-Encoding";
			parameter->value = "chunked";
			if (in->mime)
				in->cur_header = HTTP_PUT_HEADER_MIME;
			else
				in->cur_header = in->write_start_range ? HTTP_PUT_HEADER_RANGE : HTTP_PUT_HEADER_DONE;
			break;
		case HTTP_PUT_HEADER_MIME:
			parameter->name = "Content-Type";
			parameter->value = in->mime;
			in->cur_header = HTTP_PUT_HEADER_DONE;
			if (in->write_start_range)
				in->cur_header = HTTP_PUT_HEADER_RANGE;
			break;
		case HTTP_PUT_HEADER_RANGE:
			parameter->name = "Range";
			if (in->write_end_range) {
				sprintf(in->range_hdr, "bytes="LLU"-"LLU, in->write_start_range, in->write_end_range);
			} else {
				sprintf(in->range_hdr, "bytes="LLU"-", in->write_start_range);
			}
			parameter->value = in->range_hdr;
			in->cur_header = HTTP_PUT_HEADER_DONE;
			break;
		default:
			parameter->name = NULL;
			parameter->value = NULL;
			break;
		}
	}
}

static GF_Err httpout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_HTTPOutInput *pctx;
	GF_HTTPOutCtx *ctx = (GF_HTTPOutCtx *) gf_filter_get_udta(filter);

	if (!is_remove) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
		if (!p || (p->value.uint!=GF_STREAM_FILE))
			return GF_NOT_SUPPORTED;
	}

	pctx = gf_filter_pid_get_udta(pid);
	if (!pctx) {
		GF_HTTPOutCtx *ctx_orig;
		Bool patch_blocks = GF_FALSE;
		GF_FilterEvent evt;
        const char *res_path;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLE_PROGRESSIVE);
		if (p && p->value.uint) {
			if (ctx->hmode==MODE_PUSH) {
				if (p->value.uint==GF_PID_FILE_PATCH_INSERT) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Push cannot be used to insert blocks in remote files (not supported by HTTP)\n"));
					return GF_FILTER_NOT_SUPPORTED;
				}
				patch_blocks = GF_TRUE;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Server cannot deliver PIDs with progressive download disabled\n"));
				return GF_FILTER_NOT_SUPPORTED;
			}
		}

		/*if PID was connected to an alias, get the alias context to get the destination
		Otherwise PID was directly connected to the main filter, use main filter destination*/
		ctx_orig = (GF_HTTPOutCtx *) gf_filter_pid_get_alias_udta(pid);
        if (!ctx_orig) ctx_orig = ctx;

		if (!ctx_orig->dst && (ctx->hmode==MODE_PUSH))  {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Push output but no destination set !\n"));
			return GF_BAD_PARAM;
		}

		GF_SAFEALLOC(pctx, GF_HTTPOutInput);
		if (!pctx) return GF_OUT_OF_MEM;
		pctx->ipid = pid;
		pctx->ctx = ctx;
		pctx->patch_blocks = patch_blocks;
		pctx->hold = ctx->hold;

        res_path = NULL;
		if (ctx_orig->dst) {
            res_path = ctx_orig->dst;
            char *path = strstr(res_path, "://");
            if (path) path = strchr(path+3, '/');
            if (path) pctx->path = gf_strdup(path);
        } else if (!ctx->dst) {
            p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
            if (p && p->value.string) {
                res_path = p->value.string;
                pctx->path = gf_strdup(res_path);
            }
        }
        if (res_path) {

			if (ctx->hmode==MODE_PUSH) {
				GF_Err e;
				//note that ctx_orig->dst might be wrong (eg indicating MPD url rather than segment), but this is fixed in httpout_open_input by resetting up the session
				//with the correct URL
				pctx->upload = gf_dm_sess_new(gf_filter_get_download_manager(filter), ctx_orig->dst, GF_NETIO_SESSION_NOT_THREADED|GF_NETIO_SESSION_NOT_CACHED|GF_NETIO_SESSION_PERSISTENT, httpout_in_io, pctx, &e);
				if (!pctx->upload) {
					gf_free(pctx);
					return e;
				}
//				gf_sk_group_register(ctx->sg, pctx->socket);
			} else {
				if (!pctx->path) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Output path not specified\n"));
					return GF_BAD_PARAM;
				}
				httpout_set_local_path(ctx, pctx);
			}
			if (ctx->dst && !gf_list_count(ctx->inputs))
				pctx->force_dst_name = GF_TRUE;

			//reset caps to anything (mime or ext) in case a URL was given, since graph resolution is now done
			//this allows working with input filters dispatching files without creating a new destination (dashin in file mode for example)
			//we do not reset caps to default as the default caps list an output and we don't want gf_filter_connections_pending to think we will produce one
			ctx->in_caps[1].val = PROP_NAME( "*" );
		}
		//in any cast store dash state, mime, register input and fire play
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);
		if (p && p->value.uint) pctx->dash_mode = GF_TRUE;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
		if (p && p->value.string) pctx->mime = gf_strdup(p->value.string);

		gf_filter_pid_set_udta(pid, pctx);
		gf_list_add(ctx->inputs, pctx);

		gf_filter_pid_init_play_event(pid, &evt, 0.0, 1.0, "HTTPOut");
		gf_filter_pid_send_event(pid, &evt);
            
	}
	if (is_remove) {
		return GF_OK;
	}

	//we act as a server

	return GF_OK;
}


static void httpout_check_new_session(GF_HTTPOutCtx *ctx)
{
	char peer_address[GF_MAX_IP_NAME_LEN];
	GF_HTTPOutSession *sess;
	GF_Err e;
	void *ssl_c = NULL;
	GF_Socket *new_conn = NULL;

	e = gf_sk_accept(ctx->server_sock, &new_conn);
	if (e==GF_IP_SOCK_WOULD_BLOCK) return;
	else if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Accept failure %s\n", gf_error_to_string(e) ));
		return;
	}
	//check max connections
	if (ctx->maxc && (ctx->nb_connections>=ctx->maxc)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTPOut] Connection rejected due to too many connections\n"));
		gf_sk_del(new_conn);
		return;
	}
	gf_sk_get_remote_address(new_conn, peer_address);
	if (ctx->maxp) {
		u32 i, nb_conn=0, count = gf_list_count(ctx->sessions);
		for (i=0; i<count; i++) {
			sess = gf_list_get(ctx->sessions, i);
			if (!strcmp(sess->peer_address, peer_address)) nb_conn++;
		}
		if (nb_conn>=ctx->maxp) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTPOut] Connection rejected due to too many connections from peer %s\n", peer_address));
			gf_sk_del(new_conn);
			return;
		}
	}
	GF_SAFEALLOC(sess, GF_HTTPOutSession);
	if (!sess) {
		gf_sk_del(new_conn);
		return;
	}

	//we keep track of the socket for sock group (un)register
	sess->socket = new_conn;
	sess->ctx = ctx;

#ifdef GPAC_HAS_SSL
	if (ctx->ssl_ctx) {
		ssl_c = gf_ssl_new(ctx->ssl_ctx, new_conn, &e);
		if (e) {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Failed to create TLS session from %s: %s\n", sess->peer_address, gf_error_to_string(e) ));
			gf_free(sess);
			gf_sk_del(new_conn);
			return;
		}
	}
#endif

	sess->http_sess = gf_dm_sess_new_server(new_conn, ssl_c, httpout_sess_io, sess, &e);
	if (!sess->http_sess) {
		gf_sk_del(new_conn);
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Failed to create HTTP server session from %s: %s\n", sess->peer_address, gf_error_to_string(e) ));
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

	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Accepting new connection from %s\n", sess->peer_address));
	//ask immediate reschedule
	ctx->next_wake_us = 1;
}

static GF_Err httpout_initialize(GF_Filter *filter)
{
	char szIP[1024];
	GF_Err e;
	u16 port;
	char *ip;
	const char *ext = NULL;
	char *sep, *url;
	GF_HTTPOutCtx *ctx = (GF_HTTPOutCtx *) gf_filter_get_udta(filter);


	port = ctx->port;
	ip = ctx->ifce;

	url = ctx->dst;
	sep = NULL;
	if (ctx->dst) {
		if (!strncmp(ctx->dst, "http://", 7)) {
			sep = strchr(ctx->dst+7, '/');
		} else if (!strncmp(ctx->dst, "https://", 8)) {
			sep = strchr(ctx->dst+8, '/');
		}
	}
	if (sep) {
		u32 cplen;
		url = sep+1;
		cplen = (u32) (sep-ctx->dst-7);
		if (cplen>1023) cplen=1023;
		strncpy(szIP, ctx->dst+7, cplen);
		szIP[1023] = 0;
		sep = strchr(szIP, ':');
		if (sep) {
			port = atoi(sep+1);
			if (!port) port = ctx->port;
			sep[0] = 0;
		}
		if (strlen(szIP)) ip = szIP;
	}
	if (url && !strlen(url))
		url = NULL;

	if (url) {
		if (ctx->ext) ext = ctx->ext;
		else {
			ext = gf_file_ext_start(url);
			if (!ext) ext = ".*";
			ext += 1;
		}

		if (!ext && !ctx->mime) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTPOut] No extension provided nor mime type for output file %s, cannot infer format\nThis may result in invalid filter chain resolution", ctx->dst));
		} else {
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
				ctx->szExt[9] = 0;
				strlwr(ctx->szExt);
				ctx->in_caps[1].code = GF_PROP_PID_FILE_EXT;
				ctx->in_caps[1].val = PROP_NAME( ctx->szExt );
				ctx->in_caps[1].flags = GF_CAPS_INPUT;
			}
			gf_filter_override_caps(filter, ctx->in_caps, 2);
		}
	}
	/*this is an alias for our main filter, nothing to initialize*/
	if (gf_filter_is_alias(filter)) {
		return GF_OK;
	}

	if (ctx->wdir)
		ctx->hmode = MODE_DEFAULT;

	if (!url && !ctx->rdirs.nb_items && !ctx->wdir && (ctx->hmode!=MODE_SOURCE)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] No root dir(s) for server, no URL set and not configured as source, cannot run!\n" ));
		return GF_BAD_PARAM;
	}

	if (ctx->rdirs.nb_items || ctx->wdir) {
		gf_filter_make_sticky(filter);
	} else if (ctx->hmode!=MODE_PUSH) {
		ctx->single_mode = GF_TRUE;
	}

	ctx->sessions = gf_list_new();
	ctx->active_sessions = gf_list_new();
	ctx->inputs = gf_list_new();
	ctx->filter = filter;
	//used in both server and push modes
	ctx->sg = gf_sk_group_new();

	if (ip)
		ctx->ip = gf_strdup(ip);

	if (ctx->cache_control) {
		if (!strcmp(ctx->cache_control, "none")) ctx->no_etag = GF_TRUE;
	}

	if (ctx->hmode==MODE_PUSH) {
		ctx->hold = GF_FALSE;
		return GF_OK;
	}
	ctx->port = port;
	if (ctx->cert && !ctx->pkey) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] missing server private key file\n"));
		return GF_BAD_PARAM;
	}
	if (!ctx->cert && ctx->pkey) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] missing server certificate file\n"));
		return GF_BAD_PARAM;
	}
	if (ctx->cert && ctx->pkey) {
#ifdef GPAC_HAS_SSL
		if (!gf_file_exists(ctx->cert)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Certificate file %s not found\n", ctx->cert));
			return GF_IO_ERR;
		}
		if (!gf_file_exists(ctx->pkey)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Private key file %s not found\n", ctx->pkey));
			return GF_IO_ERR;
		}
		if (gf_ssl_init_lib()) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Failed to initialize OpenSSL library\n"));
			return GF_IO_ERR;
		}
		ctx->ssl_ctx = gf_ssl_server_context_new(ctx->cert, ctx->pkey);
		if (!ctx->ssl_ctx) return GF_IO_ERR;

		if (!ctx->port)
			ctx->port = 443;
#else
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] TLS key/certificate set but GPAC compiled without TLS support\n"));
		return GF_NOT_SUPPORTED;

#endif
	}

	if (!ctx->port)
		ctx->port = 80;

	ctx->server_sock = gf_sk_new(GF_SOCK_TYPE_TCP);
	e = gf_sk_bind(ctx->server_sock, NULL, ctx->port, ip, 0, GF_SOCK_REUSE_PORT);
	if (!e) e = gf_sk_listen(ctx->server_sock, ctx->maxc);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] failed to start server on port %d: %s\n", ctx->port, gf_error_to_string(e) ));
		return e;
	}
	gf_sk_group_register(ctx->sg, ctx->server_sock);

	gf_sk_server_mode(ctx->server_sock, GF_TRUE);
	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Server running on port %d\n", ctx->port));
	if (ctx->reqlog) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] Server running on port %d\n", ctx->port));
		if (strstr(ctx->reqlog, "REC"))
			ctx->log_record = GF_TRUE;
	}
	gf_filter_post_process_task(filter);
	return GF_OK;
}

static void httpout_del_session(GF_HTTPOutSession *s)
{
	gf_list_del_item(s->ctx->active_sessions, s);
	gf_list_del_item(s->ctx->sessions, s);
	if (s->http_sess)
		httpout_close_session(s);
	if (s->buffer) gf_free(s->buffer);
	if (s->path) gf_free(s->path);
	if (s->mime) gf_free(s->mime);
	if (s->opid) gf_filter_pid_remove(s->opid);
	if (s->resource) gf_fclose(s->resource);
	if (s->ranges) gf_free(s->ranges);
	gf_free(s);
}

static void httpout_close_hls_chunk(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in, Bool final_flush)
{
	if (!in->hls_chunk) return;

	gf_fclose(in->hls_chunk);
	in->hls_chunk = NULL;

	if (!final_flush) {
		u32 i, count;
		//detach all clients from this input and reassign to a regular output
		count = gf_list_count(ctx->sessions);
		for (i=0; i<count; i++) {
			GF_HTTPOutSession *sess = gf_list_get(ctx->sessions, i);
			if (sess->in_source != in) continue;
			if (!sess->in_source_is_ll_hls_chunk) continue;
			if (strcmp(sess->path, in->local_path)) continue;

			assert(sess->file_in_progress);
			if (sess->in_source) {
				sess->in_source->nb_dest--;
				sess->in_source = NULL;
				if (!sess->resource && sess->path) {
					sess->resource = gf_fopen(sess->path, "rb");
				}
			}
			sess->in_source_is_ll_hls_chunk = GF_FALSE;
			sess->file_size = gf_fsize(sess->resource);
			gf_fseek(sess->resource, sess->file_pos, SEEK_SET);
			sess->file_in_progress = GF_FALSE;
		}
	}

	if (in->hls_chunk_path) gf_free(in->hls_chunk_path);
	in->hls_chunk_path = NULL;
	if (in->hls_chunk_local_path) gf_free(in->hls_chunk_local_path);
	in->hls_chunk_local_path = NULL;
}


static void httpout_finalize(GF_Filter *filter)
{
	GF_HTTPOutCtx *ctx = (GF_HTTPOutCtx *) gf_filter_get_udta(filter);

	/*this is an alias for our main filter, nothing to finalize*/
	if (gf_filter_is_alias(filter))
		return;

	while (gf_list_count(ctx->sessions)) {
		GF_HTTPOutSession *tmp = gf_list_get(ctx->sessions, 0);
		tmp->opid = NULL;
		httpout_del_session(tmp);
	}
	gf_list_del(ctx->sessions);
	gf_list_del(ctx->active_sessions);

	while (gf_list_count(ctx->inputs)) {
		GF_HTTPOutInput *in = gf_list_pop_back(ctx->inputs);
		if (in->local_path) gf_free(in->local_path);
		if (in->path) gf_free(in->path);
		if (in->mime) gf_free(in->mime);

		httpout_close_hls_chunk(ctx, in, GF_TRUE);

		if (in->resource) gf_fclose(in->resource);
		if (in->upload) gf_dm_sess_del(in->upload);
		if (in->file_deletes) {
			while (gf_list_count(in->file_deletes)) {
				char *url = gf_list_pop_back(in->file_deletes);
				gf_free(url);
			}
			gf_list_del(in->file_deletes);
		}
		gf_free(in);
	}
	gf_list_del(ctx->inputs);
	if (ctx->server_sock) gf_sk_del(ctx->server_sock);
	if (ctx->sg) gf_sk_group_del(ctx->sg);
	if (ctx->ip) gf_free(ctx->ip);

#ifdef GPAC_HAS_SSL
	if (ctx->ssl_ctx) {
		gf_ssl_server_context_del(ctx->ssl_ctx);
	}
#endif
}

static GF_Err httpout_sess_data_upload(GF_HTTPOutSession *sess, const u8 *data, u32 size)
{
	u32 remain, write, to_write;

	if (sess->opid || sess->reconfigure_output) {
		GF_FilterPacket *pck;
		u8 *buffer;
		Bool is_first = GF_FALSE;
		if (sess->reconfigure_output) {
			sess->reconfigure_output = GF_FALSE;
			gf_filter_pid_raw_new(sess->ctx->filter, sess->path, NULL, sess->mime, NULL, (u8 *) data, size, GF_FALSE, &sess->opid);
			is_first = GF_TRUE;
		}
		if (!sess->opid) return GF_SERVICE_ERROR;
		pck = gf_filter_pck_new_alloc(sess->opid, size, &buffer);
		if (!pck) return GF_IO_ERR;
		memcpy(buffer, data, size);
		gf_filter_pck_set_framing(pck, is_first, GF_FALSE);
		gf_filter_pck_send(pck);
		return GF_OK;
	}
	if (!sess->resource) {
		assert(0);
	}
	if (!sess->nb_ranges) {
		write = (u32) gf_fwrite(data, size, sess->resource);
		if (write != size) {
			return GF_IO_ERR;
		}
		gf_fflush(sess->resource);
		sess->nb_bytes += write;
		sess->file_pos += write;
		return GF_OK;
	}
	remain = size;
	while (remain) {
		to_write = (u32) (sess->ranges[sess->range_idx].end + 1 - sess->file_pos);
		if (to_write>=remain) {
			write = (u32) gf_fwrite(data, remain, sess->resource);
			if (write != remain) {
				return GF_IO_ERR;
			}
			gf_fflush(sess->resource);
			sess->nb_bytes += write;
			sess->file_pos += remain;
			remain = 0;
			break;
		}
		write = (u32) gf_fwrite(data, to_write, sess->resource);
		sess->nb_bytes += write;
		remain -= to_write;
		sess->range_idx++;
		gf_fflush(sess->resource);
		if (sess->range_idx>=sess->nb_ranges) break;
		sess->file_pos = sess->ranges[sess->range_idx].start;
		gf_fseek(sess->resource, sess->file_pos, SEEK_SET);
	}
	if (remain) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Error in file upload, more bytes uploaded than described in byte range\n"));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}
static void httpout_check_connection(GF_HTTPOutSession *sess)
{
	GF_Err e = gf_sk_probe(sess->socket);
	if (e==GF_IP_CONNECTION_CLOSED) {
		sess->last_active_time = gf_sys_clock_high_res();
		sess->done = GF_TRUE;
		sess->canceled = GF_FALSE;
		sess->upload_type = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Client %s disconnected, destroying session\n", sess->peer_address));
		httpout_close_session(sess);
		return;
	}
}

static void log_request_done(GF_HTTPOutSession *sess)
{
	if (!sess->do_log) return;
	const char *sprefix = sess->is_h2 ? "H2 " : "";

	if (!sess->socket) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ALL, ("[HTTPOut] %sREQ#"LLU" %s aborted!\n", sprefix, sess->req_id, get_method_name(sess->method_type)));
	} else if (sess->canceled) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] %sREQ#"LLU" %s canceled\n", sprefix, sess->req_id, get_method_name(sess->method_type)));
	} else {
		char *unit = "bps";
		u64 diff_us = (gf_sys_clock_high_res() - sess->req_start_time);
		Double bps = (Double)sess->nb_bytes * 8000000;
		bps /= diff_us;
		if (bps>1000000) {
			unit = "mbps";
			bps/=1000000;
		} else if (bps>1000) {
			unit = "kbps";
			bps/=1000;
		}
#if 0
		assert(sess->nb_bytes);
		if (sess->nb_ranges) {
			u32 i;
			u64 tot_bytes = 0;
			for (i=0; i<sess->nb_ranges; i++) {
				tot_bytes += sess->ranges[i].end - sess->ranges[i].start + 1;
			}
			assert(tot_bytes==sess->nb_bytes);
		}
#endif
		GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] %sREQ#"LLU" %s done: reply %d - "LLU" bytes in %d ms at %g %s\n", sprefix, sess->req_id, get_method_name(sess->method_type), sess->reply_code, sess->nb_bytes, (u32) (diff_us/1000), bps, unit));
	}
}

static void httpout_process_session(GF_Filter *filter, GF_HTTPOutCtx *ctx, GF_HTTPOutSession *sess)
{
	u32 read;
	u64 to_read=0;
	GF_Err e = GF_OK;
	Bool close_session = ctx->close;

	if (sess->force_destroy) {
		httpout_close_session(sess);
		return;
	}


	//upload session (PUT, POST)
	if (sess->upload_type) {
		u32 i, count;
		GF_Err write_e=GF_OK;
		assert(sess->path);
		if (sess->done)
			return;

		read = 0;
		if (sess->canceled) {
			e = GF_EOS;
		} else {
			e = gf_dm_sess_fetch_data(sess->http_sess, sess->buffer, ctx->block_size, &read);
		}

		if (e>=GF_OK) {
			if (read)
				write_e = httpout_sess_data_upload(sess, sess->buffer, read);
			else
				write_e = GF_OK;

			if (!write_e) {
				sess->last_active_time = gf_sys_clock_high_res();
				//don't reschedule in upload, let server decide
				ctx->next_wake_us = 0;
				//we way be in end of stream
				if (e==GF_OK)
					return;
			} else {
				e = write_e;
			}
		} else if (e==GF_IP_NETWORK_EMPTY) {
			//don't reschedule in upload, let server decide
			ctx->next_wake_us = 0;
			sess->last_active_time = gf_sys_clock_high_res();
			httpout_check_connection(sess);
			return;
		} else if (e==GF_IP_CONNECTION_CLOSED) {
			sess->last_active_time = gf_sys_clock_high_res();
			sess->done = GF_TRUE;
			sess->canceled = GF_FALSE;
			sess->upload_type = 0;
			httpout_close_session(sess);
			log_request_done(sess);
			return;
		}
		//done (error or end of upload)
		if (sess->opid) {
			GF_FilterPacket *pck = gf_filter_pck_new_alloc(sess->opid, 0, NULL);
			if (pck) {
				gf_filter_pck_set_framing(pck, GF_FALSE, GF_TRUE);
				if (sess->canceled)
					gf_filter_pck_set_corrupted(pck, GF_TRUE);

				gf_filter_pck_send(pck);
			}
			gf_filter_pid_set_eos(sess->opid);
		} else {
			if (sess->resource) gf_fclose(sess->resource);
			sess->resource = NULL;
			//for now we remove any canceled file
			if (sess->canceled)
				gf_file_delete(sess->path);
		}

		if (sess->canceled) {
			log_request_done(sess);
		} else {
			char szDate[200];

			if (e==GF_EOS) {
				if (sess->upload_type==2) {
					sess->reply_code = 200;
				} else {
					sess->reply_code = 201;
				}
			} else if (write_e==GF_BAD_PARAM) {
				close_session = GF_TRUE;
				sess->reply_code = 416;
			} else {
				close_session = GF_TRUE;
				sess->reply_code = 500;
			}
			gf_dm_sess_set_header(sess->http_sess, "Server", sess->ctx->user_agent);

			httpout_format_date(gf_net_get_utc(), szDate, GF_FALSE);
			gf_dm_sess_set_header(sess->http_sess, "Date", szDate);

			if (close_session)
				gf_dm_sess_set_header(sess->http_sess, "Connection", "close");
			else
				gf_dm_sess_set_header(sess->http_sess, "Connection", "keep-alive");

			if (e==GF_EOS) {
				if (ctx->hmode==MODE_SOURCE) {
					char *loc = NULL;
					gf_dynstrcat(&loc, sess->path, "/");
					gf_dm_sess_set_header(sess->http_sess, "Content-Location", loc);
					gf_free(loc);
				} else {
					char *spath = strchr(sess->path, '/');
					if (spath) spath = spath+1;
					else spath = sess->path;

					if (ctx->wdir) {
						u32 plen = (u32) strlen(ctx->wdir);
						if ((ctx->wdir[plen-1] == '/') || (ctx->wdir[plen-1] == '\\'))
							plen--;
						if (!strncmp(ctx->wdir, sess->path, plen))
							spath = sess->path + plen;
					}

					gf_dm_sess_set_header(sess->http_sess, "Content-Location", spath);
				}
			}

			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Sending PUT response to %s - reply %d\n", sess->peer_address, sess->reply_code));

			log_request_done(sess);

			gf_dm_sess_send_reply(sess->http_sess, sess->reply_code, NULL, GF_TRUE);
		}

		sess->last_active_time = gf_sys_clock_high_res();
		sess->done = GF_TRUE;
		sess->canceled = GF_FALSE;
		sess->upload_type = 0;
		sess->nb_consecutive_errors = 0;

		//notify all download (GET, HEAD) sessions using the same resource that we are done
		count = gf_list_count(sess->ctx->sessions);
		for (i=0; i<count; i++) {
			GF_HTTPOutSession *a_sess = gf_list_get(sess->ctx->sessions, i);
			if (a_sess == sess) continue;
			if (a_sess->done || !a_sess->put_in_progress) continue;
			if (a_sess->path && sess->path && !strcmp(a_sess->path, sess->path)) {
				a_sess->put_in_progress = (sess->reply_code>201) ? 2 : 0;
				a_sess->file_size = gf_fsize(a_sess->resource);
				gf_fseek(a_sess->resource, a_sess->file_pos, SEEK_SET);
			}
		}

		if (close_session) {
			httpout_close_session(sess);
		} else {
			gf_dm_sess_server_reset(sess->http_sess);
		}
		return;
	}


	//other session: read incoming request and process headers
	if (!sess->headers_done) {
		//check we have something to read if not http2
		//if http2, data might have been received on this session while processing another session
		if (!sess->is_h2 && !gf_sk_group_sock_is_set(ctx->sg, sess->socket, GF_SK_SELECT_READ)) {
			return;
		}
		e = gf_dm_sess_process(sess->http_sess);

		if (e==GF_IP_NETWORK_EMPTY) {
			return;
		}

		if (e<0) {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Connection to %s closed: %s\n", sess->peer_address, gf_error_to_string(e) ));
			httpout_close_session(sess);
			if (!sess->done)
				log_request_done(sess);
			return;
		}

		sess->last_active_time = gf_sys_clock_high_res();
		//reschedule asap
		ctx->next_wake_us = 1;

		//request has been process, if not an upload we don't need the session anymore
		//otherwise we use the session to parse transfered data
		if (!sess->upload_type) {
			sess->headers_done = GF_TRUE;

			if (sess->done)
				goto session_done;
		} else {
			//flush any data received
			httpout_process_session(filter, ctx, sess);
			return;
		}
	}
	if (!sess->http_sess) return;

	//H2 session, keep on processing inputs
	if (sess->is_h2 && gf_sk_group_sock_is_set(ctx->sg, sess->socket, GF_SK_SELECT_READ)) {
		gf_dm_sess_process(sess->http_sess);
	}

	if (sess->done) return;
	//associated input directly writes to session
	if (sess->in_source && !sess->in_source->resource) return;

	if (sess->canceled) {
		log_request_done(sess);
		goto session_done;
	}

	if (!gf_sk_group_sock_is_set(ctx->sg, sess->socket, GF_SK_SELECT_WRITE)) {
		return;
	}
	//resource is not set
	if (!sess->resource && sess->path) {
		if (sess->in_source && !sess->in_source->nb_write) {
			sess->last_active_time = gf_sys_clock_high_res();
			return;
		}
		sess->resource = gf_fopen(sess->path, "rb");
		if (!sess->resource) return;
		sess->last_active_time = gf_sys_clock_high_res();
		gf_fseek(sess->resource, sess->file_pos, SEEK_SET);
	}

	//refresh file size
	if (sess->put_in_progress==1) {
		sess->file_size = gf_fsize(sess->resource);
		gf_fseek(sess->resource, sess->file_pos, SEEK_SET);
	}

resend:

	//we have ranges
	if (sess->nb_ranges) {
		//current range is done
		if ((s64) sess->file_pos >= sess->ranges[sess->range_idx].end) {
			//load next range, seeking file
			if (sess->range_idx+1<sess->nb_ranges) {
				sess->range_idx++;
				sess->file_pos = (u64) sess->ranges[sess->range_idx].start;
				gf_fseek(sess->resource, sess->file_pos, SEEK_SET);
			}
		}
		if (sess->range_idx<sess->nb_ranges) {
			to_read = sess->ranges[sess->range_idx].end + 1 - sess->file_pos;
		}
	} else if (sess->file_pos < sess->file_size) {
		to_read = sess->file_size - sess->file_pos;
	}

	if (to_read) {
		//rescedule asap while we send
		ctx->next_wake_us = 1;

		if (to_read > (u64) sess->ctx->block_size)
			to_read = (u64) sess->ctx->block_size;

		read = (u32) gf_fread(sess->buffer, (u32) to_read, sess->resource);
		//may happen when file writing is in progress
		if (!read) {
			sess->last_active_time = gf_sys_clock_high_res();
			return;
		}
		//transfer of file being uploaded, use chunk transfer
		if (!sess->is_h2 && sess->use_chunk_transfer) {
			char szHdr[100];
			u32 len;
			sprintf(szHdr, "%X\r\n", read);
			len = (u32) strlen(szHdr);

			e = gf_dm_sess_send(sess->http_sess, szHdr, len);
			e |= gf_dm_sess_send(sess->http_sess, sess->buffer, read);
			e |= gf_dm_sess_send(sess->http_sess, "\r\n", 2);
		} else {
			e = gf_dm_sess_send(sess->http_sess, sess->buffer, read);
		}
		sess->last_active_time = gf_sys_clock_high_res();

		sess->file_pos += read;
		sess->nb_bytes += read;

		if (e) {
			if (e==GF_IP_CONNECTION_CLOSED) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPOut] Connection to %s for %s closed\n", sess->peer_address, sess->path));
				sess->done = GF_TRUE;
				sess->canceled = GF_FALSE;
				httpout_close_session(sess);
				log_request_done(sess);
				return;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Error sending data to %s for %s: %s\n", sess->peer_address, sess->path, gf_error_to_string(e) ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPOut] sending data to %s for %s: "LLU"/"LLU" bytes\n", sess->peer_address, sess->path, sess->nb_bytes, sess->bytes_in_req));

			if (gf_sk_select(sess->socket, GF_SK_SELECT_WRITE)==GF_OK) {
				goto resend;
			}
		}
		return;
	}
	//file not done yet ...
	if (sess->file_in_progress || (sess->put_in_progress==1)) {
		sess->last_active_time = gf_sys_clock_high_res();
		return;
	}

session_done:

	sess->file_pos = sess->file_size;
	sess->last_active_time = gf_sys_clock_high_res();

	//an error ocured uploading the resource, we cannot notify that error so we force a close...
	if (sess->put_in_progress==2)
		close_session = GF_TRUE;

	if (ctx->quit)
		close_session = GF_TRUE;

	if (!sess->done) {
		if (!sess->is_h2 && sess->use_chunk_transfer) {
			assert(sess->nb_bytes);
			gf_dm_sess_send(sess->http_sess, "0\r\n\r\n", 5);
		} else {
			gf_dm_sess_send(sess->http_sess, NULL, 0);
		}
		if (sess->resource) gf_fclose(sess->resource);
		sess->resource = NULL;

		if (sess->nb_bytes) {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Done sending %s to %s ("LLU"/"LLU" bytes)\n", sess->path, sess->peer_address, sess->nb_bytes, sess->bytes_in_req));
		}

		log_request_done(sess);

		//keep resource active
		sess->done = GF_TRUE;
		sess->canceled = GF_FALSE;

	}
	if (close_session) {
		httpout_close_session(sess);
	}
	//might be NULL if quit was set
	else if (sess->http_sess) {
		sess->headers_done = GF_FALSE;
		gf_dm_sess_server_reset(sess->http_sess);
	}
}

static Bool httpout_open_input(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in, const char *name, Bool is_delete)
{
//	Bool reassign_clients = GF_TRUE;
	u32 len = 0;
    char *o_url = NULL;
    const char *dir = NULL;
    const char *sep;

    if (in->is_open && !is_delete) return GF_FALSE;
    if (!in->upload) {
        //singe session mode, not recording, nothing to do
        if (ctx->single_mode) {
			in->done = GF_FALSE;
			in->is_open = GF_TRUE;
			return GF_FALSE;
		}
        //server mode not recording, nothing to do
		if (!ctx->rdirs.nb_items) return GF_FALSE;
		dir = ctx->rdirs.vals[0];
		if (!dir) return GF_FALSE;
		len = (u32) strlen(dir);
		if (!len) return GF_FALSE;

        if (in->resource) return GF_FALSE;
    }

    sep = name ? strstr(name, "://") : NULL;
    if (sep) sep = strchr(sep+3, '/');
	if (!sep) {
        if (in->force_dst_name) {
            sep = in->path;
        } else if (ctx->dst) {
            u32 i, count = gf_list_count(ctx->inputs);
            for (i=0; i<count; i++) {
                char *path_sep;
                GF_HTTPOutInput *an_in = gf_list_get(ctx->inputs, i);
                if (an_in==in) continue;
                if (!an_in->path) continue;
                if (ctx->dst && !an_in->force_dst_name) continue;
                if (!gf_filter_pid_share_origin(in->ipid, an_in->ipid))
                    continue;
                
                o_url = gf_strdup(an_in->path);
                path_sep = strrchr(o_url, '/');
                if (path_sep) {
                    path_sep[1] = 0;
                    gf_dynstrcat(&o_url, name, NULL);
                    sep = o_url;
                } else {
                    sep = name;
                }
                break;
            }
		} else {
			sep = name;
		}
		if (sep && (sep[0] != '/')) {
			char *new_url = NULL;
			gf_dynstrcat(&new_url, "/", NULL);
			gf_dynstrcat(&new_url, sep, NULL);
			if (o_url) gf_free(o_url);
			sep = o_url = new_url;
		}
    }
    if (!sep) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] %s output file %s but cannot guess path !\n", is_delete ? "Deleting" : "Opening",  name));
		return GF_FALSE;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] %s output file %s\n", is_delete ? "Deleting" : "Opening",  name));
	if (in->upload) {
		GF_Err e;
		in->done = GF_FALSE;
		in->is_open = GF_TRUE;

		in->is_delete = is_delete;
		if (!in->force_dst_name) {
			char *old = in->path;
			in->path = gf_strdup(sep);
			if (old) gf_free(old);
		}
		if (o_url) gf_free(o_url);
		
		e = gf_dm_sess_setup_from_url(in->upload, in->path, GF_TRUE);
		if (!e) {
			in->cur_header = 0;
			e = gf_dm_sess_process(in->upload);
		}

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Failed to open output file %s: %s\n", in->path, gf_error_to_string(e) ));
			in->is_open = GF_FALSE;
			return GF_FALSE;
		}
		in->is_h2 = gf_dm_sess_is_h2(in->upload);

		if (is_delete) {
			//get response
			gf_dm_sess_process(in->upload);
			in->done = GF_TRUE;
			in->is_open = GF_FALSE;
			in->is_delete = GF_FALSE;
		}
		return GF_TRUE;
	}

	if (ctx->log_record) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] %s output file %s\n", is_delete ? "Deleting" : "Opening",  name));
	}

	//file delete is async (the resource associated with the input can still be active)
	if (is_delete) {
		char *loc_path = NULL;
		gf_dynstrcat(&loc_path, dir, NULL);
		if (!strchr("/\\", dir[len-1]))
			gf_dynstrcat(&loc_path, "/", NULL);
		if (sep[0]=='/')
			gf_dynstrcat(&loc_path, sep+1, NULL);
		else
			gf_dynstrcat(&loc_path, sep, NULL);

		gf_file_delete(loc_path);
		if (o_url) gf_free(o_url);
		gf_free(loc_path);
		return GF_TRUE;
	}

	in->done = GF_FALSE;
	in->is_open = GF_TRUE;


	if (in->path && !strcmp(in->path, sep)) {
//		reassign_clients = GF_FALSE;
	} else {
		if (in->path) gf_free(in->path);
		in->path = gf_strdup(sep);
	}
    if (o_url) gf_free(o_url);

	httpout_set_local_path(ctx, in);

	in->resource = gf_fopen(in->local_path, "wb");
	if (!in->resource)
		in->is_open = GF_FALSE;
	return GF_TRUE;
}

static void httpout_close_input(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in)
{
	if (!in->is_open) return;
	in->is_open = GF_FALSE;
	in->done = GF_TRUE;

	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Closing output %s\n", in->local_path ? in->local_path : in->path));

	if (in->upload) {
		GF_Err e;
		if (!in->is_h2) {
			e = gf_dm_sess_send(in->upload, "0\r\n\r\n", 5);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Error sending last chunk to %s: %s\n", in->local_path ? in->local_path : in->path, gf_error_to_string(e) ));
			}
		}
		//signal we're done sending the body
		gf_dm_sess_send(in->upload, NULL, 0);

		//fetch reply (blocking)
		e = gf_dm_sess_process(in->upload);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Error closing output %s: %s\n", in->local_path ? in->local_path : in->path, gf_error_to_string(e) ));
		}

	} else {
		u32 i, count;

		if (ctx->log_record) {
			GF_LOG(GF_LOG_INFO, GF_LOG_ALL, ("[HTTPOut] Closing output file %s\n", in->local_path ? in->local_path : in->path));
		}

		if (in->resource) {
			assert(in->local_path);
			//close all LL-HLS chunks before closing session
			httpout_close_hls_chunk(ctx, in, GF_FALSE);

			//detach all clients from this input and reassign to a regular output
			count = gf_list_count(ctx->sessions);
			for (i=0; i<count; i++) {
				GF_HTTPOutSession *sess = gf_list_get(ctx->sessions, i);
				if (sess->in_source != in) continue;
				assert(sess->file_in_progress);
				if (sess->in_source) {
					sess->in_source->nb_dest--;
					sess->in_source = NULL;
					if (!sess->resource && sess->path) {
						sess->resource = gf_fopen(sess->path, "rb");
					}
				}
				//get final size by forcing a seek
				sess->file_size = gf_fsize(sess->resource);
				gf_fseek(sess->resource, sess->file_pos, SEEK_SET);
				sess->file_in_progress = GF_FALSE;
			}
			gf_fclose(in->resource);
			in->resource = NULL;
		} else {
			count = gf_list_count(ctx->active_sessions);
			for (i=0; i<count; i++) {
				GF_HTTPOutSession *sess = gf_list_get(ctx->active_sessions, i);
				if (sess->in_source != in) continue;
				assert(sess->nb_bytes);
				if (!sess->is_h2)
					gf_dm_sess_send(sess->http_sess, "0\r\n\r\n", 5);

				//signal we're done sending the body
				gf_dm_sess_send(sess->http_sess, NULL, 0);

				log_request_done(sess);
			}
		}
	}
	in->nb_write = 0;
}
u32 httpout_write_input(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in, const u8 *pck_data, u32 pck_size, Bool file_start)
{
	u32 out=0;

	if (!in->is_open) return 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPOut] Writing %d bytes to output file %s\n", pck_size, in->local_path ? in->local_path : in->path));

	if (in->upload) {
		char szChunkHdr[100];
		u32 chunk_hdr_len=0;
		GF_Err e;
		u32 nb_retry = 0;
		out = pck_size;

		if (!in->is_h2) {
			sprintf(szChunkHdr, "%X\r\n", pck_size);
			chunk_hdr_len = (u32) strlen(szChunkHdr);
		}
retry:
		if (!in->is_h2) {
			e = gf_dm_sess_send(in->upload, szChunkHdr, chunk_hdr_len);
			if (!e) e = gf_dm_sess_send(in->upload, (u8 *) pck_data, pck_size);
			if (!e) e = gf_dm_sess_send(in->upload, "\r\n", 2);
		} else {
			e = gf_dm_sess_send(in->upload, (u8 *) pck_data, pck_size);
		}
		if (e==GF_IP_CONNECTION_CLOSED) {
			if (file_start && (nb_retry<10) ) {
				in->is_open = GF_FALSE;
				nb_retry++;
				if (httpout_open_input(ctx, in, in->path, GF_FALSE))
					goto retry;
			}
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Error writing to output file %s: %s\n", in->local_path ? in->local_path : in->path, gf_error_to_string(e) ));
			out = 0;
		}

	} else {
		char szChunkHdr[100];
		u32 chunk_hdr_len=0;
		u32 i, count = gf_list_count(ctx->active_sessions);

		if (in->resource) {
			out = (u32) gf_fwrite(pck_data, pck_size, in->resource);
			gf_fflush(in->resource);

			if (in->hls_chunk) {
				u32 wb = (u32) gf_fwrite(pck_data, pck_size, in->hls_chunk);
				if (wb != pck_size) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Write error for HLS chunk, wrote %d bytes but had %d to write\n", wb, pck_size));
				}
				gf_fflush(in->hls_chunk);
			}
		}

		for (i=0; i<count; i++) {
			GF_HTTPOutSession *sess = gf_list_get(ctx->active_sessions, i);
			if (sess->in_source != in) continue;

			if (sess->send_init_data && in->tunein_data_size && !sess->file_in_progress) {
				if (!sess->is_h2) {
					char szHdrInit[100];
					sprintf(szHdrInit, "%X\r\n", in->tunein_data_size);
					u32 len_hdr = (u32) strlen(szHdrInit);
					gf_dm_sess_send(sess->http_sess, szHdrInit, len_hdr);
					gf_dm_sess_send(sess->http_sess, in->tunein_data, in->tunein_data_size);
					gf_dm_sess_send(sess->http_sess, "\r\n", 2);
				} else {
					gf_dm_sess_send(sess->http_sess, in->tunein_data, in->tunein_data_size);
				}
				sess->nb_bytes += in->tunein_data_size;
			}
			sess->send_init_data = GF_FALSE;
			out = pck_size;

			/*source is read from disk but a different file handle is used, force refresh by using fseek (so use fsize and seek back to current pos)*/
			if (sess->file_in_progress) {
				sess->file_size = gf_fsize(sess->resource);
				gf_fseek(sess->resource, sess->file_pos, SEEK_SET);
			}
			/*source is not read from disk, write data*/
			else {
				if (!sess->is_h2) {
					if (!chunk_hdr_len) {
						sprintf(szChunkHdr, "%X\r\n", pck_size);
						chunk_hdr_len = (u32) strlen(szChunkHdr);
					}
					gf_dm_sess_send(sess->http_sess, szChunkHdr, chunk_hdr_len);
					gf_dm_sess_send(sess->http_sess, (u8*) pck_data, pck_size);
					gf_dm_sess_send(sess->http_sess, "\r\n", 2);
				} else {
					gf_dm_sess_send(sess->http_sess, (u8*) pck_data, pck_size);
				}
				sess->nb_bytes += pck_size;
			}
		}
	}
	//don't reschedule, we will be notified when new packets are ready
	ctx->next_wake_us = 0;
	return out;
}

static Bool httpout_input_write_ready(GF_HTTPOutCtx *ctx, GF_HTTPOutInput *in)
{
	u32 i, count;
	if (ctx->rdirs.nb_items)
		return GF_TRUE;

	if (in->upload) {
/*		if (!gf_sk_group_sock_is_set(ctx->sg, in->socket, GF_SK_SELECT_WRITE))
			return GF_FALSE;
*/
		return GF_TRUE;
	}

	count = gf_list_count(ctx->active_sessions);
	for (i=0; i<count; i++) {
		GF_HTTPOutSession *sess = gf_list_get(ctx->active_sessions, i);
		if (sess->in_source != in) continue;

		if (!sess->file_in_progress && !gf_sk_group_sock_is_set(ctx->sg, sess->socket, GF_SK_SELECT_WRITE))
			return GF_FALSE;
	}
	return count ? GF_TRUE : GF_FALSE;
}

static void httpin_send_seg_info(GF_HTTPOutInput *in)
{
	GF_FilterEvent evt;
	GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, in->ipid);
	evt.seg_size.seg_url = NULL;

	if (in->dash_mode==1) {
		evt.seg_size.is_init = 1;
		in->dash_mode = 2;
		evt.seg_size.media_range_start = 0;
		evt.seg_size.media_range_end = 0;
		gf_filter_pid_send_event(in->ipid, &evt);
	} else {
		evt.seg_size.is_init = 0;
		evt.seg_size.media_range_start = in->offset_at_seg_start;
		evt.seg_size.media_range_end = in->nb_write-1;
		in->offset_at_seg_start = 1+evt.seg_size.media_range_end;
		gf_filter_pid_send_event(in->ipid, &evt);
	}
}

static void httpout_process_inputs(GF_HTTPOutCtx *ctx)
{
	u32 i, nb_eos=0, nb_nopck=0, count = gf_list_count(ctx->inputs);
	for (i=0; i<count; i++) {
		Bool start, end;
		const GF_PropertyValue *p;
		const u8 *pck_data;
		u32 pck_size, nb_write;
		GF_FilterPacket *pck;
		GF_HTTPOutInput *in = gf_list_get(ctx->inputs, i);
		if (in->in_error) continue;

		//not sending/writing anything, delete files
		if (!in->is_open && in->file_deletes) {
			while (gf_list_count(in->file_deletes)) {
				char *url = gf_list_pop_back(in->file_deletes);
				httpout_open_input(ctx, in, url, GF_TRUE);
				gf_free(url);
			}
			gf_list_del(in->file_deletes);
			in->file_deletes = NULL;
		}

		//no destination and holding for first connect, don't drop
		if (!ctx->hmode && !ctx->rdirs.nb_items && !in->nb_dest && in->hold) {
			continue;
		}

		pck = gf_filter_pid_get_packet(in->ipid);
		if (!pck) {
			nb_nopck++;
			//check end of PID state
			if (gf_filter_pid_is_eos(in->ipid)) {
				nb_eos++;
				if (in->dash_mode && in->is_open) {
					httpin_send_seg_info(in);
				}
				httpout_close_input(ctx, in);
			}
			continue;
		}


		gf_filter_pck_get_framing(pck, &start, &end);

		if (in->dash_mode) {
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p) {
				httpin_send_seg_info(in);

				if ( gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME))
					start = GF_TRUE;
			}
		}

		if (start) {
			const GF_PropertyValue *fnum, *fname;
			const char *name = NULL;
			fname = NULL;

			if (in->is_open)
				httpout_close_input(ctx, in);

			//file num increased per packet, open new file
			fnum = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (fnum) {
				fname = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_OUTPATH);
				//if (!fname) name = ctx->dst;
			}
			//filename change at packet start, open new file
			if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
			if (!fname) fname = gf_filter_pck_get_property(pck, GF_PROP_PID_OUTPATH);
			if (fname) name = fname->value.string;

			if (!name) {
				/*if PID was connected to an alias, get the alias context to get the destination
				Otherwise PID was directly connected to the main filter, use main filter destination*/
				GF_HTTPOutCtx *orig_ctx = gf_filter_pid_get_alias_udta(in->ipid);
				if (!orig_ctx) orig_ctx = ctx;

				if (orig_ctx->dst_in && (orig_ctx->dst_in != in) ) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Mutliple input PIDs with no file name set, broken graph, discarding input %s\n\tYou may retry by adding a new http output filter\n", gf_filter_pid_get_name(in->ipid) ));

					gf_filter_pid_set_discard(in->ipid, GF_TRUE);
					in->in_error = GF_TRUE;
					continue;
				} else {
					name = orig_ctx->dst;
					orig_ctx->dst_in = in;
				}
			} else if (in->force_dst_name && !ctx->dst_in) {
				ctx->dst_in = in;
			}

			httpout_open_input(ctx, in, name, GF_FALSE);

			if (!ctx->hmode && !ctx->rdirs.nb_items && !in->nb_dest) {
				if ((gf_filter_pck_get_dependency_flags(pck)==0xFF) && (gf_filter_pck_get_carousel_version(pck)==1)) {
					pck_data = gf_filter_pck_get_data(pck, &pck_size);
					if (pck_data) {
						in->tunein_data_size = pck_size;
						in->tunein_data = gf_realloc(in->tunein_data, pck_size);
						memcpy(in->tunein_data, pck_data, pck_size);
					}
				}
			}
		}

		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_HLS_FRAG_NUM);
		if (p && in->resource) {
			char szHLSChunk[GF_MAX_PATH];
			snprintf(szHLSChunk, GF_MAX_PATH-1, "%s.%d", in->local_path, p->value.uint);
			httpout_close_hls_chunk(ctx, in, GF_FALSE);
			in->hls_chunk = gf_fopen(szHLSChunk, "w+b");
			in->hls_chunk_local_path = gf_strdup(szHLSChunk);
			snprintf(szHLSChunk, GF_MAX_PATH-1, "%s.%d", in->path, p->value.uint);
			in->hls_chunk_path = gf_strdup(szHLSChunk);
		}

		//no destination and not holding packets (either first connection not here or disabled), trash packet
		if (!ctx->hmode && !ctx->rdirs.nb_items && !in->nb_dest && !in->hold) {
			gf_filter_pid_drop_packet(in->ipid);
			continue;
		}

		if (!httpout_input_write_ready(ctx, in)) {
			continue;
		}

		pck_data = gf_filter_pck_get_data(pck, &pck_size);
		if (in->upload || ctx->single_mode || in->resource) {
			GF_FilterFrameInterface *hwf = gf_filter_pck_get_frame_interface(pck);
			if (pck_data && pck_size) {

				if (in->patch_blocks && gf_filter_pck_get_seek_flag(pck)) {
					u64 bo = gf_filter_pck_get_byte_offset(pck);
					if (bo==GF_FILTER_NO_BO) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Cannot patch file, wrong byte offset\n"));
					} else {
						if (gf_filter_pck_get_interlaced(pck)) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Cannot patch file by byte insertion, not supported by HTTP\n"));
						} else {
							u64 pos = in->nb_write;
							//close file
							httpout_close_input(ctx, in);
							//re-open file
							in->write_start_range = bo;
							in->write_end_range = bo + pck_size - 1;
							httpout_open_input(ctx, in, in->path, GF_FALSE);

							nb_write = httpout_write_input(ctx, in, pck_data, pck_size, start);
							if (nb_write!=pck_size) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
							}
							httpout_close_input(ctx, in);

							in->write_start_range = pos;
							in->write_end_range = 0;
						}
					}
				} else {
					if (in->write_start_range) {
						httpout_open_input(ctx, in, in->path, GF_FALSE);
					}

					nb_write = httpout_write_input(ctx, in, pck_data, pck_size, start);
					if (nb_write!=pck_size) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Write error, wrote %d bytes but had %d to write\n", nb_write, pck_size));
					}
					in->nb_write += nb_write;
				}
			} else if (hwf) {
				u32 w, h, stride, stride_uv, pf;
				u32 nb_planes, uv_height;
				p = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_WIDTH);
				w = p ? p->value.uint : 0;
				p = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_HEIGHT);
				h = p ? p->value.uint : 0;
				p = gf_filter_pid_get_property(in->ipid, GF_PROP_PID_PIXFMT);
				pf = p ? p->value.uint : 0;

				stride = stride_uv = 0;

				if (gf_pixel_get_size_info(pf, w, h, NULL, &stride, &stride_uv, &nb_planes, &uv_height) == GF_TRUE) {
					u32 k;
					for (k=0; k<nb_planes; k++) {
						u32 j, write_h, lsize;
						const u8 *out_ptr;
						u32 out_stride = k ? stride_uv : stride;
						GF_Err e = hwf->get_plane(hwf, k, &out_ptr, &out_stride);
						if (e) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Failed to fetch plane #%d data from hardware frame, cannot write\n", k));
							break;
						}
						if (k) {
							write_h = uv_height;
							lsize = stride_uv;
						} else {
							write_h = h;
							lsize = stride;
						}
						for (j=0; j<write_h; j++) {
							nb_write = (u32) httpout_write_input(ctx, in, out_ptr, lsize, start);
							if (nb_write!=lsize) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] Write error, wrote %d bytes but had %d to write\n", nb_write, lsize));
							}
							in->nb_write += nb_write;
							out_ptr += out_stride;
							start = GF_FALSE;
						}
					}
				}
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPOut] No data associated with packet, cannot write\n"));
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPOut] output file handle is not opened, discarding %d bytes\n", pck_size));
		}
		gf_filter_pid_drop_packet(in->ipid);
		if (end) {
			httpout_close_input(ctx, in);
		}
	}

	if (count && (nb_eos==count)) {
		if (ctx->rdirs.nb_items) {
			if (gf_list_count(ctx->active_sessions))
				gf_filter_post_process_task(ctx->filter);
			else
				ctx->done = GF_TRUE;
		}
		else
			ctx->done = GF_TRUE;
	}
	//push mode and no packets on inputs, do not ask for RT reschedule (we will get called if new packets are to be processed)
	if ((nb_nopck==count) && (ctx->hmode==MODE_PUSH))
		ctx->next_wake_us = 0;
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

	e = gf_sk_group_select(ctx->sg, 10, GF_SK_SELECT_BOTH);
	if ((e==GF_OK) && ctx->server_sock) {
		//server mode, check pending connections
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
			if (! sess->http_sess) {
				httpout_del_session(sess);
				i--;
				count--;
				if (!count && ctx->quit)
					ctx->done = GF_TRUE;
				continue;
			}

			if (sess->sub_sess_pending) {
				sess->sub_sess_pending = GF_FALSE;
				count = gf_list_count(ctx->active_sessions);
				i = -1;
			}
		}
	}

	httpout_process_inputs(ctx);

	if (ctx->timeout && ctx->server_sock) {
		count = gf_list_count(ctx->active_sessions);
		for (i=0; i<count; i++) {
			u32 diff_sec;
			GF_HTTPOutSession *sess = gf_list_get(ctx->active_sessions, i);
			if (!sess->done) continue;

			diff_sec = (u32) (gf_sys_clock_high_res() - sess->last_active_time)/1000000;
			if (diff_sec>ctx->timeout) {
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTPOut] Timeout for peer %s after %d sec, closing connection (last request %s)\n", sess->peer_address, diff_sec, sess->in_source ? sess->in_source->path : sess->path ));

				httpout_close_session(sess);

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

	//reschedule was canceled but we still have active sessions, reschedule for our default timeout
	if (!ctx->next_wake_us && gf_list_count(ctx->active_sessions)) {
		ctx->next_wake_us = 50000;
	}

	if (ctx->next_wake_us) {
		gf_filter_ask_rt_reschedule(filter, ctx->next_wake_us);
	}

	return e;
}

static Bool httpout_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_HTTPOutInput *in;
	GF_HTTPOutCtx *ctx;
	if (evt->base.type!=GF_FEVT_FILE_DELETE)
		return GF_FALSE;

	if (!evt->base.on_pid) return GF_TRUE;
	in = gf_filter_pid_get_udta(evt->base.on_pid);
	if (!in) return GF_TRUE;

	ctx = (GF_HTTPOutCtx *) gf_filter_get_udta(filter);
		//simple server mode (no record, no push), nothing to do
	if (!in->upload && !ctx->rdirs.nb_items) return GF_TRUE;

	if (!in->file_deletes)
		in->file_deletes = gf_list_new();
	gf_list_add(in->file_deletes, gf_strdup(evt->file_del.url));
	return GF_TRUE;
}

static GF_FilterProbeScore httpout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "http://", 7)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "https://", 8)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static Bool httpout_use_alias(GF_Filter *filter, const char *url, const char *mime)
{
	u32 len;
	char *sep;
	GF_HTTPOutCtx *ctx = (GF_HTTPOutCtx *) gf_filter_get_udta(filter);

	//check we have same hostname. If so, accept this destination as a source for our filter
	sep = strstr(url, "://");
	if (!sep) return GF_FALSE;
	sep += 3;
	sep = strchr(sep, '/');
	if (!sep) {
		if (!strcmp(ctx->dst, url)) return GF_TRUE;
		return GF_FALSE;
	}
	len = (u32) (sep - url);
	if (!strncmp(ctx->dst, url, len)) return GF_TRUE;
	return GF_FALSE;
}

static const GF_FilterCapability HTTPOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_FILE_EXT, "*"),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_MIME, "*"),
	{0},
	CAP_UINT(GF_CAPS_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};


#define OFFS(_n)	#_n, offsetof(GF_HTTPOutCtx, _n)
static const GF_FilterArgs HTTPOutArgs[] =
{
	{ OFFS(dst), "location of destination resource - see filter help", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(port), "server port", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(ifce), "default network interface to use", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(rdirs), "list of directories to expose for read - see filter help", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ OFFS(wdir), "directory to expose for write - see filter help", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(cert), "certificate file in PEM format to use for TLS mode", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(pkey), "private key file in PEM format to use for TLS mode", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read and write TCP socket", GF_PROP_UINT, "10000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(user_agent), "user agent string, by default solved from GPAC preferences", GF_PROP_STRING, "$GUA", NULL, 0},
	{ OFFS(close), "close HTTP connection after each request", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(maxc), "maximum number of connections, 0 is unlimited", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(maxp), "maximum number of connections for one peer, 0 is unlimited", GF_PROP_UINT, "6", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(cache_control), "specify the `Cache-Control` string to add; `none` disable ETag", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(hold), "hold packets until one client connects", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(hmode), "filter operation mode, ignored if [-wdir]() is set. See filter help for more details. Mode can be\n"
	"- default: run in server mode (see filter help)\n"
	"- push: run in client mode using PUT or POST (see filter help)\n"
	"- source: use server as source filter on incoming PUT/POST", GF_PROP_UINT, "default", "default|push|source", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timeout), "timeout in seconds for persistent connections; 0 disable timeout", GF_PROP_UINT, "30", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ext), "set extension for graph resolution, regardless of file extension", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mime), "set mime type for graph resolution", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(quit), "exit server once all input PIDs are done and client disconnects (for test purposes)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(post), "use POST instead of PUT for uploading files", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dlist), "enable HTML listing for GET requests on directories", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(sutc), "insert server UTC in response headers as `Server-UTC: VAL_IN_MS`", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(cors), "insert CORS header allowing all domains\n"
		"- off: disable CORS\n"
		"- on: enable CORS\n"
		"- auto: enable CORS when `Origin` is found in request", GF_PROP_UINT, "auto", "auto|off|on", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(reqlog), "provide short log of the requests indicated in this option (comma separated list, `*` for all) regardless of HTTP log settings. Value `REC` logs file writing start/end", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ice), "insert ICE meta-data in response headers in sink mode - see filter help", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(max_client_errors), "force disconnection after specified number of consecutive errors from HTTTP 1.1 client (ignored in H/2 or when `close` is set)", GF_PROP_UINT, "20", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};


GF_FilterRegister HTTPOutRegister = {
	.name = "httpout",
	GF_FS_SET_DESCRIPTION("HTTP Server")

	GF_FS_SET_HELP("The HTTP output filter can act as:\n"
		"- a simple HTTP server\n"
		"- an HTTP server sink\n"
		"- an HTTP server file sink\n"
		"- an HTTP __client__ sink\n"
		"- an HTTP server __source__\n"
		"  \n"
		"The server currently handles GET, HEAD, PUT, POST, DELETE methods, and basic OPTIONS support.\n"
		"Single or multiple byte ranges are supported for both GET and PUT/POST methods, in all server modes.\n"
		"- for GET, the resulting body is a single-part body formed by the concatenated byte ranges as requested (no overlap checking).\n"
		"- for PUT/POST, the received data is pushed to the target file according to the byte ranges specified in the client request.\n"
		"  \n"
		"Warning: the partial PUT request is RFC2616 compliant but not compliant with RFC7230. PATCH method is not yet implemented in GPAC.\n"
		"  \n"
		"When a single read directory is specified, the server root `/` is the content of this directory.\n"
		"When multiple read directories are specified, the server root `/` contains the list of the mount points with their directory names.\n"
		"When a write directory is specified, the upload resource name identifies a file in this directory (the write directory name is not present in the URL).\n"
		"  \n"
		"Listing can be enabled on server using [-dlist]().\n"
		"When disabled, a GET on a directory will fail.\n"
		"When enabled, a GET on a directory will return a simple HTML listing of the content inspired from Apache.\n"
		"  \n"
		"# Simple HTTP server\n"
		"In this mode, the filter does not need any input connection and exposes all files in the directories given by [-rdirs]().\n"
		"PUT and POST methods are only supported if a write directory is specified by [-wdir]() option.\n"
		"EX gpac httpout:rdirs=outcoming\n"
		"This sets up a read-only server.\n"
		"  \n"
		"EX gpac httpout:wdir=incoming\n"
		"This sets up a write-only server.\n"
		"  \n"
		"EX gpac httpout:rdirs=outcoming:wdir=incoming:port=8080\n"
		"This sets up a read-write server running on [-port]() 8080.\n"
		"  \n"
		"# HTTP server sink\n"
		"In this mode, the filter will forward input PIDs to connected clients, trashing the data if no client is connected unless [-hold]() is specified.\n"
		"The filter does not use any read directory in this mode.\n"
		"This mode is mostly useful to setup live HTTP streaming of media sessions such as MP3, MPEG-2 TS or other muxed representations:\n"
		"EX gpac -i MP3_SOURCE -o http://localhost/live.mp3 --hold\n"
		"In this example, the server waits for client requests on `/live.mp3` and will then push each input packet to all connected clients.\n"
		"If the source is not real-time, you can inject a reframer filter performing realtime regulation.\n"
		"EX gpac -i MP3_SOURCE reframer:rt=on @ -o http://localhost/live.mp3\n"
		"In this example, the server will push each input packet to all connected clients, or trash the packet if no connected clients.\n"
		"  \n"
		"In this mode, ICECast meta-data can be inserted using [-ice](). The default inserted values are `ice-audio-info`, `icy-br`, `icy-pub` (set to 1) and `icy-name` if input `ServiceName` property is set.\n"
		"The server will also look for any property called `ice-*` on the input pid and inject them.\n"
		"EX gpac -i source.mp3:#ice-Genre=CoolRock -o http://IP/live.mp3 --ice\n"
		"This will inject the header `ice-Genre: CoolRock` in the response."
		"  \n"
		"# HTTP server file sink\n"
		"In this mode, the filter will write input PIDs to files in the first read directory specified, acting as a file output sink.\n"
		"The filter uses a read directory in this mode, which must be writable.\n"
		"Upon client GET request, the server will check if the requested URL matches the name of a file currently being written by the server.\n"
		"- If so, the server will:\n"
		"  - send the content using HTTP chunk transfer mode, starting with what is already written on disk\n"
		"  - push remaining data to the client as soon as received while writing it to disk, until source file is done\n"
		"- If not so, the server will simply send the file from the disk as a regular HTTP session, without chunk transfer.\n"
		"  \nThis mode is typically used for origin server in HAS sessions where clients may request files while they are being produced (low latency DASH).\n"
		"EX gpac -i SOURCE reframer:rt=on @ -o http://localhost:8080/live.mpd --rdirs=temp --dmode=dynamic --cdur=0.1\n"
		"In this example, a real-time dynamic DASH session with chunks of 100ms is created, outputting files in `temp`. A client connecting to the live edge will receive segments as they are produced using HTTP chunk transfer.\n"
		"  \n"
		"# HTTP client sink\n"
		"In this mode, the filter will upload input PIDs data to remote server using PUT (or POST if [-post]() is set).\n"
		"This mode must be explicitly activated using [-hmode]().\n"
		"The filter uses no read or write directories in this mode.\n"
		"EX gpac -i SOURCE -o http://targethost:8080/live.mpd:gpac:hmode=push\n"
		"In this example, the filter will send PUT methods to the server running on [-port]() 8080 at `targethost` location (IP address or name).\n"
		"  \n"
		"# HTTP server source\n"
		"In this mode, the server acts as a source rather than a sink. It declares incoming PUT or POST methods as output PIDs\n"
		"This mode must be explicitly activated using [-hmode]().\n"
		"The filter uses no read or write directories in this mode, and uploaded data is NOT stored by the server.\n"
		"EX gpac httpout:hmode=source vout aout\n"
		"In this example, the filter will try to play uploaded files through video and audio output.\n"
		"  \n"
		"# HTTPS server\n"
		"The server can run over TLS (https) for all the server modes. TLS is enabled by specifying [-cert]() and [-pkey]() options.\n"
		"Both certificate and key must be in PEM format.\n"
		"The server currently only operates in either HTTPS or HTTP mode and cannot run both modes at the same time. You will need to use two httpout filters for this, one operating in HTTPS and one operating in HTTP.\n"
		"  \n"
		"# Multiple destinations on single server\n"
		"When running in server mode, multiple HTTP outputs with same URL/port may be used:\n"
		"- the first loaded HTTP output filter with same URL/port will be reused\n"
		"- all httpout options of subsequent httpout filters, except [-dst]() will be ignored, other options will be inherited as usual\n"
		"\n"
		"EX -i dash.mpd dashin:forward=file:SID=D1 dashin:forward=segb:SID=D2 -o http://localhost:80/live.mpd:SID=D1:rdirs=dash -o http://localhost:80/live_rw.mpd:SID=D2:sigfrag\n"
		"This will:\n"
		"- load the HTTP server and forward (through `D1`) the dash session to this server using `live.mpd` as manifest name\n"
		"- reuse the HTTP server and regenerate the manifest (through `D2` and `sigfrag` option), using `live_rw.mpd` as manifest name\n"
		)
	.private_size = sizeof(GF_HTTPOutCtx),
	.max_extra_pids = -1,
	.args = HTTPOutArgs,
	.probe_url = httpout_probe_url,
	.initialize = httpout_initialize,
	.finalize = httpout_finalize,
	SETCAPS(HTTPOutCaps),
	.configure_pid = httpout_configure_pid,
	.process = httpout_process,
	.process_event = httpout_process_event,
	.use_alias = httpout_use_alias
};


const GF_FilterRegister *httpout_register(GF_FilterSession *session)
{
	return &HTTPOutRegister;
}

