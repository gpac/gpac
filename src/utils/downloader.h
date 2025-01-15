/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2025
 *					All rights reserved
 *
 *  This file is part of GPAC / downloader sub-project
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

#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include <gpac/tools.h>

//regular downloader
#ifndef GPAC_DISABLE_NETWORK
#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/token.h>
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/base_coding.h>
#include <gpac/filters.h>
#include <gpac/crypt.h>

#ifdef GPAC_HAS_SSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#ifdef GPAC_HAS_CURL
#include <curl/curl.h>
//we need multi API
#if CURL_AT_LEAST_VERSION(7,80,0)
#ifndef CURLPIPE_MULTIPLEX
#define CURLPIPE_MULTIPLEX 0
#endif
#else
#undef GPAC_HAS_CURL
#endif

#endif //CURL

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
#endif

#ifdef __USE_POSIX
#include <unistd.h>
#endif

#define SIZE_IN_STREAM ( 2 << 29 )


#define SESSION_RETRY_COUNT	10
#define SESSION_RETRY_SSL	8

#include <gpac/revision.h>
#define GF_DOWNLOAD_AGENT_NAME		"GPAC/"GPAC_VERSION "-rev" GPAC_GIT_REVISION

#define GF_DOWNLOAD_BUFFER_SIZE		50000
#define GF_DOWNLOAD_BUFFER_SIZE_LIMIT_RATE		GF_DOWNLOAD_BUFFER_SIZE/20


#if defined(GPAC_HAS_HTTP2) || defined(GPAC_HAS_NGTCP2)
#define GPAC_HTTPMUX
#endif

enum {
	H3_MODE_NO=0,
	H3_MODE_FIRST,
	H3_MODE_AUTO,
	H3_MODE_ONLY,
};

#ifdef GPAC_HTTPMUX
typedef struct
{
	u8 * data;
	u32 size, alloc, offset;
} hmux_reagg_buffer;

typedef struct _http_mux_session
{
	Bool do_shutdown;
	GF_List *sessions;
	GF_DownloadSession *net_sess;
	GF_Mutex *mx;
	Bool copy;
	Bool connected;

	//underying implementation - all callbacks must be set

	GF_Err (*setup_session)(GF_DownloadSession *sess, Bool is_destroy);

	//format request and submit it
	GF_Err (*submit_request)(GF_DownloadSession *sess, char *req_name, const char *url, const char *param_string, Bool has_body);
	//send reply and submit it
	GF_Err (*send_reply)(GF_DownloadSession *sess, u32 reply_code, const char *response_body, u32 body_len, Bool no_body);
	//send any pending packets
	GF_Err (*send)(GF_DownloadSession *sess);
	//send stream reset
	void (*stream_reset)(GF_DownloadSession *sess);
	//move to active state if hmux_data_paused was set
	void (*resume)(GF_DownloadSession *sess);
	//destroy underlying muxed session object
	void (*destroy)(struct _http_mux_session *hmux);
	//notify that some data has been received for the parent muxed session
	GF_Err (*data_received)(GF_DownloadSession *sess, const u8 *data, u32 nb_bytes);
	//underlying muxed session object (nghttp2 session, h3 session, etc...)
	void *hmux_udta;
} GF_HMUX_Session;

#endif

#define GF_NETIO_SESSION_USE_QUIC	(1<<28)
#define GF_NETIO_SESSION_RETRY_QUIC	(1<<29)
#define GF_NETIO_SESSION_NO_STORE	(1<<30)

void gf_dm_data_received(GF_DownloadSession *sess, u8 *payload, u32 payload_size, Bool store_in_init, u32 *rewrite_size, u8 *original_payload);
GF_Err gf_dm_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read);
GF_Err gf_dm_sess_send(GF_DownloadSession *sess, u8 *data, u32 size);
GF_Err gf_dm_sess_flush_async(GF_DownloadSession *sess, Bool no_select);
void gf_dm_sess_set_header_ex(GF_DownloadSession *sess, const char *name, const char *value, Bool allow_overwrite);

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

#ifndef GPAC_DISABLE_LOG
	char *log_name;
#endif
	char *server_name;
	u16 port;

	char *orig_url;
	char *orig_url_before_redirect;
	char *remote_path;
	GF_UserCredentials * creds;
	char cookie[GF_MAX_PATH];
	DownloadedCacheEntry cache_entry;
	Bool reused_cache_entry;
	FILE *cached_file;

	u32 http_buf_size;
	//we alloc http_buf_size+1 for text dump
	char *http_buf;

	//mime type, only used when the session is not cached.
	char *mime_type;
	GF_List *headers;
	u32 rsp_code;

	const char *netcap_id;
	GF_Socket *sock;
	u32 num_retry;
	GF_NetIOStatus status;

	u32 flags;
	u32 total_size, bytes_done, icy_metaint, icy_count, icy_bytes, full_resource_size;
	u64 start_time;
	u32 nb_redirect;

	u32 bytes_per_sec;
	u32 max_data_rate;
	u64 start_time_utc;
	Bool rate_regulated;
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

#ifdef GPAC_HTTPMUX
	//Muxed HTTP (H/2 or H/3) session used by this download session. If not NULL, the mutex, socket and ssl context are moved along sessions
	GF_HMUX_Session *hmux_sess;
	hmux_reagg_buffer hmux_buf;

	s64 hmux_stream_id;
	u8 hmux_headers_seen, hmux_ready_to_send, hmux_is_eos, hmux_data_paused, hmux_data_done, hmux_switch_sess;
	u8 *hmux_send_data;
	u32 hmux_send_data_len;

	//per-hmux session user data
	void *hmux_priv;

#endif

#ifdef GPAC_HAS_HTTP2
	//HTTP2 only
	u8 *h2_upgrade_settings;
	u32 h2_upgrade_settings_len;
	//0: upgrade not started; 1: upgrade headers send; 2: upgrade done; 3: upgrade retry without SSL APN; 4: H2 disabled for session
	u8 h2_upgrade_state;
#endif

#ifdef GPAC_HAS_CURL
	CURL *curl_hnd;
	GF_Err curl_closed;
	Bool curl_hnd_registered;
	struct curl_slist *curl_hdrs;
	Bool curl_not_http;
	u32 curl_fake_hcode;
#endif

#if defined(GPAC_HTTPMUX) || defined(GPAC_HAS_CURL)
	u8 *local_buf;
	u32 local_buf_len, local_buf_alloc;
#endif

};

#define PUSH_HDR(_name, _value) {\
		GF_SAFEALLOC(hdr, GF_HTTPHeader)\
		hdr->name = gf_strdup(_name);\
		hdr->value = gf_strdup(_value);\
		gf_list_add(sess->headers, hdr);\
	}




GF_Err dm_sess_write(GF_DownloadSession *sess, const u8 *buffer, u32 size);
GF_Err gf_ssl_write(GF_DownloadSession *sess, const u8 *buffer, u32 size, u32 *written);

struct __gf_download_manager
{
	//mutex protecting cache access and all_sessions list
	GF_Mutex *cache_mx;
	char *cache_directory;

	gf_dm_get_usr_pass get_user_password;
	void *usr_cbk;

	GF_List *all_sessions;
	Bool disable_cache, simulate_no_connection, allow_offline_cache;
	u32 limit_data_rate, read_buf_size;
	u64 max_cache_size, cur_cache_size;
	Bool allow_broken_certificate;
	u32 next_cache_clean, cache_clean_ms;

	GF_List *skip_proxy_servers;
	GF_List *credentials;
	GF_List *cache_entries;
	/* FIXME : should be placed in DownloadedCacheEntry maybe... */
	GF_List *partial_downloads;
#ifdef GPAC_HAS_SSL
	SSL_CTX *ssl_ctx;
#endif

	GF_FilterSession *filter_session;

	Bool disable_http2;
	u32 h3_mode;


	Bool (*local_cache_url_provider_cbk)(void *udta, char *url, Bool cache_destroy);
	void *lc_udta;


#ifdef GPAC_HAS_CURL
	CURLM *curl_multi;
	GF_Mutex *curl_mx;
	Bool curl_has_close;
#endif

};

#if 0
#define SET_LAST_ERR(_e) \
	{\
	GF_Err __e = _e;\
	if (__e==GF_IP_NETWORK_EMPTY) {\
		gf_assert(sess->status != GF_NETIO_STATE_ERROR); \
	}\
	sess->last_error = _e;\
	}\

#else

#define SET_LAST_ERR(_e) sess->last_error = _e;

#endif

void dm_sess_sk_del(GF_DownloadSession *sess);

void gf_dm_sess_clear_headers(GF_DownloadSession *sess);

GF_DownloadSession *gf_dm_sess_new_internal(GF_DownloadManager * dm, const char *url, u32 dl_flags,
        gf_dm_user_io user_io, void *usr_cbk, GF_Socket *server, GF_Err *e);


//cache functions - NOT exported, only downloader has access to it
s32 gf_cache_remove_session_from_cache_entry(DownloadedCacheEntry entry, GF_DownloadSession * sess);
Bool gf_cache_set_mime(const DownloadedCacheEntry entry, const char *mime);
Bool gf_cache_set_range(const DownloadedCacheEntry entry, u64 size, u64 start_range, u64 end_range);
Bool gf_cache_set_content(const DownloadedCacheEntry entry, GF_Blob *blob, Bool copy, GF_Mutex *mx);
Bool gf_cache_set_headers(const DownloadedCacheEntry entry, const char *headers);
void gf_cache_set_downtime(const DownloadedCacheEntry entry, u32 download_time_ms);
void gf_cache_set_max_age(const DownloadedCacheEntry entry, u32 max_age, Bool must_revalidate);
Bool gf_cache_entry_persistent(const DownloadedCacheEntry entry);
void gf_cache_delete_entry( const DownloadedCacheEntry entry );
GF_Err gf_cache_set_etag_on_server(const DownloadedCacheEntry entry, const char * eTag );
const char * gf_cache_get_mime_type(const DownloadedCacheEntry entry );
GF_Err gf_cache_set_mime_type(const DownloadedCacheEntry entry, const char * mime_type );
const char * gf_cache_get_url(const DownloadedCacheEntry entry );
GF_Err gf_cache_set_last_modified_on_server(const DownloadedCacheEntry entry, const char * newLastModified );
const char * gf_cache_get_cache_filename(const DownloadedCacheEntry entry );
u32 gf_cache_get_cache_filesize(const DownloadedCacheEntry entry);
GF_Err gf_cache_set_content_length(const DownloadedCacheEntry entry, u32 length );
u32 gf_cache_get_content_length(const DownloadedCacheEntry entry);
GF_Err gf_cache_get_http_headers(const DownloadedCacheEntry entry, const char **etag, const char **last_modif);
u64 gf_cache_cleanup(const char * directory, u32 max_size);
void gf_cache_entry_set_delete_files_when_deleted(const DownloadedCacheEntry entry);
u64 gf_cache_get_start_range( const DownloadedCacheEntry entry );
u64 gf_cache_get_end_range( const DownloadedCacheEntry entry );
Bool gf_cache_are_headers_processed(const DownloadedCacheEntry entry);
GF_Err gf_cache_set_headers_processed(const DownloadedCacheEntry entry);
FILE *gf_cache_open_read(const DownloadedCacheEntry entry);
Bool gf_cache_is_mem(const DownloadedCacheEntry entry);
GF_Err gf_cache_write_to_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, const char * data, const u32 size, GF_Mutex *mx);
GF_Err gf_cache_close_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, Bool success);
GF_Err gf_cache_open_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess );
void gf_cache_set_end_range(DownloadedCacheEntry entry, u64 range_end);
Bool gf_cache_is_in_progress(const DownloadedCacheEntry entry);
char *gf_cache_get_forced_headers(const DownloadedCacheEntry entry);
u32 gf_cache_get_downtime(const DownloadedCacheEntry entry);
u32 gf_cache_is_done(const DownloadedCacheEntry entry);
Bool gf_cache_is_deleted(const DownloadedCacheEntry entry);
const u8 *gf_cache_get_content(const DownloadedCacheEntry entry, u32 *size, u32 *max_valid_size, Bool *was_modified);
void gf_cache_release_content(const DownloadedCacheEntry entry);
void gf_cache_remove_entry_from_session(GF_DownloadSession * sess);



void gf_dm_sess_notify_state(GF_DownloadSession *sess, GF_NetIOStatus dnload_status, GF_Err error);
void gf_dm_sess_set_header_ex(GF_DownloadSession *sess, const char *name, const char *value, Bool allow_overwrite);
void gf_dm_sess_set_header(GF_DownloadSession *sess, const char *name, const char *value);
void gf_dm_sess_reload_cached_headers(GF_DownloadSession *sess);

GF_Err gf_dm_sess_send_reply(GF_DownloadSession *sess, u32 reply_code, const char *response_body, u32 body_len, Bool no_body);



typedef enum
{
	HTTP_NO_CLOSE=0,
	HTTP_CLOSE,
	HTTP_RESET_CONN,
} HTTPCloseType;
void gf_dm_disconnect(GF_DownloadSession *sess, HTTPCloseType close_type);


GF_Err gf_dm_get_url_info(const char * url, GF_URL_Info * info, const char * baseURL);
void http_do_requests(GF_DownloadSession *sess);

void gf_dm_configure_cache(GF_DownloadSession *sess);

//multiplexed HTTP support (H2, H3)
#ifdef GPAC_HTTPMUX
GF_DownloadSession *hmux_get_session(void *user_data, s64 stream_id, Bool can_reassign);
void hmux_detach_session(GF_HMUX_Session *hmux_sess, GF_DownloadSession *sess);
void hmux_fetch_flush_data(GF_DownloadSession *sess, u8 *obuffer, u32 size, u32 *nb_bytes);

void hmux_flush_send(GF_DownloadSession *sess, Bool flush_local_buf);
void hmux_flush_internal_data(GF_DownloadSession *sess, Bool store_in_init);
GF_Err hmux_data_received(GF_DownloadSession *sess, const u8 *data, u32 nb_bytes);
GF_Err hmux_async_flush(GF_DownloadSession *sess);
GF_Err hmux_send_payload(GF_DownloadSession *sess, u8 *data, u32 size);
#endif

//HTTP2 only
#ifdef GPAC_HAS_HTTP2
GF_Err http2_check_upgrade(GF_DownloadSession *sess);
void http2_set_upgrade_headers(GF_DownloadSession *sess);
GF_Err http2_do_upgrade(GF_DownloadSession *sess, const char *body, u32 body_len);
#endif

#ifdef GPAC_HAS_NGTCP2
GF_Err http3_connect(GF_DownloadSession *sess, char *server, u32 server_port);
#endif

#ifdef GPAC_HAS_CURL
GF_Err curl_setup_session(GF_DownloadSession* sess);
void curl_flush(GF_DownloadSession *sess);
void curl_destroy(GF_DownloadSession *sess);
Bool curl_can_handle_url(const char *url);
GF_Err curl_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read);
void curl_connect(GF_DownloadSession *sess);
GF_Err curl_process_reply(GF_DownloadSession *sess, u32 *ContentLength);
#endif

#ifdef GPAC_HAS_SSL
void *gf_dm_ssl_init(GF_DownloadManager *dm, u32 mode);
Bool gf_ssl_check_cert(SSL *ssl, const char *server_name);
void gf_dm_sess_server_setup_ssl(GF_DownloadSession *sess);
GF_Err gf_ssl_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read);

typedef enum
{
	SSL_CONNECT_OK=0,
	SSL_CONNECT_WAIT,
	SSL_CONNECT_RETRY
} SSLConnectStatus;
SSLConnectStatus gf_ssl_try_connect(GF_DownloadSession *sess, const char *proxy);

#endif


//end GPAC_DISABLE_NETWORK, start EMSCRIPTEN
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



#endif // GPAC_CONFIG_EMSCRIPTEN

#endif //_DOWNLOADER_H_
