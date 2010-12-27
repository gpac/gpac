/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *				Copyright (c) 2005-2005 ENST
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
#include <gpac/crypt.h>
#include <gpac/tools.h>
#include <gpac/cache.h>


#ifdef GPAC_HAS_SSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#ifdef __USE_POSIX
#include <unistd.h>
#endif


static void gf_dm_connect(GF_DownloadSession *sess);

#define GF_DOWNLOAD_AGENT_NAME		"GPAC/" GPAC_FULL_VERSION
#define GF_DOWNLOAD_BUFFER_SIZE		8192

/*internal flags*/
enum
{
    GF_DOWNLOAD_SESSION_USE_SSL		=	1<<10,
    GF_DOWNLOAD_SESSION_THREAD_DEAD	=	1<<11
};

typedef struct __gf_user_credentials
{
    char site[1024];
    char username[50];
    char digest[1024];
    Bool valid;
} gf_user_credentials_struct;

enum REQUEST_TYPE {
    GET = 0,
    HEAD = 1,
    OTHER = 2
};

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

    char *server_name;
    u16 port;

    char *orig_url;
    char *remote_path;
    gf_user_credentials_struct * creds;
    char cookie[GF_MAX_PATH];
    DownloadedCacheEntry cache_entry;

    GF_Socket *sock;
    u32 num_retry, status;

    u32 flags;

    u32 total_size, bytes_done, start_time, icy_metaint, icy_count, icy_bytes;
    u32 bytes_per_sec, window_start, bytes_in_wnd;
    u32 limit_data_rate;

    /* Range information if needed for the download (cf flag) */
    Bool needs_range;
    u32 range_start, range_end;

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

#ifdef GPAC_HAS_SSL
    SSL *ssl;
#endif

    void (*do_requests)(struct __gf_download_session *);

    /*callback for data reception - may not be NULL*/
    gf_dm_user_io user_proc;
    void *usr_cbk;

    /*private extension*/
    void *ext;
};

struct __gf_download_manager
{
    GF_Mutex *cache_mx;
    char *cache_directory;
    char szCookieDir[GF_MAX_PATH];

    Bool (*GetUserPassword)(void *usr_cbk, const char *site_url, char *usr_name, char *password);
    void *usr_cbk;

    GF_Config *cfg;
    GF_List *sessions;

    GF_List *skip_proxy_servers;
    GF_List *credentials;
    GF_List *cache_entries;
    /* FIXME : should be placed in DownloadedCacheEntry maybe... */
    GF_List * partial_downloads;
#ifdef GPAC_HAS_SSL
    SSL_CTX *ssl_ctx;
#endif

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

#ifdef WIN32
    RAND_screen ();
    if (RAND_status ())
        return;
#endif
}

#endif

/*
 * Private methods of cache.c
 */

/**
 * \brief Write data to cache
 * Writes data to the cache. A call to gf_cache_open_write_cache should have been issued before calling this function.
 * \param entry The entry to use
 * \param data data to write
 * \param size number of elements to write
 * \return GF_OK is everything went fine, GF_BAD_PARAM if cache has not been opened, GF_IO_ERROR if a failure occurs
 */
GF_Err gf_cache_write_to_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, const char * data, const u32 size);

/**
 * \brief Close the write file pointer of cache
 * This function also flushes all buffers, so cache will always be consistent after
 * \param entry The entry to use
 * \param success 1 if cache write is success, false otherwise
 * \return GF_OK is everything went fine, GF_BAD_PARAM if entry is NULL, GF_IO_ERROR if a failure occurs
 */
GF_Err gf_cache_close_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess, Bool success);

/**
 * \brief Open the write file pointer of cache
 * This function prepares calls for gf_cache_write_to_cache
 * \param entry The entry to use
 * \return GF_OK is everything went fine, GF_BAD_PARAM if entry is NULL, GF_IO_ERROR if a failure occurs
 */
GF_Err gf_cache_open_write_cache( const DownloadedCacheEntry entry, const GF_DownloadSession * sess );

/**
 * Find a User's credentials for a given site
 */
static
gf_user_credentials_struct * gf_find_user_credentials_for_site(GF_DownloadManager *dm, const char * server_name) {
    u32 count, i;
    if (!dm || !dm->credentials || !server_name || !strlen(server_name))
        return NULL;
    count = gf_list_count( dm->credentials);
    for (i = 0 ; i < count; i++) {
        gf_user_credentials_struct * cred = gf_list_get(dm->credentials, i );
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
static
GF_Err gf_user_credentials_save_digest( GF_DownloadManager * dm, gf_user_credentials_struct * creds, const char * password) {
    int size;
    char pass_buf[1024], range_buf[1024];
    if (!dm || !creds || !password)
        return GF_BAD_PARAM;
    sprintf(pass_buf, "%s:%s", creds->username, password);
    size = gf_base64_encode(pass_buf, strlen(pass_buf), range_buf, 1024);
    range_buf[size] = 0;
    strcpy(creds->digest, range_buf);
    creds->valid = 1;
    return GF_OK;
}

/**
 * \brief Asks the user for credentials for given site
 * \param dm The download manager
 * \param creds The credentials to fill
 * \return GF_OK if info has been filled, GF_BAD_PARAM if creds == NULL or dm == NULL, GF_AUTHENTICATION_FAILURE if user did not filled the info.
 */
static
GF_Err gf_user_credentials_ask_password( GF_DownloadManager * dm, gf_user_credentials_struct * creds)
{
    char szPASS[50];
    if (!dm || !creds)
        return GF_BAD_PARAM;
    memset(szPASS, 0, 50);
    if (!dm->GetUserPassword || !dm->GetUserPassword(dm->usr_cbk, creds->site, creds->username, szPASS)) {
        return GF_AUTHENTICATION_FAILURE;
    }
    return gf_user_credentials_save_digest(dm, creds, szPASS);
    return GF_OK;
}


static
gf_user_credentials_struct * gf_user_credentials_register(GF_DownloadManager * dm, const char * server_name, const char * username, const char * password, Bool valid)
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

static Bool _ssl_is_initialized = 0;

/*!
 * initialize the SSL library once for all download managers
 * \return 0 if everyhing is OK, 1 otherwise
 */
static Bool init_ssl_lib() {
    if (_ssl_is_initialized)
        return 0;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTPS] Initializing SSL library...\n"));
    init_prng();
    if (RAND_status() != 1) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTPS] Error while initializing Random Number generator, failed to init SSL !\n"));
        return 1;
    }
    SSL_library_init();
    SSL_load_error_strings();
    SSLeay_add_all_algorithms();
    SSLeay_add_ssl_algorithms();
    _ssl_is_initialized = 1;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTPS] Initalization of SSL library complete.\n"));
    return 0;
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
    case 0:
        meth = SSLv23_client_method();
        break;
    case 1:
        meth = SSLv2_client_method();
        break;
    case 2:
        meth = SSLv3_client_method();
        break;
    case 3:
        meth = TLSv1_client_method();
        break;
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
    if (!strnicmp(url, "file://", 7)) return 1;
    if (!strnicmp(url, "file:///", 8)) return 1;
    if (!strstr(url, "://")) return 1;
    return 0;
}

static Bool gf_dm_can_handle_url(GF_DownloadManager *dm, const char *url)
{
    if (!strnicmp(url, "http://", 7)) return 1;
#ifdef GPAC_HAS_SSL
    if (!strnicmp(url, "https://", 8)) return 1;
#endif
    return 0;
}

/*!
 * Finds an existing entry in the cache for a given URL
 * \param sess The session configured with the URL
 * \return NULL if none found, the DownloadedCacheEntry otherwise
 */
DownloadedCacheEntry gf_dm_find_cached_entry_by_url(GF_DownloadSession * sess) {
    u32 i, count;
    assert( sess && sess->dm && sess->dm->cache_entries );
    gf_mx_p( sess->dm->cache_mx );
    count = gf_list_count(sess->dm->cache_entries);
    for (i = 0 ; i < count; i++) {
        const char * url;
        DownloadedCacheEntry e = gf_list_get(sess->dm->cache_entries, i);
        assert(e);
        url = gf_cache_get_url(e);
        assert( url );
        if (!strcmp(url, sess->orig_url)) {
            gf_mx_v( sess->dm->cache_mx );
            return e;
        }
    }
    gf_mx_v( sess->dm->cache_mx );
    return NULL;
}

/**
 * Creates a new cache entry
 * \param cache_directory The path to the directory containing cache files
 * \param url The full URL
 * \return The DownloadedCacheEntry
 */
DownloadedCacheEntry gf_cache_create_entry( GF_DownloadManager * dm, const char * cache_directory, const char * url);

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
void gf_dm_remove_cache_entry_from_session(GF_DownloadSession * sess) {
    if (sess && sess->cache_entry) {
        gf_cache_remove_session_from_cache_entry(sess->cache_entry, sess);
        if (sess->dm && gf_cache_entry_is_delete_files_when_deleted(sess->cache_entry) &&
                (0 == gf_cache_get_sessions_count_for_cache_entry(sess->cache_entry)))
        {
            u32 i, count;
            gf_mx_p( sess->dm->cache_mx );
            count = gf_list_count( sess->dm->cache_entries );
            for (i = 0; i < count; i++) {
                DownloadedCacheEntry ex = gf_list_get(sess->dm->cache_entries, i);
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

void gf_dm_configure_cache(GF_DownloadSession *sess)
{
    DownloadedCacheEntry entry;
    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Downloader] gf_dm_configure_cache(%p), cached=%s\n", sess, sess->flags&GF_NETIO_SESSION_NOT_CACHED?"no":"yes" ));
    gf_dm_remove_cache_entry_from_session(sess);
    entry = gf_dm_find_cached_entry_by_url(sess);
    if (!entry) {
        entry = gf_cache_create_entry(sess->dm, sess->dm->cache_directory, sess->orig_url);
        gf_mx_p( sess->dm->cache_mx );
        gf_list_add(sess->dm->cache_entries, entry);
        gf_mx_v( sess->dm->cache_mx );
    }
    assert( entry );
    sess->cache_entry = entry;
    gf_cache_add_session_to_cache_entry(sess->cache_entry, sess);
    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] Cache setup to %p %s\n", sess, gf_cache_get_cache_filename(sess->cache_entry)));
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
        DownloadedCacheEntry e = gf_list_get(dm->cache_entries, i);
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

void gf_dm_delete_cached_file_entry_session(const GF_DownloadSession * sess,  const char * url) {
    if (sess && sess->dm && url) {
        GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[CACHE] Requesting deletion for %s\n", url));
        gf_dm_delete_cached_file_entry(sess->dm, url);
    }
}


static void gf_dm_disconnect(GF_DownloadSession *sess)
{
    GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Downloader] gf_dm_disconnect(%p)\n", sess ));
    if (sess->mx)
        gf_mx_p(sess->mx);
#ifdef GPAC_HAS_SSL
    if (sess->ssl) {
        SSL_shutdown(sess->ssl);
        SSL_free(sess->ssl);
        sess->ssl = NULL;
    }
#endif
    if (sess->sock) {
        gf_sk_del(sess->sock);
        sess->sock = NULL;
    }
    sess->status = GF_NETIO_DISCONNECTED;
    if (sess->num_retry) sess->num_retry--;
    if (sess->mx)
        gf_mx_v(sess->mx);
}

void gf_dm_sess_del(GF_DownloadSession *sess)
{
    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Downloader] gf_dm_sess_del(%p)\n", sess ));
    /*self-destruction, let the download manager destroy us*/
    if (sess->th && sess->in_callback) {
        sess->destroy = 1;
        return;
    }
    gf_dm_disconnect(sess);

    /*if threaded wait for thread exit*/
    if (sess->th) {
        while (!(sess->flags & GF_DOWNLOAD_SESSION_THREAD_DEAD))
            gf_sleep(1);
        gf_th_stop(sess->th);
        gf_th_del(sess->th);
        gf_mx_del(sess->mx);
    }

    if (sess->dm) gf_list_del_item(sess->dm->sessions, sess);
    /*
             TODO: something to clean an cache files ?
    	if (sess->cache_name && !sess->use_cache_extension && !(sess->flags & GF_NETIO_SESSION_KEEP_CACHE) ) {
            if (sess->dm)
                opt = gf_cfg_get_key(sess->dm->cfg, "Downloader", "CleanCache");
            else
                opt = NULL;
            if (!opt || !stricmp(opt, "yes")) gf_delete_file(sess->cache_name);
    		gf_free(sess->cache_name);
    	}
    */
    gf_dm_remove_cache_entry_from_session(sess);
    sess->cache_entry = NULL;
    if (sess->orig_url) gf_free(sess->orig_url);
    if (sess->server_name) gf_free(sess->server_name);
    sess->server_name = NULL;
    if (sess->remote_path) gf_free(sess->remote_path);
    /* Credentials are stored into the sess->dm */
    if (sess->creds) sess->creds = NULL;
    if (sess->init_data) gf_free(sess->init_data);
    sess->orig_url = sess->server_name = sess->remote_path;
    sess->creds = NULL;
    gf_free(sess);
}

void http_do_requests(GF_DownloadSession *sess);

static void gf_dm_sess_notify_state(GF_DownloadSession *sess, u32 dnload_status, GF_Err error)
{
    if (sess->user_proc) {
        GF_NETIO_Parameter par;
        sess->in_callback = 1;
        memset(&par, 0, sizeof(GF_NETIO_Parameter));
        par.msg_type = dnload_status;
        par.error = error;
        sess->user_proc(sess->usr_cbk, &par);
        sess->in_callback = 0;
    }
}

static void gf_dm_sess_user_io(GF_DownloadSession *sess, GF_NETIO_Parameter *par)
{
    if (sess->user_proc) {
        sess->in_callback = 1;
        sess->user_proc(sess->usr_cbk, par);
        sess->in_callback = 0;
    }
}

GF_EXPORT
Bool gf_dm_is_thread_dead(GF_DownloadSession *sess)
{
    if (!sess) return 1;
    return (sess->flags & GF_DOWNLOAD_SESSION_THREAD_DEAD) ? 1 : 0;
}

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

GF_EXPORT
GF_Err gf_dm_get_url_info(const char * url, GF_URL_Info * info, const char * baseURL) {
    char *tmp, *tmp_url, *current_pos;
    char * copyOfUrl;
    gf_dm_url_info_del(info);

    if (!strnicmp(url, "http://", 7)) {
        url += 7;
        info->port = 80;
        info->protocol = "http://";
    }
    else if (!strnicmp(url, "https://", 8)) {
        url += 8;
        info->port = 443;
#ifndef GPAC_HAS_SSL
        return GF_NOT_SUPPORTED;
#endif
        info->protocol = "https://";
    }
    else if (!strnicmp(url, "ftp://", 6)) {
        url += 6;
        info->port = 21;
        info->protocol = "ftp://";
        return GF_NOT_SUPPORTED;
    }
    /*relative URL*/
    else if (!strstr(url, "://")) {
        u32 i;
        info->protocol = "file:/";
        if (!baseURL)
            return GF_BAD_PARAM;
        tmp = gf_url_concatenate(baseURL, url);
        info->remotePath = gf_url_percent_encode(tmp);
        gf_free( tmp );
        tmp = NULL;
        for (i=0; i<strlen(info->remotePath); i++)
            if (info->remotePath[i]=='\\') info->remotePath[i]='/';
        info->canonicalRepresentation = gf_malloc(strlen(info->protocol) + strlen(info->remotePath) + 1);
        strcpy(info->canonicalRepresentation, info->protocol);
        strcat(info->canonicalRepresentation, info->remotePath);
        return GF_OK;
    } else {
        return GF_BAD_PARAM;
    }

    tmp = strchr(url, '/');
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

    tmp = strrchr(current_pos, ':');
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
        char port[7];
        snprintf(port, 7, ":%d", info->port);
        info->canonicalRepresentation = gf_malloc(strlen(info->protocol)+strlen(info->server_name)+1+strlen(port)+strlen(info->remotePath));
        strcpy(info->canonicalRepresentation, info->protocol);
        strcat(info->canonicalRepresentation, info->server_name);
        strcat(info->canonicalRepresentation, port);
        strcat(info->canonicalRepresentation, info->remotePath);
    }
    gf_free(copyOfUrl);
    return GF_OK;
}

GF_Err gf_dm_setup_from_url(GF_DownloadSession *sess, const char *url)
{
    GF_Err e;
    GF_URL_Info info;
    gf_dm_url_info_init(&info);
    e = gf_dm_get_url_info(url, &info, sess->remote_path);
    sess->port = info.port;
    if (!strcmp("http://", info.protocol) || !strcmp("https://", info.protocol)) {
        sess->do_requests = http_do_requests;
        if (!strcmp("https://", info.protocol))
            sess->flags |= GF_DOWNLOAD_SESSION_USE_SSL;
    } else {
        sess->do_requests = NULL;
    }
    if (sess->orig_url)
        gf_free(sess->orig_url);
    sess->orig_url = NULL;
    if (info.server_name && sess->server_name)
    {
        gf_free(sess->server_name);
        sess->server_name = NULL;
    }
    if (e == GF_OK) {
        const char *opt;
        sess->orig_url = gf_strdup(info.canonicalRepresentation);
        if (sess->remote_path)
            gf_free(sess->remote_path);
        sess->remote_path = NULL;
        sess->remote_path = gf_strdup(info.remotePath);
        sess->server_name = info.server_name ? gf_strdup(info.server_name) : NULL;
        if (info.userName) {
            if (! sess->dm) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Did not found any download manager, credentials not supported\n"));
            } else
                sess->creds = gf_user_credentials_register(sess->dm, sess->server_name, info.userName, info.password, info.userName && info.password);
        }
        /*setup BW limiter*/
        sess->limit_data_rate = 0;
        if (sess->dm && sess->dm->cfg) {
            opt = gf_cfg_get_key(sess->dm->cfg, "Downloader", "MaxRate");
            if (opt) {
                /*use it in in BYTES per second*/
                sess->limit_data_rate = 1024 * atoi(opt) / 8;
            }
        }

    }
    gf_dm_url_info_del(&info);
    return e;
}


#define GF_WAIT_REPLY_SLEEP	20
static u32 gf_dm_session_thread(void *par)
{
    GF_DownloadSession *sess = (GF_DownloadSession *)par;

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
            if (sess->status == GF_NETIO_WAIT_FOR_REPLY) gf_sleep(GF_WAIT_REPLY_SLEEP);
            sess->do_requests(sess);
        }
        gf_mx_v(sess->mx);
        gf_sleep(2);
    }
    /*destroy all sessions*/
    gf_dm_disconnect(sess);
    sess->status = GF_NETIO_STATE_ERROR;
    sess->last_error = 0;
    sess->flags |= GF_DOWNLOAD_SESSION_THREAD_DEAD;
    return 1;
}

#define SESSION_RETRY_COUNT	20

GF_DownloadSession *gf_dm_sess_new_simple(GF_DownloadManager * dm, const char *url, u32 dl_flags,
        gf_dm_user_io user_io,
        void *usr_cbk,
        GF_Err *e)
{
    GF_DownloadSession *sess;
    sess = (GF_DownloadSession *)gf_malloc(sizeof(GF_DownloadSession));
    memset((void *)sess, 0, sizeof(GF_DownloadSession));
    sess->flags = dl_flags;
    sess->server_only_understand_get = 0;
    sess->user_proc = user_io;
    sess->usr_cbk = usr_cbk;
    sess->creds = NULL;
    sess->dm = dm;
    assert( dm );

    *e = gf_dm_setup_from_url(sess, url);
    if (*e) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("%s:%s gf_dm_sess_new_simple:%d, error=%e at setup\n", __FILE__, __LINE__, e));
        gf_dm_sess_del(sess);
        return NULL;
    }
    if (!(sess->flags & GF_NETIO_SESSION_NOT_THREADED) ) {
        sess->th = gf_th_new(url);
        sess->mx = gf_mx_new(url);
        gf_th_run(sess->th, gf_dm_session_thread, sess);
    }
    if (!(sess->flags & GF_NETIO_SESSION_FORCE_NO_CACHE))
        sess->use_cache_file = 0;
    sess->num_retry = SESSION_RETRY_COUNT;
    return sess;
}

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
        gf_list_add(dm->sessions, sess);
    }
    return sess;
}

static GF_Err gf_dm_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read)
{
    GF_Err e;
    if (!sess)
        return GF_BAD_PARAM;
#ifdef GPAC_HAS_SSL
    if (sess->ssl) {
        u32 size = SSL_read(sess->ssl, data, data_size);
        e = GF_OK;
        data[size] = 0;
        if (!size) e = GF_IP_NETWORK_EMPTY;
        *out_read = size;
    } else
#endif
        if (!sess->sock)
            return GF_NETIO_DISCONNECTED;
    e = gf_sk_receive(sess->sock, data, data_size, 0, out_read);

    return e;
}


#ifdef GPAC_HAS_SSL
/*pattern comp taken from wget*/
#define ASTERISK_EXCLUDES_DOT	/* mandated by rfc2818 */

#define TOLOWER(x) ('A' <= (x) && (x) <= 'Z' ? (x) - 32 : (x))

static Bool pattern_match(const char *pattern, const char *string)
{
    const char *p = pattern, *n = string;
    char c;
    for (; (c = TOLOWER (*p++)) != '\0'; n++) {
        if (c == '*') {
            for (c = TOLOWER (*p); c == '*'; c = TOLOWER (*++p))
                ;
            for (; *n != '\0'; n++)
                if (TOLOWER (*n) == c && pattern_match (p, n))
                    return 1;
#ifdef ASTERISK_EXCLUDES_DOT
                else if (*n == '.')
                    return 0;
#endif
            return c == '\0';
        } else {
            if (c != TOLOWER (*n))
                return 0;
        }
    }
    return *n == '\0';
}
#undef TOLOWER

#endif

static void gf_dm_connect(GF_DownloadSession *sess)
{
    GF_Err e;
    u16 proxy_port = 0;
    const char *proxy, *ip;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("gf_dm_connect"":%d\n", __LINE__));
    if (!sess->sock) {
        sess->num_retry = 40;
        sess->sock = gf_sk_new(GF_SOCK_TYPE_TCP);
    }

    /*connect*/
    sess->status = GF_NETIO_SETUP;
    gf_dm_sess_notify_state(sess, sess->status, GF_OK);

    /*PROXY setup*/
    if (sess->proxy_enabled!=2 && sess->dm && sess->dm->cfg) {
        proxy = gf_cfg_get_key(sess->dm->cfg, "HTTPProxy", "Enabled");
        if (proxy && !strcmp(proxy, "yes")) {
            u32 i;
            Bool use_proxy=1;
            for (i=0; i<gf_list_count(sess->dm->skip_proxy_servers); i++) {
                char *skip = gf_list_get(sess->dm->skip_proxy_servers, i);
                if (!strcmp(skip, sess->server_name)) {
                    use_proxy=0;
                    break;
                }
            }
            if (use_proxy) {
                proxy = gf_cfg_get_key(sess->dm->cfg, "HTTPProxy", "Port");
                proxy_port = proxy ? atoi(proxy) : 80;
                proxy = gf_cfg_get_key(sess->dm->cfg, "HTTPProxy", "Name");
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


    if (sess->dm && sess->dm->cfg) {
        ip = gf_cfg_get_key(sess->dm->cfg, "Network", "MobileIPEnabled");
        if (ip && !strcmp(ip, "yes")) {
            ip = gf_cfg_get_key(sess->dm->cfg, "Network", "MobileIP");
        } else {
            ip = NULL;
        }
    } else {
        ip = NULL;
    }

    if (!proxy) {
        proxy = sess->server_name;
        proxy_port = sess->port;
    }
    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Connecting to %s:%d\n", proxy, proxy_port));

    e = gf_sk_connect(sess->sock, (char *) proxy, proxy_port, (char *)ip);

    /*retry*/
    if ((e == GF_IP_SOCK_WOULD_BLOCK) && sess->num_retry) {
        sess->status = GF_NETIO_SETUP;
        sess->num_retry--;
        return;
    }

    /*failed*/
    if (e) {
        sess->status = GF_NETIO_STATE_ERROR;
        sess->last_error = e;
        gf_dm_sess_notify_state(sess, sess->status, e);
        return;
    }

    sess->status = GF_NETIO_CONNECTED;
    gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
    gf_sk_set_buffer_size(sess->sock, 0, GF_DOWNLOAD_BUFFER_SIZE);
    gf_dm_configure_cache(sess);

#ifdef GPAC_HAS_SSL
    if (!sess->ssl && (sess->flags & GF_DOWNLOAD_SESSION_USE_SSL)) {
        if (!sess->dm->ssl_ctx)
            ssl_init(sess->dm, 0);
        /*socket is connected, configure SSL layer*/
        if (sess->dm->ssl_ctx) {
            int ret;
            long vresult;
            char common_name[256];
            X509 *cert;
            Bool success = 1;

            sess->ssl = SSL_new(sess->dm->ssl_ctx);
            SSL_set_fd(sess->ssl, gf_sk_get_handle(sess->sock));
            SSL_set_connect_state(sess->ssl);
            ret = SSL_connect(sess->ssl);
            assert(ret>0);

            cert = SSL_get_peer_certificate(sess->ssl);
            /*if we have a cert, check it*/
            if (cert) {
                vresult = SSL_get_verify_result(sess->ssl);
                if (vresult != X509_V_OK) success = 0;
                else {
                    common_name[0] = 0;
                    X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, common_name, sizeof (common_name));
                    if (!pattern_match(common_name, sess->server_name)) success = 0;
                }
                X509_free(cert);

                if (!success) {
                    gf_dm_disconnect(sess);
                    sess->status = GF_NETIO_STATE_ERROR;
                    sess->last_error = GF_AUTHENTICATION_FAILURE;
                    gf_dm_sess_notify_state(sess, sess->status, sess->last_error);
                }
            }
        }
    }
#endif
}

DownloadedCacheEntry gf_dm_refresh_cache_entry(GF_DownloadSession *sess) {
    Bool go;
    u32 timer = 0;
    u32 flags = sess->flags;
    sess->flags |= GF_NETIO_SESSION_NOT_CACHED;
    go = 1;
    while (go) {
        switch (sess->status) {
            /*setup download*/
        case GF_NETIO_SETUP:
            gf_dm_connect(sess);
            if (sess->status == GF_NETIO_SETUP)
                gf_sleep(16);
            break;
        case GF_NETIO_WAIT_FOR_REPLY:
            if (timer == 0)
                timer = gf_sys_clock();
            gf_sleep(16);
            {
                u32 timer2 = gf_sys_clock();
                if (timer2 - timer > 5000) {
                    GF_Err e;
                    /* Since HEAD is not understood by this server, we use a GET instead */
                    sess->http_read_type = GET;
                    sess->flags |= GF_NETIO_SESSION_NOT_CACHED;
                    gf_dm_disconnect(sess);
                    sess->status = GF_NETIO_SETUP;
                    sess->server_only_understand_get = 1;
                    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("gf_dm_refresh_cache_entry() : Timeout with HEAD, try with GET\n"));
                    e = gf_dm_setup_from_url(sess, sess->orig_url);
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
            go = 0;
            break;
        }
    }
    sess->flags = flags;
    if (sess->status==GF_NETIO_STATE_ERROR) return NULL;
    return sess->cache_entry;
}


const char *gf_dm_sess_mime_type(GF_DownloadSession *sess)
{
    DownloadedCacheEntry entry;
    if (sess->cache_entry) {
        const char * oldMimeIfAny = gf_cache_get_mime_type(sess->cache_entry);
        if (oldMimeIfAny)
            return oldMimeIfAny;
    }
    entry = gf_dm_refresh_cache_entry (sess);
    assert( entry == sess->cache_entry && entry);
    return gf_cache_get_mime_type( sess->cache_entry );
}



GF_Err gf_dm_sess_process(GF_DownloadSession *sess)
{
    Bool go;
    go = 1;
    while (go) {
        switch (sess->status) {
            /*setup download*/
        case GF_NETIO_SETUP:
            gf_dm_connect(sess);
            if (sess->status == GF_NETIO_SETUP)
                gf_sleep(16);
            break;
        case GF_NETIO_WAIT_FOR_REPLY:
            gf_sleep(16);
        case GF_NETIO_CONNECTED:
        case GF_NETIO_DATA_EXCHANGE:
            sess->do_requests(sess);
            break;
        case GF_NETIO_DISCONNECTED:
        case GF_NETIO_STATE_ERROR:
            go = 0;
            break;
        }
    }
    return sess->last_error;
}

static Bool gf_dm_needs_to_delete_cache(GF_DownloadManager * dm) {
    const char * opt;
    if (!dm || !dm->cfg)
        return 0;
    opt = gf_cfg_get_key(dm->cfg, "Downloader", "CleanCache");
    return opt && (!strncmp("yes", opt, 3) || !strncmp("true", opt, 4) || !strncmp("1", opt, 1));
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

GF_DownloadManager *gf_dm_new(GF_Config *cfg)
{
    const char *opt;
    char * default_cache_dir;
    GF_DownloadManager *dm;
    GF_SAFEALLOC(dm, GF_DownloadManager);
    dm->sessions = gf_list_new();
    dm->cache_entries = gf_list_new();
    dm->credentials = gf_list_new();
    dm->skip_proxy_servers = gf_list_new();
    dm->partial_downloads = gf_list_new();
    dm->cfg = cfg;
    dm->cache_mx = gf_mx_new("download_manager_cache_mx");
    default_cache_dir = NULL;
    gf_mx_p( dm->cache_mx );
    if (cfg)
        opt = gf_cfg_get_key(cfg, "General", "CacheDirectory");
    else
        opt = NULL;
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
    gf_mx_v( dm->cache_mx );
    if (default_cache_dir)
        gf_free(default_cache_dir);
#ifdef GPAC_HAS_SSL
    dm->ssl_ctx = NULL;
#endif
    /* TODO: Not ready for now, we should find a locking strategy between several GPAC instances...
     * gf_cache_cleanup_cache(dm);
     */
    return dm;
}

void gf_dm_set_auth_callback(GF_DownloadManager *dm,
                             Bool (*GetUserPassword)(void *usr_cbk, const char *site_url, char *usr_name, char *password),
                             void *usr_cbk)
{
    if (dm) {
        dm->GetUserPassword = GetUserPassword;
        dm->usr_cbk = usr_cbk;
    }
}

void gf_dm_del(GF_DownloadManager *dm)
{
    if (!dm)
        return;
    assert( dm->sessions);
    assert( dm->cache_mx );
    gf_mx_p( dm->cache_mx );

    while (gf_list_count(dm->partial_downloads)) {
        GF_PartialDownload * entry = gf_list_get( dm->partial_downloads, 0);
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
        char *serv = gf_list_get(dm->skip_proxy_servers, 0);
        gf_list_rem(dm->skip_proxy_servers, 0);
        gf_free(serv);
    }
    gf_list_del(dm->skip_proxy_servers);
    dm->skip_proxy_servers = NULL;
    assert( dm->credentials);
    while (gf_list_count(dm->credentials)) {
        gf_user_credentials_struct * cred = gf_list_get( dm->credentials, 0);
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
            const DownloadedCacheEntry entry = gf_list_get( dm->cache_entries, 0);
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
 * \param icy_metaint The number of bytes of data before reaching possible meta data
 * \param data last data received
 * \param nbBytes The number of bytes contained into data
 */
static void gf_icy_skip_data(GF_DownloadSession * sess, u32 icy_metaint, const char * data, u32 nbBytes) {
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

                    par.error = 0;
                    par.msg_type = GF_NETIO_PARSE_HEADER;
                    par.name = "icy-meta";
                    par.value = szData;
                    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK,
                           ("[ICY] Found metainfo in stream=%s, (every %d bytes)\n", szData, icy_metaint));
                    gf_dm_sess_user_io(sess, &par);
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
            par.error = GF_OK;
            par.data = data;
            par.size = left;
            gf_dm_sess_user_io(sess, &par);

            data += left;
        }
    }
}


static GFINLINE void gf_dm_data_received(GF_DownloadSession *sess, const char *data, u32 nbBytes)
{
    GF_NETIO_Parameter par;
    u32 runtime, rcv;
    rcv = nbBytes;
    if (sess->icy_metaint > 0)
        gf_icy_skip_data(sess, sess->icy_metaint, data, nbBytes);
    else {
        if (sess->use_cache_file)
            gf_cache_write_to_cache( sess->cache_entry, sess, data, nbBytes);
        sess->bytes_done += nbBytes;
        {
            par.msg_type = GF_NETIO_DATA_EXCHANGE;
            par.error = GF_OK;
            par.data = data;
            par.size = nbBytes;
            gf_dm_sess_user_io(sess, &par);
        }
    }

    if (sess->total_size && (sess->bytes_done == sess->total_size)) {
        gf_dm_disconnect(sess);
        par.msg_type = GF_NETIO_DATA_TRANSFERED;
        par.error = GF_OK;
        gf_dm_sess_user_io(sess, &par);
        if (sess->use_cache_file) {
            gf_cache_close_write_cache(sess->cache_entry, sess, 1);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK,
                   ("[CACHE] url %s saved as %s\n", gf_cache_get_url(sess->cache_entry), gf_cache_get_cache_filename(sess->cache_entry)));
        }
        return;
    }
    /*update state if not done*/
    if (rcv) {
        sess->bytes_in_wnd += rcv;
        runtime = gf_sys_clock() - sess->window_start;
        if (!runtime) {
            sess->bytes_per_sec = 0;
        } else {
            sess->bytes_per_sec = (1000 * (sess->bytes_in_wnd)) / runtime;
            if (runtime>1000) {
                sess->window_start += runtime/2;
                sess->bytes_in_wnd = sess->bytes_per_sec / 2;
            }
        }
    }
}


GF_EXPORT
GF_Err gf_dm_sess_fetch_data(GF_DownloadSession *sess, char *buffer, u32 buffer_size, u32 *read_size)
{
    GF_Err e;
    if (/*sess->cache || */ !buffer || !buffer_size) return GF_BAD_PARAM;
    if (sess->th) return GF_BAD_PARAM;
    if (sess->status == GF_NETIO_DISCONNECTED) return GF_EOS;
    if (sess->status > GF_NETIO_DATA_TRANSFERED) return GF_BAD_PARAM;

    *read_size = 0;
    if (sess->status == GF_NETIO_DATA_TRANSFERED) return GF_EOS;

    if (sess->status == GF_NETIO_SETUP) {
        gf_dm_connect(sess);
        return GF_OK;
    } else if (sess->status < GF_NETIO_DATA_EXCHANGE) {
        sess->do_requests(sess);
        if (sess->status > GF_NETIO_DATA_TRANSFERED) return GF_SERVICE_ERROR;
        return GF_OK;
    }
    /*we're running but we had data previously*/
    if (sess->init_data) {
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
        return GF_OK;
    }

    e = gf_dm_read_data(sess, buffer, buffer_size, read_size);
    if (e) return e;
    gf_dm_data_received(sess, buffer, *read_size);
    return GF_OK;
}

GF_EXPORT
GF_Err gf_dm_sess_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u32 *total_size, u32 *bytes_done, u32 *bytes_per_sec, u32 *net_status)
{
    if (!sess) return GF_BAD_PARAM;
    if (server) *server = sess->server_name;
    if (path) *path = sess->remote_path;
    if (total_size) *total_size = sess->total_size;
    if (bytes_done) *bytes_done = sess->bytes_done;
    if (bytes_per_sec) *bytes_per_sec = sess->bytes_per_sec;
    if (net_status) *net_status = sess->status;
    if (sess->status == GF_NETIO_DISCONNECTED) return GF_EOS;
    else if (sess->status == GF_NETIO_STATE_ERROR) return GF_SERVICE_ERROR;
    return GF_OK;
}

GF_EXPORT
const char *gf_dm_sess_get_cache_name(GF_DownloadSession * sess)
{
    if (!sess) return NULL;
    return gf_cache_get_cache_filename(sess->cache_entry);
}

GF_EXPORT
Bool gf_dm_sess_can_be_cached_on_disk(const GF_DownloadSession *sess)
{
    if (!sess) return 0;
    return gf_cache_get_content_length(sess->cache_entry) != 0;
}

void gf_dm_sess_abort(GF_DownloadSession * sess)
{
    if (sess->mx) {
        gf_mx_p(sess->mx);
        gf_dm_disconnect(sess);
        sess->status = GF_NETIO_STATE_ERROR;
        gf_mx_v(sess->mx);
    } else {
        gf_dm_disconnect(sess);
    }
}
void *gf_dm_sess_get_private(GF_DownloadSession * sess)
{
    return sess ? sess->ext : NULL;
}

void gf_dm_sess_set_private(GF_DownloadSession * sess, void *private_data)
{
    if (sess) sess->ext = private_data;
}

/* HTTP(S) stuff*/
static GFINLINE u32 http_skip_space(char *val)
{
    u32 ret = 0;
    while (val[ret] == ' ') ret+=1;
    return ret;
}

static GFINLINE char *http_is_header(char *line, char *header_name)
{
    char *res;
    if (strnicmp(line, header_name, strlen(header_name))) return NULL;
    res = line + strlen(header_name);
    while ((res[0] == ':') || (res[0] == ' ')) res+=1;
    return res;
}

/*!
 * Sends the HTTP headers
 * \param sess The GF_DownloadSession
 * \param the buffer containing the request
 * \return GF_OK if everything went fine, the error otherwise
 */
static GF_Err http_send_headers(GF_DownloadSession *sess, char * sHTTP) {
    GF_Err e;
    GF_NETIO_Parameter par;
    char range_buf[1024];
    char pass_buf[1024];
    const char *user_agent;
    const char *url;
    const char *user_profile;
    const char *param_string;
    Bool has_accept, has_connection, has_range, has_agent, has_language, send_profile;
    assert (sess->status == GF_NETIO_CONNECTED);
    /*setup authentification*/
    strcpy(pass_buf, "");
    sess->creds = gf_find_user_credentials_for_site( sess->dm, sess->server_name );
    if (sess->creds && sess->creds->valid) {
        sprintf(pass_buf, "Authorization: Basic %s", sess->creds->digest);
    }
    if (sess->dm && sess->dm->cfg)
        user_agent = gf_cfg_get_key(sess->dm->cfg, "Downloader", "UserAgent");
    else
        user_agent = NULL;
    if (!user_agent) user_agent = GF_DOWNLOAD_AGENT_NAME;


    par.error = 0;
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
    if (sess->dm && sess->dm->cfg)
        param_string = gf_cfg_get_key(sess->dm->cfg, "Downloader", "ParamString");
    else
        param_string = NULL;
    if (param_string) {
        if (strchr(sess->remote_path, '?')) {
            sprintf(sHTTP, "%s %s&%s HTTP/1.0\r\nHost: %s\r\n" ,
                    par.name ? par.name : "GET", url, param_string, sess->server_name);
        } else {
            sprintf(sHTTP, "%s %s?%s HTTP/1.0\r\nHost: %s\r\n" ,
                    par.name ? par.name : "GET", url, param_string, sess->server_name);
        }
    } else {
        sprintf(sHTTP, "%s %s HTTP/1.0\r\nHost: %s\r\n" ,
                par.name ? par.name : "GET", url, sess->server_name);
    }

    /*signal we support title streaming*/
    if (!strcmp(sess->remote_path, "/")) strcat(sHTTP, "icy-metadata:1\r\n");

    /*get all headers*/
    has_agent = has_accept = has_connection = has_range = has_language = 0;
    while (1) {
        par.msg_type = GF_NETIO_GET_HEADER;
        par.value = NULL;
        gf_dm_sess_user_io(sess, &par);
        if (!par.value) break;
        strcat(sHTTP, par.name);
        strcat(sHTTP, ": ");
        strcat(sHTTP, par.value);
        strcat(sHTTP, "\r\n");
        if (!strcmp(par.name, "Accept")) has_accept = 1;
        else if (!strcmp(par.name, "Connection")) has_connection = 1;
        else if (!strcmp(par.name, "Range")) has_range = 1;
        else if (!strcmp(par.name, "User-Agent")) has_agent = 1;
        else if (!strcmp(par.name, "Accept-Language")) has_language = 1;
        if (!par.msg_type) break;
    }
    if (!has_agent) {
        strcat(sHTTP, "User-Agent: ");
        strcat(sHTTP, user_agent);
        strcat(sHTTP, "\r\n");
    }
    if (!has_accept) strcat(sHTTP, "Accept: */*\r\n");
    if (sess->proxy_enabled==1) strcat(sHTTP, "Proxy-Connection: Keep-alive\r\n");
    else if (!has_connection) strcat(sHTTP, "Connection: Keep-Alive\r\n");
    if (!has_range && sess->needs_range) {
        if (!sess->range_end) sprintf(range_buf, "Range: bytes=%d-\r\n", sess->range_start);
        else sprintf(range_buf, "Range: bytes=%d-%d\r\n", sess->range_start, sess->range_end);
        /* FIXME : cache should not be used here */
        strcat(sHTTP, range_buf);
    }
    if (!has_language) {
        const char *opt;

        if (sess->dm && sess->dm->cfg)
            opt = gf_cfg_get_key(sess->dm->cfg, "Systems", "Language2CC");
        else
            opt = NULL;
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
    send_profile = 0;
    if (sess->dm && sess->dm->cfg)
        user_profile = gf_cfg_get_key(sess->dm->cfg, "Downloader", "UserProfileID");
    else
        user_profile = NULL;
    if (user_profile) {
        strcat(sHTTP, "X-UserProfileID: ");
        strcat(sHTTP, user_profile);
        strcat(sHTTP, "\r\n");
    } else {
        if (sess->dm && sess->dm->cfg)
            user_profile = gf_cfg_get_key(sess->dm->cfg, "Downloader", "UserProfile");
        else
            user_profile = NULL;
        if (user_profile) {
            FILE *profile = gf_f64_open(user_profile, "rt");
            if (profile) {
                gf_f64_seek(profile, 0, SEEK_END);
                par.size = (u32) gf_f64_tell(profile);
                fclose(profile);
                sprintf(range_buf, "Content-Length: %d\r\n", par.size);
                strcat(sHTTP, range_buf);
                strcat(sHTTP, "Content-Type: text/xml\r\n");
                send_profile = 1;
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
    /*{
      const char * mime = gf_cache_get_mime_type(sess->cache_entry);
      if (sess->server_only_understand_get || mime && !strcmp("audio/mpeg", mime)){*/
    /* This will force the server to respond with Icy-Metaint */
    strcat(sHTTP, "Icy-Metadata: 1\r\n");
    /*  }
    }*/

    if (GF_OK < appendHttpCacheHeaders( sess->cache_entry, sHTTP)) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("Cache Entry : %p, FAILED to append cache directives.", sess->cache_entry));
    }

    strcat(sHTTP, "\r\n");

    if (send_profile || par.data) {
        u32 len = strlen(sHTTP);
        char *tmp_buf = gf_malloc(sizeof(char)*(len+par.size));
        strcpy(tmp_buf, sHTTP);
        if (par.data) {
            memcpy(tmp_buf+len, par.data, par.size);
        } else {
            FILE *profile;
            assert( sess->dm );
            assert( sess->dm->cfg );
            user_profile = gf_cfg_get_key(sess->dm->cfg, "Downloader", "UserProfile");
            assert (user_profile);
            profile = gf_f64_open(user_profile, "rt");
            if (profile) {
                u32 readen = fread(tmp_buf+len, sizeof(char), par.size, profile);
                if (readen<par.size) {
                    GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK,
                           ("Error while loading Downloader/UserProfile, size=%d, should be %d.", readen, par.size));
                    for (; readen < par.size; readen++) {
                        tmp_buf[len + readen] = 0;
                    }
                }
                fclose(profile);
            } else {
                GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("Error while loading Profile file %s.", user_profile));
            }
        }

#ifdef GPAC_HAS_SSL
        if (sess->ssl) {
            e = GF_IP_NETWORK_FAILURE;
            if (!SSL_write(sess->ssl, tmp_buf, len+par.size)) e = GF_OK;
        } else
#endif
            e = gf_sk_send(sess->sock, tmp_buf, len+par.size);

        GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Sending request %s\n\n", tmp_buf));
        gf_free(tmp_buf);
    } else {

#ifdef GPAC_HAS_SSL
        if (sess->ssl) {
            e = GF_IP_NETWORK_FAILURE;
            if (!SSL_write(sess->ssl, sHTTP, strlen(sHTTP))) e = GF_OK;
        } else
#endif
            e = gf_sk_send(sess->sock, sHTTP, strlen(sHTTP));

        GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Sending request %s\n ; Error Code=%d\n", sHTTP, e));
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


/*!
 * Parse the remaining part of body
 * \param sess The session
 * \param sHTTP the data buffer
 * \return The error code if any
 */
static GF_Err http_parse_remaining_body(GF_DownloadSession * sess, char * sHTTP) {
    u32 size;
    GF_Err e;
    while (1) {
        if (sess->status>=GF_NETIO_DISCONNECTED)
            return GF_REMOTE_SERVICE_ERROR;

#if 1
        if (sess->limit_data_rate && sess->bytes_per_sec) {
            if (sess->bytes_per_sec>sess->limit_data_rate) {
                /*update state*/
                u32 runtime = gf_sys_clock() - sess->window_start;
                sess->bytes_per_sec = (1000 * (sess->bytes_in_wnd)) / runtime;
                if (sess->bytes_per_sec > sess->limit_data_rate) return GF_OK;
            }
        }
#endif
        e = gf_dm_read_data(sess, sHTTP, GF_DOWNLOAD_BUFFER_SIZE, &size);
        if (!size || e == GF_IP_NETWORK_EMPTY) {
            if (!sess->total_size && (gf_sys_clock() - sess->window_start > 2000)) {
                sess->total_size = sess->bytes_done;
                gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
                sess->status = GF_NETIO_DISCONNECTED;
            }
            return GF_OK;
        }

        if (e) {
            gf_dm_disconnect(sess);
            sess->last_error = e;
            gf_dm_sess_notify_state(sess, sess->status, e);
            return e;
        }
        gf_dm_data_received(sess, sHTTP, size);

        /*socket empty*/
        if (size < GF_DOWNLOAD_BUFFER_SIZE) return GF_OK;
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
    u32 res;
    s32 LinePos, Pos;
    u32 rsp_code, ContentLength, first_byte, last_byte, total_size, range, no_range;
    char buf[1024];
    char comp[400];
    GF_Err e;
    char * new_location;
    assert( sess->status == GF_NETIO_WAIT_FOR_REPLY );
    bytesRead = res = 0;
    new_location = NULL;
    sess->use_cache_file = 1;
    while (1) {
        e = gf_dm_read_data(sess, sHTTP + bytesRead, GF_DOWNLOAD_BUFFER_SIZE - bytesRead, &res);
        switch (e) {
        case GF_IP_NETWORK_EMPTY:
            if (!bytesRead) return GF_OK;
            continue;
            /*socket has been closed while configuring, retry (not sure if the server got the GET)*/
        case GF_IP_CONNECTION_CLOSED:
            gf_dm_disconnect(sess);
            if (!strncmp("HEAD", sHTTP, 4)) {
                /* Some servers such as shoutcast directly close connection if HEAD or an unknown method is issued */
                sess->server_only_understand_get = 1;
            }
            if (sess->num_retry)
                sess->status = GF_NETIO_SETUP;
            else {
                sess->last_error = e;
                sess->status = GF_NETIO_STATE_ERROR;
            }
            return e;
        case GF_OK:
            if (!res) return GF_OK;
            break;
        default:
            goto exit;
        }
        bytesRead += res;

        /*locate body start*/
        BodyStart = gf_token_find(sHTTP, 0, bytesRead, "\r\n\r\n");
        if (BodyStart <= 0) {
            BodyStart=0;
            continue;
        }
        BodyStart += 4;
        break;
    }
    if (bytesRead < 0) {
        e = GF_REMOTE_SERVICE_ERROR;
        goto exit;
    }
    if (!BodyStart)
        BodyStart = bytesRead;

    sHTTP[BodyStart-1] = 0;
    GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] %s\n\n", sHTTP));

    LinePos = gf_token_get_line(sHTTP, 0, bytesRead, buf, 1024);
    Pos = gf_token_get(buf, 0, " \t\r\n", comp, 400);

    if (!strncmp("ICY", comp, 3)) {
        sess->use_cache_file = 0;
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
    Pos = gf_token_get(buf, Pos, " \r\n", comp, 400);

    no_range = range = ContentLength = first_byte = last_byte = total_size = 0;
    /* parse header */
    while (1) {
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

        par.error = 0;
        par.msg_type = GF_NETIO_PARSE_HEADER;
        par.name = hdr;
        par.value = hdr_val;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] Processing header %s: %s\n", hdr, hdr_val));
        gf_dm_sess_user_io(sess, &par);

        if (!stricmp(hdr, "Content-Length") ) {
            ContentLength = (u32) atoi(hdr_val);
            gf_cache_set_content_length(sess->cache_entry, ContentLength);
        }
        else if (!stricmp(hdr, "Content-Type")) {
            char * mime_type = gf_strdup(hdr_val);
            while (1) {
                u32 len = strlen(mime_type);
                char c = len ? mime_type[len-1] : 0;
                if ((c=='\r') || (c=='\n')) {
                    mime_type[len-1] = 0;
                } else {
                    break;
                }
            }
            hdr = strchr(mime_type, ';');
            if (hdr) hdr[0] = 0;
            gf_cache_set_mime_type(sess->cache_entry, mime_type);
            gf_free(mime_type);
        }
        else if (!stricmp(hdr, "Content-Range")) {
            range = 1;
            if (!strncmp(hdr_val, "bytes", 5)) {
                hdr_val += 5;
                if (hdr_val[0] == ':') hdr_val += 1;
                hdr_val += http_skip_space(hdr_val);
                if (hdr_val[0] == '*') {
                    sscanf(hdr_val, "*/%ud", &total_size);
                } else {
                    sscanf(hdr_val, "%ud-%ud/%ud", &first_byte, &last_byte, &total_size);
                }
            }
        }
        else if (!stricmp(hdr, "Accept-Ranges")) {
            if (strstr(hdr_val, "none")) no_range = 1;
        }
        else if (!stricmp(hdr, "Location"))
            new_location = gf_strdup(hdr_val);
        else if (!strnicmp(hdr, "ice", 3) || !strnicmp(hdr, "icy", 3) ) {
            /* For HTTP icy servers, we disable cache */
            if (sess->icy_metaint == 0)
                sess->icy_metaint = -1;
            sess->use_cache_file = 0;
            if (!stricmp(hdr, "icy-metaint")) {
                sess->icy_metaint = atoi(hdr_val);
            }
        }
        else if (!stricmp(hdr, "Cache-Control")) {
        }
        else if (!stricmp(hdr, "ETag")) {
            gf_cache_set_etag_on_server(sess->cache_entry, hdr_val);
        }
        else if (!stricmp(hdr, "Last-Modified")) {
            gf_cache_set_last_modified_on_server(sess->cache_entry, hdr_val);
        }
        else if (!stricmp(hdr, "X-UserProfileID") ) {
            if (sess->dm && sess->dm->cfg)
                gf_cfg_set_key(sess->dm->cfg, "Downloader", "UserProfileID", hdr_val);
        }
        /*			else if (!stricmp(hdr, "Connection") )
        				if (strstr(hdr_val, "close")) sess->http_read_type = 1; */


        if (sep) sep[0]=':';
        if (hdr_sep) hdr_sep[0] = '\r';
    }
    if (no_range) first_byte = 0;

    par.msg_type = GF_NETIO_PARSE_REPLY;
    par.error = GF_OK;
    par.reply = rsp_code;
    par.value = comp;
    /*
     * If response is correct, it means our credentials are correct
     */
    if (sess->creds && rsp_code != 304)
        sess->creds->valid = 1;
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
        gf_dm_disconnect(sess);
        sess->status = GF_NETIO_SETUP;
        e = gf_dm_setup_from_url(sess, new_location);
        if (e) {
            sess->status = GF_NETIO_STATE_ERROR;
            sess->last_error = e;
            gf_dm_sess_notify_state(sess, sess->status, e);
        }
        return e;
    case 304:
    {
        sess->status = GF_NETIO_PARSE_REPLY;
        gf_dm_sess_notify_state(sess, GF_NETIO_PARSE_REPLY, GF_OK);
        gf_dm_disconnect(sess);
        if (sess->user_proc) {
            /* For modules that do not use cache and have problems with GF_NETIO_DATA_TRANSFERED ... */
            const char * filename;
            FILE * f;
            filename = gf_cache_get_cache_filename(sess->cache_entry);
            GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Sending data to modules from %s...\n", filename));
            f = fopen(filename, "rb");
            assert(filename);
            if (!f) {
                GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] FAILED to open cache file %s for reading contents !\n", filename));
                /* Ooops, no cache, redowload everything ! */
                gf_dm_disconnect(sess);
                sess->status = GF_NETIO_SETUP;
                e = gf_dm_setup_from_url(sess, sess->orig_url);
                if (e) {
                    sess->status = GF_NETIO_STATE_ERROR;
                    sess->last_error = e;
                    gf_dm_sess_notify_state(sess, sess->status, e);
                }
                return e;
            }
            sess->status = GF_NETIO_DATA_EXCHANGE;
            {
                char file_cache_buff[16384];
                int read = 0;
                do {
                    read = fread(file_cache_buff, sizeof(char), 16384, f);
                    if (read > 0) {
                        sess->bytes_done += read;
                        par.size = read;
                        par.msg_type = GF_NETIO_DATA_EXCHANGE;
                        par.error = GF_OK;
                        par.reply = 200;
                        par.data = file_cache_buff;
                        gf_dm_sess_user_io(sess, &par);
                    }
                } while ( read > 0);
            }
            fclose(f);
            GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] all data has been sent to modules from %s.\n", filename));
        }
        /* Cache file is the most recent */
        sess->status = GF_NETIO_DATA_TRANSFERED;
        gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
        sess->status = GF_NETIO_DISCONNECTED;
        par.msg_type = GF_NETIO_DATA_TRANSFERED;
        par.error = GF_OK;
        gf_dm_sess_user_io(sess, &par);
        return GF_OK;
    }
    case 401:
    {
        /* Do we have a credentials struct ? */
        sess->creds = gf_user_credentials_register(sess->dm, sess->server_name, NULL, NULL, 0);
        if (!sess->creds) {
            /* User credentials have not been filled properly, we have to abort */
            gf_dm_disconnect(sess);
            sess->status = GF_NETIO_STATE_ERROR;
            par.error = GF_AUTHENTICATION_FAILURE;
            par.msg_type = GF_NETIO_DISCONNECTED;
            gf_dm_sess_user_io(sess, &par);
            e = GF_AUTHENTICATION_FAILURE;
            sess->last_error = e;
            goto exit;
        }
        gf_dm_disconnect(sess);
        sess->status = GF_NETIO_SETUP;
        e = gf_dm_setup_from_url(sess, sess->orig_url);
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
        e = GF_URL_ERROR;
        goto exit;
        break;
    case 416:
        /* Range not accepted */
        gf_dm_sess_user_io(sess, &par);
        e = GF_SERVICE_ERROR;
        goto exit;
        break;
    case 400:
    case 501:
        /* Method not implemented ! */
        if (sess->http_read_type != GET) {
            /* Since HEAD is not understood by this server, we use a GET instead */
            sess->http_read_type = GET;
            sess->flags |= GF_NETIO_SESSION_NOT_CACHED;
            gf_dm_disconnect(sess);
            sess->status = GF_NETIO_SETUP;
            sess->server_only_understand_get = 1;
            GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("Method not supported, try with GET.\n"));
            e = gf_dm_setup_from_url(sess, sess->orig_url);
            if (e) {
                sess->status = GF_NETIO_STATE_ERROR;
                sess->last_error = e;
                gf_dm_sess_notify_state(sess, sess->status, e);
            }
            return e;
        }
        return GF_URL_ERROR;
    case 503:
        /*retry without proxy*/
        if (sess->proxy_enabled==1) {
            sess->proxy_enabled=2;
            gf_dm_disconnect(sess);
            sess->status = GF_NETIO_SETUP;
            return GF_OK;
        }
    default:
        gf_dm_sess_user_io(sess, &par);
        e = GF_REMOTE_SERVICE_ERROR;
        goto exit;
    }

    if (sess->http_read_type != GET)
        sess->use_cache_file = 0;

    if (sess->http_read_type==HEAD) {
        gf_dm_disconnect(sess);
        gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
        sess->status = GF_NETIO_DISCONNECTED;
        sess->http_read_type = 0;
        return GF_OK;
    }
    {
        const char * mime_type = gf_cache_get_mime_type(sess->cache_entry);
        if (!ContentLength && mime_type && ((strstr(mime_type, "ogg") || (!strcmp(mime_type, "audio/mpeg"))))) {
            if (0 == sess->icy_metaint)
                sess->icy_metaint = -1;
            sess->use_cache_file = 0;
        }

        GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Connected to %s - error %s\n", sess->server_name, gf_error_to_string(e) ));

        /*some servers may reply without content length, but we MUST have it*/
        if (e) goto exit;
        if (sess->icy_metaint != 0) {
            assert( ! sess->use_cache_file );
            GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] ICY protocol detected\n"));
            if (mime_type && !stricmp(mime_type, "video/nsv")) {
                gf_cache_set_mime_type(sess->cache_entry, "audio/aac");
            }
            sess->icy_bytes = 0;
            sess->status = GF_NETIO_DATA_EXCHANGE;
        } else if (!ContentLength) {
            if (sess->http_read_type == GET) {
                sess->total_size = 2 << 30;
                sess->use_cache_file = 0;
                sess->status = GF_NETIO_DATA_EXCHANGE;
                sess->bytes_done = 0;
            } else {
                /*we don't expect anything*/
                gf_dm_disconnect(sess);
                gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
                sess->status = GF_NETIO_DISCONNECTED;
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
    }

    sess->window_start = sess->start_time = gf_sys_clock();
    sess->bytes_in_wnd = 0;


    /* we may have existing data in this buffer ... */
    if (!e && (BodyStart < (s32) bytesRead)) {
        gf_dm_data_received(sess, sHTTP + BodyStart, bytesRead - BodyStart);
        if (sess->init_data) gf_free(sess->init_data);
        sess->init_data_size = bytesRead - BodyStart;
        sess->init_data = (char *) gf_malloc(sizeof(char) * sess->init_data_size);
        memcpy(sess->init_data, sHTTP+BodyStart, sess->init_data_size);
    }
exit:
    if (e) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Error parsing reply: %s for URL %s\n", gf_error_to_string(e), sess->orig_url ));
        gf_dm_disconnect(sess);
        sess->status = GF_NETIO_STATE_ERROR;
        sess->last_error = e;
        gf_dm_sess_notify_state(sess, sess->status, e);
        return e;
    }
    return http_parse_remaining_body(sess, sHTTP);
}

/**
 * Default performing behaviour
 * \param sess The session
 */
void http_do_requests(GF_DownloadSession *sess)
{
    char sHTTP[GF_DOWNLOAD_BUFFER_SIZE];
    switch (sess->status) {
    case GF_NETIO_CONNECTED:
        http_send_headers(sess, sHTTP);
        break;
    case GF_NETIO_WAIT_FOR_REPLY:
        wait_for_header_and_parse(sess, sHTTP);
        break;
    case GF_NETIO_DATA_EXCHANGE:
        http_parse_remaining_body(sess, sHTTP);
        break;
    }
}

#define FILE_W_BUFFER_SZ 1024

GF_Err gf_dm_copy(const char * file_source, const char * file_dest) {
    FILE * source, * dest;
    char buff[FILE_W_BUFFER_SZ];
    u32 readen, written;
    GF_Err e = GF_OK;
    source = fopen(file_source, "rb");
    dest = fopen( file_dest, "wb");
    if (!source || !dest) {
        e = GF_IO_ERR;
        goto cleanup;
    }
    readen = fread(buff, sizeof(char), FILE_W_BUFFER_SZ, source);
    while (readen > 1) {
        written = fwrite(buff, sizeof(char), readen, dest);
        if (written != readen) {
            e = GF_IO_ERR;
            goto cleanup;
        }
        readen = fread(buff, sizeof(char), FILE_W_BUFFER_SZ, source);
    }

cleanup:
    if (source)
        fclose(source);
    if (dest)
        fclose(dest);
    return e;
}


GF_Err gf_dm_copy_or_link(const char * file_source, const char * file_dest) {
#ifdef __USE_POSIX
    if (link( file_source, file_dest))	{
#endif /* __USE_POSIX */
        return gf_dm_copy(file_source, file_dest);
#ifdef __USE_POSIX
    }
    return GF_OK;
#endif /* #ifdef __POSIX__ */
}

GF_Err gf_dm_wget(const char *url, const char *filename)
{
    GF_Err e;
    GF_DownloadSession *dnload;
    GF_DownloadManager * dm = NULL;
    if (!filename || !url)
        return GF_BAD_PARAM;
    dm = gf_dm_new(NULL);
    if (!dm)
        return GF_OUT_OF_MEM;
    dnload = gf_dm_sess_new_simple(dm, (char *)url, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL, &e);
    if (!dnload) {
        e = GF_BAD_PARAM;
        goto exit;
    }
    dnload->use_cache_file = 1;
    if (e == GF_OK) {
        e = gf_dm_sess_process(dnload);
    }
    e|= gf_cache_close_write_cache(dnload->cache_entry, dnload, e == GF_OK);
    e|= gf_dm_copy_or_link( gf_cache_get_cache_filename(dnload->cache_entry), filename );
    gf_dm_sess_del(dnload);
exit:
    if (dm)
        gf_dm_del(dm);
    return e;
}

const char *gf_dm_sess_get_resource_name(GF_DownloadSession *dnload)
{
    return dnload ? dnload->orig_url : NULL;
}

GF_Err gf_dm_sess_reset(GF_DownloadSession *sess)
{
    if (!sess) return GF_BAD_PARAM;
    sess->status = GF_NETIO_SETUP;
    sess->needs_range = 0;
    sess->range_start = sess->range_end = 0;
    sess->bytes_done = sess->bytes_in_wnd = sess->bytes_per_sec = 0;
    if (sess->init_data) gf_free(sess->init_data);
    sess->init_data = NULL;
    sess->init_data_size = 0;
    sess->last_error = GF_OK;
    sess->total_size = 0;
    sess->window_start = 0;
    sess->start_time = 0;
    return GF_OK;
}

DownloadedCacheEntry gf_dm_cache_entry_dup_readonly( const GF_DownloadSession * sess) {
    if (!sess)
        return NULL;
    return gf_cache_entry_dup_readonly(sess->cache_entry);
}

const char * gf_cache_get_cache_filename_range( const GF_DownloadSession * sess, u64 startOffset, u64 endOffset ) {
    u32 i, count;
    if (!sess || !sess->dm || endOffset < startOffset)
        return NULL;
    count = gf_list_count(sess->dm->partial_downloads);
    for (i = 0 ; i < count ; i++) {
        GF_PartialDownload * pd = gf_list_get(sess->dm->partial_downloads, i);
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
        maxLen = strlen(orig) + 22;
        newFilename = (char*)gf_malloc( maxLen );
        if (newFilename == NULL)
            return NULL;
        snprintf(newFilename, maxLen, "%s " LLU LLU, orig, startOffset, endOffset);
        fw = fopen(newFilename, "wb");
        if (!fw) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Cannot open partial cache file %s for write\n", newFilename));
            gf_free( newFilename );
            return NULL;
        }
        fr = fopen(orig, "rb");
        if (!fr) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Cannot open full cache file %s\n", orig));
            gf_free( newFilename );
            fclose( fw );
        }
        /* Now, we copy ! */
        {
            char copyBuff[GF_DOWNLOAD_BUFFER_SIZE];
            s64 read, write, total;
            total = endOffset - startOffset;
            read = gf_f64_seek(fr, startOffset, SEEK_SET);
            if (read != startOffset) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[CACHE] Cannot seek at right start offset in %s\n", orig));
                fclose( fr );
                fclose( fw );
                gf_free( newFilename );
                return NULL;
            }
            do {
                read = fread(copyBuff, sizeof(char), MIN(sizeof(copyBuff), total), fr);
                if (read > 0) {
                    total-= read;
                    write = fwrite(copyBuff, sizeof(char), read, fw);
                    if (write != read) {
                        /* Something bad happened */
                        fclose( fw );
                        fclose (fr );
                        gf_free( newFilename );
                        return NULL;
                    }
                } else {
                    if (read < 0) {
                        fclose( fw );
                        fclose( fr );
                        gf_free( newFilename );
                        return NULL;
                    }
                }
            } while (total > 0);
            fclose( fr );
            fclose (fw);
            partial = gf_malloc( sizeof(GF_PartialDownload));
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
