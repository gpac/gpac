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

#if !defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_HTTP2)

#include <nghttp2/nghttp2.h>

#define HTTP2_BUFFER_SETTINGS_SIZE 128

//link for msvc
#if !defined(__GNUC__) && ( defined(_WIN32_WCE) || defined (WIN32) )
#pragma comment(lib, "nghttp2")
#endif


static void h2_flush_send_ex(GF_DownloadSession *sess, Bool flush_local_buf);

static int h2_header_callback(nghttp2_session *session,
								const nghttp2_frame *frame, const uint8_t *name,
								size_t namelen, const uint8_t *value,
								size_t valuelen, uint8_t flags ,
								void *user_data)
{
	GF_DownloadSession *sess;

	switch (frame->hd.type) {
	case NGHTTP2_HEADERS:
		sess = hmux_get_session(user_data, frame->hd.stream_id, GF_FALSE);
		if (!sess)
			return NGHTTP2_ERR_CALLBACK_FAILURE;
		if (
			(!sess->server_mode && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE))
			|| (sess->server_mode && ((frame->headers.cat == NGHTTP2_HCAT_HEADERS) || (frame->headers.cat == NGHTTP2_HCAT_REQUEST)))
		) {
			GF_HTTPHeader *hdrp;

			GF_SAFEALLOC(hdrp, GF_HTTPHeader);
			if (hdrp) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id "LLD" got header %s: %s\n", sess->hmux_stream_id, name, value));
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
	GF_DownloadSession *sess = hmux_get_session(user_data, frame->hd.stream_id, GF_TRUE);
	if (!sess) {
		GF_HMUX_Session *h2sess = (GF_HMUX_Session *)user_data;
		GF_DownloadSession *par_sess = h2sess->net_sess;
		if (!par_sess || !par_sess->server_mode)
			return NGHTTP2_ERR_CALLBACK_FAILURE;


		if (par_sess->user_proc) {
			GF_NETIO_Parameter param;
			memset(&param, 0, sizeof(GF_NETIO_Parameter));
			param.msg_type = GF_NETIO_REQUEST_SESSION;
			par_sess->in_callback = GF_TRUE;
			param.sess = par_sess;
			param.stream_id = frame->hd.stream_id;
			par_sess->user_proc(par_sess->usr_cbk, &param);
			par_sess->in_callback = GF_FALSE;

			if (param.error == GF_OK) {
				sess = hmux_get_session(user_data, frame->hd.stream_id, GF_FALSE);
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
		sess = hmux_get_session(user_data, frame->hd.stream_id, GF_FALSE);
		if (!sess)
			return NGHTTP2_ERR_CALLBACK_FAILURE;

		if (
			(!sess->server_mode && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE))
			|| (sess->server_mode && ((frame->headers.cat == NGHTTP2_HCAT_HEADERS) || (frame->headers.cat == NGHTTP2_HCAT_REQUEST)))
		) {
			sess->hmux_headers_seen = 1;
			if (sess->server_mode) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] All headers received for stream ID "LLD"\n", sess->hmux_stream_id));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] All headers received for stream ID "LLD"\n", sess->hmux_stream_id));
			}
		}
		if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) data done\n", frame->hd.stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
			sess->hmux_data_done = 1;
			sess->hmux_is_eos = 0;
			sess->hmux_send_data = NULL;
			sess->hmux_send_data_len = 0;
		}
		break;
	case NGHTTP2_DATA:
		if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
			sess = hmux_get_session(user_data, frame->hd.stream_id, GF_FALSE);
			//if no session with such ID this means we got all our bytes and considered the session done, do not throw an error
			if (sess) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) data done\n", frame->hd.stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
				sess->hmux_data_done = 1;
			}
		}
		break;
	case NGHTTP2_RST_STREAM:
		sess = hmux_get_session(user_data, frame->hd.stream_id, GF_FALSE);
		// cancel from remote peer, signal if not done
		if (sess && sess->server_mode && !sess->hmux_is_eos) {
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
	GF_DownloadSession *sess = hmux_get_session(user_data, stream_id, GF_FALSE);
	if (!sess)
		return NGHTTP2_ERR_CALLBACK_FAILURE;

	if (sess->hmux_buf.size + len > sess->hmux_buf.alloc) {
		sess->hmux_buf.alloc = sess->hmux_buf.size + (u32) len;
		sess->hmux_buf.data = gf_realloc(sess->hmux_buf.data, sizeof(u8) * sess->hmux_buf.alloc);
		if (!sess->hmux_buf.data)	return NGHTTP2_ERR_NOMEM;
	}
	memcpy(sess->hmux_buf.data + sess->hmux_buf.size, data, len);
	sess->hmux_buf.size += (u32) len;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id "LLD" received %d bytes (%d/%d total) - flags %d\n", sess->hmux_stream_id, len, sess->hmux_buf.size, sess->total_size, flags));
	return 0;
}

static int h2_stream_close_callback(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data)
{
	Bool do_retry = GF_FALSE;
	GF_DownloadSession *sess = hmux_get_session(user_data, stream_id, GF_FALSE);
	if (!sess)
		return 0;

	gf_mx_p(sess->mx);

	if (error_code==NGHTTP2_REFUSED_STREAM)
		do_retry = GF_TRUE;
	else if (sess->hmux_sess->do_shutdown && !sess->server_mode && !sess->bytes_done)
		do_retry = GF_TRUE;

	if (do_retry) {
		sess->hmux_sess->do_shutdown = GF_TRUE;
		sess->hmux_switch_sess = GF_TRUE;
		sess->status = GF_NETIO_SETUP;
		SET_LAST_ERR(GF_OK)
		gf_mx_v(sess->mx);
		return 0;
	}

	if (error_code) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) closed with error_code=%d\n", stream_id, sess->remote_path ? sess->remote_path : sess->orig_url, error_code));
		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR(GF_IP_NETWORK_FAILURE)
		sess->put_state = 0;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] stream_id %d (%s) closed (put state %d)\n", stream_id, sess->remote_path ? sess->remote_path : sess->orig_url, sess->put_state));
		//except for PUT/POST, keep status in DATA_EXCHANGE as this frame might have been pushed while processing another session

		if (sess->put_state) {
			sess->put_state = 0;
			sess->status = GF_NETIO_DATA_TRANSFERED;
			sess->last_error = GF_OK;
		}
	}

	//stream closed
	sess->hmux_stream_id = -1;
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
	GF_HMUX_Session *h2sess = (GF_HMUX_Session *)user_data;
	GF_DownloadSession *sess = h2sess->net_sess;

	return h2_write_data(sess, data, length);
}

static int h2_before_frame_send_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
	GF_DownloadSession *sess = hmux_get_session(user_data, frame->hd.stream_id, GF_FALSE);
	if (!sess)
		return 0;
	sess->hmux_ready_to_send = 1;
	return 0;
}


ssize_t h2_data_source_read_callback(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source, void *user_data)
{
	GF_DownloadSession *sess = (GF_DownloadSession *) source->ptr;
	if (!sess)
		return NGHTTP2_ERR_EOF;

	if (!sess->hmux_send_data_len) {
		sess->hmux_send_data = NULL;
		if (!sess->local_buf_len && sess->hmux_is_eos) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] send EOS for stream_id "LLD"\n", sess->hmux_stream_id));
			*data_flags = NGHTTP2_DATA_FLAG_EOF;
			sess->hmux_is_eos = 0;
			return 0;
		}
		sess->hmux_data_paused = 1;
		return NGHTTP2_ERR_DEFERRED;
	}

	if (sess->hmux_sess->copy) {
		u32 copy = (sess->hmux_send_data_len > length) ? (u32) length : sess->hmux_send_data_len;
		memcpy(buf, sess->hmux_send_data, copy);
		sess->hmux_send_data += copy;
		sess->hmux_send_data_len -= copy;
		return copy;
	}

	*data_flags = NGHTTP2_DATA_FLAG_NO_COPY;
	if (sess->hmux_send_data_len > length) {
		return length;
	}
	return sess->hmux_send_data_len;
}


static int h2_send_data_callback(nghttp2_session *session, nghttp2_frame *frame, const uint8_t *framehd, size_t length, nghttp2_data_source *source, void *user_data)
{
	char padding[256];
	ssize_t rv;
	GF_DownloadSession *sess = (GF_DownloadSession *) source->ptr;
	if (!sess)
		return NGHTTP2_ERR_EOF;

	gf_assert(sess->hmux_send_data_len);
	gf_assert(sess->hmux_send_data_len >= length);

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
		rv = h2_write_data(sess, (u8 *) sess->hmux_send_data, length);
		if (rv<0) {
			if (rv==NGHTTP2_ERR_WOULDBLOCK) continue;
			goto err;
		}
		break;
	}
	sess->hmux_send_data += (u32) length;
	sess->hmux_send_data_len -= (u32) length;
	return 0;
err:

	sess->status = GF_NETIO_STATE_ERROR;
	SET_LAST_ERR(sess->last_error)
	return (int) rv;
}

static GF_Err h2_session_write(GF_DownloadSession *sess)
{
	gf_assert(sess->hmux_sess);
	int rv = nghttp2_session_send(sess->hmux_sess->hmux_udta);
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

static void h2_stream_reset(GF_DownloadSession *sess, Bool is_abort)
{
	nghttp2_submit_rst_stream(sess->hmux_sess->hmux_udta, NGHTTP2_FLAG_NONE, sess->hmux_stream_id, is_abort ? NGHTTP2_CANCEL : NGHTTP2_NO_ERROR);
}

static GF_Err h2_resume_stream(GF_DownloadSession *sess)
{
	sess->hmux_data_paused = 0;
	nghttp2_session_resume_data(sess->hmux_sess->hmux_udta, sess->hmux_stream_id);
	return GF_OK;
}

static void h2_destroy(struct _http_mux_session *hmux_sess)
{
	nghttp2_session_del(hmux_sess->hmux_udta);
}

static GF_Err h2_data_received(GF_DownloadSession *sess, const u8 *data, u32 nb_bytes)
{
	if (nb_bytes) {
		ssize_t read_len = nghttp2_session_mem_recv(sess->hmux_sess->hmux_udta, data, nb_bytes);
		if (read_len < 0 ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] nghttp2_session_mem_recv error:  %s\n", nghttp2_strerror((int) read_len)));
			return GF_IO_ERR;
		}
	}
	/* send pending frames - hmux_sess may be NULL at this point if the connection was reset during processing of the above
		this typically happens if we have a refused stream
	*/
	h2_session_write(sess);
	return GF_OK;
}


#define NV_HDR(_hdr, _name, _value) { \
		_hdr.name = (uint8_t *)_name;\
		_hdr.value = (uint8_t *)_value;\
		_hdr.namelen = (u32) strlen(_name);\
		_hdr.valuelen = (u32) strlen(_value);\
		_hdr.flags = NGHTTP2_NV_FLAG_NONE;\
	}

GF_Err h2_submit_request(GF_DownloadSession *sess, char *req_name, const char *url, const char *param_string, Bool has_body)
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
		nghttp2_data_provider *data_io = (nghttp2_data_provider*)sess->hmux_priv;
		gf_assert(data_io->read_callback);
		gf_assert(data_io->source.ptr != NULL);
	}
	sess->hmux_data_done = 0;
	sess->hmux_headers_seen = 0;
	sess->hmux_stream_id = nghttp2_submit_request(sess->hmux_sess->hmux_udta, NULL, hdrs, nb_hdrs+4,
		has_body ? (nghttp2_data_provider *)sess->hmux_priv : NULL, sess);
	sess->hmux_ready_to_send = 0;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_HTTP, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] send request (has_body %d) for new stream_id "LLD":\n", has_body, sess->hmux_stream_id));
		for (i=0; i<nb_hdrs+4; i++) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("\t%s: %s\n", hdrs[i].name, hdrs[i].value));
		}
	}
#endif

	gf_free(hdrs);
	gf_free(hostport);
	if (path) gf_free(path);

	if (sess->hmux_stream_id < 0) {
		return GF_IP_NETWORK_FAILURE;
	}

	return GF_OK;
}



GF_Err h2_send_reply(GF_DownloadSession *sess, u32 reply_code, const char *response_body, u32 body_len, Bool no_body)
{
	u32 i, count;
	char szFmt[50];
	if (!sess || !sess->server_mode) return GF_BAD_PARAM;

	count = gf_list_count(sess->headers);


	nghttp2_nv *hdrs;

	if (response_body) {
		no_body = GF_FALSE;
		sess->hmux_send_data = (u8 *) response_body;
		sess->hmux_send_data_len = body_len;
		sess->hmux_is_eos = 1;
	} else if (!no_body) {
		switch (reply_code) {
		case 200:
		case 206:
			no_body = GF_FALSE;
			sess->hmux_is_eos = 0;
			break;
		default:
			no_body = GF_TRUE;
			break;
		}
	}

	hdrs = gf_malloc(sizeof(nghttp2_nv) * (count + 1) );

	sprintf(szFmt, "%d", reply_code);
	NV_HDR(hdrs[0], ":status", szFmt);
	GF_LOG(GF_LOG_INFO, GF_LOG_HTTP, ("[HTTP/2] send reply for stream_id "LLD" (body %d) headers:\n:status: %s\n", sess->hmux_stream_id, !no_body, szFmt));
	for (i=0; i<count; i++) {
		GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
		NV_HDR(hdrs[i+1], hdr->name, hdr->value)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("%s: %s\n", hdr->name, hdr->value));
	}


	gf_mx_p(sess->mx);

	int rv = nghttp2_submit_response(sess->hmux_sess->hmux_udta, sess->hmux_stream_id, hdrs, count+1, no_body ? NULL : sess->hmux_priv);

	gf_free(hdrs);

	if (rv != 0) {
		gf_mx_v(sess->mx);
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] Failed to submit reply: %s\n", nghttp2_strerror(rv)));
		return GF_SERVICE_ERROR;
	}
	h2_session_write(sess);
	//in case we have a body already setup with this reply
	h2_flush_send_ex(sess, GF_FALSE);

	gf_mx_v(sess->mx);

	return GF_OK;
}

static GF_Err h2_async_flush(GF_DownloadSession *sess, Bool for_close)
{
	if (!sess->local_buf_len && !sess->hmux_is_eos) return GF_OK;
	gf_assert(sess->hmux_send_data==NULL);

	//HTTP2: resume data sending, set data to local copy send packets and check what was sent
	GF_Err ret = GF_OK;
	u8 was_eos = sess->hmux_is_eos;
	gf_assert(sess->hmux_send_data==NULL);
	gf_mx_p(sess->mx);

	sess->hmux_send_data = sess->local_buf;
	sess->hmux_send_data_len = sess->local_buf_len;
	if (sess->hmux_data_paused) {
		sess->hmux_data_paused = 0;
		sess->hmux_sess->resume(sess);
	}
	h2_flush_send_ex(sess, GF_TRUE);
	gf_mx_v(sess->mx);

	if (sess->hmux_send_data_len) {
		//we will return empty to avoid pushing data too fast, but we also need to flush the async buffer
		ret = GF_IP_NETWORK_EMPTY;
		if (sess->hmux_send_data_len<sess->local_buf_len) {
			memmove(sess->local_buf, sess->hmux_send_data, sess->hmux_send_data_len);
			sess->local_buf_len = sess->hmux_send_data_len;
		} else {
			//nothing sent, check socket
			ret = gf_sk_probe(sess->sock);
		}
		sess->hmux_send_data = NULL;
		sess->hmux_send_data_len = 0;
		sess->hmux_is_eos = was_eos;
	} else {
		sess->local_buf_len = 0;
		sess->hmux_send_data = NULL;
		sess->hmux_send_data_len = 0;
		if (was_eos && !sess->hmux_is_eos) {
			if (sess->put_state==1) {
				sess->put_state = 2;
				sess->status = GF_NETIO_WAIT_FOR_REPLY;
			}
		}
	}
	return ret;
}

static GF_Err h2_setup_session(GF_DownloadSession *sess, Bool is_destroy)
{
	if (is_destroy) {
		if (sess->hmux_priv) {
			gf_free(sess->hmux_priv);
			sess->hmux_priv = NULL;
		}
		return GF_OK;
	}
	if (!sess->hmux_priv) {
		GF_SAFEALLOC(sess->hmux_priv, nghttp2_data_provider);
		if (!sess->hmux_priv) return GF_OUT_OF_MEM;
	}
	nghttp2_data_provider *data_io = sess->hmux_priv;
	data_io->read_callback = h2_data_source_read_callback;
	data_io->source.ptr = sess;
	return GF_OK;
}

static void h2_flush_send_ex(GF_DownloadSession *sess, Bool flush_local_buf)
{
	char h2_flush[1024];
	u32 res;

	while (sess->hmux_send_data) {
		GF_Err e;
		u32 nb_bytes = sess->hmux_send_data_len;

		if (!sess->local_buf_len || flush_local_buf) {
			//read any frame pending from remote peer (window update and co)
			e = gf_dm_read_data(sess, h2_flush, 1023, &res);
			if ((e<0) && (e != GF_IP_NETWORK_EMPTY)) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(sess->last_error)
				break;
			}

			h2_session_write(sess);

			//error or regular eos
			if (sess->hmux_stream_id<0)
				break;
			if (sess->status==GF_NETIO_STATE_ERROR)
				break;
		}

		if (nb_bytes > sess->hmux_send_data_len) continue;
		if (!(sess->flags & GF_NETIO_SESSION_NO_BLOCK)) continue;

		if (flush_local_buf) return;

		//stream will now block, no choice but do a local copy...
		if (sess->local_buf_len + nb_bytes > sess->local_buf_alloc) {
			sess->local_buf_alloc = sess->local_buf_len + nb_bytes;
			sess->local_buf = gf_realloc(sess->local_buf, sess->local_buf_alloc);
			if (!sess->local_buf) {
				sess->status = GF_NETIO_STATE_ERROR;
				SET_LAST_ERR(sess->last_error)
				break;
			}
		}
		if (nb_bytes) {
			memcpy(sess->local_buf + sess->local_buf_len, sess->hmux_send_data, nb_bytes);
			sess->local_buf_len+=nb_bytes;
		}
		sess->hmux_send_data = NULL;
		sess->hmux_send_data_len = 0;
	}
}
static GF_Err h2_data_pending(GF_DownloadSession *sess)
{
	GF_Err e = h2_session_write(sess);
	if (e) return e;
	h2_flush_send_ex(sess, GF_FALSE);
	return GF_OK;
}
static void h2_close_session(GF_DownloadSession *sess)
{
	nghttp2_submit_shutdown_notice(sess->hmux_sess->hmux_udta);
	h2_session_write(sess);

#if 1
	u64 in_time;
	in_time = gf_sys_clock_high_res();
	while (nghttp2_session_want_read(sess->hmux_sess->hmux_udta)) {
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
		h2_session_write(sess);
	}
#endif
	sess->status = GF_NETIO_DISCONNECTED;
}

void h2_initialize_session(GF_DownloadSession *sess)
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

	GF_SAFEALLOC(sess->hmux_sess, GF_HMUX_Session)
	sess->hmux_sess->sessions = gf_list_new();
	sess->hmux_sess->copy = gf_opts_get_bool("core", "h2-copy");
	if (!sess->hmux_sess->copy)
		nghttp2_session_callbacks_set_send_data_callback(callbacks, h2_send_data_callback);

	if (sess->server_mode) {
		nghttp2_session_server_new((nghttp2_session**) &sess->hmux_sess->hmux_udta, callbacks, sess->hmux_sess);
	} else {
		nghttp2_session_client_new((nghttp2_session**) &sess->hmux_sess->hmux_udta, callbacks, sess->hmux_sess);
	}
	nghttp2_session_callbacks_del(callbacks);
	sess->hmux_sess->net_sess = sess;
	//setup function pointers
	sess->hmux_sess->submit_request = h2_submit_request;
	sess->hmux_sess->send_reply = h2_send_reply;
	sess->hmux_sess->write = h2_session_write;
	sess->hmux_sess->destroy = h2_destroy;
	sess->hmux_sess->stream_reset = h2_stream_reset;
	sess->hmux_sess->resume = h2_resume_stream;
	sess->hmux_sess->data_received = h2_data_received;
	sess->hmux_sess->setup_session = h2_setup_session;
	sess->hmux_sess->send_pending_data = h2_data_pending;
	sess->hmux_sess->async_flush = h2_async_flush;
	sess->hmux_sess->close_session = h2_close_session;

	sess->hmux_sess->connected = GF_TRUE;

	gf_list_add(sess->hmux_sess->sessions, sess);

	if (!sess->mx) {
		sprintf(szMXName, "http2_%p", sess->hmux_sess);
		sess->hmux_sess->mx = gf_mx_new(szMXName);
		sess->mx = sess->hmux_sess->mx;
	} else {
		sess->hmux_sess->mx = sess->mx;
	}
	sess->chunked = GF_FALSE;

	h2_setup_session(sess, GF_FALSE);

	if (sess->server_mode) {
		sess->hmux_stream_id = 1;
		if (sess->h2_upgrade_settings) {
			rv = nghttp2_session_upgrade2(sess->hmux_sess->hmux_udta, sess->h2_upgrade_settings, sess->h2_upgrade_settings_len, 0, sess);
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
	rv = nghttp2_submit_settings(sess->hmux_sess->hmux_udta, NGHTTP2_FLAG_NONE, iv, GF_ARRAY_LENGTH(iv));
	if (rv != 0) {
		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR((rv==NGHTTP2_ERR_NOMEM) ? GF_OUT_OF_MEM : GF_SERVICE_ERROR)
		return;
	}
	h2_session_write(sess);
}



void http2_set_upgrade_headers(GF_DownloadSession *sess)
{
	GF_HTTPHeader *hdr;
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
}

GF_Err http2_do_upgrade(GF_DownloadSession *sess, const char *body, u32 body_len)
{
	int rv;
	u8 settings[HTTP2_BUFFER_SETTINGS_SIZE];
	u32 settings_len;

	nghttp2_settings_entry h2_settings[2] = {
		{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
		{NGHTTP2_SETTINGS_ENABLE_PUSH, 0}
	};
	settings_len = (u32) nghttp2_pack_settings_payload(settings, HTTP2_BUFFER_SETTINGS_SIZE, h2_settings, GF_ARRAY_LENGTH(h2_settings));

	h2_initialize_session(sess);
	sess->hmux_stream_id = 1;
	rv = nghttp2_session_upgrade2(sess->hmux_sess->hmux_udta, settings, settings_len, (sess->http_read_type==1) ? 1 : 0, sess);
	if (rv < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] nghttp2_session_upgrade2 error: %s\n", nghttp2_strerror(rv)));
		return GF_IP_NETWORK_FAILURE;
	}
	//push the body
	if (body_len) {
		rv = (int) nghttp2_session_mem_recv(sess->hmux_sess->hmux_udta, body, body_len);
		if (rv < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/2] nghttp2_session_mem_recv error: %s\n", nghttp2_strerror(rv)));
			return GF_IP_NETWORK_FAILURE;
		}
		//stay in WAIT_FOR_REPLY state and do not flush data, cache is not fully configured yet
	}
	//send pending frames
	GF_Err e = h2_session_write(sess);
	if (e) return e;
	sess->connection_close = GF_FALSE;
	sess->h2_upgrade_state = 2;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[%s] Upgraded connection to HTTP/2\n", sess->log_name));
	return GF_OK;
}

GF_Err http2_check_upgrade(GF_DownloadSession *sess)
{
	GF_Err e = GF_OK;
	char *rsp_buf = NULL;
	if (!sess || !sess->server_mode) return GF_BAD_PARAM;
	if (!sess->h2_upgrade_settings) return GF_OK;

	u32 len;
	gf_assert(!sess->hmux_sess);
	gf_dynstrcat(&rsp_buf, "HTTP/1.1 101 Switching Protocols\r\n"
							"Connection: Upgrade\r\n"
							"Upgrade: h2c\r\n\r\n", NULL);

	len = (u32) strlen(rsp_buf);
	e = dm_sess_write(sess, rsp_buf, len);

	gf_free(rsp_buf);
	h2_initialize_session(sess);
	return e;
}

#endif //!defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_HTTP2)

