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

#ifdef WIN32
//#define GPAC_HAS_SSL
#endif


#ifdef GPAC_HAS_SSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif


static void gf_dm_connect(GF_DownloadSession *sess);

#define GF_DOWNLOAD_AGENT_NAME		"GPAC " GPAC_VERSION
#define GF_DOWNLOAD_BUFFER_SIZE		8192

/*internal flags*/
enum
{
	GF_DOWNLOAD_SESSION_USE_SSL		=	1<<10,
	GF_DOWNLOAD_SESSION_THREAD_DEAD	=	1<<11,
	GF_DOWNLOAD_IS_ICY				=	1<<12,
};


struct __gf_download_session
{
	/*this is always 0 and helps differenciating downloads from other interfaces (interfaceType != 0)*/
	u32 reserved;

	struct __gf_download_manager *dm;
	GF_Thread *th;
	GF_Mutex *mx;

	char *server_name;
	u16 port;
	char *remote_path;
	char *user;
	char *passwd;
	char cookie[GF_MAX_PATH];

	FILE *cache;
	char *cache_name;
	u32 cache_size;

	GF_Socket *sock;
	u32 num_retry, status;

	char *mime_type;
	u32 flags;

	u32 total_size, bytes_done, bytes_per_sec, start_time, icy_metaint, icy_count, icy_bytes;

	GF_Err last_error;
	char *init_data;
	u32 init_data_size;

#ifdef GPAC_HAS_SSL
	SSL *ssl;
#endif

	void (*do_requests)(struct __gf_download_session *);

	/*callback for data reception - may not be NULL*/
	void (*OnDataRcv)(void *usr_cbk, char *data, u32 data_size, u32 dl_state, GF_Err dl_error);
	void *usr_cbk;

	/*private extension*/
	void *usr_private;
};

struct __gf_download_manager
{
	char *cache_directory;
	char szCookieDir[GF_MAX_PATH];

	Bool (*GetUserPassword)(void *usr_cbk, const char *site_url, char *usr_name, char *password);
	void *usr_cbk;

	GF_Config *cfg;
	GF_List *sessions;

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

static int ssl_init(GF_DownloadManager *dm, u32 mode)
{
	SSL_METHOD *meth;
	
	if (!dm) return 0;
    /* The SSL has already been initialized. */
	if (dm->ssl_ctx) return 1;
	/* Init the PRNG.  If that fails, bail out.  */
	init_prng();
	if (RAND_status() != 1) goto error;
	SSL_library_init();
	SSL_load_error_strings();
	SSLeay_add_all_algorithms();
	SSLeay_add_ssl_algorithms();
	
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
	return 1;
error:
	if (dm->ssl_ctx) SSL_CTX_free(dm->ssl_ctx);
	dm->ssl_ctx = NULL;
	return 0;
}

#endif


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


void gf_dm_configure_cache(GF_DownloadSession *sess)
{
	u32 i, last_sep;
	char cache_name[GF_MAX_PATH], tmp[GF_MAX_PATH];
	const char *opt;

	if (!sess->dm->cache_directory) return;
	if (sess->flags & GF_DOWNLOAD_SESSION_NOT_CACHED) return;

	strcpy(cache_name, sess->dm->cache_directory);

	strcpy(tmp, sess->server_name);
	strcat(tmp, sess->remote_path);
	last_sep = 0;

	for (i=0; i<strlen(tmp); i++) {
		if (tmp[i] == '/') tmp[i] = '_';
		else if (tmp[i] == '.') {
			tmp[i] = '_';
			last_sep = i;
		}
	}
	if (last_sep) tmp[last_sep] = '.';
	strcat(cache_name, tmp);

	/*first try, check cached file*/
	if (!sess->cache_size) {
		/*if file present figure out how much of the file is downloaded - we assume 2^31 byte file max*/
		FILE *the_cache = fopen(cache_name, "rb");
		if (the_cache) {
			fseek(the_cache, 0, SEEK_END);
			sess->cache_size = ftell(the_cache);
			fclose(the_cache);
		}
	}
	/*second try, disable cached file*/
	else {
		sess->cache_size = 0;
	}
	sess->cache_name = strdup(cache_name);

	/*are we using existing cached files ?*/
	opt = gf_cfg_get_key(sess->dm->cfg, "Downloader", "RestartFiles");
	if (opt && !stricmp(opt, "yes")) sess->cache_size = 0;
}

static void gf_dm_disconnect(GF_DownloadSession *sess)
{
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
	if (sess->cache) fclose(sess->cache);
	sess->cache = NULL;
	sess->status = GF_DOWNLOAD_STATE_DISCONNECTED;
	if (sess->num_retry) sess->num_retry--;
}

void gf_dm_sess_del(GF_DownloadSession *sess)
{
	const char *opt;
	gf_dm_disconnect(sess);
	/*if threaded wait for thread exit*/
	if (sess->th) {
		while (!(sess->flags & GF_DOWNLOAD_SESSION_THREAD_DEAD)) 
			gf_sleep(1);
		gf_th_del(sess->th);
		gf_mx_del(sess->mx);
	}
	
	gf_list_del_item(sess->dm->sessions, sess);

	if (sess->cache_name) {
		opt = gf_cfg_get_key(sess->dm->cfg, "Downloader", "CleanCache");
		if (opt && !stricmp(opt, "yes")) gf_delete_file(sess->cache_name);
		free(sess->cache_name);
	}
	
	if (sess->server_name) free(sess->server_name);
	if (sess->remote_path) free(sess->remote_path);
	if (sess->user) free(sess->user);
	if (sess->passwd) free(sess->passwd);
	if (sess->mime_type) free(sess->mime_type);
	if (sess->cache) fclose(sess->cache);


	free(sess);
}

void http_do_requests(GF_DownloadSession *sess);


GF_Err gf_dm_sess_last_error(GF_DownloadSession *sess)
{
	if (!sess) return GF_BAD_PARAM;
	return sess->last_error;
}


static GF_Err gf_dm_setup_from_url(GF_DownloadSession *sess, char *url)
{
	char *tmp, tmp_url[GF_MAX_PATH];

	if (!strnicmp(url, "http://", 7)) {
		url += 7;
		sess->port = 80;
		sess->do_requests = http_do_requests;
	} 
	else if (!strnicmp(url, "https://", 8)) {
		url += 8;
		sess->port = 443;
#ifndef GPAC_HAS_SSL
		return GF_NOT_SUPPORTED;
#endif
		sess->flags |= GF_DOWNLOAD_SESSION_USE_SSL;
		sess->do_requests = http_do_requests;
	}
	else if (!strnicmp(url, "ftp://", 6)) {
		url += 6;
		sess->port = 21;
		sess->do_requests = NULL;
		return GF_NOT_SUPPORTED;
	} 
	/*relative URL*/
	else if (!strstr(url, "://")) {
		u32 i;
		if (!sess->remote_path) return GF_BAD_PARAM;
		tmp = gf_url_concatenate(sess->remote_path, url);
		free(sess->remote_path);
		sess->remote_path = tmp;
		for (i=0; i<strlen(sess->remote_path); i++)
			if (sess->remote_path[i]=='\\') sess->remote_path[i]='/';

	} else {
		return GF_BAD_PARAM;
	}


	tmp = strchr(url, '/');
	sess->remote_path = strdup(tmp ? tmp : "/");
	if (tmp) {
		tmp[0] = 0;
		strcpy(tmp_url, url);
		tmp[0] = '/';
	} else {
		strcpy(tmp_url, url);
	}
	
	tmp = strrchr(tmp_url, ':');
	if (tmp) {
		sess->port = atoi(tmp+1);
		tmp[0] = 0;
	}
	tmp = strrchr(tmp_url, '@');
	if (tmp) {
		sess->server_name = strdup(tmp+1);
		tmp[0] = 0;
		tmp = strchr(tmp_url, ':');
		if (sess->user) free(sess->user);
		sess->user = NULL;
		if (sess->passwd) free(sess->passwd);
		sess->passwd = NULL;

		if (tmp) {
			sess->passwd = strdup(tmp+1);
			tmp[0] = 0;
		}
		sess->user = strdup(tmp_url);
	} else {
		sess->server_name = strdup(tmp_url);
	}
	return GF_OK;
}


#define GF_WAIT_REPLY_SLEEP	20
static u32 gf_dm_session_thread(void *par)
{
	GF_DownloadSession *sess = par;

	sess->flags &= ~GF_DOWNLOAD_SESSION_THREAD_DEAD;
	while (1) {
		gf_mx_p(sess->mx);
		if (sess->status >= GF_DOWNLOAD_STATE_DISCONNECTED) break;

		if (sess->status < GF_DOWNLOAD_STATE_CONNECTED) {
			gf_dm_connect(sess);
		} else {
			if (sess->status ==GF_DOWNLOAD_STATE_WAIT_FOR_REPLY) gf_sleep(GF_WAIT_REPLY_SLEEP);
			sess->do_requests(sess);
		}
		gf_mx_v(sess->mx);
		gf_sleep(2);
	}
	/*destroy all sessions*/
	gf_dm_disconnect(sess);
	sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
	sess->flags |= GF_DOWNLOAD_SESSION_THREAD_DEAD;
	return 1;
}

#define SESSION_RETRY_COUNT	20

GF_DownloadSession *gf_dm_sess_new(GF_DownloadManager *dm, char *url, u32 dl_flags,
									  void (*OnDataRcv)(void *usr_cbk, char *data, u32 data_size, u32 dnload_status, GF_Err dl_error),
									  void *usr_cbk,
									  void *private_data,
									  GF_Err *e)
{
	GF_DownloadSession *sess;

	*e = GF_OK;
	if (gf_dm_is_local(dm, url)) return NULL;

	if (!gf_dm_can_handle_url(dm, url)) {
		*e = GF_NOT_SUPPORTED;
		return NULL;
	}
	if (!OnDataRcv) {
		*e = GF_BAD_PARAM;
		return NULL;
	}


	GF_SAFEALLOC(sess, sizeof(GF_DownloadSession));
	sess->flags = dl_flags;
	sess->OnDataRcv = OnDataRcv;
	sess->usr_cbk = usr_cbk;
	sess->dm = dm;
	sess->usr_private = private_data;
	gf_list_add(dm->sessions, sess);

	*e = gf_dm_setup_from_url(sess, url);
	if (*e) {
		gf_dm_sess_del(sess);
		return NULL;
	}
	if (!(sess->flags & GF_DOWNLOAD_SESSION_NOT_THREADED) ) {
		sess->th = gf_th_new();
		sess->mx = gf_mx_new();
		gf_th_run(sess->th, gf_dm_session_thread, sess);
	}
	sess->num_retry = SESSION_RETRY_COUNT;
	return sess;
}

static GF_Err gf_dm_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read)
{
	GF_Err e;
	
#ifdef GPAC_HAS_SSL
	if (sess->ssl) {
		u32 size = SSL_read(sess->ssl, data, data_size);
		e = GF_OK;
		data[size] = 0;
		if (!size) e = GF_IP_NETWORK_EMPTY;
		*out_read = size;
	} else 
#endif
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
	if (!sess->sock) {
		//sess->num_retry = 40;
		sess->sock = gf_sk_new(GF_SOCK_TYPE_TCP);
	}

	/*connect*/
	sess->status = GF_DOWNLOAD_STATE_SETUP;
	sess->OnDataRcv(sess->usr_cbk, NULL, 0, sess->status, GF_OK);

	e = gf_sk_connect(sess->sock, sess->server_name, sess->port);
	/*retry*/
	if ((e == GF_IP_SOCK_WOULD_BLOCK) && sess->num_retry) {
		sess->status = GF_DOWNLOAD_STATE_SETUP;
		sess->num_retry--;
		return;
	}

	/*failed*/
	if (e) {
		sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
		sess->OnDataRcv(sess->usr_cbk, NULL, 0, GF_DOWNLOAD_STATE_UNAVAILABLE, e);
		sess->last_error = e;
		return;
	} 
	
	sess->status = GF_DOWNLOAD_STATE_CONNECTED;
	sess->OnDataRcv(sess->usr_cbk, NULL, 0, GF_DOWNLOAD_STATE_CONNECTED, GF_OK);
	gf_sk_set_blocking(sess->sock, 1);
	gf_dm_configure_cache(sess);

#ifdef GPAC_HAS_SSL
	/*socket is connected, configure SSL layer*/
	if (!sess->ssl && sess->dm->ssl_ctx && (sess->flags & GF_DOWNLOAD_SESSION_USE_SSL)) {
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
				sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
				sess->last_error = GF_AUTHENTICATION_FAILURE;
				sess->OnDataRcv(sess->usr_cbk, NULL, 0, sess->status, GF_AUTHENTICATION_FAILURE);
			}
		}
	}
#endif
}

const char *gf_dm_sess_mime_type(GF_DownloadSession *sess)
{
	Bool go;
	u32 flags = sess->flags;
	sess->flags |= GF_DOWNLOAD_SESSION_NOT_CACHED;

	go = 1;
	while (go) {
		switch (sess->status) {
		/*setup download*/
		case GF_DOWNLOAD_STATE_SETUP:
			gf_dm_connect(sess);
			break;
		case GF_DOWNLOAD_STATE_WAIT_FOR_REPLY:
			gf_sleep(20);
		case GF_DOWNLOAD_STATE_CONNECTED:
			sess->do_requests(sess);
			break;
		case GF_DOWNLOAD_STATE_RUNNING:
		case GF_DOWNLOAD_STATE_DISCONNECTED:
		case GF_DOWNLOAD_STATE_UNAVAILABLE:
			go = 0;
			break;
		}
	}
	sess->flags = flags;
	if (sess->status==GF_DOWNLOAD_STATE_UNAVAILABLE) return NULL;
	return sess->mime_type;
}




GF_DownloadManager *gf_dm_new(GF_Config *cfg)
{
	const char *opt;
	GF_DownloadManager *dm;
	if (!cfg) return NULL;
	GF_SAFEALLOC(dm, sizeof(GF_DownloadManager));
	dm->sessions = gf_list_new();
	dm->cfg = cfg;

	opt = gf_cfg_get_key(cfg, "General", "CacheDirectory");
	if (opt) {
		if (opt[strlen(opt)-1] != GF_PATH_SEPARATOR) {
			dm->cache_directory = malloc(sizeof(char)* (strlen(opt)+2));
			sprintf(dm->cache_directory, "%s%c", opt, GF_PATH_SEPARATOR);
		} else {
			dm->cache_directory = strdup(opt);
		}
	}
#ifdef GPAC_HAS_SSL
	ssl_init(dm, 0);
#endif
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
	/*this should never happen (bad cleanup from user)*/
	while (gf_list_count(dm->sessions)) {
		GF_DownloadSession *sess = gf_list_get(dm->sessions, 0);
		gf_dm_sess_del(sess);
	}
	gf_list_del(dm->sessions);

	free(dm->cache_directory);

#ifdef GPAC_HAS_SSL
	if (dm->ssl_ctx) SSL_CTX_free(dm->ssl_ctx);
#endif

	free(dm);
}


static GFINLINE void gf_dm_data_recieved(GF_DownloadSession *sess, char *data, u32 nbBytes)
{
	u32 runtime;
	if (! (sess->flags & GF_DOWNLOAD_SESSION_NOT_CACHED)) {
		if (sess->cache) {
			fwrite(data, nbBytes, 1, sess->cache);
			fflush(sess->cache);
		}
		sess->bytes_done += nbBytes;
		/*if not threaded don't signal data to user*/
		if (sess->th) sess->OnDataRcv(sess->usr_cbk, NULL, nbBytes, sess->status, GF_OK);
	} else if (sess->icy_metaint) {
		while (nbBytes) {
			if (sess->icy_bytes == sess->icy_metaint) {
				sess->icy_count = 1 + 16* (u8) data[0];
				
				/*skip icy metadata*/
				if (sess->icy_count >= nbBytes) {
					sess->icy_count -= nbBytes;
					nbBytes = 0;
				} else {
					if (sess->icy_count > 1) {
						char szData[4096];
						memcpy(szData, data+1, sess->icy_count-1);
						szData[sess->icy_count] = 0;
					}

					nbBytes -= sess->icy_count;
					data += sess->icy_count;
					sess->icy_count = 0;
					sess->icy_bytes = 0;
				}
			} else {
				u32 left = sess->icy_metaint - sess->icy_bytes;
				if (left > nbBytes) {
					left = nbBytes;
					sess->icy_bytes += left;
					nbBytes = 0;
				} else {
					sess->icy_bytes = sess->icy_metaint;
					nbBytes -= left;
				}
				sess->OnDataRcv(sess->usr_cbk, data, left, sess->status, GF_OK);
				data += left;
			}
		}
	} else {
		sess->bytes_done += nbBytes;
		/*if not threaded don't signal data to user*/
		if (sess->th) sess->OnDataRcv(sess->usr_cbk, data, nbBytes, sess->status, GF_OK);
	}

	if (sess->total_size && (sess->bytes_done == sess->total_size)) {
		gf_dm_disconnect(sess);
		sess->OnDataRcv(sess->usr_cbk, NULL, 0, sess->status, GF_EOS);
		return;
	}
	/*update state if not done*/
	runtime = gf_sys_clock() - sess->start_time;
	if (!runtime) {
		sess->bytes_per_sec = 0;
	} else {
		sess->bytes_per_sec = (1000 * (sess->bytes_done - sess->cache_size)) / runtime;
	}
}


GF_Err gf_dm_sess_fetch_data(GF_DownloadSession *sess, char *buffer, u32 buffer_size, u32 *read_size)
{
	GF_Err e;
	if (sess->cache || !buffer || !buffer_size) return GF_BAD_PARAM;
	if (sess->th) return GF_BAD_PARAM;
	if (sess->status == GF_DOWNLOAD_STATE_DISCONNECTED) return GF_EOS;
	if (sess->status > GF_DOWNLOAD_STATE_RUNNING) return GF_BAD_PARAM;

	*read_size = 0;

	if (sess->status == GF_DOWNLOAD_STATE_SETUP) {
		gf_dm_connect(sess);
		return GF_OK;
	} else if (sess->status < GF_DOWNLOAD_STATE_RUNNING) {
		sess->do_requests(sess);
		return GF_OK;
	}
	/*we're running but we had data previously*/
	if (sess->init_data) {
		memcpy(buffer, sess->init_data, sizeof(char)*sess->init_data_size);
		*read_size = sess->init_data_size;
		free(sess->init_data);
		sess->init_data = NULL;
		sess->init_data_size = 0;
		return GF_OK;
	}

	e = gf_dm_read_data(sess, buffer, buffer_size, read_size);
	if (e) return e;
	gf_dm_data_recieved(sess, buffer, *read_size);
	return GF_OK;
}

GF_Err gf_dm_sess_get_stats(GF_DownloadSession * sess, const char **server, const char **path, u32 *total_size, u32 *bytes_done, u32 *bytes_per_sec, u32 *net_status)
{
	if (!sess) return GF_BAD_PARAM;
	if (server) *server = sess->server_name;
	if (path) *path = sess->remote_path;
	if (total_size) *total_size = sess->total_size;
	if (bytes_done) *bytes_done = sess->bytes_done;
	if (bytes_per_sec) *bytes_per_sec = sess->bytes_per_sec;
	if (net_status) *net_status = sess->status;
	if (sess->status == GF_DOWNLOAD_STATE_DISCONNECTED) return GF_EOS;
	return GF_OK;
}

const char *gf_dm_sess_get_cache_name(GF_DownloadSession * sess)
{
	if (!sess) return NULL;
	if (sess->cache) return sess->cache_name;
	else if (sess->total_size==sess->cache_size) return sess->cache_name;
	return NULL;
}

void gf_dm_sess_abort(GF_DownloadSession * sess)
{
	if (sess->mx) {
		gf_mx_p(sess->mx);
		gf_dm_disconnect(sess);
		sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
		gf_mx_v(sess->mx);
	} else {
		gf_dm_disconnect(sess);
	}
}
void *gf_dm_sess_get_private(GF_DownloadSession * sess)
{
	return sess ? sess->usr_private : NULL;
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

void http_do_requests(GF_DownloadSession *sess)
{
	GF_Err e;
	Bool is_ice;
	char sHTTP[GF_DOWNLOAD_BUFFER_SIZE];
	unsigned char buf[1024];
	unsigned char comp[400];
	unsigned char *new_location;
	char *hdr;
	s32 bytesRead, res;
	s32 LinePos, Pos;
	u32 rsp_code, BodyStart, ContentLength, first_byte, last_byte, total_size, range, no_range;

	/*sent HTTP request*/
	if (sess->status==GF_DOWNLOAD_STATE_CONNECTED) {
		unsigned char range_buf[1024];
		unsigned char pass_buf[1024];
		u32 size;

		/*setup authentification*/
		strcpy(pass_buf, "");
		if (sess->user) {
			if (!sess->passwd) {
				char szUSR[50], szPASS[50];
				strcpy(szUSR, sess->user);
				strcpy(szPASS, "");
				/*failed getting pass*/
				if (!sess->dm->GetUserPassword || !sess->dm->GetUserPassword(sess->dm->usr_cbk, sess->server_name, szUSR, szPASS)) {
					sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
					return;
				}
				sess->passwd = strdup(szPASS);
			}
			sprintf(pass_buf, "%s:%s", sess->user, sess->passwd);
			size = gf_base64_encode(pass_buf, strlen(pass_buf), range_buf, 1024);
			range_buf[size] = 0;
			sprintf(pass_buf, "Authorization: Basic %s", range_buf);
		}

		/*setup file range*/
		strcpy(range_buf, "");
		if (sess->cache_size) sprintf(range_buf, "Range: bytes=%d-\r\n", sess->cache_size);
		

		/*MIX2005 KMS project*/
#if 0
		if (strstr(sess->remote_path, "getKey.php?")) {
			char *sLogin, *sPass;
			sLogin = gf_modules_get_option((GF_BaseInterface *)sess->dm->cfg, "General", "KMS_User");
			sPass = gf_modules_get_option((GF_BaseInterface *)sess->dm->cfg, "General", "KMS_Password");
			if (!sLogin) sLogin = "mix";
			if (!sPass) sPass = "mix";
			sprintf(https_get_buffer, "%s&login=%s&password=%s", sess->remote_path, sLogin, sPass);
		}
#endif	

		sprintf(sHTTP, "GET %s HTTP/1.0\r\n"
					"Host: %s\r\n"
					"User-Agent: %s\r\n"
					"Accept: */*\r\n"
					"%s"
					"%s"
					"%s"
					"\r\n", 
					sess->remote_path, 
					sess->server_name,
					GF_DOWNLOAD_AGENT_NAME,
					(const char *) pass_buf,
					(const char *) range_buf,
					(sess->flags & GF_DOWNLOAD_IS_ICY) ? "Icy-Metadata:1\r\n" : ""
					);

#ifdef GPAC_HAS_SSL
		if (sess->ssl) {
			e = GF_IP_NETWORK_FAILURE;
			if (!SSL_write(sess->ssl, sHTTP, strlen(sHTTP))) e = GF_OK;
		} else 
#endif
			e = gf_sk_send(sess->sock, sHTTP, strlen(sHTTP));

		if (e) {
			sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
			sess->last_error = e;
			sess->OnDataRcv(sess->usr_cbk, NULL, 0, GF_DOWNLOAD_STATE_UNAVAILABLE, e);
		} else {
			sess->status = GF_DOWNLOAD_STATE_WAIT_FOR_REPLY;
			sess->OnDataRcv(sess->usr_cbk, NULL, 0, GF_DOWNLOAD_STATE_WAIT_FOR_REPLY, GF_OK);
		}
		return;
	}

	/*process HTTP request*/
	if (sess->status == GF_DOWNLOAD_STATE_WAIT_FOR_REPLY) {
		bytesRead = res = 0;
		new_location = NULL;
		while (1) {
			e = gf_dm_read_data(sess, sHTTP + bytesRead, GF_DOWNLOAD_BUFFER_SIZE - bytesRead, &res);
	
			switch (e) {
			case GF_IP_NETWORK_EMPTY:
				if (!bytesRead) return;
				continue;
			/*socket has been closed while configuring, retry (not sure if the server got the GET)*/
			case GF_IP_CONNECTION_CLOSED:
				gf_dm_disconnect(sess);
				if (sess->num_retry)
					sess->status = GF_DOWNLOAD_STATE_SETUP;
				else {
					sess->last_error = e;
					sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
				}
				return;
			case GF_OK:
				break;
			default:
				goto exit;
			}
			if (!bytesRead) {
				bytesRead += res;
				continue;
			}
			bytesRead += res;

			/*locate body start*/
			BodyStart = gf_token_find(sHTTP, 0, bytesRead, "\r\n\r\n");
			if (!BodyStart) {
				//BodyStart = gf_token_find(sHTTP, 0, bytesRead, "\n\n");
				if (!BodyStart) continue;
				BodyStart += 2;
			} else {
				BodyStart += 4;
			}
			break;
		}
		if (bytesRead < 0) {
			e = GF_REMOTE_SERVICE_ERROR;
			goto exit;
		}
		sHTTP[BodyStart] = 0;

		LinePos = gf_token_get_line(sHTTP, 0, bytesRead, buf, 1024);
		Pos = gf_token_get(buf, 0, " \t\r\n", comp, 400);

		if (sess->mime_type) free(sess->mime_type);
		sess->mime_type = NULL;

		is_ice = 0;
		if (!strncmp("ICY", comp, 4)) {
			is_ice = 1;
			/*be prepared not to recieve any mime type from ShoutCast servers*/
			sess->mime_type = strdup("audio/mpeg");
		} else if ((strncmp("HTTP", comp, 4) != 0)) {
			e = GF_REMOTE_SERVICE_ERROR;
			goto exit;
		}
		Pos = gf_token_get(buf, Pos, " \t\r\n", comp, 400);
		if (Pos <= 0) {
			e = GF_REMOTE_SERVICE_ERROR;
			goto exit;
		}
		rsp_code = (u32) atoi(comp);	


		no_range = range = ContentLength = first_byte = last_byte = total_size = 0;
		//parse header
		while (1) {
			if ( (u32) LinePos + 4 > BodyStart) break;
			LinePos = gf_token_get_line(sHTTP, LinePos , bytesRead, buf, 1024);
			if (LinePos < 0) break;

			if ((hdr = http_is_header(buf, "Content-Length")) ) {			
				ContentLength = (u32) atoi(hdr);
			}
			else if ((hdr = http_is_header(buf, "Content-Type")) ) {			
				if (sess->mime_type) free(sess->mime_type);
				sess->mime_type = strdup(hdr);
				while (1) {
					u32 len = strlen(sess->mime_type);
					char c = len ? sess->mime_type[len-1] : 0;
					if ((c=='\r') || (c=='\n')) {
						sess->mime_type[len-1] = 0;
					} else {
						break;
					}
				}
				hdr = strchr(sess->mime_type, ';');
				if (hdr) hdr[0] = 0;
			}
			else if ((hdr = http_is_header(buf, "Content-Range")) ) {			
				range = 1;
				if (!strncmp(hdr, "bytes", 5)) {
					hdr += 5;
					if (*hdr == ':') hdr += 1;
					hdr += http_skip_space(hdr);
					if (*hdr == '*') {
						sscanf(hdr, "*/%d", &total_size);
					} else {
						sscanf(hdr, "%d-%d/%d", &first_byte, &last_byte, &total_size);
					}
				}
			}
			else if ((hdr = http_is_header(buf, "Accept-Ranges"))) {
				if (strstr(hdr, "none")) no_range = 1;
			}
			else if ((hdr = http_is_header(buf, "Location"))) {
				new_location = strdup(hdr);
			}
			else if ((hdr = http_is_header(buf, "icy-metaint"))) {
				sess->icy_metaint = atoi(hdr);
			}
			else if (!strnicmp(buf, "ice", 3) || !strnicmp(buf, "icy", 3) ) is_ice = 1;
		}
		if (no_range) first_byte = 0;

		if (sess->cache_size) {
			if (total_size && (sess->cache_size >= total_size) ) {
				rsp_code = 200;
				ContentLength = total_size;
			}
			if (ContentLength && (sess->cache_size == ContentLength) ) rsp_code = 200;
		}	

		switch (rsp_code) {
		case 200:
		case 201:
		case 202:
		case 206:
			e = GF_OK;
			break;
		/*redirection: extract the new location*/
		case 301:
		case 302:
			if (!new_location || !strlen(new_location) ) {
				e = GF_URL_ERROR;
				goto exit;
			}
			while (
				(new_location[strlen(new_location)-1] == '\n') 
				|| (new_location[strlen(new_location)-1] == '\r')  )
				new_location[strlen(new_location)-1] = 0;

			/*reset and reconnect*/
			gf_dm_disconnect(sess);
			sess->status = GF_DOWNLOAD_STATE_SETUP;
			e = gf_dm_setup_from_url(sess, new_location);
			if (e) {
				sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
				sess->last_error = e;
				sess->OnDataRcv(sess->usr_cbk, NULL, 0, sess->status, e);
				return;
			}
			return;
		case 404:
		case 416:
			/*try without cache (some servers screw up when content-length is specified)*/
			if (sess->cache_size) {
				gf_dm_disconnect(sess);
				sess->status = GF_DOWNLOAD_STATE_SETUP;
				return;
			} else if (is_ice && !(sess->flags & GF_DOWNLOAD_IS_ICY)) {
				gf_dm_disconnect(sess);
				sess->status = GF_DOWNLOAD_STATE_SETUP;
				sess->flags |= GF_DOWNLOAD_IS_ICY;
				return;
			}
			e = GF_URL_ERROR;
			goto exit;
		case 503:
		default:
			e = GF_REMOTE_SERVICE_ERROR;
			goto exit;
		}

		if (!ContentLength && sess->mime_type && strstr(sess->mime_type, "ogg")) is_ice = 1;

		/*some servers may reply without content length, but we MUST have it*/
		if (!is_ice && !ContentLength) e = GF_REMOTE_SERVICE_ERROR;
		if (e) goto exit;

		/*force disabling cache (no content length)*/
		if (is_ice) {
			sess->flags |= GF_DOWNLOAD_SESSION_NOT_CACHED;
			if (sess->mime_type && !stricmp(sess->mime_type, "video/nsv")) {
				free(sess->mime_type);
				sess->mime_type = strdup("audio/aac");
			}
		}


		/*done*/
		if (sess->cache_size 
			&& ( (total_size && sess->cache_size >= total_size) || (sess->cache_size == ContentLength)) ) {
			sess->total_size = sess->bytes_done = sess->cache_size;
			/*disconnect*/
			gf_dm_disconnect(sess);
			BodyStart = bytesRead;
			sess->OnDataRcv(sess->usr_cbk, NULL, 0, sess->status, GF_EOS);
		}
		else if (sess->flags & GF_DOWNLOAD_IS_ICY) {
			sess->icy_bytes = 0;
			sess->status = GF_DOWNLOAD_STATE_RUNNING;
		}
		/*no range header, Accep-Ranges deny or dumb server : restart*/
		else if (!range || !first_byte || (first_byte != sess->cache_size) ) {
			sess->cache_size = sess->bytes_done = 0;
			sess->total_size = ContentLength;
			if (! (sess->flags & GF_DOWNLOAD_SESSION_NOT_CACHED) ) {
				sess->cache = fopen(sess->cache_name, "wb");
				if (!sess->cache) {
					e = GF_IO_ERR;
					goto exit;
				}
			}
			sess->status = GF_DOWNLOAD_STATE_RUNNING;
		}
		/*resume*/
		else {
			sess->total_size = ContentLength + sess->cache_size;
			if (! (sess->flags & GF_DOWNLOAD_SESSION_NOT_CACHED) ) {
				sess->cache = fopen(sess->cache_name, "ab");
				if (!sess->cache) {
					e = GF_IO_ERR;
					goto exit;
				}
			}
			sess->status = GF_DOWNLOAD_STATE_RUNNING;
			sess->bytes_done = sess->cache_size;
		}

		sess->start_time = gf_sys_clock();
		
		//we may have existing data in this buffer ...
		if (!e && (BodyStart < (u32) bytesRead)) {
			gf_dm_data_recieved(sess, sHTTP + BodyStart, bytesRead - BodyStart);
			/*store data if no callbacks or cache*/
			if (sess->flags & GF_DOWNLOAD_SESSION_NOT_CACHED) {
				if (sess->init_data) free(sess->init_data);
				sess->init_data_size = bytesRead - BodyStart;
				sess->init_data = malloc(sizeof(char) * sess->init_data_size);
				memcpy(sess->init_data, sHTTP+BodyStart, sess->init_data_size);
			}
		}
exit:
		if (e) {
			gf_dm_disconnect(sess);
			sess->status = GF_DOWNLOAD_STATE_UNAVAILABLE;
			sess->last_error = e;
			sess->OnDataRcv(sess, NULL, 0, sess->status, e);
		}
		return;
	}
	/*fetch data*/
	while (1) {
		u32 size;
		e = gf_dm_read_data(sess, sHTTP, GF_DOWNLOAD_BUFFER_SIZE, &size);
		if (!size || e == GF_IP_NETWORK_EMPTY) return;

		if (e) {
			gf_dm_disconnect(sess);
			sess->last_error = e;
			sess->OnDataRcv(sess->usr_cbk, NULL, 0, sess->status, e);
			return;
		}
		gf_dm_data_recieved(sess, sHTTP, size);
		/*socket empty*/
		if (size < GF_DOWNLOAD_BUFFER_SIZE) return;
	}
}

