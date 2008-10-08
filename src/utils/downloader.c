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


#ifdef GPAC_HAS_SSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif


static void gf_dm_connect(GF_DownloadSession *sess);

#define GF_DOWNLOAD_AGENT_NAME		"GPAC/" GPAC_FULL_VERSION
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

	Bool in_callback, destroy;

	char *server_name;
	u16 port;

	char *remote_path;
	char *user;
	char *passwd;
	char cookie[GF_MAX_PATH];

	FILE *cache;
	char *cache_name;
	/*cache size if existing*/
	u32 cache_start_size;

	GF_Socket *sock;
	u32 num_retry, status;

	char *mime_type;
	u32 flags;

	u32 total_size, bytes_done, start_time, icy_metaint, icy_count, icy_bytes;
	u32 bytes_per_sec, window_start, bytes_in_wnd;
	u32 limit_data_rate;
	
	/*0: GET
	  1: HEAD
	  2: all the rest
	*/
	u32 http_read_type;

	GF_Err last_error;
	char *init_data;
	u32 init_data_size;

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
	u32 i, len;
	char *tmp, *ext;
	u8 hash[20];
	const char *opt;

	if (!sess->dm->cache_directory) return;
	if (sess->flags & GF_NETIO_SESSION_NOT_CACHED) return;

	len = strlen(sess->server_name) + strlen(sess->remote_path) + 1;
	if (len<50) len = 50;
	tmp = malloc(sizeof(char) * len);
	tmp[0] = 0;

	/*generate hash of the full url*/
	strcpy(tmp, sess->server_name);
	strcat(tmp, sess->remote_path);
	gf_sha1_csum(tmp, strlen(tmp), hash);
	tmp[0] = 0;
	for (i=0; i<20; i++) {
		char t[3];
		t[2] = 0;
		sprintf(t, "%02X", hash[i]);
		strcat(tmp, t);
	}

	len += strlen(sess->dm->cache_directory) + 6;
	sess->cache_name = malloc(sizeof(char)*len);
	sess->cache_name[0] = 0;

	strcpy(sess->cache_name, sess->dm->cache_directory);
	strcat(sess->cache_name, tmp);

	/*try to locate an extension*/
	strcpy(tmp, sess->remote_path);
	ext = strchr(tmp, '?');
	if (ext) ext[0] = 0;
	ext = strchr(tmp, '.');
	if (ext && (strlen(ext)<6) ) strcat(sess->cache_name, ext);
	free(tmp);

	/*first try, check cached file*/
	if (!sess->cache_start_size) {
		/*if file present figure out how much of the file is downloaded - we assume 2^31 byte file max*/
		FILE *the_cache = fopen(sess->cache_name, "rb");
		if (the_cache) {
			fseek(the_cache, 0, SEEK_END);
			sess->cache_start_size = ftell(the_cache);
			fclose(the_cache);
		}
	}
	/*second try, disable cached file*/
	else {
		sess->cache_start_size = 0;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Cache setup to %s\n", sess->cache_name));

	/*are we using existing cached files ?*/
	opt = gf_cfg_get_key(sess->dm->cfg, "Downloader", "RestartFiles");
	if (opt && !stricmp(opt, "yes")) sess->cache_start_size = 0;
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
	sess->status = GF_NETIO_DISCONNECTED;
	if (sess->num_retry) sess->num_retry--;
}

void gf_dm_sess_del(GF_DownloadSession *sess)
{
	const char *opt;

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
	if (sess->init_data) free(sess->init_data);
	free(sess);
}

void http_do_requests(GF_DownloadSession *sess);

static void gf_dm_sess_notify_state(GF_DownloadSession *sess, u32 dnload_status, GF_Err error)
{
	GF_NETIO_Parameter par;
	sess->in_callback = 1;
	memset(&par, 0, sizeof(GF_NETIO_Parameter));
	par.msg_type = dnload_status;
	par.error = error;
	sess->user_proc(sess->usr_cbk, &par);
	sess->in_callback = 0;
}

static void gf_dm_sess_user_io(GF_DownloadSession *sess, GF_NETIO_Parameter *par)
{
	sess->in_callback = 1;
	sess->user_proc(sess->usr_cbk, par);
	sess->in_callback = 0;
}

GF_Err gf_dm_sess_last_error(GF_DownloadSession *sess)
{
	if (!sess) return GF_BAD_PARAM;
	return sess->last_error;
}


static GF_Err gf_dm_setup_from_url(GF_DownloadSession *sess, char *url)
{
	char *tmp, *tmp_url;
	const char *opt;
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
		tmp_url = strdup(url);
		tmp[0] = '/';
	} else {
		tmp_url = strdup(url);
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
	free(tmp_url);

	/*setup BW limiter*/
	sess->limit_data_rate = 0;
	opt = gf_cfg_get_key(sess->dm->cfg, "Downloader", "MaxRate");
	if (opt) {
		/*use it in in BYTES per second*/
		sess->limit_data_rate = 1024 * atoi(opt) / 8;
	}

	return GF_OK;
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

GF_DownloadSession *gf_dm_sess_new(GF_DownloadManager *dm, char *url, u32 dl_flags,
									  gf_dm_user_io user_io,
									  void *usr_cbk,
									  GF_Err *e)
{
	GF_DownloadSession *sess;

	*e = GF_OK;
	if (gf_dm_is_local(dm, url)) return NULL;

	if (!gf_dm_can_handle_url(dm, url)) {
		*e = GF_NOT_SUPPORTED;
		return NULL;
	}
	if (!user_io) {
		*e = GF_BAD_PARAM;
		return NULL;
	}


	sess = (GF_DownloadSession *)malloc(sizeof(GF_DownloadSession));
	memset((void *)sess, 0, sizeof(GF_DownloadSession));
	sess->flags = dl_flags;
	sess->user_proc = user_io;
	sess->usr_cbk = usr_cbk;
	sess->dm = dm;
	gf_list_add(dm->sessions, sess);

	*e = gf_dm_setup_from_url(sess, url);
	if (*e) {
		gf_dm_sess_del(sess);
		return NULL;
	}
	if (!(sess->flags & GF_NETIO_SESSION_NOT_THREADED) ) {
		sess->th = gf_th_new(url);
		sess->mx = gf_mx_new(url);
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
	u16 proxy_port = 0;
	const char *proxy, *ip;
	if (!sess->sock) {
		sess->num_retry = 40;
		sess->sock = gf_sk_new(GF_SOCK_TYPE_TCP);
		//gf_sk_set_block_mode(sess->sock, 1);
	}

	/*connect*/
	sess->status = GF_NETIO_SETUP;
	gf_dm_sess_notify_state(sess, sess->status, GF_OK);
	
	/*PROXY setup*/
	proxy = gf_cfg_get_key(sess->dm->cfg, "HTTPProxy", "Enabled");
	if (proxy && !strcmp(proxy, "yes")) {
		proxy = gf_cfg_get_key(sess->dm->cfg, "HTTPProxy", "Port");
		proxy_port = proxy ? atoi(proxy) : 80;
		
		proxy = gf_cfg_get_key(sess->dm->cfg, "HTTPProxy", "Name");
	} else {
		proxy = NULL;
	}


	ip = gf_cfg_get_key(sess->dm->cfg, "Network", "MobileIPEnabled");
	if (ip && !strcmp(ip, "yes")) {
		ip = gf_cfg_get_key(sess->dm->cfg, "Network", "MobileIP");
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
	//gf_sk_set_block_mode(sess->sock, 1);
	gf_sk_set_buffer_size(sess->sock, 0, GF_DOWNLOAD_BUFFER_SIZE);
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
				sess->status = GF_NETIO_STATE_ERROR;
				sess->last_error = GF_AUTHENTICATION_FAILURE;
				gf_dm_sess_notify_state(sess, sess->status, sess->last_error);
			}
		}
	}
#endif
}

const char *gf_dm_sess_mime_type(GF_DownloadSession *sess)
{
	Bool go;
	u32 flags = sess->flags;
	sess->flags |= GF_NETIO_SESSION_NOT_CACHED;

	go = 1;
	while (go) {
		switch (sess->status) {
		/*setup download*/
		case GF_NETIO_SETUP:
			gf_dm_connect(sess);
			if (sess->status == GF_NETIO_SETUP)
				gf_sleep(200);
			break;
		case GF_NETIO_WAIT_FOR_REPLY:
			gf_sleep(20);
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
	return sess->mime_type;
}




GF_DownloadManager *gf_dm_new(GF_Config *cfg)
{
	const char *opt;
	GF_DownloadManager *dm;
	if (!cfg) return NULL;
	GF_SAFEALLOC(dm, GF_DownloadManager);
	dm->sessions = gf_list_new();
	dm->cfg = cfg;

	opt = gf_cfg_get_key(cfg, "General", "CacheDirectory");
	if (opt) {
		if (opt[strlen(opt)-1] != GF_PATH_SEPARATOR) {
			dm->cache_directory = (char *) malloc(sizeof(char)* (strlen(opt)+2));
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
	/*destroy all pending sessions*/
	while (gf_list_count(dm->sessions)) {
		GF_DownloadSession *sess = (GF_DownloadSession *) gf_list_get(dm->sessions, 0);
		gf_dm_sess_del(sess);
	}
	gf_list_del(dm->sessions);

	free(dm->cache_directory);

#ifdef GPAC_HAS_SSL
	if (dm->ssl_ctx) SSL_CTX_free(dm->ssl_ctx);
#endif

	free(dm);
}


static GFINLINE void gf_dm_data_received(GF_DownloadSession *sess, char *data, u32 nbBytes)
{
	GF_NETIO_Parameter par;
	u32 runtime, rcv;
	
	rcv = nbBytes;

	if (! (sess->flags & GF_NETIO_SESSION_NOT_CACHED)) {
		if (sess->cache) {
			fwrite(data, nbBytes, 1, sess->cache);
			fflush(sess->cache);
		}
		sess->bytes_done += nbBytes;
		/*if not threaded don't signal data to user*/
		if (sess->th) {
			par.msg_type = GF_NETIO_DATA_EXCHANGE;
			par.error = GF_OK;
			par.data = NULL;
			par.size = nbBytes;
			gf_dm_sess_user_io(sess, &par);
		}
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
						GF_NETIO_Parameter par;
						char szData[4096];
						memcpy(szData, data+1, sess->icy_count-1);
						szData[sess->icy_count] = 0;
	
						par.error = 0;
						par.msg_type = GF_NETIO_PARSE_HEADER;
						par.name = "icy-meta";
						par.value = szData;
						gf_dm_sess_user_io(sess, &par);
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

				par.msg_type = GF_NETIO_DATA_EXCHANGE;
				par.error = GF_OK;
				par.data = data;
				par.size = left;
				gf_dm_sess_user_io(sess, &par);

				data += left;
			}
		}
	} else {
		sess->bytes_done += nbBytes;
		/*if not threaded don't signal data to user*/
		if (sess->th) {
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
//	if (sess->cache || !buffer || !buffer_size) return GF_BAD_PARAM;
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
			free(sess->init_data);
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
	return sess->cache_name;
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

void http_do_requests(GF_DownloadSession *sess)
{
	GF_Err e;
	Bool is_ice;
	GF_NETIO_Parameter par;
	char sHTTP[GF_DOWNLOAD_BUFFER_SIZE];
	char buf[1024];
	char comp[400];
	char *new_location;
	char *hdr, *hdr_val;
	u32 bytesRead, res;
	s32 LinePos, Pos;
	u32 rsp_code, ContentLength, first_byte, last_byte, total_size, range, no_range;
	s32 BodyStart;

	/*sent HTTP request*/
	if (sess->status==GF_NETIO_CONNECTED) {
		char range_buf[1024];
		char pass_buf[1024];
		const char *user_agent;
		const char *user_profile;
		const char *param_string;
		u32 size;
		Bool has_accept, has_connection, has_range, has_agent, send_profile;

		/*setup authentification*/
		strcpy(pass_buf, "");
		if (sess->user) {
			if (!sess->passwd) {
				char szUSR[50], szPASS[50];
				strcpy(szUSR, sess->user);
				strcpy(szPASS, "");
				/*failed getting pass*/
				if (!sess->dm->GetUserPassword || !sess->dm->GetUserPassword(sess->dm->usr_cbk, sess->server_name, szUSR, szPASS)) {
					sess->status = GF_NETIO_STATE_ERROR;
					return;
				}
				sess->passwd = strdup(szPASS);
			}
			sprintf(pass_buf, "%s:%s", sess->user, sess->passwd);
			size = gf_base64_encode(pass_buf, strlen(pass_buf), range_buf, 1024);
			range_buf[size] = 0;
			sprintf(pass_buf, "Authorization: Basic %s", range_buf);
		}


		/*MIX2005 KMS project*/
#if 0
		if (strstr(sess->remote_path, "getKey.php?")) {
			char *sLogin, *sPass;
			sLogin = gf_cfg_get_key(sess->dm->cfg, "General", "KMS_User");
			sPass = gf_cfg_get_key(sess->dm->cfg, "General", "KMS_Password");
			if (!sLogin) sLogin = "mix";
			if (!sPass) sPass = "mix";
			sprintf(https_get_buffer, "%s&login=%s&password=%s", sess->remote_path, sLogin, sPass);
		}
#endif	

		user_agent = gf_cfg_get_key(sess->dm->cfg, "Downloader", "UserAgent");
		if (!user_agent) user_agent = GF_DOWNLOAD_AGENT_NAME;

		
		par.error = 0;
		par.msg_type = GF_NETIO_GET_METHOD;
		par.name = NULL;
		gf_dm_sess_user_io(sess, &par);

		if (par.name) {
			if (!strcmp(par.name, "GET")) sess->http_read_type = 0;
			else if (!strcmp(par.name, "HEAD")) sess->http_read_type = 1;
			else sess->http_read_type = 2;
		} else {
			sess->http_read_type = 0;
		}

		param_string = gf_cfg_get_key(sess->dm->cfg, "Downloader", "ParamString");
		if (param_string) {
			if (strchr(sess->remote_path, '?')) {
				sprintf(sHTTP, "%s %s&%s HTTP/1.0\r\nHost: %s\r\n" ,
					par.name ? par.name : "GET", sess->remote_path, param_string, sess->server_name);
			} else {
				sprintf(sHTTP, "%s %s?%s HTTP/1.0\r\nHost: %s\r\n" ,
					par.name ? par.name : "GET", sess->remote_path, param_string, sess->server_name);
			}
		} else {
			sprintf(sHTTP, "%s %s HTTP/1.0\r\nHost: %s\r\n" ,
				par.name ? par.name : "GET", sess->remote_path, sess->server_name);
		}

		/*signal we support title streaming*/
		if (!strcmp(sess->remote_path, "/")) strcat(sHTTP, "icy-metadata:1\r\n");

		/*get all headers*/
		has_agent = has_accept = has_connection = has_range = 0;
		while (1) {
			par.msg_type = GF_NETIO_GET_HEADER;
			par.name = NULL;
			par.value = NULL;
			gf_dm_sess_user_io(sess, &par);
			if (!par.name) break;
			strcat(sHTTP, par.name);
			strcat(sHTTP, ": ");
			strcat(sHTTP, par.value);
			strcat(sHTTP, "\r\n");
			if (!strcmp(par.name, "Accept")) has_accept = 1;
			else if (!strcmp(par.name, "Connection")) has_connection = 1;
			else if (!strcmp(par.name, "Range")) has_range = 1;
			else if (!strcmp(par.name, "User-Agent")) has_agent = 1;
		}
		if (!has_agent) {
			strcat(sHTTP, "User-Agent: ");
			strcat(sHTTP, user_agent);
			strcat(sHTTP, "\r\n");
		}
		if (!has_accept) strcat(sHTTP, "Accept: */*\r\n");
		if (!has_connection) strcat(sHTTP, "Connection: Keep-Alive\r\n");
		if (!has_range && sess->cache_start_size) {
			sprintf(range_buf, "Range: bytes=%d-\r\n", sess->cache_start_size);
			strcat(sHTTP, range_buf);
		}
		if (strlen(pass_buf)) {
			strcat(sHTTP, pass_buf);
			strcat(sHTTP, "\r\n");
		}
		if (sess->flags & GF_DOWNLOAD_IS_ICY) strcat(sHTTP, "Icy-Metadata: 1\r\n");

		par.msg_type = GF_NETIO_GET_CONTENT;
		par.data = NULL;
		par.size = 0;

		/*check if we have personalization info*/
		send_profile = 0;
		user_profile = gf_cfg_get_key(sess->dm->cfg, "Downloader", "UserProfileID");
		if (user_profile) {
			strcat(sHTTP, "X-UserProfileID: ");
			strcat(sHTTP, user_profile);
			strcat(sHTTP, "\r\n");
		} else {
			user_profile = gf_cfg_get_key(sess->dm->cfg, "Downloader", "UserProfile");
			if (user_profile) {
				FILE *profile = fopen(user_profile, "rt");
				if (profile) {
					fseek(profile, 0, SEEK_END);
					par.size = ftell(profile);
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
		strcat(sHTTP, "\r\n");

		if (send_profile || par.data) {
			u32 len = strlen(sHTTP);
			char *tmp_buf = malloc(sizeof(char)*(len+par.size));
			strcpy(tmp_buf, sHTTP);
			if (par.data) {
				memcpy(tmp_buf+len, par.data, par.size);
			} else {
				FILE *profile;
				user_profile = gf_cfg_get_key(sess->dm->cfg, "Downloader", "UserProfile");
				assert (user_profile);
				profile = fopen(user_profile, "rt");
				fread(tmp_buf+len, 1, par.size, profile);
				fclose(profile);
			}

#ifdef GPAC_HAS_SSL
			if (sess->ssl) {
				e = GF_IP_NETWORK_FAILURE;
				if (!SSL_write(sess->ssl, tmp_buf, len+par.size)) e = GF_OK;
			} else 
#endif
				e = gf_sk_send(sess->sock, tmp_buf, len+par.size);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] %s\n\n", tmp_buf));
			free(tmp_buf);
		} else {

#ifdef GPAC_HAS_SSL
			if (sess->ssl) {
				e = GF_IP_NETWORK_FAILURE;
				if (!SSL_write(sess->ssl, sHTTP, strlen(sHTTP))) e = GF_OK;
			} else 
#endif
				e = gf_sk_send(sess->sock, sHTTP, strlen(sHTTP));

			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] %s\n\n", sHTTP));
		}

		if (e) {
			sess->status = GF_NETIO_STATE_ERROR;
			sess->last_error = e;
			gf_dm_sess_notify_state(sess, GF_NETIO_STATE_ERROR, e);
			return;
		}

		sess->status = GF_NETIO_WAIT_FOR_REPLY;
		gf_dm_sess_notify_state(sess, GF_NETIO_WAIT_FOR_REPLY, GF_OK);
		return;
	}

	/*process HTTP request*/
	if (sess->status == GF_NETIO_WAIT_FOR_REPLY) {
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
					sess->status = GF_NETIO_SETUP;
				else {
					sess->last_error = e;
					sess->status = GF_NETIO_STATE_ERROR;
				}
				return;
			case GF_OK:
				if (!res) return;
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] %s\n\n", sHTTP));

		LinePos = gf_token_get_line(sHTTP, 0, bytesRead, buf, 1024);
		Pos = gf_token_get(buf, 0, " \t\r\n", comp, 400);

		if (sess->mime_type) free(sess->mime_type);
		sess->mime_type = NULL;

		is_ice = 0;
		if (!strncmp("ICY", comp, 4)) {
			is_ice = 1;
			/*be prepared not to receive any mime type from ShoutCast servers*/
			sess->mime_type = strdup("audio/mpeg");
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
		//parse header
		while (1) {
			char *sep, *hdr_sep;
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
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Processing header %s: %s\n", hdr, hdr_val));
			gf_dm_sess_user_io(sess, &par);

			if (!stricmp(hdr, "Content-Length") ) ContentLength = (u32) atoi(hdr_val);
			else if (!stricmp(hdr, "Content-Type")) {			
				if (sess->mime_type) free(sess->mime_type);
				sess->mime_type = strdup(hdr_val);
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
			else if (!stricmp(hdr, "Content-Range")) {			
				range = 1;
				if (!strncmp(hdr_val, "bytes", 5)) {
					hdr_val += 5;
					if (hdr_val[0] == ':') hdr_val += 1;
					hdr_val += http_skip_space(hdr_val);
					if (hdr_val[0] == '*') {
						sscanf(hdr_val, "*/%d", &total_size);
					} else {
						sscanf(hdr_val, "%d-%d/%d", &first_byte, &last_byte, &total_size);
					}
				}
			}
			else if (!stricmp(hdr, "Accept-Ranges")) {
				if (strstr(hdr_val, "none")) no_range = 1;
			}
			else if (!stricmp(hdr, "Location")) 
				new_location = strdup(hdr_val);
			else if (!stricmp(hdr, "icy-metaint")) 
				sess->icy_metaint = atoi(hdr_val);
			else if (!stricmp(hdr, "ice") || !stricmp(hdr, "icy") ) 
				is_ice = 1;
			else if (!stricmp(hdr, "X-UserProfileID") ) 
				gf_cfg_set_key(sess->dm->cfg, "Downloader", "UserProfileID", hdr_val);
/*			else if (!stricmp(hdr, "Connection") ) 
				if (strstr(hdr_val, "close")) sess->http_read_type = 1; */


			if (sep) sep[0]=':';
			if (hdr_sep) hdr_sep[0] = '\r';
		}
		if (no_range) first_byte = 0;

		if (sess->cache_start_size) {
			if (total_size && (sess->cache_start_size >= total_size) ) {
				rsp_code = 200;
				ContentLength = total_size;
			}
			if (ContentLength && (sess->cache_start_size == ContentLength) ) rsp_code = 200;
		}	

		par.msg_type = GF_NETIO_PARSE_REPLY;
		par.error = GF_OK;
		par.reply = rsp_code;
		par.value = comp;

		switch (rsp_code) {
		case 200:
		case 201:
		case 202:
		case 206:
			gf_dm_sess_user_io(sess, &par);
			e = GF_OK;
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
				return;
			}
			return;
		case 404:
		case 416:
			/*try without cache (some servers screw up when content-length is specified)*/
			if (sess->cache_start_size) {
				gf_dm_disconnect(sess);
				sess->status = GF_NETIO_SETUP;
				return;
			} else if (is_ice && !(sess->flags & GF_DOWNLOAD_IS_ICY)) {
				gf_dm_disconnect(sess);
				sess->status = GF_NETIO_SETUP;
				sess->flags |= GF_DOWNLOAD_IS_ICY;
				return;
			}
			gf_dm_sess_user_io(sess, &par);
			e = GF_URL_ERROR;
			goto exit;
		case 503:
		default:
			gf_dm_sess_user_io(sess, &par);
			e = GF_REMOTE_SERVICE_ERROR;
			goto exit;
		}

		/*head*/
		if (sess->http_read_type==1) {
			gf_dm_disconnect(sess);
			gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
			sess->status = GF_NETIO_DISCONNECTED;
			sess->http_read_type = 0;
			return;
		}

		if (!ContentLength && sess->mime_type && strstr(sess->mime_type, "ogg")) is_ice = 1;

		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP] Connected to %s - error %s\n", sess->server_name, gf_error_to_string(e) ));

		/*some servers may reply without content length, but we MUST have it*/
//		if (!is_ice && !ContentLength) e = GF_REMOTE_SERVICE_ERROR;
		if (e) goto exit;

		/*force disabling cache (no content length)*/
		if (is_ice) {
			sess->flags |= GF_NETIO_SESSION_NOT_CACHED;
			if (sess->mime_type && !stricmp(sess->mime_type, "video/nsv")) {
				free(sess->mime_type);
				sess->mime_type = strdup("audio/aac");
			}
		}

		/*done*/
		if (sess->cache_start_size 
			&& ( (total_size && sess->cache_start_size >= total_size) || (sess->cache_start_size == ContentLength)) ) {
			sess->total_size = sess->bytes_done = sess->cache_start_size;
			/*disconnect*/
			gf_dm_disconnect(sess);
			BodyStart = bytesRead;
			gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
		}
		else if (sess->flags & GF_DOWNLOAD_IS_ICY) {
			sess->icy_bytes = 0;
			sess->status = GF_NETIO_DATA_EXCHANGE;
		}
		/*we don't expect anything*/
		else if (!ContentLength && sess->http_read_type) {
			gf_dm_disconnect(sess);
			gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
			sess->status = GF_NETIO_DISCONNECTED;
			sess->http_read_type = 0;
		}
		/*no range header, Accept-Ranges deny or dumb server : restart*/
		else if (!range || !first_byte || (first_byte != sess->cache_start_size) ) {
			sess->cache_start_size = sess->bytes_done = 0;
			sess->total_size = ContentLength;
			if (! (sess->flags & GF_NETIO_SESSION_NOT_CACHED) ) {
				sess->cache = fopen(sess->cache_name, "wb");
				if (!sess->cache) {
					e = GF_IO_ERR;
					goto exit;
				}
			}
			sess->status = GF_NETIO_DATA_EXCHANGE;
		}
		/*resume*/
		else {
			sess->total_size = ContentLength + sess->cache_start_size;
			if (! (sess->flags & GF_NETIO_SESSION_NOT_CACHED) ) {
				sess->cache = fopen(sess->cache_name, "ab");
				if (!sess->cache) {
					e = GF_IO_ERR;
					goto exit;
				}
			}
			sess->status = GF_NETIO_DATA_EXCHANGE;
			sess->bytes_done = sess->cache_start_size;
		}

		sess->window_start = sess->start_time = gf_sys_clock();
		sess->bytes_in_wnd = 0;


		//we may have existing data in this buffer ...
		if (!e && (BodyStart < (s32) bytesRead)) {
			gf_dm_data_received(sess, sHTTP + BodyStart, bytesRead - BodyStart);
			/*store data if no callbacks or cache*/
//			if (sess->flags & GF_NETIO_SESSION_NOT_CACHED) {
				if (sess->init_data) free(sess->init_data);
				sess->init_data_size = bytesRead - BodyStart;
				sess->init_data = (char *) malloc(sizeof(char) * sess->init_data_size);
				memcpy(sess->init_data, sHTTP+BodyStart, sess->init_data_size);
//			}
		}
exit:
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] Error parsing reply: %s\n", gf_error_to_string(e) ));
			gf_dm_disconnect(sess);
			sess->status = GF_NETIO_STATE_ERROR;
			sess->last_error = e;
			gf_dm_sess_notify_state(sess, sess->status, e);
		}
		return;
	}
	/*fetch data*/
	while (1) {
		u32 size;
#if 1
		if (sess->limit_data_rate && sess->bytes_per_sec) {
			if (sess->bytes_per_sec>sess->limit_data_rate) {
				/*update state*/
				u32 runtime = gf_sys_clock() - sess->window_start;
				sess->bytes_per_sec = (1000 * (sess->bytes_in_wnd)) / runtime;
				if (sess->bytes_per_sec > sess->limit_data_rate) return;
			}
		}
#endif
		e = gf_dm_read_data(sess, sHTTP, GF_DOWNLOAD_BUFFER_SIZE, &size);
		if (!size || e == GF_IP_NETWORK_EMPTY) {	
			if (!sess->total_size && (gf_sys_clock() - sess->window_start > 1000)) {
				sess->total_size = sess->bytes_done;
				gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
			}
			return;
		}

		if (e) {
			gf_dm_disconnect(sess);
			sess->last_error = e;
			gf_dm_sess_notify_state(sess, sess->status, e);
			return;
		}
		gf_dm_data_received(sess, sHTTP, size);
		/*socket empty*/
		if (size < GF_DOWNLOAD_BUFFER_SIZE) return;
	}
}

