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

#if !defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_CURL)

#include <curl/curl.h>
//we need multi API
#if CURL_AT_LEAST_VERSION(7,80,0)
#ifndef CURLPIPE_MULTIPLEX
#define CURLPIPE_MULTIPLEX 0
#endif
#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#pragma comment(lib, "libcurl")
# endif
#endif


#ifndef GPAC_DISABLE_LOG
static int curl_trace(CURL *handle, curl_infotype type, char *data, size_t size, void *clientp)
{
	(void)clientp;
	(void)handle;

	switch(type) {
	case CURLINFO_TEXT:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] Info: %s", data));
		return 0;
	case CURLINFO_HEADER_OUT:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] Send header %s\n", data));
		return 0;
	case CURLINFO_DATA_OUT:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] Send data\n"));
		return 0;
	case CURLINFO_SSL_DATA_OUT:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] Send SSL data\n"));
		return 0;
	case CURLINFO_HEADER_IN:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] Recv header %s\n", data));
		return 0;
	case CURLINFO_DATA_IN:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] Recv data %d byes\n", size));
		return 0;
	case CURLINFO_SSL_DATA_IN:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] Recv SSL data\n"));
		return 0;
	default:
		return 0;
	}
	return 0;
}
#endif

static int curl_on_connect(void *clientp, char *conn_primary_ip, char *conn_local_ip, int conn_primary_port, int conn_local_port)
{
	GF_DownloadSession *sess = clientp;
	if (sess->status == GF_NETIO_SETUP) {
		sess->status = GF_NETIO_CONNECTED;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] connected\n"));
		gf_dm_sess_notify_state(sess, GF_NETIO_CONNECTED, GF_OK);
		gf_dm_configure_cache(sess);
		gf_mx_p(sess->dm->curl_mx);
		curl_multi_remove_handle(sess->dm->curl_multi, sess->curl_hnd);
		sess->curl_hnd_registered = GF_FALSE;
		gf_mx_v(sess->dm->curl_mx);
	}
	return CURL_PREREQFUNC_OK;
}

static int curl_on_socket_close(void *clientp, curl_socket_t item)
{
	GF_DownloadManager *dm = clientp;
	dm->curl_has_close = GF_TRUE;
	return 0;
}

static size_t curl_on_header(char *buffer, size_t size, size_t nitems, void *clientp)
{
	GF_DownloadSession *sess = clientp;
	if ((buffer[0]=='\r') || (buffer[0]=='\n'))
		return nitems * size;

	if (sess->status==GF_NETIO_WAIT_FOR_REPLY) {
		//push headers
		char *sep = memchr(buffer, ':', nitems*size);
		if (sep) {
			sep[0] = 0;
			GF_HTTPHeader *hdrp;
			GF_SAFEALLOC(hdrp, GF_HTTPHeader);
			if (hdrp) {
				u32 len = (u32) strlen(buffer) + 2;
				hdrp->name = gf_strdup(buffer);
				hdrp->value = gf_malloc(nitems * size - len + 1);
				memcpy(hdrp->value, buffer+len, nitems * size - len);
				hdrp->value[nitems * size - len] = 0;
				gf_list_add(sess->headers, hdrp);
			}
		}
		if (!sess->reply_time)
			sess->reply_time = (u32) (gf_sys_clock_high_res() - sess->request_start_time);
	}
	return nitems * size;
}

static size_t curl_on_data(char *ptr, size_t size, size_t nmemb, void *clientp)
{
	GF_DownloadSession *sess = clientp;
	u32 len = (u32) (size*nmemb);
	if (sess->local_buf_len + len > sess->local_buf_alloc) {
		sess->local_buf_alloc = sess->local_buf_len + len;
		sess->local_buf = gf_realloc(sess->local_buf, sizeof(u8) * sess->local_buf_alloc);
		if (!sess->local_buf) return 0;
	}
	memcpy(sess->local_buf + sess->local_buf_len, ptr, len);
	sess->local_buf_len += (u32) len;
	sess->last_fetch_time = gf_sys_clock_high_res();
	//dirty hack: libcurl may use blocking calls for some protocols (ftp)
	//if we know the total size and have all bytes, force a timeout and tach it in curl_flush
	if (sess->total_size && (sess->bytes_done + sess->local_buf_len == sess->total_size)) {
		curl_easy_setopt(sess->curl_hnd, CURLOPT_TIMEOUT_MS, 1);
	}
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] sess %s received %d bytes (%d/%d total)\n", sess->orig_url, len, sess->bytes_done, sess->total_size));
	return len;
}
#endif //GPAC_DISABLE_LOG

GF_Err curl_setup_session(GF_DownloadSession* sess)
{
	CURLcode res;
	sess->curl_hnd = curl_easy_init();
	res = curl_easy_setopt(sess->curl_hnd, CURLOPT_PRIVATE, sess);
	res = curl_easy_setopt(sess->curl_hnd, CURLOPT_URL, sess->orig_url);

#ifndef GPAC_DISABLE_LOG
	if (gf_opts_get_bool("curl", "trace") && gf_log_tool_level_on(GF_LOG_HTTP, GF_LOG_DEBUG)) {
		curl_easy_setopt(sess->curl_hnd, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(sess->curl_hnd, CURLOPT_DEBUGFUNCTION, curl_trace);
		curl_easy_setopt(sess->curl_hnd, CURLOPT_DEBUGDATA, sess);
	}
#endif

	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_BUFFERSIZE, sess->dm->read_buf_size);
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_FOLLOWLOCATION, 0L);
#if (CURLPIPE_MULTIPLEX > 0)
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_PIPEWAIT, 1L);
#endif
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_PREREQFUNCTION, curl_on_connect);
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_PREREQDATA, sess);
	//set close callback at DM level since a same socket can be used by several sessions (H2/H3)
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_CLOSESOCKETFUNCTION, curl_on_socket_close);
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_CLOSESOCKETDATA, sess->dm);

	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_HEADERFUNCTION, curl_on_header);
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_HEADERDATA, sess);
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_WRITEFUNCTION, curl_on_data);
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_WRITEDATA, sess);
	//DO NOT SET CURLOPT_TIMEOUT as we have no clue how long the session will be running
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_CONNECTTIMEOUT_MS, sess->conn_timeout);
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_ACCEPTTIMEOUT_MS, sess->conn_timeout);
	if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_NOSIGNAL, 1);
	if (gf_opts_get_bool("core", "broken-cert")) {
		if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_SSL_VERIFYPEER, 0);
		if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_SSL_VERIFYHOST, 0);
	}
	else {

		const char* ca_bundle = gf_opts_get_key("core", "ca-bundle");

		if (ca_bundle && gf_file_exists(ca_bundle)) {
			curl_easy_setopt(sess->curl_hnd, CURLOPT_CAINFO, ca_bundle);
			curl_easy_setopt(sess->curl_hnd, CURLOPT_CAPATH, NULL);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[CURL] Using CA bundle at %s\n", ca_bundle));
		}
#if CURL_AT_LEAST_VERSION(7,84,0)
		else {
			const char* curl_bundle = NULL;
			curl_easy_getinfo(sess->curl_hnd, CURLINFO_CAINFO, &curl_bundle);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[CURL] libcurl CA bundle location: %s\n", curl_bundle));

			if (!curl_bundle || !gf_file_exists(curl_bundle)) {

				const char* ca_bundle_default = gf_opts_get_key("core", "ca-bundle-default");

				if (ca_bundle_default) {
					curl_easy_setopt(sess->curl_hnd, CURLOPT_CAINFO, ca_bundle_default);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[CURL] using default CA bundle at %s\n", ca_bundle_default));
				}

			}
		}
#endif
	}

	//set HTTP version
	if (!res) {
		curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);
		res = CURLE_UNSUPPORTED_PROTOCOL;
		//try H3
		if ((ver->features & CURL_VERSION_HTTP3) && sess->dm && (sess->dm->h3_mode!=H3_MODE_NO)) {
			if (sess->dm->h3_mode==H3_MODE_ONLY)
				res = curl_easy_setopt(sess->curl_hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_3ONLY);
			else
				res = curl_easy_setopt(sess->curl_hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_3);
		}
		//try H2
		if (res && (ver->features & CURL_VERSION_HTTP2) && !gf_opts_get_bool("core", "no-h2")) {
			res = curl_easy_setopt(sess->curl_hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2_0);
		}
		//fallback 1.1 - we could use CURL_HTTP_VERSION_NONE
		if (res==CURLE_UNSUPPORTED_PROTOCOL)
			res = curl_easy_setopt(sess->curl_hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
	}
	const char *proxy = (sess->flags & GF_NETIO_SESSION_NO_PROXY) ? NULL : gf_opts_get_key("core", "proxy");
	if (proxy) {
		sess->proxy_enabled = 1;
		if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_PROXY, proxy);
	}

	sess->curl_not_http = GF_FALSE;
	if (!sess->do_requests) {
		sess->curl_not_http = GF_TRUE;
		sess->do_requests = http_do_requests;
		if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_FOLLOWLOCATION, 1L);
		if (!res) res = curl_easy_setopt(sess->curl_hnd, CURLOPT_TCP_KEEPALIVE, 1L);
		if (sess->log_name) {
			gf_free(sess->log_name);
			sess->log_name = NULL;
		}
		sess->log_name = gf_strdup("CURL");
	}

	//set user-defined options
	const struct curl_easyoption *opt = curl_easy_option_next(NULL);
	while (opt) {
		u64 llvalue;
		s32 value;
		const char *val = gf_opts_get_key("curl", opt->name);
		if (!val) {
			opt = curl_easy_option_next(opt);
			continue;
		}
		switch (opt->type) {
		case CURLOT_LONG:
		case CURLOT_VALUES:
			sscanf(val, "%d", &value);
			if (!res) res = curl_easy_setopt(sess->curl_hnd, opt->id, value);
			break;
		case CURLOT_OFF_T:
			sscanf(val, LLU, &llvalue);
			if (!res) res = curl_easy_setopt(sess->curl_hnd, opt->id, llvalue);
			break;
		case CURLOT_STRING:
			if (!res) res = curl_easy_setopt(sess->curl_hnd, opt->id, val);
			break;
		case CURLOT_OBJECT:
		case CURLOT_SLIST:
		case CURLOT_CBPTR:
		case CURLOT_BLOB:
		case CURLOT_FUNCTION:
			break;
		}
		opt = curl_easy_option_next(opt);
	}

	if (res) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[CURL] Failed to setup session: %s\n", curl_easy_strerror(res)));
		return GF_SERVICE_ERROR;
	}

	sess->status = GF_NETIO_SETUP;
	sess->last_error = GF_OK;
	//reset number of retry and start time
	sess->num_retry = SESSION_RETRY_COUNT;
	sess->start_time = 0;
	sess->reply_time = 0;
	sess->local_buf_len = 0;
	return GF_OK;
}


void curl_flush(GF_DownloadSession *sess)
{
	gf_mx_p(sess->dm->curl_mx);
	if (!sess->curl_hnd_registered) {
		curl_multi_add_handle(sess->dm->curl_multi, sess->curl_hnd);
		sess->curl_hnd_registered = GF_TRUE;
	}
	int still_running = 0;
	CURLMcode res = curl_multi_perform(sess->dm->curl_multi, &still_running);
	gf_mx_v(sess->dm->curl_mx);
	if (res != CURLM_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[CURL] curl_multi_perform() failed: %s\n", curl_multi_strerror(res) ));
	}

	//if no close detected and session has total size set nothing to do
	//otherwise check for EOS detection, either error (close) or OK (chunk transfer)
	if (!sess->dm->curl_has_close && sess->total_size) return;
	//process close messages
	sess->dm->curl_has_close = GF_FALSE;
	CURLMsg *m=NULL;
	int nb_msgs=0;
	while (1) {
		m = curl_multi_info_read(sess->dm->curl_multi, &nb_msgs);
		if (!m) break;
		if (m->msg != CURLMSG_DONE) continue;
		GF_DownloadSession *a_sess = NULL;
		res = curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &a_sess);
		if (!a_sess) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[CURL] failed to retrieve private pointer: %s\n", curl_multi_strerror(res) ));
			continue;
		}
		if (a_sess->curl_hnd_registered) {
			curl_multi_remove_handle(sess->dm->curl_multi, a_sess->curl_hnd);
			a_sess->curl_hnd_registered = GF_FALSE;
		}
		if (m->data.result==CURLE_COULDNT_CONNECT)
			a_sess->curl_closed = GF_IP_CONNECTION_FAILURE;
		else if (m->data.result==CURLE_COULDNT_RESOLVE_HOST)
			a_sess->curl_closed = GF_IP_ADDRESS_NOT_FOUND;
		else if (m->data.result==CURLE_PARTIAL_FILE)
			a_sess->curl_closed = GF_IP_CONNECTION_CLOSED;
		else if (m->data.result==CURLE_REMOTE_FILE_NOT_FOUND)
			a_sess->curl_closed = GF_URL_ERROR;
		else if (m->data.result==CURLE_OK)
			a_sess->curl_closed = GF_EOS;
		else if (m->data.result==CURLE_REMOTE_ACCESS_DENIED) {
			if (a_sess->num_retry) {
				a_sess->curl_fake_hcode = 401;
				a_sess->num_retry--;
				a_sess->last_error = GF_OK;
				a_sess->status = GF_NETIO_WAIT_FOR_REPLY;
				a_sess->curl_closed = GF_OK;
			} else {
				a_sess->curl_closed = GF_AUTHENTICATION_FAILURE;
			}
		}
		else if (m->data.result==CURLE_OPERATION_TIMEDOUT) {
			//timeout was forced
			if (a_sess->total_size && (a_sess->total_size == a_sess->local_buf_len + a_sess->bytes_done))
				a_sess->curl_closed = GF_EOS;
			else
				a_sess->curl_closed = GF_IP_CONNECTION_FAILURE;
		}
		else
			a_sess->curl_closed = GF_IP_NETWORK_FAILURE;
	}
}

void curl_destroy(GF_DownloadSession *sess)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[CURL] closing session\n"));
	if (sess->curl_hnd_registered) {
		gf_mx_p(sess->dm->curl_mx);
		curl_multi_remove_handle(sess->dm->curl_multi, sess->curl_hnd);
		gf_mx_v(sess->dm->curl_mx);
	}
	curl_easy_cleanup(sess->curl_hnd);
	sess->curl_hnd = NULL;
	sess->curl_hnd_registered = GF_FALSE;
	if (sess->curl_hdrs) curl_slist_free_all(sess->curl_hdrs);
	sess->curl_hdrs = NULL;
	sess->curl_closed = GF_OK;
}

Bool curl_can_handle_url(const char *url)
{
	curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);
	char *prot_sep = strstr(url, "://");
	if (!prot_sep) return GF_FALSE;
	u32 i=0, len = (u32) (prot_sep - url);
	while (ver && ver->protocols[i]) {
		if (!strnicmp(ver->protocols[i], url, len))
			return GF_TRUE;
		i++;
	}
	return GF_FALSE;
}

GF_Err curl_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read)
{
	if (!sess->curl_hnd) return GF_BAD_PARAM;

	if (!sess->local_buf_len)
		curl_flush(sess);

	if (!sess->local_buf_len) {
		*out_read = 0;
		if (sess->curl_closed==GF_EOS) {
			//for chunk transfer
			if (!sess->total_size)
				sess->total_size = sess->bytes_done;
			//reutrn OK so that data_received is processed
			return GF_OK;
		}
		else if (sess->curl_closed) {
			sess->status = GF_NETIO_STATE_ERROR;
			SET_LAST_ERR(GF_IP_CONNECTION_CLOSED);
			return sess->curl_closed;
		}
		return GF_IP_NETWORK_EMPTY;
	}
	if (sess->curl_not_http && !sess->total_size) {
		long cl=0;
		curl_easy_getinfo(sess->curl_hnd, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
		if (cl>=0)
			sess->total_size = cl;
	}
	u32 nb_copy = MIN(data_size, sess->local_buf_len);
	memcpy(data, sess->local_buf, nb_copy);
	sess->local_buf_len -= nb_copy;
	memmove(sess->local_buf, sess->local_buf + nb_copy, sess->local_buf_len);
	*out_read = nb_copy;
	return GF_OK;
}

void curl_connect(GF_DownloadSession*sess)
{
	curl_flush(sess);
	if (sess->curl_closed) {
		if (sess->cache_entry && sess->cached_file) return;

		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR(GF_IP_CONNECTION_FAILURE)
		gf_dm_sess_notify_state(sess, sess->status, GF_IP_CONNECTION_FAILURE);
	}
}

GF_Err curl_process_reply(GF_DownloadSession *sess, u32 *ContentLength)
{
	if (!sess->reply_time) {
		curl_flush(sess);
		if (!sess->reply_time) return GF_IP_NETWORK_EMPTY;
	}
	if (sess->curl_closed<0) {
		gf_dm_disconnect(sess, HTTP_RESET_CONN);
		sess->num_retry--;
		if (sess->num_retry) {
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[%s] Connection closed by server when processing %s - retrying\n", sess->log_name, sess->remote_path));
			sess->status = GF_NETIO_SETUP;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[%s] Connection closed by server when processing %s - aborting\n", sess->log_name, sess->remote_path));
			SET_LAST_ERR(GF_IP_CONNECTION_CLOSED)
			sess->status = GF_NETIO_STATE_ERROR;
		}
		return GF_IP_CONNECTION_CLOSED;
	}

	if (sess->curl_not_http) {
		//if not HTTP we have no guarantee that the return code for the URL is known when receiving server first bytes
		//eg FTP may need extra round trips before we get OK/ not found.
		//We assume by default OK and will throw an error in curl_flush
		// curl_fake_hcode can be set in curl_flush to force reprocessing the reply (eg on authentication failure)
		sess->rsp_code = sess->curl_fake_hcode ? sess->curl_fake_hcode : 200;
		sess->curl_fake_hcode = 0;
		long cl=0;
		curl_easy_getinfo(sess->curl_hnd, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
		if (cl>=0)
			*ContentLength = cl;
	} else {
		long rsp=505;
		curl_easy_getinfo(sess->curl_hnd, CURLINFO_RESPONSE_CODE, &rsp);
		sess->rsp_code = rsp;
		if (gf_log_tool_level_on(GF_LOG_HTTP, GF_LOG_INFO)) {
			long http_version;
			const char *version="unknown";
			curl_easy_getinfo(sess->curl_hnd, CURLINFO_HTTP_VERSION, &http_version);
			switch (http_version) {
			case CURL_HTTP_VERSION_1_0: version = "1.0"; break;
			case CURL_HTTP_VERSION_1_1: version = "1.1"; break;
			case CURL_HTTP_VERSION_2_0: version = "2"; break;
			case CURL_HTTP_VERSION_3: version = "3"; break;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[CURL] Using HTTP %s\n", version));
		}
	}
	sess->start_time = gf_sys_clock_high_res();
	sess->start_time_utc = gf_net_get_utc();
	return GF_OK;
}

#endif //GPAC_DISABLE_NETWORK && HAS_CURL

