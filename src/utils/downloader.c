/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#include <gpac/tools.h>
#ifndef GPAC_DISABLE_NETWORK
#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/token.h>
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/base_coding.h>
#include <gpac/cache.h>
#include <gpac/filters.h>

#ifdef GPAC_HAS_SSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#ifdef GPAC_HAS_HTTP2
#if defined(_MSC_VER)
typedef SSIZE_T ssize_t;
#define NGHTTP2_STATICLIB
#else
#if defined(WIN32) && defined(GPAC_STATIC_BUILD)
#define NGHTTP2_STATICLIB
#endif
#endif
#include <nghttp2/nghttp2.h>

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#pragma comment(lib, "nghttp2")
# endif
#endif

#endif



#ifdef __USE_POSIX
#include <unistd.h>
#endif

#define SIZE_IN_STREAM ( 2 << 29 )


#define SESSION_RETRY_COUNT	10
#define SESSION_RETRY_SSL	8

#include <gpac/revision.h>
#define GF_DOWNLOAD_AGENT_NAME		"GPAC/"GPAC_VERSION "-rev" GPAC_GIT_REVISION

//let's be agressive with socket buffer size
#define GF_DOWNLOAD_BUFFER_SIZE		131072
#define GF_DOWNLOAD_BUFFER_SIZE_LIMIT_RATE		GF_DOWNLOAD_BUFFER_SIZE/20


#ifdef GPAC_HAS_HTTP2
#define HTTP2_BUFFER_SETTINGS_SIZE 128


typedef struct
{
	u8 * data;
	u32 size, alloc, offset;
} h2_reagg_buffer;

typedef struct
{
	Bool do_shutdown;
	GF_List *sessions;
	nghttp2_session *ng_sess;

	GF_DownloadSession *net_sess;
	GF_Mutex *mx;
	Bool copy;
} GF_H2_Session;


#endif

static void gf_dm_data_received(GF_DownloadSession *sess, u8 *payload, u32 payload_size, Bool store_in_init, u32 *rewrite_size, u8 *original_payload);
static GF_Err gf_dm_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read);

static void gf_dm_connect(GF_DownloadSession *sess);
GF_Err gf_dm_sess_send(GF_DownloadSession *sess, u8 *data, u32 size);
GF_Err gf_dm_sess_flush_async(GF_DownloadSession *sess, Bool no_select);

/*internal flags*/
enum
{
	GF_DOWNLOAD_SESSION_USE_SSL     = 1<<10,
	GF_DOWNLOAD_SESSION_THREAD_DEAD = 1<<11,
	GF_DOWNLOAD_SESSION_SSL_FORCED = 1<<12,
};

enum REQUEST_TYPE
{
	GET = 0,
	HEAD = 1,
	OTHER = 2
};

/*!the structure used to store an HTTP header*/
typedef struct
{
	char *name;
	char *value;
} GF_HTTPHeader;


/**
 * This structure handles partial downloads
 */
typedef struct __partialDownloadStruct {
	char * url;
	u64 startOffset;
	u64 endOffset;
	char * filename;
} GF_PartialDownload ;

typedef struct
{
	struct __gf_download_session *sess;
} GF_SessTask;

struct __gf_download_session
{
	/*this is always 0 and helps differenciating downloads from other interfaces (interfaceType != 0)*/
	u32 reserved;

	struct __gf_download_manager *dm;
	GF_Thread *th;
	GF_Mutex *mx;
	GF_SessTask *ftask;

	Bool in_callback, destroy;
	u32 proxy_enabled;
	Bool allow_direct_reuse;

	char *server_name;
	u16 port;

	char *orig_url;
	char *orig_url_before_redirect;
	char *remote_path;
	GF_UserCredentials * creds;
	char cookie[GF_MAX_PATH];
	DownloadedCacheEntry cache_entry;
	Bool reused_cache_entry, from_cache_only;

	//mime type, only used when the session is not cached.
	char *mime_type;
	GF_List *headers;

	const char *netcap_id;
	GF_Socket *sock;
	u32 num_retry;
	GF_NetIOStatus status;

	u32 flags;
	u32 total_size, bytes_done, icy_metaint, icy_count, icy_bytes;
	u64 start_time;

	u32 bytes_per_sec;
	u64 start_time_utc;
	Bool last_chunk_found;
	Bool connection_close;
	Bool is_range_continuation;
	/*0: no cache reconfig before next GET request: 1: try to rematch the cache entry: 2: force to create a new cache entry (for byte-range cases)*/
	u32 needs_cache_reconfig;
	/* Range information if needed for the download (cf flag) */
	Bool needs_range;
	u64 range_start, range_end;

	u32 connect_time, ssl_setup_time, reply_time, total_time_since_req, req_hdr_size, rsp_hdr_size;

	/*0: GET
	  1: HEAD
	  2: all the rest
	*/
	enum REQUEST_TYPE http_read_type;

	GF_Err last_error;
	char *init_data;
	u32 init_data_size;
	Bool server_only_understand_get;
	/* True if cache file must be stored on disk */
	Bool use_cache_file;
	Bool disable_cache;
	/*forces notification of data exchange to be sent regardless of threading mode*/
	Bool force_data_write_callback;
	u32 connect_pending;
#ifdef GPAC_HAS_SSL
	SSL *ssl;
#endif

	void (*do_requests)(struct __gf_download_session *);

	/*callback for data reception - may not be NULL*/
	gf_dm_user_io user_proc;
	void *usr_cbk;
	Bool reassigned;

	Bool chunked;
	u32 nb_left_in_chunk;
	u32 current_chunk_size;
	u64 current_chunk_start;

	u64 request_start_time, last_fetch_time;
	/*private extension*/
	void *ext;

	char *remaining_data;
	u32 remaining_data_size;

	u32 conn_timeout, request_timeout;

	Bool local_cache_only;
	Bool server_mode;
	//0: not PUT/POST, 1: waiting for body to be completed, 2: body done
	u32 put_state;

	u64 last_cap_rate_time;
	u64 last_cap_rate_bytes;
	u32 last_cap_rate_bytes_per_sec;

	u64 last_chunk_start_time;
	u32 chunk_wnd_dur;
	u32 chunk_bytes, chunk_header_bytes, cumulated_chunk_header_bytes;
	//in bytes per seconds
	Double cumulated_chunk_rate;

	u32 connection_timeout_ms;

	u8 *async_req_reply;
	u32 async_req_reply_size;
	u8 *async_buf;
	u32 async_buf_size, async_buf_alloc;

	GF_SockGroup *sock_group;
#ifdef GPAC_HAS_HTTP2
	//HTTP/2 session used by this download session. If not NULL, the mutex, socket and ssl context are moved along sessions
	GF_H2_Session *h2_sess;
	int32_t h2_stream_id;
	h2_reagg_buffer h2_buf;

	nghttp2_data_provider data_io;
	u8 *h2_send_data;
	u32 h2_send_data_len;

	u8 *h2_local_buf;
	u32 h2_local_buf_len, h2_local_buf_alloc;

	u8 *h2_upgrade_settings;
	u32 h2_upgrade_settings_len;
	u8 h2_headers_seen, h2_ready_to_send, h2_is_eos, h2_data_paused, h2_upgrade_state, h2_data_done, h2_switch_sess;
#endif
};
static GF_Err dm_sess_write(GF_DownloadSession *sess, const u8 *buffer, u32 size);

void dm_sess_sk_del(GF_DownloadSession *sess)
{
	if (sess->sock) {
		GF_Socket *sock = sess->sock;
		sess->sock = NULL;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[Downloader] closing socket\n"));
		if (sess->sock_group) gf_sk_group_unregister(sess->sock_group, sock);
		gf_sk_del(sock);
#ifdef GPAC_HAS_HTTP2
		sess->h2_upgrade_state = 0;
#endif
	}
}

struct __gf_download_manager
{
	GF_Mutex *cache_mx;
	char *cache_directory;

	gf_dm_get_usr_pass get_user_password;
	void *usr_cbk;

	GF_List *sessions;
	Bool disable_cache, simulate_no_connection, allow_offline_cache, clean_cache;
	u32 limit_data_rate, read_buf_size;
	u64 max_cache_size;
	Bool allow_broken_certificate;

	GF_List *skip_proxy_servers;
	GF_List *credentials;
	GF_List *cache_entries;
	/* FIXME : should be placed in DownloadedCacheEntry maybe... */
	GF_List *partial_downloads;
#ifdef GPAC_HAS_SSL
	SSL_CTX *ssl_ctx;
#endif

	GF_FilterSession *filter_session;

#ifdef GPAC_HAS_HTTP2
	Bool disable_http2;
#endif

	Bool (*local_cache_url_provider_cbk)(void *udta, char *url, Bool cache_destroy);
	void *lc_udta;
};

#ifdef GPAC_HAS_SSL

static void init_prng (void)
{
	char namebuf[256];
	const char *random_file;

	if (RAND_status ()) return;

	namebuf[0] = '\0';
	random_file = RAND_file_name (namebuf, sizeof (namebuf));

	if (random_file && *random_file)
		RAND_load_file(random_file, 16384);

	if (RAND_status ()) return;
}

#endif

#if 0
#define SET_LAST_ERR(_e) \
	{\
	GF_Err __e = _e;\
	if (__e==GF_IP_NETWORK_EMPTY) {\
		assert(sess->status != GF_NETIO_STATE_ERROR); \
	}\
	sess->last_error = _e;\
	}\

#else

#define SET_LAST_ERR(_e) sess->last_error = _e;

#endif

/*HTTP2 callbacks*/
#ifdef GPAC_HAS_HTTP2

#ifdef GPAC_HAS_SSL
/* NPN TLS extension client callback. We check that server advertised
   the HTTP/2 protocol the nghttp2 library supports. If not, exit
   the program. */
static int h2_select_next_proto_cb(SSL *ssl , unsigned char **out,
                                unsigned char *outlen, const unsigned char *in,
                                unsigned int inlen, void *arg)
{
	if (nghttp2_select_next_protocol(out, outlen, in, inlen) <= 0) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] Server did not advertise " NGHTTP2_PROTO_VERSION_ID));
	}
	return SSL_TLSEXT_ERR_OK;
}
#endif

//detach session from HTTP2 session - the session mutex SHALL be grabbed before calling this
void h2_detach_session(GF_H2_Session *h2_sess, GF_DownloadSession *sess)
{
	if (!h2_sess || !sess) return;
	assert(sess->h2_sess == h2_sess);
	assert(sess->mx);

	gf_list_del_item(h2_sess->sessions, sess);
	if (!gf_list_count(h2_sess->sessions)) {
		dm_sess_sk_del(sess);

#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[Downloader] shut down SSL context\n"));
			SSL_shutdown(sess->ssl);
			SSL_free(sess->ssl);
			sess->ssl = NULL;
		}
#endif
		//destroy h2 session
		nghttp2_session_del(h2_sess->ng_sess);
		gf_list_del(h2_sess->sessions);
		gf_mx_v(h2_sess->mx);
		gf_mx_del(sess->mx);
		gf_free(h2_sess);
	} else {
		GF_DownloadSession *asess = gf_list_get(h2_sess->sessions, 0);
		assert(asess->h2_sess == h2_sess);
		assert(asess->sock);

		h2_sess->net_sess = asess;
		//swap async buf if any to new active session
		if (sess->async_buf_alloc) {
			assert(!asess->async_buf);
			asess->async_buf = sess->async_buf;
			asess->async_buf_alloc = sess->async_buf_alloc;
			asess->async_buf_size = sess->async_buf_size;
			sess->async_buf_alloc = sess->async_buf_size = 0;
			sess->async_buf = NULL;
		}
		sess->sock = NULL;
#ifdef GPAC_HAS_SSL
		sess->ssl = NULL;
#endif
		gf_mx_v(sess->mx);
	}

	if (sess->h2_buf.data) {
		gf_free(sess->h2_buf.data);
		memset(&sess->h2_buf, 0, sizeof(h2_reagg_buffer));
	}
	sess->h2_sess = NULL;
	sess->mx = NULL;
}

static GF_Err h2_session_send(GF_DownloadSession *sess)
{
	assert(sess->h2_sess);
	int rv = nghttp2_session_send(sess->h2_sess->ng_sess);
	if (rv != 0) {
		if (sess->last_error != GF_IP_CONNECTION_CLOSED) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] session_send error :  %s\n", nghttp2_strerror(rv)));
		}
		if (sess->status != GF_NETIO_STATE_ERROR) {
			sess->status = GF_NETIO_STATE_ERROR;
			if (rv==NGHTTP2_ERR_NOMEM) SET_LAST_ERR(GF_OUT_OF_MEM)
			else SET_LAST_ERR(GF_SERVICE_ERROR)
		}
		return sess->last_error;
	}
//	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] session_send OK\n"));
	return GF_OK;
}

static GF_DownloadSession *h2_get_session(void *user_data, s32 stream_id, Bool can_reassign)
{
	u32 i, nb_sess;
	GF_DownloadSession *first_not_assigned = NULL;
	GF_H2_Session *h2sess = (GF_H2_Session *)user_data;

	nb_sess = gf_list_count(h2sess->sessions);
	for (i=0;i<nb_sess; i++) {
		GF_DownloadSession *s = gf_list_get(h2sess->sessions, i);
		if (s->h2_stream_id == stream_id)
			return s;

		if (s->server_mode && !s->h2_stream_id && !first_not_assigned) {
			first_not_assigned = s;
		}
	}
	if (can_reassign && first_not_assigned) {
		first_not_assigned->h2_stream_id = stream_id;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] reassigning old server session to new stream %d\n", stream_id));
		assert(first_not_assigned->data_io.source.ptr);
		first_not_assigned->total_size = first_not_assigned->bytes_done = 0;
		first_not_assigned->status = GF_NETIO_CONNECTED;
		first_not_assigned->h2_data_done = 0;
		return first_not_assigned;
	}
	return NULL;
}

static int h2_header_callback(nghttp2_session *session,
								const nghttp2_frame *frame, const uint8_t *name,
								size_t namelen, const uint8_t *value,
								size_t valuelen, uint8_t flags ,
								void *user_data)
{
	GF_DownloadSession *sess;

	switch (frame->hd.type) {
	case NGHTTP2_HEADERS:
		sess = h2_get_session(user_data, frame->hd.stream_id, GF_FALSE);
		if (!sess)
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		if (
			(!sess->server_mode && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE))
			|| (sess->server_mode && ((frame->headers.cat == NGHTTP2_HCAT_HEADERS) || (frame->headers.cat == NGHTTP2_HCAT_REQUEST)))
		) {
			GF_HTTPHeader *hdrp;

			GF_SAFEALLOC(hdrp, GF_HTTPHeader);
			if (hdrp) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d got header %s: %s\n", sess->h2_stream_id, name, value));
				hdrp->name = gf_strdup(name);
				hdrp->value = gf_strdup(value);
				gf_list_add(sess->headers, hdrp);
			}
		break;
		}
	}
	return 0;
}

static int h2_begin_headers_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
	GF_DownloadSession *sess = h2_get_session(user_data, frame->hd.stream_id, GF_TRUE);
	if (!sess) {
		GF_H2_Session *h2sess = (GF_H2_Session *)user_data;
		GF_DownloadSession *par_sess = h2sess->net_sess;
		assert(par_sess);
		if (!par_sess->server_mode)
			return NGHTTP2_ERR_CALLBACK_FAILURE;


		if (par_sess->user_proc) {
			GF_NETIO_Parameter param;
			memset(&param, 0, sizeof(GF_NETIO_Parameter));
			param.msg_type = GF_NETIO_REQUEST_SESSION;
			par_sess->in_callback = GF_TRUE;
			param.sess = par_sess;
			param.reply = frame->hd.stream_id;
			par_sess->user_proc(par_sess->usr_cbk, &param);
			par_sess->in_callback = GF_FALSE;

			if (param.error == GF_OK) {
				sess = h2_get_session(user_data, frame->hd.stream_id, GF_FALSE);
			}
		}

		if (!sess)
			return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	switch (frame->hd.type) {
	case NGHTTP2_HEADERS:
		if (sess->server_mode) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d header callback\n", frame->hd.stream_id));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) header callback\n", frame->hd.stream_id, sess->remote_path));
		}
		break;
	}
	return 0;
}

static int h2_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
	GF_DownloadSession *sess;
	switch (frame->hd.type) {
	case NGHTTP2_HEADERS:
		sess = h2_get_session(user_data, frame->hd.stream_id, GF_FALSE);
		if (!sess)
			return NGHTTP2_ERR_CALLBACK_FAILURE;

		if (
			(!sess->server_mode && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE))
			|| (sess->server_mode && ((frame->headers.cat == NGHTTP2_HCAT_HEADERS) || (frame->headers.cat == NGHTTP2_HCAT_REQUEST)))
		) {
			sess->h2_headers_seen = 1;
			if (sess->server_mode) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] All headers received for stream ID %d\n", sess->h2_stream_id));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] All headers received for stream ID %d\n", sess->h2_stream_id));
			}
		}
		if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) data done\n", frame->hd.stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
			sess->h2_data_done = 1;
			sess->h2_send_data = NULL;
			sess->h2_send_data_len = 0;
		}
		break;
	case NGHTTP2_DATA:
		if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
			sess = h2_get_session(user_data, frame->hd.stream_id, GF_FALSE);
			//if no session with such ID this means we got all our bytes and considered the session done, do not throw an error
			if (sess) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) data done\n", frame->hd.stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
				sess->h2_data_done = 1;
			}
		}
		break;
	case NGHTTP2_RST_STREAM:
		sess = h2_get_session(user_data, frame->hd.stream_id, GF_FALSE);
		// cancel from remote peer, signal if not done
		if (sess && sess->server_mode && !sess->h2_is_eos) {
			GF_NETIO_Parameter param;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) canceled\n", frame->hd.stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
			memset(&param, 0, sizeof(GF_NETIO_Parameter));
			param.msg_type = GF_NETIO_CANCEL_STREAM;
			gf_mx_p(sess->mx);
			sess->in_callback = GF_TRUE;
			param.sess = sess;
			sess->user_proc(sess->usr_cbk, &param);
			sess->in_callback = GF_FALSE;
			gf_mx_v(sess->mx);
		}
		break;
	}

	return 0;
}

static int h2_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data)
{
	GF_DownloadSession *sess = h2_get_session(user_data, stream_id, GF_FALSE);
	if (!sess)
		return NGHTTP2_ERR_CALLBACK_FAILURE;

	if (sess->h2_buf.size + len > sess->h2_buf.alloc) {
		sess->h2_buf.alloc = sess->h2_buf.size + (u32) len;
		sess->h2_buf.data = gf_realloc(sess->h2_buf.data, sizeof(u8) * sess->h2_buf.alloc);
		if (!sess->h2_buf.data)	return NGHTTP2_ERR_NOMEM;
	}
	memcpy(sess->h2_buf.data + sess->h2_buf.size, data, len);
	sess->h2_buf.size += (u32) len;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d received %d bytes (%d/%d total) - flags %d\n", sess->h2_stream_id, len, sess->bytes_done, sess->total_size, flags));
	return 0;
}

static int h2_stream_close_callback(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data)
{
	Bool do_retry = GF_FALSE;
	GF_DownloadSession *sess = h2_get_session(user_data, stream_id, GF_FALSE);
	if (!sess)
		return 0;

	gf_mx_p(sess->mx);

	if (error_code==NGHTTP2_REFUSED_STREAM)
		do_retry = GF_TRUE;
	else if (sess->h2_sess->do_shutdown && !sess->server_mode && !sess->bytes_done)
		do_retry = GF_TRUE;

	if (do_retry) {
		sess->h2_sess->do_shutdown = GF_TRUE;
		sess->h2_switch_sess = GF_TRUE;
		sess->status = GF_NETIO_SETUP;
		SET_LAST_ERR(GF_OK)
		gf_mx_v(sess->mx);
		return 0;
	}

	if (error_code) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) closed with error_code=%d\n", stream_id, sess->remote_path ? sess->remote_path : sess->orig_url, error_code));
		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR(GF_IP_NETWORK_FAILURE)
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) closed\n", stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
		//keep status in DATA_EXCHANGE as this frame might have been pushed while processing another session
	}
	//stream closed
	sess->h2_stream_id = 0;
	gf_mx_v(sess->mx);
	return 0;
}

static int h2_error_callback(nghttp2_session *session, const char *msg, size_t len, void *user_data)
{
	if (session)
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] error %s\n", msg));
	return 0;
}

static ssize_t h2_write_data(GF_DownloadSession *sess, const uint8_t *data, size_t length)
{
	GF_Err e = dm_sess_write(sess, data, (u32) length);
	switch (e) {
	case GF_OK:
		return length;
	case GF_IP_NETWORK_EMPTY:
		if (sess->flags & GF_NETIO_SESSION_NO_BLOCK)
			return length;
		return NGHTTP2_ERR_WOULDBLOCK;
	case GF_IP_CONNECTION_CLOSED:
		SET_LAST_ERR(GF_IP_CONNECTION_CLOSED)
		return NGHTTP2_ERR_EOF;
	default:
		break;
	}
	return NGHTTP2_ERR_CALLBACK_FAILURE;
}

static ssize_t h2_send_callback(nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_data)
{
	GF_H2_Session *h2sess = (GF_H2_Session *)user_data;
	GF_DownloadSession *sess = h2sess->net_sess;

	return h2_write_data(sess, data, length);
}

static int h2_before_frame_send_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
	GF_DownloadSession *sess = h2_get_session(user_data, frame->hd.stream_id, GF_FALSE);
	if (!sess)
		return 0;
	sess->h2_ready_to_send = 1;
	return 0;
}


static ssize_t h2_data_source_read_callback(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source, void *user_data)
{
	GF_DownloadSession *sess = (GF_DownloadSession *) source->ptr;

	if (!sess->h2_send_data_len) {
		sess->h2_send_data = NULL;
		if (sess->h2_is_eos) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] send EOS for stream_id %d\n", sess->h2_stream_id));
			*data_flags = NGHTTP2_DATA_FLAG_EOF;
			return 0;
		}
		sess->h2_data_paused = 1;
		return NGHTTP2_ERR_DEFERRED;
	}

	if (sess->h2_sess->copy) {
		u32 copy = (sess->h2_send_data_len > length) ? (u32) length : sess->h2_send_data_len;
		memcpy(buf, sess->h2_send_data, copy);
		sess->h2_send_data += copy;
		sess->h2_send_data_len -= copy;
		return copy;
	}

	*data_flags = NGHTTP2_DATA_FLAG_NO_COPY;
	if (sess->h2_send_data_len > length) {
		return length;
	}
	return sess->h2_send_data_len;
}

static void h2_flush_send(GF_DownloadSession *sess, Bool flush_local_buf)
{
	char h2_flush[1024];
	u32 res;

	while (sess->h2_send_data) {
		GF_Err e;
		u32 nb_bytes = sess->h2_send_data_len;

		if (!sess->h2_local_buf_len || flush_local_buf) {
			//read any frame pending from remote peer (window update and co)
			e = gf_dm_read_data(sess, h2_flush, 1023, &res);
			if ((e<0) && (e != GF_IP_NETWORK_EMPTY)) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(sess->last_error)
				break;
			}

			h2_session_send(sess);

			//error or regular eos
			if (!sess->h2_stream_id)
				break;
			if (sess->status==GF_NETIO_STATE_ERROR)
				break;
		}

		if (nb_bytes > sess->h2_send_data_len) continue;
		if (!(sess->flags & GF_NETIO_SESSION_NO_BLOCK)) continue;

		if (flush_local_buf) return;

		//stream will now block, no choice but do a local copy...
		if (sess->h2_local_buf_len + nb_bytes > sess->h2_local_buf_alloc) {
			sess->h2_local_buf_alloc = sess->h2_local_buf_len + nb_bytes;
			sess->h2_local_buf = gf_realloc(sess->h2_local_buf, sess->h2_local_buf_alloc);
			if (!sess->h2_local_buf) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(sess->last_error)
				break;
			}
		}
		memcpy(sess->h2_local_buf + sess->h2_local_buf_len, sess->h2_send_data, nb_bytes);
		sess->h2_local_buf_len+=nb_bytes;
		sess->h2_send_data = NULL;
		sess->h2_send_data_len = 0;
	}
}

static char padding[256];

static int h2_send_data_callback(nghttp2_session *session, nghttp2_frame *frame, const uint8_t *framehd, size_t length, nghttp2_data_source *source, void *user_data)
{
	ssize_t rv;
	GF_DownloadSession *sess = (GF_DownloadSession *) source->ptr;

	assert(sess->h2_send_data_len);
	assert(sess->h2_send_data_len >= length);

	rv = h2_write_data(sess, (u8 *) framehd, 9);
	if (rv<0) {
		if (rv==NGHTTP2_ERR_WOULDBLOCK) return NGHTTP2_ERR_WOULDBLOCK;
		goto err;
	}

	while (frame->data.padlen > 0) {
		u32 padlen = (u32) frame->data.padlen - 1;
		rv = h2_write_data(sess, padding, padlen);
		if (rv<0) {
			if (rv==NGHTTP2_ERR_WOULDBLOCK) continue;
			goto err;
		}
		break;
	}
	while (1) {
		rv = h2_write_data(sess, (u8 *) sess->h2_send_data, length);
		if (rv<0) {
			if (rv==NGHTTP2_ERR_WOULDBLOCK) continue;
			goto err;
		}
		break;
	}
	sess->h2_send_data += (u32) length;
	sess->h2_send_data_len -= (u32) length;
	return 0;
err:

	sess->status = GF_NETIO_STATE_ERROR;
	SET_LAST_ERR(sess->last_error)
	return (int) rv;
}

static void h2_initialize_session(GF_DownloadSession *sess)
{
	int rv;
	nghttp2_settings_entry iv[2] = {
		{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
		{NGHTTP2_SETTINGS_ENABLE_PUSH, 0}
	};
	char szMXName[100];
	nghttp2_session_callbacks *callbacks;

	nghttp2_session_callbacks_new(&callbacks);
	nghttp2_session_callbacks_set_send_callback(callbacks, h2_send_callback);
	nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, h2_frame_recv_callback);
	nghttp2_session_callbacks_set_before_frame_send_callback(callbacks, h2_before_frame_send_callback);

	nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, h2_data_chunk_recv_callback);
	nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, h2_stream_close_callback);
	nghttp2_session_callbacks_set_on_header_callback(callbacks, h2_header_callback);
	nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, h2_begin_headers_callback);
	nghttp2_session_callbacks_set_error_callback(callbacks, h2_error_callback);

	GF_SAFEALLOC(sess->h2_sess, GF_H2_Session)
	sess->h2_sess->sessions = gf_list_new();
	sess->h2_sess->copy = gf_opts_get_bool("core", "h2-copy");
	if (!sess->h2_sess->copy)
		nghttp2_session_callbacks_set_send_data_callback(callbacks, h2_send_data_callback);

	if (sess->server_mode) {
		nghttp2_session_server_new(&sess->h2_sess->ng_sess, callbacks, sess->h2_sess);
	} else {
		nghttp2_session_client_new(&sess->h2_sess->ng_sess, callbacks, sess->h2_sess);
	}
	nghttp2_session_callbacks_del(callbacks);
	sess->h2_sess->net_sess = sess;
	gf_list_add(sess->h2_sess->sessions, sess);

	sprintf(szMXName, "http2_%p", sess->h2_sess);
	sess->h2_sess->mx = gf_mx_new(szMXName);
	sess->mx = sess->h2_sess->mx;
	sess->chunked = GF_FALSE;

	sess->data_io.read_callback = h2_data_source_read_callback;
	sess->data_io.source.ptr = sess;

	if (sess->server_mode) {
		sess->h2_stream_id = 1;
		if (sess->h2_upgrade_settings) {
			rv = nghttp2_session_upgrade2(sess->h2_sess->ng_sess, sess->h2_upgrade_settings, sess->h2_upgrade_settings_len, 0, sess);
			gf_free(sess->h2_upgrade_settings);
			sess->h2_upgrade_settings = NULL;

			if (rv) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR( (rv==NGHTTP2_ERR_NOMEM) ? GF_OUT_OF_MEM : GF_REMOTE_SERVICE_ERROR)
				return;
			}
		}
	}

	/* client 24 bytes magic string will be sent by nghttp2 library */
	rv = nghttp2_submit_settings(sess->h2_sess->ng_sess, NGHTTP2_FLAG_NONE, iv, GF_ARRAY_LENGTH(iv));
	if (rv != 0) {
		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR((rv==NGHTTP2_ERR_NOMEM) ? GF_OUT_OF_MEM : GF_SERVICE_ERROR)
		return;
	}
	h2_session_send(sess);
}


#define NV_HDR(_hdr, _name, _value) { \
		_hdr.name = (uint8_t *)_name;\
		_hdr.value = (uint8_t *)_value;\
		_hdr.namelen = (u32) strlen(_name);\
		_hdr.valuelen = (u32) strlen(_value);\
		_hdr.flags = NGHTTP2_NV_FLAG_NONE;\
	}

static GF_Err h2_submit_request(GF_DownloadSession *sess, char *req_name, const char *url, const char *param_string, Bool has_body)
{
	u32 nb_hdrs, i;
	char *hostport = NULL;
	char *path = NULL;
	char port[20];
	nghttp2_nv *hdrs;

	nb_hdrs = gf_list_count(sess->headers);
	hdrs = gf_malloc(sizeof(nghttp2_nv) * (nb_hdrs + 4));

	NV_HDR(hdrs[0], ":method", req_name);
	NV_HDR(hdrs[1], ":scheme", "https");

	gf_dynstrcat(&hostport, sess->server_name, NULL);
	sprintf(port, ":%d", sess->port);
	gf_dynstrcat(&hostport, port, NULL);
	NV_HDR(hdrs[2], ":authority", hostport);

	if (param_string) {
		gf_dynstrcat(&path, url, NULL);
		if (strchr(sess->remote_path, '?')) {
			gf_dynstrcat(&path, param_string, "&");
		} else {
			gf_dynstrcat(&path, param_string, "?");
		}
		NV_HDR(hdrs[3], ":path", path);
	} else {
		NV_HDR(hdrs[3], ":path", url);
	}

	for (i=0; i<nb_hdrs; i++) {
		GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
		NV_HDR(hdrs[4+i], hdr->name, hdr->value);
	}
	if (has_body) {
		assert(sess->data_io.read_callback);
		assert(sess->data_io.source.ptr != NULL);
	}

	sess->h2_data_done = 0;
	sess->h2_headers_seen = 0;
	sess->h2_stream_id = nghttp2_submit_request(sess->h2_sess->ng_sess, NULL, hdrs, nb_hdrs+4, has_body ? &sess->data_io : NULL, sess);
	sess->h2_ready_to_send = 0;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_HTTP, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] send request (has_body %d) for new stream_id %d:\n", has_body, sess->h2_stream_id));
		for (i=0; i<nb_hdrs+4; i++) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("\t%s: %s\n", hdrs[i].name, hdrs[i].value));
		}
	}
#endif

	gf_free(hdrs);
	gf_free(hostport);
	if (path) gf_free(path);

	if (sess->h2_stream_id < 0) {
		return GF_IP_NETWORK_FAILURE;
	}

	return GF_OK;
}


static void h2_flush_data(GF_DownloadSession *sess, Bool store_in_init)
{
	gf_dm_data_received(sess, (u8 *) sess->h2_buf.data, sess->h2_buf.size, store_in_init, NULL, NULL);
	sess->h2_buf.size = 0;
}

static void h2_flush_data_ex(GF_DownloadSession *sess, u8 *obuffer, u32 size, u32 *nb_bytes)
{
	u32 copy, nb_b_pck;
	u8 *data;
	*nb_bytes = 0;
	if (!sess->h2_buf.size)
		return;

	assert(sess->h2_buf.offset<=sess->h2_buf.size);

	nb_b_pck = sess->h2_buf.size - sess->h2_buf.offset;
	if (nb_b_pck > size)
		copy = size;
	else
		copy = nb_b_pck;

	data = sess->h2_buf.data + sess->h2_buf.offset;
	memcpy(obuffer, data, copy);
	*nb_bytes = copy;
	gf_dm_data_received(sess, (u8 *) data, copy, GF_FALSE, NULL, NULL);

	if (copy < nb_b_pck) {
		sess->h2_buf.offset += copy;
	} else {
		sess->h2_buf.size = sess->h2_buf.offset = 0;
	}
	assert(sess->h2_buf.offset<=sess->h2_buf.size);
}


#endif

static void sess_connection_closed(GF_DownloadSession *sess)
{
#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess) {
		sess->h2_sess->do_shutdown = GF_TRUE;
		sess->h2_switch_sess = GF_TRUE;
	}
#endif
}

/*
 * Private methods of cache
 */

//Writes data to the cache. A call to gf_cache_open_write_cache should have been issued before calling this function.
GF_Err gf_cache_write_to_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, const char * data, const u32 size, GF_Mutex *mx);

/**
 * \brief Close the write file pointer of cache
 * This function also flushes all buffers, so cache will always be consistent after
\param entry The entry to use
\param sess The download session
\param success 1 if cache write is success, false otherwise
\return GF_OK is everything went fine, GF_BAD_PARAM if entry is NULL, GF_IO_ERR if a failure occurs
 */
GF_Err gf_cache_close_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, Bool success);

/**
 * \brief Open the write file pointer of cache
 * This function prepares calls for gf_cache_write_to_cache
\param entry The entry to use
\param sess The download session
\return GF_OK is everything went fine, GF_BAD_PARAM if entry is NULL, GF_IO_ERR if a failure occurs
 */
GF_Err gf_cache_open_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess );

/*modify end range when chaining byte-range requests*/
void gf_cache_set_end_range(DownloadedCacheEntry entry, u64 range_end);

/*returns 1 if cache is currently open for write*/
Bool gf_cache_is_in_progress(const DownloadedCacheEntry entry);

#include <gpac/crypt.h>

static GF_Err gf_user_credentials_save_digest( GF_DownloadManager * dm, GF_UserCredentials * creds, const char * password, Bool store_info);

/**
 * Find a User's credentials for a given site
 */
GF_UserCredentials* gf_user_credentials_find_for_site(GF_DownloadManager *dm, const char *server_name, const char *user_name)
{
	GF_UserCredentials * cred;
	u32 count, i;
	if (!dm || !dm->credentials || !server_name || !strlen(server_name))
		return NULL;
	count = gf_list_count( dm->credentials);
	for (i = 0 ; i < count; i++) {
		cred = (GF_UserCredentials*)gf_list_get(dm->credentials, i );
		assert( cred );
		if (strcmp(cred->site, server_name))
			continue;

		if (!user_name || !strcmp(user_name, cred->username))
			return cred;
	}

#ifndef GPAC_DISABLE_CRYPTO
	char *key = (char*) gf_opts_get_key("credentials", server_name);
	if (key) {
		bin128 k;
		const char *credk = gf_opts_get_key("core", "cred");
		if (credk) {
			FILE *f = gf_fopen(credk, "r");
			if (f) {
				gf_fread(k, 16, f);
				gf_fclose(f);
			} else {
				credk = NULL;
			}
		}
		char *sep = credk ? strrchr(key, ':') : NULL;
		if (sep) {
			char szP[1024];
			GF_SAFEALLOC(cred, GF_UserCredentials);
			if (!cred) return NULL;
			cred->dm = dm;
			sep[0] = 0;
			strcpy(cred->username, key);
			strcpy(szP, sep+1);
			sep[0] = ':';

			GF_Crypt *gfc = gf_crypt_open(GF_AES_128, GF_CTR);
			gf_crypt_init(gfc, k, NULL);
			gf_crypt_decrypt(gfc, szP, (u32) strlen(szP));
			gf_crypt_close(gfc);
			gf_user_credentials_save_digest(dm, cred, szP, GF_FALSE);
			return cred;
		}
	}
#endif
	return NULL;
}
/**
 * \brief Saves the digest for authentication of password and username
\param dm The download manager
\param creds The credentials to fill
\return GF_OK if info has been filled, GF_BAD_PARAM if creds == NULL or dm == NULL, GF_AUTHENTICATION_FAILURE if user did not filled the info.
 */
static GF_Err gf_user_credentials_save_digest( GF_DownloadManager * dm, GF_UserCredentials * creds, const char * password, Bool store_info) {
	int size;
	char *pass_buf = NULL;
	char range_buf[1024];
	if (!dm || !creds || !password)
		return GF_BAD_PARAM;
	gf_dynstrcat(&pass_buf, creds->username, NULL);
	gf_dynstrcat(&pass_buf, password, ":");
	if (!pass_buf) return GF_OUT_OF_MEM;
	size = gf_base64_encode(pass_buf, (u32) strlen(pass_buf), range_buf, 1024);
	gf_free(pass_buf);
	range_buf[size] = 0;
	strcpy(creds->digest, range_buf);
	creds->valid = GF_TRUE;

#ifndef GPAC_DISABLE_CRYPTO
	if (store_info) {
		u32 plen = (u32) strlen(password);
		char *key = NULL;
		char szPATH[GF_MAX_PATH];
		const char *cred = gf_opts_get_key("core", "cred");
		if (!cred) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] No credential key defined, will not store password - use -cred=PATH_TO_KEY to specify it\n"));
			return GF_OK;
		}

		FILE *f = gf_fopen(cred, "r");
		bin128 k;
		if (!f) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] Failed to open credential key %s, will not store password\n", cred));
			return GF_OK;
		}
		u32 ksize = (u32) gf_fread(k, 16, f);
		gf_fclose(f);
		if (ksize!=16) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] Invalid credential key %s: size %d but expecting 16 bytes, will not store password\n", cred, ksize));
			return GF_OK;
		}

		GF_Crypt *gfc = gf_crypt_open(GF_AES_128, GF_CTR);
		gf_crypt_init(gfc, k, NULL);
		strcpy(szPATH, password);
		gf_crypt_encrypt(gfc, szPATH, plen);
		gf_crypt_close(gfc);
		szPATH[plen] = 0;
		gf_dynstrcat(&key, creds->username, NULL);
		gf_dynstrcat(&key, szPATH, ":");
		gf_opts_set_key("credentials", creds->site, key);
		gf_free(key);
		gf_opts_save();
	}
#endif
	return GF_OK;
}


static void on_user_pass(void *udta, const char *user, const char *pass, Bool store_info)
{
	GF_UserCredentials *creds = (GF_UserCredentials *)udta;
	if (!creds) return;
	u32 len = user ? (u32) strlen(user) : 0;
	if (len && (user != creds->username)) {
		if (len> 49) len = 49;
		strncpy(creds->username, user, 49);
		creds->username[len]=0;
	}
	if (user && pass) {
		GF_Err e = gf_user_credentials_save_digest((GF_DownloadManager *)creds->dm, creds, pass, store_info);
		if (e != GF_OK) {
			creds->valid = GF_FALSE;
		}
	} else {
		creds->valid = GF_FALSE;
	}
	creds->req_state = GF_CREDS_STATE_DONE;
}

/**
 * \brief Asks the user for credentials for given site
\param dm The download manager
\param creds The credentials to fill
\return GF_OK if info has been filled, GF_BAD_PARAM if creds == NULL or dm == NULL, GF_AUTHENTICATION_FAILURE if user did not filled the info.
 */
static GF_Err gf_user_credentials_ask_password( GF_DownloadManager * dm, GF_UserCredentials * creds, Bool secure)
{
	char szPASS[50];
	if (!dm || !creds)
		return GF_BAD_PARAM;
	memset(szPASS, 0, 50);
	if (!dm->get_user_password) return GF_AUTHENTICATION_FAILURE;
	creds->req_state = GF_CREDS_STATE_PENDING;
	if (!dm->get_user_password(dm->usr_cbk, secure, creds->site, creds->username, szPASS, on_user_pass, creds)) {
		creds->req_state = GF_CREDS_STATE_NONE;
		return GF_AUTHENTICATION_FAILURE;
	}
	return GF_OK;
}

GF_UserCredentials * gf_user_credentials_register(GF_DownloadManager * dm, Bool secure, const char * server_name, const char * username, const char * password, Bool valid)
{
	GF_UserCredentials * creds;
	if (!dm)
		return NULL;
	assert( server_name );
	creds = gf_user_credentials_find_for_site(dm, server_name, username);
	/* If none found, we create one */
	if (!creds) {
		GF_SAFEALLOC(creds, GF_UserCredentials);
		if (!creds) return NULL;
		creds->dm = dm;
		gf_list_insert(dm->credentials, creds, 0);
	}
	creds->valid = valid;
	if (username) {
		strncpy(creds->username, username, 49);
		creds->username[49] = 0;
	} else {
		creds->username[0] = 0;
	}
	strncpy(creds->site, server_name, 1023);
	creds->site[1023] = 0;
	if (username && password && valid)
		gf_user_credentials_save_digest(dm, creds, password, GF_FALSE);
	else {
		if (GF_OK != gf_user_credentials_ask_password(dm, creds, secure)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP,
			       ("[HTTP] Failed to get password information.\n"));
			gf_list_del_item( dm->credentials, creds);
			gf_free( creds );
			creds = NULL;
		}
	}
	return creds;
}

#ifdef GPAC_HAS_SSL

static Bool _ssl_is_initialized = GF_FALSE;

/*!
 * initialize the SSL library once for all download managers
\return GF_FALSE if everyhing is OK, GF_TRUE otherwise
 */
Bool gf_ssl_init_lib() {
	if (_ssl_is_initialized)
		return GF_FALSE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPS] Initializing SSL library...\n"));
	init_prng();
	if (RAND_status() != 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPS] Error while initializing Random Number generator, failed to init SSL !\n"));
		return GF_TRUE;
	}

	/* per https://www.openssl.org/docs/man1.1.0/ssl/OPENSSL_init_ssl.html
	** As of version 1.1.0 OpenSSL will automatically allocate all resources that it needs so no explicit initialisation is required.
	** Similarly it will also automatically deinitialise as required.
	*/
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_library_init();
	SSL_load_error_strings();
	SSLeay_add_all_algorithms();
	SSLeay_add_ssl_algorithms();
#endif

	_ssl_is_initialized = GF_TRUE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPS] Initalization of SSL library complete.\n"));
	return GF_FALSE;
}

#ifndef GPAC_DISABLE_LOG
static const char *tls_rt_get_name(int type)
{
	switch(type) {
#ifdef SSL3_RT_HEADER
	case SSL3_RT_HEADER: return "TLS header";
#endif
	case SSL3_RT_CHANGE_CIPHER_SPEC: return "TLS change cipher";
	case SSL3_RT_ALERT: return "TLS alert";
	case SSL3_RT_HANDSHAKE: return "TLS handshake";
	case SSL3_RT_APPLICATION_DATA: return "TLS app data";
	default: return "TLS Unknown";
	}
}

static const char *ssl_msg_get_name(int version, int msg)
{
#ifdef SSL_CTRL_SET_MSG_CALLBACK

#ifdef SSL2_VERSION_MAJOR
	if (version == SSL2_VERSION_MAJOR) {
		switch(msg) {
		case SSL2_MT_ERROR: return "Error";
		case SSL2_MT_CLIENT_HELLO: return "Client hello";
		case SSL2_MT_CLIENT_MASTER_KEY: return "Client key";
		case SSL2_MT_CLIENT_FINISHED: return "Client finished";
		case SSL2_MT_SERVER_HELLO: return "Server hello";
		case SSL2_MT_SERVER_VERIFY: return "Server verify";
		case SSL2_MT_SERVER_FINISHED: return "Server finished";
		case SSL2_MT_REQUEST_CERTIFICATE: return "Request CERT";
		case SSL2_MT_CLIENT_CERTIFICATE: return "Client CERT";
		}
	} else
#endif
	if (version == SSL3_VERSION_MAJOR) {
		switch(msg) {
		case SSL3_MT_HELLO_REQUEST: return "Hello request";
		case SSL3_MT_CLIENT_HELLO: return "Client hello";
		case SSL3_MT_SERVER_HELLO: return "Server hello";
	#ifdef SSL3_MT_NEWSESSION_TICKET
		case SSL3_MT_NEWSESSION_TICKET: return "Newsession Ticket";
	#endif
		case SSL3_MT_CERTIFICATE: return "Certificate";
		case SSL3_MT_SERVER_KEY_EXCHANGE: return "Server key exchange";
		case SSL3_MT_CLIENT_KEY_EXCHANGE: return "Client key exchange";
		case SSL3_MT_CERTIFICATE_REQUEST: return "Request CERT";
		case SSL3_MT_SERVER_DONE: return "Server finished";
		case SSL3_MT_CERTIFICATE_VERIFY: return "CERT verify";
		case SSL3_MT_FINISHED: return "Finished";
#ifdef SSL3_MT_CERTIFICATE_STATUS
		case SSL3_MT_CERTIFICATE_STATUS: return "Certificate Status";
#endif
#ifdef SSL3_MT_ENCRYPTED_EXTENSIONS
		case SSL3_MT_ENCRYPTED_EXTENSIONS: return "Encrypted Extensions";
#endif
#ifdef SSL3_MT_SUPPLEMENTAL_DATA
		case SSL3_MT_SUPPLEMENTAL_DATA: return "Supplemental data";
#endif
#ifdef SSL3_MT_END_OF_EARLY_DATA
		case SSL3_MT_END_OF_EARLY_DATA: return "End of early data";
#endif
#ifdef SSL3_MT_KEY_UPDATE
		case SSL3_MT_KEY_UPDATE: return "Key update";
#endif
#ifdef SSL3_MT_NEXT_PROTO
		case SSL3_MT_NEXT_PROTO: return "Next protocol";
#endif
#ifdef SSL3_MT_MESSAGE_HASH
		case SSL3_MT_MESSAGE_HASH: return "Message hash";
#endif
		}
	}
#endif
	return "Unknown";
}

static void ssl_on_log(int write_p, int version, int content_type, const void *buf, size_t len, SSL *ssl, void *udat)
{
	const char *msg_name, *tls_rt_name;
	int msg_type;
	char szUnknown[32];
	const char *szVersion = NULL;

	if (!version) return;
#ifdef SSL3_RT_INNER_CONTENT_TYPE
	if (content_type == SSL3_RT_INNER_CONTENT_TYPE) return;
#endif

	switch (version) {
#ifdef SSL2_VERSION
	case SSL2_VERSION: szVersion = "SSLv2"; break;
#endif
#ifdef SSL3_VERSION
	case SSL3_VERSION: szVersion = "SSLv3"; break;
#endif
	case TLS1_VERSION: szVersion = "TLSv1.0"; break;
#ifdef TLS1_1_VERSION
	case TLS1_1_VERSION: szVersion = "TLSv1.1"; break;
#endif
#ifdef TLS1_2_VERSION
	case TLS1_2_VERSION: szVersion = "TLSv1.2"; break;
#endif
#ifdef TLS1_3_VERSION
	case TLS1_3_VERSION: szVersion = "TLSv1.3"; break;
#endif
	case 0: break;
	default:
		snprintf(szUnknown, 31, "(%x)", version);
		szVersion = szUnknown;
		break;
	}
	version >>= 8;
	if ((version == SSL3_VERSION_MAJOR) && content_type)
		tls_rt_name = tls_rt_get_name(content_type);
	else
		tls_rt_name = "";

	if (content_type == SSL3_RT_CHANGE_CIPHER_SPEC) {
		msg_type = *(char *)buf;
		msg_name = "Change cipher spec";
	} else if (content_type == SSL3_RT_ALERT) {
		msg_type = (((char *)buf)[0] << 8) + ((char *)buf)[1];
		msg_name = SSL_alert_desc_string_long(msg_type);
	} else {
		msg_type = *(char *)buf;
		msg_name = ssl_msg_get_name(version, msg_type);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[%s] %s %s (%d)\n", szVersion, tls_rt_name, msg_name, msg_type));
}
#endif //GPAC_DISABLE_LOG

void *gf_dm_ssl_init(GF_DownloadManager *dm, u32 mode)
{
#if OPENSSL_VERSION_NUMBER > 0x00909000
	const
#endif
	SSL_METHOD *meth;

	if (!dm) return NULL;

	gf_mx_p(dm->cache_mx);
	/* The SSL has already been initialized. */
	if (dm->ssl_ctx) {
		gf_mx_v(dm->cache_mx);
		return dm->ssl_ctx;
	}
	/* Init the PRNG.  If that fails, bail out.  */
	if (gf_ssl_init_lib()) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPS] Failed to properly initialize SSL library\n"));
		goto error;
	}

	switch (mode) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	case 0:
		meth = SSLv23_client_method();
		break;
#if 0 /*SSL v2 is no longer supported in OpenSSL 1.0*/
	case 1:
		meth = SSLv2_client_method();
		break;
#endif
	case 2:
		meth = SSLv3_client_method();
		break;
	case 3:
		meth = TLSv1_client_method();
		break;
#else /* for openssl 1.1+ this is the preferred method */
	case 0:
		meth = TLS_client_method();
		break;
#endif
	default:
		goto error;
	}

	dm->ssl_ctx = SSL_CTX_new(meth);
	if (!dm->ssl_ctx) goto error;
	SSL_CTX_set_options(dm->ssl_ctx, SSL_OP_ALL);
//	SSL_CTX_set_max_proto_version(dm->ssl_ctx, 0);

	SSL_CTX_set_default_verify_paths(dm->ssl_ctx);
	SSL_CTX_load_verify_locations (dm->ssl_ctx, NULL, NULL);
	/* SSL_VERIFY_NONE instructs OpenSSL not to abort SSL_connect if the
	 certificate is invalid.  We verify the certificate separately in
	 ssl_check_certificate, which provides much better diagnostics
	 than examining the error stack after a failed SSL_connect.  */
	SSL_CTX_set_verify(dm->ssl_ctx, SSL_VERIFY_NONE, NULL);

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_NETWORK, GF_LOG_DEBUG) ) {
		SSL_CTX_set_msg_callback(dm->ssl_ctx, ssl_on_log);
	}
#endif


#ifdef GPAC_HAS_HTTP2
	if (!dm->disable_http2) {
		SSL_CTX_set_next_proto_select_cb(dm->ssl_ctx, h2_select_next_proto_cb, NULL);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
		SSL_CTX_set_alpn_protos(dm->ssl_ctx, (const unsigned char *)"\x02h2", 3);
#endif
	}
#endif

	/* Since fd_write unconditionally assumes partial writes (and handles them correctly),
	allow them in OpenSSL.  */
	SSL_CTX_set_mode(dm->ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
	gf_mx_v(dm->cache_mx);
	return dm->ssl_ctx;
error:
	if (dm->ssl_ctx) SSL_CTX_free(dm->ssl_ctx);
	dm->ssl_ctx = NULL;
	gf_mx_v(dm->cache_mx);
	return NULL;
}

#ifdef GPAC_HAS_HTTP2

static unsigned char next_proto_list[256];
static size_t next_proto_list_len;

#ifndef OPENSSL_NO_NEXTPROTONEG
static int next_proto_cb(SSL *ssl, const unsigned char **data, unsigned int *len, void *arg)
{
	if (data && len) {
		*data = next_proto_list;
		*len = (unsigned int)next_proto_list_len;
	}
	return SSL_TLSEXT_ERR_OK;
}
#endif //OPENSSL_NO_NEXTPROTONEG

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg)
{
	int rv = nghttp2_select_next_protocol((unsigned char **)out, outlen, in, inlen);
	if (rv != 1) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	return SSL_TLSEXT_ERR_OK;
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

#endif


void *gf_ssl_server_context_new(const char *cert, const char *key)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unable to create SSL context\n"));
		ERR_print_errors_fp(stderr);
		return NULL;
    }
    SSL_CTX_set_ecdh_auto(ctx, 1);
	if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		return NULL;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0 ) {
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		return NULL;
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_NETWORK, GF_LOG_DEBUG) ) {
		SSL_CTX_set_msg_callback(ctx, ssl_on_log);
	}
#endif


#ifdef GPAC_HAS_HTTP2
	if (!gf_opts_get_bool("core", "no-h2")) {
		next_proto_list[0] = NGHTTP2_PROTO_VERSION_ID_LEN;
		memcpy(&next_proto_list[1], NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN);
		next_proto_list_len = 1 + NGHTTP2_PROTO_VERSION_ID_LEN;

		SSL_CTX_set_next_protos_advertised_cb(ctx, next_proto_cb, NULL);
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			next_proto_cb(NULL, NULL, NULL, NULL);
			h2_error_callback(NULL, NULL, 0, NULL);
			on_user_pass(NULL, NULL, NULL, GF_FALSE);
		}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
		SSL_CTX_set_alpn_select_cb(ctx, alpn_select_proto_cb, NULL);
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L
	}

#endif

    return ctx;
}

void gf_ssl_server_context_del(void *ssl_ctx)
{
	SSL_CTX_free(ssl_ctx);
}
void *gf_ssl_new(void *ssl_ctx, GF_Socket *client_sock, GF_Err *e)
{
	SSL *ssl;
	ssl = SSL_new(ssl_ctx);
	if (!ssl) {
		*e = GF_IO_ERR;
		return NULL;
	}
	SSL_set_fd(ssl, gf_sk_get_handle(client_sock) );
	if (SSL_accept(ssl) <= 0) {
		if (gf_log_tool_level_on(GF_LOG_NETWORK, GF_LOG_DEBUG)) {
			//this one crashes /exits on windows, only use if debug level
			ERR_print_errors_fp(stderr);
		} else {
			ERR_print_errors_fp(stderr);
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[SSL] Accept failure\n"));
		}
		SSL_shutdown(ssl);
        SSL_free(ssl);
		*e = GF_AUTHENTICATION_FAILURE;
		return NULL;
	}
	*e = GF_OK;
	return ssl;
}
void gf_ssl_del(void *ssl)
{
	if (ssl) SSL_free(ssl);
}

static GF_Err gf_ssl_write(GF_DownloadSession *sess, const u8 *buffer, u32 size, u32 *written)
{
	u32 idx=0;
	s32 nb_tls_blocks = size/16000;
	*written = 0;
	while (nb_tls_blocks>=0) {
		u32 len, to_write = 16000;
		if (nb_tls_blocks==0)
			to_write = size - idx*16000;

		len = SSL_write(sess->ssl, buffer + idx*16000, to_write);
		nb_tls_blocks--;
		idx++;

		if (len != to_write) {
			if (sess->flags & GF_NETIO_SESSION_NO_BLOCK) {
				int err = SSL_get_error(sess->ssl, len);
				if ((err==SSL_ERROR_WANT_READ) || (err==SSL_ERROR_WANT_WRITE)) {
					return GF_IP_NETWORK_EMPTY;
				}
				if (err==SSL_ERROR_SSL) {
					char msg[1024];
					SSL_load_error_strings();
					ERR_error_string_n(ERR_get_error(), msg, sizeof(msg));
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Cannot send, error %s\n", msg));
				}
			}
			return GF_IP_NETWORK_FAILURE;
		}
		*written += to_write;
	}
	return GF_OK;
}


#endif /* GPAC_HAS_SSL */


static GF_Err dm_sess_write(GF_DownloadSession *session, const u8 *buffer, u32 size)
{
	GF_Err e;
	u32 written=0;
	GF_DownloadSession *par_sess = session;
#ifdef GPAC_HAS_HTTP2
	//if h2 we aggregate pending frames on the session currently holding the HTTP2 session
	if (session->h2_sess) par_sess = session->h2_sess->net_sess;
#endif

	//we are writing and we already have pending data, append and try to flush after
	if (par_sess->async_buf_size && (buffer != par_sess->async_buf)) {
		e = GF_IP_NETWORK_EMPTY;
	} else
#ifdef GPAC_HAS_SSL
	if (session->ssl) {
		e = gf_ssl_write(session, buffer, size, &written);
	} else
#endif
	{
		e = gf_sk_send_ex(session->sock, buffer, size, &written);
	}


	if (!(session->flags & GF_NETIO_SESSION_NO_BLOCK) || (e!=GF_IP_NETWORK_EMPTY)) {
		par_sess->async_buf_size = 0;
		return e;
	}

	//we are blocking, store content
	u32 remain = size - written;
	if (buffer == par_sess->async_buf) {
		if (written) {
			memmove(par_sess->async_buf, par_sess->async_buf + written, remain);
			assert(par_sess->async_buf_size >= written);
			par_sess->async_buf_size -= written;
		}
	} else {
		if (par_sess->async_buf_alloc < par_sess->async_buf_size + remain) {
			par_sess->async_buf_alloc = par_sess->async_buf_size + remain;
			par_sess->async_buf = gf_realloc(par_sess->async_buf, par_sess->async_buf_alloc);
			if (!par_sess->async_buf) return GF_OUT_OF_MEM;
		}
		memcpy(par_sess->async_buf+par_sess->async_buf_size, buffer + written, remain);
		par_sess->async_buf_size += remain;
	}
	return GF_OK;
}


static Bool gf_dm_is_local(GF_DownloadManager *dm, const char *url)
{
	if (!strnicmp(url, "file://", 7)) return GF_TRUE;
	if (!strstr(url, "://")) return GF_TRUE;
	return GF_FALSE;
}

static Bool gf_dm_can_handle_url(GF_DownloadManager *dm, const char *url)
{
	if (!strnicmp(url, "http://", 7)) return GF_TRUE;
#ifdef GPAC_HAS_SSL
	if (!strnicmp(url, "https://", 8)) return GF_TRUE;
#endif
	return GF_FALSE;
}

/*!
 * Finds an existing entry in the cache for a given URL
\param sess The session configured with the URL
\return NULL if none found, the DownloadedCacheEntry otherwise
 */
DownloadedCacheEntry gf_dm_find_cached_entry_by_url(GF_DownloadSession * sess)
{
	u32 i, count;
	assert( sess && sess->dm && sess->dm->cache_entries );
	gf_mx_p( sess->dm->cache_mx );
	count = gf_list_count(sess->dm->cache_entries);
	for (i = 0 ; i < count; i++) {
		const char * url;
		DownloadedCacheEntry e = (DownloadedCacheEntry)gf_list_get(sess->dm->cache_entries, i);
		assert(e);
		url = gf_cache_get_url(e);
		assert( url );
		if (strcmp(url, sess->orig_url)) continue;

		if (! sess->is_range_continuation) {
			if (sess->range_start != gf_cache_get_start_range(e)) continue;
			if (sess->range_end != gf_cache_get_end_range(e)) continue;
		}
		/*OK that's ours*/
		gf_mx_v( sess->dm->cache_mx );
		return e;
	}
	gf_mx_v( sess->dm->cache_mx );
	return NULL;
}

/**
 * Creates a new cache entry
 */
DownloadedCacheEntry gf_cache_create_entry( GF_DownloadManager * dm, const char * cache_directory, const char * url, u64 start_range, u64 end_range, Bool mem_storage, GF_Mutex *mx);

/*!
 * Removes a session for a DownloadedCacheEntry
\param entry The entry
\param sess The session to remove
\return the number of sessions left in the cached entry, -1 if one of the parameters is wrong
 */
s32 gf_cache_remove_session_from_cache_entry(DownloadedCacheEntry entry, GF_DownloadSession * sess);

Bool gf_cache_set_mime(const DownloadedCacheEntry entry, const char *mime);
Bool gf_cache_set_range(const DownloadedCacheEntry entry, u64 size, u64 start_range, u64 end_range);
Bool gf_cache_set_content(const DownloadedCacheEntry entry, GF_Blob *blob, Bool copy, GF_Mutex *mx);
Bool gf_cache_set_headers(const DownloadedCacheEntry entry, const char *headers);
void gf_cache_set_downtime(const DownloadedCacheEntry entry, u32 download_time_ms);


/**
 * Removes a cache entry from cache and performs a cleanup if possible.
 * If the cache entry is marked for deletion and has no sessions associated with it, it will be
 * removed (so some modules using a streaming like cache will still work).
 */
static void gf_dm_remove_cache_entry_from_session(GF_DownloadSession * sess) {
	if (sess && sess->cache_entry) {
		gf_cache_remove_session_from_cache_entry(sess->cache_entry, sess);
		if (sess->dm
		        /*JLF - not sure what the rationale of this test is, and it prevents cleanup of cache entry
		        which then results to crash when restarting the session (entry->writeFilePtr is not set back to NULL)*/
		        && gf_cache_entry_is_delete_files_when_deleted(sess->cache_entry)

		        && (0 == gf_cache_get_sessions_count_for_cache_entry(sess->cache_entry)))
		{
			u32 i, count;
			gf_mx_p( sess->dm->cache_mx );
			count = gf_list_count( sess->dm->cache_entries );
			for (i = 0; i < count; i++) {
				DownloadedCacheEntry ex = (DownloadedCacheEntry)gf_list_get(sess->dm->cache_entries, i);
				if (ex == sess->cache_entry) {
					gf_list_rem(sess->dm->cache_entries, i);
					gf_cache_delete_entry( sess->cache_entry );
					sess->cache_entry = NULL;
					break;
				}
			}
			gf_mx_v( sess->dm->cache_mx );
		}
	}
}

/*!
 * Adds a session to a DownloadedCacheEntry.
 * implemented in cache.c
\param entry The entry
\param sess The session to add
\return the number of sessions in the cached entry, -1 if one of the parameters is wrong
 */
s32 gf_cache_add_session_to_cache_entry(DownloadedCacheEntry entry, GF_DownloadSession * sess);
Bool gf_cache_entry_persistent(const DownloadedCacheEntry entry);
void gf_cache_entry_set_persistent(const DownloadedCacheEntry entry);

static void gf_dm_sess_notify_state(GF_DownloadSession *sess, GF_NetIOStatus dnload_status, GF_Err error);

static void gf_dm_configure_cache(GF_DownloadSession *sess)
{
	DownloadedCacheEntry entry;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[Downloader] gf_dm_configure_cache(%p), cached=%s URL=%s\n", sess, (sess->flags & GF_NETIO_SESSION_NOT_CACHED) ? "no" : "yes", sess->orig_url ));
	gf_dm_remove_cache_entry_from_session(sess);
	//session is not cached and we don't cache the first URL
	if ((sess->flags & GF_NETIO_SESSION_NOT_CACHED) && !(sess->flags & GF_NETIO_SESSION_KEEP_FIRST_CACHE))  {
		sess->reused_cache_entry = GF_FALSE;
		if (sess->cache_entry)
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);

		sess->cache_entry = NULL;
	} else {
		Bool found = GF_FALSE;
		u32 i, count;
		entry = gf_dm_find_cached_entry_by_url(sess);
		if (!entry) {
			if (sess->local_cache_only) {
				sess->cache_entry = NULL;
				SET_LAST_ERR(GF_URL_ERROR)
				return;
			}
			/* We found the existing session */
			if (sess->cache_entry) {
				Bool delete_cache = GF_TRUE;

				if (sess->flags & GF_NETIO_SESSION_KEEP_CACHE) {
					delete_cache = GF_FALSE;
				}
				if (gf_cache_entry_persistent(sess->cache_entry))
					delete_cache = GF_FALSE;

				/*! indicate we can destroy file upon destruction, except if disabled at session level*/
				if (delete_cache)
					gf_cache_entry_set_delete_files_when_deleted(sess->cache_entry);

				if (!gf_cache_entry_persistent(sess->cache_entry) && !gf_cache_get_sessions_count_for_cache_entry(sess->cache_entry)) {
					gf_mx_p( sess->dm->cache_mx );
					/* No session attached anymore... we can delete it */
					gf_list_del_item(sess->dm->cache_entries, sess->cache_entry);
					gf_mx_v( sess->dm->cache_mx );
					gf_cache_delete_entry(sess->cache_entry);
				}
				sess->cache_entry = NULL;
			}
			entry = gf_cache_create_entry(sess->dm, sess->dm->cache_directory, sess->orig_url, sess->range_start, sess->range_end, (sess->flags&GF_NETIO_SESSION_MEMORY_CACHE) ? GF_TRUE : GF_FALSE, sess->dm->cache_mx);
			gf_mx_p( sess->dm->cache_mx );
			gf_list_add(sess->dm->cache_entries, entry);
			gf_mx_v( sess->dm->cache_mx );
			sess->is_range_continuation = GF_FALSE;
		}
		assert( entry );
		sess->cache_entry = entry;
		sess->reused_cache_entry = 	gf_cache_is_in_progress(entry);
		count = gf_list_count(sess->dm->sessions);
		for (i=0; i<count; i++) {
			GF_DownloadSession *a_sess = (GF_DownloadSession*)gf_list_get(sess->dm->sessions, i);
			assert(a_sess);
			if (a_sess==sess) continue;
			if (a_sess->cache_entry==entry) {
				found = GF_TRUE;
				break;
			}
		}
		if (!found) {
			sess->reused_cache_entry = GF_FALSE;
			if (sess->cache_entry)
				gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
		}
		gf_cache_add_session_to_cache_entry(sess->cache_entry, sess);
		if (sess->needs_range)
			gf_cache_set_range(sess->cache_entry, 0, sess->range_start, sess->range_end);
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[CACHE] Cache setup to %p %s\n", sess, gf_cache_get_cache_filename(sess->cache_entry)));

		if (sess->cache_entry) {
			if (sess->flags & GF_NETIO_SESSION_KEEP_FIRST_CACHE) {
				sess->flags &= ~GF_NETIO_SESSION_KEEP_FIRST_CACHE;
				gf_cache_entry_set_persistent(sess->cache_entry);
			}
			if ((sess->flags & GF_NETIO_SESSION_MEMORY_CACHE) && (sess->flags & GF_NETIO_SESSION_KEEP_CACHE) ) {
				gf_cache_entry_set_persistent(sess->cache_entry);
			}
		}

		if ( (sess->allow_direct_reuse || sess->dm->allow_offline_cache) && !gf_cache_check_if_cache_file_is_corrupted(sess->cache_entry)
		) {
			sess->from_cache_only = GF_TRUE;
			sess->connect_time = 0;
			sess->status = GF_NETIO_CONNECTED;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] using existing cache entry\n"));
			gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
		}
	}
}

void gf_dm_delete_cached_file_entry(const GF_DownloadManager * dm,  const char * url)
{
	GF_Err e;
	u32 count, i;
	char * realURL;
	GF_URL_Info info;
	if (!url || !dm)
		return;
	gf_mx_p( dm->cache_mx );
	gf_dm_url_info_init(&info);
	e = gf_dm_get_url_info(url, &info, NULL);
	if (e != GF_OK) {
		gf_mx_v( dm->cache_mx );
		gf_dm_url_info_del(&info);
		return;
	}
	realURL = gf_strdup(info.canonicalRepresentation);
	gf_dm_url_info_del(&info);
	assert( realURL );
	count = gf_list_count(dm->cache_entries);
	for (i = 0 ; i < count; i++) {
		const char * e_url;
		DownloadedCacheEntry cache_ent = (DownloadedCacheEntry)gf_list_get(dm->cache_entries, i);
		assert(cache_ent);
		e_url = gf_cache_get_url(cache_ent);
		assert( e_url );
		if (!strcmp(e_url, realURL)) {
			/* We found the existing session */
			gf_cache_entry_set_delete_files_when_deleted(cache_ent);
			if (0 == gf_cache_get_sessions_count_for_cache_entry( cache_ent )) {
				/* No session attached anymore... we can delete it */
				gf_list_rem(dm->cache_entries, i);
				gf_cache_delete_entry(cache_ent);
			}
			/* If deleted or not, we don't search further */
			gf_mx_v( dm->cache_mx );
			gf_free(realURL);
			return;
		}
	}
	/* If we are heren it means we did not found this URL in cache */
	gf_mx_v( dm->cache_mx );
	gf_free(realURL);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CACHE] Cannot find URL %s, cache file won't be deleted.\n", url));
}

GF_EXPORT
void gf_dm_delete_cached_file_entry_session(const GF_DownloadSession * sess,  const char * url, Bool force)
{
	if (sess && sess->dm && url) {
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[CACHE] Requesting deletion for %s\n", url));
		gf_dm_delete_cached_file_entry(sess->dm, url);
		if (sess->local_cache_only && sess->dm->local_cache_url_provider_cbk)
			sess->dm->local_cache_url_provider_cbk(sess->dm->lc_udta, (char *) url, GF_TRUE);
	}
}

void gf_dm_sess_set_header_ex(GF_DownloadSession *sess, const char *name, const char *value, Bool allow_overwrite)
{
	GF_HTTPHeader *hdr;
	if (!sess) return;

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess || sess->h2_upgrade_settings) {
		if (!stricmp(name, "Transfer-Encoding"))
			return;
		if (!stricmp(name, "Connection")) return;
		if (!stricmp(name, "Keep-Alive")) return;
	}
#endif
	//check existing headers
	u32 i, count = gf_list_count(sess->headers);
	for (i=0; i<count; i++) {
		hdr = gf_list_get(sess->headers, i);
		if (stricmp(hdr->name, name)) continue;
		if (!allow_overwrite) return;

		gf_free(hdr->value);
		if (value) {
			hdr->value = gf_strdup(value);
			return;
		}
		gf_list_rem(sess->headers, i);
		gf_free(hdr->name);
		gf_free(hdr);
		return;
	}

	GF_SAFEALLOC(hdr, GF_HTTPHeader)
	if (hdr) {
		hdr->name = gf_strdup(name);
		hdr->value = gf_strdup(value);
		gf_list_add(sess->headers, hdr);
	}
}

void gf_dm_sess_set_header(GF_DownloadSession *sess, const char *name, const char *value)
{
	gf_dm_sess_set_header_ex(sess, name, value, GF_TRUE);
}

GF_Err gf_dm_sess_send_reply(GF_DownloadSession *sess, u32 reply_code, const char *response_body, u32 body_len, Bool no_body)
{
	u32 i, count;
	GF_Err e;
	char szFmt[50];
	char *rsp_buf = NULL;
	if (!sess || !sess->server_mode) return GF_BAD_PARAM;

	count = gf_list_count(sess->headers);

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_upgrade_settings) {
		u32 len;
		assert(!sess->h2_sess);
		gf_dynstrcat(&rsp_buf, "HTTP/1.1 101 Switching Protocols\r\n"
								"Connection: Upgrade\r\n"
								"Upgrade: h2c\r\n\r\n", NULL);


		len = (u32) strlen(rsp_buf);
		e = dm_sess_write(sess, rsp_buf, len);

		gf_free(rsp_buf);
		rsp_buf = NULL;

		h2_initialize_session(sess);
	}

	if (sess->h2_sess) {
		nghttp2_nv *hdrs;

		if (response_body) {
			no_body = GF_FALSE;
			sess->h2_send_data = (u8 *) response_body;
			sess->h2_send_data_len = body_len;
			sess->h2_is_eos = 1;
		} else if (!no_body) {
			switch (reply_code) {
			case 200:
			case 206:
				no_body = GF_FALSE;
				sess->h2_is_eos = 0;
				break;
			default:
				no_body = GF_TRUE;
				break;
			}
		}

		hdrs = gf_malloc(sizeof(nghttp2_nv) * (count + 1) );

		sprintf(szFmt, "%d", reply_code);
		NV_HDR(hdrs[0], ":status", szFmt);
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP/2] send reply for stream_id %d (body %d) headers:\n:status: %s\n", sess->h2_stream_id, !no_body, szFmt));
		for (i=0; i<count; i++) {
			GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
			NV_HDR(hdrs[i+1], hdr->name, hdr->value)
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("%s: %s\n", hdr->name, hdr->value));
		}


		gf_mx_p(sess->mx);

		int rv = nghttp2_submit_response(sess->h2_sess->ng_sess, sess->h2_stream_id, hdrs, count+1, no_body ? NULL : &sess->data_io);

		gf_free(hdrs);

		if (rv != 0) {
			gf_mx_v(sess->mx);
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] Failed to submit reply: %s\n", nghttp2_strerror(rv)));
			return GF_SERVICE_ERROR;
		}
		h2_session_send(sess);
		//in case we have a body already setup with this reply
		h2_flush_send(sess, GF_FALSE);

		gf_mx_v(sess->mx);

		//h2_stream_id may still be 0 at this point (typically reply to PUT/POST)
		return GF_OK;
	}
#endif



	sprintf(szFmt, "HTTP/1.1 %d ", reply_code);
	gf_dynstrcat(&rsp_buf, szFmt, NULL);
	switch (reply_code) {
	case 400: gf_dynstrcat(&rsp_buf, "Bad Request", NULL); break;
	case 401: gf_dynstrcat(&rsp_buf, "Unauthorized", NULL); break;
	case 403: gf_dynstrcat(&rsp_buf, "Forbidden", NULL); break;
	case 405: gf_dynstrcat(&rsp_buf, "Not Allowed", NULL); break;
	case 416: gf_dynstrcat(&rsp_buf, "Requested Range Not Satisfiable", NULL); break;
	case 411: gf_dynstrcat(&rsp_buf, "Length Required", NULL); break;
	case 404: gf_dynstrcat(&rsp_buf, "Not Found", NULL); break;
	case 501: gf_dynstrcat(&rsp_buf, "Not Implemented", NULL); break;
	case 500: gf_dynstrcat(&rsp_buf, "Internal Server Error", NULL); break;
	case 304: gf_dynstrcat(&rsp_buf, "Not Modified", NULL); break;
	case 204: gf_dynstrcat(&rsp_buf, "No Content", NULL); break;
	case 206: gf_dynstrcat(&rsp_buf, "Partial Content", NULL); break;
	case 200: gf_dynstrcat(&rsp_buf, "OK", NULL); break;
	case 201: gf_dynstrcat(&rsp_buf, "Created", NULL); break;
	default:
		gf_dynstrcat(&rsp_buf, "ERROR", NULL); break;
	}
	gf_dynstrcat(&rsp_buf, "\r\n", NULL);
	if (!rsp_buf) return GF_OUT_OF_MEM;

	for (i=0; i<count; i++) {
		GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
		gf_dynstrcat(&rsp_buf, hdr->name, NULL);
		gf_dynstrcat(&rsp_buf, ": ", NULL);
		gf_dynstrcat(&rsp_buf, hdr->value, NULL);
		gf_dynstrcat(&rsp_buf, "\r\n", NULL);
	}
	gf_dynstrcat(&rsp_buf, "\r\n", NULL);
	if (!rsp_buf) return GF_OUT_OF_MEM;

	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] send reply for %s:\n%s\n", sess->orig_url, rsp_buf));

	count = (u32) strlen(rsp_buf);
	if (response_body) {
		rsp_buf = gf_realloc(rsp_buf, count+body_len);
		if (!rsp_buf) return GF_OUT_OF_MEM;
		memcpy(rsp_buf+count, response_body, body_len);
		count+=body_len;
	}
	e = dm_sess_write(sess, rsp_buf, count);
	gf_free(rsp_buf);
	return e;
}

void gf_dm_sess_clear_headers(GF_DownloadSession *sess)
{
	while (gf_list_count(sess->headers)) {
		GF_HTTPHeader *hdr = (GF_HTTPHeader*)gf_list_last(sess->headers);
		gf_list_rem_last(sess->headers);
		gf_free(hdr->name);
		gf_free(hdr->value);
		gf_free(hdr);
	}
	if (sess->mime_type) {
		gf_free(sess->mime_type);
		sess->mime_type = NULL;
	}
}

typedef enum
{
	HTTP_NO_CLOSE=0,
	HTTP_CLOSE,
	HTTP_RESET_CONN,
} HTTPCloseType;

static void gf_dm_disconnect(GF_DownloadSession *sess, HTTPCloseType close_type)
{
	assert( sess );
	if (sess->connection_close) close_type = HTTP_RESET_CONN;
	sess->connection_close = GF_FALSE;
	sess->remaining_data_size = 0;
	sess->start_time = 0;
	if (sess->async_req_reply) gf_free(sess->async_req_reply);
	sess->async_req_reply = NULL;
	sess->async_req_reply_size = 0;
	sess->async_buf_size = 0;

	if (sess->status >= GF_NETIO_DISCONNECTED) {
		if (close_type && sess->use_cache_file && sess->cache_entry) {
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
		}
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Downloader] gf_dm_disconnect(%p)\n", sess ));

	gf_mx_p(sess->mx);

	if (!sess->server_mode) {
		Bool do_close = (close_type || !(sess->flags & GF_NETIO_SESSION_PERSISTENT)) ? GF_TRUE : GF_FALSE;
#ifdef GPAC_HAS_HTTP2
		if (sess->h2_sess) {
			do_close = (close_type==HTTP_RESET_CONN) ? GF_TRUE : GF_FALSE;
		}
		//if H2 stream is still valid, issue a reset stream
		if (sess->h2_sess && sess->h2_stream_id)
			nghttp2_submit_rst_stream(sess->h2_sess->ng_sess, NGHTTP2_FLAG_NONE, sess->h2_stream_id, NGHTTP2_NO_ERROR);
#endif

		if (do_close) {
#ifdef GPAC_HAS_HTTP2
			if (sess->h2_sess) {
				sess->h2_sess->do_shutdown = GF_TRUE;
				h2_detach_session(sess->h2_sess, sess);
			}
#endif

#ifdef GPAC_HAS_SSL
			if (sess->ssl) {
				SSL_shutdown(sess->ssl);
				SSL_free(sess->ssl);
				sess->ssl = NULL;
			}
#endif
			dm_sess_sk_del(sess);
		}

		if (close_type && sess->use_cache_file && sess->cache_entry) {
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
		}
	}


	sess->status = GF_NETIO_DISCONNECTED;
	if (sess->num_retry) sess->num_retry--;

	gf_mx_v(sess->mx);
}

GF_EXPORT
void gf_dm_sess_del(GF_DownloadSession *sess)
{
	if (!sess)
		return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[Downloader] Destroy session URL %s\n", sess->orig_url));
	/*self-destruction, let the download manager destroy us*/
	if ((sess->th || sess->ftask) && sess->in_callback) {
		sess->destroy = GF_TRUE;
		return;
	}
	gf_dm_disconnect(sess, HTTP_CLOSE);
	gf_dm_sess_clear_headers(sess);

	/*if threaded wait for thread exit*/
	if (sess->th) {
		while (!(sess->flags & GF_DOWNLOAD_SESSION_THREAD_DEAD))
			gf_sleep(1);
		gf_th_stop(sess->th);
		gf_th_del(sess->th);
		sess->th = NULL;
	}

	if (sess->dm) {
		gf_mx_p(sess->dm->cache_mx);
		gf_list_del_item(sess->dm->sessions, sess);
		gf_mx_v(sess->dm->cache_mx);
	}

	gf_dm_remove_cache_entry_from_session(sess);
	sess->cache_entry = NULL;
	if (sess->orig_url) gf_free(sess->orig_url);
	if (sess->orig_url_before_redirect) gf_free(sess->orig_url_before_redirect);
	if (sess->server_name) gf_free(sess->server_name);
	sess->server_name = NULL;
	if (sess->remote_path) gf_free(sess->remote_path);
	/* Credentials are stored into the sess->dm */
	if (sess->creds) sess->creds = NULL;
	if (sess->init_data) gf_free(sess->init_data);
	if (sess->remaining_data) gf_free(sess->remaining_data);
	if (sess->async_req_reply) gf_free(sess->async_req_reply);

	sess->orig_url = sess->server_name = sess->remote_path;
	sess->creds = NULL;

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess) {
		gf_mx_p(sess->mx);
		h2_detach_session(sess->h2_sess, sess);
		gf_mx_v(sess->mx);
	}

	if (sess->h2_upgrade_settings)
		gf_free(sess->h2_upgrade_settings);

	if (sess->h2_local_buf)
		gf_free(sess->h2_local_buf);
#endif

	//free this once we have reassigned the session
	if (sess->async_buf) gf_free(sess->async_buf);

#ifdef GPAC_HAS_SSL
	//in server mode SSL context is managed by caller
	if (sess->ssl) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[Downloader] shut down SSL context\n"));
		SSL_shutdown(sess->ssl);
		SSL_free(sess->ssl);
		sess->ssl = NULL;
	}
#endif
	dm_sess_sk_del(sess);

	gf_list_del(sess->headers);

	gf_mx_del(sess->mx);


	if (sess->ftask) {
		sess->ftask->sess = NULL;
		sess->ftask = NULL;
	}
	gf_free(sess);
}

void http_do_requests(GF_DownloadSession *sess);

static void gf_dm_sess_notify_state(GF_DownloadSession *sess, GF_NetIOStatus dnload_status, GF_Err error)
{
	if (sess->user_proc) {
		GF_NETIO_Parameter par;
		sess->in_callback = GF_TRUE;
		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = dnload_status;
		par.error = error;
		par.sess = sess;
		par.reply = 200;
		sess->user_proc(sess->usr_cbk, &par);
		sess->in_callback = GF_FALSE;
	}
}

static void gf_dm_sess_user_io(GF_DownloadSession *sess, GF_NETIO_Parameter *par)
{
	if (sess->user_proc) {
		sess->in_callback = GF_TRUE;
		par->sess = sess;
		sess->user_proc(sess->usr_cbk, par);
		sess->in_callback = GF_FALSE;
	}
}

#if 0 //unused
/*!
\brief is download manager thread dead?
 *
 *Indicates whether the thread has ended
\param sess the download session
 */
Bool gf_dm_is_thread_dead(GF_DownloadSession *sess)
{
	if (!sess) return GF_TRUE;
	return (sess->flags & GF_DOWNLOAD_SESSION_THREAD_DEAD) ? GF_TRUE : GF_FALSE;
}
#endif

GF_EXPORT
GF_Err gf_dm_sess_last_error(GF_DownloadSession *sess)
{
	if (!sess)
		return GF_BAD_PARAM;
	return sess->last_error;
}

GF_EXPORT
void gf_dm_url_info_init(GF_URL_Info * info)
{
	memset(info, 0, sizeof(GF_URL_Info));
}

GF_EXPORT
void gf_dm_url_info_del(GF_URL_Info * info) {
	if (!info)
		return;
	if (info->canonicalRepresentation)
		gf_free(info->canonicalRepresentation);
	if (info->password)
		gf_free(info->password);
	if (info->userName)
		gf_free(info->userName);
	if (info->remotePath)
		gf_free(info->remotePath);
	if (info->server_name)
		gf_free(info->server_name);
	gf_dm_url_info_init(info);
}

/**
\param url The url to parse for protocol
\param info The info to fill
\return Returns the offset in url of the protocol found -1 if not found
 */
static s32 gf_dm_parse_protocol(const char * url, GF_URL_Info * info) {
	assert(info);
	assert(url);
	if (!strnicmp(url, "http://", 7)) {
		info->port = 80;
		info->protocol = "http://";
		return 7;
	}
	else if (!strnicmp(url, "https://", 8)) {
		info->port = 443;
#ifndef GPAC_HAS_SSL
		return -1;
#endif
		info->protocol = "https://";
		return 8;
	}
	else if (!strnicmp(url, "ftp://", 6)) {
		info->port = 21;
		info->protocol = "ftp://";
		return -1;
	}
	return -1;
}

GF_EXPORT
GF_Err gf_dm_get_url_info(const char * url, GF_URL_Info * info, const char * baseURL) {
	char *tmp, *tmp_url, *current_pos, *urlConcatenateWithBaseURL, *ipv6;
	char * copyOfUrl;
	s32 proto_offset;
	gf_dm_url_info_del(info);
	urlConcatenateWithBaseURL = NULL;
	proto_offset = gf_dm_parse_protocol(url, info);
	if (proto_offset > 0) {
		url += proto_offset;
	} else {
		/*relative URL*/
		if (!strstr(url, "://")) {
			u32 i;
			info->protocol = "file://";
			if (baseURL) {
				urlConcatenateWithBaseURL = gf_url_concatenate(baseURL, url);
				/*relative file path*/
				if (!strstr(baseURL, "://")) {
					info->canonicalRepresentation = urlConcatenateWithBaseURL;
					return GF_OK;
				}
				proto_offset = gf_dm_parse_protocol(urlConcatenateWithBaseURL, info);
			} else {
				proto_offset = -1;
			}

			if (proto_offset < 0) {
				tmp = urlConcatenateWithBaseURL;
				assert( ! info->remotePath );
				info->remotePath = gf_url_percent_encode(tmp);
				gf_free( urlConcatenateWithBaseURL );
				urlConcatenateWithBaseURL = NULL;

				if (!info->remotePath) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[Network] No supported protocol for url %s\n", url));
					gf_dm_url_info_del(info);
					return GF_BAD_PARAM;
				}
				for (i=0; i<strlen(info->remotePath); i++)
					if (info->remotePath[i]=='\\') info->remotePath[i]='/';
				info->canonicalRepresentation = (char*)gf_malloc(strlen(info->protocol) + strlen(info->remotePath) + 1);
				strcpy(info->canonicalRepresentation, info->protocol);
				strcat(info->canonicalRepresentation, info->remotePath);

				return GF_OK;
			} else {
				/* We continue the parsing as usual */
				url = urlConcatenateWithBaseURL + proto_offset;
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[Network] No supported protocol for url %s\n", url));
			gf_dm_url_info_del(info);
			return GF_BAD_PARAM;
		}
	}
	assert( proto_offset >= 0 );
	tmp = strchr(url, '/');
	//patch for some broken url http://servername?cgi
	if (!tmp)
		tmp = strchr(url, '?');
	assert( !info->remotePath );
	info->remotePath = gf_url_percent_encode(tmp ? tmp : "/");
	if (info->remotePath[0]=='?') {
		char *rpath = NULL;
		gf_dynstrcat(&rpath, "/", NULL);
		gf_dynstrcat(&rpath, info->remotePath, NULL);
		gf_free(info->remotePath);
		info->remotePath = rpath;
	}

	if (tmp) {
		tmp[0] = 0;
		copyOfUrl = gf_strdup(url);
		tmp[0] = '/';
	} else {
		copyOfUrl = gf_strdup(url);
	}
	tmp_url = copyOfUrl;
	current_pos = tmp_url;
	tmp = strrchr(tmp_url, '@');
	if (tmp) {
		current_pos = tmp + 1;
		assert( ! info->server_name );
		info->server_name = gf_strdup(current_pos);
		tmp[0] = 0;
		tmp = strchr(tmp_url, ':');

		if (tmp) {
			tmp[0] = 0;
			info->password = gf_strdup(tmp+1);
		}
		info->userName = gf_strdup(tmp_url);
	} else {
		assert( ! info->server_name );
		info->server_name = gf_strdup(tmp_url);
	}

	//scan for port number after IPv6 address ']' end char
	ipv6 = strrchr(current_pos, ']');
	tmp = strrchr(ipv6 ? ipv6 : current_pos, ':');

	if (tmp) {
		info->port = atoi(tmp+1);
		tmp[0] = 0;
		if (info->server_name) {
			gf_free(info->server_name);
		}
		info->server_name = gf_strdup(current_pos);
	}

	/* builds orig_url */
	/* We dont't want orig_url to contain user/passwords for security reasons or mismatch in cache hit */
	{
		char port[8];
		snprintf(port, sizeof(port)-1, ":%d", info->port);
		info->canonicalRepresentation = (char*)gf_malloc(strlen(info->protocol)+strlen(info->server_name)+1+strlen(port)+strlen(info->remotePath));
		strcpy(info->canonicalRepresentation, info->protocol);
		strcat(info->canonicalRepresentation, info->server_name);
		if (info->port!=80)
			strcat(info->canonicalRepresentation, port);
		strcat(info->canonicalRepresentation, info->remotePath);
	}
	gf_free(copyOfUrl);
	if (urlConcatenateWithBaseURL)
		gf_free(urlConcatenateWithBaseURL);
	return GF_OK;
}

char *gf_cache_get_forced_headers(const DownloadedCacheEntry entry);
u32 gf_cache_get_downtime(const DownloadedCacheEntry entry);
Bool gf_cache_is_done(const DownloadedCacheEntry entry);
Bool gf_cache_is_deleted(const DownloadedCacheEntry entry);

static void gf_dm_sess_reload_cached_headers(GF_DownloadSession *sess)
{
	char *hdrs;

	if (!sess || !sess->local_cache_only) return;

	hdrs = gf_cache_get_forced_headers(sess->cache_entry);

	gf_dm_sess_clear_headers(sess);
	while (hdrs) {
		char *sep2, *sepL = strstr(hdrs, "\r\n");
		if (sepL) sepL[0] = 0;
		sep2 = strchr(hdrs, ':');
		if (sep2) {
			GF_HTTPHeader *hdr;
			GF_SAFEALLOC(hdr, GF_HTTPHeader);
			if (!hdr) break;
			sep2[0]=0;
			hdr->name = gf_strdup(hdrs);
			sep2[0]=':';
			sep2++;
			while (sep2[0]==' ') sep2++;
			hdr->value = gf_strdup(sep2);
			gf_list_add(sess->headers, hdr);
		}
		if (!sepL) break;
		sepL[0] = '\r';
		hdrs = sepL + 2;
	}
}

GF_EXPORT
GF_Err gf_dm_sess_setup_from_url(GF_DownloadSession *sess, const char *url, Bool allow_direct_reuse)
{
	Bool socket_changed = GF_FALSE;
	GF_URL_Info info;
	Bool free_proto = GF_FALSE;
	char *sep_frag=NULL;
	if (!url)
		return GF_BAD_PARAM;

	gf_dm_sess_clear_headers(sess);
	sess->allow_direct_reuse = allow_direct_reuse;
	gf_dm_url_info_init(&info);

	if (!sess->sock)
		socket_changed = GF_TRUE;
	else if (sess->status>GF_NETIO_DISCONNECTED)
		socket_changed = GF_TRUE;

	else if (sess->connection_timeout_ms) {
		u32 diff = (u32) ( gf_sys_clock_high_res() - sess->last_fetch_time) / 1000;
		if (diff >= sess->connection_timeout_ms) {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Timeout reached (%d ms since last vs %d ms timeout), reconnecting\n", diff, sess->connection_timeout_ms));
			socket_changed = GF_TRUE;
		}
	}

	assert(sess->status != GF_NETIO_WAIT_FOR_REPLY);
	assert(sess->status != GF_NETIO_DATA_EXCHANGE);

	//strip fragment
	sep_frag = strchr(url, '#');
	if (sep_frag) sep_frag[0]=0;
	sess->last_error = gf_dm_get_url_info(url, &info, sess->orig_url);
	if (sess->last_error) {
		if (sep_frag) sep_frag[0]='#';
		return sess->last_error;
	}

	if (!strstr(url, "://")) {
		char c, *sep;
		gf_dm_url_info_del(&info);
		info.port = sess->port;
		info.server_name = sess->server_name ? gf_strdup(sess->server_name) : NULL;
		info.remotePath = gf_strdup(url);
		sep = strstr(sess->orig_url_before_redirect, "://");
		assert(sep);
		c = sep[3];
		sep[3] = 0;
		info.protocol = gf_strdup(sess->orig_url_before_redirect);
		sep[3] = c;
		free_proto = GF_TRUE;
	}

	if (sess->port != info.port) {
		socket_changed = GF_TRUE;
		sess->port = info.port;
	}

	if (sess->from_cache_only) {
		socket_changed = GF_TRUE;
		sess->from_cache_only = GF_FALSE;
		if (sess->cache_entry) {
			gf_dm_remove_cache_entry_from_session(sess);
			sess->cache_entry = NULL;
		}
	}

	if (!strcmp("http://", info.protocol) || !strcmp("https://", info.protocol)) {
		if (sess->do_requests != http_do_requests) {
			sess->do_requests = http_do_requests;
			socket_changed = GF_TRUE;
		}
		if (!strcmp("https://", info.protocol)) {
			if (!(sess->flags & GF_DOWNLOAD_SESSION_USE_SSL)) {
				sess->flags |= GF_DOWNLOAD_SESSION_USE_SSL;
				socket_changed = GF_TRUE;
			}
		} else if (sess->flags & GF_DOWNLOAD_SESSION_USE_SSL) {
			sess->flags &= ~GF_DOWNLOAD_SESSION_USE_SSL;
			socket_changed = GF_TRUE;
		}
	} else {
		sess->do_requests = NULL;
	}

	if (sess->server_name && info.server_name && !strcmp(sess->server_name, info.server_name)) {
	} else {
		socket_changed = GF_TRUE;
		if (sess->server_name) gf_free(sess->server_name);
		sess->server_name = info.server_name ? gf_strdup(info.server_name) : NULL;
	}

	if (info.canonicalRepresentation) {
		if (sess->orig_url) gf_free(sess->orig_url);
		sess->orig_url = gf_strdup(info.canonicalRepresentation);
	}

	if (!sess->orig_url_before_redirect)
		sess->orig_url_before_redirect = gf_strdup(url);

	if (sess->remote_path) gf_free(sess->remote_path);
	sess->remote_path = gf_strdup(info.remotePath);

	if (sess->status==GF_NETIO_STATE_ERROR)
		socket_changed = GF_TRUE;

	if (!socket_changed && info.userName && !strcmp(info.userName, sess->creds->username)) {
	} else {
		sess->creds = NULL;
		if (info.userName) {
			if (! sess->dm) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP] Did not found any download manager, credentials not supported\n"));
			} else
				sess->creds = gf_user_credentials_register(sess->dm, !strcmp("https://", info.protocol), sess->server_name, info.userName, info.password, info.userName && info.password);
		}
	}
	if (free_proto) gf_free((char *) info.protocol);
	gf_dm_url_info_del(&info);
	if (sep_frag) sep_frag[0]='#';

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess) {
		if (sess->h2_sess->do_shutdown)
			socket_changed = GF_TRUE;
		sess->h2_buf.size = 0;
		sess->h2_buf.offset = 0;
	}
#endif

	if (sess->sock && !socket_changed) {
		sess->status = GF_NETIO_CONNECTED;
		sess->last_error = GF_OK;
		//reset number of retry and start time
		sess->num_retry = SESSION_RETRY_COUNT;
		sess->start_time = 0;
		sess->needs_cache_reconfig = 1;
	} else {

#ifdef GPAC_HAS_HTTP2
		if (sess->h2_sess) {
			gf_mx_p(sess->mx);
			h2_detach_session(sess->h2_sess, sess);
			gf_mx_v(sess->mx);
		}
#endif

		dm_sess_sk_del(sess);

		sess->status = GF_NETIO_SETUP;
		sess->last_error = GF_OK;
		//reset number of retry and start time
		sess->num_retry = SESSION_RETRY_COUNT;
		sess->start_time = 0;
#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			SSL_shutdown(sess->ssl);
			SSL_free(sess->ssl);
			sess->ssl = NULL;
		}
#endif
	}
	sess->total_size = 0;
	sess->bytes_done = 0;
	//could be not-0 after a byte-range request using chunk transfer
	sess->remaining_data_size = 0;

	sess->local_cache_only = GF_FALSE;
	if (sess->dm && sess->dm->local_cache_url_provider_cbk) {
		Bool res = sess->dm->local_cache_url_provider_cbk(sess->dm->lc_udta, (char *)url, GF_FALSE);
		if (res == GF_TRUE) {
			sess->local_cache_only = GF_TRUE;
			gf_free(sess->orig_url);
			sess->orig_url = gf_strdup(url);
			SET_LAST_ERR(sess->last_error)
			sess->use_cache_file = GF_TRUE;
			gf_dm_configure_cache(sess);
			sess->bytes_done = 0;
            if (sess->cache_entry && gf_cache_is_deleted(sess->cache_entry)) {
                sess->status = GF_NETIO_DATA_TRANSFERED;
                SET_LAST_ERR(GF_URL_REMOVED)
                //return GF_OK;
            } else if (! gf_cache_is_done(sess->cache_entry)) {
                sess->total_size = 0;
                sess->status = GF_NETIO_DATA_EXCHANGE;
            } else {
				sess->total_size = gf_cache_get_content_length(sess->cache_entry);
                sess->bytes_done = sess->total_size;
                sess->status = GF_NETIO_DATA_TRANSFERED;
                SET_LAST_ERR(GF_OK)
            }

			sess->total_time_since_req = gf_cache_get_downtime(sess->cache_entry);
			if (sess->total_time_since_req)
				sess->bytes_per_sec = (u32) ((1000 * (u64) sess->bytes_done) / sess->total_time_since_req);
			else
				sess->bytes_per_sec = 0;
			gf_dm_sess_reload_cached_headers(sess);
		}
	}
	if (sess->last_error)
		return sess->last_error;
	return gf_dm_sess_set_range(sess, 0, 0, GF_TRUE);
}


Bool gf_dm_session_do_task(GF_DownloadSession *sess)
{
	Bool do_run = GF_TRUE;

	if (sess->destroy) {
		do_run = GF_FALSE;
	} else {
		Bool unlock = sess->mx ? GF_TRUE : GF_FALSE;
		gf_mx_p(sess->mx);
		if (sess->status >= GF_NETIO_DATA_TRANSFERED) {
			do_run = GF_FALSE;
		} else {
			if (sess->status < GF_NETIO_CONNECTED) {
				gf_dm_connect(sess);
			} else {
				sess->do_requests(sess);
			}
		}
		if (unlock)
			gf_mx_v(sess->mx);
	}
	if (do_run) return GF_TRUE;

	/*destroy all session but keep connection active*/
	gf_dm_disconnect(sess, HTTP_NO_CLOSE);
	if (sess->last_error==GF_EOS) sess->last_error = GF_OK;
	else if (sess->last_error==GF_IP_NETWORK_EMPTY) sess->last_error = GF_OK;
	else if (sess->last_error) {
		sess->status = GF_NETIO_STATE_ERROR;
	}
	return GF_FALSE;
}

Bool gf_dm_session_task(GF_FilterSession *fsess, void *callback, u32 *reschedule_ms)
{
	GF_SessTask *task = callback;
	GF_DownloadSession *sess = task->sess;
	if (!sess) {
		gf_free(task);
		return GF_FALSE;
	}
	Bool ret = gf_dm_session_do_task(sess);
	if (ret) {
		*reschedule_ms = 1;
		return GF_TRUE;
	}
	assert(sess->ftask);
	gf_free(sess->ftask);
	sess->ftask = NULL;
	if (sess->destroy) {
		gf_dm_sess_del(sess);
	}
	return GF_FALSE;
}

#ifndef GPAC_DISABLE_THREADS
static u32 gf_dm_session_thread(void *par)
{
	GF_DownloadSession *sess = (GF_DownloadSession *)par;
	if (!sess) return 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Downloader] Entering thread ID %d\n", gf_th_id() ));
	sess->flags &= ~GF_DOWNLOAD_SESSION_THREAD_DEAD;
	while (!sess->destroy) {
		Bool ret = gf_dm_session_do_task(sess);
		if (!ret) break;
		gf_sleep(0);
	}
	sess->flags |= GF_DOWNLOAD_SESSION_THREAD_DEAD;
	if (sess->destroy)
		gf_dm_sess_del(sess);
	return 1;
}
#endif

static GF_DownloadSession *gf_dm_sess_new_internal(GF_DownloadManager * dm, const char *url, u32 dl_flags,
        gf_dm_user_io user_io,
        void *usr_cbk,
        GF_Socket *server,
        GF_Err *e)
{
	GF_DownloadSession *sess;

	GF_SAFEALLOC(sess, GF_DownloadSession);
	if (!sess) {
		return NULL;
	}
	sess->headers = gf_list_new();
	sess->flags = dl_flags;
	if (sess->flags & GF_NETIO_SESSION_NOTIFY_DATA)
		sess->force_data_write_callback = GF_TRUE;
	sess->user_proc = user_io;
	sess->usr_cbk = usr_cbk;
	sess->creds = NULL;

	sess->conn_timeout = gf_opts_get_int("core", "tcp-timeout");
	if (!sess->conn_timeout) sess->conn_timeout = 5000;

	sess->request_timeout = gf_opts_get_int("core", "req-timeout");

	sess->chunk_wnd_dur = gf_opts_get_int("core", "cte-rate-wnd") * 1000;
	if (!sess->chunk_wnd_dur) sess->chunk_wnd_dur = 20000;

	sess->dm = dm;
	if (server) {
		sess->sock = server;
		sess->flags = GF_NETIO_SESSION_NOT_THREADED;
		sess->status = GF_NETIO_CONNECTED;
		sess->server_mode = GF_TRUE;
		sess->do_requests = http_do_requests;
		if (e) *e = GF_OK;
		if (dl_flags & GF_NETIO_SESSION_NO_BLOCK) {
			sess->flags |= GF_NETIO_SESSION_NO_BLOCK;
			gf_sk_set_block_mode(server, GF_TRUE);
		}
		return sess;
	}

	if (!sess->conn_timeout) sess->server_only_understand_get = GF_TRUE;
	if (dm)
		sess->disable_cache = dm->disable_cache;

	if (! (dl_flags & GF_NETIO_SESSION_NOT_THREADED)) {
		sess->mx = gf_mx_new(url);
		if (!sess->mx) {
			gf_free(sess);
			return NULL;
		}
	}

	*e = gf_dm_sess_setup_from_url(sess, url, GF_FALSE);
	if (*e) {
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] failed to create session for %s: %s\n", url, gf_error_to_string(*e)));
		gf_dm_sess_del(sess);
		return NULL;
	}
	assert( sess );
	sess->num_retry = SESSION_RETRY_COUNT;
	/*threaded session must be started with gf_dm_sess_process*/
	return sess;
}

GF_EXPORT
GF_DownloadSession *gf_dm_sess_new_server(GF_DownloadManager *dm, GF_Socket *server,
		void *ssl_sock_ctx,
        gf_dm_user_io user_io,
        void *usr_cbk,
        Bool async,
        GF_Err *e)
{
	GF_DownloadSession *sess;

#if defined(GPAC_HAS_HTTP2) && defined(GPAC_HAS_SSL)
	Bool h2_negotiated = GF_FALSE;
	if (ssl_sock_ctx) {
		const unsigned char *alpn = NULL;
		unsigned int alpnlen = 0;
		SSL_get0_next_proto_negotiated(ssl_sock_ctx, &alpn, &alpnlen);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
		if (alpn == NULL) {
			SSL_get0_alpn_selected(ssl_sock_ctx, &alpn, &alpnlen);
		}
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L

		if (alpn && (alpnlen == 2) && !memcmp("h2", alpn, 2)) {
			h2_negotiated = GF_TRUE;
		}
	}
#endif //GPAC_HAS_HTTP2 && GPAC_HAS_SSL

	sess = gf_dm_sess_new_internal(dm, NULL, async ? GF_NETIO_SESSION_NO_BLOCK : 0, user_io, usr_cbk, server, e);

#ifdef GPAC_HAS_SSL
	if (sess) {
		sess->ssl = ssl_sock_ctx;
		if (ssl_sock_ctx && (sess->flags & GF_NETIO_SESSION_NO_BLOCK))
			SSL_set_mode(sess->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER|SSL_MODE_ENABLE_PARTIAL_WRITE);

#if defined(GPAC_HAS_HTTP2)
		if (h2_negotiated) {
			h2_initialize_session(sess);
		}
#endif
	}
#endif
	return sess;
}

GF_DownloadSession *gf_dm_sess_new_subsession(GF_DownloadSession *sess, u32 stream_id, void *usr_cbk, GF_Err *e)
{
#ifdef GPAC_HAS_HTTP2
	GF_DownloadSession *sub_sess;
	if (!sess->h2_sess || !stream_id) return NULL;
	gf_mx_p(sess->mx);
	sub_sess = gf_dm_sess_new_internal(NULL, NULL, 0, sess->user_proc, usr_cbk, sess->sock, e);
	if (!sub_sess) {
		gf_mx_v(sess->mx);
		return NULL;
	}
	gf_list_add(sess->h2_sess->sessions, sub_sess);
#ifdef GPAC_HAS_SSL
	sub_sess->ssl = sess->ssl;
#endif
	sub_sess->h2_sess = sess->h2_sess;
	if (sub_sess->mx) gf_mx_del(sub_sess->mx);
	sub_sess->mx = sess->h2_sess->mx;
	sub_sess->h2_stream_id = stream_id;
	sub_sess->status = GF_NETIO_CONNECTED;
	sub_sess->data_io.read_callback = h2_data_source_read_callback;
	sub_sess->data_io.source.ptr = sub_sess;
	sub_sess->flags = sess->flags;
	gf_mx_v(sess->mx);
	return sub_sess;
#else
	return NULL;
#endif
}

u32 gf_dm_sess_subsession_count(GF_DownloadSession *sess)
{
#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess)
		return gf_list_count(sess->h2_sess->sessions);
#endif
	return 1;
}


void gf_dm_sess_server_reset(GF_DownloadSession *sess)
{
	if (!sess->server_mode) return;

	gf_dm_sess_clear_headers(sess);
	sess->total_size = sess->bytes_done = 0;
	sess->chunk_bytes = 0;
	sess->chunk_header_bytes = 0;
	sess->chunked = GF_FALSE;
	sess->status = GF_NETIO_CONNECTED;
}


GF_EXPORT
GF_DownloadSession *gf_dm_sess_new_simple(GF_DownloadManager * dm, const char *url, u32 dl_flags,
        gf_dm_user_io user_io,
        void *usr_cbk,
        GF_Err *e)
{
	return gf_dm_sess_new_internal(dm, url, dl_flags, user_io, usr_cbk, NULL, e);
}
GF_EXPORT
GF_DownloadSession *gf_dm_sess_new(GF_DownloadManager *dm, const char *url, u32 dl_flags,
                                   gf_dm_user_io user_io,
                                   void *usr_cbk,
                                   GF_Err *e)
{
	GF_DownloadSession *sess;
	*e = GF_OK;
	if (gf_dm_is_local(dm, url)) {
		*e = GF_NOT_SUPPORTED;
		return NULL;
	}

	if (!gf_dm_can_handle_url(dm, url)) {
		*e = GF_NOT_SUPPORTED;
		return NULL;
	}
	sess = gf_dm_sess_new_simple(dm, url, dl_flags, user_io, usr_cbk, e);
	if (sess && dm) {
		sess->dm = dm;
		gf_mx_p(dm->cache_mx);
		gf_list_add(dm->sessions, sess);
		gf_mx_v(dm->cache_mx);
	}
	return sess;
}

static GF_Err gf_dm_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read)
{
	GF_Err e;

	if (sess->dm && sess->dm->simulate_no_connection) {
		if (sess->sock) {
			sess->status = GF_NETIO_DISCONNECTED;
		}
		return GF_IP_NETWORK_FAILURE;
	}

	if (!sess)
		return GF_BAD_PARAM;

	gf_mx_p(sess->mx);
	if (!sess->sock) {
		sess->status = GF_NETIO_DISCONNECTED;
		gf_mx_v(sess->mx);
		return GF_IP_CONNECTION_CLOSED;
	}

	*out_read = 0;

#ifdef GPAC_HAS_SSL
	if (sess->ssl) {
		s32 size;

		//receive on null buffer (select only, check if data available)
		e = gf_sk_receive(sess->sock, NULL, 0, NULL);
		//empty and no pending bytes in SSL, network empty
		if ((e==GF_IP_NETWORK_EMPTY) &&
#if 1
			!SSL_pending(sess->ssl)
#else
			//no support for SSL_has_pending in old libSSL and same result can be achieved with SSL_pending
			!SSL_has_pending(sess->ssl)
#endif
		) {
			gf_mx_v(sess->mx);
			return e;
		}
		size = SSL_read(sess->ssl, data, data_size);
		if (size < 0) {
			int err = SSL_get_error(sess->ssl, size);
			if (err==SSL_ERROR_SSL) {
/*
				char msg[1024];
				SSL_load_error_strings();
				ERR_error_string_n(ERR_get_error(), msg, sizeof(msg));
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Cannot read, error %s\n", msg));
*/
				e = GF_IO_ERR;
			} else {
				e = gf_sk_probe(sess->sock);
			}
		} else if (!size)
			e = GF_IP_NETWORK_EMPTY;
		else {
			e = GF_OK;
			data[size] = 0;
			*out_read = size;
		}
	} else
#endif

		e = gf_sk_receive(sess->sock, data, data_size, out_read);

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess) {
		if (*out_read > 0) {
			ssize_t read_len = nghttp2_session_mem_recv(sess->h2_sess->ng_sess, data, *out_read);
			if(read_len < 0 ) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] nghttp2_session_mem_recv error:  %s\n", nghttp2_strerror((int) read_len)));
				return GF_IO_ERR;
			}
		}
		/* send pending frames - h2_sess may be NULL at this point if the connection was reset during processing of nghttp2_session_mem_recv
			this typically happens if we have a refused stream
		 */
		if (sess->h2_sess)
			h2_session_send(sess);
	}
#endif //GPAC_HAS_HTTP2

	if (*out_read)
		sess->last_fetch_time = gf_sys_clock_high_res();

	gf_mx_v(sess->mx);
	return e;
}


#ifdef GPAC_HAS_SSL

#define LWR(x) ('A' <= (x) && (x) <= 'Z' ? (x) - 32 : (x))

static Bool rfc2818_match(const char *pattern, const char *string)
{
	char d;
	u32 i=0, k=0;
	while (1) {
		char c = LWR(pattern[i]);
		if (c == '\0') break;

		if (c=='*') {
			/*remove *** patterns*/
			while (c == '*') {
				i++;
				c = LWR(pattern[i]);
			}
			/*look for same c character*/
			while (1) {
				d = LWR(string[k]);
				if (d == '\0') break;
				/*matched c character, check following substrings*/
				if ((d == c) && rfc2818_match (&pattern[i], &string[k]))
					return GF_TRUE;
				else if (d == '.')
					return GF_FALSE;

				k++;
			}
			return (c == '\0') ? GF_TRUE : GF_FALSE;
		} else {
			if (c != LWR(string[k]))
				return GF_FALSE;
		}
		i++;
		k++;
	}
	return (string[k]=='\0') ? GF_TRUE : GF_FALSE;
}
#undef LWR

#endif


#ifdef GPAC_HAS_SSL
Bool gf_ssl_check_cert(SSL *ssl, const char *server_name)
{
	Bool success;
	X509 *cert = SSL_get_peer_certificate(ssl);
	if (!cert) return GF_TRUE;

	long vresult;
	SSL_set_verify_result(ssl, 0);
	vresult = SSL_get_verify_result(ssl);

	if (vresult == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[SSL] Cannot locate issuer's certificate on the local system, will not attempt to validate\n"));
		SSL_set_verify_result(ssl, 0);
		vresult = SSL_get_verify_result(ssl);
	}

	if (vresult == X509_V_OK) {
		char common_name[256];
		STACK_OF(GENERAL_NAME) *altnames;
		GF_List* valid_names;
		int i;

		valid_names = gf_list_new();

		common_name[0] = 0;
		X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, common_name, sizeof (common_name));
		gf_list_add(valid_names, common_name);

		altnames = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
		if (altnames) {
			for (i = 0; i < sk_GENERAL_NAME_num(altnames); ++i) {
				const GENERAL_NAME *altname = sk_GENERAL_NAME_value(altnames, i);
				if (altname->type == GEN_DNS)
				{
					#if OPENSSL_VERSION_NUMBER < 0x10100000L
						unsigned char *altname_str = ASN1_STRING_data(altname->d.ia5);
					#else
						unsigned char *altname_str = (unsigned char *)ASN1_STRING_get0_data(altname->d.ia5);
					#endif
					gf_list_add(valid_names, altname_str);
				}
			}
		}

		success = GF_FALSE;
		for (i = 0; i < (int)gf_list_count(valid_names); ++i) {
			const char *valid_name = (const char*) gf_list_get(valid_names, i);
			if (rfc2818_match(valid_name, server_name)) {
				success = GF_TRUE;
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[SSL] Hostname %s matches %s\n", server_name, valid_name));
				break;
			}
		}
		if (!success) {
			if ( gf_opts_get_bool("core", "broken-cert")) {
				success = GF_TRUE;
				GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[SSL] Mismatch in certificate names: expected %s\n", server_name));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Mismatch in certificate names, try using -broken-cert: expected %s\n", 	server_name));
			}
#ifndef GPAC_DISABLE_LOG
			for (i = 0; i < (int)gf_list_count(valid_names); ++i) {
				const char *valid_name = (const char*) gf_list_get(valid_names, i);
				GF_LOG(success ? GF_LOG_DEBUG : GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Tried name: %s\n", valid_name));
			}
#endif
		}

		gf_list_del(valid_names);
		GENERAL_NAMES_free(altnames);
	} else {
		success = GF_FALSE;
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Error verifying certificate %x\n", vresult));
	}

	X509_free(cert);
	return success;
}

#endif

static void gf_dm_connect(GF_DownloadSession *sess)
{
	GF_Err e;
	u16 proxy_port = 0;
	const char *proxy;

#ifdef GPAC_HAS_HTTP2

	if (sess->h2_switch_sess) {
		sess->h2_switch_sess = 0;
		gf_mx_p(sess->mx);
		h2_detach_session(sess->h2_sess, sess);
		gf_mx_v(sess->mx);

		if (sess->num_retry) {
			SET_LAST_ERR(GF_OK)
			sess->num_retry--;
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) refused by server, retrying and marking session as no longer available\n", sess->h2_stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));

			sess->h2_stream_id = 0;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) refused by server after all retries, marking session as no longer available\n", sess->h2_stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(GF_REMOTE_SERVICE_ERROR)
			sess->h2_stream_id = 0;
			return;
		}
	}
	if (sess->h2_sess) {
		sess->connect_time = 0;
		sess->status = GF_NETIO_CONNECTED;
		sess->last_error = GF_OK;
		gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
		gf_dm_configure_cache(sess);
		return;
	}

	if (sess->dm && !sess->dm->disable_http2 && !sess->sock && (sess->h2_upgrade_state!=3)) {
		u32 i, count = gf_list_count(sess->dm->sessions);
		for (i=0; i<count; i++) {
			GF_DownloadSession *a_sess = gf_list_get(sess->dm->sessions, i);
			if (strcmp(a_sess->server_name, sess->server_name)) continue;
			if (!a_sess->h2_sess) {
				//we have the same server name
				//trick in non-block mode, we want to wait for upgrade to be completed
				//before creating a new socket
				if ((a_sess != sess)
					&& a_sess->sock
					&& (a_sess->status <= GF_NETIO_WAIT_FOR_REPLY)
					&& (a_sess->h2_upgrade_state<2)
					&& (sess->flags & GF_NETIO_SESSION_NO_BLOCK)
					&& !sess->dm->disable_http2
					&& !gf_opts_get_bool("core", "no-h2c")
				) {
					sess->connect_pending = 1;
					SET_LAST_ERR(GF_IP_NETWORK_EMPTY)
					return;
				}
				continue;
			}
			if (a_sess->h2_sess->do_shutdown) continue;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] associating session %s to existing http2 session\n", sess->remote_path ? sess->remote_path : sess->orig_url));

			u32 nb_locks=0;
			if (sess->mx) {
				nb_locks = gf_mx_get_num_locks(sess->mx);
				gf_mx_del(sess->mx);
			}
			sess->h2_sess = a_sess->h2_sess;
			sess->mx = a_sess->h2_sess->mx;
			if (nb_locks)
				gf_mx_p(sess->mx);

			sess->sock = a_sess->sock;
#ifdef GPAC_HAS_SSL
			sess->ssl = a_sess->ssl;
#endif
			sess->data_io.read_callback = h2_data_source_read_callback;
			sess->data_io.source.ptr = sess;
			gf_list_add(sess->h2_sess->sessions, sess);

			if (sess->allow_direct_reuse) {
				gf_dm_configure_cache(sess);
				if (sess->from_cache_only) return;
			}

			sess->connect_time = 0;
			sess->status = GF_NETIO_CONNECTED;
			gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
			gf_dm_configure_cache(sess);
			return;
		}
	}
#endif

	Bool register_sock = GF_FALSE;
	if (!sess->sock) {
		sess->sock = gf_sk_new_ex(GF_SOCK_TYPE_TCP, sess->netcap_id);

		if (sess->sock && (sess->flags & GF_NETIO_SESSION_NO_BLOCK))
			gf_sk_set_block_mode(sess->sock, GF_TRUE);

		if (sess->sock_group) register_sock = GF_TRUE;
	}

	/*connect*/
	sess->status = GF_NETIO_SETUP;
	gf_dm_sess_notify_state(sess, sess->status, GF_OK);

	/*PROXY setup*/
	if (sess->proxy_enabled!=2) {
		proxy = NULL;
		if (gf_opts_get_bool("core", "proxy-on")) {
			u32 i;
			Bool use_proxy=GF_TRUE;
			for (i=0; i<gf_list_count(sess->dm->skip_proxy_servers); i++) {
				char *skip = (char*)gf_list_get(sess->dm->skip_proxy_servers, i);
				if (!strcmp(skip, sess->server_name)) {
					use_proxy=GF_FALSE;
					break;
				}
			}
			if (use_proxy) {
				proxy_port = gf_opts_get_int("core", "proxy-port");
				if (!proxy_port) proxy_port = 80;
				proxy = gf_opts_get_key("core", "proxy-name");
				sess->proxy_enabled = 1;
			} else {
				proxy = NULL;
			}
		} else {
			proxy = NULL;
			sess->proxy_enabled = 0;
		}
	} else {
		proxy = NULL;
	}


	if (!proxy) {
		proxy = sess->server_name;
		proxy_port = sess->port;
	}
	if (!sess->connect_pending) {
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Connecting to %s:%d for URL %s\n", proxy, proxy_port, sess->remote_path ? sess->remote_path : "undefined"));
	}

	if ((sess->status == GF_NETIO_SETUP) && (sess->connect_pending<2)) {
		u64 now;
		if (sess->dm && sess->dm->simulate_no_connection) {
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(GF_IP_NETWORK_FAILURE)
			gf_dm_sess_notify_state(sess, sess->status, sess->last_error);
			return;
		}

		sess->async_buf_size = 0;
		now = gf_sys_clock_high_res();
		if ((sess->flags & GF_NETIO_SESSION_NO_BLOCK) && !sess->start_time)
			sess->start_time = now;

		e = gf_sk_connect(sess->sock, (char *) proxy, proxy_port, NULL);

		/*retry*/
		if (e == GF_IP_NETWORK_EMPTY) {
			if (sess->num_retry) {
				if (register_sock) gf_sk_group_register(sess->sock_group, sess->sock);
				sess->status = GF_NETIO_SETUP;
				//reset pending flag
				sess->connect_pending = 0;
				if (sess->flags & GF_NETIO_SESSION_NO_BLOCK) {
					//either timeout or set pending flag
					if ((now - sess->start_time) / 1000 > sess->conn_timeout) {
						sess->num_retry = 0;
					} else {
						sess->connect_pending = 1;
					}
				} else {
					sess->num_retry--;
					sess->connect_pending = 1;
				}
				if (sess->connect_pending) {
					SET_LAST_ERR(GF_IP_NETWORK_EMPTY)
					return;
				}
			}

			e = GF_IP_CONNECTION_FAILURE;
		}

		sess->connect_pending = 0;
		SET_LAST_ERR(GF_OK)

		/*failed*/
		if (e) {
			if (!sess->cache_entry && sess->dm && sess->dm->allow_offline_cache) {
				gf_dm_configure_cache(sess);
				if (sess->from_cache_only) return;
			}
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(e)
			gf_dm_sess_notify_state(sess, sess->status, e);
			return;
		}
		if (register_sock) gf_sk_group_register(sess->sock_group, sess->sock);
		if (sess->allow_direct_reuse) {
			gf_dm_configure_cache(sess);
			if (sess->from_cache_only) return;
		}

		sess->connect_time = (u32) (gf_sys_clock_high_res() - now);
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Connected to %s:%d\n", proxy, proxy_port));
		gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
//		gf_sk_set_buffer_size(sess->sock, GF_TRUE, GF_DOWNLOAD_BUFFER_SIZE);
//		gf_sk_set_buffer_size(sess->sock, GF_FALSE, GF_DOWNLOAD_BUFFER_SIZE);
	}

	if (sess->status == GF_NETIO_SETUP)
		sess->status = GF_NETIO_CONNECTED;

#ifdef GPAC_HAS_SSL
	if ((!sess->ssl || (sess->connect_pending==2)) && (sess->flags & (GF_DOWNLOAD_SESSION_USE_SSL|GF_DOWNLOAD_SESSION_SSL_FORCED))) {
		u64 now = gf_sys_clock_high_res();
		if (sess->dm && !sess->dm->ssl_ctx)
			gf_dm_ssl_init(sess->dm, 0);
		/*socket is connected, configure SSL layer*/
		if (sess->dm && sess->dm->ssl_ctx) {
			int ret;
			Bool success;

			if (!sess->ssl) {
				sess->ssl = SSL_new(sess->dm->ssl_ctx);
				SSL_set_fd(sess->ssl, gf_sk_get_handle(sess->sock));
				SSL_ctrl(sess->ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, (void*) proxy);
				SSL_set_connect_state(sess->ssl);

				if (sess->flags & GF_NETIO_SESSION_NO_BLOCK)
					SSL_set_mode(sess->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER|SSL_MODE_ENABLE_PARTIAL_WRITE);

#ifdef GPAC_HAS_HTTP2
				if (sess->h2_upgrade_state==3) {
					SSL_set_alpn_protos(sess->ssl, NULL, 0);
					sess->h2_upgrade_state = 0;
				}
#endif
			}

			sess->connect_pending = 0;
			ret = SSL_connect(sess->ssl);
			if (ret<=0) {
				ret = SSL_get_error(sess->ssl, ret);
				if (ret==SSL_ERROR_SSL) {

#if (OPENSSL_VERSION_NUMBER >= 0x10002000L) && defined(GPAC_HAS_HTTP2)
					//error and we tried with alpn for h2, retry without (kill/reconnect)
					if (!sess->h2_upgrade_state && !sess->dm->disable_http2) {
						SSL_free(sess->ssl);
						sess->ssl = NULL;
						dm_sess_sk_del(sess);
						sess->status = GF_NETIO_SETUP;
						sess->h2_upgrade_state = 3;
						gf_dm_connect(sess);
						return;
					}
#endif

					char msg[1024];
					SSL_load_error_strings();
					ERR_error_string_n(ERR_get_error(), msg, sizeof(msg));
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Cannot connect, error %s\n", msg));
#ifdef GPAC_HAS_HTTP2
					if (!sess->dm->disable_http2) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("\tYou may want to retry with HTTP/2 support disabled (-no-h2)\n"));
					}
#endif
					SET_LAST_ERR(GF_SERVICE_ERROR)
				} else if ((ret==SSL_ERROR_WANT_READ) || (ret==SSL_ERROR_WANT_WRITE)) {
					sess->status = GF_NETIO_SETUP;
					sess->connect_pending = 2;
					return;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Cannot connect, error %d\n", ret));
					SET_LAST_ERR(GF_REMOTE_SERVICE_ERROR)
				}
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[SSL] connected\n"));


#ifdef GPAC_HAS_HTTP2
				if (!sess->dm->disable_http2) {
					const u8 *alpn = NULL;
					u32 alpnlen = 0;
					SSL_get0_next_proto_negotiated(sess->ssl, &alpn, &alpnlen);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
					if (alpn == NULL) {
						SSL_get0_alpn_selected(sess->ssl, &alpn, &alpnlen);
					}
#endif
					if (alpn == NULL || alpnlen != 2 || memcmp("h2", alpn, 2) != 0) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[SSL] HTTP/2 is not negotiated\n"));
					} else {
						h2_initialize_session(sess);
					}
				}
#endif
			}

			success = gf_ssl_check_cert(sess->ssl, sess->server_name);
			if (!success) {
				gf_dm_disconnect(sess, HTTP_RESET_CONN);
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(GF_AUTHENTICATION_FAILURE)
				gf_dm_sess_notify_state(sess, sess->status, sess->last_error);
			}

			sess->ssl_setup_time = (u32) (gf_sys_clock_high_res() - now);
		}
	}
#endif

	/*this should be done when building HTTP GET request in case we have range directives*/
	gf_dm_configure_cache(sess);

}

DownloadedCacheEntry gf_dm_refresh_cache_entry(GF_DownloadSession *sess)
{
	Bool go;
	u32 timer = 0;
	u32 flags;
	if (!sess) return NULL;
	flags = sess->flags;
	sess->flags |= GF_NETIO_SESSION_NOT_CACHED;
	go = GF_TRUE;
	while (go) {
		switch (sess->status) {
		/*setup download*/
		case GF_NETIO_SETUP:
			gf_dm_connect(sess);
			break;
		case GF_NETIO_WAIT_FOR_REPLY:
			if (timer == 0)
				timer = gf_sys_clock();
			{
				u32 timer2 = gf_sys_clock();
				if (timer2 - timer > 5000) {
					GF_Err e;
					/* Since HEAD is not understood by this server, we use a GET instead */
					sess->http_read_type = GET;
					sess->flags |= GF_NETIO_SESSION_NOT_CACHED;
					gf_dm_disconnect(sess, HTTP_NO_CLOSE);
					sess->status = GF_NETIO_SETUP;
					sess->server_only_understand_get = GF_TRUE;
					GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("gf_dm_refresh_cache_entry() : Timeout with HEAD, try with GET\n"));
					e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
					if (e) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("gf_dm_refresh_cache_entry() : Error with GET %d\n", e));
						sess->status = GF_NETIO_STATE_ERROR;
						SET_LAST_ERR(e)
						gf_dm_sess_notify_state(sess, sess->status, e);
					} else {
						timer = 0;
						continue;
					}
				}
			}
		case GF_NETIO_CONNECTED:
			sess->do_requests(sess);
			break;
		case GF_NETIO_DATA_EXCHANGE:
		case GF_NETIO_DISCONNECTED:
		case GF_NETIO_STATE_ERROR:
		case GF_NETIO_DATA_TRANSFERED:
			go = GF_FALSE;
			break;
		default:
			break;
		}
	}
	sess->flags = flags;
	if (sess->status==GF_NETIO_STATE_ERROR) return NULL;
	return sess->cache_entry;
}

GF_EXPORT
const char *gf_dm_sess_mime_type(GF_DownloadSession *sess)
{
	DownloadedCacheEntry entry;
	if (sess->cache_entry) {
		const char * oldMimeIfAny = gf_cache_get_mime_type(sess->cache_entry);
		if (oldMimeIfAny)
			return oldMimeIfAny;
	}
	entry = gf_dm_refresh_cache_entry (sess);
	if (!entry)
		return sess->mime_type;
	assert( entry == sess->cache_entry && entry);
	return gf_cache_get_mime_type( sess->cache_entry );
}

GF_EXPORT
GF_Err gf_dm_sess_set_range(GF_DownloadSession *sess, u64 start_range, u64 end_range, Bool discontinue_cache)
{
	if (!sess)
		return GF_BAD_PARAM;
	if (sess->cache_entry) {
		if (!discontinue_cache) {
			if (gf_cache_get_end_range(sess->cache_entry) + 1 != start_range)
				discontinue_cache = GF_TRUE;
		}
		if (sess->sock) {
			switch (sess->status) {
			case GF_NETIO_CONNECTED:
			case GF_NETIO_DISCONNECTED:
			case GF_NETIO_DATA_TRANSFERED:
				break;
			default:
				return GF_BAD_PARAM;
			}
		}
		if (!sess->local_cache_only) {
			sess->status = sess->sock ? GF_NETIO_CONNECTED : GF_NETIO_SETUP;
			sess->num_retry = SESSION_RETRY_COUNT;

			if (!discontinue_cache) {
				gf_cache_set_end_range(sess->cache_entry, end_range);
				/*remember this in case we get disconnected*/
				sess->is_range_continuation = GF_TRUE;
			} else {
				sess->needs_cache_reconfig = 1;
				sess->reused_cache_entry = GF_FALSE;
			}
		}
	} else {
		if (sess->status == GF_NETIO_DISCONNECTED)
			sess->status = GF_NETIO_SETUP;
		else if ((sess->status != GF_NETIO_SETUP) && (sess->status != GF_NETIO_CONNECTED))
			return GF_BAD_PARAM;
	}
	sess->range_start = start_range;
	sess->range_end = end_range;
	sess->needs_range = (start_range || end_range) ? GF_TRUE : GF_FALSE;
	return GF_OK;
}

#ifdef GPAC_HAS_HTTP2
static void gf_dm_sess_flush_input(GF_DownloadSession *sess)
{
	char sHTTP[GF_DOWNLOAD_BUFFER_SIZE+1];
	u32 res;
	sHTTP[0] = 0;
	GF_Err e = gf_dm_read_data(sess, sHTTP, GF_DOWNLOAD_BUFFER_SIZE, &res);
	switch (e) {
	case GF_IP_NETWORK_EMPTY:
	case GF_OK:
		return;
	default:
		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR(e)
		return;
	}
}
#endif

GF_EXPORT
GF_Err gf_dm_sess_process(GF_DownloadSession *sess)
{
	Bool go;

#ifndef GPAC_DISABLE_THREADS
	/*if session is threaded, start thread*/
	if (! (sess->flags & GF_NETIO_SESSION_NOT_THREADED)) {
		if (sess->dm->filter_session && !gf_opts_get_bool("core", "dm-threads")) {
			if (sess->ftask)
				return GF_OK;
			GF_SAFEALLOC(sess->ftask, GF_SessTask);
			if (!sess->ftask) return GF_OUT_OF_MEM;
			sess->ftask->sess = sess;
			gf_fs_post_user_task(sess->dm->filter_session, gf_dm_session_task, sess->ftask, "download");
			return GF_OK;
		}
		if (sess->th) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTP] Session already started - ignoring start\n"));
			return GF_OK;
		}
		sess->th = gf_th_new( gf_file_basename(sess->orig_url) );
		if (!sess->th) return GF_OUT_OF_MEM;
		gf_th_run(sess->th, gf_dm_session_thread, sess);
		return GF_OK;
	}
#endif //GPAC_DISABLE_THREADS

	if (sess->put_state==2) {
		if (sess->status==GF_NETIO_DATA_TRANSFERED)
			sess->status = GF_NETIO_WAIT_FOR_REPLY;
	}

	/*otherwise do a synchronous download*/
	go = GF_TRUE;
	while (go) {
		switch (sess->status) {
		/*setup download*/
		case GF_NETIO_SETUP:
			gf_dm_connect(sess);
			if (sess->connect_pending) {
				go = GF_FALSE;
			}
			break;
		case GF_NETIO_WAIT_FOR_REPLY:
		case GF_NETIO_CONNECTED:
			sess->do_requests(sess);
			if (sess->server_mode) {
				if (sess->status == GF_NETIO_STATE_ERROR) {
					sess->status = GF_NETIO_DISCONNECTED;
					SET_LAST_ERR(GF_IP_CONNECTION_CLOSED)
					sess_connection_closed(sess);
					go = GF_FALSE;
				} else if (sess->last_error==GF_IP_NETWORK_EMPTY) {
					go = GF_FALSE;
				} else if (sess->status==GF_NETIO_DISCONNECTED) {
					go = GF_FALSE;
				}
			} else if (sess->flags & GF_NETIO_SESSION_NO_BLOCK) {
				if (sess->last_error==GF_IP_NETWORK_EMPTY)
					go = GF_FALSE;
			}
			break;
		case GF_NETIO_DATA_EXCHANGE:
			if (sess->put_state==2) {
				sess->status = GF_NETIO_DATA_TRANSFERED;
                SET_LAST_ERR(GF_OK)
				go = GF_FALSE;
				break;
			}
			sess->do_requests(sess);
			break;
		case GF_NETIO_DATA_TRANSFERED:
#ifdef GPAC_HAS_HTTP2
			if (sess->h2_sess && sess->server_mode) {
				gf_dm_sess_flush_input(sess);
				h2_session_send(sess);
			}
#endif
			go = GF_FALSE;
			break;
		case GF_NETIO_DISCONNECTED:
		case GF_NETIO_STATE_ERROR:
			go = GF_FALSE;
			break;

		case GF_NETIO_GET_METHOD:
		case GF_NETIO_GET_HEADER:
		case GF_NETIO_GET_CONTENT:
		case GF_NETIO_PARSE_HEADER:
		case GF_NETIO_PARSE_REPLY:
			break;

		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[Downloader] Session in unknown state !! - aborting\n"));
			go = GF_FALSE;
			break;
		}
	}
	return sess->last_error;
}

GF_EXPORT
GF_Err gf_dm_sess_process_headers(GF_DownloadSession *sess)
{
	Bool go;
	if (!(sess->flags & GF_NETIO_SESSION_NOT_THREADED)) {
		return GF_OK;
	}

	go = GF_TRUE;
	while (go) {
		switch (sess->status) {
		/*setup download*/
		case GF_NETIO_SETUP:
			gf_dm_connect(sess);
			break;
		case GF_NETIO_WAIT_FOR_REPLY:
		case GF_NETIO_CONNECTED:
			sess->do_requests(sess);

			if (sess->reused_cache_entry && sess->cache_entry && gf_cache_are_headers_processed(sess->cache_entry) ) {
				sess->status = GF_NETIO_DATA_EXCHANGE;
			}
			break;
		case GF_NETIO_DATA_EXCHANGE:
		case GF_NETIO_DATA_TRANSFERED:
		case GF_NETIO_DISCONNECTED:
		case GF_NETIO_STATE_ERROR:
			go = GF_FALSE;
			break;
		default:
			break;
		}
	}
	return sess->last_error;
}

static Bool gf_dm_needs_to_delete_cache(GF_DownloadManager * dm)
{
	if (!dm) return GF_FALSE;
	return dm->clean_cache;
}

#ifdef BUGGY_gf_cache_cleanup_cache
/*!
 * Cleans up the cache at start and stop.
 * Note that this method will perform any cleanup if
 * Configuration section [Downloader]/CleanCache is not set, meaning
 * that methods that create a "fake" GF_DownloadManager such as
 * gf_dm_wget() are not impacted and won't cleanup the cache
 *
 * FIXME: should be probably threaded to avoid too long start time
\param dm The GF_DownloadManager
 */
static void gf_cache_cleanup_cache(GF_DownloadManager * dm) {
	if (gf_dm_needs_to_delete_cache(dm)) {
		gf_cache_delete_all_cached_files(dm->cache_directory);
	}
}
#endif

typedef struct
{
	Bool check_size;
	u64 out_size;
} cache_probe;


static void gf_dm_clean_cache(GF_DownloadManager *dm)
{
	u64 out_size = gf_cache_get_size(dm->cache_directory);
	if (out_size >= dm->max_cache_size) {
		GF_LOG(dm->max_cache_size ? GF_LOG_WARNING : GF_LOG_INFO, GF_LOG_HTTP, ("[Cache] Cache size %d exceeds max allowed %d, deleting entire cache\n", out_size, dm->max_cache_size));
		gf_cache_delete_all_cached_files(dm->cache_directory);
	}
}

GF_EXPORT
GF_DownloadManager *gf_dm_new(GF_FilterSession *fsess)
{
	const char *opt;
	const char * default_cache_dir;
	GF_DownloadManager *dm;
	GF_SAFEALLOC(dm, GF_DownloadManager);
	if (!dm) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[Downloader] Failed to allocate downloader\n"));
		return NULL;
	}
	dm->sessions = gf_list_new();
	dm->cache_entries = gf_list_new();
	dm->credentials = gf_list_new();
	dm->skip_proxy_servers = gf_list_new();
	dm->partial_downloads = gf_list_new();
	dm->cache_mx = gf_mx_new("download_manager_cache_mx");
	dm->filter_session = fsess;
	default_cache_dir = NULL;
	gf_mx_p( dm->cache_mx );

#ifdef GPAC_HAS_HTTP2
	dm->disable_http2 = gf_opts_get_bool("core", "no-h2");
#endif

	opt = gf_opts_get_key("core", "cache");

retry_cache:
	if (!opt) {
		default_cache_dir = gf_get_default_cache_directory();
		opt = default_cache_dir;
	}
	if (opt[strlen(opt)-1] != GF_PATH_SEPARATOR) {
		dm->cache_directory = (char *) gf_malloc(sizeof(char)* (strlen(opt)+2));
		sprintf(dm->cache_directory, "%s%c", opt, GF_PATH_SEPARATOR);
	} else {
		dm->cache_directory = gf_strdup(opt);
	}

	//check cache exists
	if (!default_cache_dir) {
		FILE *test;
		char szTemp[GF_MAX_PATH];
		strcpy(szTemp, dm->cache_directory);
		strcat(szTemp, "gpaccache.test");
		test = gf_fopen(szTemp, "wb");
		if (!test) {
			gf_mkdir(dm->cache_directory);
			test = gf_fopen(szTemp, "wb");
			if (!test) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[Cache] Cannot write to %s directory, using system temp cache\n", dm->cache_directory ));
				gf_free(dm->cache_directory);
				dm->cache_directory = NULL;
				opt = NULL;
				goto retry_cache;
			}
		}
		if (test) {
			gf_fclose(test);
			gf_file_delete(szTemp);
		}
	}

	/*use it in in BYTES per second*/
	dm->limit_data_rate = gf_opts_get_int("core", "maxrate") / 8;

	dm->read_buf_size = GF_DOWNLOAD_BUFFER_SIZE;
	//when rate is limited, use smaller smaller read size
	if (dm->limit_data_rate) {
		dm->read_buf_size = GF_DOWNLOAD_BUFFER_SIZE_LIMIT_RATE;
	}

	dm->disable_cache = gf_opts_get_bool("core", "no-cache");

	dm->allow_offline_cache = gf_opts_get_bool("core", "offline-cache");

	dm->clean_cache = GF_FALSE;
	dm->allow_broken_certificate = GF_FALSE;
	if ( gf_opts_get_bool("core", "clean-cache")) {
		dm->clean_cache = GF_TRUE;
		dm->max_cache_size=0;
		gf_dm_clean_cache(dm);
	} else {
		dm->max_cache_size = gf_opts_get_int("core", "cache-size");
		if (dm->max_cache_size) {
			gf_dm_clean_cache(dm);
		}
	}
	dm->allow_broken_certificate = gf_opts_get_bool("core", "broken-cert");

	gf_mx_v( dm->cache_mx );

#ifdef GPAC_HAS_SSL
	dm->ssl_ctx = NULL;
#endif
	/* TODO: Not ready for now, we should find a locking strategy between several GPAC instances...
	 * gf_cache_cleanup_cache(dm);
	 */
	return dm;
}

GF_EXPORT
void gf_dm_set_auth_callback(GF_DownloadManager *dm, gf_dm_get_usr_pass get_user_password, void *usr_cbk)
{
	if (dm) {
		dm->get_user_password = get_user_password;
		dm->usr_cbk = usr_cbk;
	}
}

GF_EXPORT
void gf_dm_del(GF_DownloadManager *dm)
{
	if (!dm)
		return;
	assert( dm->sessions);
	assert( dm->cache_mx );
	gf_mx_p( dm->cache_mx );

	while (gf_list_count(dm->partial_downloads)) {
		GF_PartialDownload * entry = (GF_PartialDownload*)gf_list_get( dm->partial_downloads, 0);
		gf_list_rem( dm->partial_downloads, 0);
		assert( entry->filename );
		gf_file_delete( entry->filename );
		gf_free(entry->filename );
		entry->filename = NULL;
		entry->url = NULL;
		gf_free( entry );
	}

	/*destroy all pending sessions*/
	while (gf_list_count(dm->sessions)) {
		GF_DownloadSession *sess = (GF_DownloadSession *) gf_list_get(dm->sessions, 0);
		gf_dm_sess_del(sess);
	}
	gf_list_del(dm->sessions);
	dm->sessions = NULL;
	assert( dm->skip_proxy_servers );
	while (gf_list_count(dm->skip_proxy_servers)) {
		char *serv = (char*)gf_list_get(dm->skip_proxy_servers, 0);
		gf_list_rem(dm->skip_proxy_servers, 0);
		gf_free(serv);
	}
	gf_list_del(dm->skip_proxy_servers);
	dm->skip_proxy_servers = NULL;
	assert( dm->credentials);
	while (gf_list_count(dm->credentials)) {
		GF_UserCredentials * cred = (GF_UserCredentials*)gf_list_get( dm->credentials, 0);
		gf_list_rem( dm->credentials, 0);
		gf_free( cred );
	}
	gf_list_del( dm->credentials);
	dm->credentials = NULL;
	assert( dm->cache_entries );
	{
		/* Deletes DownloadedCacheEntry and associated files if required */
		Bool delete_my_files = gf_dm_needs_to_delete_cache(dm);
		while (gf_list_count(dm->cache_entries)) {
			const DownloadedCacheEntry entry = (const DownloadedCacheEntry)gf_list_get( dm->cache_entries, 0);
			gf_list_rem( dm->cache_entries, 0);
			if (delete_my_files)
				gf_cache_entry_set_delete_files_when_deleted(entry);
			gf_cache_delete_entry(entry);
		}
		gf_list_del( dm->cache_entries );
		dm->cache_entries = NULL;
	}

	gf_list_del( dm->partial_downloads );
	dm->partial_downloads = NULL;
	/* TODO: Not ready for now, we should find a locking strategy between several GPAC instances...
	* gf_cache_cleanup_cache(dm);
	*/
	if (dm->cache_directory)
		gf_free(dm->cache_directory);
	dm->cache_directory = NULL;

#ifdef GPAC_HAS_SSL
	if (dm->ssl_ctx) SSL_CTX_free(dm->ssl_ctx);
#endif
	/* Stored elsewhere, no need to free */
	gf_mx_v( dm->cache_mx );
	gf_mx_del( dm->cache_mx);
	dm->cache_mx = NULL;
	gf_free(dm);
}

/*!
 * Skip ICY metadata from SHOUTCAST or ICECAST streams.
 * Data will be skipped and parsed and sent as a GF_NETIO_Parameter to the user_io,
 * so modules interrested by those streams may use the data
\param sess The GF_DownloadSession
\param data last data received
\param nbBytes The number of bytes contained into data
 */
static void gf_icy_skip_data(GF_DownloadSession * sess, const char * data, u32 nbBytes)
{
	u32 icy_metaint;
	if (!sess || !data ) return;

	icy_metaint = sess->icy_metaint;
	assert( icy_metaint > 0 );
	while (nbBytes) {
		if (sess->icy_bytes == icy_metaint) {
			sess->icy_count = 1 + 16* (u8) data[0];
			/*skip icy metadata*/
			if (sess->icy_count > nbBytes) {
				sess->icy_count -= nbBytes;
				nbBytes = 0;
			} else {
				if (sess->icy_count > 1) {
					GF_NETIO_Parameter par;
					char szData[4096];
					memset(szData, 0, 4096);
					memcpy(szData, data+1, sess->icy_count-1);
					szData[sess->icy_count] = 0;

					par.error = GF_OK;
					par.msg_type = GF_NETIO_PARSE_HEADER;
					par.name = "icy-meta";
					par.value = szData;
					par.sess = sess;
					GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[ICY] Found metainfo in stream=%s, (every %d bytes)\n", szData, icy_metaint));
					gf_dm_sess_user_io(sess, &par);
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[ICY] Empty metainfo in stream, (every %d bytes)\n", icy_metaint));
				}
				nbBytes -= sess->icy_count;
				data += sess->icy_count;
				sess->icy_count = 0;
				sess->icy_bytes = 0;
			}
		} else {
			GF_NETIO_Parameter par;
			u32 left = icy_metaint - sess->icy_bytes;
			if (left > nbBytes) {
				left = nbBytes;
				sess->icy_bytes += left;
				nbBytes = 0;
			} else {
				sess->icy_bytes = icy_metaint;
				nbBytes -= left;
			}

			par.msg_type = GF_NETIO_DATA_EXCHANGE;
			par.data = data;
			par.size = left;
			gf_dm_sess_user_io(sess, &par);

			data += left;
		}
	}
}


static char *gf_dm_get_chunk_data(GF_DownloadSession *sess, Bool first_chunk_in_payload, char *body_start, u32 *payload_size, u32 *header_size)
{
	u32 size;
	s32 res;
	char *te_header, *sep;

	if (!sess || !body_start) return NULL;
	if (!sess->chunked) return body_start;

	if (sess->nb_left_in_chunk) {
		if (sess->nb_left_in_chunk > *payload_size) {
			sess->nb_left_in_chunk -= (*payload_size);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] Chunk encoding: still %d bytes to get\n", sess->nb_left_in_chunk));
		} else {
			*payload_size = sess->nb_left_in_chunk;
			sess->nb_left_in_chunk = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] Chunk encoding: last bytes in chunk received\n"));
		}
		*header_size = 0;
		return body_start;
	}

	if (*payload_size == 2) {
		*header_size = 0;
	}
	*header_size = 0;
	/*skip remaining CRLF from previous chunk if any*/
	if (*payload_size >= 2) {
		if ((body_start[0]=='\r') && (body_start[1]=='\n')) {
			body_start += 2;
			*header_size = 2;
		}
		if (*payload_size <= 4) {
			*header_size = 0;
			return NULL;
		}
		te_header = strstr((char *) body_start, "\r\n");
	} else {
		//not enough bytes to read CRLF, don't bother parsing
		te_header = NULL;
	}

	//cannot parse now, copy over the bytes
	if (!te_header) {
		*header_size = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] Chunk encoding: current buffer does not contain enough bytes (%d) to read the size\n", *payload_size));
		return NULL;
	}

	te_header[0] = 0;
	//assert(strlen(body_start));
	*header_size += (u32) (strlen(body_start)) + 2;

	sep = strchr(body_start, ';');
	if (sep) sep[0] = 0;
	res = sscanf(body_start, "%x", &size);
	if (res<0) {
		te_header[0] = '\r';
		if (sep) sep[0] = ';';
		*header_size = 0;
		*payload_size = 0;
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP] Chunk encoding: fail to read chunk size from buffer %s, aborting\n", body_start));
		return NULL;
	}
	if (sep) sep[0] = ';';
	*payload_size = size;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] Chunk Start: Header \"%s\" - header size %d - payload size %d (bytes done %d) at UTC "LLD"\n", body_start, 2+strlen(body_start), size, sess->bytes_done, gf_net_get_utc()));

	te_header[0] = '\r';
	if (!size)
		sess->last_chunk_found = GF_TRUE;

	sess->current_chunk_size = size;
	sess->current_chunk_start = gf_sys_clock_high_res();
	return te_header+2;
}


static void dm_sess_update_download_rate(GF_DownloadSession * sess)
{
	if (!sess->bytes_done) {
		sess->bytes_per_sec = 0;
		return;
	}

	//session is chunked and we have reached our first full window
	if (sess->chunked && sess->cumulated_chunk_rate) {
		/*use our cumulated weighted rate in bytes per seconds, and divide by total size*/
		sess->bytes_per_sec = (u32) (sess->cumulated_chunk_rate / (sess->bytes_done + sess->cumulated_chunk_header_bytes) );

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_HTTP, GF_LOG_DEBUG)) {
			u64 runtime = (gf_sys_clock_high_res() - sess->request_start_time);
			if (!runtime) runtime=1;
			u32 kbps = (u32) ((1000000 * (u64) (sess->bytes_done + sess->cumulated_chunk_header_bytes)) / runtime) / 125;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] bandwidth estimation: download time "LLD" us - bytes %u - chunk rate %u kbps (overall rate rate %u kbps)\n", runtime, sess->bytes_done, sess->bytes_per_sec / 125, kbps));
		}
#endif
	} else {
		/*compute bps starting from request send time*/
		u64 runtime = (gf_sys_clock_high_res() - sess->request_start_time);
		if (!runtime) runtime=1;

		sess->bytes_per_sec = (u32) ((1000000 * (u64) sess->bytes_done) / runtime);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] bandwidth estimation: download time "LLD" us - bytes %u - rate %u kbps\n", runtime, sess->bytes_done, sess->bytes_per_sec / 125));
	}
}


static void gf_dm_data_received(GF_DownloadSession *sess, u8 *payload, u32 payload_size, Bool store_in_init, u32 *rewrite_size, u8 *original_payload)
{
	u32 nbBytes, remaining, hdr_size;
	u8 *data;
	Bool first_chunk_in_payload = GF_TRUE;
	Bool flush_chunk = GF_FALSE;
	GF_NETIO_Parameter par;

	nbBytes = payload_size;
	hdr_size = 0;
	remaining = 0;
	if (!payload)
		return; //nothing to do
	if (sess->chunked) {
 		data = (u8 *) gf_dm_get_chunk_data(sess, first_chunk_in_payload, (char *) payload, &nbBytes, &hdr_size);
		if (!hdr_size && !data && nbBytes) {
			/* keep the data and wait for the rest */
			sess->remaining_data_size = nbBytes;
			sess->remaining_data = (char *)gf_realloc(sess->remaining_data, nbBytes * sizeof(char));
			memcpy(sess->remaining_data, payload, nbBytes);
			payload_size = 0;
			payload = NULL;
		} else if (hdr_size + nbBytes > payload_size) {
			/* chunk header is processed but we will need several TCP frames to get the entire chunk*/
			remaining = nbBytes + hdr_size - payload_size;
			assert(payload_size >= hdr_size);
			nbBytes = payload_size - hdr_size;
			payload_size = 0;
			payload = NULL;
			sess->chunk_header_bytes += hdr_size;
		} else {
			payload_size -= hdr_size + nbBytes;
			payload += hdr_size + nbBytes;
			flush_chunk = GF_TRUE;
			sess->chunk_header_bytes += hdr_size;
		}

		/*chunk transfer is done*/
		if (sess->last_chunk_found) {
			sess->total_size = sess->bytes_done;
		}
	} else {
		data = payload;
		remaining = payload_size = 0;
	}

	if (data && nbBytes && store_in_init) {
		sess->init_data = (char *) gf_realloc(sess->init_data , sizeof(char) * (sess->init_data_size + nbBytes) );
		memcpy(sess->init_data+sess->init_data_size, data, nbBytes);
		sess->init_data_size += nbBytes;
	}

	//we have some new bytes received
	if (nbBytes && !sess->remaining_data_size) {
		sess->bytes_done += nbBytes;
		dm_sess_update_download_rate(sess);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] url %s received %d new bytes (%d kbps)\n", sess->orig_url, nbBytes, 8*sess->bytes_per_sec/1000));
		if (sess->total_size && (sess->bytes_done > sess->total_size)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTP] url %s received more bytes than planned!! Got %d bytes vs %d content length\n", sess->orig_url, sess->bytes_done , sess->total_size ));
			sess->bytes_done = sess->total_size;
		}

		if (sess->icy_metaint > 0)
			gf_icy_skip_data(sess, (char *) data, nbBytes);
		else {
			if (sess->use_cache_file)
				gf_cache_write_to_cache( sess->cache_entry, sess, (char *) data, nbBytes, sess->dm ? sess->dm->cache_mx : NULL);

			par.msg_type = GF_NETIO_DATA_EXCHANGE;
			par.error = GF_OK;
			par.data = (char *) data;
			par.size = nbBytes;
			par.reply = flush_chunk;
			gf_dm_sess_user_io(sess, &par);
		}
	}
	//and we're done
	if (sess->total_size && (sess->bytes_done == sess->total_size)) {
		u64 run_time;

#if 0 //def GPAC_HAS_HTTP2
		if (0 && sess->h2_sess && sess->h2_stream_id)
			return;
#endif

		if (sess->use_cache_file) {
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_TRUE);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP,
			       ("[CACHE] url %s saved as %s\n", gf_cache_get_url(sess->cache_entry), gf_cache_get_cache_filename(sess->cache_entry)));
		}

		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		par.msg_type = GF_NETIO_DATA_TRANSFERED;
		par.error = GF_OK;

		gf_dm_sess_user_io(sess, &par);
		sess->total_time_since_req = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
		run_time = gf_sys_clock_high_res() - sess->start_time;

		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] %s (%d bytes) downloaded in "LLU" us (%d kbps) (%d us since request - got response in %d us)\n", gf_file_basename(gf_cache_get_url(sess->cache_entry)), sess->bytes_done,
		                                     run_time, 8*sess->bytes_per_sec/1000, sess->total_time_since_req, sess->reply_time));

		if (sess->chunked && (payload_size==2))
			payload_size=0;
	}

	if (rewrite_size && sess->chunked && data) {
		if (original_payload) {
			//use memmove since regions overlap
			memmove(original_payload + *rewrite_size, data, nbBytes);
		}
		*rewrite_size += nbBytes;
	}

	if (!sess->nb_left_in_chunk && remaining) {
		sess->nb_left_in_chunk = remaining;
	} else if (payload_size) {
		gf_dm_data_received(sess, payload, payload_size, store_in_init, rewrite_size, original_payload);
	}
}

static Bool dm_exceeds_cap_rate(GF_DownloadManager * dm)
{
	u32 cumul_rate = 0;
//	u32 nb_sess = 0;
	u64 now = gf_sys_clock_high_res();
	u32 i, count = gf_list_count(dm->sessions);

	//check if this fits with all other sessions
	for (i=0; i<count; i++) {
		GF_DownloadSession * sess = (GF_DownloadSession*)gf_list_get(dm->sessions, i);

		//session not running done
		if (sess->status != GF_NETIO_DATA_EXCHANGE) continue;

		//compute average rate on a window of 200 ms
		//we cannot just use sess->bytes_per_sec because the rate limit might be changed dynamically
		//so we need a recent history, not the session history
		//note that we don't try to use the estimated bps of chunk transfer when capping
		if (!sess->last_cap_rate_time) {
			u64 runtime;
			sess->last_cap_rate_time = sess->request_start_time;
			sess->last_cap_rate_bytes = sess->bytes_done;

			/*compute bps starting from request send time, do not call update_download_rate as we don't want the chunk transfer rate*/
			runtime = (gf_sys_clock_high_res() - sess->request_start_time);
			if (!runtime) runtime=1;
			sess->last_cap_rate_bytes_per_sec = (u32) ((1000000 * (u64) sess->bytes_done) / runtime);
		} else if (now > sess->last_cap_rate_time) {
			u64 time = now - sess->last_cap_rate_time;
			u64 bytes = sess->bytes_done - sess->last_cap_rate_bytes;
			sess->last_cap_rate_bytes_per_sec = (u32) ((1000000 * (u64) bytes) / time);
			if (time > 200000) {
				//this is an approximation we don't know precisely when these were received
				//and we don't really care since next rate estimation will be really high anyway and will exceed cap
				sess->last_cap_rate_bytes = sess->bytes_done;
				sess->last_cap_rate_time = now;
			}
		} else {
			return GF_TRUE;
		}
		cumul_rate += sess->last_cap_rate_bytes_per_sec;
		//nb_sess ++;
	}
	if ( cumul_rate >= dm->limit_data_rate)
		return GF_TRUE;

	return GF_FALSE;
}

static void gf_dm_sess_estimate_chunk_rate(GF_DownloadSession *sess, u32 nb_bytes)
{
	u64 now = gf_sys_clock_high_res();
	sess->chunk_bytes += nb_bytes;
	if ((now > sess->last_chunk_start_time + sess->chunk_wnd_dur) || (sess->total_size==sess->bytes_done) ) {
		if (sess->chunk_bytes) {
			u32 tot_bytes = sess->chunk_bytes + sess->chunk_header_bytes;
			//compute rate in bytes per seconds
			Double rate = 1000000.0 * tot_bytes;
			rate /= (now - sess->last_chunk_start_time);

			//cumulated rate is the weighted sum of our probe rates, the weight being the number of bytes
			//when comuting the bitrate, we will divide by the total size
			sess->cumulated_chunk_rate += rate * tot_bytes;

			sess->chunk_bytes = 0;
			sess->cumulated_chunk_header_bytes += sess->chunk_header_bytes;
			sess->chunk_header_bytes = 0;

			//we are done, update rate
			if (sess->total_size==sess->bytes_done)
				dm_sess_update_download_rate(sess);
		}
		sess->last_chunk_start_time = now;
	}
}

const u8 *gf_cache_get_content(const DownloadedCacheEntry entry, u32 *size);
void gf_cache_release_content(const DownloadedCacheEntry entry);

GF_EXPORT
GF_Err gf_dm_sess_fetch_data(GF_DownloadSession *sess, char *buffer, u32 buffer_size, u32 *read_size)
{
	u32 size;
	GF_Err e;

	if (!buffer || !buffer_size) {
		if (sess->put_state) {
			sess->put_state = 2;
			sess->status = GF_NETIO_WAIT_FOR_REPLY;
			return GF_OK;
		}
		return GF_BAD_PARAM;
	}
	if (sess->th)
		return GF_BAD_PARAM;
	if (sess->status == GF_NETIO_DISCONNECTED) {
		if (!sess->init_data_size)
			return GF_EOS;
	}
	else if (sess->status == GF_NETIO_STATE_ERROR) {
		return sess->last_error;
	}
	else if (sess->status > GF_NETIO_DATA_TRANSFERED)
		return GF_BAD_PARAM;

	*read_size = 0;
	if (sess->status == GF_NETIO_DATA_TRANSFERED) {
		if (!sess->server_mode)
			return GF_EOS;
		if (!sess->init_data_size && sess->total_size && (sess->total_size==sess->bytes_done))
			return GF_EOS;
		sess->status = GF_NETIO_DATA_EXCHANGE;
	}

	if (sess->status == GF_NETIO_SETUP) {
		gf_dm_connect(sess);
		if (sess->last_error)
			return sess->last_error;
		e = GF_OK;
	} else if (sess->status < GF_NETIO_DATA_EXCHANGE) {
		sess->do_requests(sess);
		e = sess->last_error;
	}
	/*we're running but we had data previously*/
	else if (sess->init_data) {
		e = GF_OK;
		if (sess->init_data_size<=buffer_size) {
			memcpy(buffer, sess->init_data, sizeof(char)*sess->init_data_size);
			*read_size = sess->init_data_size;
			gf_free(sess->init_data);
			sess->init_data = NULL;
			if (sess->init_data_size==sess->total_size)
				e = GF_EOS;
			sess->init_data_size = 0;
		} else {
			memcpy(buffer, sess->init_data, sizeof(char)*buffer_size);
			*read_size = buffer_size;
			sess->init_data_size -= buffer_size;
			memmove(sess->init_data, sess->init_data+buffer_size, sizeof(char)*sess->init_data_size);
			e = GF_OK;
		}
    } else if (sess->local_cache_only) {
        u32 to_copy, data_size;
        const u8 *ptr;
        e = GF_OK;
        assert(sess->cache_entry);
        //always refresh total size
        sess->total_size = gf_cache_get_content_length(sess->cache_entry);

        ptr = gf_cache_get_content(sess->cache_entry, &data_size);
        if (!ptr) return GF_OUT_OF_MEM;

        if (sess->bytes_done >= data_size) {
            *read_size = 0;
			gf_cache_release_content(sess->cache_entry);
            if (gf_cache_is_done(sess->cache_entry)) {
                sess->status = GF_NETIO_DATA_TRANSFERED;
                SET_LAST_ERR(GF_OK)
                return GF_EOS;
            }
            return GF_IP_NETWORK_EMPTY;
        }
        to_copy = data_size - sess->bytes_done;
        if (to_copy > buffer_size) to_copy = buffer_size;

        memcpy(buffer, ptr + sess->bytes_done, to_copy);
        sess->bytes_done += to_copy;
        *read_size = to_copy;
        if (gf_cache_is_done(sess->cache_entry)) {
            sess->status = GF_NETIO_DATA_TRANSFERED;
			SET_LAST_ERR(GF_OK)
		} else {
			sess->total_size = 0;
		}
		gf_cache_release_content(sess->cache_entry);
	} else {

		if (sess->dm && sess->dm->limit_data_rate) {
			if (dm_exceeds_cap_rate(sess->dm))
				return GF_IP_NETWORK_EMPTY;

			if (buffer_size > sess->dm->read_buf_size)
				buffer_size = sess->dm->read_buf_size;
		}

		e = GF_OK;
		*read_size = 0;
		u32 nb_read = 0;
		//perform a loop, mostly for chunk-tranfer mode where a server may push a lot of small TCP frames,
		//we want to flush everything as fast as possible
		while (1) {
			u32 single_read = 0;

			if (sess->remaining_data && sess->remaining_data_size) {
				if (nb_read + sess->remaining_data_size >= buffer_size) {
					if (!nb_read) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP] No HTTP chunk header found for %d bytes, assuming broken chunk transfer and aborting\n", sess->remaining_data_size));
						return GF_NON_COMPLIANT_BITSTREAM;
					}
					break;
				}
				memcpy(buffer + nb_read, sess->remaining_data, sess->remaining_data_size);
			} else if (nb_read >= buffer_size) {
				break;
			}
			e = gf_dm_read_data(sess, buffer + nb_read + sess->remaining_data_size, buffer_size - sess->remaining_data_size - nb_read, &single_read);
			if (e<0) {
				assert(single_read==0);
				break;
			}

			size = sess->remaining_data_size + single_read;
			sess->remaining_data_size = 0;
			single_read = 0;

#ifdef GPAC_HAS_HTTP2
			if (!sess->h2_sess)
#endif
				gf_dm_data_received(sess, (u8 *) buffer + nb_read, size, GF_FALSE, &single_read, buffer + nb_read);


			if (!sess->chunked)
				single_read = size;

			nb_read += single_read;
		}

#ifdef GPAC_HAS_HTTP2
		if (sess->h2_sess) {
			nb_read = 0;
			h2_flush_data_ex(sess, buffer, buffer_size, &nb_read);
			h2_session_send(sess);

			//stream is over and all data flushed, move to GF_NETIO_DATA_TRANSFERED in client mode
			if (!sess->h2_stream_id && sess->h2_data_done && !sess->h2_buf.size && !sess->server_mode) {
				sess->status = GF_NETIO_DATA_TRANSFERED;
				SET_LAST_ERR(GF_OK)
			}
		}
#endif

		*read_size = nb_read;
		//we had data but last call to gf_dm_read_data may have returned network empty
		if (nb_read && (e<0))
			e = GF_OK;


		//estimate rate for chunk-transfer - we only do that for fetch_data
		if (sess->chunked
#ifdef GPAC_HAS_HTTP2
			|| sess->h2_sess
#endif
		)
			gf_dm_sess_estimate_chunk_rate(sess, nb_read);

		if (! (*read_size) && (e==GF_IP_NETWORK_EMPTY)) {
#ifdef GPAC_HAS_HTTP2
			if (sess->h2_sess && !sess->total_size
				//for client, wait for close - for server move to data_transfered as soon as we're done pushing data
				&& ((!sess->h2_stream_id && sess->bytes_done) || (sess->h2_data_done && sess->server_mode))
			) {
				sess->status = GF_NETIO_DATA_TRANSFERED;
				SET_LAST_ERR(GF_OK)
				return GF_EOS;
			}
#endif

			e = gf_sk_probe(sess->sock);
			if ((e==GF_IP_CONNECTION_CLOSED)
				|| (sess->request_timeout && (gf_sys_clock_high_res() - sess->last_fetch_time > 1000 * sess->request_timeout))
			) {
				if (e==GF_IP_CONNECTION_CLOSED) {
					SET_LAST_ERR(GF_IP_CONNECTION_CLOSED)
					sess_connection_closed(sess);
				} else {
					SET_LAST_ERR(GF_IP_NETWORK_FAILURE)
				}
				sess->status = GF_NETIO_STATE_ERROR;
				return GF_IP_NETWORK_EMPTY;
			}
		}
	}

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess && sess->h2_buf.size) {
		return e;
	}
#endif

	if (sess->server_mode && (sess->status == GF_NETIO_DATA_EXCHANGE)) {
		sess->status = GF_NETIO_DATA_TRANSFERED;
		SET_LAST_ERR(GF_OK)
	}

	return e;
}

GF_EXPORT
GF_Err gf_dm_sess_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u64 *total_size, u64 *bytes_done, u32 *bytes_per_sec, GF_NetIOStatus *net_status)
{
	if (!sess)
		return GF_BAD_PARAM;
	if (server) *server = sess->server_name;
	if (path) *path = sess->remote_path;
	if (total_size) {
		if (sess->total_size==SIZE_IN_STREAM) *total_size  = 0;
		else *total_size = sess->total_size;
	}
	if (bytes_done) *bytes_done = sess->bytes_done;
	if (bytes_per_sec) {
		if (sess->dm && sess->dm->limit_data_rate && sess->last_cap_rate_bytes_per_sec) {
			*bytes_per_sec = sess->last_cap_rate_bytes_per_sec;
		} else {
			*bytes_per_sec = sess->bytes_per_sec;
		}
	}

	if (net_status) *net_status = sess->status;

	if (sess->status == GF_NETIO_DISCONNECTED) {
		if (sess->last_error) return sess->last_error;
		return GF_EOS;
	}
	else if (sess->status == GF_NETIO_STATE_ERROR)
		return sess->last_error ? sess->last_error : GF_SERVICE_ERROR;
	return GF_OK;
}

GF_EXPORT
u64 gf_dm_sess_get_utc_start(GF_DownloadSession * sess)
{
	if (!sess) return 0;
	return sess->start_time_utc;
}

GF_EXPORT
const char *gf_dm_sess_get_cache_name(GF_DownloadSession * sess)
{
	if (!sess) return NULL;
	if (! sess->cache_entry || sess->needs_cache_reconfig) return NULL;
	if (!sess->use_cache_file) return NULL;
	return gf_cache_get_cache_filename(sess->cache_entry);
}

#if 0 //unused
/*!
 * Tells whether session can be cached on disk.
 * Typically, when request has no content length, it deserves being streamed an cannot be cached
 * (ICY or MPEG-streamed content
\param sess The session
\param True if a cache can be created
 */
Bool gf_dm_sess_can_be_cached_on_disk(const GF_DownloadSession *sess)
{
	if (!sess) return GF_FALSE;
	return gf_cache_get_content_length(sess->cache_entry) != 0;
}
#endif

GF_EXPORT
void gf_dm_sess_abort(GF_DownloadSession * sess)
{
	if (sess) {
		gf_mx_p(sess->mx);

#ifdef GPAC_HAS_HTTP2
		if (sess->h2_sess && (sess->status==GF_NETIO_DATA_EXCHANGE)) {
			nghttp2_submit_rst_stream(sess->h2_sess->ng_sess, NGHTTP2_FLAG_NONE, sess->h2_stream_id, NGHTTP2_NO_ERROR);
			h2_session_send(sess);
		}
#endif
		gf_dm_disconnect(sess, HTTP_CLOSE);
		//not an error, move to DISCONNECTED state
		sess->status = GF_NETIO_DISCONNECTED;
		SET_LAST_ERR(GF_IP_CONNECTION_CLOSED)
		gf_mx_v(sess->mx);
	}
}

/*!
 * Sends the HTTP headers
\param sess The GF_DownloadSession
\param sHTTP buffer containing the request
\return GF_OK if everything went fine, the error otherwise
 */
static GF_Err http_send_headers(GF_DownloadSession *sess, char * sHTTP) {
	GF_Err e;
	GF_NETIO_Parameter par;
	Bool no_cache = GF_FALSE;
	char range_buf[1024];
	char pass_buf[1124];
	char req_name[20];
	const char *user_agent;
	const char *url;
	const char *user_profile;
	const char *param_string;
	Bool inject_icy = GF_FALSE;
	u32 i, count;
	GF_HTTPHeader *hdr;
	Bool has_accept, has_connection, has_range, has_agent, has_language, send_profile, has_mime, has_chunk_transfer;
	assert (sess->status == GF_NETIO_CONNECTED);

	gf_dm_sess_clear_headers(sess);
	assert(sess->remaining_data_size == 0);

	if (sess->needs_cache_reconfig) {
		gf_dm_configure_cache(sess);
		sess->needs_cache_reconfig = 0;
	}
	if (sess->from_cache_only) {
		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = 0;
		sess->status = GF_NETIO_WAIT_FOR_REPLY;
		gf_dm_sess_notify_state(sess, GF_NETIO_WAIT_FOR_REPLY, GF_OK);
		return GF_OK;
	}

	//in case we got disconnected, reconnect
	e = gf_sk_probe(sess->sock);
	if (e && (e!=GF_IP_NETWORK_EMPTY)) {
		sess_connection_closed(sess);
		if ((e==GF_IP_CONNECTION_CLOSED) && sess->num_retry) {
			sess->num_retry--;
			sess->status = GF_NETIO_SETUP;
			return GF_OK;
		}
		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR(e)
		gf_dm_sess_notify_state(sess, GF_NETIO_STATE_ERROR, e);
		return e;
	}

	/*setup authentification*/
	strcpy(pass_buf, "");
	sess->creds = gf_user_credentials_find_for_site( sess->dm, sess->server_name, NULL);
	if (sess->creds && sess->creds->valid) {
		sprintf(pass_buf, "Basic %s", sess->creds->digest);
	}

	user_agent = gf_opts_get_key("core", "ua");
	if (!user_agent) user_agent = GF_DOWNLOAD_AGENT_NAME;

	sess->put_state = 0;

	par.error = GF_OK;
	par.msg_type = GF_NETIO_GET_METHOD;
	par.name = NULL;
	gf_dm_sess_user_io(sess, &par);
	if (!par.name || sess->server_only_understand_get) {
		par.name = "GET";
	}

	strncpy(req_name, par.name, 19);
	req_name[19] = 0;

	if (!strcmp(req_name, "GET")) {
		sess->http_read_type = GET;
#ifdef GPAC_HAS_HTTP2
		if (!sess->h2_sess)
#endif
			inject_icy = GF_TRUE;
	} else if (!strcmp(req_name, "HEAD")) sess->http_read_type = HEAD;
	else sess->http_read_type = OTHER;

	if (!strcmp(req_name, "PUT") || !strcmp(req_name, "POST"))
		sess->put_state = 1;

	url = (sess->proxy_enabled==1) ? sess->orig_url : sess->remote_path;

	/*get all headers*/
	gf_dm_sess_clear_headers(sess);


#define PUSH_HDR(_name, _value) {\
		GF_SAFEALLOC(hdr, GF_HTTPHeader)\
		hdr->name = gf_strdup(_name);\
		hdr->value = gf_strdup(_value);\
		gf_list_add(sess->headers, hdr);\
		}

	has_agent = has_accept = has_connection = has_range = has_language = has_mime = has_chunk_transfer = GF_FALSE;
	while (1) {
		par.msg_type = GF_NETIO_GET_HEADER;
		par.value = NULL;
		gf_dm_sess_user_io(sess, &par);
		if (!par.value) break;

		if (!stricmp(par.name, "Connection")) {
			if (!stricmp(par.value, "close"))
				has_connection = GF_TRUE;
			else
				continue;
		}
		else if (!stricmp(par.name, "Transfer-Encoding")) {
			if (!stricmp(par.value, "chunked"))
				has_chunk_transfer = GF_TRUE;
			continue;
		}

		PUSH_HDR(par.name, par.value)

		if (!stricmp(par.name, "Accept")) has_accept = GF_TRUE;
		else if (!stricmp(par.name, "Range")) has_range = GF_TRUE;
		else if (!stricmp(par.name, "User-Agent")) has_agent = GF_TRUE;
		else if (!stricmp(par.name, "Accept-Language")) has_language = GF_TRUE;
		else if (!stricmp(par.name, "Content-Type")) has_mime = GF_TRUE;

		if (!par.msg_type) break;
	}
	if (!has_agent) PUSH_HDR("User-Agent", user_agent)

	/*no mime and POST/PUT, default to octet stream*/
	if (!has_mime && (sess->http_read_type==OTHER)) PUSH_HDR("Content-Type", "application/octet-stream")

	if (!has_accept && (sess->http_read_type!=OTHER) ) PUSH_HDR("Accept", "*/*")

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess)
		has_connection = GF_TRUE;

	if (!has_connection && !sess->h2_sess && !sess->h2_upgrade_state
#ifdef GPAC_HAS_SSL
		&& !sess->ssl
#endif
		&& !sess->dm->disable_http2 && (sess->h2_upgrade_state!=2)
		&& !gf_opts_get_bool("core", "no-h2c")
	) {
		u8 settings[HTTP2_BUFFER_SETTINGS_SIZE];
		u32 settings_len;
		u8 b64[100];
		u32 b64len;

		nghttp2_settings_entry h2_settings[2] = {
			{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
			{NGHTTP2_SETTINGS_ENABLE_PUSH, 0}
		};

		PUSH_HDR("Connection", "Upgrade, HTTP2-Settings")
		PUSH_HDR("Upgrade", "h2c")


		settings_len = (u32) nghttp2_pack_settings_payload(settings, HTTP2_BUFFER_SETTINGS_SIZE, h2_settings, GF_ARRAY_LENGTH(h2_settings));
		b64len = gf_base64_encode(settings, settings_len, b64, 100);
		b64[b64len] = 0;
		PUSH_HDR("HTTP2-Settings", b64)

		sess->h2_upgrade_state = 1;
		inject_icy = GF_FALSE;
	} else
#endif
	if (sess->proxy_enabled==1) PUSH_HDR("Proxy-Connection", "Keep-alive")
	else if (!has_connection) {
		PUSH_HDR("Connection", "Keep-Alive");
	}


	if (has_chunk_transfer
#ifdef GPAC_HAS_HTTP2
		&& !sess->h2_sess
#endif
	) {
		PUSH_HDR("Transfer-Encoding", "chunked");
		sess->chunked = GF_TRUE;
	}

	if (!has_range && sess->needs_range) {
		if (!sess->range_end)
			sprintf(range_buf, "bytes="LLD"-", sess->range_start);
		//if end is set to -1 use open end
		else if (sess->range_end==(u64)-1)
			sprintf(range_buf, "bytes="LLD"-", sess->range_start);
		else
			sprintf(range_buf, "bytes="LLD"-"LLD"", sess->range_start, sess->range_end);
		PUSH_HDR("Range", range_buf)
		no_cache = GF_TRUE;
	}
	if (!has_language) {
		const char *opt = gf_opts_get_key("core", "lang");
		if (opt) PUSH_HDR("Accept-Language", opt)
	}


	if (strlen(pass_buf)) {
		PUSH_HDR("Authorization", pass_buf)
	}

	par.msg_type = GF_NETIO_GET_CONTENT;
	par.data = NULL;
	par.size = 0;

	/*check if we have personalization info*/
	send_profile = GF_FALSE;
	user_profile = gf_opts_get_key("core", "user-profileid");

	if (user_profile) {
		PUSH_HDR("X-UserProfileID", user_profile);
	} else if ((sess->http_read_type == GET) || (sess->http_read_type == HEAD) ) {
		user_profile = gf_opts_get_key("core", "user-profile");
		if (user_profile && gf_file_exists(user_profile)) {
			FILE *profile = gf_fopen(user_profile, "rb");
			if (profile) {
				par.size = (u32) gf_fsize(profile);
				gf_fclose(profile);
				sprintf(range_buf, "%d", par.size);
				PUSH_HDR("Content-Length", range_buf);
				PUSH_HDR("Content-Type", "text/xml");
				send_profile = GF_TRUE;
			}
		}
	}


	if (!send_profile) {
		gf_dm_sess_user_io(sess, &par);
		if (par.data && par.size) {
			sprintf(range_buf, "%d", par.size);
			PUSH_HDR("Content-Length", range_buf);
		} else {
			par.data = NULL;
			par.size = 0;
		}
	}

	if (inject_icy) {
		/* This will force the server to respond with Icy-Metaint */
		PUSH_HDR("Icy-Metadata", "1");
	}

	if (sess->http_read_type!=OTHER) {
		const char *etag=NULL, *last_modif=NULL;

		/*cached headers are not appended in POST*/
		if (!no_cache && !sess->disable_cache && (GF_OK < gf_cache_get_http_headers( sess->cache_entry, &etag, &last_modif)) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("Cache Entry : %p, FAILED to append cache directives.", sess->cache_entry));
		}

		if (etag) PUSH_HDR("If-None-Match", etag)
		if (last_modif) PUSH_HDR("If-Modified-Since", last_modif)
	}


	//done gathering headers

	param_string = gf_opts_get_key("core", "query-string");

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess) {
		Bool has_body = GF_FALSE;

		gf_mx_p(sess->mx);

		sess->h2_send_data = NULL;
		if (par.data && par.size) {
			has_body = GF_TRUE;
			sess->h2_send_data = (u8 *) par.data;
			sess->h2_send_data_len = par.size;
			sess->h2_is_eos = GF_TRUE;
		} else if (sess->put_state==1) {
			has_body = GF_TRUE;
		}

		e = h2_submit_request(sess, req_name, url, param_string, has_body);

		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
		if (!e)
			e = h2_session_send(sess);

		//in case we have a body already setup with this request
		h2_flush_send(sess, GF_FALSE);

		gf_mx_v(sess->mx);

		sess->h2_is_eos = GF_FALSE;
		goto req_sent;
	}
#endif // GPAC_HAS_HTTP2

	if (param_string) {
		if (strchr(sess->remote_path, '?')) {
			sprintf(sHTTP, "%s %s&%s HTTP/1.1\r\nHost: %s\r\n", req_name, url, param_string, sess->server_name);
		} else {
			sprintf(sHTTP, "%s %s?%s HTTP/1.1\r\nHost: %s\r\n", req_name, url, param_string, sess->server_name);
		}
	} else {
		sprintf(sHTTP, "%s %s HTTP/1.1\r\nHost: %s\r\n", req_name, url, sess->server_name);
	}

	//serialize headers
	count = gf_list_count(sess->headers);
	for (i=0; i<count; i++) {
		hdr = gf_list_get(sess->headers, i);
		strcat(sHTTP, hdr->name);
		strcat(sHTTP, ": ");
		strcat(sHTTP, hdr->value);
		strcat(sHTTP, "\r\n");
	}

	strcat(sHTTP, "\r\n");

	if (send_profile || par.data) {
		u32 len = (u32) strlen(sHTTP);
		char *tmp_buf = (char*)gf_malloc(sizeof(char)*(len+par.size+1));
		strcpy(tmp_buf, sHTTP);
		if (par.data) {
			memcpy(tmp_buf+len, par.data, par.size);
			tmp_buf[len+par.size] = 0;

			sess->put_state = 2;
		} else {
			FILE *profile;
			user_profile = gf_opts_get_key("core", "user-profile");
			assert (user_profile);
			profile = gf_fopen(user_profile, "rt");
			if (profile) {
				s32 read = (s32) gf_fread(tmp_buf+len, par.size, profile);
				if ((read<0) || (read< (s32) par.size)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP,
					       ("[HTTP] Error while loading UserProfile, size=%d, should be %d.", read, par.size));
					for (; read < (s32) par.size; read++) {
						tmp_buf[len + read] = 0;
					}
				}
				gf_fclose(profile);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTP] Error while loading Profile file %s.", user_profile));
			}
		}

		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = len+par.size;

		e = dm_sess_write(sess, tmp_buf, len+par.size);

		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Sending request at UTC "LLD" %s\n\n", gf_net_get_utc(), tmp_buf));
		gf_free(tmp_buf);
	} else {
		u32 len = (u32) strlen(sHTTP);

		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = len;

		e = dm_sess_write(sess, sHTTP, len);

#ifndef GPAC_DISABLE_LOG
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP] Error sending request %s\n", gf_error_to_string(e) ));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Sending request at UTC "LLU" %s\n\n", gf_net_get_utc(), sHTTP));
		}
#endif
	}


#ifdef GPAC_HAS_HTTP2
req_sent:
#endif
	gf_dm_sess_clear_headers(sess);

	if (e) {
		SET_LAST_ERR(e)
		sess->status = GF_NETIO_STATE_ERROR;
		gf_dm_sess_notify_state(sess, GF_NETIO_STATE_ERROR, e);
		return e;
	}

	gf_dm_sess_notify_state(sess, GF_NETIO_WAIT_FOR_REPLY, GF_OK);
	if (sess->put_state==1) {
		sess->status = GF_NETIO_DATA_TRANSFERED;
	} else {
		sess->status = GF_NETIO_WAIT_FOR_REPLY;
	}
	SET_LAST_ERR(GF_OK)
	return GF_OK;
}


/*!
 * Parse the remaining part of body
\param sess The session
\param sHTTP the data buffer
\return The error code if any
 */
static GF_Err http_parse_remaining_body(GF_DownloadSession * sess, char * sHTTP)
{
	GF_Err e;
	u32 buf_size = sess->dm ? sess->dm->read_buf_size : GF_DOWNLOAD_BUFFER_SIZE;

	while (1) {
		u32 prev_remaining_data_size, size=0, rewrite_size=0;
		if (sess->status>=GF_NETIO_DISCONNECTED)
			return GF_REMOTE_SERVICE_ERROR;

		if (sess->dm && sess->dm->limit_data_rate && sess->bytes_per_sec) {
			if (dm_exceeds_cap_rate(sess->dm)) {
				gf_sleep(1);
				return GF_OK;
			}
		}

		//the data remaining from the last buffer (i.e size for chunk that couldn't be read because the buffer does not contain enough bytes)
		if (sess->remaining_data && sess->remaining_data_size) {
			if (sess->remaining_data_size >= buf_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP] No HTTP chunk header found for %d bytes, assuming broken chunk transfer and aborting\n", sess->remaining_data_size));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			memcpy(sHTTP, sess->remaining_data, sess->remaining_data_size);
		}
		e = gf_dm_read_data(sess, sHTTP + sess->remaining_data_size, buf_size - sess->remaining_data_size, &size);
		if ((e != GF_IP_CONNECTION_CLOSED) && (!size || e == GF_IP_NETWORK_EMPTY)) {
			if (!sess->total_size && !sess->chunked && (gf_sys_clock_high_res() - sess->start_time > 5000000)) {
				sess->total_size = sess->bytes_done;
				gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
				assert(sess->server_name);
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP] Disconnected from %s: %s\n", sess->server_name, gf_error_to_string(e)));
				gf_dm_disconnect(sess, HTTP_NO_CLOSE);
			}
			return GF_OK;
		}

		if (e) {
			if (sess->sock && (e == GF_IP_CONNECTION_CLOSED)) {
				u32 len = gf_cache_get_content_length(sess->cache_entry);
				if (size > 0) {
#ifdef GPAC_HAS_HTTP2
					if (sess->h2_sess) {
						h2_flush_data(sess, GF_FALSE);
					} else
#endif
						gf_dm_data_received(sess, (u8 *) sHTTP, size, GF_FALSE, NULL, NULL);
				}

				if ( ( (len == 0) && sess->use_cache_file) || sess->bytes_done) {
					sess->total_size = sess->bytes_done;
					// HTTP 1.1 without content length...
					gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
					assert(sess->server_name);
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP] Disconnected from %s: %s\n", sess->server_name, gf_error_to_string(e)));
					if (sess->use_cache_file)
						gf_cache_set_content_length(sess->cache_entry, sess->bytes_done);
					e = GF_OK;
				}
			}
			gf_dm_disconnect(sess, HTTP_CLOSE);
			SET_LAST_ERR(e)
			gf_dm_sess_notify_state(sess, sess->status, e);
			return e;
		}

		prev_remaining_data_size = sess->remaining_data_size;
		sess->remaining_data_size = 0;

		sHTTP[size + prev_remaining_data_size] = 0;

#ifdef GPAC_HAS_HTTP2
		if (sess->h2_sess) {
			h2_flush_data(sess, GF_FALSE);
			if (!sess->h2_stream_id) {
				sess->status = GF_NETIO_DATA_TRANSFERED;
				SET_LAST_ERR(GF_OK)
			}
		} else
#endif
			gf_dm_data_received(sess, (u8 *) sHTTP, size + prev_remaining_data_size, GF_FALSE, &rewrite_size, NULL);

		if (sess->chunked)
			gf_dm_sess_estimate_chunk_rate(sess, rewrite_size);


		/*socket empty*/
		if (size < buf_size) {
			return GF_OK;
		}
	}
	return GF_OK;
}

static void notify_headers(GF_DownloadSession *sess, char * sHTTP, s32 bytesRead, s32 BodyStart)
{
	GF_NETIO_Parameter par;
	u32 i, count;

	count = gf_list_count(sess->headers);
	memset(&par, 0, sizeof(GF_NETIO_Parameter));

	for (i=0; i<count; i++) {
		GF_HTTPHeader *hdrp = (GF_HTTPHeader*)gf_list_get(sess->headers, i);
		par.name = hdrp->name;
		par.value = hdrp->value;

		par.error = GF_OK;
		par.msg_type = GF_NETIO_PARSE_HEADER;
		gf_dm_sess_user_io(sess, &par);
	}

	if (sHTTP) {
		sHTTP[bytesRead]=0;
		par.error = GF_OK;
		par.data = sHTTP + BodyStart;
		par.size = (u32) strlen(par.data);
		par.msg_type = GF_NETIO_DATA_EXCHANGE;
		gf_dm_sess_user_io(sess, &par);
	}
}

static u32 http_parse_method(const char *comp)
{
	if (!strcmp(comp, "GET")) return GF_HTTP_GET;
	else if (!strcmp(comp, "HEAD")) return GF_HTTP_HEAD;
	else if (!strcmp(comp, "OPTIONS")) return GF_HTTP_OPTIONS;
	else if (!strcmp(comp, "PUT")) return GF_HTTP_PUT;
	else if (!strcmp(comp, "POST")) return GF_HTTP_POST;
	else if (!strcmp(comp, "DELETE")) return GF_HTTP_DELETE;
	else if (!strcmp(comp, "CONNECT")) return GF_HTTP_CONNECT;
	else if (!strcmp(comp, "TRACE")) return GF_HTTP_TRACE;
	else return 0;
}

/*!
 * Waits for the response HEADERS, parse the information... and so on
\param sess The session
\param sHTTP the data buffer
 */
static GF_Err wait_for_header_and_parse(GF_DownloadSession *sess, char * sHTTP)
{
	GF_NETIO_Parameter par;
	s32 bytesRead, BodyStart;
	u32 res, i, buf_size;
	s32 LinePos, Pos;
	u32 method=0;
	u32 rsp_code=0, ContentLength, first_byte, last_byte, total_size, range, no_range;
	Bool connection_closed = GF_FALSE;
	Bool connection_keep_alive = GF_FALSE;
	u32 connection_timeout=0;
	char buf[1025];
	char comp[400];
	GF_Err e;
	char * new_location;
	const char * mime_type;
#ifdef GPAC_HAS_HTTP2
	Bool upgrade_to_http2 = GF_FALSE;
#endif

	if (sess->creds && sess->creds->req_state) {
		if (sess->creds->req_state==GF_CREDS_STATE_PENDING)
			return GF_OK;
		sess->creds->req_state = GF_CREDS_STATE_NONE;
		if (!sess->creds->valid) {
			gf_dm_disconnect(sess, HTTP_CLOSE);
			sess->status = GF_NETIO_STATE_ERROR;
			par.error = GF_AUTHENTICATION_FAILURE;
			par.msg_type = GF_NETIO_DISCONNECTED;
			gf_dm_sess_user_io(sess, &par);
			e = GF_AUTHENTICATION_FAILURE;
			SET_LAST_ERR(e)
			goto exit;
		}

		sess->status = GF_NETIO_SETUP;
		e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
		if (e) {
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(e)
			gf_dm_sess_notify_state(sess, sess->status, e);
		}
		return e;
	}


	if (sess->server_mode) {
		assert( sess->status == GF_NETIO_CONNECTED );
	} else {
		assert( sess->status == GF_NETIO_WAIT_FOR_REPLY );
		if (!(sess->flags & GF_NETIO_SESSION_NOT_CACHED)) {
			sess->use_cache_file = sess->dm->disable_cache ? GF_FALSE : GF_TRUE;
		}
	}
	bytesRead = res = 0;
	new_location = NULL;

	if (sess->from_cache_only) {
		sess->reply_time = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
		sess->rsp_hdr_size = 0;
		sess->total_size = sess->bytes_done = gf_cache_get_content_length(sess->cache_entry);

		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = GF_NETIO_DATA_TRANSFERED;
		par.error = GF_OK;
		gf_dm_sess_user_io(sess, &par);
		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		return GF_OK;
	}

	buf_size = sess->dm ? sess->dm->read_buf_size : GF_DOWNLOAD_BUFFER_SIZE;

	//always set start time to the time at last attempt reply parsing
	sess->start_time = gf_sys_clock_high_res();
	sess->start_time_utc = gf_net_get_utc();
	sess->chunked = GF_FALSE;
	sess->last_chunk_found = GF_FALSE;

	sess->last_chunk_start_time = sess->request_start_time;
	sess->chunk_bytes = 0;
	sess->cumulated_chunk_rate = 0;

	sHTTP[0] = 0;

	while (1) {
		Bool probe = (!bytesRead || (sess->flags & GF_NETIO_SESSION_NO_BLOCK) ) ? GF_TRUE : GF_FALSE;
		e = gf_dm_read_data(sess, sHTTP + bytesRead, buf_size - bytesRead, &res);

#ifdef GPAC_HAS_HTTP2
		/* break as soon as we have a header frame*/
		if (sess->h2_headers_seen) {
			sess->h2_headers_seen = 0;
			res = 0;
			bytesRead = 0;
			BodyStart = 0;
			e = GF_OK;
			SET_LAST_ERR(GF_OK)
			break;
		}
#endif

		switch (e) {
		case GF_IP_NETWORK_EMPTY:
			if (probe) {
				e = gf_sk_probe(sess->sock);

				if (e==GF_IP_CONNECTION_CLOSED) {
					SET_LAST_ERR(GF_IP_CONNECTION_CLOSED)
					sess_connection_closed(sess);
					sess->status = GF_NETIO_STATE_ERROR;
					return GF_IP_NETWORK_EMPTY;
				}
				if (!sess->server_mode
					&& sess->request_timeout
					&& (gf_sys_clock_high_res() - sess->request_start_time > 1000 * sess->request_timeout)
				) {
					SET_LAST_ERR(GF_IP_NETWORK_FAILURE)
					sess->status = GF_NETIO_STATE_ERROR;
					return GF_IP_NETWORK_FAILURE;
				}
				if (sess->status != GF_NETIO_STATE_ERROR) {
					if (sess->server_mode
						|| (sess->flags & GF_NETIO_SESSION_NO_BLOCK)
					) {
						SET_LAST_ERR(GF_IP_NETWORK_EMPTY)
					}
				}
				return GF_OK;
			}
			if (sess->status==GF_NETIO_STATE_ERROR)
				return sess->last_error;
			if (!res && sess->status<=GF_NETIO_CONNECTED)
				return GF_OK;

#ifdef GPAC_HAS_HTTP2
			//we may have received bytes (bytesRead>0) yet none for this session, return GF_IP_NETWORK_EMPTY if empty
			if (sess->h2_sess)
				return GF_IP_NETWORK_EMPTY;
#endif

			continue;
		/*socket has been closed while configuring, retry (not sure if the server got the GET)*/
		case GF_IP_CONNECTION_CLOSED:
			if (sess->http_read_type == HEAD) {
				/* Some servers such as shoutcast directly close connection if HEAD or an unknown method is issued */
				sess->server_only_understand_get = GF_TRUE;
			}
			if (sess->server_mode) {
				SET_LAST_ERR(GF_IP_CONNECTION_CLOSED)
				sess_connection_closed(sess);
				sess->status = GF_NETIO_DISCONNECTED;
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Connection closed by client\n", sess->remote_path));
				return GF_IP_CONNECTION_CLOSED;
			}
			gf_dm_disconnect(sess, HTTP_RESET_CONN);

			if (sess->num_retry) {
#ifdef GPAC_HAS_SSL
				if ((sess->num_retry < SESSION_RETRY_SSL)
					&& !(sess->flags & GF_DOWNLOAD_SESSION_USE_SSL)
					&& !gf_opts_get_bool("core", "no-tls-rcfg")
				) {
					GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Connection closed by server when processing %s - retrying using SSL\n", sess->remote_path));
					sess->flags |= GF_DOWNLOAD_SESSION_SSL_FORCED;
				} else
#endif
				{
					GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Connection closed by server when processing %s - retrying\n", sess->remote_path));
				}
				sess->status = GF_NETIO_SETUP;
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Connection closed by server when processing %s - aborting\n", sess->remote_path));
				SET_LAST_ERR(e)
				sess->status = GF_NETIO_STATE_ERROR;
			}
			return e;
		case GF_OK:
			SET_LAST_ERR(GF_OK)
			if (!res)
				return GF_OK;
			break;
		default:
			goto exit;
		}
		bytesRead += res;

#ifdef GPAC_HAS_HTTP2
		//in case we got a refused stream
		if (sess->status==GF_NETIO_SETUP) {
			return GF_OK;
		}
		if (sess->h2_sess)
			continue;
#endif

		//HTTP1.1 only

		char *hdr_buf = sHTTP;
		u32 hdr_buf_len = bytesRead;
		if (sess->flags & GF_NETIO_SESSION_NO_BLOCK) {
			sess->async_req_reply = gf_realloc(sess->async_req_reply, sess->async_req_reply_size+res+1);
			if (!sess->async_req_reply) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(GF_OUT_OF_MEM)
				return sess->last_error;
			}
			memcpy(sess->async_req_reply + sess->async_req_reply_size, sHTTP, res);
			sess->async_req_reply_size += res;
			bytesRead = 0;
			sHTTP[0] = 0;
			hdr_buf = sess->async_req_reply;
			hdr_buf_len = sess->async_req_reply_size;
		}

		//weird bug on some servers sending twice the last chunk
		if (!strncmp(hdr_buf, "0\r\n\r\n", 5) ) {
			if (bytesRead) {
				bytesRead -= res;
			} else {
				sess->async_req_reply_size -= res;
			}
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTP] End of chunk found while waiting server response when processing %s - retrying\n", sess->remote_path));
			continue;
		}

		/*locate body start*/
		BodyStart = gf_token_find(hdr_buf, 0, hdr_buf_len, "\r\n\r\n");
		if (BodyStart > 0) {
			BodyStart += 4;
			break;
		}
		BodyStart = gf_token_find(hdr_buf, 0, hdr_buf_len, "\n\n");
		if (BodyStart > 0) {
			BodyStart += 2;
			break;
		}
	}

	no_range = range = ContentLength = first_byte = last_byte = total_size = rsp_code = 0;

	if (sess->flags & GF_NETIO_SESSION_NO_BLOCK) {
		sHTTP = sess->async_req_reply;
		buf_size = bytesRead = sess->async_req_reply_size;
		sess->async_req_reply_size = 0;
	}

#ifdef GPAC_HAS_HTTP2
	if (!sess->h2_sess) {
#endif
		if (bytesRead < 0) {
			e = GF_REMOTE_SERVICE_ERROR;
			goto exit;
		}

		if (!BodyStart)
			BodyStart = bytesRead;

		if (BodyStart) sHTTP[BodyStart-1] = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] %s\n\n", sHTTP));

		sess->reply_time = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
		sess->rsp_hdr_size = BodyStart;

		LinePos = gf_token_get_line(sHTTP, 0, bytesRead, buf, 1024);
		Pos = gf_token_get(buf, 0, " \t\r\n", comp, 400);

		//TODO for HTTP2
		if (sess->server_mode) {
			method = http_parse_method(comp);

			Pos = gf_token_get(buf, Pos, " \t\r\n", comp, 400);
			if (sess->orig_url) gf_free(sess->orig_url);
			sess->orig_url = gf_strdup(comp);
			/*Pos = */gf_token_get(buf, Pos, " \t\r\n", comp, 400);
			if ((strncmp("HTTP", comp, 4) != 0)) {
				e = GF_REMOTE_SERVICE_ERROR;
				goto exit;
			}
			//flush potential body except for PUT/POST
			if ((method==GF_HTTP_PUT) || (method==GF_HTTP_POST))
				rsp_code = 200;
			else
				rsp_code = 300;
		} else {

			if (!strncmp("ICY", comp, 3)) {
				sess->use_cache_file = GF_FALSE;
				/*be prepared not to receive any mime type from ShoutCast servers*/
				if (!gf_cache_get_mime_type(sess->cache_entry))
					gf_cache_set_mime_type(sess->cache_entry, "audio/mpeg");
			} else if ((strncmp("HTTP", comp, 4) != 0)) {
				e = GF_REMOTE_SERVICE_ERROR;
				goto exit;
			}
			Pos = gf_token_get(buf, Pos, " ", comp, 400);
			if (Pos <= 0) {
				e = GF_REMOTE_SERVICE_ERROR;
				goto exit;
			}
			rsp_code = (u32) atoi(comp);
			/*Pos = */gf_token_get(buf, Pos, " \r\n", comp, 400);

		}

		/* parse headers*/
		while (1) {
			GF_HTTPHeader *hdrp;
			char *sep, *hdr_sep, *hdr, *hdr_val;
			if ( (s32) LinePos + 4 > BodyStart) break;
			LinePos = gf_token_get_line(sHTTP, LinePos , bytesRead, buf, 1024);
			if (LinePos < 0) break;

			hdr_sep = NULL;
			hdr_val = NULL;
			hdr = buf;
			sep = strchr(buf, ':');
			if (sep) {
				sep[0]=0;
				hdr_val = sep+1;
				while (hdr_val[0]==' ') hdr_val++;
				hdr_sep = strrchr(hdr_val, '\r');
				if (hdr_sep) hdr_sep[0] = 0;
			}

			GF_SAFEALLOC(hdrp, GF_HTTPHeader);
			if (hdrp) {
				hdrp->name = gf_strdup(hdr);
				hdrp->value = gf_strdup(hdr_val);
				gf_list_add(sess->headers, hdrp);
			}

			if (sep) sep[0]=':';
			if (hdr_sep) hdr_sep[0] = '\r';

			if (sess->server_mode) {
				if (!stricmp(hdrp->name, "Transfer-Encoding") && !stricmp(hdrp->value, "chunked"))
					sess->chunked = GF_TRUE;
			}
		}

#ifdef GPAC_HAS_HTTP2
	}
#endif

	if (!sess->server_mode) {
		Bool cache_no_store = GF_FALSE;
		//default pre-processing of headers - needs cleanup, not all of these have to be parsed before checking reply code
		for (i=0; i<gf_list_count(sess->headers); i++) {
			char *val;
			GF_HTTPHeader *hdr = (GF_HTTPHeader*)gf_list_get(sess->headers, i);

#ifdef GPAC_HAS_HTTP2
			if (!stricmp(hdr->name, ":status") ) {
				rsp_code = (u32) atoi(hdr->value);
			} else
#endif
			if (!stricmp(hdr->name, "Content-Length") ) {
				ContentLength = (u32) atoi(hdr->value);

				if ((rsp_code<300) && sess->cache_entry)
					gf_cache_set_content_length(sess->cache_entry, ContentLength);

			}
			else if (!stricmp(hdr->name, "Content-Type")) {
				char *mime = gf_strdup(hdr->value);
				while (1) {
					u32 len = (u32) strlen(mime);
					char c = len ? mime[len-1] : 0;
					if ((c=='\r') || (c=='\n')) {
						mime[len-1] = 0;
					} else {
						break;
					}
				}
				val = strchr(mime, ';');
				if (val) val[0] = 0;

				strlwr(mime);
				if (rsp_code<300) {
					if (sess->cache_entry) {
						gf_cache_set_mime_type(sess->cache_entry, mime);
					} else {
						sess->mime_type = mime;
						mime = NULL;
					}
				}
				if (mime) gf_free(mime);
			}
			else if (!stricmp(hdr->name, "Content-Range")) {
				if (!strnicmp(hdr->value, "bytes", 5)) {
					val = hdr->value + 5;
					if (val[0] == ':') val += 1;
					while (val[0] == ' ') val += 1;

					if (val[0] == '*') {
						sscanf(val, "*/%u", &total_size);
					} else {
						sscanf(val, "%u-%u/%u", &first_byte, &last_byte, &total_size);
					}
				}
			}
			else if (!stricmp(hdr->name, "Accept-Ranges")) {
				if (strstr(hdr->value, "none")) no_range = 1;
			}
			else if (!stricmp(hdr->name, "Location"))
				new_location = gf_strdup(hdr->value);
			else if (!strnicmp(hdr->name, "ice", 3) || !strnicmp(hdr->name, "icy", 3) ) {
				/* For HTTP icy servers, we disable cache */
				if (sess->icy_metaint == 0)
					sess->icy_metaint = -1;
				sess->use_cache_file = GF_FALSE;
				if (!stricmp(hdr->name, "icy-metaint")) {
					sess->icy_metaint = atoi(hdr->value);
				}
			}
			else if (!stricmp(hdr->name, "Cache-Control")) {
				if (strstr(hdr->value, "no-store")) {
					cache_no_store = GF_TRUE;
				}
			}
			else if (!stricmp(hdr->name, "ETag")) {
				if (rsp_code<300)
					gf_cache_set_etag_on_server(sess->cache_entry, hdr->value);
			}
			else if (!stricmp(hdr->name, "Last-Modified")) {
				if (rsp_code<300)
					gf_cache_set_last_modified_on_server(sess->cache_entry, hdr->value);
			}
			else if (!stricmp(hdr->name, "Transfer-Encoding")) {
				if (!stricmp(hdr->value, "chunked"))
					sess->chunked = GF_TRUE;
			}
			else if (!stricmp(hdr->name, "X-UserProfileID") ) {
				gf_opts_set_key("core", "user-profileid", hdr->value);
			}
			else if (!stricmp(hdr->name, "Connection") ) {
				if (strstr(hdr->value, "close"))
					connection_closed = GF_TRUE;
				else if (strstr(hdr->value, "Keep-Alive"))
					connection_keep_alive = GF_TRUE;
			}
			else if (!stricmp(hdr->name, "Keep-Alive") ) {
				char *tout = strstr(hdr->value, "timeout=");
				if (tout) {
					char c=0;
					s32 end = gf_token_find(tout, 0, (u32) strlen(tout), ", ");
					if (end>=0) {
						c = tout[end];
						tout[end] = 0;
					}
					connection_timeout = atoi(tout+8);
					if (end>=0) tout[end] = c;
				}
				if (strstr(hdr->value, "close"))
					connection_closed = GF_TRUE;
			}
#ifdef GPAC_HAS_HTTP2
			else if (!stricmp(hdr->name, "Upgrade") ) {
				if (!sess->dm->disable_http2 && !gf_opts_get_bool("core", "no-h2c") && !strncmp(hdr->value,"h2c", 3)) {
					upgrade_to_http2 = GF_TRUE;
				}
			}
#endif

			if (sess->status==GF_NETIO_DISCONNECTED) return GF_OK;
		}

		if ((sess->flags & GF_NETIO_SESSION_AUTO_CACHE) && !ContentLength && (rsp_code>=200) && (rsp_code<300) ) {
			sess->use_cache_file = GF_FALSE;
			if (sess->cache_entry) {
				gf_cache_remove_session_from_cache_entry(sess->cache_entry, sess);
				sess->cache_entry = NULL;
			}
		}

		if (cache_no_store) {
			if (sess->cache_entry && !ContentLength && !sess->chunked && (rsp_code<300)
#ifdef GPAC_HAS_HTTP2
				&& !sess->h2_sess
#endif
			) {
				sess->use_cache_file = GF_FALSE;
				gf_cache_remove_session_from_cache_entry(sess->cache_entry, sess);
				sess->cache_entry = NULL;
			}
		}

		if (no_range) first_byte = 0;


		gf_cache_set_headers_processed(sess->cache_entry);

		if (connection_keep_alive
			&& !gf_opts_get_bool("core", "no-timeout")
			&& !sess->server_mode
#ifdef GPAC_HAS_HTTP2
			&& !sess->h2_sess
#endif
		) {
			sess->connection_timeout_ms = connection_timeout*1000;
		}
	}

	par.msg_type = GF_NETIO_PARSE_REPLY;
	par.error = GF_OK;
	par.reply = rsp_code;
	par.value = comp;
	/*
	 * If response is correct, it means our credentials are correct
	 */
	if (sess->creds && rsp_code != 304)
		sess->creds->valid = GF_TRUE;

#ifdef GPAC_HAS_HTTP2
	if ((rsp_code == 101) && upgrade_to_http2) {
		int rv;
		u8 settings[HTTP2_BUFFER_SETTINGS_SIZE];
		u32 settings_len;

		nghttp2_settings_entry h2_settings[2] = {
			{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
			{NGHTTP2_SETTINGS_ENABLE_PUSH, 0}
		};
		settings_len = (u32) nghttp2_pack_settings_payload(settings, HTTP2_BUFFER_SETTINGS_SIZE, h2_settings, GF_ARRAY_LENGTH(h2_settings));

		h2_initialize_session(sess);
		sess->h2_stream_id = 1;
		rv = nghttp2_session_upgrade2(sess->h2_sess->ng_sess, settings, settings_len, (sess->http_read_type==1) ? 1 : 0, sess);
		if (rv < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] nghttp2_session_upgrade2 error: %s\n", nghttp2_strerror(rv)));
			return GF_IP_NETWORK_FAILURE;
		}
		//push the body
		if (bytesRead > BodyStart) {
			rv = (int) nghttp2_session_mem_recv(sess->h2_sess->ng_sess, sHTTP + BodyStart , bytesRead - BodyStart);
			if (rv < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] nghttp2_session_mem_recv error: %s\n", nghttp2_strerror(rv)));
				return GF_IP_NETWORK_FAILURE;
			}
			//stay in WAIT_FOR_REPLY state and do not flush data, cache is not fully configured yet
		}
		//send pending frames
		e = h2_session_send(sess);
		if (e) return e;
		sess->connection_close = GF_FALSE;
		sess->h2_upgrade_state = 2;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] Upgraded connection to HTTP/2\n"));
		return GF_OK;
	}
//	if (sess->h2_upgrade_state<2)
		sess->h2_upgrade_state = 2;
#endif


	/*try to flush body */
	if ((rsp_code>=300)
#ifdef GPAC_HAS_HTTP2
		&& !sess->h2_sess
#endif
	) {
		u32 start = gf_sys_clock();
		while (BodyStart + ContentLength > (u32) bytesRead) {
			e = gf_dm_read_data(sess, sHTTP + bytesRead, buf_size - bytesRead, &res);
			switch (e) {
			case GF_IP_NETWORK_EMPTY:
				break;
			case GF_OK:
				bytesRead += res;
				break;
			default:
				start=0;
				break;
			}
			if (gf_sys_clock()-start>100)
				break;

			//does not fit in our buffer, too bad we'll kill the connection
			if (bytesRead == GF_DOWNLOAD_BUFFER_SIZE)
				break;
		}

		if (BodyStart + ContentLength > (u32) bytesRead) {
			ContentLength = 0;
			//cannot flush, discard socket
			sess->connection_close = GF_TRUE;
		}
	}

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess) {
		u32 count = gf_list_count(sess->headers);
		for (i=0; i<count; i++) {
			GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
			if (!stricmp(hdr->name, ":method")) {
				method = http_parse_method(hdr->value);
				rsp_code = 200;
			}
			else if (!stricmp(hdr->name, ":path")) {
				if (sess->orig_url) gf_free(sess->orig_url);
				sess->orig_url = gf_strdup(hdr->value);
			}
		}
	} else if (sess->server_mode && !gf_opts_get_bool("core", "no-h2") && !gf_opts_get_bool("core", "no-h2c")) {
		Bool is_upgradeable = GF_FALSE;
		char *h2_settings = NULL;
		u32 count = gf_list_count(sess->headers);
		for (i=0; i<count; i++) {
			GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
			if (!stricmp(hdr->name, "Upgrade")) {
				if (strstr(hdr->value, "h2c"))
					is_upgradeable = GF_TRUE;
			}
			else if (!stricmp(hdr->name, "HTTP2-Settings")) {
				h2_settings = hdr->value;
			}
		}

		if (is_upgradeable && h2_settings) {
			u32 len = (u32) strlen(h2_settings);
			sess->h2_upgrade_settings = gf_malloc(sizeof(char) * len * 2);
			sess->h2_upgrade_settings_len = gf_base64_decode(h2_settings, len, sess->h2_upgrade_settings, len*2);
		}
	}
#endif


	if (sess->server_mode) {
		if (ContentLength) {
			par.data = sHTTP + BodyStart;
			par.size = ContentLength;
		} else if ((BodyStart < (s32) bytesRead)
#ifdef GPAC_HAS_HTTP2
			&& !sess->h2_sess
#endif
		) {
			if (sess->init_data) gf_free(sess->init_data);
			sess->init_data_size = 0;
			sess->init_data = NULL;

			gf_dm_data_received(sess, (u8 *) sHTTP + BodyStart, bytesRead - BodyStart, GF_TRUE, NULL, NULL);
		}

		sess->request_start_time = gf_sys_clock_high_res();

		par.reply = method;
		gf_dm_sess_user_io(sess, &par);
		sess->status = GF_NETIO_DATA_TRANSFERED;
		SET_LAST_ERR(GF_OK)
		return GF_OK;
	}
	//remember if we can keep the session alive after the transfer is done
	sess->connection_close = connection_closed;
	assert(rsp_code);

	switch (rsp_code) {
	//100 continue
	case 100:
		break;
	case 200:
	case 201:
	case 202:
	case 206:
		gf_dm_sess_user_io(sess, &par);
		e = GF_OK;
		if (sess->proxy_enabled==2) {
			sess->proxy_enabled=0;
			if (sess->dm)
				gf_list_add(sess->dm->skip_proxy_servers, gf_strdup(sess->server_name));
		}
		break;
	/*redirection: extract the new location*/
	case 301:
	case 302:
	case 303:
	case 307:
		if (!new_location || !strlen(new_location) ) {
			gf_dm_sess_user_io(sess, &par);
			e = GF_URL_ERROR;
			goto exit;
		}
		while (
		    (new_location[strlen(new_location)-1] == '\n')
		    || (new_location[strlen(new_location)-1] == '\r')  )
			new_location[strlen(new_location)-1] = 0;

		/*reset and reconnect*/
		gf_dm_disconnect(sess, HTTP_CLOSE);
		sess->status = GF_NETIO_SETUP;
		e = gf_dm_sess_setup_from_url(sess, new_location, GF_FALSE);
		if (e) {
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(e)
			gf_dm_sess_notify_state(sess, sess->status, e);
		}
		gf_free(new_location);
		return e;
	case 304:
	{
		sess->status = GF_NETIO_PARSE_REPLY;
		assert(sess->cache_entry);
		sess->total_size = gf_cache_get_cache_filesize(sess->cache_entry);

		gf_dm_sess_notify_state(sess, GF_NETIO_PARSE_REPLY, GF_OK);

		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		if (sess->user_proc) {
			/* For modules that do not use cache and have problems with GF_NETIO_DATA_TRANSFERED ... */
			const char * filename;
			FILE * f;
			filename = gf_cache_get_cache_filename(sess->cache_entry);
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Sending data to modules from %s...\n", filename));
			f = gf_fopen(filename, "rb");
			assert(filename);
			if (!f) {
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] FAILED to open cache file %s for reading contents !\n", filename));
				/* Ooops, no cache, redownload everything ! */
				gf_dm_disconnect(sess, HTTP_NO_CLOSE);
				sess->status = GF_NETIO_SETUP;
				e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
				sess->total_size = gf_cache_get_cache_filesize(sess->cache_entry);
				if (e) {
					sess->status = GF_NETIO_STATE_ERROR;
					SET_LAST_ERR(e)
					gf_dm_sess_notify_state(sess, sess->status, e);
				}
				return e;
			}

			par.error = GF_OK;
			par.msg_type = GF_NETIO_PARSE_HEADER;
			par.name = "Content-Type";
			par.value = (char *) gf_cache_get_mime_type(sess->cache_entry);
			gf_dm_sess_user_io(sess, &par);

			sess->status = GF_NETIO_DATA_EXCHANGE;
			if (! (sess->flags & GF_NETIO_SESSION_NOT_THREADED) || sess->force_data_write_callback) {
				char file_cache_buff[16544];
				s32 read = 0;
				total_size = gf_cache_get_cache_filesize(sess->cache_entry);
				do {
					read = (s32) gf_fread(file_cache_buff, 16384, f);
					if (read > 0) {
						sess->bytes_done += read;
						sess->total_size = total_size;
						sess->bytes_per_sec = 0xFFFFFFFF;
						par.size = read;
						par.msg_type = GF_NETIO_DATA_EXCHANGE;
						par.error = GF_EOS;
						par.reply = 2;
						par.data = file_cache_buff;
						gf_dm_sess_user_io(sess, &par);
					}
				} while ( read > 0);
			}
			gf_fclose(f);
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] all data has been sent to modules from %s.\n", filename));
		}
		/* Cache file is the most recent */
		sess->status = GF_NETIO_DATA_TRANSFERED;
		SET_LAST_ERR(GF_OK)
		par.error = GF_OK;
		gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		return GF_OK;
	}
	case 401:
	{
		if (sess->creds && sess->creds->valid) {
			gf_opts_set_key("credentials", sess->creds->site, NULL);
			sess->creds->valid = GF_FALSE;
		}
		Bool secure = GF_FALSE;
#ifdef GPAC_HAS_SSL
		if (sess->ssl) secure = GF_TRUE;
#endif
		/* Do we have a credentials struct ? */
		sess->creds = gf_user_credentials_register(sess->dm, secure, sess->server_name, NULL, NULL, GF_FALSE);
		if (!sess->creds) {
			/* User credentials have not been filled properly, we have to abort */
			gf_dm_disconnect(sess, HTTP_CLOSE);
			sess->status = GF_NETIO_STATE_ERROR;
			par.error = GF_AUTHENTICATION_FAILURE;
			par.msg_type = GF_NETIO_DISCONNECTED;
			gf_dm_sess_user_io(sess, &par);
			e = GF_AUTHENTICATION_FAILURE;
			SET_LAST_ERR(e)
			goto exit;
		}
		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		sess->status = GF_NETIO_SETUP;

		if (sess->creds->req_state==GF_CREDS_STATE_PENDING) {
			//force a wait for reply until we have resolved user pass
			sess->status = GF_NETIO_WAIT_FOR_REPLY;
			return GF_OK;
		}

		e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
		if (e) {
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(e)
			gf_dm_sess_notify_state(sess, sess->status, e);
		}
		return e;
	}
    case 400:
	case 501:
		/* Method not implemented ! */
		if (sess->http_read_type == HEAD) {
			/* Since HEAD is not understood by this server, we use a GET instead */
			sess->http_read_type = GET;
			sess->flags |= GF_NETIO_SESSION_NOT_CACHED;
			gf_dm_disconnect(sess, HTTP_NO_CLOSE);
			sess->status = GF_NETIO_SETUP;
			sess->server_only_understand_get = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("Method not supported, try with GET.\n"));
			e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
			if (e) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(e)
				gf_dm_sess_notify_state(sess, sess->status, e);
			}
			return e;
		}

		gf_dm_sess_user_io(sess, &par);
		notify_headers(sess, sHTTP, bytesRead, BodyStart);
		e = GF_REMOTE_SERVICE_ERROR;
		goto exit;

	case 503:
		/*retry without proxy*/
		if (sess->proxy_enabled==1) {
			sess->proxy_enabled=2;
			gf_dm_disconnect(sess, HTTP_CLOSE);
			sess->status = GF_NETIO_SETUP;
			return GF_OK;
		}
		//fall-through

/*	case 204:
    case 504:
	case 404:
    case 403:
    case 416:
*/
    default:
		gf_dm_sess_user_io(sess, &par);
		if ((BodyStart < (s32) bytesRead)) {
			sHTTP[bytesRead] = 0;
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTP] Failure - body: %s\n", sHTTP + BodyStart));
		}
		notify_headers(sess, sHTTP, bytesRead, BodyStart);

		switch (rsp_code) {
		case 204: e = GF_EOS; break;
		/* File not found */
		case 404: e = GF_URL_ERROR; break;
		/* Forbidden */
		case 403: e = GF_AUTHENTICATION_FAILURE; break;
		/* Range not accepted */
		case 416: e = GF_SERVICE_ERROR; break;
		case 504: e = GF_URL_ERROR; break;
		default:
			if (rsp_code>=500) e = GF_REMOTE_SERVICE_ERROR;
			else e = GF_SERVICE_ERROR;
			break;
		}
		goto exit;
	}

	notify_headers(sess, NULL, bytesRead, BodyStart);

	if (sess->http_read_type != GET)
		sess->use_cache_file = GF_FALSE;

	if (sess->http_read_type==HEAD) {
		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
		sess->http_read_type = GET;
		return GF_OK;
	}


	mime_type = gf_cache_get_mime_type(sess->cache_entry);
	if (!ContentLength && mime_type && ((strstr(mime_type, "ogg") || (!strcmp(mime_type, "audio/mpeg"))))) {
		if (0 == sess->icy_metaint)
			sess->icy_metaint = -1;
		sess->use_cache_file = GF_FALSE;
	}

#ifndef GPAC_DISABLE_LOG
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP] Error processing rely from %s: %s\n", sess->server_name, gf_error_to_string(e) ) );
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP] Reply processed from %s\n", sess->server_name ) );
	}
#endif

	//in HTTP2 we may resume to setup state if we got a refused stream
	if (sess->status == GF_NETIO_SETUP) {
		return GF_OK;
	}


	/*some servers may reply without content length, but we MUST have it*/
	if (e) goto exit;
	if (sess->icy_metaint != 0) {
		assert( ! sess->use_cache_file );
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] ICY protocol detected\n"));
		if (mime_type && !stricmp(mime_type, "video/nsv")) {
			gf_cache_set_mime_type(sess->cache_entry, "audio/aac");
		}
		sess->icy_bytes = 0;
		sess->total_size = SIZE_IN_STREAM;
		sess->status = GF_NETIO_DATA_EXCHANGE;
	} else if (!ContentLength && !sess->chunked
#ifdef GPAC_HAS_HTTP2
		&& !sess->h2_sess
#endif
	) {
		if (sess->http_read_type == GET) {
			sess->total_size = SIZE_IN_STREAM;
			sess->use_cache_file = GF_FALSE;
			sess->status = GF_NETIO_DATA_EXCHANGE;
			sess->bytes_done = 0;
		} else {
			gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
			gf_dm_disconnect(sess, HTTP_NO_CLOSE);
			return GF_OK;
		}
#ifdef GPAC_HAS_HTTP2
	} else if (sess->h2_sess && !ContentLength && (sess->http_read_type != GET)) {
		gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		return GF_OK;
#endif
	} else {
		sess->total_size = ContentLength;
		if (sess->use_cache_file && sess->http_read_type == GET ) {
			e = gf_cache_open_write_cache(sess->cache_entry, sess);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ( "[CACHE] Failed to open cache, error=%d\n", e));
				goto exit;
			}
		}
		sess->status = GF_NETIO_DATA_EXCHANGE;
		sess->bytes_done = 0;
	}

	/* we may have existing data in this buffer ... */
#ifdef GPAC_HAS_HTTP2
	if (!e && sess->h2_sess) {
		h2_flush_data(sess, GF_TRUE);
	} else
#endif
	if (!e && (BodyStart < (s32) bytesRead)) {
		u32 rewrite_size=0;
		if (sess->init_data) gf_free(sess->init_data);
		sess->init_data_size = 0;
		sess->init_data = NULL;

		gf_dm_data_received(sess, (u8 *) sHTTP + BodyStart, bytesRead - BodyStart, GF_TRUE, &rewrite_size, NULL);

		if (sess->chunked)
			gf_dm_sess_estimate_chunk_rate(sess, rewrite_size);
	}
exit:
	if (e) {
		if (e<0) {
			GF_LOG((e==GF_URL_ERROR) ? GF_LOG_INFO : GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTP] Error parsing reply for URL %s: %s (code %d)\n", sess->orig_url,  gf_error_to_string(e), rsp_code ));
		} else {
			e = GF_OK;
		}
		gf_cache_entry_set_delete_files_when_deleted(sess->cache_entry);
		gf_dm_remove_cache_entry_from_session(sess);
		sess->cache_entry = NULL;
		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		if (connection_closed)
			sess->status = GF_NETIO_STATE_ERROR;
		else
			sess->status = GF_NETIO_DATA_TRANSFERED;
		SET_LAST_ERR(e)
		gf_dm_sess_notify_state(sess, sess->status, e);
		return e;
	}
	/*DO NOT call parse_body yet, as the final user may not be connected to our session*/
	return GF_OK;
}

/**
 * Default performing behavior
\param sess The session
 */
void http_do_requests(GF_DownloadSession *sess)
{
	char sHTTP[GF_DOWNLOAD_BUFFER_SIZE+1];
	sHTTP[0] = 0;

	if (sess->reused_cache_entry) {
		//main session is done downloading, notify - to do we should send progress events on this session also ...
		if (!gf_cache_is_in_progress(sess->cache_entry)) {
			GF_NETIO_Parameter par;
			gf_dm_disconnect(sess, HTTP_NO_CLOSE);
			sess->reused_cache_entry = GF_FALSE;
			memset(&par, 0, sizeof(GF_NETIO_Parameter));
			par.msg_type = GF_NETIO_DATA_TRANSFERED;
			par.error = GF_OK;
			gf_dm_sess_user_io(sess, &par);
		}
		return;
	}

	if (sess->async_buf_size && (gf_dm_sess_flush_async(sess, GF_FALSE) == GF_IP_NETWORK_EMPTY)) {
		return;
	}

	switch (sess->status) {
	case GF_NETIO_CONNECTED:
		if (sess->server_mode) {
			wait_for_header_and_parse(sess, sHTTP);
		} else {
			http_send_headers(sess, sHTTP);
		}
		break;
	case GF_NETIO_WAIT_FOR_REPLY:
		if (sess->server_mode) {
			http_send_headers(sess, sHTTP);
		} else {
			wait_for_header_and_parse(sess, sHTTP);
		}
		break;
	case GF_NETIO_DATA_EXCHANGE:
		if (sess->server_mode) {
			sess->status = GF_NETIO_CONNECTED;
			break;
		}
		/*session has been reassigned, resend data retrieved in first GET reply to user but don't write to cache*/
		if (sess->reassigned) {

			if (sess->icy_metaint > 0) {
				//we are reparsing init data, reset icy status
				sess->icy_bytes = 0;
				gf_icy_skip_data(sess, sess->init_data, sess->init_data_size);
			} else {
				GF_NETIO_Parameter par;
				par.msg_type = GF_NETIO_DATA_EXCHANGE;
				par.error = GF_OK;
				par.data = sess->init_data;
				par.size = sess->init_data_size;
				gf_dm_sess_user_io(sess, &par);
			}
			sess->reassigned = GF_FALSE;
		}
		http_parse_remaining_body(sess, sHTTP);
		break;
	default:
		break;
	}
}


/**
 * NET IO for MPD, we don't need this anymore since mime-type can be given by session
 */
static void wget_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	FILE * f = (FILE*) cbk;

	/*handle service message*/
	if (param->msg_type == GF_NETIO_DATA_EXCHANGE) {
		s32 written = (u32) gf_fwrite( param->data, param->size, f);
		if (written != param->size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("Failed to write data on disk\n"));
		}
	}
}

GF_EXPORT
GF_Err gf_dm_wget(const char *url, const char *filename, u64 start_range, u64 end_range, char **redirected_url)
{
	GF_Err e;
	GF_DownloadManager * dm = gf_dm_new(NULL);
	if (!dm)
		return GF_OUT_OF_MEM;
	e = gf_dm_wget_with_cache(dm, url, filename, start_range, end_range, redirected_url);
	gf_dm_del(dm);
	return e;
}

GF_EXPORT
GF_Err gf_dm_wget_with_cache(GF_DownloadManager * dm, const char *url, const char *filename, u64 start_range, u64 end_range, char **redirected_url)
{
	GF_Err e;
	FILE * f;
	GF_DownloadSession *dnload;
	if (!filename || !url || !dm)
		return GF_BAD_PARAM;
	f = gf_fopen(filename, "wb");
	if (!f) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[WGET] Failed to open file %s for write.\n", filename));
		return GF_IO_ERR;
	}
	dnload = gf_dm_sess_new_simple(dm, (char *)url, GF_NETIO_SESSION_NOT_THREADED, &wget_NetIO, f, &e);
	if (!dnload) {
		return GF_BAD_PARAM;
	}
	dnload->use_cache_file = GF_FALSE;
	dnload->force_data_write_callback = GF_TRUE;
	if (end_range) {
		dnload->range_start = start_range;
		dnload->range_end = end_range;
		dnload->needs_range = GF_TRUE;
	}
	if (e == GF_OK) {
		e = gf_dm_sess_process(dnload);
	}
	e |= gf_cache_close_write_cache(dnload->cache_entry, dnload, (e == GF_OK) ? GF_TRUE : GF_FALSE);
	gf_fclose(f);

	if (redirected_url) {
		if (dnload->orig_url_before_redirect) *redirected_url = gf_strdup(dnload->orig_url);
	}
	gf_dm_sess_del(dnload);
	return e;
}

#if 0 //unused

/*
\brief fetches remote file in memory
 *
 *Fetches remote file in memory.
\param url the data to fetch
\param out_data output data (allocated by function)
\param out_size output data size
\param out_mime if not NULL, pointer will contain the mime type (allocated by function)
\return error code if any
 */
GF_Err gf_dm_get_file_memory(const char *url, char **out_data, u32 *out_size, char **out_mime)
{
	GF_Err e;
	FILE * f;
	char * f_fn = NULL;
	GF_DownloadSession *dnload;
	GF_DownloadManager *dm;

	if (!url || !out_data || !out_size)
		return GF_BAD_PARAM;
	f = gf_file_temp(&f_fn);
	if (!f) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[WGET] Failed to create temp file for write.\n"));
		return GF_IO_ERR;
	}

	dm = gf_dm_new(NULL);
	if (!dm) {
		gf_fclose(f);
		gf_file_delete(f_fn);
		return GF_OUT_OF_MEM;
	}

	dnload = gf_dm_sess_new_simple(dm, (char *)url, GF_NETIO_SESSION_NOT_THREADED, &wget_NetIO, f, &e);
	if (!dnload) {
		gf_dm_del(dm);
		gf_fclose(f);
		gf_file_delete(f_fn);
		return GF_BAD_PARAM;
	}
	dnload->use_cache_file = GF_FALSE;
	dnload->disable_cache = GF_TRUE;
	if (!e)
		e = gf_dm_sess_process(dnload);

	if (!e)
		e = gf_cache_close_write_cache(dnload->cache_entry, dnload, e == GF_OK);

	if (!e) {
		u32 size = (u32) gf_ftell(f);
		s32 read;
		*out_size = size;
		*out_data = (char*)gf_malloc(sizeof(char)* ( 1 + size));
		gf_fseek(f, 0, SEEK_SET);
		read = (s32) gf_fread(*out_data, size, f);
		if (read != size) {
			gf_free(*out_data);
			e = GF_IO_ERR;
		} else {
			(*out_data)[size] = 0;
			if (out_mime) {
				const char *mime = gf_dm_sess_mime_type(dnload);
				if (mime) *out_mime = gf_strdup(mime);
			}
		}
	}
	gf_fclose(f);
	gf_file_delete(f_fn);
	gf_free(f_fn);
	gf_dm_sess_del(dnload);
	gf_dm_del(dm);
	return e;
}
#endif

GF_EXPORT
const char *gf_dm_sess_get_resource_name(GF_DownloadSession *dnload)
{
	return dnload ? dnload->orig_url : NULL;
}


#if 0 //unused
/*!
\brief Get session original resource url
 *
 *Returns the original resource URL before any redirection associated with the session
\param sess the download session
\return the associated URL
 */
const char *gf_dm_sess_get_original_resource_name(GF_DownloadSession *dnload)
{
	if (dnload) return dnload->orig_url_before_redirect ? dnload->orig_url_before_redirect : dnload->orig_url;
	return NULL;
}

/*!
\brief fetch session status
 *
 *Fetch the session current status
\param sess the download session
\return the session status*/
u32 gf_dm_sess_get_status(GF_DownloadSession *dnload)
{
	return dnload ? dnload->status : GF_NETIO_STATE_ERROR;
}


/*!
\brief Reset session
 *
 *Resets the session for new processing of the same url
\param sess the download session
\return error code if any
 */
GF_Err gf_dm_sess_reset(GF_DownloadSession *sess)
{
	if (!sess)
		return GF_BAD_PARAM;
	sess->status = GF_NETIO_SETUP;
	sess->needs_range = GF_FALSE;
	sess->range_start = sess->range_end = 0;
	sess->bytes_done = sess->bytes_per_sec = 0;
	if (sess->init_data) gf_free(sess->init_data);
	sess->init_data = NULL;
	sess->init_data_size = 0;
	SET_LAST_ERR(GF_OK)
	sess->total_size = 0;
	sess->start_time = 0;
	sess->start_time_utc = 0;
	sess->max_chunk_size = 0;
	sess->max_chunk_bytes_per_sec = 0;
	return GF_OK;
}

/*!
 * Get a range of a cache entry file
\param sess The session
\param startOffset The first byte of the request to get
\param endOffset The last byte of request to get
\param The temporary name for the file created to have a range of the file
 */
const char * gf_cache_get_cache_filename_range( const GF_DownloadSession * sess, u64 startOffset, u64 endOffset ) {
	u32 i, count;
	if (!sess || !sess->dm || endOffset < startOffset)
		return NULL;
	count = gf_list_count(sess->dm->partial_downloads);
	for (i = 0 ; i < count ; i++) {
		GF_PartialDownload * pd = (GF_PartialDownload*)gf_list_get(sess->dm->partial_downloads, i);
		assert( pd->filename && pd->url);
		if (!strcmp(pd->url, sess->orig_url) && pd->startOffset == startOffset && pd->endOffset == endOffset) {
			/* File already created, just return the file */
			return pd->filename;
		}
	}
	{
		/* Not found, we are gonna create the file */
		char * newFilename;
		GF_PartialDownload * partial;
		FILE * fw, *fr;
		u32 maxLen;
		const char * orig = gf_cache_get_cache_filename(sess->cache_entry);
		if (orig == NULL)
			return NULL;
		/* 22 if 1G + 1G + 2 dashes */
		maxLen = (u32) strlen(orig) + 22;
		newFilename = (char*)gf_malloc( maxLen );
		if (newFilename == NULL)
			return NULL;
		snprintf(newFilename, maxLen, "%s " LLU LLU, orig, startOffset, endOffset);
		fw = gf_fopen(newFilename, "wb");
		if (!fw) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[CACHE] Cannot open partial cache file %s for write\n", newFilename));
			gf_free( newFilename );
			return NULL;
		}
		fr = gf_fopen(orig, "rb");
		if (!fr) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[CACHE] Cannot open full cache file %s\n", orig));
			gf_free( newFilename );
			gf_fclose( fw );
		}
		/* Now, we copy ! */
		{
			char copyBuff[GF_DOWNLOAD_BUFFER_SIZE+1];
			s64 read, write, total;
			total = endOffset - startOffset;
			read = gf_fseek(fr, startOffset, SEEK_SET);
			if (read != startOffset) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[CACHE] Cannot seek at right start offset in %s\n", orig));
				gf_fclose( fr );
				gf_fclose( fw );
				gf_free( newFilename );
				return NULL;
			}
			do {
				read = gf_fread(copyBuff, MIN(sizeof(copyBuff), (size_t)  total), fr);
				if (read > 0) {
					total-= read;
					write = gf_fwrite(copyBuff, (size_t) read, fw);
					if (write != read) {
						/* Something bad happened */
						gf_fclose( fw );
						gf_fclose (fr );
						gf_free( newFilename );
						return NULL;
					}
				} else {
					if (read < 0) {
						gf_fclose( fw );
						gf_fclose( fr );
						gf_free( newFilename );
						return NULL;
					}
				}
			} while (total > 0);
			gf_fclose( fr );
			gf_fclose (fw);
			partial = (GF_PartialDownload*)gf_malloc( sizeof(GF_PartialDownload));
			if (partial == NULL) {
				gf_free(newFilename);
				return NULL;
			}
			partial->filename = newFilename;
			partial->url = sess->orig_url;
			partial->endOffset = endOffset;
			partial->startOffset = startOffset;
			gf_list_add(sess->dm->partial_downloads, partial);
			return newFilename;
		}
	}
}

/*!
 * Reassigns session flags and callbacks. This is only possible if the session is not threaded.
\param sess The session
\param flags The new flags for the session - if flags is 0xFFFFFFFF, existing flags are not modified
\param user_io The new callback function
\param cbk The new user data to be used in the callback function
\param GF_OK or error
 */
GF_Err gf_dm_sess_reassign(GF_DownloadSession *sess, u32 flags, gf_dm_user_io user_io, void *cbk)
{
	/*shall only be called for non-threaded sessions!! */
	if (sess->th)
		return GF_BAD_PARAM;

	if (flags == 0xFFFFFFFF) {
		sess->user_proc = user_io;
		sess->usr_cbk = cbk;
		return GF_OK;
	}

	if (sess->flags & GF_DOWNLOAD_SESSION_USE_SSL) flags |= GF_DOWNLOAD_SESSION_USE_SSL;
	sess->flags = flags;
	if (sess->flags & GF_NETIO_SESSION_NOTIFY_DATA)
		sess->force_data_write_callback = GF_TRUE;

	sess->user_proc = user_io;
	sess->usr_cbk = cbk;
	sess->reassigned = sess->init_data ? GF_TRUE : GF_FALSE;
	sess->num_retry = SESSION_RETRY_COUNT;

	if (sess->status==GF_NETIO_DISCONNECTED)
		sess->status = GF_NETIO_SETUP;

	/*threaded session shall be started with gf_dm_sess_process*/
	return GF_OK;
}
#endif


GF_EXPORT
void gf_dm_set_data_rate(GF_DownloadManager *dm, u32 rate_in_bits_per_sec)
{
	if (rate_in_bits_per_sec == 0xFFFFFFFF) {
		dm->simulate_no_connection=GF_TRUE;
	} else {
		char opt[100];
		dm->simulate_no_connection=GF_FALSE;
		dm->limit_data_rate = rate_in_bits_per_sec/8;

		sprintf(opt, "%d", rate_in_bits_per_sec);
		//temporary store of maxrate
		gf_opts_set_key("temp", "maxrate", opt);

		dm->read_buf_size = GF_DOWNLOAD_BUFFER_SIZE;
		//when rate is limited, use smaller smaller read size
		if (dm->limit_data_rate) dm->read_buf_size = GF_DOWNLOAD_BUFFER_SIZE_LIMIT_RATE;

#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			dm_exceeds_cap_rate(dm);
		}
#endif

	}
}

GF_EXPORT
u32 gf_dm_get_data_rate(GF_DownloadManager *dm)
{
	return dm->limit_data_rate*8;
}

GF_EXPORT
u32 gf_dm_get_global_rate(GF_DownloadManager *dm)
{
	u32 ret = 0;
	u32 i, count;
	if (!dm) return 0;
	gf_mx_p(dm->cache_mx);
	count = gf_list_count(dm->sessions);

	for (i=0; i<count; i++) {
		GF_DownloadSession *sess = (GF_DownloadSession*)gf_list_get(dm->sessions, i);
		if (sess->status >= GF_NETIO_DATA_TRANSFERED) {
			if (sess->total_size==sess->bytes_done) {
				//do not aggregate session if done/interrupted since more than 1/2 a sec
				if (gf_sys_clock_high_res() - sess->start_time > 500000) {
					continue;
				}
			}
		}
		ret += sess->bytes_per_sec;
	}
	gf_mx_v(dm->cache_mx);
	return 8*ret;
}

Bool gf_dm_sess_is_h2(GF_DownloadSession *sess)
{
#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess) return GF_TRUE;
#endif
	return GF_FALSE;
}

GF_EXPORT
const char *gf_dm_sess_get_header(GF_DownloadSession *sess, const char *name)
{
	u32 i, count;
	if( !sess || !name) return NULL;
	count = gf_list_count(sess->headers);
	for (i=0; i<count; i++) {
		GF_HTTPHeader *header = (GF_HTTPHeader*)gf_list_get(sess->headers, i);
		if (!stricmp(header->name, name)) return header->value;
	}
	return NULL;
}

GF_EXPORT
GF_Err gf_dm_sess_enum_headers(GF_DownloadSession *sess, u32 *idx, const char **hdr_name, const char **hdr_val)
{
	GF_HTTPHeader *hdr;
	if( !sess || !idx || !hdr_name || !hdr_val)
		return GF_BAD_PARAM;
	hdr = gf_list_get(sess->headers, *idx);
	if (!hdr) return GF_EOS;
	(*idx) = (*idx) + 1;
	(*hdr_name) = hdr->name;
	(*hdr_val) = hdr->value;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dm_sess_get_header_sizes_and_times(GF_DownloadSession *sess, u32 *req_hdr_size, u32 *rsp_hdr_size, u32 *connect_time, u32 *reply_time, u32 *download_time)
{
	if (!sess)
		return GF_BAD_PARAM;

	if (req_hdr_size) *req_hdr_size = sess->req_hdr_size;
	if (rsp_hdr_size) *rsp_hdr_size = sess->rsp_hdr_size;
	if (connect_time) *connect_time = sess->connect_time;
	if (reply_time) *reply_time = sess->reply_time;
	if (download_time) *download_time = sess->total_time_since_req;
	return GF_OK;
}

GF_EXPORT
void gf_dm_sess_force_memory_mode(GF_DownloadSession *sess, u32 force_keep)
{
	if (sess) {
		sess->flags |= GF_NETIO_SESSION_MEMORY_CACHE;
		sess->flags &= ~GF_NETIO_SESSION_NOT_CACHED;
		if (force_keep==1) {
			sess->flags |= GF_NETIO_SESSION_KEEP_CACHE;
		} else if (force_keep==2)
			sess->flags |= GF_NETIO_SESSION_KEEP_FIRST_CACHE;
	}
}

GF_EXPORT
GF_Err gf_dm_set_localcache_provider(GF_DownloadManager *dm, Bool (*local_cache_url_provider_cbk)(void *udta, char *url, Bool is_cache_destroy), void *lc_udta)
{
	if (!dm)
		return GF_BAD_PARAM;
	dm->local_cache_url_provider_cbk = local_cache_url_provider_cbk;
	dm->lc_udta = lc_udta;
	return GF_OK;

}

GF_EXPORT
DownloadedCacheEntry gf_dm_add_cache_entry(GF_DownloadManager *dm, const char *szURL, GF_Blob *blob, u64 start_range, u64 end_range, const char *mime, Bool clone_memory, u32 download_time_ms)
{
	u32 i, count;
	DownloadedCacheEntry the_entry = NULL;

	gf_mx_p(dm->cache_mx );
	if (blob)
		GF_LOG(GF_LOG_INFO, GF_LOG_CACHE, ("[HTTP] Pushing %s to cache "LLU" bytes (done %s)\n", szURL, blob->size, (blob->flags & GF_BLOB_IN_TRANSFER) ? "no" : "yes"));
	count = gf_list_count(dm->cache_entries);
	for (i = 0 ; i < count; i++) {
		const char * url;
		DownloadedCacheEntry e = (DownloadedCacheEntry)gf_list_get(dm->cache_entries, i);
		assert(e);
		url = gf_cache_get_url(e);
		assert( url );
		if (strcmp(url, szURL)) continue;

		if (end_range) {
			if (start_range != gf_cache_get_start_range(e)) continue;
			if (end_range != gf_cache_get_end_range(e)) continue;
		}
		/*OK that's ours*/
		the_entry = e;
		break;
	}
	if (!the_entry) {
		the_entry = gf_cache_create_entry(dm, "", szURL, 0, 0, GF_TRUE, dm->cache_mx);
		if (!the_entry) {
			gf_mx_v(dm->cache_mx );
			return NULL;
		}
		gf_list_add(dm->cache_entries, the_entry);
	}

	gf_cache_set_mime(the_entry, mime);
	if (blob && ! (blob->flags & GF_BLOB_IN_TRANSFER))
		gf_cache_set_range(the_entry, blob->size, start_range, end_range);

	gf_cache_set_content(the_entry, blob, clone_memory ? GF_TRUE : GF_FALSE, dm->cache_mx);
	gf_cache_set_downtime(the_entry, download_time_ms);
	gf_mx_v(dm->cache_mx );
	return the_entry;
}

GF_EXPORT
GF_Err gf_dm_force_headers(GF_DownloadManager *dm, const DownloadedCacheEntry entry, const char *headers)
{
	u32 i, count;
	Bool res;
	if (!entry)
		return GF_BAD_PARAM;
	gf_mx_p(dm->cache_mx);
	res = gf_cache_set_headers(entry, headers);
	count = gf_list_count(dm->sessions);
	for (i=0; i<count; i++) {
		GF_DownloadSession *sess = gf_list_get(dm->sessions, i);
		if (sess->cache_entry != entry) continue;
		gf_dm_sess_reload_cached_headers(sess);
	}

#ifdef GPAC_ENABLE_COVERAGE
	if (!count && gf_sys_is_cov_mode()) {
		gf_dm_sess_reload_cached_headers(NULL);
		gf_dm_refresh_cache_entry(NULL);
		gf_dm_session_thread(NULL);
		gf_cache_are_headers_processed(NULL);
		gf_cache_get_start_range(NULL);
		gf_cache_get_end_range(NULL);
		gf_cache_get_content_length(NULL);
		gf_cache_set_end_range(NULL, 0);
		gf_cache_get_forced_headers(NULL);
		gf_cache_get_downtime(NULL);
	}
#endif

	gf_mx_v(dm->cache_mx);
	if (res) return GF_OK;
	return GF_BAD_PARAM;
}

GF_Err gf_dm_sess_flush_async(GF_DownloadSession *sess, Bool no_select)
{
	if (!sess) return GF_OK;

	if (!no_select && (gf_sk_select(sess->sock, GF_SK_SELECT_WRITE)!=GF_OK)) {
		return GF_IP_NETWORK_EMPTY;
	}
#ifdef GPAC_HAS_HTTP2
	if (sess->h2_local_buf_len) {
		u8 was_eos = sess->h2_is_eos;
		assert(sess->h2_send_data==NULL);
		sess->h2_send_data = sess->h2_local_buf;
		sess->h2_send_data_len = sess->h2_local_buf_len;
		gf_mx_p(sess->mx);
		if (sess->h2_data_paused) {
			sess->h2_data_paused = 0;
			nghttp2_session_resume_data(sess->h2_sess->ng_sess, sess->h2_stream_id);
		}
		h2_flush_send(sess, GF_TRUE);
		gf_mx_v(sess->mx);
		if (sess->h2_send_data_len) {
			if (sess->h2_send_data_len<sess->h2_local_buf_len) {
				memmove(sess->h2_local_buf, sess->h2_send_data, sess->h2_send_data_len);
				sess->h2_local_buf_len = sess->h2_send_data_len;
			}
			sess->h2_send_data = NULL;
			sess->h2_send_data_len = 0;
			sess->h2_is_eos = was_eos;
			return GF_IP_NETWORK_EMPTY;
		}
		sess->h2_local_buf_len = 0;
		sess->h2_is_eos = 0;
	}
#endif

#ifdef GPAC_HAS_HTTP2
	//if H2 flush the parent session holding the http2 session
	if (sess->h2_sess)
		sess = sess->h2_sess->net_sess;
#endif

	if (sess->async_buf_size) {
		GF_Err e = dm_sess_write(sess, sess->async_buf, sess->async_buf_size);
		if (e) return e;
		if (sess->async_buf_size) return GF_IP_NETWORK_EMPTY;
	}
	return GF_OK;
}

u32 gf_dm_sess_async_pending(GF_DownloadSession *sess)
{
	if (!sess) return 0;
#ifdef GPAC_HAS_HTTP2
	if (sess->h2_local_buf_len) return sess->h2_local_buf_len;
	if (sess->h2_sess)
		sess = sess->h2_sess->net_sess;
#endif
	return sess ? sess->async_buf_size : 0;
}

GF_EXPORT
GF_Err gf_dm_sess_send(GF_DownloadSession *sess, u8 *data, u32 size)
{
	GF_Err e = GF_OK;

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_sess) {
		if (sess->h2_send_data)
			return GF_SERVICE_ERROR;
		if (!sess->h2_stream_id)
			return GF_URL_REMOVED;

		gf_mx_p(sess->mx);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] Sending %d bytes on stream_id %d\n", size, sess->h2_stream_id));

		sess->h2_send_data = data;
		sess->h2_send_data_len = size;
		if (sess->h2_data_paused) {
			sess->h2_data_paused = 0;
			nghttp2_session_resume_data(sess->h2_sess->ng_sess, sess->h2_stream_id);
		}
		//if no data, signal end of stream, otherwise regular send
		if (!data || !size) {
			if (sess->h2_local_buf_len) {
				gf_mx_v(sess->mx);
				if (sess->h2_is_eos) {
					return GF_IP_NETWORK_EMPTY;
				}
				sess->h2_is_eos = 1;
				return GF_OK;
			}
			sess->h2_is_eos = 1;
			h2_session_send(sess);
			//stream_id is not yet 0 in case of PUT/PUSH, stream is closed once we get reply from server
		} else {
			sess->h2_is_eos = 0;
			//send the data
			h2_flush_send(sess, GF_FALSE);
		}
		sess->h2_is_eos = 0;

		gf_mx_v(sess->mx);

		if (!data || !size) {
			if (sess->put_state) {
				sess->put_state = 2;
				sess->status = GF_NETIO_WAIT_FOR_REPLY;
				return GF_OK;
			}
		}
		if (!sess->h2_stream_id && sess->h2_send_data)
			return GF_URL_REMOVED;
		return GF_OK;
	}
#endif

	if (!data || !size) {
		if (sess->put_state) {
			sess->put_state = 2;
			sess->status = GF_NETIO_WAIT_FOR_REPLY;
			return GF_OK;
		}
		return GF_OK;
	}

	e = dm_sess_write(sess, data, size);

	if (e==GF_IP_CONNECTION_CLOSED) {
		sess_connection_closed(sess);
		sess->status = GF_NETIO_STATE_ERROR;
		return e;
	}
	else if (e==GF_IP_NETWORK_EMPTY) {
		if (sess->flags & GF_NETIO_SESSION_NO_BLOCK)
			return GF_OK;

		return gf_dm_sess_send(sess, data, size);
	}
	return e;
}

void gf_dm_sess_flush_h2(GF_DownloadSession *sess)
{
#ifdef GPAC_HAS_HTTP2
	if (!sess->h2_sess) return;
	nghttp2_submit_shutdown_notice(sess->h2_sess->ng_sess);
	h2_session_send(sess);

	//commented out as it seems go_away is never set
#if 1
	u64 in_time;
	in_time = gf_sys_clock_high_res();
	while (nghttp2_session_want_read(sess->h2_sess->ng_sess)) {
		u32 res;
		char h2_flush[2024];
		GF_Err e;
		if (gf_sys_clock_high_res() - in_time > 100000)
			break;

		//read any frame pending from remote peer (window update and co)
		e = gf_dm_read_data(sess, h2_flush, 2023, &res);
		if ((e<0) && (e != GF_IP_NETWORK_EMPTY)) {
			if (e!=GF_IP_CONNECTION_CLOSED) {
				SET_LAST_ERR(e)
				sess->status = GF_NETIO_STATE_ERROR;
			}
			break;
		}

		h2_session_send(sess);
	}
#else
	sess->status = GF_NETIO_DISCONNECTED;
#endif

#endif
}

GF_Socket *gf_dm_sess_get_socket(GF_DownloadSession *sess)
{
	return sess ? sess->sock : NULL;
}

void gf_dm_sess_set_sock_group(GF_DownloadSession *sess, GF_SockGroup *sg)
{
	if (sess) {
		if (sess->sock_group!=sg) {
			if (sess->sock_group)
				gf_sk_group_unregister(sess->sock_group, sess->sock);
			if (sg)
				gf_sk_group_register(sg, sess->sock);
		}
		sess->sock_group = sg;
	}
}

void gf_dm_sess_detach_async(GF_DownloadSession *sess)
{
	if (sess->sock) {
		while (sess->async_buf_size) {
			gf_dm_sess_flush_async(sess, GF_FALSE);
			gf_sleep(1);
		}
		gf_sk_set_block_mode(sess->sock, GF_TRUE);
	}
	sess->flags |= GF_NETIO_SESSION_NO_BLOCK;
	//FOR TEST INLY
	sess->flags &= ~GF_NETIO_SESSION_NOT_THREADED;
	//mutex may already be created for H2 sessions
	if (!sess->mx)
		sess->mx = gf_mx_new(sess->orig_url);
}

void gf_dm_sess_set_timeout(GF_DownloadSession *sess, u32 timeout)
{
	if (sess) sess->request_timeout = 1000*timeout;
}

void gf_dm_sess_set_netcap_id(GF_DownloadSession *sess, const char *netcap_id)
{
	if (sess) sess->netcap_id = netcap_id;
}


//end GAPC_DISABLE_NETWORK
#elif defined(GPAC_CONFIG_EMSCRIPTEN)
#include <gpac/download.h>
#include <gpac/list.h>
#include <gpac/thread.h>
#include <gpac/filters.h>

struct __gf_download_manager
{
	GF_FilterSession *fsess;
	GF_List *all_sessions;
	GF_Mutex *mx;
};

GF_DownloadManager *gf_dm_new(GF_DownloadFilterSession *fsess)
{
	GF_DownloadManager *tmp;
	GF_SAFEALLOC(tmp, GF_DownloadManager);
	if (!tmp) return NULL;
	tmp->fsess = fsess;
	tmp->all_sessions = gf_list_new();
	tmp->mx = gf_mx_new("cache");
	return tmp;
}
void gf_dm_del(GF_DownloadManager *dm)
{
	if (!dm) return;
	gf_list_del(dm->all_sessions);
	gf_mx_del(dm->mx);
	gf_free(dm);
}

u32 gf_dm_get_global_rate(GF_DownloadManager *dm)
{
	return 0;
}
void gf_dm_set_data_rate(GF_DownloadManager *dm, u32 rate_in_bits_per_sec)
{
}

typedef struct __cache_blob
{
	GF_Blob blob;
	char *url, *cache_name;
	char *mime;
	u64 start_range, end_range;
	Bool persistent;
} GF_CacheBlob;

struct __gf_download_session
{
	GF_DownloadManager *dm;
	gf_dm_user_io user_io;
	void *usr_cbk;
	char method[100];

	char *req_url;
	char *rsp_url;
	/*request headers*/
	char **req_hdrs;
	u32 nb_req_hdrs;
	u32 req_body_size;
	char *req_body;
	u64 start_range, end_range;

	/*response headers*/
	char **rsp_hdrs;
	u32 nb_rsp_hdrs;
	char *mime; //pointer to content-type header or blob

	u32 state, bps;
	u64 start_time, start_time_utc;
	u64 total_size, bytes_done;
	GF_Err last_error;
	GF_NetIOStatus netio_status;
	u32 dl_flags;
	Bool reuse_cache;
	u32 ftask;
	GF_List *cached_blobs;

	GF_CacheBlob *active_cache;

	Bool destroy, in_callback;

};

EM_JS(int, dm_fetch_cancel, (int sess), {
  let fetcher = libgpac._get_fetcher(sess);
  if (!fetcher) return -1;
  fetcher._controller.abort();
  libgpac._del_fetcher(fetcher);
  return 0;
});

static void clear_headers(char ***_hdrs, u32 *nb_hdrs)
{
	char **hdrs = *_hdrs;
	if (hdrs) {
		u32 i=0;
		for (i=0; i<*nb_hdrs; i++) {
			if (hdrs[i]) gf_free(hdrs[i]);
		}
		gf_free(hdrs);
		*_hdrs = NULL;
		*nb_hdrs = 0;
	}
}

void cache_blob_del(GF_CacheBlob *b)
{
	if (!b) return;
	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Removing cache entry of %s (%s - range "LLU"-"LLU")\n", b->url, b->cache_name, b->start_range, b->end_range));
	gf_blob_unregister(&b->blob);
	if (b->blob.data) gf_free(b->blob.data);
	if (b->url) gf_free(b->url);
	if (b->cache_name) gf_free(b->cache_name);
	if (b->mime) gf_free(b->mime);
	gf_free(b);
}

static void gf_dm_sess_reset(GF_DownloadSession *sess)
{
	if (!sess) return;
	clear_headers(&sess->req_hdrs, &sess->nb_req_hdrs);
	clear_headers(&sess->rsp_hdrs, &sess->nb_rsp_hdrs);
	if (sess->req_body) gf_free(sess->req_body);
	sess->req_body = NULL;

	if (sess->state==1) {
		dm_fetch_cancel(EM_CAST_PTR sess);
	}
	if (sess->req_url) gf_free(sess->req_url);
	sess->req_url = NULL;
	if (sess->rsp_url) gf_free(sess->rsp_url);
	sess->rsp_url = NULL;
	sess->mime = NULL;
	sess->state = 0;
	sess->total_size = 0;
	sess->bytes_done = 0;
	sess->start_time = 0;
	sess->start_range = 0;
	sess->end_range = 0;
	sess->bps = 0;
	sess->last_error = GF_OK;
	sess->reuse_cache = GF_FALSE;
	sess->netio_status = GF_NETIO_SETUP;
}


EM_JS(int, fs_fetch_setup, (), {
	if ((typeof libgpac.gpac_fetch == 'boolean') && !libgpac.gpac_fetch) return 2;
	if (typeof libgpac._fetcher_set_header == 'function') return 1;
	try {
		libgpac._fetchers = [];
		libgpac._get_fetcher = (sess) => {
          for (let i=0; i<libgpac._fetchers.length; i++) {
            if (libgpac._fetchers[i].sess==sess) return libgpac._fetchers[i];
          }
          return null;
		};
		libgpac._del_fetcher = (fetcher) => {
          let i = libgpac._fetchers.indexOf(fetcher);
          if (i>=0) libgpac._fetchers.splice(i, 1);
		};
        libgpac._fetcher_set_header = libgpac.cwrap('gf_dm_sess_push_header', null, ['number', 'string', 'string']);
        libgpac._fetcher_set_reply = libgpac.cwrap('gf_dm_sess_async_reply', null, ['number', 'number', 'string']);
	} catch (e) {
		return 0;
	}
	return 1;
});

static GF_Err gf_dm_setup_cache(GF_DownloadSession *sess)
{
	if (sess->active_cache) {
		if (!sess->cached_blobs) {
			cache_blob_del(sess->active_cache);
		}
		else if (sess->active_cache->blob.flags & GF_BLOB_CORRUPTED) {
			gf_list_del_item(sess->cached_blobs, sess->active_cache);
			cache_blob_del(sess->active_cache);
		}
		//remove if not persistent
		else if (!sess->active_cache->persistent) {
			gf_list_del_item(sess->cached_blobs, sess->active_cache);
			cache_blob_del(sess->active_cache);
		}
		sess->active_cache = NULL;
	}
	//look in session for cache
	if (sess->cached_blobs) {
		u32 i, count = gf_list_count(sess->cached_blobs);
		for (i=0; i<count; i++) {
			GF_CacheBlob *cb = gf_list_get(sess->cached_blobs, i);
			if (strcmp(cb->url, sess->req_url)) continue;
			if (cb->start_range != sess->start_range) continue;
			if (cb->end_range != sess->end_range) continue;
			if (!cb->blob.size) continue;
			sess->active_cache = cb;
			sess->reuse_cache = GF_TRUE;
			sess->total_size = cb->blob.size;
			sess->bytes_done = 0;
			sess->netio_status = GF_NETIO_DATA_EXCHANGE;
			sess->last_error = GF_OK;
			sess->mime = cb->mime;
			sess->dl_flags &= ~GF_NETIO_SESSION_KEEP_FIRST_CACHE;
			return GF_OK;
		}
	}
	if (!(sess->dl_flags & GF_NETIO_SESSION_MEMORY_CACHE)) {
		return GF_OK;
	}

	GF_SAFEALLOC(sess->active_cache, GF_CacheBlob);
	if (!sess->active_cache) return GF_OUT_OF_MEM;
	if (!sess->cached_blobs) sess->cached_blobs = gf_list_new();

	gf_list_add(sess->cached_blobs, sess->active_cache);
	sess->active_cache->cache_name = gf_blob_register(&sess->active_cache->blob);

	sess->active_cache->blob.flags = GF_BLOB_IN_TRANSFER;
	sess->active_cache->url = gf_strdup(sess->req_url);
	sess->active_cache->start_range = sess->start_range;
	sess->active_cache->end_range = sess->end_range;
	sess->reuse_cache = GF_FALSE;
	//only files marked with GF_NETIO_SESSION_KEEP_FIRST_CACHE are kept in our local cache
	//all other cache settings are ignored, we rely on browser cache for this
	if (sess->dl_flags & GF_NETIO_SESSION_KEEP_FIRST_CACHE) {
		sess->active_cache->persistent = GF_TRUE;
		sess->dl_flags &= ~GF_NETIO_SESSION_KEEP_FIRST_CACHE;
	} else {
		sess->active_cache->persistent = GF_FALSE;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Setting up memory cache for %s - persistent %d - name %s\n", sess->req_url, sess->active_cache->persistent, sess->active_cache->cache_name));
	return GF_OK;
}

GF_EXPORT
GF_DownloadSession *gf_dm_sess_new(GF_DownloadManager *dm, const char *url, u32 dl_flags,
                                   gf_dm_user_io user_io,
                                   void *usr_cbk,
                                   GF_Err *e)
{
	GF_NETIO_Parameter par;
	GF_DownloadSession *sess;

	int fetch_init = fs_fetch_setup();
	if (fetch_init==2) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[Downloader] fecth() disabled\n"));
		if (e) *e = GF_NOT_SUPPORTED;
		return NULL;
	} else if (fetch_init==0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[Downloader] Failed to initialize fetch\n"));
		if (e) *e = GF_SERVICE_ERROR;
		return NULL;
	}

	GF_SAFEALLOC(sess, GF_DownloadSession);
	if (!sess) {
		if (e) *e = GF_OUT_OF_MEM;
		return NULL;
	}
	sess->dm = dm;
	sess->req_url = gf_strdup(url);
	if (!sess->req_url) {
		gf_free(sess);
		if (e) *e = GF_OUT_OF_MEM;
		return NULL;
	}
	sess->user_io = user_io;
	sess->usr_cbk = usr_cbk;

	strcpy(sess->method, "GET");

	sess->dl_flags = dl_flags;

	if (dl_flags & GF_NETIO_SESSION_MEMORY_CACHE) {
		sess->cached_blobs = gf_list_new();
	}

	if (user_io) {
		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = GF_NETIO_GET_METHOD;
		sess->user_io(sess->usr_cbk, &par);
		if (par.name) strcpy(sess->method, par.name);

		sess->nb_req_hdrs=0;
		while (1) {
			par.msg_type = GF_NETIO_GET_HEADER;
			par.value = NULL;
			sess->user_io(sess->usr_cbk, &par);
			if (!par.value) break;

			sess->req_hdrs = gf_realloc(sess->req_hdrs, sizeof(char*) * (sess->nb_req_hdrs+2));
			if (!sess->req_hdrs) {
				if (e) *e = GF_OUT_OF_MEM;
				gf_free(sess);
				return NULL;
			}
			sess->req_hdrs[sess->nb_req_hdrs] = gf_strdup(par.name);
			sess->req_hdrs[sess->nb_req_hdrs+1] = gf_strdup(par.value);
			sess->nb_req_hdrs+=2;
		}

		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = GF_NETIO_GET_CONTENT;
		sess->user_io(sess->usr_cbk, &par);

		sess->req_body_size = par.size;
		if (par.size && par.data) {
			sess->req_body = gf_malloc(sizeof(u8)*par.size);
			if (!sess->req_body) {
				gf_dm_sess_reset(sess);
				gf_list_del(sess->cached_blobs);
				if (e) *e = GF_OUT_OF_MEM;
				gf_free(sess);
				return NULL;
			}
			memcpy(sess->req_body, par.data, par.size);
		}
	}
	if (e) *e = GF_OK;
	*e = gf_dm_setup_cache(sess);
	if (*e) {
		gf_dm_sess_del(sess);
		return NULL;
	}
	if (dm) {
		gf_mx_p(dm->mx);
		gf_list_add(dm->all_sessions, sess);
		gf_mx_v(dm->mx);
	}

	sess->user_io = user_io;
	sess->usr_cbk = usr_cbk;

	return sess;
}

void gf_dm_sess_abort(GF_DownloadSession * sess)
{
	if (sess->state==1) {
		dm_fetch_cancel(EM_CAST_PTR sess);
		sess->state = 0;
		sess->netio_status = GF_NETIO_DISCONNECTED;
		sess->last_error = GF_IP_CONNECTION_CLOSED;
		if (sess->active_cache && !sess->reuse_cache && (sess->active_cache->blob.flags & GF_BLOB_IN_TRANSFER)) {
			sess->active_cache->blob.flags = GF_BLOB_CORRUPTED;
		}
	}
	clear_headers(&sess->rsp_hdrs, &sess->nb_rsp_hdrs);
	if (sess->ftask) sess->ftask = 2;
}

void gf_dm_sess_del(GF_DownloadSession *sess)
{
	if (sess->in_callback) {
		sess->destroy = GF_TRUE;
		return;
	}
	gf_dm_sess_reset(sess);
	while (gf_list_count(sess->cached_blobs)) {
		cache_blob_del( gf_list_pop_back(sess->cached_blobs) );
	}
	gf_list_del(sess->cached_blobs);
	if (sess->dm) {
		gf_mx_p(sess->dm->mx);
		gf_list_del_item(sess->dm->all_sessions, sess);
		gf_mx_v(sess->dm->mx);
	}
	gf_free(sess);
}

const char *gf_dm_sess_get_cache_name(GF_DownloadSession *sess)
{
	if (!sess || !sess->active_cache) return NULL;
	return sess->active_cache->cache_name;
}

void gf_dm_sess_force_memory_mode(GF_DownloadSession *sess, u32 force_keep)
{
	if (!sess) return;
	sess->dl_flags |= GF_NETIO_SESSION_MEMORY_CACHE;
	if (force_keep==1) {
		//cache is handled by the browser if any, or forced by fetch headers provied by users
		//sess->dl_flags |= GF_NETIO_SESSION_KEEP_CACHE;
	} else if (force_keep==2) {
		sess->dl_flags |= GF_NETIO_SESSION_KEEP_FIRST_CACHE;
	}
}

GF_Err gf_dm_sess_setup_from_url(GF_DownloadSession *sess, const char *url, Bool allow_direct_reuse)
{
	gf_dm_sess_reset(sess);
	sess->req_url = gf_strdup(url);
	if (!sess->req_url) {
		sess->state=2;
		return GF_OUT_OF_MEM;
	}
	return gf_dm_setup_cache(sess);
}

GF_Err gf_dm_sess_set_range(GF_DownloadSession *sess, u64 start_range, u64 end_range, Bool discontinue_cache)
{
	if (!sess) return GF_BAD_PARAM;
	if (sess->state==1) return GF_BAD_PARAM;
	sess->start_range = start_range;
	sess->end_range = end_range;
	Bool cache_reset = GF_FALSE;
	if ((sess->state!=2) || discontinue_cache) {
		cache_reset = GF_TRUE;
	}
	else if (sess->active_cache) {
		if (sess->active_cache->start_range + sess->active_cache->end_range + 1 == start_range) {
			sess->active_cache->end_range = end_range;
			sess->active_cache->blob.flags |= GF_BLOB_IN_TRANSFER;
		} else {
			cache_reset = GF_TRUE;
		}
	}
	//set range called before starting session, just adjust cache
	if (sess->active_cache && cache_reset && !sess->state) {
		sess->active_cache->start_range = sess->start_range;
		sess->active_cache->end_range = sess->end_range;
		cache_reset=GF_FALSE;
	}
	if (cache_reset) {
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Cache discontinuity, reset\n", sess->req_url, sess->active_cache->persistent, sess->active_cache->cache_name));
		GF_Err e = gf_dm_setup_cache(sess);
		if (e) return e;
		sess->last_error = GF_OK;
	}
	sess->netio_status = GF_NETIO_CONNECTED;
	sess->state = 0;
	return GF_OK;
}



EM_JS(int, dm_fetch_init, (int sess, int _url, int _method, int _headers, int nb_headers, int req_body, int req_body_size), {
  let url = _url ? libgpac.UTF8ToString(_url) : null;
  ret = GPAC.OK;

  let fetcher = libgpac._get_fetcher(sess);
  if (!fetcher) {
	fetcher = {};
	fetcher.sess = sess;
	libgpac._fetchers.push(fetcher);
  }

  fetcher._controller = new AbortController();
  let options = {
	signal: fetcher._controller.signal,
	method: _method ? libgpac.UTF8ToString(_method) : "GET",
	mode: libgpac.gpac_fetch_mode || 'cors',
  };
  let mime_type = 'application/octet-stream';
  options.headers = {};
  if (_headers) {
	for (let i=0; i<nb_headers; i+=2) {
	  let _s = libgpac.getValue(_headers+i*4, 'i32');
	  let h_name = _s ? libgpac.UTF8ToString(_s).toLowerCase() : "";
	  _s = libgpac.getValue(_headers+(i+1)*4, 'i32');
	  let h_val = _s ? libgpac.UTF8ToString(_s) : "";
	  if (h_name.length && h_val.length) {
		  options.headers[h_name] = h_val;
		  if (h_name=='content-type')
			mime_type = h_val;
	  }
	}
  }
  if (typeof libgpac.gpac_extra_headers == 'object') {
	for (const hdr in libgpac.gpac_extra_headers) {
	  options.headers[hdr] = object[hdr];
	}
  }

  if (req_body) {
	let body_ab = new Uint8Array(libgpac.HEAPU8.buffer, req_body, req_body_size);
	options.body = new Blob(body_ab, {type: mime_type} );
  }
  fetcher._state = 0;
  fetch(url, options).then( (response) => {
	if (response.ok) {
	  fetcher._state = 1;
	  fetcher._bytes = 0;
	  fetcher._reader = response.body.getReader();

	  let final_url = null;
	  if (response.redirected) final_url = response.url;
	  libgpac._fetcher_set_reply(fetcher.sess, response.status, final_url);

	  response.headers.forEach((value, key) => {
		libgpac._fetcher_set_header(fetcher.sess, key, value);
	  });
      libgpac._fetcher_set_header(fetcher.sess, 0, 0);
	} else {
	  libgpac._fetcher_set_reply(fetcher.sess, response.status, null);
	  do_log_err('fetcher for ' + url + ' failed ' + response.status);
	  fetcher._state = 3;
	}
  })
  .catch( (e) => {
	  do_log_err('fetcher exception ' + e);
	  ret = GPAC.REMOTE_SERVICE_ERROR;
	  libgpac._del_fetcher(fetcher);
  });
  return ret;
});


EM_JS(int, dm_fetch_data, (int sess, int buffer, int buffer_size, int read_size), {
  let f = libgpac._get_fetcher(sess);
  if (!f) return -1;
  if (f._state==0) return -44;
  if (f._state==3) return -12;
  if (f._state==4) return 1;
  if (f._state == 1) {
	f._state = 0;
	f._reader.read().then( (block) => {
		if (block.done) {
			f._state = 4;
		} else {
			f._ab = block.value;
			f._bytes += f._ab.byteLength;
			f._block_pos = 0;
			f._state = 2;
		}
	})
	.catch( e => {
		f._state = 0;
	});
	if (f._state==0) return -44;
  }

  let avail = f._ab.byteLength - f._block_pos;
  if (avail<buffer_size) buffer_size = avail;

  //copy array buffer
  let src = f._ab.subarray(f._block_pos, f._block_pos+buffer_size);
  let dst = new Uint8Array(libgpac.HEAPU8.buffer, buffer, buffer_size);
  dst.set(src);

  libgpac.setValue(read_size, buffer_size, 'i32');
  f._block_pos += buffer_size;
  if (f._ab.byteLength == f._block_pos) {
	  f._ab = null;
	  f._state = 1;
  }
  return 0;
});


GF_Err gf_dm_sess_fetch_data(GF_DownloadSession *sess, char *buffer, u32 buffer_size, u32 *read_size)
{
	if (sess->reuse_cache) {
		u32 remain = sess->active_cache->blob.size - sess->bytes_done;
		if (!remain) {
			*read_size = 0;
			sess->last_error = GF_EOS;
			sess->netio_status = GF_NETIO_DATA_TRANSFERED;
			return GF_EOS;
		}
		if (remain < buffer_size) buffer_size = remain;
		memcpy(buffer, sess->active_cache->blob.data + sess->bytes_done, buffer_size);
		*read_size = buffer_size;
		sess->bytes_done += buffer_size;
		return GF_OK;
	}
	if (sess->state == 0) {
		if (sess->start_range || sess->end_range) {
			char szHdr[100];
			if (sess->end_range)
				sprintf(szHdr, "bytes="LLU"-"LLU, sess->start_range, sess->end_range);
			else
				sprintf(szHdr, "bytes="LLU"-", sess->start_range);

			sess->req_hdrs = gf_realloc(sess->req_hdrs, sizeof(char*) * (sess->nb_req_hdrs+2));
			if (!sess->req_hdrs) {
				sess->state = 2;
				return sess->last_error = GF_OUT_OF_MEM;
			}
			sess->req_hdrs[sess->nb_req_hdrs] = gf_strdup("Range");
			sess->req_hdrs[sess->nb_req_hdrs+1] = gf_strdup(szHdr);
			sess->nb_req_hdrs+=2;
		}

		sess->start_time_utc = gf_net_get_utc();
		sess->start_time = gf_sys_clock();
		sess->netio_status = GF_NETIO_CONNECTED;

		if (sess->start_range || sess->end_range) {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Starting download of %s (%s - range "LLU"-"LLU")\n", sess->req_url, sess->active_cache ? sess->active_cache->cache_name : "no cache", sess->start_range, sess->end_range));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Starting download of %s (%s)\n", sess->req_url, sess->active_cache ? sess->active_cache->cache_name : "no cache"));
		}
		GF_Err fetch_e = dm_fetch_init(
			EM_CAST_PTR sess,
			EM_CAST_PTR sess->req_url,
			EM_CAST_PTR sess->method,
			EM_CAST_PTR sess->req_hdrs,
			sess->nb_req_hdrs,
			EM_CAST_PTR sess->req_body,
			sess->req_body_size
		);

		if (fetch_e) {
			gf_dm_sess_reset(sess);
			sess->state = 2;
			sess->netio_status = GF_NETIO_DISCONNECTED;
			return sess->last_error = fetch_e;
		}
		sess->state = 1;
		sess->netio_status = GF_NETIO_WAIT_FOR_REPLY;
	}
	if (sess->state==2) return sess->last_error;

	*read_size = 0;
	GF_Err fetch_err = dm_fetch_data(
		EM_CAST_PTR sess,
		EM_CAST_PTR buffer,
		buffer_size,
		EM_CAST_PTR read_size
	);

	if (*read_size) {
		u64 ellapsed = gf_sys_clock() - sess->start_time;
		sess->bytes_done += *read_size;
		if (ellapsed)
			sess->bps = (u32) (sess->bytes_done * 1000 / ellapsed);
		sess->netio_status = GF_NETIO_DATA_EXCHANGE;

		if (sess->active_cache) {
			u32 nb_bytes= *read_size;
			sess->active_cache->blob.data = gf_realloc(sess->active_cache->blob.data, sess->active_cache->blob.size + nb_bytes);
			if (!sess->active_cache->blob.data) {
				sess->active_cache->blob.flags |= GF_BLOB_CORRUPTED;
			} else {
				memcpy(sess->active_cache->blob.data + sess->active_cache->blob.size, buffer, nb_bytes);
				sess->active_cache->blob.size += nb_bytes;
			}
		}
		if (sess->total_size == sess->bytes_done) fetch_err = GF_EOS;

	} else if (!fetch_err) {
		return GF_IP_NETWORK_EMPTY;
	} else if (fetch_err!=GF_IP_NETWORK_EMPTY) {
		sess->state=2;
		sess->last_error = fetch_err;
		sess->netio_status = GF_NETIO_STATE_ERROR;
		if (sess->active_cache)
			sess->active_cache->blob.flags = GF_BLOB_CORRUPTED;
	}

	if (fetch_err==GF_EOS) {
		sess->total_size = sess->bytes_done;
		sess->state=2;
		sess->last_error = GF_EOS;
		sess->netio_status = GF_NETIO_DATA_TRANSFERED;
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] Done downloading %s - rate %d bps\n", sess->req_url, sess->bps));
		if (sess->active_cache)
			sess->active_cache->blob.flags &= ~GF_BLOB_IN_TRANSFER;
	}
	return fetch_err;
}


GF_Err gf_dm_sess_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u64 *total_size, u64 *bytes_done, u32 *bytes_per_sec, GF_NetIOStatus *net_status)
{
	if (!sess) return GF_OUT_OF_MEM;
	if (total_size) *total_size = sess->total_size;
	if (bytes_done) *bytes_done = sess->bytes_done;
	if (bytes_per_sec) *bytes_per_sec = sess->bps;
	if (net_status) *net_status = sess->netio_status;
	return sess->last_error;
}

const char *gf_dm_sess_mime_type(GF_DownloadSession * sess)
{
	return sess->mime;
}

GF_Err gf_dm_sess_enum_headers(GF_DownloadSession *sess, u32 *idx, const char **hdr_name, const char **hdr_val)
{
	if( !sess || !idx || !hdr_name || !hdr_val)
		return GF_BAD_PARAM;
	if (*idx >= sess->nb_rsp_hdrs/2) return GF_EOS;

	u32 i = *idx * 2;
	(*hdr_name) = sess->rsp_hdrs[i];
	(*hdr_val) = sess->rsp_hdrs[i+1];
	(*idx) = (*idx) + 1;
	return GF_OK;
}

void gf_dm_delete_cached_file_entry_session(const GF_DownloadSession * sess, const char * url, Bool force)
{
	//unused we automatically purge cache when setting up session
	if (!sess || !force) return;
	if (sess->state==1) return;
	if (sess->active_cache) {
		gf_list_del_item(sess->cached_blobs, sess->active_cache);
		cache_blob_del(sess->active_cache);
		((GF_DownloadSession *)sess)->active_cache = NULL;
	}
}

GF_Err gf_dm_sess_process_headers(GF_DownloadSession *sess)
{
	//no control of this for fetch
	return GF_OK;
}

GF_EXPORT
void gf_dm_sess_push_header(GF_DownloadSession *sess, const char *hdr, const char *value)
{
	if (!sess) return;
	if (!hdr || !value) return;

	if (!stricmp(hdr, "Content-Length")) {
		sscanf(value, LLU, &sess->total_size);
	}

	sess->rsp_hdrs = gf_realloc(sess->rsp_hdrs, sizeof(char*) * (sess->nb_rsp_hdrs+2));
	if (!sess->rsp_hdrs) return;

	sess->rsp_hdrs[sess->nb_rsp_hdrs] = gf_strdup(hdr);
	sess->rsp_hdrs[sess->nb_rsp_hdrs+1] = gf_strdup(value);
	if (!stricmp(hdr, "Content-Type")) {
		sess->mime = sess->rsp_hdrs[sess->nb_rsp_hdrs+1];
		//keep a copy of mime
		if (sess->active_cache && sess->active_cache->persistent)
			sess->active_cache->mime = gf_strdup(sess->mime);
	}
	sess->nb_rsp_hdrs+=2;
}

GF_EXPORT
void gf_dm_sess_async_reply(GF_DownloadSession *sess, int rsp_code, const char *final_url)
{
	if (!sess) return;
	GF_Err e;
	switch (rsp_code) {
	case 404: e = GF_URL_ERROR; break;
	case 400: e = GF_SERVICE_ERROR; break;
	case 401: e = GF_AUTHENTICATION_FAILURE; break;
	case 405: e = GF_AUTHENTICATION_FAILURE; break;
	case 416: e = GF_SERVICE_ERROR; break; //range error
	default:
		if (rsp_code>=500) e = GF_REMOTE_SERVICE_ERROR;
		else if (rsp_code>=400) e = GF_SERVICE_ERROR;
		else e = GF_OK;
		break;
	}
	sess->last_error = e;

	if (final_url) {
		if (sess->rsp_url) gf_free(sess->rsp_url);
		sess->rsp_url = final_url ? gf_strdup(final_url) : NULL;
	}
}

GF_Err gf_dm_sess_fetch_once(GF_DownloadSession *sess)
{
	GF_NETIO_Parameter par;
	char buffer[10000];
	u32 read = 0;
	GF_Err e = gf_dm_sess_fetch_data(sess, buffer, 10000, &read);
	if (e==GF_IP_NETWORK_EMPTY) {

	} else if ((e==GF_OK) || (e==GF_EOS)) {
		if (sess->user_io && read) {
			memset(&par, 0, sizeof(GF_NETIO_Parameter));
			par.sess = sess;
			par.error = sess->last_error;
			if (e) {
				par.msg_type = GF_NETIO_DATA_TRANSFERED;
			} else {
				par.msg_type = GF_NETIO_DATA_EXCHANGE;
				par.data = buffer;
				par.size = read;
			}
			sess->in_callback = GF_FALSE;
			sess->user_io(sess->usr_cbk, &par);
			sess->in_callback = GF_FALSE;
		}
	} else {
		if (sess->user_io) {
			memset(&par, 0, sizeof(GF_NETIO_Parameter));
			par.sess = sess;
			par.msg_type = sess->netio_status;
			par.error = sess->last_error;
			sess->in_callback = GF_TRUE;
			sess->user_io(sess->usr_cbk, &par);
			sess->in_callback = GF_FALSE;
		}
	}
	return e;
}

Bool gf_dm_session_task(GF_FilterSession *fsess, void *callback, u32 *reschedule_ms)
{
	Bool ret = GF_TRUE;
	GF_DownloadSession *sess = callback;
	if (!sess) return GF_FALSE;
	assert(sess->ftask);
	if (sess->ftask==2) {
		sess->ftask = 0;
		return GF_FALSE;
	}

	GF_Err e = gf_dm_sess_fetch_once(sess);
	if (e==GF_EOS) ret = GF_FALSE;
	else if (e && (e!=GF_IP_NETWORK_EMPTY)) ret = GF_FALSE;

	if (ret) {
		*reschedule_ms = 1;
		return GF_TRUE;
	}
	sess->ftask = 0;
	if (sess->destroy) {
		sess->destroy = GF_FALSE;
		gf_dm_sess_del(sess);
	}
	return GF_FALSE;
}

GF_Err gf_dm_sess_process(GF_DownloadSession *sess)
{
	if (sess->netio_status == GF_NETIO_DATA_TRANSFERED) {
		return GF_OK;
	}

	if ((sess->dl_flags & GF_NETIO_SESSION_NOT_THREADED) || !sess->dm->fsess) {
		return gf_dm_sess_fetch_once(sess);
	}
	//ignore if task already allocated
	if (sess->ftask) return GF_OK;
	sess->ftask = GF_TRUE;
	return gf_fs_post_user_task(sess->dm->fsess, gf_dm_session_task, sess, "download");
}


const char *gf_dm_sess_get_resource_name(GF_DownloadSession *sess)
{
	if (!sess) return NULL;
	return sess->rsp_url ? sess->rsp_url : sess->req_url;
}

const char *gf_dm_sess_get_header(GF_DownloadSession *sess, const char *name)
{
	u32 i;
	if (!sess || !name) return NULL;
	for (i=0; i<sess->nb_rsp_hdrs; i+=2) {
		char *h = sess->rsp_hdrs[i];
		if (!stricmp(h, name)) return sess->rsp_hdrs[i+1];
	}
	return NULL;
}

u64 gf_dm_sess_get_utc_start(GF_DownloadSession * sess)
{
	if (!sess) return 0;
	return sess->start_time_utc;
}

//only used for dash playing local files
u32 gf_dm_get_data_rate(GF_DownloadManager *dm)
{
	return gf_opts_get_int("core", "maxrate");
}

void gf_dm_sess_detach_async(GF_DownloadSession *sess)
{
	if (!sess) return;
	sess->dl_flags &= ~GF_NETIO_SESSION_NOT_THREADED;
	gf_dm_sess_force_memory_mode(sess, 1);
}
void gf_dm_sess_set_netcap_id(GF_DownloadSession *sess, const char *netcap_id)
{

}

#endif // GPAC_CONFIG_EMSCRIPTEN

