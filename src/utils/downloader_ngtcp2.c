/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2025-2025
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

/*
	Functions prefixes:
		h3_*: function used for hmux session function pointers
		ngq_*: function callback for ngtcp2 lib (quic)
		ngh3_*: function callback for nghttp3 lib
*/


#if !defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_NGTCP2)

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_quictls.h>
#include <nghttp3/nghttp3.h>

GF_Err gf_sk_bind_ex(GF_Socket *sock, const char *ifce_ip_or_name, u16 port, const char *peer_name, u16 peer_port, u32 options,
	u8 **dst_sock_addr, u32 *dst_sock_addr_len, u8 **src_sock_addr, u32 *src_sock_addr_len);


static u64 ngtcp2_timestamp()
{
	return 1000*gf_sys_clock_high_res();
}

typedef struct
{
	//keep parent
	GF_HMUX_Session *hmux;
	ngtcp2_path path;
	ngtcp2_conn *conn;
	ngtcp2_crypto_conn_ref conn_ref;
	nghttp3_conn *http_conn;
	ngtcp2_ccerr last_error;
	u32 handshake_state; //0: pending, 1: done, 2: error
} NGTCP2Priv;

static void ngq_rand_cb(uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx)
{
	size_t i;
	(void)rand_ctx;
	for (i = 0; i < destlen; ++i) {
		*dest = (uint8_t)random();
	}
}

static int ngq_get_new_connection_id_cb(ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token, size_t cidlen, void *user_data)
{
	(void)conn;
	(void)user_data;

	if (RAND_bytes(cid->data, (int)cidlen) != 1) {
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	cid->datalen = cidlen;
	if (RAND_bytes(token, NGTCP2_STATELESS_RESET_TOKENLEN) != 1) {
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	return 0;
}

static int ngq_extend_max_local_streams_bidi(ngtcp2_conn *conn, uint64_t max_streams,
                                  void *user_data)
{
	return 0;
}

static void ngq_printf(void *user_data, const char *format, ...)
{
  va_list ap;
  (void)user_data;

  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);

  fprintf(stderr, "\n");
}
static void ngq_debug_trace(const char *format, va_list args)
{
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
}
static void ngq_debug_trace_noop(const char *format, va_list args)
{
}

static nghttp3_ssize ngh3_data_source_read_callback(
  nghttp3_conn *conn, int64_t stream_id, nghttp3_vec *vec, size_t veccnt,
  uint32_t *pflags, void *conn_user_data, void *stream_user_data)
{
	//TODO
	return 0;
}

static GF_Err h3_setup_session(GF_DownloadSession *sess, Bool is_destroy)
{
	if (is_destroy) {
		if (sess->hmux_priv) {
			gf_free(sess->hmux_priv);
			sess->hmux_priv = NULL;
		}
		return GF_OK;
	}

	if (!sess->hmux_priv) {
		GF_SAFEALLOC(sess->hmux_priv, nghttp3_data_reader);
		if (!sess->hmux_priv) return GF_OUT_OF_MEM;
	}
	nghttp3_data_reader *data_io = sess->hmux_priv;
	data_io->read_data = ngh3_data_source_read_callback;
	return GF_OK;
}

static int ngh3_recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                   size_t datalen, void *conn_user_data, void *stream_user_data)
{
	NGTCP2Priv *qc = (NGTCP2Priv *)conn_user_data;
	GF_DownloadSession *sess = stream_user_data;

	ngtcp2_conn_extend_max_stream_offset(qc->conn, stream_id, datalen);
	ngtcp2_conn_extend_max_offset(qc->conn, datalen);

	if (!sess)
		return NGHTTP3_ERR_CALLBACK_FAILURE;

	if (sess->hmux_buf.size + datalen > sess->hmux_buf.alloc) {
		sess->hmux_buf.alloc = sess->hmux_buf.size + (u32) datalen;
		sess->hmux_buf.data = gf_realloc(sess->hmux_buf.data, sizeof(u8) * sess->hmux_buf.alloc);
		if (!sess->hmux_buf.data) return NGHTTP3_ERR_NOMEM;
	}
	memcpy(sess->hmux_buf.data + sess->hmux_buf.size, data, datalen);
	sess->hmux_buf.size += (u32) datalen;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/3] stream_id "LLU" received %d bytes (%d/%d total)\n", sess->hmux_stream_id, (u32) datalen, sess->hmux_buf.size, sess->total_size));
	return 0;
}

static int ngh3_deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                          size_t nconsumed, void *conn_user_data,
                          void *stream_user_data)
{
//	NGTCP2Priv *qc = (NGTCP2Priv *)user_data;
	//c->http_consume(stream_id, nconsumed);
	return 0;
}

static int ngh3_begin_headers(nghttp3_conn *conn, int64_t stream_id, void *conn_user_data,
                       void *stream_user_data)
{
	GF_DownloadSession *sess = stream_user_data;
	gf_dm_sess_clear_headers(sess);
	return 0;
}

static int ngh3_recv_header(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                     nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                     void *conn_user_data, void *stream_user_data)
{
	GF_DownloadSession *sess = stream_user_data;
	GF_HTTPHeader *hdrp;

	GF_SAFEALLOC(hdrp, GF_HTTPHeader);
	if (hdrp) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/3] stream_id %d got header %s: %s\n", sess->hmux_stream_id, name, value));

		nghttp3_vec v = nghttp3_rcbuf_get_buf(name);

		hdrp->name = gf_malloc(v.len+1);
		memcpy(hdrp->name,v.base,v.len+1);
		hdrp->name[v.len]=0;

		v = nghttp3_rcbuf_get_buf(value);
		hdrp->value = gf_malloc(v.len+1);
		memcpy(hdrp->value,v.base,v.len+1);
		hdrp->value[v.len]=0;

		gf_list_add(sess->headers, hdrp);
	}
	return 0;
}

static int ngh3_end_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
	void *conn_user_data, void *stream_user_data)
{
	GF_DownloadSession *sess = stream_user_data;
	sess->hmux_headers_seen = 1;
	if (sess->server_mode) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/3] All headers received for stream ID %d\n", sess->hmux_stream_id));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/3] All headers received for stream ID %d\n", sess->hmux_stream_id));
	}
	if (fin) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/3] stream_id "LLU" (%s) data done\n", stream_id, sess->remote_path ? sess->remote_path : sess->orig_url));
		sess->hmux_data_done = 1;
		sess->hmux_is_eos = GF_FALSE;
		sess->hmux_send_data = NULL;
		sess->hmux_send_data_len = 0;
	}
	return 0;
}

static int ngh3_begin_trailers(nghttp3_conn *conn, int64_t stream_id, void *conn_user_data, void *stream_user_data) {
	return 0;
}

static int ngh3_recv_trailer(nghttp3_conn *conn, int64_t stream_id, int32_t token,
	nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
	void *conn_user_data, void *stream_user_data)
{
	return 0;
}

static int ngh3_end_trailers(nghttp3_conn *conn, int64_t stream_id, int fin,
                      void *conn_user_data, void *stream_user_data)
{
  return 0;
}

static int ngh3_stop_sending(nghttp3_conn *conn, int64_t stream_id, uint64_t app_error_code,
	void *conn_user_data, void *stream_user_data)
{
	NGTCP2Priv *qc = (NGTCP2Priv *)conn_user_data;
	if (!qc->http_conn) return 0;
	int rv = nghttp3_conn_shutdown_stream_read(qc->http_conn, stream_id);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_shutdown_stream_read failed %s\n", nghttp3_strerror(rv) ));
		return NGHTTP3_ERR_CALLBACK_FAILURE;
	}
	//TODO : abort put/post or server mode
	return 0;
}

static int ngh3_reset_stream(nghttp3_conn *conn, int64_t stream_id, uint64_t app_error_code,
	void *conn_user_data, void *stream_user_data)
{
	NGTCP2Priv *qc = (NGTCP2Priv *)conn_user_data;
	if (!qc->http_conn) return 0;
	int rv = ngtcp2_conn_shutdown_stream_write(qc->conn, 0, stream_id, app_error_code);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_shutdown_stream_write failed %s\n", ngtcp2_strerror(rv) ));
		return NGHTTP3_ERR_CALLBACK_FAILURE;
	}
	return 0;
}

static int ngh3_stream_close(nghttp3_conn *conn, int64_t stream_id, uint64_t app_error_code,
	void *conn_user_data, void *stream_user_data)
{
	Bool do_retry = GF_FALSE;
	GF_DownloadSession *sess = stream_user_data;
	if (!sess)
		return 0;

	if (!ngtcp2_is_bidi_stream(stream_id)) {
		NGTCP2Priv *qc = (NGTCP2Priv *)conn_user_data;
		assert(!ngtcp2_conn_is_local_stream(qc->conn, stream_id));
		ngtcp2_conn_extend_max_streams_uni(qc->conn, 1);
	}

	gf_mx_p(sess->mx);

	if ((app_error_code==NGHTTP3_H3_EXCESSIVE_LOAD)
		|| (app_error_code==NGHTTP3_H3_REQUEST_REJECTED)
	) {
		do_retry = GF_TRUE;
	} else if (sess->hmux_sess->do_shutdown && !sess->server_mode && !sess->bytes_done) {
		do_retry = GF_TRUE;
	}

	if (do_retry) {
		sess->hmux_sess->do_shutdown = GF_TRUE;
		sess->hmux_switch_sess = GF_TRUE;
		sess->status = GF_NETIO_SETUP;
		SET_LAST_ERR(GF_OK)
		gf_mx_v(sess->mx);
		return 0;
	}

	if (app_error_code!=NGHTTP3_H3_NO_ERROR) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/3] stream_id "LLU" (%s) closed with error_code=%d\n", stream_id, sess->remote_path ? sess->remote_path : sess->orig_url, app_error_code));
		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR(GF_IP_NETWORK_FAILURE)
		sess->put_state = 0;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/3] stream_id "LLU" (%s) closed (put state)\n", stream_id, sess->remote_path ? sess->remote_path : sess->orig_url, sess->put_state));

		if (sess->put_state) {
			sess->put_state = 0;
			sess->status = GF_NETIO_DATA_TRANSFERED;
			sess->last_error = GF_OK;
		}
	}

	//stream closed
	sess->hmux_stream_id = 0;
	gf_mx_v(sess->mx);
	return 0;
}

static GF_Err h3_setup_http3(GF_HMUX_Session *hmux_sess)
{
	NGTCP2Priv *qc = (NGTCP2Priv *)hmux_sess->hmux_udta;
	if (qc->http_conn) return GF_OK;

	if (ngtcp2_conn_get_streams_uni_left(qc->conn) < 3) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] peer does not allow at least 3 unidirectional streams.\n"));
		return GF_REMOTE_SERVICE_ERROR;
	}

	nghttp3_callbacks callbacks = {
		.stream_close = ngh3_stream_close,
		.recv_data = ngh3_recv_data,
		.deferred_consume = ngh3_deferred_consume,
		.begin_headers = ngh3_begin_headers,
		.recv_header = ngh3_recv_header,
		.end_headers = ngh3_end_headers,
		.begin_trailers = ngh3_begin_trailers,
		.recv_trailer = ngh3_recv_trailer,
		.end_trailers = ngh3_end_trailers,
		.stop_sending = ngh3_stop_sending,
		.reset_stream = ngh3_reset_stream,
	};
	nghttp3_settings settings;
	nghttp3_settings_default(&settings);
	settings.qpack_max_dtable_capacity = 4096;
	settings.qpack_blocked_streams = 100;

	const nghttp3_mem *mem = nghttp3_mem_default();

	int rv = nghttp3_conn_client_new(&qc->http_conn, &callbacks, &settings, mem, hmux_sess->hmux_udta);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_client_new error %s\n", nghttp3_strerror(rv) ));
		return GF_SERVICE_ERROR;
	}

	s64 ctrl_stream_id;
	rv = ngtcp2_conn_open_uni_stream(qc->conn, &ctrl_stream_id, NULL);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_open_uni_stream: ", ngtcp2_strerror(rv) ));
		return GF_SERVICE_ERROR;
	}

	rv = nghttp3_conn_bind_control_stream(qc->http_conn, ctrl_stream_id);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_bind_control_stream error %s\n", nghttp3_strerror(rv) ));
		return GF_SERVICE_ERROR;
	}
	s64 qpack_enc_stream_id, qpack_dec_stream_id;
	rv = ngtcp2_conn_open_uni_stream(qc->conn, &qpack_enc_stream_id, NULL);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_open_uni_stream error %s\n", ngtcp2_strerror(rv) ));
		return GF_SERVICE_ERROR;
	}
	rv = ngtcp2_conn_open_uni_stream(qc->conn, &qpack_dec_stream_id, NULL);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_open_uni_stream error %s\n", ngtcp2_strerror(rv) ));
		return GF_SERVICE_ERROR;
	}
	rv = nghttp3_conn_bind_qpack_streams(qc->http_conn, qpack_enc_stream_id, qpack_dec_stream_id);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_bind_qpack_streams error %s\n", nghttp3_strerror(rv) ));
		return GF_SERVICE_ERROR;
	}
	return GF_OK;
}

#define NGH3_HDR(_hdr, _name, _value) { \
		_hdr.name = (uint8_t *)_name;\
		_hdr.value = (uint8_t *)_value;\
		_hdr.namelen = (u32) strlen(_name);\
		_hdr.valuelen = (u32) strlen(_value);\
		_hdr.flags = NGHTTP2_NV_FLAG_NONE;\
	}


static GF_Err h3_submit_request(GF_DownloadSession *sess, char *req_name, const char *url, const char *param_string, Bool has_body)
{
	NGTCP2Priv *qc = (NGTCP2Priv *)sess->hmux_sess->hmux_udta;
	if (!qc->http_conn) return GF_SERVICE_ERROR;

	u32 nb_hdrs, i;
	char *hostport = NULL;
	char *path = NULL;
	char port[20];
	nghttp3_nv *hdrs;

	nb_hdrs = gf_list_count(sess->headers);
	hdrs = gf_malloc(sizeof(nghttp3_nv) * (nb_hdrs + 4));

	NGH3_HDR(hdrs[0], ":method", req_name);
	NGH3_HDR(hdrs[1], ":scheme", "https");

	gf_dynstrcat(&hostport, sess->server_name, NULL);
	sprintf(port, ":%d", sess->port);
	gf_dynstrcat(&hostport, port, NULL);
	NGH3_HDR(hdrs[2], ":authority", hostport);

	if (param_string) {
		gf_dynstrcat(&path, url, NULL);
		if (strchr(sess->remote_path, '?')) {
			gf_dynstrcat(&path, param_string, "&");
		} else {
			gf_dynstrcat(&path, param_string, "?");
		}
		NGH3_HDR(hdrs[3], ":path", path);
	} else {
		NGH3_HDR(hdrs[3], ":path", url);
	}

	for (i=0; i<nb_hdrs; i++) {
		GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
		NGH3_HDR(hdrs[4+i], hdr->name, hdr->value);
	}
	if (has_body) {
		nghttp3_data_reader *data_io = (nghttp3_data_reader*)sess->hmux_priv;
		gf_assert(data_io && data_io->read_data);
	}
	sess->hmux_data_done = 0;
	sess->hmux_headers_seen = 0;
	sess->hmux_ready_to_send = 0;

    int rv = ngtcp2_conn_open_bidi_stream(qc->conn, &sess->hmux_stream_id, sess);
    if (rv != 0) {
		return GF_SERVICE_ERROR;
	}

	rv = nghttp3_conn_submit_request(qc->http_conn, sess->hmux_stream_id, hdrs, nb_hdrs+4,
			has_body ? (nghttp3_data_reader *)sess->hmux_priv : NULL, sess);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP3] nghttp3_conn_submit_request error %s\n", nghttp3_strerror(rv) ));
		return GF_SERVICE_ERROR;
	}


#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_HTTP, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP3] send request (has_body %d) for new stream_id %d:\n", has_body, sess->hmux_stream_id));
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

	return sess->hmux_sess->send(sess);
}

static GF_Err h3_send_reply(GF_DownloadSession *sess, u32 reply_code, const char *response_body, u32 body_len, Bool no_body)
{
	return GF_OK;
}

static GF_Err h3_session_send(GF_DownloadSession *sess)
{
	NGTCP2Priv *qc = (NGTCP2Priv *)sess->hmux_sess->hmux_udta;
	u32 max_pay_size = ngtcp2_conn_get_max_tx_udp_payload_size(qc->conn);
	u32 path_max_udp_size = ngtcp2_conn_get_path_max_tx_udp_payload_size(qc->conn);
	u64 ts = ngtcp2_timestamp();
	u8 buffer[1500];
	ngtcp2_path_storage ps;
	ngtcp2_pkt_info pi;
	u32 pay_size=0;

	ngtcp2_path_storage_zero(&ps);
	for (;;) {
		s64 stream_id = -1;
		s32 fin = 0;
		nghttp3_vec vec[256];
		u32 nb_vec=0;
		vec[0].len = 0;
		vec[0].base = NULL;

		if (qc->http_conn && ngtcp2_conn_get_max_data_left(qc->conn)) {
			s32 rv = nghttp3_conn_writev_stream(qc->http_conn, &stream_id, &fin, vec, GF_ARRAY_LENGTH(vec) );
			if (rv < 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP3] nghttp3_conn_writev_stream error %s\n", nghttp3_strerror(rv) ));
				ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
				return GF_IP_NETWORK_FAILURE;
			}
			nb_vec = (u32) rv;
		}
		ngtcp2_ssize ndatalen;
		u32 flags = NGTCP2_WRITE_STREAM_FLAG_MORE;
		if (fin) {
		  flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
		}
		u32 buflen = pay_size >= max_pay_size ? max_pay_size : path_max_udp_size;

		ndatalen=0;
		ngtcp2_ssize nwrite = ngtcp2_conn_writev_stream(qc->conn, &ps.path, &pi,
			buffer, buflen, &ndatalen, flags,
			stream_id, (ngtcp2_vec*)vec, nb_vec,
			ts
		);

		if (nwrite < 0) {
			s32 rv;
			switch (nwrite) {
			case NGTCP2_ERR_STREAM_DATA_BLOCKED:
				assert(ndatalen == -1);
				nghttp3_conn_block_stream(qc->http_conn, stream_id);
				continue;
			case NGTCP2_ERR_STREAM_SHUT_WR:
				assert(ndatalen == -1);
				nghttp3_conn_shutdown_stream_write(qc->http_conn, stream_id);
				continue;
			case NGTCP2_ERR_WRITE_MORE:
				assert(ndatalen >= 0);
				rv = nghttp3_conn_add_write_offset(qc->http_conn, stream_id, ndatalen);
				if (rv != 0) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Error in nghttp3_conn_add_write_offset: %s\n", nghttp3_strerror(rv)));
					ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
					if (!qc->handshake_state) qc->handshake_state = 2;
					return GF_IP_NETWORK_FAILURE;
				}
				continue;
			}
			assert(ndatalen == -1);
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Error in ngtcp2_conn_write_stream: %s\n", ngtcp2_strerror(nwrite) ));
			ngtcp2_ccerr_set_liberr(&qc->last_error, nwrite, NULL, 0);
			if (!qc->handshake_state) qc->handshake_state = 2;
			return GF_IP_NETWORK_FAILURE;
		}

		if (ndatalen >= 0) {
			s32 rv = nghttp3_conn_add_write_offset(qc->http_conn, stream_id, ndatalen);
			if (rv != 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Error in nghttp3_conn_add_write_offset: %s\n", nghttp3_strerror(rv) ));
				ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
				if (!qc->handshake_state) qc->handshake_state = 2;
				return GF_IP_NETWORK_FAILURE;
			}
		}

		if (nwrite == 0) {
			ngtcp2_conn_update_pkt_tx_time(qc->conn, ts);
			return 0;
		}

		u32 written=0;
		GF_Err e = gf_sk_send_ex(sess->sock, buffer, nwrite, &written);

		if (e==GF_IP_NETWORK_EMPTY) {
			break;
		}
	}
	return GF_OK;
}

static void h3_stream_reset(GF_DownloadSession *sess)
{
	NGTCP2Priv *qc = sess->hmux_sess->hmux_udta;
	if (qc->http_conn && sess->hmux_stream_id)
		nghttp3_conn_close_stream(qc->http_conn, sess->hmux_stream_id, NGHTTP3_H3_NO_ERROR);
}

static void h3_resume_stream(GF_DownloadSession *sess)
{
	NGTCP2Priv *qc = sess->hmux_sess->hmux_udta;
	if (qc->http_conn && sess->hmux_stream_id)
		nghttp3_conn_resume_stream(qc->http_conn, sess->hmux_stream_id);
}

static void h3_close(struct _http_mux_session *hmux)
{
	NGTCP2Priv *qc = hmux->hmux_udta;
	if (!qc->conn || !hmux->net_sess || !hmux->net_sess->sock
		|| ngtcp2_conn_in_closing_period(qc->conn)
		|| ngtcp2_conn_in_draining_period(qc->conn)
	) {
		return;
	}
	ngtcp2_path_storage ps;
	ngtcp2_path_storage_zero(&ps);
	ngtcp2_pkt_info pi;
	u8 buffer[NGTCP2_MAX_UDP_PAYLOAD_SIZE];

	int nwrite = ngtcp2_conn_write_connection_close(qc->conn, &ps.path, &pi, buffer, NGTCP2_MAX_UDP_PAYLOAD_SIZE, &qc->last_error, ngtcp2_timestamp() );

	if (nwrite > 0) {
		u32 written;
		gf_sk_send_ex(hmux->net_sess->sock, buffer, nwrite, &written);
	}
}

static void h3_destroy(struct _http_mux_session *hmux)
{
	NGTCP2Priv *qc = hmux->hmux_udta;
	if (qc->http_conn)
		nghttp3_conn_del(qc->http_conn);
	ngtcp2_conn_del(qc->conn);
	if (qc->path.local.addr) gf_free(qc->path.local.addr);
	if (qc->path.remote.addr) gf_free(qc->path.remote.addr);
	gf_free(qc);
}

static GF_Err h3_data_received(GF_DownloadSession *sess, const u8 *data, u32 nb_bytes)
{
	NGTCP2Priv *qc = (	NGTCP2Priv *) sess->hmux_sess->hmux_udta;
	ngtcp2_path path;
	ngtcp2_pkt_info pi = {0};

	path = qc->path;

	int rv = ngtcp2_conn_read_pkt(qc->conn, &path, &pi, data, (size_t)nb_bytes, ngtcp2_timestamp() );
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_read_pkt: %s\n", ngtcp2_strerror(rv) ));
		if (!qc->last_error.error_code) {
			if (rv == NGTCP2_ERR_CRYPTO) {
				ngtcp2_ccerr_set_tls_alert(&qc->last_error, ngtcp2_conn_get_tls_alert(qc->conn), NULL, 0);
			} else {
				ngtcp2_ccerr_set_liberr(&qc->last_error, rv, NULL, 0);
			}
		}
		if (!qc->handshake_state) qc->handshake_state = 2;
		return GF_IP_NETWORK_FAILURE;
	}
	return GF_OK;
}
static ngtcp2_conn *ngq_get_conn(ngtcp2_crypto_conn_ref *conn_ref)
{
	NGTCP2Priv *qc = conn_ref->user_data;
	return qc->conn;
}
static int ngq_handshake_completed(ngtcp2_conn *conn, void *user_data)
{
	NGTCP2Priv *qc = user_data;
	if (!qc->handshake_state) qc->handshake_state = 1;
	return 0;
}

int ngq_recv_stream_data(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                     uint64_t offset, const uint8_t *data, size_t datalen,
                     void *user_data, void *stream_user_data)
{
	NGTCP2Priv *qc = user_data;
	if (!qc->http_conn) return -1;

	int rv = nghttp3_conn_read_stream(qc->http_conn, stream_id, data, datalen, flags & NGTCP2_STREAM_DATA_FLAG_FIN);
	if (rv < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP3] nghttp3_conn_read_stream error %s\n", nghttp3_strerror(rv) ));
		ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	ngtcp2_conn_extend_max_stream_offset(qc->conn, stream_id, rv);
	ngtcp2_conn_extend_max_offset(qc->conn, rv);
	return 0;
}

static int ngq_acked_stream_data_offset(ngtcp2_conn *conn, int64_t stream_id,
                             uint64_t offset, uint64_t datalen, void *user_data,
                             void *stream_user_data)
{
	NGTCP2Priv *qc = user_data;
	int rv = nghttp3_conn_add_ack_offset(qc->http_conn, stream_id, datalen);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_add_ack_offset error %s\n", nghttp3_strerror(rv) ));
		ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	return 0;
}

int ngq_extend_max_stream_data(ngtcp2_conn *conn, int64_t stream_id,
                           uint64_t max_data, void *user_data,
                           void *stream_user_data)
{
	NGTCP2Priv *qc = user_data;
	int rv = nghttp3_conn_unblock_stream(qc->http_conn, stream_id);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_unblock_stream error %s\n", nghttp3_strerror(rv) ));
		ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	return 0;
}

static int ngq_recv_rx_key(ngtcp2_conn *conn, ngtcp2_encryption_level level, void *user_data)
{
	NGTCP2Priv *qc = user_data;
	if (level != NGTCP2_ENCRYPTION_LEVEL_1RTT) return 0;
	if (qc->http_conn) return GF_OK;

	GF_Err e = h3_setup_http3(qc->hmux);
	if (e) return NGTCP2_ERR_CALLBACK_FAILURE;
	return 0;
}

static int ngq_stream_stop_sending(ngtcp2_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *user_data,
                        void *stream_user_data)
{
	NGTCP2Priv *qc = user_data;
	if (!qc->http_conn) return 0;
	int rv = nghttp3_conn_shutdown_stream_read(qc->http_conn, stream_id);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_shutdown_stream_read error %s\n", nghttp3_strerror(rv) ));
		ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	return 0;
}


static int ngq_stream_close(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                 uint64_t app_error_code, void *user_data,
                 void *stream_user_data)
{
	NGTCP2Priv *qc = user_data;
//	GF_DownloadSession *sess = stream_user_data;
	if (!(flags & NGTCP2_STREAM_CLOSE_FLAG_APP_ERROR_CODE_SET)) {
		app_error_code = NGHTTP3_H3_NO_ERROR;
	}

	if (!qc->http_conn) return 0;

	if (app_error_code == 0) {
		app_error_code = NGHTTP3_H3_NO_ERROR;
    }
	int rv = nghttp3_conn_close_stream(qc->http_conn, stream_id, app_error_code);
	switch (rv) {
	case 0:
		break;
    case NGHTTP3_ERR_STREAM_NOT_FOUND:
		if (!ngtcp2_is_bidi_stream(stream_id)) {
			assert(!ngtcp2_conn_is_local_stream(qc->conn, stream_id));
			ngtcp2_conn_extend_max_streams_uni(qc->conn, 1);
		}
		break;
	default:
      GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_close_stream error %s\n", nghttp3_strerror(rv) ));
      ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
      return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	return 0;
}

int ngq_stream_reset(ngtcp2_conn *conn, int64_t stream_id, uint64_t final_size,
                 uint64_t app_error_code, void *user_data,
                 void *stream_user_data)
{
	NGTCP2Priv *qc = user_data;
	if (!qc->http_conn) return 0;

    int rv = nghttp3_conn_shutdown_stream_read(qc->http_conn, stream_id);
    if (rv != 0) {
      GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_shutdown_stream_read error %s\n", nghttp3_strerror(rv) ));
      return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	return 0;
}

static GF_Err h3_initialize(GF_DownloadSession *sess, char *server, u32 server_port)
{
	NGTCP2Priv *ng_quic;

	//TODO
	if (sess->server_mode)
		return GF_NOT_SUPPORTED;

	if (sess->dm && !sess->dm->ssl_ctx) {
		gf_ssl_try_connect(sess, NULL);
		if (!sess->ssl) return GF_IO_ERR;
	}

	GF_SAFEALLOC(sess->hmux_sess, GF_HMUX_Session);
	if (!sess->hmux_sess) return GF_OUT_OF_MEM;
	GF_SAFEALLOC(ng_quic, NGTCP2Priv);
	if (!sess->hmux_sess) {
		gf_free(sess->hmux_sess);
		sess->hmux_sess = NULL;
		return GF_OUT_OF_MEM;
	}
	ng_quic->hmux = sess->hmux_sess;
	sess->hmux_sess->hmux_udta = ng_quic;
	ng_quic->conn_ref.get_conn = ngq_get_conn;
	ng_quic->conn_ref.user_data = ng_quic;
	SSL_set_app_data(sess->ssl, &ng_quic->conn_ref);

	u8 *src, *dst;
	u32 src_len, dst_len;
	GF_Err e = gf_sk_bind_ex(sess->sock, "127.0.0.1", 1234, server, server_port, 0, &dst, &dst_len, &src, &src_len);
	if (e) goto err;

	ng_quic->path.local.addr = (ngtcp2_sockaddr*)src;
	ng_quic->path.local.addrlen = src_len;
	ng_quic->path.remote.addr = (ngtcp2_sockaddr*)dst;
	ng_quic->path.remote.addrlen = dst_len;


	//setup callbacks
	ngtcp2_callbacks callbacks = {
		.client_initial = ngtcp2_crypto_client_initial_cb,
		.handshake_completed = ngq_handshake_completed,
		.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
		.encrypt = ngtcp2_crypto_encrypt_cb,
		.decrypt = ngtcp2_crypto_decrypt_cb,
		.hp_mask = ngtcp2_crypto_hp_mask_cb,
		.recv_stream_data = ngq_recv_stream_data,
		.recv_retry = ngtcp2_crypto_recv_retry_cb,
		.acked_stream_data_offset = ngq_acked_stream_data_offset,
		.extend_max_local_streams_bidi = ngq_extend_max_local_streams_bidi,
		.rand = ngq_rand_cb,
		.extend_max_stream_data = ngq_extend_max_stream_data,
		.get_new_connection_id = ngq_get_new_connection_id_cb,
		.update_key = ngtcp2_crypto_update_key_cb,
		.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
		.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
		.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb,
		.version_negotiation = ngtcp2_crypto_version_negotiation_cb,
		.recv_rx_key = ngq_recv_rx_key,
		.stream_close = ngq_stream_close,
		.stream_reset = ngq_stream_reset,
		.stream_stop_sending = ngq_stream_stop_sending
	};
	//todo ?
//    .recv_version_negotiation = ::recv_version_negotiation,
//    .path_validation = path_validation,
//    .select_preferred_addr = ::select_preferred_address,
//    .handshake_confirmed = ::handshake_confirmed,
//    .recv_new_token = ::recv_new_token,
//    .tls_early_data_rejected = ::early_data_rejected,


	ngtcp2_cid dcid, scid;
	ngtcp2_settings settings;
	ngtcp2_transport_params params;
	int rv;

	dcid.datalen = NGTCP2_MIN_INITIAL_DCIDLEN;
	if (RAND_bytes(dcid.data, (int)dcid.datalen) != 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Failed to get random data for destination connection ID\n"));
		e = GF_IO_ERR;
		goto err;
	}

	scid.datalen = 8;
	if (RAND_bytes(scid.data, (int)scid.datalen) != 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Failed to get random data for source connection ID\n"));
		e = GF_IO_ERR;
		goto err;
	}

	ngtcp2_settings_default(&settings);

	if (gf_opts_get_bool("core", "h3-trace")) {
		settings.log_printf = ngq_printf;
		nghttp3_set_debug_vprintf_callback(ngq_debug_trace);
	} else {
		nghttp3_set_debug_vprintf_callback(ngq_debug_trace_noop);
	}

	settings.initial_ts = ngtcp2_timestamp();
	settings.cc_algo = NGTCP2_CC_ALGO_CUBIC;
	settings.initial_rtt = NGTCP2_DEFAULT_INITIAL_RTT;
	settings.max_window = 0;
	settings.max_stream_window = 0;
	settings.handshake_timeout = sess->conn_timeout*1000000;
	settings.no_pmtud = 0;
	settings.ack_thresh = 3;

	ngtcp2_transport_params_default(&params);

	params.initial_max_streams_uni = 3;
	params.initial_max_stream_data_bidi_local = 16777216;
	params.initial_max_data = 1024 * 1024;
	params.initial_max_stream_data_bidi_remote = 0;
	params.initial_max_stream_data_uni = 16777216;
	params.initial_max_streams_bidi = 0;
	params.initial_max_streams_uni = 100;
	params.max_idle_timeout = 30 * NGTCP2_SECONDS;
	params.active_connection_id_limit = 7;
	params.grease_quic_bit = 1;

	rv = ngtcp2_conn_client_new(&ng_quic->conn, &dcid, &scid, &ng_quic->path, NGTCP2_PROTO_VER_V1,
						   &callbacks, &settings, &params, NULL, ng_quic);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_client_new error %s\n", ngtcp2_strerror(rv) ));
		e = GF_IP_CONNECTION_FAILURE;
		goto err;
	}

	ngtcp2_conn_set_tls_native_handle(ng_quic->conn, sess->ssl);

	//set hmux callbacks
	sess->hmux_sess->sessions = gf_list_new();

	sess->hmux_sess->net_sess = sess;
	//setup function pointers
	sess->hmux_sess->submit_request = h3_submit_request;
	sess->hmux_sess->send_reply = h3_send_reply;
	sess->hmux_sess->send = h3_session_send;
	sess->hmux_sess->close = h3_close;
	sess->hmux_sess->destroy = h3_destroy;
	sess->hmux_sess->stream_reset = h3_stream_reset;
	sess->hmux_sess->resume = h3_resume_stream;
	sess->hmux_sess->data_received = h3_data_received;
	sess->hmux_sess->setup_session = h3_setup_session;

	gf_list_add(sess->hmux_sess->sessions, sess);

	if (!sess->mx) {
		char szMXName[100];
		sprintf(szMXName, "http3_%p", sess->hmux_sess);
		sess->hmux_sess->mx = gf_mx_new(szMXName);
		sess->mx = sess->hmux_sess->mx;
	} else {
		sess->hmux_sess->mx = sess->mx;
	}
	sess->chunked = GF_FALSE;
	return h3_session_send(sess);

err:
	gf_free(sess->hmux_sess);
	sess->hmux_sess = NULL;
	if (ng_quic->path.local.addr) gf_free(ng_quic->path.local.addr);
	if (ng_quic->path.remote.addr) gf_free(ng_quic->path.remote.addr);
	gf_free(ng_quic);
	return e;
}

GF_Err http3_connect(GF_DownloadSession *sess, char *server, u32 server_port)
{
	if (!sess->hmux_sess) {
		GF_Err e = h3_initialize(sess, server, server_port);
		if (e) return e;
		return GF_IP_NETWORK_EMPTY;
	}
	if (!sess->hmux_sess)
		return GF_IP_CONNECTION_CLOSED;

	u32 res;
	GF_Err e = gf_dm_read_data(sess, sess->http_buf, sess->http_buf_size, &res);
	if (e) return e;

	NGTCP2Priv *qc = sess->hmux_sess->hmux_udta;
	if (qc->handshake_state==2)
		return GF_IP_CONNECTION_FAILURE;
	if (!qc->handshake_state) return GF_IP_NETWORK_EMPTY;

	return h3_setup_http3(sess->hmux_sess);
}


#endif // !defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_NGTCP2)
