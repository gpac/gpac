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

#if !defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_NGTCP2)

#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>

#define NV2_HDR(_hdr, _name, _value) { \
		_hdr.name = (uint8_t *)_name;\
		_hdr.value = (uint8_t *)_value;\
		_hdr.namelen = (u32) strlen(_name);\
		_hdr.valuelen = (u32) strlen(_value);\
		_hdr.flags = NGHTTP2_NV_FLAG_NONE;\
	}

GF_Err gf_sk_bind_ex(GF_Socket *sock, const char *ifce_ip_or_name, u16 port, const char *peer_name, u16 peer_port, u32 options,
	u8 **dst_sock_addr, u32 *dst_sock_addr_len, u8 **src_sock_addr, u32 *src_sock_addr_len);


static u64 ngtcp2_timestamp()
{
	return 1000*gf_sys_clock_high_res();
}

typedef struct
{
	ngtcp2_path path;
	ngtcp2_conn *conn;
	ngtcp2_crypto_conn_ref conn_ref;
	nghttp3_conn *http_conn;
	ngtcp2_ccerr last_error;
	u32 handshake_state; //0: pending, 1: done, 2: error
} NGTCP2Priv;

static void h3_rand_cb(uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx)
{
	size_t i;
	(void)rand_ctx;
	for (i = 0; i < destlen; ++i) {
		*dest = (uint8_t)random();
	}
}

static int h3_get_new_connection_id_cb(ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token, size_t cidlen, void *user_data)
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

static int h3_extend_max_local_streams_bidi(ngtcp2_conn *conn, uint64_t max_streams,
                                  void *user_data)
{
	GF_HMUX_Session *hmux = user_data;
	return 0;
}

static void h3_printf(void *user_data, const char *format, ...)
{
  va_list ap;
  (void)user_data;

  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);

  fprintf(stderr, "\n");
}

static nghttp3_ssize h3_data_source_read_callback(
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
	data_io->read_data = h3_data_source_read_callback;
	return GF_OK;
}

#if 0
static int http3_stream_close(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                 uint64_t app_error_code, void *user_data,
                 void *stream_user_data)
{
	GF_HMUX_Session *hmux = user_data;
	if (!(flags & NGTCP2_STREAM_CLOSE_FLAG_APP_ERROR_CODE_SET)) {
		app_error_code = NGHTTP3_H3_NO_ERROR;
	}

/*	if (c->on_stream_close(stream_id, app_error_code) != 0) {
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
*/
	return 0;
}
#endif


static int http3_recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                   size_t datalen, void *user_data, void *stream_user_data)
{
	GF_HMUX_Session *hmux = user_data;
	return 0;
}

static int http3_deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                          size_t nconsumed, void *user_data,
                          void *stream_user_data)
{
	GF_HMUX_Session *hmux = user_data;
	//c->http_consume(stream_id, nconsumed);
	return 0;
}

static int http3_begin_headers(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                       void *stream_user_data)
{
  return 0;
}

static int http3_recv_header(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                     nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                     void *user_data, void *stream_user_data)
{
  return 0;
}

static int http3_end_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
                     void *user_data, void *stream_user_data)
{
  return 0;
}

static int http3_begin_trailers(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                        void *stream_user_data) {
	return 0;
}

static int http3_recv_trailer(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                      nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                      void *user_data, void *stream_user_data)
{
	return 0;
}

static int http3_end_trailers(nghttp3_conn *conn, int64_t stream_id, int fin,
                      void *user_data, void *stream_user_data)
{
  return 0;
}

int http3_stop_sending(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data)
{

/*  if (c->stop_sending(stream_id, app_error_code) != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
*/
	return 0;
}

static int http3_reset_stream(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *user_data,
                      void *stream_user_data)
{
/*  if (c->reset_stream(stream_id, app_error_code) != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
*/
	return 0;
}

static int http3_stream_close(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *conn_user_data,
                      void *stream_user_data)
{

/*  if (c->http_stream_close(stream_id, app_error_code) != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
*/
	return 0;
}

static int http3_recv_settings(nghttp3_conn *conn, const nghttp3_settings *settings, void *conn_user_data)
{
	return 0;
}

static GF_Err h3_setup_http3(GF_DownloadSession *sess)
{
	NGTCP2Priv *qc = (NGTCP2Priv *)sess->hmux_sess->hmux_udta;
	if (ngtcp2_conn_get_streams_uni_left(qc->conn) < 3) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] peer does not allow at least 3 unidirectional streams.\n"));
		return GF_REMOTE_SERVICE_ERROR;
	}

	nghttp3_callbacks callbacks = {
		.stream_close = http3_stream_close,
		.recv_data = http3_recv_data,
		.deferred_consume = http3_deferred_consume,
		.begin_headers = http3_begin_headers,
		.recv_header = http3_recv_header,
		.end_headers = http3_end_headers,
		.begin_trailers = http3_begin_trailers,
		.recv_trailer = http3_recv_trailer,
		.end_trailers = http3_end_trailers,
		.stop_sending = http3_stop_sending,
		.reset_stream = http3_reset_stream,
		.recv_settings = http3_recv_settings,
	};
	nghttp3_settings settings;
	nghttp3_settings_default(&settings);
	settings.qpack_max_dtable_capacity = 4096;
	settings.qpack_blocked_streams = 100;

	const nghttp3_mem *mem = nghttp3_mem_default();

	int rv = nghttp3_conn_client_new(&qc->http_conn, &callbacks, &settings, mem, sess->hmux_sess);
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

	NV2_HDR(hdrs[0], ":method", req_name);
	NV2_HDR(hdrs[1], ":scheme", "https");

	gf_dynstrcat(&hostport, sess->server_name, NULL);
	sprintf(port, ":%d", sess->port);
	gf_dynstrcat(&hostport, port, NULL);
	NV2_HDR(hdrs[2], ":authority", hostport);

	if (param_string) {
		gf_dynstrcat(&path, url, NULL);
		if (strchr(sess->remote_path, '?')) {
			gf_dynstrcat(&path, param_string, "&");
		} else {
			gf_dynstrcat(&path, param_string, "?");
		}
		NV2_HDR(hdrs[3], ":path", path);
	} else {
		NV2_HDR(hdrs[3], ":path", url);
	}

	for (i=0; i<nb_hdrs; i++) {
		GF_HTTPHeader *hdr = gf_list_get(sess->headers, i);
		NV2_HDR(hdrs[4+i], hdr->name, hdr->value);
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

	sess->hmux_sess->send(sess);
	return GF_OK;
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

//				disconnect();
				return GF_IP_NETWORK_FAILURE;
			}
			nb_vec = (u32) rv;
		}

//		ngtcp2_vec src = {0};
//		u32 src_len = 0;
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
//					disconnect();
					if (!qc->handshake_state) qc->handshake_state = 2;
					return GF_IP_NETWORK_FAILURE;
				}
				continue;
			}
			assert(ndatalen == -1);
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Error in ngtcp2_conn_write_stream: %s\n", ngtcp2_strerror(nwrite) ));
			ngtcp2_ccerr_set_liberr(&qc->last_error, nwrite, NULL, 0);
//			disconnect();
			if (!qc->handshake_state) qc->handshake_state = 2;
			return GF_IP_NETWORK_FAILURE;
		}

		if (ndatalen >= 0) {
			s32 rv = nghttp3_conn_add_write_offset(qc->http_conn, stream_id, ndatalen);
			if (rv != 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Error in nghttp3_conn_add_write_offset: %s\n", nghttp3_strerror(rv) ));
				ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
//				disconnect();
				if (!qc->handshake_state) qc->handshake_state = 2;
				return GF_IP_NETWORK_FAILURE;
			}
		}

		if (nwrite == 0) {
			// We are congestion limited.
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
}

static void h3_resume_stream(GF_DownloadSession *sess)
{
}

static void h3_destroy(struct _http_mux_session *hmux)
{
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
static ngtcp2_conn *h3_get_conn(ngtcp2_crypto_conn_ref *conn_ref)
{
	NGTCP2Priv *qc = conn_ref->user_data;
	return qc->conn;
}
static int h3_handshake_completed(ngtcp2_conn *conn, void *user_data)
{
	GF_HMUX_Session *hmux = user_data;
	NGTCP2Priv *qc = hmux->hmux_udta;
	if (!qc->handshake_state) qc->handshake_state = 1;
	return 0;
}

int h3_recv_stream_data(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                     uint64_t offset, const uint8_t *data, size_t datalen,
                     void *user_data, void *stream_user_data)
{
	GF_HMUX_Session *hmux = user_data;
	NGTCP2Priv *qc = hmux->hmux_udta;
	if (!qc->http_conn) return -1;

	int rv = nghttp3_conn_read_stream(qc->http_conn, stream_id, data, datalen, flags & NGTCP2_STREAM_DATA_FLAG_FIN);
	if (rv < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP3] nghttp3_conn_read_stream error %s\n", nghttp3_strerror(rv) ));
		ngtcp2_ccerr_set_application_error(&qc->last_error, nghttp3_err_infer_quic_app_error_code(rv), NULL, 0);
		return -1;
	}
	ngtcp2_conn_extend_max_stream_offset(qc->conn, stream_id, rv);
	ngtcp2_conn_extend_max_offset(qc->conn, rv);
	return 0;
}

static int h3_acked_stream_data_offset(ngtcp2_conn *conn, int64_t stream_id,
                             uint64_t offset, uint64_t datalen, void *user_data,
                             void *stream_user_data)
{
	GF_HMUX_Session *hmux = user_data;
	NGTCP2Priv *qc = hmux->hmux_udta;
	int rv = nghttp3_conn_add_ack_offset(qc->http_conn, stream_id, datalen);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_add_ack_offset error %s\n", nghttp3_strerror(rv) ));
		return -1;
	}
	return 0;
}

int h3_extend_max_stream_data(ngtcp2_conn *conn, int64_t stream_id,
                           uint64_t max_data, void *user_data,
                           void *stream_user_data)
{
	GF_HMUX_Session *hmux = user_data;
	NGTCP2Priv *qc = hmux->hmux_udta;
	int rv = nghttp3_conn_unblock_stream(qc->http_conn, stream_id);
	if (rv != 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_unblock_stream error %s\n", nghttp3_strerror(rv) ));
		return -1;
	}
	return 0;
}


static GF_Err http3_initialize(GF_DownloadSession *sess, char *server, u32 server_port)
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
	sess->hmux_sess->hmux_udta = ng_quic;
	ng_quic->conn_ref.get_conn = h3_get_conn;
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
		.handshake_completed = h3_handshake_completed,
		.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
		.encrypt = ngtcp2_crypto_encrypt_cb,
		.decrypt = ngtcp2_crypto_decrypt_cb,
		.hp_mask = ngtcp2_crypto_hp_mask_cb,
		.recv_stream_data = h3_recv_stream_data,
		.recv_retry = ngtcp2_crypto_recv_retry_cb,
		.acked_stream_data_offset = h3_acked_stream_data_offset,
		.extend_max_local_streams_bidi = h3_extend_max_local_streams_bidi,
		.rand = h3_rand_cb,
		.extend_max_stream_data = h3_extend_max_stream_data,
		.get_new_connection_id = h3_get_new_connection_id_cb,
		.update_key = ngtcp2_crypto_update_key_cb,
		.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
		.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
		.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb,
		.version_negotiation = ngtcp2_crypto_version_negotiation_cb,
	};

//    .recv_version_negotiation = ::recv_version_negotiation,
//    .stream_close = stream_close,
//    .path_validation = path_validation,
//    .select_preferred_addr = ::select_preferred_address,
//    .stream_reset = stream_reset,
//    .handshake_confirmed = ::handshake_confirmed,
//    .recv_new_token = ::recv_new_token,
//    .stream_stop_sending = stream_stop_sending,
//    .recv_rx_key = ::recv_rx_key,
//    .tls_early_data_rejected = ::early_data_rejected,


	ngtcp2_cid dcid, scid;
	ngtcp2_settings settings;
	ngtcp2_transport_params params;
	int rv;

	dcid.datalen = NGTCP2_MIN_INITIAL_DCIDLEN;
	if (RAND_bytes(dcid.data, (int)dcid.datalen) != 1) {
		fprintf(stderr, "RAND_bytes failed\n");
		e = GF_IO_ERR;
		goto err;
	}

	scid.datalen = 8;
	if (RAND_bytes(scid.data, (int)scid.datalen) != 1) {
		fprintf(stderr, "RAND_bytes failed\n");
		e = GF_IO_ERR;
		goto err;
	}

	ngtcp2_settings_default(&settings);

	settings.initial_ts = ngtcp2_timestamp();
	settings.log_printf = h3_printf;

	ngtcp2_transport_params_default(&params);

	params.initial_max_streams_uni = 3;
	params.initial_max_stream_data_bidi_local = 128 * 1024;
	params.initial_max_data = 1024 * 1024;

	rv = ngtcp2_conn_client_new(&ng_quic->conn, &dcid, &scid, &ng_quic->path, NGTCP2_PROTO_VER_V1,
						   &callbacks, &settings, &params, NULL, sess->hmux_sess);
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
		GF_Err e = http3_initialize(sess, server, server_port);
		if (e) return e;
		return GF_IP_NETWORK_EMPTY;
	}
	if (!sess->hmux_sess) return GF_IP_CONNECTION_CLOSED;

	u32 res;
	GF_Err e = gf_dm_read_data(sess, sess->http_buf, sess->http_buf_size, &res);
	if (e) return e;

	NGTCP2Priv *qc = sess->hmux_sess->hmux_udta;
	if (qc->handshake_state==2) return GF_IP_CONNECTION_FAILURE;
	if (!qc->handshake_state) return GF_IP_NETWORK_EMPTY;

	return h3_setup_http3(sess);
}


#endif // !defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_NGTCP2)
