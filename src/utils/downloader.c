/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/token.h>
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/base_coding.h>
#include <gpac/tools.h>
#include <gpac/cache.h>

#ifndef GPAC_DISABLE_CORE_TOOLS

#ifdef GPAC_HAS_SSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#endif

#ifdef __USE_POSIX
#include <unistd.h>
#endif

#define SIZE_IN_STREAM ( 2 << 29 )


#define SESSION_RETRY_COUNT	20

#include <gpac/revision.h>
#define GF_DOWNLOAD_AGENT_NAME		"GPAC/"GPAC_VERSION "-rev" GPAC_GIT_REVISION

//let's be agressive with socket buffer size
#define GF_DOWNLOAD_BUFFER_SIZE		131072


static void gf_dm_connect(GF_DownloadSession *sess);

/*internal flags*/
enum
{
	GF_DOWNLOAD_SESSION_USE_SSL     = 1<<10,
	GF_DOWNLOAD_SESSION_THREAD_DEAD = 1<<11
};

typedef struct __gf_user_credentials
{
	char site[1024];
	char username[50];
	char digest[1024];
	Bool valid;
} gf_user_credentials_struct;

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

struct __gf_download_session
{
	/*this is always 0 and helps differenciating downloads from other interfaces (interfaceType != 0)*/
	u32 reserved;

	struct __gf_download_manager *dm;
	GF_Thread *th;
	GF_Mutex *mx;

	Bool in_callback, destroy;
	u32 proxy_enabled;
	Bool allow_direct_reuse;

	char *server_name;
	u16 port;

	char *orig_url;
	char *orig_url_before_redirect;
	char *remote_path;
	gf_user_credentials_struct * creds;
	char cookie[GF_MAX_PATH];
	DownloadedCacheEntry cache_entry;
	Bool reused_cache_entry, from_cache_only;

	//mime type, only used when the session is not cached.
	char *mime_type;
	GF_List *headers;

	GF_Socket *sock;
	u32 num_retry;
	GF_NetIOStatus status;

	u32 flags;
	u32 total_size, bytes_done, icy_metaint, icy_count, icy_bytes;
	u64 start_time;
	u64 chunk_run_time;
	u64 active_time, in_time;

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

	u64 request_start_time;
	/*private extension*/
	void *ext;

	char *remaining_data;
	u32 remaining_data_size;

	Bool local_cache_only;
};

struct __gf_download_manager
{
	GF_Mutex *cache_mx;
	char *cache_directory;

	Bool (*get_user_password)(void *usr_cbk, const char *site_url, char *usr_name, char *password);
	void *usr_cbk;

	u32 head_timeout, request_timeout;
	GF_Config *cfg;
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

/*
 * Private methods of cache
 */

/**
 * \brief Write data to cache
 * Writes data to the cache. A call to gf_cache_open_write_cache should have been issued before calling this function.
 * \param entry The entry to use
 * \param sess The download session
 * \param data data to write
 * \param size number of elements to write
 * \return GF_OK is everything went fine, GF_BAD_PARAM if cache has not been opened, GF_IO_ERR if a failure occurs
 */
GF_Err gf_cache_write_to_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, const char * data, const u32 size);

/**
 * \brief Close the write file pointer of cache
 * This function also flushes all buffers, so cache will always be consistent after
 * \param entry The entry to use
 * \param sess The download session
 * \param success 1 if cache write is success, false otherwise
 * \return GF_OK is everything went fine, GF_BAD_PARAM if entry is NULL, GF_IO_ERR if a failure occurs
 */
GF_Err gf_cache_close_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, Bool success);

/**
 * \brief Open the write file pointer of cache
 * This function prepares calls for gf_cache_write_to_cache
 * \param entry The entry to use
 * \param sess The download session
 * \return GF_OK is everything went fine, GF_BAD_PARAM if entry is NULL, GF_IO_ERR if a failure occurs
 */
GF_Err gf_cache_open_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess );

/*modify end range when chaining byte-range requests*/
void gf_cache_set_end_range(DownloadedCacheEntry entry, u64 range_end);

/*returns 1 if cache is currently open for write*/
Bool gf_cache_is_in_progress(const DownloadedCacheEntry entry);

/**
 * Find a User's credentials for a given site
 */
static gf_user_credentials_struct* gf_find_user_credentials_for_site(GF_DownloadManager *dm, const char *server_name) {
	u32 count, i;
	if (!dm || !dm->credentials || !server_name || !strlen(server_name))
		return NULL;
	count = gf_list_count( dm->credentials);
	for (i = 0 ; i < count; i++) {
		gf_user_credentials_struct * cred = (gf_user_credentials_struct*)gf_list_get(dm->credentials, i );
		assert( cred );
		if (!strcmp(cred->site, server_name))
			return cred;
	}
	return NULL;
}

/**
 * \brief Saves the digest for authentication of password and username
 * \param dm The download manager
 * \param creds The credentials to fill
 * \return GF_OK if info has been filled, GF_BAD_PARAM if creds == NULL or dm == NULL, GF_AUTHENTICATION_FAILURE if user did not filled the info.
 */
static GF_Err gf_user_credentials_save_digest( GF_DownloadManager * dm, gf_user_credentials_struct * creds, const char * password) {
	int size;
	char pass_buf[1024], range_buf[1024];
	if (!dm || !creds || !password)
		return GF_BAD_PARAM;
	sprintf(pass_buf, "%s:%s", creds->username, password);
	size = gf_base64_encode(pass_buf, (u32) strlen(pass_buf), range_buf, 1024);
	range_buf[size] = 0;
	strcpy(creds->digest, range_buf);
	creds->valid = GF_TRUE;
	return GF_OK;
}

/**
 * \brief Asks the user for credentials for given site
 * \param dm The download manager
 * \param creds The credentials to fill
 * \return GF_OK if info has been filled, GF_BAD_PARAM if creds == NULL or dm == NULL, GF_AUTHENTICATION_FAILURE if user did not filled the info.
 */
static GF_Err gf_user_credentials_ask_password( GF_DownloadManager * dm, gf_user_credentials_struct * creds)
{
	char szPASS[50];
	if (!dm || !creds)
		return GF_BAD_PARAM;
	memset(szPASS, 0, 50);
	if (!dm->get_user_password || !dm->get_user_password(dm->usr_cbk, creds->site, creds->username, szPASS)) {
		return GF_AUTHENTICATION_FAILURE;
	}
	return gf_user_credentials_save_digest(dm, creds, szPASS);
	return GF_OK;
}

static gf_user_credentials_struct * gf_user_credentials_register(GF_DownloadManager * dm, const char * server_name, const char * username, const char * password, Bool valid)
{
	gf_user_credentials_struct * creds;
	if (!dm)
		return NULL;
	assert( server_name );
	creds = gf_find_user_credentials_for_site(dm, server_name);
	/* If none found, we create one */
	if (!creds) {
		creds = (gf_user_credentials_struct*)gf_malloc(sizeof( gf_user_credentials_struct));
		if (!creds)
			return NULL;
		gf_list_insert(dm->credentials, creds, 0);
	}
	creds->valid = valid;
	strncpy(creds->username, username ? username : "", 50);
	strcpy(creds->site, server_name);
	if (username && password && valid)
		gf_user_credentials_save_digest(dm, creds, password);
	else {
		if (GF_OK != gf_user_credentials_ask_password(dm, creds)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK,
			       ("[HTTP] Failed to get password information.\n"));
			gf_list_rem( dm->credentials, 0);
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
 * \return GF_FALSE if everyhing is OK, GF_TRUE otherwise
 */
static Bool init_ssl_lib() {
	if (_ssl_is_initialized)
		return GF_FALSE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTPS] Initializing SSL library...\n"));
	init_prng();
	if (RAND_status() != 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPS] Error while initializing Random Number generator, failed to init SSL !\n"));
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTPS] Initalization of SSL library complete.\n"));
	return GF_FALSE;
}

static int ssl_init(GF_DownloadManager *dm, u32 mode)
{
#if OPENSSL_VERSION_NUMBER > 0x00909000
	const
#endif
	SSL_METHOD *meth;

	if (!dm) return 0;
	gf_mx_p(dm->cache_mx);
	/* The SSL has already been initialized. */
	if (dm->ssl_ctx) {
		gf_mx_v(dm->cache_mx);
		return 1;
	}
	/* Init the PRNG.  If that fails, bail out.  */
	if (init_ssl_lib()) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPS] Failed to properly initialize SSL library\n"));
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
	SSL_CTX_set_default_verify_paths(dm->ssl_ctx);
	SSL_CTX_load_verify_locations (dm->ssl_ctx, NULL, NULL);
	/* SSL_VERIFY_NONE instructs OpenSSL not to abort SSL_connect if the
	 certificate is invalid.  We verify the certificate separately in
	 ssl_check_certificate, which provides much better diagnostics
	 than examining the error stack after a failed SSL_connect.  */
	SSL_CTX_set_verify(dm->ssl_ctx, SSL_VERIFY_NONE, NULL);

	/* Since fd_write unconditionally assumes partial writes (and handles them correctly),
	allow them in OpenSSL.  */
	SSL_CTX_set_mode(dm->ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
	gf_mx_v(dm->cache_mx);
	return 1;
error:
	if (dm->ssl_ctx) SSL_CTX_free(dm->ssl_ctx);
	dm->ssl_ctx = NULL;
	gf_mx_v(dm->cache_mx);
	return 0;
}

#endif /* GPAC_HAS_SSL */


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
 * \param sess The session configured with the URL
 * \return NULL if none found, the DownloadedCacheEntry otherwise
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
		if (sess->needs_cache_reconfig==2)
			continue;

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
 * \param dm The download manager to create this entry
 * \param cache_directory The path to the directory containing cache files
 * \param url The full URL
 * \param start_range the start of the byte range request
 * \param end_range the end of the byte range request
 * \param mem_storage Boolean indicating if the cache data should be stored in memory
 * \return The DownloadedCacheEntry
 */
DownloadedCacheEntry gf_cache_create_entry( GF_DownloadManager * dm, const char * cache_directory, const char * url, u64 start_range, u64 end_range, Bool mem_storage);

/*!
 * Removes a session for a DownloadedCacheEntry
 * \param entry The entry
 * \param sess The session to remove
 * \return the number of sessions left in the cached entry, -1 if one of the parameters is wrong
 */
s32 gf_cache_remove_session_from_cache_entry(DownloadedCacheEntry entry, GF_DownloadSession * sess);

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
		        which then results to crash when restarting the session (entry->writeFilePtr i snot set back to NULL)*/
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
 * \param entry The entry
 * \param sess The session to add
 * \return the number of sessions in the cached entry, -1 if one of the parameters is wrong
 */
s32 gf_cache_add_session_to_cache_entry(DownloadedCacheEntry entry, GF_DownloadSession * sess);

static void gf_dm_sess_notify_state(GF_DownloadSession *sess, GF_NetIOStatus dnload_status, GF_Err error);

static void gf_dm_configure_cache(GF_DownloadSession *sess)
{
	DownloadedCacheEntry entry;
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Downloader] gf_dm_configure_cache(%p), cached=%s\n", sess, sess->flags & GF_NETIO_SESSION_NOT_CACHED ? "no" : "yes" ));
	gf_dm_remove_cache_entry_from_session(sess);
	if (sess->flags & GF_NETIO_SESSION_NOT_CACHED) {
		sess->reused_cache_entry = GF_FALSE;
		gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
	} else {
		Bool found = GF_FALSE;
		u32 i, count;
		entry = gf_dm_find_cached_entry_by_url(sess);
		if (!entry) {
			if (sess->local_cache_only) {
				sess->cache_entry = NULL;
				sess->last_error = GF_URL_ERROR;
				return;
			}
			entry = gf_cache_create_entry(sess->dm, sess->dm->cache_directory, sess->orig_url, sess->range_start, sess->range_end, (sess->flags&GF_NETIO_SESSION_MEMORY_CACHE) ? GF_TRUE : GF_FALSE);
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
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
		}
		gf_cache_add_session_to_cache_entry(sess->cache_entry, sess);
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] Cache setup to %p %s\n", sess, gf_cache_get_cache_filename(sess->cache_entry)));


		if ( (sess->allow_direct_reuse || sess->dm->allow_offline_cache) && !gf_cache_check_if_cache_file_is_corrupted(sess->cache_entry)
		) {
			sess->from_cache_only = GF_TRUE;
			sess->connect_time = 0;
			sess->status = GF_NETIO_CONNECTED;
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[HTTP] using existing cache entry\n"));
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
		gf_mx_p( dm->cache_mx );
		gf_dm_url_info_del(&info);
		return;
	}
	realURL = gf_strdup(info.canonicalRepresentation);
	gf_dm_url_info_del(&info);
	assert( realURL );
	count = gf_list_count(dm->cache_entries);
	for (i = 0 ; i < count; i++) {
		const char * e_url;
		DownloadedCacheEntry e = (DownloadedCacheEntry)gf_list_get(dm->cache_entries, i);
		assert(e);
		e_url = gf_cache_get_url(e);
		assert( e_url );
		if (!strcmp(e_url, realURL)) {
			/* We found the existing session */
			gf_cache_entry_set_delete_files_when_deleted(e);
			if (0 == gf_cache_get_sessions_count_for_cache_entry( e )) {
				/* No session attached anymore... we can delete it */
				gf_list_rem(dm->cache_entries, i);
				gf_cache_delete_entry(e);
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
	GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[CACHE] Cannot find URL %s, cache file won't be deleted.\n", url));
}

GF_EXPORT
void gf_dm_delete_cached_file_entry_session(const GF_DownloadSession * sess,  const char * url) {
	if (sess && sess->dm && url) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] Requesting deletion for %s\n", url));
		gf_dm_delete_cached_file_entry(sess->dm, url);
		if (sess->local_cache_only && sess->dm->local_cache_url_provider_cbk)
			sess->dm->local_cache_url_provider_cbk(sess->dm->lc_udta, (char *) url, GF_TRUE);
	}
}


static void gf_dm_clear_headers(GF_DownloadSession *sess)
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


static void gf_dm_disconnect(GF_DownloadSession *sess, Bool force_close)
{
	assert( sess );
	if (sess->connection_close) force_close = GF_TRUE;
	sess->connection_close = GF_FALSE;
	if (sess->remaining_data && sess->remaining_data_size) {
		gf_free(sess->remaining_data);
		sess->remaining_data = NULL;
		sess->remaining_data_size = 0;
	}

	if (sess->status >= GF_NETIO_DISCONNECTED) {
		if (force_close && sess->use_cache_file && sess->cache_entry) {
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
		}
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Downloader] gf_dm_disconnect(%p)\n", sess ));

	gf_mx_p(sess->mx);

	if (force_close || !(sess->flags & GF_NETIO_SESSION_PERSISTENT)) {
#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			SSL_shutdown(sess->ssl);
			SSL_free(sess->ssl);
			sess->ssl = NULL;
		}
#endif
		if (sess->sock) {
			GF_Socket * sx = sess->sock;
			sess->sock = NULL;
			gf_sk_del(sx);
		}
	}
	if (force_close && sess->use_cache_file) {
		gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
	}
	sess->status = GF_NETIO_DISCONNECTED;
	if (sess->num_retry) sess->num_retry--;

	gf_mx_v(sess->mx);
}

GF_EXPORT
void gf_dm_sess_del(GF_DownloadSession *sess)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Downloader] gf_dm_sess_del(%p)\n", sess ));
	if (!sess)
		return;
	/*self-destruction, let the download manager destroy us*/
	if (sess->th && sess->in_callback) {
		sess->destroy = GF_TRUE;
		return;
	}
	gf_dm_disconnect(sess, GF_TRUE);
	gf_dm_clear_headers(sess);

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
	sess->orig_url = sess->server_name = sess->remote_path;
	sess->creds = NULL;
	if (sess->sock)
		gf_sk_del(sess->sock);
	gf_list_del(sess->headers);
	gf_mx_del(sess->mx);

	gf_free(sess);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[Downloader] gf_dm_sess_del(%p) : DONE\n", sess ));
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
 *\brief is download manager thread dead?
 *
 *Indicates whether the thread has ended
 *\param sess the download session
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
	if (!sess) return GF_BAD_PARAM;
	return sess->last_error;
}

GF_EXPORT
void gf_dm_url_info_init(GF_URL_Info * info) {
	info->password = NULL;
	info->userName = NULL;
	info->canonicalRepresentation = NULL;
	info->protocol = NULL;
	info->port = 0;
	info->remotePath = NULL;
	info->server_name = NULL;
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
 * \param url The url to parse for protocol
 * \param info The info to fill
 * \return Returns the offset in url of the protocol found -1 if not found
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
					GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Network] No supported protocol for url %s\n", url));
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
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Network] No supported protocol for url %s\n", url));
			return GF_BAD_PARAM;
		}
	}
	assert( proto_offset >= 0 );
	tmp = strchr(url, '/');
	assert( !info->remotePath );
	info->remotePath = gf_url_percent_encode(tmp ? tmp : "/");
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

static void gf_dm_sess_reload_cached_headers(GF_DownloadSession *sess)
{
	char *hdrs;

	if (!sess || !sess->local_cache_only) return;

	hdrs = gf_cache_get_forced_headers(sess->cache_entry);

	gf_dm_clear_headers(sess);
	while (hdrs) {
		char *sep2, *sepL = strstr(hdrs, "\r\n");
		if (sepL) sepL[0] = 0;
		sep2 = strchr(hdrs, ':');
		if (sep2) {
			GF_HTTPHeader *hdr;
			GF_SAFEALLOC(hdr, GF_HTTPHeader);
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
	char *sep_frag=NULL;
	if (!url) return GF_BAD_PARAM;

	gf_dm_clear_headers(sess);
	sess->allow_direct_reuse = allow_direct_reuse;
	gf_dm_url_info_init(&info);

	if (!sess->sock) socket_changed = GF_TRUE;
	else if (sess->status>GF_NETIO_DISCONNECTED)
		socket_changed = GF_TRUE;

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
		info.port = sess->port;
		info.server_name = sess->server_name ? gf_strdup(sess->server_name) : NULL;
		info.remotePath = gf_strdup(url);
		sep = strstr(sess->orig_url_before_redirect, "://");
		assert(sep);
		c = sep[3];
		sep[3] = 0;
		info.protocol = gf_strdup(sess->orig_url_before_redirect);
		sep[3] = c;
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

	if (sess->orig_url) gf_free(sess->orig_url);
	sess->orig_url = gf_strdup(info.canonicalRepresentation);

	if (!sess->orig_url_before_redirect)
		sess->orig_url_before_redirect = gf_strdup(url);

	if (sess->remote_path) gf_free(sess->remote_path);
	sess->remote_path = gf_strdup(info.remotePath);

	if (!socket_changed && info.userName  && !strcmp(info.userName, sess->creds->username)) {
	} else {
		sess->creds = NULL;
		if (info.userName ) {
			if (! sess->dm) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Did not found any download manager, credentials not supported\n"));
			} else
				sess->creds = gf_user_credentials_register(sess->dm, sess->server_name, info.userName, info.password, info.userName && info.password);
		}
	}
	gf_dm_url_info_del(&info);
	if (sep_frag) sep_frag[0]='#';

	if (sess->sock && !socket_changed) {
		sess->status = GF_NETIO_CONNECTED;
		sess->num_retry = SESSION_RETRY_COUNT;
		sess->needs_cache_reconfig = 1;
	} else {
		if (sess->sock) gf_sk_del(sess->sock);
		sess->sock = NULL;
		sess->status = GF_NETIO_SETUP;
#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			SSL_shutdown(sess->ssl);
			SSL_free(sess->ssl);
			sess->ssl = NULL;
		}
#endif

	}
	sess->total_size=0;
	sess->bytes_done=0;
	assert(sess->remaining_data_size==0);

	sess->local_cache_only = GF_FALSE;
	if (sess->dm && sess->dm->local_cache_url_provider_cbk) {
		Bool res = sess->dm->local_cache_url_provider_cbk(sess->dm->lc_udta, (char *)url, GF_FALSE);
		if (res == GF_TRUE) {
			sess->local_cache_only = GF_TRUE;
			gf_free(sess->orig_url);
			sess->orig_url = gf_strdup(url);
			sess->last_error = GF_OK;
			gf_dm_configure_cache(sess);
			sess->status = GF_NETIO_DATA_TRANSFERED;
			sess->total_size = gf_cache_get_content_length(sess->cache_entry);
			sess->bytes_done = sess->total_size;
			sess->total_time_since_req = gf_cache_get_downtime(sess->cache_entry);
			if (sess->total_time_since_req)
				sess->bytes_per_sec = (u32) ((1000 * (u64) sess->bytes_done) / sess->total_time_since_req);
			else
				sess->bytes_per_sec = 0;
			gf_dm_sess_reload_cached_headers(sess);
		}
	}
	return sess->last_error;
}



static u32 gf_dm_session_thread(void *par)
{
	GF_DownloadSession *sess = (GF_DownloadSession *)par;
	if (!sess) return 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Downloader] Entering thread ID %d\n", gf_th_id() ));
	sess->flags &= ~GF_DOWNLOAD_SESSION_THREAD_DEAD;
	while (!sess->destroy) {
		gf_mx_p(sess->mx);
		if (sess->status >= GF_NETIO_DISCONNECTED) {
			gf_mx_v(sess->mx);
			break;
		}
		if (sess->status < GF_NETIO_CONNECTED) {
			gf_dm_connect(sess);
		} else {
			sess->do_requests(sess);
		}
		gf_mx_v(sess->mx);
		gf_sleep(0);
	}
	/*destroy all sessions*/
	gf_dm_disconnect(sess, GF_FALSE);
	sess->status = GF_NETIO_STATE_ERROR;
	sess->last_error = GF_OK;
	sess->flags |= GF_DOWNLOAD_SESSION_THREAD_DEAD;
	return 1;
}


GF_EXPORT
GF_DownloadSession *gf_dm_sess_new_simple(GF_DownloadManager * dm, const char *url, u32 dl_flags,
        gf_dm_user_io user_io,
        void *usr_cbk,
        GF_Err *e)
{
	GF_DownloadSession *sess;
	if (!dm) return NULL;

	GF_SAFEALLOC(sess, GF_DownloadSession);
	if (!sess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("%s:%d Cannot allocate session for URL %s: OUT OF MEMORY!\n", __FILE__, __LINE__, url));
		return NULL;
	}
	sess->headers = gf_list_new();
	sess->flags = dl_flags;
	if (sess->flags & GF_NETIO_SESSION_NOTIFY_DATA)
		sess->force_data_write_callback = GF_TRUE;
	if (!dm->head_timeout) sess->server_only_understand_get = GF_TRUE;
	sess->user_proc = user_io;
	sess->usr_cbk = usr_cbk;
	sess->creds = NULL;
	sess->dm = dm;
	sess->disable_cache = dm->disable_cache;
	sess->mx = gf_mx_new(url);
	if (!sess->mx) {
		gf_free(sess);
		return NULL;
	}


	assert( dm );

	*e = gf_dm_sess_setup_from_url(sess, url, GF_FALSE);
	if (*e) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("%s:%d gf_dm_sess_new_simple: error=%s at setup for '%s'\n", __FILE__, __LINE__, gf_error_to_string(*e), url));
		gf_dm_sess_del(sess);
		return NULL;
	}
	assert( sess );
	sess->num_retry = SESSION_RETRY_COUNT;
	/*threaded session must be started with gf_dm_sess_process*/
	return sess;
}

GF_EXPORT
GF_DownloadSession *gf_dm_sess_new(GF_DownloadManager *dm, const char *url, u32 dl_flags,
                                   gf_dm_user_io user_io,
                                   void *usr_cbk,
                                   GF_Err *e)
{
	GF_DownloadSession *sess;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("%s:%d gf_dm_sess_new(%s)\n", __FILE__, __LINE__, url));
	*e = GF_OK;
	if (gf_dm_is_local(dm, url)) return NULL;

	if (!gf_dm_can_handle_url(dm, url)) {
		*e = GF_NOT_SUPPORTED;
		return NULL;
	}
	sess = gf_dm_sess_new_simple(dm, url, dl_flags, user_io, usr_cbk, e);
	if (sess) {
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

#ifdef GPAC_HAS_SSL
	if (sess->ssl) {
		s32 size;
		e = gf_sk_receive(sess->sock, NULL, 0, NULL);
		if (e==GF_IP_NETWORK_EMPTY) {
			gf_mx_v(sess->mx);
			return e;
		}
		size = SSL_read(sess->ssl, data, data_size);
		if (size < 0)
			e = GF_IO_ERR;
		else if (!size)
			e = GF_IP_NETWORK_EMPTY;
		else {
			e = GF_OK;
			data[size] = 0;
			*out_read = size;
		}
	} else
#endif

		e = gf_sk_receive(sess->sock, data, data_size, out_read);

	gf_mx_v(sess->mx);
	return e;
}


#ifdef GPAC_HAS_SSL

#define LWR(x) ('A' <= (x) && (x) <= 'Z' ? (x) - 32 : (x))

static Bool rfc2818_match(const char *pattern, const char *string)
{
	char c, d;
	u32 i=0, k=0;
	while (1) {
		c = LWR(pattern[i]);
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

static void gf_dm_connect(GF_DownloadSession *sess)
{
	GF_Err e;
	u16 proxy_port = 0;
	const char *proxy;

	if (!sess->sock) {
		sess->num_retry = 40;
		sess->sock = gf_sk_new(GF_SOCK_TYPE_TCP);
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
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Connecting to %s:%d\n", proxy, proxy_port));

	if (sess->status == GF_NETIO_SETUP) {
		u64 now;
		if (sess->dm && sess->dm->simulate_no_connection) {
			sess->status = GF_NETIO_STATE_ERROR;
			sess->last_error = GF_IP_NETWORK_FAILURE;
			gf_dm_sess_notify_state(sess, sess->status, sess->last_error);
			return;
		}

		now = gf_sys_clock_high_res();
		e = gf_sk_connect(sess->sock, (char *) proxy, proxy_port, NULL);

		/*retry*/
		if ((e == GF_IP_SOCK_WOULD_BLOCK) && sess->num_retry) {
			sess->status = GF_NETIO_SETUP;
			sess->num_retry--;
			return;
		}

		/*failed*/
		if (e) {
			if (!sess->cache_entry && sess->dm && sess->dm->allow_offline_cache) {
				gf_dm_configure_cache(sess);
				if (sess->from_cache_only) return;
			}
			sess->status = GF_NETIO_STATE_ERROR;
			sess->last_error = e;
			gf_dm_sess_notify_state(sess, sess->status, e);
			return;
		}
		if (sess->allow_direct_reuse) {
			gf_dm_configure_cache(sess);
			if (sess->from_cache_only) return;
		}

		sess->connect_time = (u32) (gf_sys_clock_high_res() - now);
		sess->status = GF_NETIO_CONNECTED;
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Connected to %s:%d\n", proxy, proxy_port));
		gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
//		gf_sk_set_buffer_size(sess->sock, GF_TRUE, GF_DOWNLOAD_BUFFER_SIZE);
//		gf_sk_set_buffer_size(sess->sock, GF_FALSE, GF_DOWNLOAD_BUFFER_SIZE);
	}

#ifdef GPAC_HAS_SSL
	if (!sess->ssl && (sess->flags & GF_DOWNLOAD_SESSION_USE_SSL)) {
		u64 now = gf_sys_clock_high_res();
		if (sess->dm && !sess->dm->ssl_ctx)
			ssl_init(sess->dm, 0);
		/*socket is connected, configure SSL layer*/
		if (sess->dm && sess->dm->ssl_ctx) {
			int ret;
			long vresult;
			char common_name[256];
			X509 *cert;
			Bool success = GF_TRUE;

			sess->ssl = SSL_new(sess->dm->ssl_ctx);
			SSL_set_fd(sess->ssl, gf_sk_get_handle(sess->sock));
			SSL_set_connect_state(sess->ssl);
			ret = SSL_connect(sess->ssl);
			if (ret<=0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SSL] Cannot connect, error %d\n", ret));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[SSL] connected\n"));
			}

			cert = SSL_get_peer_certificate(sess->ssl);
			/*if we have a cert, check it*/
			if (cert) {
				SSL_set_verify_result(sess->ssl, 0);
				vresult = SSL_get_verify_result(sess->ssl);

				if (vresult == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[SSL] Cannot locate issuer's certificate on the local system, will not attempt to validate\n"));
					SSL_set_verify_result(sess->ssl, 0);
					vresult = SSL_get_verify_result(sess->ssl);
				}

				if (vresult == X509_V_OK) {
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
						if (rfc2818_match(valid_name, sess->server_name)) {
							success = GF_TRUE;
							GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[SSL] Hostname %s matches %s\n", sess->server_name, valid_name));
							break;
						}
					}
					if (!success) {
						if (sess->dm && sess->dm->allow_broken_certificate) {
							success = GF_TRUE;
							GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[SSL] Mismatch in certificate names: expected %s\n", sess->server_name));
						} else {
							GF_LOG(success ? GF_LOG_WARNING : GF_LOG_ERROR, GF_LOG_NETWORK, ("[SSL] Mismatch in certificate names, try using -broken-cert: expected %s\n", 	sess->server_name));
						}
						for (i = 0; i < (int)gf_list_count(valid_names); ++i) {
							const char *valid_name = (const char*) gf_list_get(valid_names, i);
							GF_LOG(success ? GF_LOG_WARNING : GF_LOG_ERROR, GF_LOG_NETWORK, ("[SSL] Tried name: %s\n", valid_name));
						}
					}

					gf_list_del(valid_names);
					GENERAL_NAMES_free(altnames);
				} else {
					success = GF_FALSE;
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[SSL] Error verifying certificate %x\n", vresult));
				}

				X509_free(cert);

				if (!success) {
					gf_dm_disconnect(sess, GF_TRUE);
					sess->status = GF_NETIO_STATE_ERROR;
					sess->last_error = GF_AUTHENTICATION_FAILURE;
					gf_dm_sess_notify_state(sess, sess->status, sess->last_error);
				}
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
					gf_dm_disconnect(sess, GF_FALSE);
					sess->status = GF_NETIO_SETUP;
					sess->server_only_understand_get = GF_TRUE;
					GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("gf_dm_refresh_cache_entry() : Timeout with HEAD, try with GET\n"));
					e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
					if (e) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("gf_dm_refresh_cache_entry() : Error with GET %d\n", e));
						sess->status = GF_NETIO_STATE_ERROR;
						sess->last_error = e;
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
	if (!sess) return GF_BAD_PARAM;
	if (sess->cache_entry) {
		if (!discontinue_cache) {
			if (gf_cache_get_end_range(sess->cache_entry) + 1 != start_range)
				return GF_NOT_SUPPORTED;
		}
		if (sess->sock) {
			if (sess->status != GF_NETIO_CONNECTED) {
				if (sess->status != GF_NETIO_DISCONNECTED) {
					return GF_BAD_PARAM;
				}
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
		if (sess->status != GF_NETIO_SETUP) return GF_BAD_PARAM;
	}
	sess->range_start = start_range;
	sess->range_end = end_range;
	sess->needs_range = (start_range || end_range) ? GF_TRUE : GF_FALSE;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dm_sess_process(GF_DownloadSession *sess)
{
	Bool go;

	/*if session is threaded, start thread*/
	if (! (sess->flags & GF_NETIO_SESSION_NOT_THREADED)) {
		if (sess->th) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[HTTP] Session already started - ignoring start\n"));
			return GF_OK;
		}
		sess->th = gf_th_new(sess->orig_url);
		if (!sess->th) return GF_OUT_OF_MEM;
		gf_th_run(sess->th, gf_dm_session_thread, sess);
		return GF_OK;
	}
	/*otherwise do a synchronous download*/
	go = GF_TRUE;
	while (go) {
		switch (sess->status) {
		/*setup download*/
		case GF_NETIO_SETUP:
			gf_dm_connect(sess);
			break;
		case GF_NETIO_WAIT_FOR_REPLY:
		case GF_NETIO_CONNECTED:
		case GF_NETIO_DATA_EXCHANGE:
			sess->do_requests(sess);
			break;
		case GF_NETIO_DATA_TRANSFERED:
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
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Downloader] Session in unknown state !! - aborting\n"));
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
 * \param dm The GF_DownloadManager
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
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Cache] Cache size %d exceeds max allowed %d, deleting entire cache\n", out_size, dm->max_cache_size));
		gf_cache_delete_all_cached_files(dm->cache_directory);
	}
}

GF_EXPORT
GF_DownloadManager *gf_dm_new(GF_Config *cfg)
{
	const char *opt;
	const char * default_cache_dir;
	GF_DownloadManager *dm;
	GF_SAFEALLOC(dm, GF_DownloadManager);
	if (!dm) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Downloader] Failed to allocate downloader\n"));
		return NULL;
	}
	dm->sessions = gf_list_new();
	dm->cache_entries = gf_list_new();
	dm->credentials = gf_list_new();
	dm->skip_proxy_servers = gf_list_new();
	dm->partial_downloads = gf_list_new();
	dm->cfg = cfg;
	dm->cache_mx = gf_mx_new("download_manager_cache_mx");
	default_cache_dir = NULL;
	gf_mx_p( dm->cache_mx );

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
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Cache] Cannot write to %s directory, using system temp cache\n", dm->cache_directory ));
				gf_free(dm->cache_directory);
				dm->cache_directory = NULL;
				opt = NULL;
				goto retry_cache;
			}
		}
		if (test) {
			gf_fclose(test);
			gf_delete_file(szTemp);
		}
	}

	/*use it in in BYTES per second*/
	dm->limit_data_rate = 1000 * gf_opts_get_int("core", "maxrate") / 8;

	dm->read_buf_size = GF_DOWNLOAD_BUFFER_SIZE;
	//when rate is limited, use smaller smaller read size
	if (dm->limit_data_rate) {
		dm->read_buf_size = 1024;
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

	dm->head_timeout = gf_opts_get_int("core", "head-timeout");
	if (!dm->head_timeout) dm->head_timeout = 5000;

	dm->request_timeout = gf_opts_get_int("core", "req-timeout");
	if (!dm->request_timeout) dm->request_timeout = 20000;

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
void gf_dm_set_auth_callback(GF_DownloadManager *dm,
                             Bool (*get_user_password)(void *usr_cbk, const char *site_url, char *usr_name, char *password),
                             void *usr_cbk)
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
		gf_delete_file( entry->filename );
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
		gf_user_credentials_struct * cred = (gf_user_credentials_struct*)gf_list_get( dm->credentials, 0);
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
	dm->cfg = NULL;
	gf_mx_v( dm->cache_mx );
	gf_mx_del( dm->cache_mx);
	dm->cache_mx = NULL;
	gf_free(dm);
}

/*!
 * Skip ICY metadata from SHOUTCAST or ICECAST streams.
 * Data will be skipped and parsed and sent as a GF_NETIO_Parameter to the user_io,
 * so modules interrested by those streams may use the data
 * \param sess The GF_DownloadSession
 * \param data last data received
 * \param nbBytes The number of bytes contained into data
 */
static void gf_icy_skip_data(GF_DownloadSession * sess, const char * data, u32 nbBytes)
{
	u32 icy_metaint;
	if (!sess) return;

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
					GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[ICY] Found metainfo in stream=%s, (every %d bytes)\n", szData, icy_metaint));
					gf_dm_sess_user_io(sess, &par);
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[ICY] Empty metainfo in stream, (every %d bytes)\n", icy_metaint));
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

	if (!sess) return NULL;
	if (!sess->chunked) return body_start;

	if (sess->nb_left_in_chunk) {
		if (sess->nb_left_in_chunk > *payload_size) {
			sess->nb_left_in_chunk -= (*payload_size);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] Chunk encoding: still %d bytes to get\n", sess->nb_left_in_chunk));
		} else {
			*payload_size = sess->nb_left_in_chunk;
			sess->nb_left_in_chunk = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] Chunk encoding: last bytes in chunk received\n"));
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
			//chunk exactly ends our packet, reset session start time
			if (*payload_size == 2) {
				sess->chunk_run_time += gf_sys_clock_high_res() - sess->start_time;
				sess->start_time = 0;
			}
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

	//start of a new chunk, update start time
	if (!sess->start_time) {
		sess->start_time = gf_sys_clock_high_res();
		//assume RTT is session reply time, and that chunk transfer started RTT/2 ago
		if (first_chunk_in_payload && sess->start_time > sess->reply_time/2)
			sess->start_time -= sess->reply_time/2;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] First byte in chunk received (%d bytes in packet), new start time %u ms\n", *payload_size, (u32) sess->start_time/1000));
	}

	//cannot parse now, copy over the bytes
	if (!te_header) {
		*header_size = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] Chunk encoding: current buffer does not contain enough bytes (%d) to read the size\n", *payload_size));
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Chunk encoding: fail to read chunk size from buffer %s, aborting\n", body_start));
		return NULL;
	}
	if (sep) sep[0] = ';';
	*payload_size = size;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] Chunk Start: Header \"%s\" - header size %d - payload size %d at UTC "LLD"\n", body_start, 2+strlen(body_start), size, gf_net_get_utc()));

	te_header[0] = '\r';
	if (!size) sess->last_chunk_found = GF_TRUE;
	return te_header+2;
}


static void dm_sess_update_download_rate(GF_DownloadSession * sess, Bool always_check)
{
	u64 runtime;
	if (!always_check && (sess->bytes_done==sess->total_size)) return;

	/*update state*/
	runtime = sess->chunk_run_time;
	if (sess->start_time) {
		runtime += (gf_sys_clock_high_res() - sess->start_time);
		if (sess->active_time) {
			runtime = sess->active_time;
		}
	}
	if (!runtime) runtime=1;

	sess->bytes_per_sec = (u32) ((1000000 * (u64) sess->bytes_done) / runtime);

	if (sess->chunked) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] bandwidth estimation: download time "LLD" us (chunk download time "LLD" us) - bytes %u - rate %u kbps\n", runtime, sess->chunk_run_time, sess->bytes_done, sess->bytes_per_sec*8/1000));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] bandwidth estimation: download time "LLD" us - bytes %u - rate %u kbps\n", runtime, sess->bytes_done, sess->bytes_per_sec*8/1000));
	}
}


static GFINLINE void gf_dm_data_received(GF_DownloadSession *sess, u8 *payload, u32 payload_size, Bool store_in_init, u32 *rewrite_size, u8 *original_payload)
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
		//first_chunk_in_payload = GF_FALSE;
		if (!hdr_size && !data) {
			/* keep the data and wait for the rest */
			if (sess->remaining_data) gf_free(sess->remaining_data);
			sess->remaining_data_size = nbBytes;
			sess->remaining_data = (char *)gf_malloc(nbBytes * sizeof(char));
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
		} else {
			payload_size -= hdr_size + nbBytes;
			payload += hdr_size + nbBytes;
			flush_chunk = GF_TRUE;
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
		dm_sess_update_download_rate(sess, GF_TRUE);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] url %s received %d new bytes (%d kbps)\n", gf_cache_get_url(sess->cache_entry), nbBytes, 8*sess->bytes_per_sec/1000));
		if (sess->total_size && (sess->bytes_done > sess->total_size)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[HTTP] url %s received more bytes than planned!! Got %d bytes vs %d content length\n", gf_cache_get_url(sess->cache_entry), sess->bytes_done , sess->total_size ));
			sess->bytes_done = sess->total_size;
		}

		if (sess->icy_metaint > 0)
			gf_icy_skip_data(sess, (char *) data, nbBytes);
		else {
			if (sess->use_cache_file)
				gf_cache_write_to_cache( sess->cache_entry, sess, (char *) data, nbBytes);

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
		gf_dm_disconnect(sess, GF_FALSE);
		par.msg_type = GF_NETIO_DATA_TRANSFERED;
		par.error = GF_OK;

		if (sess->use_cache_file) {
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_TRUE);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK,
			       ("[CACHE] url %s saved as %s\n", gf_cache_get_url(sess->cache_entry), gf_cache_get_cache_filename(sess->cache_entry)));
		}
		gf_dm_sess_user_io(sess, &par);
		sess->total_time_since_req = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
		run_time = gf_sys_clock_high_res() - sess->start_time;
		if (sess->in_time) {
			sess->active_time += gf_sys_clock_high_res() - sess->in_time;
			if (run_time > sess->active_time) {
				sess->total_time_since_req -= (u32) (run_time - sess->active_time);
				run_time = sess->active_time; // + (run_time - sess->active_time)/3;
				sess->bytes_per_sec = (u32) ((1000000 * (u64) sess->bytes_done) / run_time);
			}
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] url %s (%d bytes) downloaded in "LLU" us (%d kbps) (%d us since request - got response in %d us)\n", gf_cache_get_url(sess->cache_entry), sess->bytes_done,
		                                     run_time, 8*sess->bytes_per_sec/1000, sess->total_time_since_req, sess->reply_time ));

		if (sess->chunked && (payload_size==2))
			payload_size=0;
	}

	if (rewrite_size && sess->chunked && data && original_payload) {
		//use memmove since regions overlap
		memmove(original_payload + *rewrite_size, data, nbBytes);
		*rewrite_size += nbBytes;
	}

	if (!sess->nb_left_in_chunk && remaining) {
		sess->nb_left_in_chunk = remaining;
	} else if (payload_size) {
		gf_dm_data_received(sess, payload, payload_size, store_in_init, rewrite_size, original_payload);
	}
}


GF_EXPORT
GF_Err gf_dm_sess_fetch_data(GF_DownloadSession *sess, char *buffer, u32 buffer_size, u32 *read_size)
{
	u32 size;
	GF_Err e;
	if (/*sess->cache || */ !buffer || !buffer_size) return GF_BAD_PARAM;
	if (sess->th) return GF_BAD_PARAM;
	if (sess->status == GF_NETIO_DISCONNECTED) {
		if (!sess->init_data_size)
			return GF_EOS;
	}
	else if (sess->status > GF_NETIO_DATA_TRANSFERED) return GF_BAD_PARAM;

	*read_size = 0;
	if (sess->status == GF_NETIO_DATA_TRANSFERED) return GF_EOS;

	sess->in_time = gf_sys_clock_high_res();
	if (sess->status == GF_NETIO_SETUP) {
		gf_dm_connect(sess);
		if (sess->last_error) return sess->last_error;
		e = GF_OK;
	} else if (sess->status < GF_NETIO_DATA_EXCHANGE) {
		sess->do_requests(sess);
		e = sess->last_error;
	}
	/*we're running but we had data previously*/
	else if (sess->init_data) {
		if (sess->init_data_size<=buffer_size) {
			memcpy(buffer, sess->init_data, sizeof(char)*sess->init_data_size);
			*read_size = sess->init_data_size;
			gf_free(sess->init_data);
			sess->init_data = NULL;
			sess->init_data_size = 0;
		} else {
			memcpy(buffer, sess->init_data, sizeof(char)*buffer_size);
			*read_size = buffer_size;
			sess->init_data_size -= buffer_size;
			memcpy(sess->init_data, sess->init_data+buffer_size, sizeof(char)*sess->init_data_size);
		}
		e = GF_OK;
	} else {
#if 1
		u32 single_read;
		u32 nb_read = 0;
		*read_size = 0;
		while (1) {
			single_read=0;
			if (nb_read>=buffer_size)
				break;
			e = gf_dm_read_data(sess, buffer+nb_read, buffer_size-nb_read, &single_read);
			if (e<0) {
				break;
			}

			size = single_read;
			single_read = 0;

			gf_dm_data_received(sess, (u8 *) buffer+nb_read, size, GF_FALSE, &single_read, (u8 *) buffer+nb_read);
			if (!sess->chunked)
				single_read = size;

			nb_read+=single_read;
			*read_size += single_read;

			if (e) break;
		}
		if (*read_size && (e<0)) e = GF_OK;
		nb_read = 0;
#else
		e = gf_dm_read_data(sess, buffer, buffer_size, read_size);
		if (!e) {
			size = *read_size;
			*read_size = 0;
			gf_dm_data_received(sess, (u8 *) buffer, size, GF_FALSE, read_size, buffer);
			if (!sess->chunked)
				*read_size = size;
		}
#endif

	}
	sess->active_time += gf_sys_clock_high_res() - sess->in_time;
	return e;
}

GF_EXPORT
GF_Err gf_dm_sess_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u64 *total_size, u64 *bytes_done, u32 *bytes_per_sec, GF_NetIOStatus *net_status)
{
	if (!sess) return GF_BAD_PARAM;
	if (server) *server = sess->server_name;
	if (path) *path = sess->remote_path;
	if (total_size) {
		if (sess->total_size==SIZE_IN_STREAM) *total_size  = 0;
		else *total_size = sess->total_size;
	}
	if (bytes_done) *bytes_done = sess->bytes_done;
	if (bytes_per_sec) *bytes_per_sec = sess->bytes_per_sec;
	if (net_status) *net_status = sess->status;
	if (sess->status == GF_NETIO_DISCONNECTED) return GF_EOS;
	else if (sess->status == GF_NETIO_STATE_ERROR) return GF_SERVICE_ERROR;
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
	return gf_cache_get_cache_filename(sess->cache_entry);
}

#if 0 //unused
/*!
 * Tells whether session can be cached on disk.
 * Typically, when request has no content length, it deserves being streamed an cannot be cached
 * (ICY or MPEG-streamed content
 * \param sess The session
 * \return True if a cache can be created
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
		gf_dm_disconnect(sess, GF_TRUE);
		sess->status = GF_NETIO_STATE_ERROR;
		gf_mx_v(sess->mx);
	}
}

#if 0 //unused
/*!
 *\brief gets private data
 *
 *Gets private data associated with the session.
 *\param sess the download session
 *\return the private data
 *\warning the private_data parameter is reserved for bandwidth statistics per service when used in the GPAC terminal.
 */
void *gf_dm_sess_get_private(GF_DownloadSession * sess)
{
	return sess ? sess->ext : NULL;
}

/*!
 *\brief sets private data
 *
 *associate private data with the session.
 *\param sess the download session
 *\param private_data the private data
 *\warning the private_data parameter is reserved for bandwidth statistics per service when used in the GPAC terminal.
 */
void gf_dm_sess_set_private(GF_DownloadSession * sess, void *private_data)
{
	if (sess) sess->ext = private_data;
}
#endif

/*!
 * Sends the HTTP headers
 * \param sess The GF_DownloadSession
 * \param sHTTP buffer containing the request
 * \return GF_OK if everything went fine, the error otherwise
 */
static GF_Err http_send_headers(GF_DownloadSession *sess, char * sHTTP) {
	GF_Err e;
	GF_NETIO_Parameter par;
	Bool no_cache = GF_FALSE;
	char range_buf[1024];
	char pass_buf[1100];
	const char *user_agent;
	const char *url;
	const char *user_profile;
	const char *param_string;
	Bool has_accept, has_connection, has_range, has_agent, has_language, send_profile, has_mime;
	assert (sess->status == GF_NETIO_CONNECTED);

	gf_dm_clear_headers(sess);
	sess->active_time = 0;

	assert(sess->remaining_data_size == 0);

	if (sess->needs_cache_reconfig) {
		gf_dm_configure_cache(sess);
		sess->needs_cache_reconfig = 0;
	}
	if (sess->from_cache_only) {
		sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = 0;
		sess->status = GF_NETIO_WAIT_FOR_REPLY;
		gf_dm_sess_notify_state(sess, GF_NETIO_WAIT_FOR_REPLY, GF_OK);
		return GF_OK;
	}

	/*setup authentification*/
	strcpy(pass_buf, "");
	sess->creds = gf_find_user_credentials_for_site( sess->dm, sess->server_name );
	if (sess->creds && sess->creds->valid) {
		sprintf(pass_buf, "Authorization: Basic %s", sess->creds->digest);
	}

	user_agent = gf_opts_get_key("core", "ua");
	if (!user_agent) user_agent = GF_DOWNLOAD_AGENT_NAME;

	par.error = GF_OK;
	par.msg_type = GF_NETIO_GET_METHOD;
	par.name = NULL;
	gf_dm_sess_user_io(sess, &par);
	if (!par.name || sess->server_only_understand_get) {
		par.name = "GET";
	}

	if (par.name) {
		if (!strcmp(par.name, "GET")) sess->http_read_type = GET;
		else if (!strcmp(par.name, "HEAD")) sess->http_read_type = HEAD;
		else sess->http_read_type = OTHER;
	} else {
		sess->http_read_type = GET;
	}

	url = (sess->proxy_enabled==1) ? sess->orig_url : sess->remote_path;

	param_string = gf_opts_get_key("core", "query-string");
	if (param_string) {
		if (strchr(sess->remote_path, '?')) {
			sprintf(sHTTP, "%s %s&%s HTTP/1.0\r\nHost: %s\r\n" ,
			        par.name ? par.name : "GET", url, param_string, sess->server_name);
		} else {
			sprintf(sHTTP, "%s %s?%s HTTP/1.0\r\nHost: %s\r\n" ,
			        par.name ? par.name : "GET", url, param_string, sess->server_name);
		}
	} else {
		sprintf(sHTTP, "%s %s HTTP/1.1\r\nHost: %s\r\n" ,
		        par.name ? par.name : "GET", url, sess->server_name);
	}

	/*get all headers*/
	has_agent = has_accept = has_connection = has_range = has_language = has_mime = GF_FALSE;
	while (1) {
		par.msg_type = GF_NETIO_GET_HEADER;
		par.value = NULL;
		gf_dm_sess_user_io(sess, &par);
		if (!par.value) break;
		strcat(sHTTP, par.name);
		strcat(sHTTP, ": ");
		strcat(sHTTP, par.value);
		strcat(sHTTP, "\r\n");
		if (!strcmp(par.name, "Accept")) has_accept = GF_TRUE;
		else if (!strcmp(par.name, "Connection")) has_connection = GF_TRUE;
		else if (!strcmp(par.name, "Range")) has_range = GF_TRUE;
		else if (!strcmp(par.name, "User-Agent")) has_agent = GF_TRUE;
		else if (!strcmp(par.name, "Accept-Language")) has_language = GF_TRUE;
		else if (!strcmp(par.name, "Content-Type")) has_mime = GF_TRUE;
		if (!par.msg_type) break;
	}
	if (!has_agent) {
		strcat(sHTTP, "User-Agent: ");
		strcat(sHTTP, user_agent);
		strcat(sHTTP, "\r\n");
	}
	/*no mime and POST/PUT, default to octet stream*/
	if (!has_mime && (sess->http_read_type==OTHER)) strcat(sHTTP, "Content-Type: application/octet-stream\r\n");
	if (!has_accept && (sess->http_read_type!=OTHER) ) strcat(sHTTP, "Accept: */*\r\n");
	if (sess->proxy_enabled==1) strcat(sHTTP, "Proxy-Connection: Keep-alive\r\n");
	else if (!has_connection) strcat(sHTTP, "Connection: Keep-Alive\r\n");
	if (!has_range && sess->needs_range) {
		if (!sess->range_end) sprintf(range_buf, "Range: bytes="LLD"-\r\n", sess->range_start);
		else sprintf(range_buf, "Range: bytes="LLD"-"LLD"\r\n", sess->range_start, sess->range_end);
		strcat(sHTTP, range_buf);

		no_cache = GF_TRUE;
	}
	if (!has_language) {
		const char *opt = gf_opts_get_key("core", "lang");
		if (opt) {
			strcat(sHTTP, "Accept-Language: ");
			strcat(sHTTP, opt);
			strcat(sHTTP, "\r\n");
		}
	}


	if (strlen(pass_buf)) {
		strcat(sHTTP, pass_buf);
		strcat(sHTTP, "\r\n");
	}

	par.msg_type = GF_NETIO_GET_CONTENT;
	par.data = NULL;
	par.size = 0;

	/*check if we have personalization info*/
	send_profile = GF_FALSE;
	user_profile = gf_opts_get_key("core", "user-profileid");

	if (user_profile) {
		strcat(sHTTP, "X-UserProfileID: ");
		strcat(sHTTP, user_profile);
		strcat(sHTTP, "\r\n");
	} else {
		user_profile = gf_opts_get_key("core", "user-profile");
		if (user_profile) {
			FILE *profile = gf_fopen(user_profile, "rt");
			if (profile) {
				gf_fseek(profile, 0, SEEK_END);
				par.size = (u32) gf_ftell(profile);
				gf_fclose(profile);
				sprintf(range_buf, "Content-Length: %d\r\n", par.size);
				strcat(sHTTP, range_buf);
				strcat(sHTTP, "Content-Type: text/xml\r\n");
				send_profile = GF_TRUE;
			}
		}
	}


	if (!send_profile) {
		gf_dm_sess_user_io(sess, &par);
		if (par.data && par.size) {
			sprintf(range_buf, "Content-Length: %d\r\n", par.size);
			strcat(sHTTP, range_buf);
		}
	}

	if (sess->http_read_type!=OTHER) {
		/*signal we support title streaming*/
//		if (!strcmp(sess->remote_path, "/")) strcat(sHTTP, "icy-metadata:1\r\n");
		/* This will force the server to respond with Icy-Metaint */
		strcat(sHTTP, "Icy-Metadata: 1\r\n");

		/*cached headers are not appended in POST*/
		if (!no_cache && !sess->disable_cache && (GF_OK < gf_cache_append_http_headers( sess->cache_entry, sHTTP)) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("Cache Entry : %p, FAILED to append cache directives.", sess->cache_entry));
		}
	}

	strcat(sHTTP, "\r\n");

	if (send_profile || par.data) {
		u32 len = (u32) strlen(sHTTP);
		char *tmp_buf = (char*)gf_malloc(sizeof(char)*(len+par.size+1));
		strcpy(tmp_buf, sHTTP);
		if (par.data) {
			memcpy(tmp_buf+len, par.data, par.size);
			tmp_buf[len+par.size] = 0;
		} else {
			FILE *profile;
			assert( sess->dm );
			assert( sess->dm->cfg );
			user_profile = gf_opts_get_key("core", "user-profile");
			assert (user_profile);
			profile = gf_fopen(user_profile, "rt");
			if (profile) {
				s32 read = (s32) fread(tmp_buf+len, sizeof(char), par.size, profile);
				if ((read<0) || (read< (s32) par.size)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
					       ("[HTTP] Error while loading UserProfile, size=%d, should be %d.", read, par.size));
					for (; read < (s32) par.size; read++) {
						tmp_buf[len + read] = 0;
					}
				}
				gf_fclose(profile);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[HTTP] Error while loading Profile file %s.", user_profile));
			}
		}

		sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = len+par.size;

#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			u32 writelen = len+par.size;
			e = GF_OK;
			if (writelen != SSL_write(sess->ssl, tmp_buf, writelen))
				e = GF_IP_NETWORK_FAILURE;
		} else
#endif
			e = gf_sk_send(sess->sock, tmp_buf, len+par.size);

		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Sending request at UTC "LLD" %s\n\n", gf_net_get_utc(), tmp_buf));
		gf_free(tmp_buf);
	} else {
		u32 len = (u32) strlen(sHTTP);

		sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = len;

#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			e = GF_OK;
			if (len != SSL_write(sess->ssl, sHTTP, len))
				e = GF_IP_NETWORK_FAILURE;
		} else
#endif
			e = gf_sk_send(sess->sock, sHTTP, len);

#ifndef GPAC_DISABLE_LOG
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Error sending request %s\n", gf_error_to_string(e) ));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Sending request at UTC "LLU" %s\n\n", gf_net_get_utc(), sHTTP));
		}
#endif
	}

	if (e) {
		sess->status = GF_NETIO_STATE_ERROR;
		sess->last_error = e;
		gf_dm_sess_notify_state(sess, GF_NETIO_STATE_ERROR, e);
		return e;
	}

	sess->status = GF_NETIO_WAIT_FOR_REPLY;
	gf_dm_sess_notify_state(sess, GF_NETIO_WAIT_FOR_REPLY, GF_OK);
	return GF_OK;
}



static Bool dm_exceeds_cap_rate(GF_DownloadManager * dm)
{
	u32 cumul_rate = 0;
	u32 nb_sess = 0;
	u32 i, count = gf_list_count(dm->sessions);

	//check if this fits with all other sessions
	for (i=0; i<count; i++) {
		GF_DownloadSession * sess = (GF_DownloadSession*)gf_list_get(dm->sessions, i);

		//session not running done
		if (sess->status != GF_NETIO_DATA_EXCHANGE) continue;

		dm_sess_update_download_rate(sess, GF_FALSE);
		cumul_rate += sess->bytes_per_sec;
		nb_sess ++;
	}
	if ( cumul_rate >= nb_sess * dm->limit_data_rate)
		return GF_TRUE;

	return GF_FALSE;
}

/*!
 * Parse the remaining part of body
 * \param sess The session
 * \param sHTTP the data buffer
 * \return The error code if any
 */
static GF_Err http_parse_remaining_body(GF_DownloadSession * sess, char * sHTTP)
{
	u32 size;
	GF_Err e;
	u32 buf_size = sess->dm ? sess->dm->read_buf_size : GF_DOWNLOAD_BUFFER_SIZE;

	while (1) {
		u32 remaining_data_size;
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] No HTTP chunk header found for %d bytes, assuming broken chunk transfer and aborting\n", sess->remaining_data_size));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			memcpy(sHTTP, sess->remaining_data, sess->remaining_data_size);
		}

		e = gf_dm_read_data(sess, sHTTP + sess->remaining_data_size, buf_size - sess->remaining_data_size, &size);
		if (e!= GF_IP_CONNECTION_CLOSED && (!size || e == GF_IP_NETWORK_EMPTY)) {
			if (e == GF_IP_CONNECTION_CLOSED || (!sess->total_size && !sess->chunked && (gf_sys_clock_high_res() - sess->start_time > 5000000))) {
				sess->total_size = sess->bytes_done;
				gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
				assert(sess->server_name);
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Disconnected from %s: %s\n", sess->server_name, gf_error_to_string(e)));
				gf_dm_disconnect(sess, (e == GF_IP_CONNECTION_CLOSED) ? GF_TRUE : GF_FALSE);
			}
			return GF_OK;
		}

		if (e) {
			if (sess->sock && (e == GF_IP_CONNECTION_CLOSED)) {
				u32 len = gf_cache_get_content_length(sess->cache_entry);
				if (size > 0)
					gf_dm_data_received(sess, (u8 *) sHTTP, size, GF_FALSE, NULL, NULL);
				if ( ( (len == 0) && sess->use_cache_file)
				        /*ivica patch*/
				        || (size==0)
				   ) {
					sess->total_size = sess->bytes_done;
					// HTTP 1.1 without content length...
					gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
					assert(sess->server_name);
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Disconnected from %s: %s\n", sess->server_name, gf_error_to_string(e)));
					if (sess->use_cache_file)
						gf_cache_set_content_length(sess->cache_entry, sess->bytes_done);
					e = GF_OK;
				}
			}
			gf_dm_disconnect(sess, GF_TRUE);
			sess->last_error = e;
			gf_dm_sess_notify_state(sess, sess->status, e);
			return e;
		}

		remaining_data_size = sess->remaining_data_size;
		if (sess->remaining_data && sess->remaining_data_size) {
			gf_free(sess->remaining_data);
			sess->remaining_data = NULL;
			sess->remaining_data_size = 0;
		}
		sHTTP[size + remaining_data_size] = 0;

		gf_dm_data_received(sess, (u8 *) sHTTP, size + remaining_data_size, GF_FALSE, NULL, NULL);

		/*socket empty*/
		if (size < buf_size) {
			return GF_OK;
		}
	}
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

/*!
 * Waits for the response HEADERS, parse the information... and so on
 * \param sess The session
 * \param sHTTP the data buffer
 */
static GF_Err wait_for_header_and_parse(GF_DownloadSession *sess, char * sHTTP)
{
	GF_NETIO_Parameter par;
	s32 bytesRead, BodyStart;
	u32 res, i, buf_size;
	s32 LinePos, Pos;
	u32 rsp_code, ContentLength, first_byte, last_byte, total_size, range, no_range;
	Bool connection_closed = GF_FALSE;
	char buf[1025];
	char comp[400];
	GF_Err e;
	char * new_location;
	const char * mime_type;
	assert( sess->status == GF_NETIO_WAIT_FOR_REPLY );
	bytesRead = res = 0;
	new_location = NULL;
	if (!(sess->flags & GF_NETIO_SESSION_NOT_CACHED)) {
		sess->use_cache_file = GF_TRUE;
	}

	if (sess->from_cache_only) {
		GF_NETIO_Parameter par;
		sess->reply_time = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
		sess->rsp_hdr_size = 0;
		sess->total_size = sess->bytes_done = gf_cache_get_content_length(sess->cache_entry);

		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = GF_NETIO_DATA_TRANSFERED;
		par.error = GF_OK;
		gf_dm_sess_user_io(sess, &par);
		gf_dm_disconnect(sess, GF_FALSE);
		return GF_OK;
	}

	buf_size = sess->dm ? sess->dm->read_buf_size : GF_DOWNLOAD_BUFFER_SIZE;

	//always set start time to the time at last attempt reply parsing
	sess->start_time = gf_sys_clock_high_res();
	sess->start_time_utc = gf_net_get_utc();
	sess->chunked = GF_FALSE;
	sess->chunk_run_time = 0;
	sess->last_chunk_found = GF_FALSE;
	gf_sk_reset(sess->sock);
	sHTTP[0] = 0;
	
	while (1) {
		e = gf_dm_read_data(sess, sHTTP + bytesRead, buf_size - bytesRead, &res);
		switch (e) {
		case GF_IP_NETWORK_EMPTY:
			if (!bytesRead) {
				if (gf_sys_clock_high_res() - sess->request_start_time > 1000 * sess->dm->request_timeout) {
					sess->last_error = GF_IP_NETWORK_EMPTY;
					sess->status = GF_NETIO_STATE_ERROR;
					return GF_IP_NETWORK_EMPTY;
				}
				assert(res==0);
				return GF_OK;
			}
			continue;
		/*socket has been closed while configuring, retry (not sure if the server got the GET)*/
		case GF_IP_CONNECTION_CLOSED:
			if (sess->http_read_type == HEAD) {
				/* Some servers such as shoutcast directly close connection if HEAD or an unknown method is issued */
				sess->server_only_understand_get = GF_TRUE;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Connection closed by server when getting %s - retrying\n", sess->remote_path));
			gf_dm_disconnect(sess, GF_TRUE);

			if (sess->num_retry)
				sess->status = GF_NETIO_SETUP;
			else {
				sess->last_error = e;
				sess->status = GF_NETIO_STATE_ERROR;
			}
			return e;
		case GF_OK:
			if (!res)
				return GF_OK;
			break;
		default:
			goto exit;
		}
		bytesRead += res;

		/*locate body start*/
		BodyStart = gf_token_find(sHTTP, 0, bytesRead, "\r\n\r\n");
		if (BodyStart > 0) {
			BodyStart += 4;
			break;
		}
		BodyStart = gf_token_find(sHTTP, 0, bytesRead, "\n\n");
		if (BodyStart > 0) {
			BodyStart += 2;
			break;
		}
	}
	if (bytesRead < 0) {
		e = GF_REMOTE_SERVICE_ERROR;
		goto exit;
	}
	if (!BodyStart)
		BodyStart = bytesRead;

	sHTTP[BodyStart-1] = 0;
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] %s\n\n", sHTTP));

	sess->reply_time = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
	sess->rsp_hdr_size = BodyStart;

	LinePos = gf_token_get_line(sHTTP, 0, bytesRead, buf, 1024);
	Pos = gf_token_get(buf, 0, " \t\r\n", comp, 400);

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

	no_range = range = ContentLength = first_byte = last_byte = total_size = 0;
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
	}

	//default pre-processing of headers - needs cleanup, not all of these have to be parsed before checking reply code
	for (i=0; i<gf_list_count(sess->headers); i++) {
		char *val;
		GF_HTTPHeader *hdrp = (GF_HTTPHeader*)gf_list_get(sess->headers, i);

		if (!stricmp(hdrp->name, "Content-Length") ) {
			ContentLength = (u32) atoi(hdrp->value);

			if (rsp_code<300)
				gf_cache_set_content_length(sess->cache_entry, ContentLength);

		}
		else if (!stricmp(hdrp->name, "Content-Type")) {
			char *mime = gf_strdup(hdrp->value);
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
		else if (!stricmp(hdrp->name, "Content-Range")) {
			if (!strncmp(hdrp->value, "bytes", 5)) {
				val = hdrp->value + 5;
				if (val[0] == ':') val += 1;
				while (val[0] == ' ') val += 1;

				if (val[0] == '*') {
					sscanf(val, "*/%u", &total_size);
				} else {
					sscanf(val, "%u-%u/%u", &first_byte, &last_byte, &total_size);
				}
			}
		}
		else if (!stricmp(hdrp->name, "Accept-Ranges")) {
			if (strstr(hdrp->value, "none")) no_range = 1;
		}
		else if (!stricmp(hdrp->name, "Location"))
			new_location = gf_strdup(hdrp->value);
		else if (!strnicmp(hdrp->name, "ice", 3) || !strnicmp(hdrp->name, "icy", 3) ) {
			/* For HTTP icy servers, we disable cache */
			if (sess->icy_metaint == 0)
				sess->icy_metaint = -1;
			sess->use_cache_file = GF_FALSE;
			if (!stricmp(hdrp->name, "icy-metaint")) {
				sess->icy_metaint = atoi(hdrp->value);
			}
		}
		else if (!stricmp(hdrp->name, "Cache-Control")) {
		}
		else if (!stricmp(hdrp->name, "ETag")) {
			if (rsp_code<300)
				gf_cache_set_etag_on_server(sess->cache_entry, hdrp->value);
		}
		else if (!stricmp(hdrp->name, "Last-Modified")) {
			if (rsp_code<300)
				gf_cache_set_last_modified_on_server(sess->cache_entry, hdrp->value);
		}
		else if (!stricmp(hdrp->name, "Transfer-Encoding")) {
			if (!stricmp(hdrp->value, "chunked"))
				sess->chunked = GF_TRUE;
		}
		else if (!stricmp(hdrp->name, "X-UserProfileID") ) {
			gf_opts_set_key("core", "user-profileid", hdrp->value);
		}
		else if (!stricmp(hdrp->name, "Connection") ) {
			if (strstr(hdrp->value, "close"))
				connection_closed = GF_TRUE;
		}

		if (sess->status==GF_NETIO_DISCONNECTED) return GF_OK;
	}
	if (no_range) first_byte = 0;

	gf_cache_set_headers_processed(sess->cache_entry);

	par.msg_type = GF_NETIO_PARSE_REPLY;
	par.error = GF_OK;
	par.reply = rsp_code;
	par.value = comp;
	/*
	 * If response is correct, it means our credentials are correct
	 */
	if (sess->creds && rsp_code != 304)
		sess->creds->valid = GF_TRUE;


	/*try to flush body */
	if (rsp_code>=300) {
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
	//remember if we can keep the session alive after the transfer is done
	sess->connection_close = connection_closed;

	switch (rsp_code) {
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
		gf_dm_disconnect(sess, GF_TRUE);
		sess->status = GF_NETIO_SETUP;
		e = gf_dm_sess_setup_from_url(sess, new_location, GF_FALSE);
		if (e) {
			sess->status = GF_NETIO_STATE_ERROR;
			sess->last_error = e;
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

		gf_dm_disconnect(sess, GF_FALSE);
		if (sess->user_proc) {
			/* For modules that do not use cache and have problems with GF_NETIO_DATA_TRANSFERED ... */
			const char * filename;
			FILE * f;
			filename = gf_cache_get_cache_filename(sess->cache_entry);
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Sending data to modules from %s...\n", filename));
			f = gf_fopen(filename, "rb");
			assert(filename);
			if (!f) {
				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] FAILED to open cache file %s for reading contents !\n", filename));
				/* Ooops, no cache, redowload everything ! */
				gf_dm_disconnect(sess, GF_FALSE);
				sess->status = GF_NETIO_SETUP;
				e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
				sess->total_size = gf_cache_get_cache_filesize(sess->cache_entry);
				if (e) {
					sess->status = GF_NETIO_STATE_ERROR;
					sess->last_error = e;
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
				u32 total_size = gf_cache_get_cache_filesize(sess->cache_entry);
				do {
					read = (s32) fread(file_cache_buff, sizeof(char), 16384, f);
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
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] all data has been sent to modules from %s.\n", filename));
		}
		/* Cache file is the most recent */
		sess->status = GF_NETIO_DATA_TRANSFERED;
		par.error = GF_OK;
		gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
		gf_dm_disconnect(sess, GF_FALSE);
		return GF_OK;
	}
	case 401:
	{
		/* Do we have a credentials struct ? */
		sess->creds = gf_user_credentials_register(sess->dm, sess->server_name, NULL, NULL, GF_FALSE);
		if (!sess->creds) {
			/* User credentials have not been filled properly, we have to abort */
			gf_dm_disconnect(sess, GF_TRUE);
			sess->status = GF_NETIO_STATE_ERROR;
			par.error = GF_AUTHENTICATION_FAILURE;
			par.msg_type = GF_NETIO_DISCONNECTED;
			gf_dm_sess_user_io(sess, &par);
			e = GF_AUTHENTICATION_FAILURE;
			sess->last_error = e;
			goto exit;
		}
		gf_dm_disconnect(sess, GF_FALSE);
		sess->status = GF_NETIO_SETUP;
		e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
		if (e) {
			sess->status = GF_NETIO_STATE_ERROR;
			sess->last_error = e;
			gf_dm_sess_notify_state(sess, sess->status, e);
		}
		return e;
	}
	case 404:
		/* File not found */
		gf_dm_sess_user_io(sess, &par);
		if ((BodyStart < (s32) bytesRead)) {
			sHTTP[bytesRead] = 0;
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[HTTP] Failure - body: %s\n", sHTTP + BodyStart));
		}
		notify_headers(sess, sHTTP, bytesRead, BodyStart);
		e = GF_URL_ERROR;
		goto exit;
		break;
	case 416:
		/* Range not accepted */
		gf_dm_sess_user_io(sess, &par);

		notify_headers(sess, sHTTP, bytesRead, BodyStart);
		e = GF_SERVICE_ERROR;
		goto exit;
		break;
	case 400:
	case 501:
		/* Method not implemented ! */
		if (sess->http_read_type == HEAD) {
			/* Since HEAD is not understood by this server, we use a GET instead */
			sess->http_read_type = GET;
			sess->flags |= GF_NETIO_SESSION_NOT_CACHED;
			gf_dm_disconnect(sess, GF_FALSE);
			sess->status = GF_NETIO_SETUP;
			sess->server_only_understand_get = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("Method not supported, try with GET.\n"));
			e = gf_dm_sess_setup_from_url(sess, sess->orig_url, GF_FALSE);
			if (e) {
				sess->status = GF_NETIO_STATE_ERROR;
				sess->last_error = e;
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
			gf_dm_disconnect(sess, GF_TRUE);
			sess->status = GF_NETIO_SETUP;
			return GF_OK;
		}
	default:
		gf_dm_sess_user_io(sess, &par);
		notify_headers(sess, sHTTP, bytesRead, BodyStart);
		e = GF_REMOTE_SERVICE_ERROR;
		goto exit;
	}

	notify_headers(sess, NULL, bytesRead, BodyStart);

	if (sess->http_read_type != GET)
		sess->use_cache_file = GF_FALSE;

	if (sess->http_read_type==HEAD) {
		gf_dm_disconnect(sess, GF_FALSE);
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

#ifndef GPAC_DISABLE_LOGS
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Error processing rely from %s: %s\n", sess->server_name, gf_error_to_string(e) ) );
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] Reply processed from %s\n", sess->server_name ) );
	}
#endif

	/*some servers may reply without content length, but we MUST have it*/
	if (e) goto exit;
	if (sess->icy_metaint != 0) {
		assert( ! sess->use_cache_file );
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] ICY protocol detected\n"));
		if (mime_type && !stricmp(mime_type, "video/nsv")) {
			gf_cache_set_mime_type(sess->cache_entry, "audio/aac");
		}
		sess->icy_bytes = 0;
		sess->total_size = SIZE_IN_STREAM;
		sess->status = GF_NETIO_DATA_EXCHANGE;
	} else if (!ContentLength && !sess->chunked) {
		if (sess->http_read_type == GET) {
			sess->total_size = SIZE_IN_STREAM;
			sess->use_cache_file = GF_FALSE;
			sess->status = GF_NETIO_DATA_EXCHANGE;
			sess->bytes_done = 0;
		} else {
			gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
			gf_dm_disconnect(sess, GF_FALSE);
			return GF_OK;
		}
	} else {
		sess->total_size = ContentLength;
		if (sess->use_cache_file && sess->http_read_type == GET ) {
			e = gf_cache_open_write_cache(sess->cache_entry, sess);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ( "[CACHE] Failed to open cache, error=%d\n", e));
				goto exit;
			}
		}
		sess->status = GF_NETIO_DATA_EXCHANGE;
		sess->bytes_done = 0;
	}


	/* we may have existing data in this buffer ... */
	if (!e && (BodyStart < (s32) bytesRead)) {
		if (sess->init_data) gf_free(sess->init_data);
		sess->init_data_size = 0;
		sess->init_data = NULL;

		gf_dm_data_received(sess, (u8 *) sHTTP + BodyStart, bytesRead - BodyStart, GF_TRUE, NULL, NULL);
	}
exit:
	if (e) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[HTTP] Error parsing reply: %s for URL %s\nReply was:\n%s\n", gf_error_to_string(e), sess->orig_url, sHTTP ));
		gf_cache_entry_set_delete_files_when_deleted(sess->cache_entry);
		gf_dm_remove_cache_entry_from_session(sess);
		sess->cache_entry = NULL;
		gf_dm_disconnect(sess, GF_FALSE);
		sess->status = GF_NETIO_STATE_ERROR;
		sess->last_error = e;
		gf_dm_sess_notify_state(sess, sess->status, e);
		return e;
	}
	/*DO NOT call parse_body yet, as the final user may not be connected to our session*/
	return GF_OK;
}

/**
 * Default performing behaviour
 * \param sess The session
 */
void http_do_requests(GF_DownloadSession *sess)
{
	char sHTTP[GF_DOWNLOAD_BUFFER_SIZE+1];

	if (sess->reused_cache_entry) {
		//main session is done downloading, notify - to do we should send progress events on this session also ...
		if (!gf_cache_is_in_progress(sess->cache_entry)) {
			GF_NETIO_Parameter par;
			gf_dm_disconnect(sess, GF_FALSE);
			sess->reused_cache_entry = GF_FALSE;
			memset(&par, 0, sizeof(GF_NETIO_Parameter));
			par.msg_type = GF_NETIO_DATA_TRANSFERED;
			par.error = GF_OK;
			gf_dm_sess_user_io(sess, &par);
		}
		return;
	}

	switch (sess->status) {
	case GF_NETIO_CONNECTED:
		http_send_headers(sess, sHTTP);
		break;
	case GF_NETIO_WAIT_FOR_REPLY:
		wait_for_header_and_parse(sess, sHTTP);
		break;
	case GF_NETIO_DATA_EXCHANGE:
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
		s32 written = (u32) gf_fwrite( param->data, sizeof(char), param->size, f);
		if (written != param->size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("Failed to write data on disk\n"));
		}
	}
}

GF_EXPORT
GF_Err gf_dm_wget(const char *url, const char *filename, u64 start_range, u64 end_range, char **redirected_url)
{
	GF_Err e;
	GF_DownloadManager * dm = NULL;
	dm = gf_dm_new(NULL);
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[WGET] Failed to open file %s for write.\n", filename));
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
 *\brief fetches remote file in memory
 *
 *Fetches remote file in memory.
 *\param url the data to fetch
 *\param out_data output data (allocated by function)
 *\param out_size output data size
 *\param out_mime if not NULL, pointer will contain the mime type (allocated by function)
 *\return error code if any
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
	f = gf_temp_file_new(&f_fn);
	if (!f) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[WGET] Failed to create temp file for write.\n"));
		return GF_IO_ERR;
	}

	dm = gf_dm_new(NULL);
	if (!dm) {
		gf_fclose(f);
		gf_delete_file(f_fn);
		return GF_OUT_OF_MEM;
	}

	dnload = gf_dm_sess_new_simple(dm, (char *)url, GF_NETIO_SESSION_NOT_THREADED, &wget_NetIO, f, &e);
	if (!dnload) {
		gf_dm_del(dm);
		gf_fclose(f);
		gf_delete_file(f_fn);
		return GF_BAD_PARAM;
	}
	dnload->use_cache_file = GF_FALSE;
	dnload->disable_cache = GF_TRUE;
	if (!e)
		e = gf_dm_sess_process(dnload);

	if (!e)
		e = gf_cache_close_write_cache(dnload->cache_entry, dnload, e == GF_OK);

	if (!e) {
		u32 size = (u32) ftell(f);
		s32 read;
		*out_size = size;
		*out_data = (char*)gf_malloc(sizeof(char)* ( 1 + size));
		fseek(f, 0, SEEK_SET);
		read = (s32) fread(*out_data, 1, size, f);
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
	gf_delete_file(f_fn);
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
 *\brief Get session original resource url
 *
 *Returns the original resource URL before any redirection associated with the session
 *\param sess the download session
 *\return the associated URL
 */
const char *gf_dm_sess_get_original_resource_name(GF_DownloadSession *dnload)
{
	if (dnload) return dnload->orig_url_before_redirect ? dnload->orig_url_before_redirect : dnload->orig_url;
	return NULL;
}

/*!
 *\brief fetch session status
 *
 *Fetch the session current status
 *\param sess the download session
 *\return the session status*/
u32 gf_dm_sess_get_status(GF_DownloadSession *dnload)
{
	return dnload ? dnload->status : GF_NETIO_STATE_ERROR;
}


/*!
 *\brief Reset session
 *
 *Resets the session for new processing of the same url
 *\param sess the download session
 *\return error code if any
 */
GF_Err gf_dm_sess_reset(GF_DownloadSession *sess)
{
	if (!sess) return GF_BAD_PARAM;
	sess->status = GF_NETIO_SETUP;
	sess->needs_range = GF_FALSE;
	sess->range_start = sess->range_end = 0;
	sess->bytes_done = sess->bytes_per_sec = 0;
	if (sess->init_data) gf_free(sess->init_data);
	sess->init_data = NULL;
	sess->init_data_size = 0;
	sess->last_error = GF_OK;
	sess->total_size = 0;
	sess->start_time = 0;
	sess->start_time_utc = 0;
	return GF_OK;
}

/*!
 * Get a range of a cache entry file
 * \param sess The session
 * \param startOffset The first byte of the request to get
 * \param endOffset The last byte of request to get
 * \return The temporary name for the file created to have a range of the file
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
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Cannot open partial cache file %s for write\n", newFilename));
			gf_free( newFilename );
			return NULL;
		}
		fr = gf_fopen(orig, "rb");
		if (!fr) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Cannot open full cache file %s\n", orig));
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Cannot seek at right start offset in %s\n", orig));
				gf_fclose( fr );
				gf_fclose( fw );
				gf_free( newFilename );
				return NULL;
			}
			do {
				read = fread(copyBuff, sizeof(char), MIN(sizeof(copyBuff), (size_t)  total), fr);
				if (read > 0) {
					total-= read;
					write = gf_fwrite(copyBuff, sizeof(char), (size_t) read, fw);
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
 * \param sess The session
 * \param flags The new flags for the session - if flags is 0xFFFFFFFF, existing flags are not modified
 * \param user_io The new callback function
 * \param cbk The new user data to ba used in the callback function
 * \return GF_OK or error
 */
GF_Err gf_dm_sess_reassign(GF_DownloadSession *sess, u32 flags, gf_dm_user_io user_io, void *cbk)
{
	/*shall only be called for non-threaded sessions!! */
	if (sess->th) return GF_BAD_PARAM;

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
		gf_opts_set_key("core", "maxrate", opt);

		dm->read_buf_size = GF_DOWNLOAD_BUFFER_SIZE;
		//when rate is limited, use smaller smaller read size
		if (dm->limit_data_rate) dm->read_buf_size = 1024;

#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_test_mode()) {
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
		if (sess->total_size==sess->bytes_done) {
			if (gf_sys_clock_high_res() - sess->start_time>2000000) {
				continue;
			}
		}
		dm_sess_update_download_rate(sess, GF_FALSE);
		ret += sess->bytes_per_sec;
	}
	gf_mx_v(dm->cache_mx);
	return 8*ret;
}

GF_EXPORT
const char *gf_dm_sess_get_header(GF_DownloadSession *sess, const char *name)
{
	u32 i, count;
	if( !sess || !name) return NULL;
	count = gf_list_count(sess->headers);
	for (i=0; i<count; i++) {
		GF_HTTPHeader *header = (GF_HTTPHeader*)gf_list_get(sess->headers, i);
		if (!strcmp(header->name, name)) return header->value;
	}
	return NULL;
}

GF_EXPORT
GF_Err gf_dm_sess_enum_headers(GF_DownloadSession *sess, u32 *idx, const char **hdr_name, const char **hdr_val)
{
	GF_HTTPHeader *hdr;
	if( !sess || !idx || !hdr_name || !hdr_val) return GF_BAD_PARAM;
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
	if (!sess) return GF_BAD_PARAM;

	if (req_hdr_size) *req_hdr_size = sess->req_hdr_size;
	if (rsp_hdr_size) *rsp_hdr_size = sess->rsp_hdr_size;
	if (connect_time) *connect_time = sess->connect_time;
	if (reply_time) *reply_time = sess->reply_time;
	if (download_time) *download_time = sess->total_time_since_req;
	return GF_OK;
}

GF_EXPORT
void gf_dm_sess_force_memory_mode(GF_DownloadSession *sess)
{
	if (sess) sess->flags |= GF_NETIO_SESSION_MEMORY_CACHE;
}


GF_EXPORT
GF_Err gf_dm_set_localcache_provider(GF_DownloadManager *dm, Bool (*local_cache_url_provider_cbk)(void *udta, char *url, Bool is_cache_destroy), void *lc_udta)
{
	if (!dm) return GF_BAD_PARAM;
	dm->local_cache_url_provider_cbk = local_cache_url_provider_cbk;
	dm->lc_udta = lc_udta;
	return GF_OK;

}

Bool gf_cache_set_mime(const DownloadedCacheEntry entry, const char *mime);
Bool gf_cache_set_range(const DownloadedCacheEntry entry, u64 size, u64 start_range, u64 end_range);
Bool gf_cache_set_content(const DownloadedCacheEntry entry, char *data, u32 size, Bool copy);
Bool gf_cache_set_headers(const DownloadedCacheEntry entry, const char *headers);
Bool gf_cache_set_downtime(const DownloadedCacheEntry entry, u32 download_time_ms);

GF_EXPORT
const DownloadedCacheEntry gf_dm_add_cache_entry(GF_DownloadManager *dm, const char *szURL, u8 *data, u64 size, u64 start_range, u64 end_range,  const char *mime, Bool clone_memory, u32 download_time_ms)
{
	u32 i, count;
	DownloadedCacheEntry the_entry = NULL;

	gf_mx_p(dm->cache_mx );
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
		the_entry = gf_cache_create_entry(dm, "", szURL, 0, 0, GF_TRUE);
		if (!the_entry) return NULL;
		gf_list_add(dm->cache_entries, the_entry);
	}

	gf_cache_set_mime(the_entry, mime);
	gf_cache_set_range(the_entry, size, start_range, end_range);
	gf_cache_set_content(the_entry, data, (u32) size, clone_memory ? GF_TRUE : GF_FALSE);
	gf_cache_set_downtime(the_entry, download_time_ms);

	gf_mx_v(dm->cache_mx );
	return the_entry;
}

GF_EXPORT
GF_Err gf_dm_force_headers(GF_DownloadManager *dm, const DownloadedCacheEntry entry, const char *headers)
{
	u32 i, count;
	Bool res;
	if (!entry) return GF_BAD_PARAM;
	gf_mx_p(dm->cache_mx);
	res = gf_cache_set_headers(entry, headers);
	count = gf_list_count(dm->sessions);
	for (i=0; i<count; i++) {
		GF_DownloadSession *sess = gf_list_get(dm->sessions, i);
		if (sess->cache_entry != entry) continue;
		gf_dm_sess_reload_cached_headers(sess);
	}

#ifdef GPAC_ENABLE_COVERAGE
	if (!count && gf_sys_is_test_mode()) {
		gf_dm_sess_reload_cached_headers(NULL);
		gf_dm_refresh_cache_entry(NULL);
		gf_dm_session_thread(NULL);
		gf_user_credentials_save_digest(NULL, NULL, NULL);
		gf_user_credentials_ask_password(NULL, NULL);
		gf_user_credentials_register(NULL, NULL, NULL, NULL, GF_FALSE);
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
	return res ? GF_OK : GF_BAD_PARAM;
}

#endif
