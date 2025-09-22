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

#include "downloader.h"

#ifndef GPAC_DISABLE_NETWORK

static void gf_dm_connect(GF_DownloadSession *sess);

void dm_sess_sk_del(GF_DownloadSession *sess)
{
#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd) {
		curl_destroy(sess);
		return;
	}
#endif
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

static void sess_connection_closed(GF_DownloadSession *sess)
{
#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		sess->hmux_sess->do_shutdown = GF_TRUE;
		sess->hmux_switch_sess = GF_TRUE;
	}
#endif
}

GF_Err dm_sess_write(GF_DownloadSession *session, const u8 *buffer, u32 size)
{
	GF_Err e;
	u32 written=0;
	GF_DownloadSession *par_sess = session;
#ifdef GPAC_HTTPMUX
	//if h2 we aggregate pending frames on the session currently holding the HTTP2 session
	if (session->hmux_sess) par_sess = session->hmux_sess->net_sess;
#endif

	//we are writing and we already have pending data, append and try to flush after
	if (par_sess->async_buf_size && (buffer != par_sess->async_buf)) {
		e = GF_IP_NETWORK_EMPTY;
	} else
#ifdef GPAC_HAS_SSL
	if (session->ssl) {
		e = gf_ssl_write(session, buffer, size, &written);
		if (e==GF_IP_NETWORK_FAILURE)
			e = GF_IP_CONNECTION_CLOSED;
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
			if (par_sess->async_buf_size >= written) {
				memmove(par_sess->async_buf, par_sess->async_buf + written, remain);
				par_sess->async_buf_size -= written;
			} else {
				gf_assert(0);
				par_sess->async_buf_size = 0;
			}
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

Bool gf_dm_can_handle_url(const char *url)
{
	if (!strnicmp(url, "http://", 7)) return GF_TRUE;
#ifdef GPAC_HAS_SSL
	if (!strnicmp(url, "https://", 8)) return GF_TRUE;
#endif

#ifdef GPAC_HAS_CURL
	if (curl_can_handle_url(url)) return GF_TRUE;
#endif
	return GF_FALSE;
}


void gf_dm_sess_set_header_ex(GF_DownloadSession *sess, const char *name, const char *value, Bool allow_overwrite)
{
	GF_HTTPHeader *hdr;
	if (!sess) return;

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess
#ifdef GPAC_HAS_HTTP2
		|| sess->h2_upgrade_settings
#endif
	) {
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
	e = http2_check_upgrade(sess);
	if (e) return e;
#endif

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		return sess->hmux_sess->send_reply(sess, reply_code, response_body, body_len, no_body);
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
	case 101: gf_dynstrcat(&rsp_buf, "Switching Protocols", NULL); break;
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

	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] send reply for %s:\n%s\n", sess->log_name, sess->orig_url, rsp_buf));

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


void gf_dm_disconnect(GF_DownloadSession *sess, HTTPCloseType close_type)
{
	gf_assert( sess );
	if (sess->connection_close) close_type = HTTP_RESET_CONN;
	sess->connection_close = GF_FALSE;
	sess->remaining_data_size = 0;
	if (sess->async_req_reply) gf_free(sess->async_req_reply);
	sess->async_req_reply = NULL;
	sess->async_req_reply_size = 0;
	sess->async_buf_size = 0;
	if (sess->cached_file) {
		gf_fclose(sess->cached_file);
		sess->cached_file = NULL;
	}

	if ((sess->status >= GF_NETIO_DISCONNECTED)
#ifdef GPAC_HAS_CURL
		&& !sess->curl_hnd
#endif
	) {
		if (close_type && sess->use_cache_file && sess->cache_entry) {
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_FALSE);
		}
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Downloader] gf_dm_disconnect(%p)\n", sess ));

	gf_mx_p(sess->mx);

	if (!sess->server_mode) {
		Bool do_close = (close_type || !(sess->flags & GF_NETIO_SESSION_PERSISTENT)) ? GF_TRUE : GF_FALSE;
#ifdef GPAC_HTTPMUX
		if (sess->hmux_sess) {
			do_close = (close_type==HTTP_RESET_CONN) ? GF_TRUE : GF_FALSE;
		}
		//if H2 stream is still valid, issue a reset stream
		if (sess->hmux_sess && (sess->hmux_stream_id>=0) && (sess->put_state!=2))
			sess->hmux_sess->stream_reset(sess, GF_FALSE);
#endif

#ifdef GPAC_HAS_CURL
		//always remove curl session, let multi-connection manager handle disconnect/reconnect
		if (sess->curl_hnd) {
			sess->local_buf_len = 0;
			dm_sess_sk_del(sess);
		} else
#endif

		if (do_close) {
#ifdef GPAC_HTTPMUX
			if (sess->hmux_sess) {
				sess->hmux_sess->do_shutdown = GF_TRUE;
				hmux_detach_session(sess->hmux_sess, sess);
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
	if (sess->th || sess->ftask) {
		sess->destroy = GF_TRUE;
		if (sess->ftask->in_task)
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
		gf_list_del_item(sess->dm->all_sessions, sess);
		gf_mx_v(sess->dm->cache_mx);
	}

	gf_cache_remove_entry_from_session(sess);
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

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		sess->hmux_sess->setup_session(sess, GF_TRUE);
		gf_mx_p(sess->mx);
		hmux_detach_session(sess->hmux_sess, sess);
		gf_mx_v(sess->mx);
	}
#endif

#ifdef GPAC_HAS_HTTP2
	if (sess->h2_upgrade_settings)
		gf_free(sess->h2_upgrade_settings);
#endif

#if defined(GPAC_HTTPMUX) || defined(GPAC_HAS_CURL)
	if (sess->local_buf)
		gf_free(sess->local_buf);
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

#ifdef GPAC_HAS_CURL
	gf_assert(!sess->curl_hnd);
	if (sess->curl_hdrs) curl_slist_free_all(sess->curl_hdrs);
#endif

#ifndef GPAC_DISABLE_LOG
	if (sess->log_name) gf_free(sess->log_name);
#endif
	assert(!sess->ftask || !sess->ftask->in_task || !sess->mx);
	gf_mx_del(sess->mx);

	if (sess->http_buf) gf_free(sess->http_buf);

	if (sess->ftask) {
		sess->ftask->sess = NULL;
		sess->ftask = NULL;
	}

	gf_free(sess);
}

void gf_dm_sess_notify_state(GF_DownloadSession *sess, GF_NetIOStatus dnload_status, GF_Err error)
{
	if (!sess->user_proc) return;
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

static void gf_dm_sess_user_io(GF_DownloadSession *sess, GF_NETIO_Parameter *par)
{
	if (sess->user_proc) {
		sess->in_callback = GF_TRUE;
		par->sess = sess;
		sess->user_proc(sess->usr_cbk, par);
		sess->in_callback = GF_FALSE;
	}
}


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
	if (info->protocol)
		gf_free(info->protocol);
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
static s32 gf_dm_parse_protocol(const char * url, GF_URL_Info * info)
{
	gf_assert(info);
	gf_assert(url);
	if (!strnicmp(url, "http://", 7)) {
		info->port = 80;
		info->protocol = gf_strdup("http");
		return 7;
	}
	else if (!strnicmp(url, "https://", 8)) {
		info->port = 443;
#ifndef GPAC_HAS_SSL
		return -1;
#endif
		info->protocol = gf_strdup("https");
		return 8;
	}
	char *sep = strstr(url, "://");
	if (sep) {
		sep[0] = 0;
		info->protocol = gf_strdup(url);
		sep[0] = ':';
		return 3 + (u32) (sep-url);
	}
	return -1;
}

GF_EXPORT
GF_Err gf_dm_get_url_info(const char * url, GF_URL_Info * info, const char * baseURL) {
	char *tmp, *tmp_url, *current_pos, *urlConcatenateWithBaseURL, *ipv6;
	char * copyOfUrl;
	s32 proto_offset;
	u32 default_port;
	gf_dm_url_info_del(info);
	urlConcatenateWithBaseURL = NULL;
	proto_offset = gf_dm_parse_protocol(url, info);
	default_port = info->port;

	if (proto_offset > 0) {
		url += proto_offset;
	} else {
		/*relative URL*/
		if (!strstr(url, "://")) {
			u32 i;
			gf_dm_url_info_del(info);
			gf_dm_url_info_init(info);
			if (baseURL) {
				urlConcatenateWithBaseURL = gf_url_concatenate(baseURL, url);
				/*relative file path*/
				if (!strstr(baseURL, "://")) {
					info->protocol = gf_strdup("file");
					info->canonicalRepresentation = urlConcatenateWithBaseURL;
					return GF_OK;
				}
				proto_offset = gf_dm_parse_protocol(urlConcatenateWithBaseURL, info);
			} else {
				proto_offset = -1;
			}

			if (proto_offset < 0) {
				tmp = urlConcatenateWithBaseURL;
				gf_assert( ! info->remotePath );
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

				info->canonicalRepresentation = NULL;
				gf_dynstrcat(&info->canonicalRepresentation, info->protocol, NULL);
				gf_dynstrcat(&info->canonicalRepresentation, "://", NULL);
				gf_dynstrcat(&info->canonicalRepresentation, info->remotePath, NULL);

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
	gf_assert( proto_offset >= 0 );
	tmp = strchr(url, '/');
	//patch for some broken url http://servername?cgi
	if (!tmp)
		tmp = strchr(url, '?');
	gf_assert( !info->remotePath );
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
		gf_assert( ! info->server_name );
		info->server_name = gf_strdup(current_pos);
		tmp[0] = 0;
		tmp = strchr(tmp_url, ':');

		if (tmp) {
			tmp[0] = 0;
			info->password = gf_strdup(tmp+1);
		}
		info->userName = gf_strdup(tmp_url);
	} else {
		gf_assert( ! info->server_name );
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

	/* We dont't want orig_url to contain user/passwords for security reasons or mismatch in cache hit */
	info->canonicalRepresentation = NULL;
	gf_dynstrcat(&info->canonicalRepresentation, info->protocol, NULL);
	gf_dynstrcat(&info->canonicalRepresentation, "://", NULL);
	gf_dynstrcat(&info->canonicalRepresentation, info->server_name, NULL);
	if (info->port && (info->port!=default_port)) {
		char port[8];
		snprintf(port, sizeof(port)-1, ":%d", info->port);
		gf_dynstrcat(&info->canonicalRepresentation, port, NULL);
	}
	gf_dynstrcat(&info->canonicalRepresentation, info->remotePath, NULL);

	gf_free(copyOfUrl);
	if (urlConcatenateWithBaseURL)
		gf_free(urlConcatenateWithBaseURL);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dm_sess_setup_from_url(GF_DownloadSession *sess, const char *url, Bool allow_direct_reuse)
{
	Bool socket_changed = GF_FALSE;
	GF_URL_Info info;
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
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Timeout reached (%d ms since last vs %d ms timeout), reconnecting\n", sess->log_name, diff, sess->connection_timeout_ms));
			socket_changed = GF_TRUE;
		}
	}

	gf_fatal_assert(sess->status != GF_NETIO_WAIT_FOR_REPLY);
	gf_fatal_assert(sess->status != GF_NETIO_DATA_EXCHANGE);
	//always detach task if any
	if (sess->ftask) {
		sess->ftask->sess	 = NULL;
		sess->ftask = NULL;
	}

	//strip fragment
	sep_frag = strchr(url, '#');
	if (sep_frag) sep_frag[0]=0;
	sess->last_error = gf_dm_get_url_info(url, &info, sess->orig_url);
	if (sess->last_error) {
		if (sep_frag) sep_frag[0]='#';
		return sess->last_error;
	}

	if (!strstr(url, "://")) {
		char *sep;
		gf_dm_url_info_del(&info);
		gf_dm_url_info_init(&info);
		info.port = sess->port;
		info.server_name = sess->server_name ? gf_strdup(sess->server_name) : NULL;
		info.remotePath = gf_strdup(url);
		sep = strstr(sess->orig_url_before_redirect, "://");
		gf_assert(sep);
		sep[0] = 0;
		info.protocol = gf_strdup(sess->orig_url_before_redirect);
		sep[0] = ':';
	}

	if (sess->port != info.port) {
		socket_changed = GF_TRUE;
		sess->port = info.port;
	}
#ifdef GPAC_HTTPMUX
	//safety in case we had a previous error but underlying stream_id was not reset
	sess->hmux_stream_id = -1;
#endif

	if (sess->cached_file) {
		socket_changed = GF_TRUE;
		gf_fclose(sess->cached_file);
		sess->cached_file = NULL;
		if (sess->cache_entry) {
			gf_cache_remove_entry_from_session(sess);
			sess->cache_entry = NULL;
		}
	}
	if (sess->log_name) {
		gf_free(sess->log_name);
		sess->log_name = NULL;
	}
	if (!strcmp("http", info.protocol) || !strcmp("https", info.protocol)) {
		sess->log_name = gf_strdup("HTTP");
		if (sess->do_requests != http_do_requests) {
			sess->do_requests = http_do_requests;
			socket_changed = GF_TRUE;
		}
		Bool use_ssl = !strcmp("https", info.protocol) ? GF_TRUE : GF_FALSE;
		//if proxy, check scheme and port
		const char *proxy = (sess->flags & GF_NETIO_SESSION_NO_PROXY) ? NULL : gf_opts_get_key("core", "proxy");
		if (proxy) {
			if (!strnicmp(proxy, "http://", 7)) use_ssl = GF_FALSE;
			else if (!strnicmp(proxy, "https://", 8)) use_ssl = GF_TRUE;
			else if (strstr(proxy, ":80")) use_ssl = GF_FALSE;
			else if (strstr(proxy, ":443")) use_ssl = GF_TRUE;
		}

		if (use_ssl) {
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

#ifdef GPAC_HAS_CURL
		if (!sess->dm->curl_multi) {
			sess->dm->curl_multi = curl_multi_init();
		}
#endif

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
	} else {
		if (sess->orig_url) gf_free(sess->orig_url);
		sess->orig_url = gf_strdup(info.protocol);
		gf_dynstrcat(&sess->orig_url, info.server_name, "://");
		if (info.port) {
			char szTmp[10];
			sprintf(szTmp, ":%u", info.port);
			gf_dynstrcat(&sess->orig_url, szTmp, NULL);
		}
		gf_dynstrcat(&sess->orig_url, info.remotePath, NULL);
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] Did not found any download manager, credentials not supported\n", sess->log_name));
			} else
				sess->creds = gf_user_credentials_register(sess->dm, !strcmp("https", info.protocol), sess->server_name, info.userName, info.password, info.userName && info.password);
		}
	}
	gf_dm_url_info_del(&info);
	if (sep_frag) sep_frag[0]='#';

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		if (sess->hmux_sess->do_shutdown)
			socket_changed = GF_TRUE;
		sess->hmux_buf.size = 0;
		sess->hmux_buf.offset = 0;
	}
#endif


#ifdef GPAC_HAS_CURL
	if (sess->dm->curl_multi && (!sess->do_requests || gf_opts_get_bool("core", "curl"))
		//if the caller wants a GF_Socket interface, we cannot use curl
		&& !(sess->flags & GF_NETIO_SESSION_SHARE_SOCKET)
	) {
		GF_Err e = curl_setup_session(sess);
		if (e) return e;
	} else
#endif

	if (sess->sock && !socket_changed) {
		if (!sess->do_requests) return GF_NOT_SUPPORTED;
		sess->status = GF_NETIO_CONNECTED;
		sess->last_error = GF_OK;
		//reset number of retry and start time
		sess->num_retry = SESSION_RETRY_COUNT;
		sess->start_time = 0;
		sess->needs_cache_reconfig = 1;
	} else {

		if (!sess->do_requests) return GF_NOT_SUPPORTED;
#ifdef GPAC_HTTPMUX
		if (sess->hmux_sess) {
			gf_mx_p(sess->mx);
			hmux_detach_session(sess->hmux_sess, sess);
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
				if (!sess->cache_entry) {
					SET_LAST_ERR(GF_URL_ERROR)
				} else {
					SET_LAST_ERR(GF_OK)
				}
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
	if (!sess || sess->destroy) {
		gf_free(task);
		if (sess) sess->ftask = NULL;
		return GF_FALSE;
	}
	task->in_task = GF_TRUE;
	Bool ret = gf_dm_session_do_task(sess);
	task->in_task = GF_FALSE;
	if (ret) {
		if (sess->rate_regulated) {
			*reschedule_ms = (sess->last_cap_rate_bytes_per_sec > sess->max_data_rate) ? 1000 : 100;
		} else {
			*reschedule_ms = 1;
		};
		return GF_TRUE;
	}
	gf_assert(sess->ftask);
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
		gf_sleep(sess->rate_regulated ? 100 : 0);
	}
	sess->flags |= GF_DOWNLOAD_SESSION_THREAD_DEAD;
	if (sess->destroy)
		gf_dm_sess_del(sess);
	return 1;
}
#endif

GF_DownloadSession *gf_dm_sess_new_internal(GF_DownloadManager * dm, const char *url, u32 dl_flags,
        gf_dm_user_io user_io,
        void *usr_cbk,
        GF_Socket *server,
        Bool force_server,
        GF_Err *e)
{
	GF_DownloadSession *sess;

	GF_SAFEALLOC(sess, GF_DownloadSession);
	if (!sess) {
		return NULL;
	}
#ifdef GPAC_HTTPMUX
	sess->hmux_stream_id = -1;
#endif
	sess->headers = gf_list_new();
	sess->flags = dl_flags;
	if (sess->flags & GF_NETIO_SESSION_NOTIFY_DATA)
		sess->force_data_write_callback = GF_TRUE;
	sess->user_proc = user_io;
	sess->usr_cbk = usr_cbk;
	sess->creds = NULL;
	sess->log_name = gf_strdup("HTTP");
	sess->http_buf_size = dm ? dm->read_buf_size : GF_DOWNLOAD_BUFFER_SIZE;
	sess->http_buf = gf_malloc(sess->http_buf_size + 1);

	sess->conn_timeout = gf_opts_get_int("core", "tcp-timeout");
	if (!sess->conn_timeout) sess->conn_timeout = 5000;

	sess->request_timeout = gf_opts_get_int("core", "req-timeout");

	sess->chunk_wnd_dur = gf_opts_get_int("core", "cte-rate-wnd") * 1000;
	if (!sess->chunk_wnd_dur) sess->chunk_wnd_dur = 20000;

	sess->dm = dm;
	if (server || force_server) {
		sess->sock = server;
		sess->flags = GF_NETIO_SESSION_NOT_THREADED;
		sess->status = GF_NETIO_CONNECTED;
		sess->server_mode = GF_TRUE;
		sess->do_requests = http_do_requests;
		if (e) *e = GF_OK;
		if (dl_flags & GF_NETIO_SESSION_NO_BLOCK) {
			sess->flags |= GF_NETIO_SESSION_NO_BLOCK;
			if (server)
				gf_sk_set_block_mode(server, GF_TRUE);
		}
		return sess;
	}

	if (!sess->conn_timeout) sess->server_only_understand_get = GF_TRUE;
	if (dm)
		sess->disable_cache = dm->disable_cache;

	if (! (dl_flags & GF_NETIO_SESSION_NOT_THREADED)
#ifdef GPAC_HAS_CURL
		&& !sess->curl_hnd
#endif
	) {
		sess->mx = gf_mx_new(url);
		if (!sess->mx) {
			gf_free(sess);
			return NULL;
		}
	}

	if ((dm->h3_mode == H3_MODE_FIRST) || (dm->h3_mode == H3_MODE_ONLY))
		sess->flags |= GF_NETIO_SESSION_USE_QUIC;
	else if (dm->h3_mode == H3_MODE_AUTO)
		sess->flags |= GF_NETIO_SESSION_RETRY_QUIC;

	*e = gf_dm_sess_setup_from_url(sess, url, GF_FALSE);
	if (*e) {
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[Downloader] failed to create session for %s: %s\n", url, gf_error_to_string(*e)));
		gf_dm_sess_del(sess);
		return NULL;
	}
	gf_assert( sess );
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
	GF_DownloadSession *sess = gf_dm_sess_new_internal(dm, NULL, async ? GF_NETIO_SESSION_NO_BLOCK : 0, user_io, usr_cbk, server, GF_TRUE, e);
	if (!sess) return NULL;

#ifdef GPAC_HAS_SSL
	if (ssl_sock_ctx) {
		sess->ssl = ssl_sock_ctx;
		gf_dm_sess_server_setup_ssl(sess);
	}
#endif
	return sess;
}


u32 gf_dm_sess_subsession_count(GF_DownloadSession *sess)
{
#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess)
		return gf_list_count(sess->hmux_sess->sessions);
#endif
	return 1;
}


void gf_dm_sess_server_reset(GF_DownloadSession *sess)
{
	if (!sess->server_mode) return;
	//DO NOT clear headers if H2: the underlying h2 session could have been ended (stream_id=0) and then reassigned
	//when processsing another session, so the headers would be the ones of the new stream_id
	//for H2, the reset is done when reassigning sessions
#ifdef GPAC_HTTPMUX
	if (!sess->hmux_sess)
#endif
		gf_dm_sess_clear_headers(sess);

	sess->total_size = sess->bytes_done = 0;
	sess->async_buf_size = 0;
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
	return gf_dm_sess_new_internal(dm, url, dl_flags, user_io, usr_cbk, NULL, GF_FALSE, e);
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

	if (!gf_dm_can_handle_url(url)) {
		*e = GF_NOT_SUPPORTED;
		return NULL;
	}
	sess = gf_dm_sess_new_simple(dm, url, dl_flags, user_io, usr_cbk, e);
	if (sess && dm) {
		sess->dm = dm;
		gf_mx_p(dm->cache_mx);
		gf_list_add(dm->all_sessions, sess);
		gf_mx_v(dm->cache_mx);
	}
	return sess;
}


GF_Err gf_dm_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read)
{
	GF_Err e;

	if (sess->cached_file) {
		*out_read = (u32) gf_fread(data, data_size, sess->cached_file);
		if (! *out_read && gf_cache_is_done(sess->cache_entry)) {
			sess->total_size = gf_cache_get_content_length(sess->cache_entry);
			if (sess->total_size != sess->bytes_done) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(GF_CORRUPTED_DATA);
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] Broken cache for %s: %u bytes in cache but %u advertized\n", sess->log_name, sess->orig_url, sess->bytes_done, sess->total_size));
				return GF_CORRUPTED_DATA;
			}
		}
		return GF_OK;
	}

#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd) {
		return curl_read_data(sess, data, data_size, out_read);
	}
#endif

	if (sess->dm && sess->dm->simulate_no_connection) {
		if (sess->sock) {
			sess->status = GF_NETIO_DISCONNECTED;
		}
		return GF_IP_NETWORK_FAILURE;
	}

	if (!sess)
		return GF_BAD_PARAM;

	if (sess->server_mode && (sess->flags & GF_NETIO_SESSION_USE_QUIC))
		return GF_IP_NETWORK_EMPTY;

	gf_mx_p(sess->mx);
	if (!sess->sock) {
		sess->status = GF_NETIO_DISCONNECTED;
		gf_mx_v(sess->mx);
		return GF_IP_CONNECTION_CLOSED;
	}

	*out_read = 0;

#ifdef GPAC_HAS_SSL
	if (sess->ssl && !(sess->flags & GF_NETIO_SESSION_USE_QUIC)) {
		e = gf_ssl_read_data(sess, data, data_size, out_read);
		if (e==GF_NOT_READY) {
			gf_mx_v(sess->mx);
			return GF_IP_NETWORK_EMPTY;
		}
	} else
#endif
		e = gf_sk_receive(sess->sock, data, data_size, out_read);

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		GF_Err hme = sess->hmux_sess->data_received(sess, data, *out_read);
		if (hme) {
			gf_mx_v(sess->mx);
			*out_read = 0;
			return hme;
		}
	}
#endif //GPAC_HTTPMUX

	if (*out_read)
		sess->last_fetch_time = gf_sys_clock_high_res();

	gf_mx_v(sess->mx);
	return e;
}

static void gf_dm_connect(GF_DownloadSession *sess)
{
	GF_Err e;
	Bool register_sock;
	Bool allow_offline = sess->dm ? sess->dm->allow_offline_cache : GF_FALSE;
	u16 proxy_port = 0;
	char szProxy[GF_MAX_PATH];
	const char *proxy;

	//offline cache enabled, setup cache before connection
	if (!sess->connect_pending && allow_offline) {
		gf_dm_configure_cache(sess);
		if (sess->cached_file) return;
	}

#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd) {
		curl_connect(sess);
		return;
	}
#endif

#ifdef GPAC_HTTPMUX
	if (sess->hmux_switch_sess) {
		sess->hmux_switch_sess = 0;
		gf_mx_p(sess->mx);
		hmux_detach_session(sess->hmux_sess, sess);
		gf_mx_v(sess->mx);

		if (sess->num_retry) {
			SET_LAST_ERR(GF_OK)
			sess->num_retry--;
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] stream_id "LLD" (%s) refused by server, retrying and marking session as no longer available\n", sess->log_name, sess->hmux_stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));

			sess->hmux_stream_id = -1;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("%s] stream_id "LLD" (%s) refused by server after all retries, marking session as no longer available\n", sess->log_name, sess->hmux_stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(GF_REMOTE_SERVICE_ERROR)
			sess->hmux_stream_id = -1;
			return;
		}
	}
	if (sess->hmux_sess && sess->hmux_sess->connected) {
		sess->connect_time = 0;
		sess->status = GF_NETIO_CONNECTED;
		sess->last_error = GF_OK;
		gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
		if (!allow_offline)
			gf_dm_configure_cache(sess);
		return;
	}
	Bool attach_to_parent_sess = GF_FALSE;
	if (sess->dm && !sess->sock) {
#ifdef GPAC_HAS_HTTP2
		if (!sess->dm->disable_http2 && (sess->h2_upgrade_state<3)) attach_to_parent_sess = GF_TRUE;
#endif
	}

	if (attach_to_parent_sess) {
		gf_mx_p(sess->dm->cache_mx);
		u32 i, count = gf_list_count(sess->dm->all_sessions);
		for (i=0; i<count; i++) {
			GF_DownloadSession *a_sess = gf_list_get(sess->dm->all_sessions, i);
			if (strcmp(a_sess->server_name, sess->server_name)) continue;
			if (!a_sess->hmux_sess) {
#ifdef GPAC_HAS_HTTP2
				//we already ahd a connection to this server with H2 failure, do not try h2
				if (a_sess->h2_upgrade_state==4) {
					sess->h2_upgrade_state=4;
					break;
				}
#endif
				//we have the same server name
				//trick in non-block mode, we want to wait for upgrade to be completed
				//before creating a new socket
				if ((a_sess != sess)
					&& a_sess->sock
					&& (a_sess->status <= GF_NETIO_WAIT_FOR_REPLY)
					&& (sess->flags & GF_NETIO_SESSION_NO_BLOCK)
#ifdef GPAC_HAS_HTTP2
					&& (a_sess->h2_upgrade_state<2)
					&& !sess->dm->disable_http2
					&& !gf_opts_get_bool("core", "no-h2c")
#endif
					//TODO: H3 test
				) {
					sess->connect_pending = 1;
					SET_LAST_ERR(GF_IP_NETWORK_EMPTY)
					gf_mx_v(sess->dm->cache_mx);
					return;
				}
				continue;
			}
			if (a_sess->hmux_sess->do_shutdown) continue;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] associating session %s to existing %s session\n", sess->log_name, sess->remote_path ? sess->remote_path : sess->orig_url, a_sess->log_name));

			u32 nb_locks=0;
			if (sess->mx) {
				nb_locks = gf_mx_get_num_locks(sess->mx);
				if (nb_locks) gf_mx_v(sess->mx);
				gf_mx_del(sess->mx);
			}
			sess->hmux_sess = a_sess->hmux_sess;
			sess->mx = a_sess->hmux_sess->mx;
			if (nb_locks)
				gf_mx_p(sess->mx);

			sess->sock = a_sess->sock;
#ifdef GPAC_HAS_SSL
			sess->ssl = a_sess->ssl;
#endif
			sess->proxy_enabled = a_sess->proxy_enabled;
			sess->hmux_sess->setup_session(sess, GF_FALSE);

			gf_list_add(sess->hmux_sess->sessions, sess);

			//reconfigure cache
			if (sess->allow_direct_reuse) {
				gf_dm_configure_cache(sess);
				if (sess->cached_file) {
					gf_mx_v(sess->dm->cache_mx);
					return;
				}
			}

			sess->connect_time = 0;
			if (sess->hmux_sess->connected) {
				sess->status = GF_NETIO_CONNECTED;
				gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
				gf_dm_configure_cache(sess);
			}
			gf_mx_v(sess->dm->cache_mx);
			return;
		}
		gf_mx_v(sess->dm->cache_mx);
	}
#endif

resetup_socket:

	register_sock = GF_FALSE;
	if (!sess->sock) {
		u32 sock_type = GF_SOCK_TYPE_TCP;
		if (sess->flags & GF_NETIO_SESSION_USE_QUIC)
			sock_type = GF_SOCK_TYPE_UDP;

		sess->sock = gf_sk_new_ex(sock_type, sess->netcap_id);

		if (sess->sock && (sess->flags & GF_NETIO_SESSION_NO_BLOCK))
			gf_sk_set_block_mode(sess->sock, GF_TRUE);

		if (sess->sock_group) register_sock = GF_TRUE;
	}

	/*connect*/
	sess->status = GF_NETIO_SETUP;
	gf_dm_sess_notify_state(sess, sess->status, GF_OK);

	/*PROXY setup*/
	if (sess->proxy_enabled!=2) {
		proxy = (sess->flags & GF_NETIO_SESSION_NO_PROXY) ? NULL : gf_opts_get_key("core", "proxy");
		if (proxy) {
			u32 i;
			proxy_port = 80;
			char *sep = strstr(proxy, "://");
			strcpy(szProxy, sep ? sep+3 : proxy);
			sep = strchr(szProxy, ':');
			if (sep) {
				proxy_port = atoi(sep+1);
				sep[0]=0;
			}
			proxy = szProxy;
			Bool use_proxy=GF_TRUE;
			for (i=0; i<gf_list_count(sess->dm->skip_proxy_servers); i++) {
				char *skip = (char*)gf_list_get(sess->dm->skip_proxy_servers, i);
				if (!strcmp(skip, sess->server_name)) {
					use_proxy=GF_FALSE;
					break;
				}
			}
			if (!use_proxy) {
				proxy = NULL;
			} else {
				sess->proxy_enabled = 1;
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
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Connecting to %s:%d for URL %s\n", sess->log_name, proxy, proxy_port, sess->remote_path ? sess->remote_path : "undefined"));
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

#ifdef GPAC_HAS_NGTCP2
		if (sess->flags & GF_NETIO_SESSION_USE_QUIC) {
			e = http3_connect(sess, (char *) proxy, proxy_port);
			if (sess->num_retry && (e==GF_IP_CONNECTION_FAILURE) ) {
				register_sock = GF_FALSE;
				e = GF_IP_NETWORK_EMPTY;
			}
		} else
#endif
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
			if ((sess->flags & GF_NETIO_SESSION_RETRY_QUIC) && !(sess->flags & GF_NETIO_SESSION_USE_QUIC)) {
				sess->status = GF_NETIO_SETUP;
				sess->connect_pending = 0;
				sess->start_time = 0;
				gf_sk_group_unregister(sess->sock_group, sess->sock);
				gf_sk_del(sess->sock);
				sess->sock = NULL;
				GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[%s] Failed to connect through TCP (%s), retrying with QUIC\n", sess->log_name, gf_error_to_string(e)));
				sess->flags |= GF_NETIO_SESSION_USE_QUIC;
				sess->flags &= ~GF_NETIO_SESSION_RETRY_QUIC;
				goto resetup_socket;
			}
			if ((sess->flags & GF_NETIO_SESSION_USE_QUIC)
				&& !(sess->flags & GF_NETIO_SESSION_RETRY_QUIC)
				&& (sess->dm->h3_mode!=H3_MODE_ONLY)
			) {
				sess->flags &= ~GF_NETIO_SESSION_USE_QUIC;
				sess->status = GF_NETIO_SETUP;
				sess->connect_pending = 0;
				sess->start_time = 0;
				gf_sk_group_unregister(sess->sock_group, sess->sock);
				gf_sk_del(sess->sock);
				sess->sock = NULL;
#ifdef GPAC_HTTPMUX
				if (sess->hmux_sess) {
					sess->hmux_sess->destroy(sess->hmux_sess);
					gf_list_del(sess->hmux_sess->sessions);
					gf_free(sess->hmux_sess);
					sess->hmux_sess = NULL;
				}
#endif
				GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[%s] Failed to connect through QUIC (%s), retrying with TCP\n", sess->log_name, gf_error_to_string(e)));
				goto resetup_socket;
			}
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(e)
			gf_dm_sess_notify_state(sess, sess->status, e);
			return;
		}
		if (register_sock) gf_sk_group_register(sess->sock_group, sess->sock);
		if (sess->allow_direct_reuse) {
			gf_dm_configure_cache(sess);
			if (sess->cached_file) return;
		}

		sess->connect_time = (u32) (gf_sys_clock_high_res() - now);
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Connected to %s:%d\n", sess->log_name, proxy, proxy_port));
		gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
	}

	if (sess->status == GF_NETIO_SETUP)
		sess->status = GF_NETIO_CONNECTED;

#ifdef GPAC_HAS_SSL
	if ((!sess->ssl || (sess->connect_pending==2))
		&& (sess->flags & (GF_DOWNLOAD_SESSION_USE_SSL|GF_DOWNLOAD_SESSION_SSL_FORCED))
	) {
		SSLConnectStatus ret = gf_ssl_try_connect(sess, proxy);
		if (ret != SSL_CONNECT_OK) {
			if (ret == SSL_CONNECT_RETRY)
				gf_dm_connect(sess);

			return;
		}
	} else
#endif
	{
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] connected\n", sess->log_name));
	}

	if (!allow_offline)
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
	gf_assert( entry == sess->cache_entry && entry);
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

#ifdef GPAC_HTTPMUX
static void gf_dm_sess_flush_input(GF_DownloadSession *sess)
{
	u32 res;
	sess->http_buf[0] = 0;
	GF_Err e = gf_dm_read_data(sess, sess->http_buf, sess->http_buf_size, &res);
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
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[%s] Session already started - ignoring start\n", sess->log_name));
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
				if (!sess->last_error) {
					//in case someone forgot to set this - if no error we must be in network empty state while connection is pending
					sess->last_error = GF_IP_NETWORK_EMPTY;
				}
			}
			break;
		case GF_NETIO_WAIT_FOR_REPLY:
		case GF_NETIO_CONNECTED:
			if (!sess->last_error)
				sess->last_error = GF_IP_NETWORK_EMPTY;

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
#ifdef GPAC_HTTPMUX
			if (sess->hmux_sess && sess->server_mode) {
				gf_dm_sess_flush_input(sess);
				sess->hmux_sess->write(sess);
			}
			//in put and waiting for EOS flush
			if ((sess->put_state==1) && sess->hmux_is_eos) return GF_IP_NETWORK_EMPTY;
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
	dm->all_sessions = gf_list_new();
	dm->cache_entries = gf_list_new();
	dm->credentials = gf_list_new();
	dm->skip_proxy_servers = gf_list_new();
	dm->partial_downloads = gf_list_new();
	dm->cache_mx = gf_mx_new("download_manager_cache_mx");
	dm->filter_session = fsess;
	default_cache_dir = NULL;
	gf_mx_p( dm->cache_mx );

#ifdef GPAC_HAS_CURL
	if (gf_opts_get_bool("core", "curl")) {
		dm->curl_multi = curl_multi_init();
		curl_multi_setopt(dm->curl_multi, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
		dm->curl_mx = gf_mx_new("curl");
		curl_global_init(CURL_GLOBAL_ALL);
	}
#endif

	dm->disable_http2 = gf_opts_get_bool("core", "no-h2");
#ifdef GPAC_HAS_NGTCP2
	opt = gf_opts_get_key("core", "h3");
	if (!opt || !strcmp(opt, "auto")) dm->h3_mode = H3_MODE_AUTO;
	else if (!strcmp(opt, "first")) dm->h3_mode = H3_MODE_FIRST;
	else if (!strcmp(opt, "only")) dm->h3_mode = H3_MODE_ONLY;
	else dm->h3_mode = H3_MODE_NO;
#else
	dm->h3_mode = H3_MODE_NO;
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

	dm->allow_broken_certificate = GF_FALSE;
	Bool do_clean = GF_TRUE;
	if ( gf_opts_get_bool("core", "clean-cache")) {
		dm->max_cache_size = 0;
		dm->cur_cache_size = gf_cache_cleanup(dm->cache_directory, dm->max_cache_size);
		do_clean = GF_FALSE;
	}
	dm->cache_clean_ms = 1000*gf_opts_get_int("core", "cache-check");
	dm->max_cache_size = gf_opts_get_int("core", "cache-size");
	dm->allow_broken_certificate = gf_opts_get_bool("core", "broken-cert");

	gf_mx_v( dm->cache_mx );

#ifdef GPAC_HAS_SSL
	dm->ssl_ctx = NULL;
#endif

	if (dm->max_cache_size && do_clean)
		dm->cur_cache_size = gf_cache_cleanup(dm->cache_directory, dm->max_cache_size);
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
	gf_assert( dm->all_sessions);
	gf_assert( dm->cache_mx );
	gf_mx_p( dm->cache_mx );

	while (gf_list_count(dm->partial_downloads)) {
		GF_PartialDownload * entry = (GF_PartialDownload*)gf_list_get( dm->partial_downloads, 0);
		gf_list_rem( dm->partial_downloads, 0);
		gf_assert( entry->filename );
		gf_file_delete( entry->filename );
		gf_free(entry->filename );
		entry->filename = NULL;
		entry->url = NULL;
		gf_free( entry );
	}

	/*destroy all pending sessions*/
	while (gf_list_count(dm->all_sessions)) {
		GF_DownloadSession *sess = (GF_DownloadSession *) gf_list_get(dm->all_sessions, 0);
		gf_dm_sess_del(sess);
	}
	gf_list_del(dm->all_sessions);
	dm->all_sessions = NULL;
	gf_assert( dm->skip_proxy_servers );
	while (gf_list_count(dm->skip_proxy_servers)) {
		char *serv = (char*)gf_list_get(dm->skip_proxy_servers, 0);
		gf_list_rem(dm->skip_proxy_servers, 0);
		gf_free(serv);
	}
	gf_list_del(dm->skip_proxy_servers);
	dm->skip_proxy_servers = NULL;
	gf_assert( dm->credentials);
	while (gf_list_count(dm->credentials)) {
		GF_UserCredentials * cred = (GF_UserCredentials*)gf_list_get( dm->credentials, 0);
		gf_list_rem( dm->credentials, 0);
		gf_free( cred );
	}
	gf_list_del( dm->credentials);
	dm->credentials = NULL;
	gf_assert( dm->cache_entries );

	/* Deletes DownloadedCacheEntry and associated files if required */
	while (gf_list_count(dm->cache_entries)) {
		const DownloadedCacheEntry entry = (const DownloadedCacheEntry)gf_list_pop_front( dm->cache_entries );
		if (!dm->max_cache_size)
			gf_cache_entry_set_delete_files_when_deleted(entry);
		gf_cache_delete_entry(entry);
	}
	gf_list_del( dm->cache_entries );
	dm->cache_entries = NULL;

	gf_list_del( dm->partial_downloads );
	dm->partial_downloads = NULL;
	if (dm->cache_directory)
		gf_free(dm->cache_directory);
	dm->cache_directory = NULL;

#ifdef GPAC_HAS_CURL
	if (dm->curl_multi) {
		curl_multi_cleanup(dm->curl_multi);
	}
	gf_mx_del(dm->curl_mx);
#endif

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
	gf_assert( icy_metaint > 0 );
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
					par.msg_type = GF_NETIO_ICY_META;
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
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] Chunk encoding: still %d bytes to get\n", sess->log_name, sess->nb_left_in_chunk));
		} else {
			*payload_size = sess->nb_left_in_chunk;
			sess->nb_left_in_chunk = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] Chunk encoding: last bytes in chunk received\n", sess->log_name));
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] Chunk encoding: current buffer does not contain enough bytes (%d) to read the size\n", sess->log_name, *payload_size));
		return NULL;
	}

	te_header[0] = 0;
	*header_size += (u32) (strlen(body_start)) + 2;

	sep = strchr(body_start, ';');
	if (sep) sep[0] = 0;
	res = sscanf(body_start, "%x", &size);
	if (res<0) {
		te_header[0] = '\r';
		if (sep) sep[0] = ';';
		*header_size = 0;
		*payload_size = 0;
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] Chunk encoding: fail to read chunk size from buffer %s, aborting\n", sess->log_name, body_start));
		return NULL;
	}
	if (sep) sep[0] = ';';
	*payload_size = size;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] Chunk Start: Header \"%s\" - header size %d - payload size %d (bytes done %d) at UTC "LLD"\n", sess->log_name, body_start, 2+strlen(body_start), size, sess->bytes_done, gf_net_get_utc()));

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

			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] bandwidth estimation: download time "LLD" us - bytes %u - chunk rate %u kbps (overall rate rate %u kbps)\n", sess->log_name, runtime, sess->bytes_done, sess->bytes_per_sec / 125, kbps));
		}
#endif
	} else {
		/*compute bps starting from request send time*/
		u64 runtime = (gf_sys_clock_high_res() - sess->request_start_time);
		if (!runtime) runtime=1;

		sess->bytes_per_sec = (u32) ((1000000 * (u64) sess->bytes_done) / runtime);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] bandwidth estimation: download time "LLD" us - bytes %u - rate %u kbps\n", sess->log_name, runtime, sess->bytes_done, sess->bytes_per_sec / 125));
	}
}


void gf_dm_data_received(GF_DownloadSession *sess, u8 *payload, u32 payload_size, Bool store_in_init, u32 *rewrite_size, u8 *original_payload)
{
	u32 nbBytes, remaining, hdr_size;
	u8* data = NULL;
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
			gf_assert(payload_size >= hdr_size);
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

		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] url %s received %d new bytes (%d kbps)\n", sess->log_name, sess->orig_url, nbBytes, 8*sess->bytes_per_sec/1000));
		if (sess->total_size && (sess->bytes_done > sess->total_size)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[%s] url %s received more bytes than planned!! Got %d bytes vs %d content length\n", sess->log_name, sess->orig_url, sess->bytes_done , sess->total_size ));
			sess->bytes_done = sess->total_size;
		}

		if (sess->icy_metaint > 0)
			gf_icy_skip_data(sess, (char *) data, nbBytes);
		else {
			if (sess->use_cache_file && !sess->cached_file)
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

#ifdef GPAC_HTTPMUX
		if (sess->hmux_sess && !sess->server_mode)
			sess->hmux_stream_id = -1;
#endif

		if (sess->use_cache_file) {
			//for chunk transfer or H2/H3
			gf_cache_set_content_length(sess->cache_entry, sess->total_size);
			gf_cache_close_write_cache(sess->cache_entry, sess, GF_TRUE);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE,
			       ("[CACHE] url %s saved as %s\n", gf_cache_get_url(sess->cache_entry), gf_cache_get_cache_filename(sess->cache_entry)));

			if (sess->dm && sess->dm->cache_clean_ms) {
				sess->dm->cur_cache_size += sess->total_size;
				if (sess->dm->cur_cache_size > sess->dm->max_cache_size) {
					if (!sess->dm->next_cache_clean) sess->dm->next_cache_clean = gf_sys_clock()+sess->dm->cache_clean_ms;
					else if (gf_sys_clock()>sess->dm->next_cache_clean) {
						sess->dm->cur_cache_size = gf_cache_cleanup(sess->dm->cache_directory, sess->dm->max_cache_size);
						sess->dm->next_cache_clean = 0;
					}
				}
			}
		}

		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		par.msg_type = GF_NETIO_DATA_TRANSFERED;
		par.error = GF_OK;

		gf_dm_sess_user_io(sess, &par);
		sess->total_time_since_req = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
		run_time = gf_sys_clock_high_res() - sess->start_time;

		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] %s (%d bytes) downloaded in "LLU" us (%d kbps) (%d us since request - got response in %d us)\n", sess->log_name, gf_file_basename(gf_cache_get_url(sess->cache_entry)), sess->bytes_done,
		                                     run_time, 8*sess->bytes_per_sec/1000, sess->total_time_since_req, sess->reply_time));

		if (sess->chunked && (payload_size==2))
			payload_size=0;
		sess->last_error = GF_OK;
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

static Bool dm_exceeds_cap_rate(GF_DownloadManager * dm, GF_DownloadSession *for_sess)
{
	u32 cumul_rate = 0;
	u32 i, count;

	gf_mx_p(dm->cache_mx);
	u64 now = gf_sys_clock_high_res();
	count = gf_list_count(dm->all_sessions);
	//check if this fits with all other sessions
	for (i=0; i<count; i++) {
		GF_DownloadSession * sess = (GF_DownloadSession*)gf_list_get(dm->all_sessions, i);
		if (for_sess && (sess != for_sess)) continue;

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
			gf_mx_v(dm->cache_mx);
			return GF_TRUE;
		}
		cumul_rate += sess->last_cap_rate_bytes_per_sec;
		if (for_sess) break;
	}
	gf_mx_v(dm->cache_mx);
	if (for_sess) {
		if (cumul_rate >= for_sess->max_data_rate)
			return GF_TRUE;
	} else if ( cumul_rate >= dm->limit_data_rate) {
		return GF_TRUE;
	}

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
		e = GF_IP_NETWORK_EMPTY;
	} else if (sess->status < GF_NETIO_DATA_EXCHANGE) {
		sess->do_requests(sess);
		if (!sess->server_mode
			&& (sess->status > GF_NETIO_WAIT_FOR_REPLY)
			&& sess->needs_range
			&& (sess->rsp_code==200)
		) {
			//reset for next call to process if user wants so
			sess->needs_range = GF_FALSE;
			return GF_IO_BYTE_RANGE_NOT_SUPPORTED;
		}
		e = sess->last_error ? sess->last_error : GF_IP_NETWORK_EMPTY;
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
		Bool was_modified;
		u32 to_copy, full_cache_size, max_valid_size=0, cache_done;
		const u8 *ptr;
		e = GF_OK;
		gf_assert(sess->cache_entry);
		//always refresh total size
		sess->total_size = gf_cache_get_content_length(sess->cache_entry);

		ptr = gf_cache_get_content(sess->cache_entry, &full_cache_size, &max_valid_size, &was_modified);

		cache_done = gf_cache_is_done(sess->cache_entry);
		//something went wrong, we cannot have less valid bytes than what we had at previous call(s)
		if (max_valid_size < sess->bytes_done)
			cache_done = 2;

		if ((sess->bytes_done >= full_cache_size)|| (cache_done==2)) {
			*read_size = 0;
			gf_cache_release_content(sess->cache_entry);
			if (cache_done==2) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(GF_IP_NETWORK_FAILURE)
				return GF_IP_NETWORK_FAILURE;
			}
			else if (cache_done) {
				sess->status = GF_NETIO_DATA_TRANSFERED;
				SET_LAST_ERR( GF_OK)
				return GF_EOS;
			}
			return was_modified ? GF_OK : GF_IP_NETWORK_EMPTY;
		}
		if (!ptr) return GF_OUT_OF_MEM;

		//only copy valid bytes for http
		to_copy = max_valid_size - sess->bytes_done;
		if (to_copy > buffer_size) to_copy = buffer_size;

		memcpy(buffer, ptr + sess->bytes_done, to_copy);
		sess->bytes_done += to_copy;
		*read_size = to_copy;
		if (cache_done==1) {
			sess->status = GF_NETIO_DATA_TRANSFERED;
			SET_LAST_ERR( (cache_done==2) ? GF_IP_NETWORK_FAILURE : GF_OK)
		} else {
			sess->total_size = 0;
		}
		gf_cache_release_content(sess->cache_entry);
	} else {

		if (sess->dm && (sess->dm->limit_data_rate || sess->max_data_rate)) {
			if (dm_exceeds_cap_rate(sess->dm, sess->max_data_rate ? sess : NULL))
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
						GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] No HTTP chunk header found for %d bytes, assuming broken chunk transfer and aborting\n", sess->log_name, sess->remaining_data_size));
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
				gf_assert(single_read==0);
				break;
			}

			size = sess->remaining_data_size + single_read;
			sess->remaining_data_size = 0;
			single_read = 0;

#ifdef GPAC_HTTPMUX
			//do NOT call data received if hmux, done once input is empty
			if (!sess->hmux_sess || sess->cached_file)
#endif
				gf_dm_data_received(sess, (u8 *) buffer + nb_read, size, GF_FALSE, &single_read, buffer + nb_read);


			if (!sess->chunked)
				single_read = size;

			nb_read += single_read;
		}

#ifdef GPAC_HTTPMUX
		//process any received data
		if (sess->hmux_sess) {
			nb_read = 0;
			hmux_fetch_data(sess, buffer, buffer_size, &nb_read);
			sess->hmux_sess->write(sess);

			//stream is over and all data flushed, move to GF_NETIO_DATA_TRANSFERED in client mode
			if ((sess->hmux_stream_id<0) && sess->hmux_data_done && !sess->hmux_buf.size && !sess->server_mode) {
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
#ifdef GPAC_HTTPMUX
			|| sess->hmux_sess
#endif
		)
			gf_dm_sess_estimate_chunk_rate(sess, nb_read);

		if (! (*read_size) && (e==GF_IP_NETWORK_EMPTY)) {
#ifdef GPAC_HTTPMUX
			if (sess->hmux_sess && !sess->total_size
				//for client, wait for close - for server move to data_transfered as soon as we're done pushing data
				&& (((sess->hmux_stream_id<0) && sess->bytes_done) || (sess->hmux_data_done && sess->server_mode))
			) {
				sess->status = GF_NETIO_DATA_TRANSFERED;
				SET_LAST_ERR(GF_OK)
				return GF_EOS;
			}
#endif

#ifdef GPAC_HAS_CURL
			if (sess->curl_hnd) {
				if (sess->status == GF_NETIO_WAIT_FOR_REPLY)
					return GF_IP_NETWORK_EMPTY;
			} else
#endif
			if (sess->sock)
				e = gf_sk_probe(sess->sock);

			if ((e==GF_IP_CONNECTION_CLOSED)
				|| (sess->request_timeout && (gf_sys_clock_high_res() - sess->last_fetch_time > 1000 * sess->request_timeout))
			) {
				if (e==GF_IP_CONNECTION_CLOSED) {
					SET_LAST_ERR(GF_IP_CONNECTION_CLOSED)
					sess_connection_closed(sess);
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP,
					       ("[%s] Session timeout for %s after %u ms, aborting\n", sess->log_name, sess->orig_url, sess->request_timeout));

					SET_LAST_ERR(GF_IP_NETWORK_FAILURE)
				}
				sess->status = GF_NETIO_STATE_ERROR;
				return GF_IP_NETWORK_EMPTY;
			}
		}
	}

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess && sess->hmux_buf.size) {
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
		if (sess->dm && (sess->dm->limit_data_rate || sess->max_data_rate) && sess->last_cap_rate_bytes_per_sec) {
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

GF_EXPORT
void gf_dm_sess_abort(GF_DownloadSession * sess)
{
	if (sess) {
		gf_mx_p(sess->mx);

#ifdef GPAC_HTTPMUX
		if (sess->hmux_sess && (sess->status==GF_NETIO_DATA_EXCHANGE)) {
			sess->hmux_sess->stream_reset(sess, GF_TRUE);
			sess->hmux_sess->write(sess);
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
static GF_Err http_send_headers(GF_DownloadSession *sess) {
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

	char * sHTTP = sess->http_buf;

	gf_assert(sess->remaining_data_size == 0);

	if (sess->needs_cache_reconfig) {
		gf_dm_sess_clear_headers(sess);
		gf_dm_configure_cache(sess);
		sess->needs_cache_reconfig = 0;
	}
	if (sess->cached_file) {
		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = 0;
		sess->status = GF_NETIO_WAIT_FOR_REPLY;
		gf_dm_sess_notify_state(sess, GF_NETIO_WAIT_FOR_REPLY, GF_OK);
		return GF_OK;
	}
	gf_dm_sess_clear_headers(sess);

	//in case we got disconnected, reconnect
#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd)
		e = GF_OK;
	else
#endif
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

	/*setup authentication*/
	strcpy(pass_buf, "");
	sess->creds = gf_user_credentials_find_for_site( sess->dm, sess->server_name, NULL);
	if (sess->creds && sess->creds->valid) {
#ifdef GPAC_HAS_CURL
		//let curl handle authentication methods
		if (sess->curl_hnd) {
			char szUsrPass[101];
			u32 len = gf_base64_decode(sess->creds->digest, (u32) strlen(sess->creds->digest), szUsrPass, 100);
			szUsrPass[len]=0;
			curl_easy_setopt(sess->curl_hnd, CURLOPT_USERPWD, szUsrPass);
		} else
#endif
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
#ifdef GPAC_HTTPMUX
		if (!sess->hmux_sess)
#endif
			inject_icy = GF_TRUE;
	} else if (!strcmp(req_name, "HEAD")) sess->http_read_type = HEAD;
	else sess->http_read_type = OTHER;

	if (!strcmp(req_name, "PUT") || !strcmp(req_name, "POST"))
		sess->put_state = 1;

	//url is the remote path event if proxy (Host header will point to desired host)
	//note that url is not used for CURL, already setup together with proxy
	url = sess->remote_path;

	/*get all headers*/
	gf_dm_sess_clear_headers(sess);


#ifdef GPAC_HTTPMUX
	if (!sess->hmux_sess)
#endif
	{
		//always put port number when proxy is enabled
		if (sess->proxy_enabled || ((sess->port!=80) && (sess->port!=443))) {
			sprintf(sHTTP, "%s:%u", sess->server_name, sess->port);
			PUSH_HDR("Host", sHTTP);
		} else {
			PUSH_HDR("Host", sess->server_name);
		}
	}

	has_agent = has_accept = has_connection = has_range = has_language = has_mime = has_chunk_transfer = GF_FALSE;
	while (1) {
		par.msg_type = GF_NETIO_GET_HEADER;
		par.value = NULL;
		gf_dm_sess_user_io(sess, &par);
		if (!par.value) break;
		//if name is not set, skip this header
		if (!par.name) continue;

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

		gf_dm_sess_set_header_ex(sess, par.name, par.value, GF_TRUE);

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

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess)
		has_connection = GF_TRUE;
#endif


#ifdef GPAC_HAS_HTTP2
	//inject upgrade for h2
	if (!has_connection && !sess->hmux_sess
#ifdef GPAC_HAS_CURL
		&& !sess->curl_hnd
#endif
#ifdef GPAC_HAS_SSL
		&& !sess->ssl
#endif
		&& !sess->dm->disable_http2
		&& !sess->h2_upgrade_state
		&& !gf_opts_get_bool("core", "no-h2c")
	) {
		http2_set_upgrade_headers(sess);
		inject_icy = GF_FALSE;
	} else
#endif //GPAC_HAS_HTTP2

	if (sess->proxy_enabled==1) {
#ifdef GPAC_HTTPMUX
		if (!sess->hmux_sess)
#endif
			PUSH_HDR("Proxy-Connection", "Keep-alive")
	}
	else if (!has_connection) {
		PUSH_HDR("Connection", "Keep-Alive");
	}


	if (has_chunk_transfer
#ifdef GPAC_HTTPMUX
		&& !sess->hmux_sess
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

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		Bool has_body = GF_FALSE;

		gf_mx_p(sess->mx);

		sess->hmux_is_eos = 0;
		sess->hmux_send_data = NULL;
		if (par.data && par.size) {
			has_body = GF_TRUE;
			sess->hmux_send_data = (u8 *) par.data;
			sess->hmux_send_data_len = par.size;
			sess->hmux_is_eos = 1;
		} else if (sess->put_state==1) {
			has_body = GF_TRUE;
		}
		e = sess->hmux_sess->submit_request(sess, req_name, url, param_string, has_body);
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP] Sending request %s %s%s\n", req_name, sess->server_name, url));

		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();

		if (!e || (e==GF_IP_NETWORK_EMPTY)) {
			e = sess->hmux_sess->send_pending_data(sess);
			if (e==GF_IP_NETWORK_EMPTY) e = GF_OK;
		}

		gf_mx_v(sess->mx);
		goto req_sent;
	}
#endif // GPAC_HTTPMUX

#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd) {
		if (!sess->curl_not_http)
			curl_easy_setopt(sess->curl_hnd, CURLOPT_CUSTOMREQUEST, req_name);
	} else
#endif
	{

		if (param_string) {
			if (strchr(sess->remote_path, '?')) {
				sprintf(sHTTP, "%s %s&%s HTTP/1.1\r\n", req_name, url, param_string);
			} else {
				sprintf(sHTTP, "%s %s?%s HTTP/1.1\r\n", req_name, url, param_string);
			}
		} else {
			sprintf(sHTTP, "%s %s HTTP/1.1\r\n", req_name, url);
		}
	}

	//serialize headers
	count = gf_list_count(sess->headers);
	for (i=0; i<count; i++) {
		hdr = gf_list_get(sess->headers, i);
#ifdef GPAC_HAS_CURL
		if (sess->curl_hnd) {
			if (sess->curl_not_http) continue;
			char szHDR[1000];
			sprintf(szHDR, "%s: %s", hdr->name, hdr->value);
			sess->curl_hdrs = curl_slist_append(sess->curl_hdrs, szHDR);
			continue;
		}
#endif
		strcat(sHTTP, hdr->name);
		strcat(sHTTP, ": ");
		strcat(sHTTP, hdr->value);
		strcat(sHTTP, "\r\n");
	}
#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd) {
		if (!sess->curl_not_http) {
			curl_easy_setopt(sess->curl_hnd, CURLOPT_HTTPHEADER, sess->curl_hdrs);
		}
	}
#endif

	strcat(sHTTP, "\r\n");

#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd) {
		gf_assert(!sess->curl_hnd_registered);
	} else
#endif
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
					       ("[%s] Error while loading UserProfile, size=%d, should be %d\n", sess->log_name, read, par.size));
					for (; read < (s32) par.size; read++) {
						tmp_buf[len + read] = 0;
					}
				}
				gf_fclose(profile);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[%s] Error while loading Profile file %s\n", sess->log_name, user_profile));
			}
		}

		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = len+par.size;

		e = dm_sess_write(sess, tmp_buf, len+par.size);

		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Sending request to %s %s\n\n", sess->log_name, sess->server_name, tmp_buf));
		gf_free(tmp_buf);
	} else {
		u32 len = (u32) strlen(sHTTP);

		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
		sess->req_hdr_size = len;

		e = dm_sess_write(sess, sHTTP, len);

#ifndef GPAC_DISABLE_LOG
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] Error sending request %s\n", sess->log_name, gf_error_to_string(e) ));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Sending request to %s %s\n\n", sess->log_name, sess->server_name, sHTTP));
		}
#endif
	}


#ifdef GPAC_HTTPMUX
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

#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd) {
		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
		curl_flush(sess);
	}
#endif
	return GF_OK;
}


/*!
 * Parse the remaining part of body
\param sess The session
\param sHTTP the data buffer
\return The error code if any
 */
static GF_Err http_parse_remaining_body(GF_DownloadSession * sess)
{
	GF_Err e;

	while (1) {
		u32 prev_remaining_data_size, size=0, rewrite_size=0;
		if (sess->status>=GF_NETIO_DISCONNECTED)
			return GF_REMOTE_SERVICE_ERROR;

		if (sess->dm && sess->bytes_per_sec && (sess->max_data_rate || sess->dm->limit_data_rate)) {
			sess->rate_regulated = GF_FALSE;
			if (dm_exceeds_cap_rate(sess->dm, sess->max_data_rate ? sess : NULL)) {
				sess->rate_regulated = GF_TRUE;
				return GF_OK;
			}
		}

		//the data remaining from the last buffer (i.e size for chunk that couldn't be read because the buffer does not contain enough bytes)
		if (sess->remaining_data && sess->remaining_data_size) {
			if (sess->remaining_data_size >= sess->http_buf_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] No HTTP chunk header found for %d bytes, assuming broken chunk transfer and aborting\n", sess->log_name, sess->remaining_data_size));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			memcpy(sess->http_buf, sess->remaining_data, sess->remaining_data_size);
		}
		e = gf_dm_read_data(sess, sess->http_buf + sess->remaining_data_size, sess->http_buf_size - sess->remaining_data_size, &size);
		if ((e != GF_IP_CONNECTION_CLOSED) && (!size || e == GF_IP_NETWORK_EMPTY)) {
			if (!sess->total_size && !sess->chunked && (gf_sys_clock_high_res() - sess->start_time > 5000000)) {
				sess->total_size = sess->bytes_done;
				gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
				gf_assert(sess->server_name);
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] Disconnected from %s: %s\n", sess->log_name, sess->server_name, gf_error_to_string(e)));
				gf_dm_disconnect(sess, HTTP_NO_CLOSE);
			}
			return GF_OK;
		}

		if (e) {
			if (sess->sock && (e == GF_IP_CONNECTION_CLOSED)) {
				u32 len = gf_cache_get_content_length(sess->cache_entry);
				if (size > 0) {
#ifdef GPAC_HTTPMUX
					if (sess->hmux_sess) {
						hmux_flush_internal_data(sess, GF_FALSE);
					} else
#endif
						gf_dm_data_received(sess, (u8 *) sess->http_buf, size, GF_FALSE, NULL, NULL);
				}

				if ( ( (len == 0) && sess->use_cache_file) || sess->bytes_done) {
					sess->total_size = sess->bytes_done;
					// HTTP 1.1 without content length...
					gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
					gf_assert(sess->server_name);
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] Disconnected from %s: %s\n", sess->log_name, sess->server_name, gf_error_to_string(e)));
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

		sess->http_buf[size + prev_remaining_data_size] = 0;

#ifdef GPAC_HTTPMUX
		if (sess->hmux_sess) {
			hmux_flush_internal_data(sess, GF_FALSE);
			if (sess->hmux_stream_id<0) {
				sess->status = GF_NETIO_DATA_TRANSFERED;
				SET_LAST_ERR(GF_OK)
			}
		} else
#endif
			gf_dm_data_received(sess, (u8 *) sess->http_buf, size + prev_remaining_data_size, GF_FALSE, &rewrite_size, NULL);

		if (sess->chunked)
			gf_dm_sess_estimate_chunk_rate(sess, rewrite_size);


		/*socket empty*/
		if (size < sess->http_buf_size) {
			return GF_OK;
		}
	}
	return GF_OK;
}

static void notify_error_body(GF_DownloadSession *sess, char * sHTTP, s32 bytesRead, s32 BodyStart)
{
	GF_NETIO_Parameter par;

	if (sHTTP) {
		sHTTP[bytesRead]=0;
		par.error = GF_BAD_PARAM;
		par.reply = sess->rsp_code;
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
static GF_Err wait_for_header_and_parse(GF_DownloadSession *sess)
{
	GF_NETIO_Parameter par;
	s32 bytesRead=0, BodyStart=0;
	u32 res, i, buf_size = sess->http_buf_size;
	s32 LinePos, Pos;
	u32 method=0;
	u32 ContentLength=0, first_byte, last_byte, total_size, range, no_range;
	Bool connection_closed = GF_FALSE;
	Bool has_content_length = GF_FALSE;
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

	char *sHTTP = sess->http_buf;
	sHTTP[0] = 0;

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
		gf_fatal_assert( sess->status == GF_NETIO_CONNECTED );
	} else {
		gf_fatal_assert( sess->status == GF_NETIO_WAIT_FOR_REPLY );
		if (!(sess->flags & GF_NETIO_SESSION_NOT_CACHED)) {
			sess->use_cache_file = sess->dm->disable_cache ? GF_FALSE : GF_TRUE;
		}
	}
	bytesRead = res = 0;
	new_location = NULL;

	if (sess->cached_file) {
		sess->reply_time = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
		sess->rsp_hdr_size = 0;
		sess->total_size = gf_cache_get_content_length(sess->cache_entry);
		sess->bytes_done = 0;

		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = GF_NETIO_DATA_EXCHANGE;
		par.error = GF_OK;
		gf_dm_sess_user_io(sess, &par);
		sess->status = GF_NETIO_DATA_EXCHANGE;
//		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		return GF_OK;
	}

#ifdef GPAC_HAS_CURL
	if (sess->curl_hnd) {
		e = curl_process_reply(sess, &ContentLength);
		if (e) return e;
		goto process_reply;
	}
#endif

	//always set start time to the time at last attempt reply parsing
	sess->start_time = gf_sys_clock_high_res();
	sess->start_time_utc = gf_net_get_utc();
	sess->chunked = GF_FALSE;
	sess->last_chunk_found = GF_FALSE;

	sess->last_chunk_start_time = sess->request_start_time;
	sess->chunk_bytes = 0;
	sess->cumulated_chunk_rate = 0;

	while (1) {
		Bool probe = (!bytesRead || (sess->flags & GF_NETIO_SESSION_NO_BLOCK) ) ? GF_TRUE : GF_FALSE;
#ifdef GPAC_HAS_NGTCP2
		if (sess->server_mode && sess->hmux_sess && (sess->hmux_sess->net_sess->flags & GF_NETIO_SESSION_USE_QUIC)) {
			GF_Err h3_check_sess(GF_DownloadSession *sess);
			probe = GF_FALSE;
			e = h3_check_sess(sess);
		} else
#endif
			e = gf_dm_read_data(sess, sHTTP + bytesRead, buf_size - bytesRead, &res);

#ifdef GPAC_HTTPMUX
		/* break as soon as we have a header frame*/
		if (sess->hmux_headers_seen) {
			sess->hmux_headers_seen = 0;
			res = 0;
			bytesRead = 0;
			BodyStart = 0;
			e = GF_OK;
			SET_LAST_ERR(GF_OK)
			break;
		}
#endif
		//should not happen, but do it for safety
		if (!res && !bytesRead && !e) e = GF_IP_NETWORK_EMPTY;

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
					GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP,
					       ("[%s] Session timeout for %s after %u ms, aborting\n", sess->log_name, sess->orig_url, sess->request_timeout));
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

#ifdef GPAC_HTTPMUX
			//we may have received bytes (bytesRead>0) yet none for this session, return GF_IP_NETWORK_EMPTY if empty
			if (sess->hmux_sess)
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
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Connection closed by client\n", sess->log_name, sess->remote_path));
				return GF_IP_CONNECTION_CLOSED;
			}
			gf_dm_disconnect(sess, HTTP_RESET_CONN);

			if (sess->num_retry) {
#ifdef GPAC_HAS_SSL
				if ((sess->num_retry < SESSION_RETRY_SSL)
					&& !(sess->flags & GF_DOWNLOAD_SESSION_USE_SSL)
					&& !gf_opts_get_bool("core", "no-tls-rcfg")
				) {
					GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Connection closed by server when processing %s - retrying using SSL\n", sess->log_name, sess->remote_path));
					sess->flags |= GF_DOWNLOAD_SESSION_SSL_FORCED;
				} else
#endif
				{
					GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Connection closed by server when processing %s - retrying\n", sess->log_name, sess->remote_path));
				}
				sess->status = GF_NETIO_SETUP;
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Connection closed by server when processing %s - aborting\n", sess->log_name, sess->remote_path));
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

#ifdef GPAC_HTTPMUX
		//in case we got a refused stream
		if (sess->status==GF_NETIO_SETUP) {
			return GF_OK;
		}
		if (sess->hmux_sess)
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
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[%s] End of chunk found while waiting server response when processing %s - retrying\n", sess->log_name, sess->remote_path));
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

	no_range = range = ContentLength = first_byte = last_byte = total_size = sess->rsp_code = 0;

	if (sess->flags & GF_NETIO_SESSION_NO_BLOCK) {
		sHTTP = sess->async_req_reply;
		buf_size = bytesRead = sess->async_req_reply_size;
		sess->async_req_reply_size = 0;
	}

#ifdef GPAC_HTTPMUX
	if (!sess->hmux_sess) {
#endif
		if (bytesRead < 0) {
			e = GF_REMOTE_SERVICE_ERROR;
			goto exit;
		}

		if (!BodyStart)
			BodyStart = bytesRead;

		if (BodyStart) sHTTP[BodyStart-1] = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] %s\n\n", sess->log_name, sHTTP));

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
				sess->rsp_code = 200;
			else
				sess->rsp_code = 300;
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
			sess->rsp_code = (u32) atoi(comp);
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
				if (hdr_val)
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

#ifdef GPAC_HTTPMUX
	}
#endif


#ifdef GPAC_HAS_CURL
process_reply:
#endif

	if (!sess->server_mode) {
		Bool cache_no_store = GF_FALSE;
		Bool cache_must_revalidate = GF_FALSE;
		u32 delta_age = 0;
		u32 max_age = 0;

		//default pre-processing of headers - needs cleanup, not all of these have to be parsed before checking reply code
		for (i=0; i<gf_list_count(sess->headers); i++) {
			char *val;
			GF_HTTPHeader *hdr = (GF_HTTPHeader*)gf_list_get(sess->headers, i);

#ifdef GPAC_HTTPMUX
			if (!stricmp(hdr->name, ":status") ) {
				sess->rsp_code = (u32) atoi(hdr->value);
			} else
#endif
			if (!stricmp(hdr->name, "Content-Length") ) {
				ContentLength = (u32) atoi(hdr->value);
				has_content_length=GF_TRUE;

				if ((sess->rsp_code<300) && sess->cache_entry)
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
				if (sess->rsp_code<300) {
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
					while (strchr(":= ", val[0]))
						val++;

					if (val[0] == '*') {
						sscanf(val, "*/%u", &total_size);
					} else {
						sscanf(val, "%u-%u/%u", &first_byte, &last_byte, &total_size);
					}
					sess->full_resource_size = total_size;
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
			else if (!stricmp(hdr->name, "Age")) {
				sscanf(hdr->value, "%u", &delta_age);
			}
			else if (!stricmp(hdr->name, "Cache-Control")) {
				char *hval = hdr->value;
				while (hval[0]) {
					char *hsep = strchr(hval, ',');
					if (hsep) hsep[0] = 0;
					while (hval[0]==' ') hval++;
					char *vsep = strchr(hval, '=');
					if (vsep) vsep[0] = 0;
					if (!strcmp(hval, "no-store")) cache_no_store = GF_TRUE;
					else if (!strcmp(hval, "no-cache")) cache_no_store = GF_TRUE;
					else if (!strcmp(hval, "private")) {
						//we need a way to differentiate proxy modes and client modes
					}
					else if (!strcmp(hval, "public")) {}
					else if (!strcmp(hval, "max-age") && vsep && !max_age) {
						sscanf(vsep+1, "%u", &max_age);
					}
					else if (!strcmp(hval, "s-maxage") && vsep) {
						sscanf(vsep+1, "%u", &max_age);
					}
					else if (!strcmp(hval, "must-revalidate")) cache_must_revalidate = GF_TRUE;
					else if (!strcmp(hval, "proxy-revalidate")) cache_must_revalidate = GF_TRUE;

					if (vsep) vsep[0] = '=';
					if (!hsep) break;
					hsep[0] = ',';
					hval = hsep+1;
				}
			}
			else if (!stricmp(hdr->name, "ETag")) {
				if (sess->rsp_code<300)
					gf_cache_set_etag_on_server(sess->cache_entry, hdr->value);
			}
			else if (!stricmp(hdr->name, "Last-Modified")) {
				if (sess->rsp_code<300)
					gf_cache_set_last_modified_on_server(sess->cache_entry, hdr->value);
			}
			else if (!stricmp(hdr->name, "Transfer-Encoding")) {
				if (!stricmp(hdr->value, "chunked")
#ifdef GPAC_HAS_CURL
					&& !sess->curl_hnd
#endif
				)
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

		if ((sess->flags & GF_NETIO_SESSION_AUTO_CACHE) && !ContentLength && (sess->rsp_code>=200) && (sess->rsp_code<300) ) {
			sess->use_cache_file = GF_FALSE;
			if (sess->cache_entry) {
				gf_cache_entry_set_delete_files_when_deleted(sess->cache_entry);
				gf_cache_remove_session_from_cache_entry(sess->cache_entry, sess);
				sess->cache_entry = NULL;
			}
		}

		sess->flags &= ~GF_NETIO_SESSION_NO_STORE;
		if (cache_no_store) {
			gf_cache_entry_set_delete_files_when_deleted(sess->cache_entry);
			sess->flags |= GF_NETIO_SESSION_NO_STORE;

			if (sess->cache_entry && !ContentLength && !sess->chunked && (sess->rsp_code<300)
#ifdef GPAC_HTTPMUX
				&& !sess->hmux_sess
#endif
			) {
				sess->use_cache_file = GF_FALSE;
				gf_cache_remove_session_from_cache_entry(sess->cache_entry, sess);
				sess->cache_entry = NULL;
			}
		}
		else if (sess->cache_entry) {
			if (max_age) max_age += delta_age;
			gf_cache_set_max_age(sess->cache_entry, max_age, cache_must_revalidate);
		}


		if (no_range) first_byte = 0;


		gf_cache_set_headers_processed(sess->cache_entry);

		if (connection_keep_alive
			&& !gf_opts_get_bool("core", "no-timeout")
			&& !sess->server_mode
#ifdef GPAC_HTTPMUX
			&& !sess->hmux_sess
#endif
		) {
			sess->connection_timeout_ms = connection_timeout*1000;
		}
	} else {
		//server mode, start timers as soon as we see the request headers
		sess->last_fetch_time = sess->request_start_time = gf_sys_clock_high_res();
	}

	//if we issued an open-range from end of file till unknown we may get a 416. If the server is indicating
	//resource size and it matches our range, move o 206
	if ((sess->rsp_code==416) && (sess->range_start==sess->full_resource_size)) {
		sess->rsp_code = 206;
		ContentLength = 0;
		has_content_length = GF_TRUE;
	}
	//if no start range, a server may reply with 200 if open end range or if end range is file size
	//move this to 200 to avoid triggering a byte range not supported detection
	else if (sess->needs_range && (sess->rsp_code==200) && !sess->range_start && (!sess->range_end || (sess->range_end+1==ContentLength))) {
		sess->rsp_code = 206;
	}

	par.msg_type = GF_NETIO_PARSE_REPLY;
	par.error = GF_OK;
	par.reply = sess->rsp_code;
	par.value = comp;
	/*
	 * If response is correct, it means our credentials are correct
	 */
	if (sess->creds && sess->rsp_code != 304)
		sess->creds->valid = GF_TRUE;

#ifdef GPAC_HAS_HTTP2
	if ((sess->rsp_code == 101) && upgrade_to_http2) {
		char *body = NULL;
		u32 body_len = 0;
		if (bytesRead > BodyStart) {
			body = sHTTP + BodyStart;
			body_len = bytesRead - BodyStart;
		}
		return http2_do_upgrade(sess, body, body_len);
	}
	if (sess->h2_upgrade_state<4)
		sess->h2_upgrade_state = 2;
#endif


	/*try to flush body */
	if ((sess->rsp_code>=300)
#ifdef GPAC_HTTPMUX
		&& !sess->hmux_sess
#endif
#ifdef GPAC_HAS_CURL
		&& !sess->curl_hnd
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
			if (bytesRead == buf_size)
				break;
		}

		if (BodyStart + ContentLength > (u32) bytesRead) {
			ContentLength = 0;
			//cannot flush, discard socket
			sess->connection_close = GF_TRUE;
		}
	}

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		u32 count = gf_list_count(sess->headers);
		for (i=0; i<count; i++) {
			GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
			if (!stricmp(hdr->name, ":method")) {
				method = http_parse_method(hdr->value);
				sess->rsp_code = 200;
			}
			else if (!stricmp(hdr->name, ":path")) {
				if (sess->orig_url) gf_free(sess->orig_url);
				sess->orig_url = gf_strdup(hdr->value);
			}
		}
	}
#endif
#ifdef GPAC_HAS_HTTP2
	else if (sess->server_mode
		&& !gf_opts_get_bool("core", "no-h2")
		&& !gf_opts_get_bool("core", "no-h2c")
		&& !(sess->flags & GF_NETIO_SESSION_USE_QUIC)
	) {
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
#ifdef GPAC_HTTPMUX
			&& !sess->hmux_sess
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
	gf_assert(sess->rsp_code);


	switch (sess->rsp_code) {
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
		sess->nb_redirect=0;
		break;
	/*redirection: extract the new location*/
	case 301:
	case 302:
	case 303:
	case 307:
		if ((sess->nb_redirect > 4) || !new_location || !strlen(new_location) ) {
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
		sess->nb_redirect++;
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
		gf_assert(sess->cache_entry);
		gf_assert(!sess->cached_file);

		//special case for resources stored as persistent (mpd, init seg): we don't push the data
		if (gf_cache_entry_persistent(sess->cache_entry)) {
			gf_dm_sess_notify_state(sess, GF_NETIO_PARSE_REPLY, GF_OK);

			/* Cache file is the most recent */
			sess->status = GF_NETIO_DATA_TRANSFERED;
			SET_LAST_ERR(GF_OK)
			gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
			gf_dm_disconnect(sess, HTTP_NO_CLOSE);
			return GF_OK;
		}

		sess->status = GF_NETIO_PARSE_REPLY;
		if (!gf_cache_is_mem(sess->cache_entry)) {
			sess->cached_file = gf_cache_open_read(sess->cache_entry);
			if (!sess->cached_file) {
				GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] FAILED to open cache file %s for reading contents !\n", sess->log_name, gf_cache_get_cache_filename(sess->cache_entry)));
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
		} else {
			//we read from mem cache
			sess->local_cache_only = GF_TRUE;
		}
		sess->status = GF_NETIO_DATA_EXCHANGE;
		sess->total_size = ContentLength = gf_cache_get_cache_filesize(sess->cache_entry);

		gf_dm_sess_user_io(sess, &par);
		sess->status = GF_NETIO_DATA_EXCHANGE;
		e = GF_EOS;
		break;
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
		notify_error_body(sess, sHTTP, bytesRead, BodyStart);
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
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[%s] Failure - body: %s\n", sess->log_name, sHTTP + BodyStart));
		}
		notify_error_body(sess, sHTTP, bytesRead, BodyStart);

		switch (sess->rsp_code) {
		case 204: e = GF_EOS; break;
		/* File not found */
		case 404:
		//too early
		case 425:
			e = GF_URL_ERROR; break;
		/* Forbidden */
		case 403: e = GF_AUTHENTICATION_FAILURE; break;
		/* Range not accepted */
		case 416:
			e = GF_SERVICE_ERROR;
			break;
		case 504: e = GF_URL_ERROR; break;
		default:
			if (sess->rsp_code>=500) e = GF_REMOTE_SERVICE_ERROR;
			else e = GF_SERVICE_ERROR;
			break;
		}
		goto exit;
	}

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
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] Error processing rely from %s: %s\n", sess->log_name, sess->server_name, gf_error_to_string(e) ) );
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] Reply processed from %s\n", sess->log_name, sess->server_name ) );
	}
#endif

	//in HTTP2 we may resume to setup state if we got a refused stream
	if (sess->status == GF_NETIO_SETUP) {
		return GF_OK;
	}


	/*some servers may reply without content length, but we MUST have it*/
	if (e) goto exit;
	if (sess->icy_metaint != 0) {
		gf_assert( ! sess->use_cache_file );
		GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] ICY protocol detected\n", sess->log_name));
		if (mime_type && !stricmp(mime_type, "video/nsv")) {
			gf_cache_set_mime_type(sess->cache_entry, "audio/aac");
		}
		sess->icy_bytes = 0;
		sess->total_size = SIZE_IN_STREAM;
		sess->status = GF_NETIO_DATA_EXCHANGE;
	} else if (!ContentLength && !has_content_length && !sess->chunked
#ifdef GPAC_HTTPMUX
		&& !sess->hmux_sess
#endif
#ifdef GPAC_HAS_CURL
		&& !sess->curl_hnd
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
#ifdef GPAC_HTTPMUX
	} else if (sess->hmux_sess && !ContentLength && (sess->http_read_type != GET)) {
		gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		return GF_OK;
#endif
	} else {
		sess->total_size = ContentLength;
		if (sess->use_cache_file && !sess->cached_file && (sess->http_read_type == GET)) {

			e = gf_cache_open_write_cache(sess->cache_entry, sess);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ( "[CACHE] Failed to open cache, error=%d\n", e));
				goto exit;
			}
		}
		sess->status = GF_NETIO_DATA_EXCHANGE;
		sess->bytes_done = 0;
		if (!ContentLength && has_content_length) {
			gf_dm_sess_notify_state(sess, GF_NETIO_DATA_TRANSFERED, GF_OK);
			gf_dm_disconnect(sess, HTTP_NO_CLOSE);
			sess->status = GF_NETIO_DATA_TRANSFERED;
			return GF_OK;
		}
	}

	/* we may have existing data in this buffer ... */
#ifdef GPAC_HTTPMUX
	if (!e && sess->hmux_sess) {
		hmux_flush_internal_data(sess, GF_TRUE);
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
			GF_LOG((e==GF_URL_ERROR) ? GF_LOG_INFO : GF_LOG_WARNING, GF_LOG_HTTP, ("[%s] Error parsing reply for URL %s: %s (code %d)\n", sess->log_name, sess->orig_url,  gf_error_to_string(e), sess->rsp_code ));
		} else {
			e = GF_OK;
		}
		gf_cache_entry_set_delete_files_when_deleted(sess->cache_entry);
		gf_cache_remove_entry_from_session(sess);
		sess->cache_entry = NULL;
		gf_dm_disconnect(sess, HTTP_NO_CLOSE);
		if ((e<0) && connection_closed)
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
	sess->http_buf[0] = 0;

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
			wait_for_header_and_parse(sess);
		} else {
			http_send_headers(sess);
		}
		break;
	case GF_NETIO_WAIT_FOR_REPLY:
		if (sess->server_mode) {
			http_send_headers(sess);
		} else {
			wait_for_header_and_parse(sess);
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
		http_parse_remaining_body(sess);
		break;
	default:
		break;
	}
}

GF_EXPORT
void gf_dm_sess_set_max_rate(GF_DownloadSession *sess, u32 max_rate)
{
	if (sess) {
		sess->max_data_rate = max_rate/8;
		sess->rate_regulated = GF_FALSE;
	}
}
GF_EXPORT
u32 gf_dm_sess_get_max_rate(GF_DownloadSession *sess)
{
	return sess ? 8*sess->max_data_rate : 0;
}
GF_EXPORT
Bool gf_dm_sess_is_regulated(GF_DownloadSession *sess)
{
	return sess ? sess->rate_regulated : GF_FALSE;
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
	while (e == GF_OK) {
		e = gf_dm_sess_process(dnload);
		if (e && (e!=GF_IP_NETWORK_EMPTY))
			break;
		if (dnload->status>=GF_NETIO_DATA_TRANSFERED) break;
		e = GF_OK;
		if (dnload->connect_pending)
			gf_sleep(100);
	}
	if (e==GF_OK)
		gf_cache_set_content_length(dnload->cache_entry, dnload->total_size);
	e |= gf_cache_close_write_cache(dnload->cache_entry, dnload, (e == GF_OK) ? GF_TRUE : GF_FALSE);
	gf_fclose(f);

	if (redirected_url) {
		if (dnload->orig_url_before_redirect) *redirected_url = gf_strdup(dnload->orig_url);
	}
	gf_dm_sess_del(dnload);
	return e;
}

GF_EXPORT
const char *gf_dm_sess_get_resource_name(GF_DownloadSession *dnload)
{
	return dnload ? dnload->orig_url : NULL;
}



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
	count = gf_list_count(dm->all_sessions);

	for (i=0; i<count; i++) {
		GF_DownloadSession *sess = (GF_DownloadSession*)gf_list_get(dm->all_sessions, i);
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

GF_HTTPSessionType gf_dm_sess_is_hmux(GF_DownloadSession *sess)
{
#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		if (sess->hmux_sess->net_sess && sess->hmux_sess->net_sess->flags & GF_NETIO_SESSION_USE_QUIC)
			return GF_SESS_TYPE_HTTP3;
		if (sess->hmux_sess->net_sess && !sess->hmux_sess->net_sess->sock)
			return GF_SESS_TYPE_HTTP3;
		return GF_SESS_TYPE_HTTP2;
	}
#endif
	return GF_SESS_TYPE_HTTP;
}

Bool gf_dm_sess_use_tls(GF_DownloadSession * sess)
{
#ifdef GPAC_HAS_SSL
	if (sess->ssl)
		return GF_TRUE;
#endif

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess && (sess->hmux_sess->net_sess->flags & GF_NETIO_SESSION_USE_QUIC))
		return GF_TRUE;
#endif
	return GF_FALSE;
}

u32 gf_dm_sess_get_resource_size(GF_DownloadSession * sess)
{
	if (!sess) return 0;
	if (sess->full_resource_size) return sess->full_resource_size;
	return sess->total_size;
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


static GF_Err gf_dm_sess_flush_async_close(GF_DownloadSession *sess, Bool no_select, Bool for_close)
{
	if (!sess) return GF_OK;
	if (sess->status==GF_NETIO_STATE_ERROR) return sess->last_error;

	if (!no_select && sess->sock && (gf_sk_select(sess->sock, GF_SK_SELECT_WRITE)!=GF_OK)) {
		return sess->async_buf_size ? GF_IP_NETWORK_EMPTY : GF_OK;
	}
	GF_Err ret = GF_OK;

#ifdef GPAC_HTTPMUX
	if(sess->hmux_sess)
		ret = sess->hmux_sess->async_flush(sess, for_close);

	//if H2 flush the parent session holding the http2 session
	if (sess->hmux_sess)
		sess = sess->hmux_sess->net_sess;
#endif

	if (sess->async_buf_size) {
		GF_Err e = dm_sess_write(sess, sess->async_buf, sess->async_buf_size);
		if (e) return e;
		if (sess->async_buf_size) return GF_IP_NETWORK_EMPTY;
	}
	return ret;
}

GF_Err gf_dm_sess_flush_async(GF_DownloadSession *sess, Bool no_select)
{
	return gf_dm_sess_flush_async_close(sess, no_select, GF_FALSE);
}
GF_Err gf_dm_sess_flush_close(GF_DownloadSession *sess)
{
	return gf_dm_sess_flush_async_close(sess, GF_TRUE, GF_TRUE);
}

u32 gf_dm_sess_async_pending(GF_DownloadSession *sess)
{
	if (!sess) return 0;
#ifdef GPAC_HTTPMUX
	if (sess->local_buf_len) return sess->local_buf_len;
	if (sess->hmux_sess)
		sess = sess->hmux_sess->net_sess;
#endif
	return sess ? sess->async_buf_size : 0;
}

GF_EXPORT
GF_Err gf_dm_sess_send(GF_DownloadSession *sess, u8 *data, u32 size)
{
	GF_Err e = GF_OK;

#ifdef GPAC_HTTPMUX
	if (sess->hmux_sess) {
		return hmux_send_payload(sess, data, size);
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
	//FOR TEST ONLY
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


#endif //GPAC_DISABLE_NETWORK
