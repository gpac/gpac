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

GF_Err gf_sk_send_to(GF_Socket *sock, const u8 *buffer, u32 length, const u8 *addr, u32 addr_len, u32 *written);

static u64 ngtcp2_timestamp()
{
	return 1000*gf_sys_clock_high_res();
}


#define NGTCP2_STATELESS_RESET_BURST 100
#define NGTCP2_SV_SCIDLEN 18
#define MAX_CONNID_LEN	255
#define MAX_ADDR_LEN	100
const u8 *gf_sk_get_address(GF_Socket *sock, u32 *addr_size);


struct __gf_quic_server
{
	GF_Socket *sock;
	GF_DownloadManager *dm;
	Bool (*accept_conn)(void *udta, GF_DownloadSession *sess, const char *address, u32 port);
	void *udta;
	SSL_CTX *ssl_ctx;

	u8 *local_add;
	u32 local_add_len;

	GF_List *connections;
	u8 secret[GF_SHA256_DIGEST_SIZE];
	Bool validate_address;
	u32 stateless_reset_count;

};

typedef struct __gf_quic_server GF_QuicServer;


typedef struct __gf_quic_connection
{
	GF_QuicServer *server;

	u8 dcid[MAX_CONNID_LEN];
	u32 dcid_len;

	u8 addr[MAX_ADDR_LEN];
	u32 addr_len;

	u64 drain_period_end;
	u64 close_period_end;
	u8 closebuf[NGTCP2_MAX_UDP_PAYLOAD_SIZE];
	u32 closebuf_len;

	GF_HMUX_Session *hmux;
} GF_QUICConnection;


typedef struct
{
	//if server, pointer to the server, NULL otherwise
	GF_QUICConnection *serv_conn;

	// parent hmux downlaoder session
	GF_HMUX_Session *hmux;
	ngtcp2_path path;
	ngtcp2_conn *conn;
	ngtcp2_crypto_conn_ref conn_ref;
	nghttp3_conn *http_conn;
	ngtcp2_ccerr last_error;

	//for client
	u32 handshake_state; //0: pending, 1: done, 2: error

	//for server

} NGTCP2Priv;

static GF_Err quic_send_packet(GF_QuicServer *qs, const u8 *buf, u32 len, const u8 *to_a, u32 to_alen);

static void ngq_rand_cb(uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx)
{
	size_t i;
	(void)rand_ctx;
	for (i = 0; i < destlen; ++i) {
		*dest = (uint8_t)random();
	}
}

static int ngq_get_new_connection_id(ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token, size_t cidlen, void *user_data)
{
	(void)conn;
	NGTCP2Priv *qc = user_data;

	if (RAND_bytes(cid->data, (int)cidlen) != 1) {
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	cid->datalen = cidlen;
	if (RAND_bytes(token, NGTCP2_STATELESS_RESET_TOKENLEN) != 1) {
		return NGTCP2_ERR_CALLBACK_FAILURE;
	}
	//client
	if (qc->serv_conn) {
		memcpy(qc->serv_conn->dcid, cid->data, cid->datalen);
		qc->serv_conn->dcid_len = cid->datalen;
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

static int ngh3_acked_stream_data(nghttp3_conn *conn, int64_t stream_id, uint64_t datalen,
	void *conn_user_data, void *stream_user_data)
{
	NGTCP2Priv *qc = (NGTCP2Priv *)conn_user_data;
	GF_DownloadSession *sess = stream_user_data;
//	stream->http_acked_stream_data(datalen);
	ngtcp2_conn_info ci;
	ngtcp2_conn_get_conn_info(qc->conn, &ci);
/*	if (stream->dynresp && stream->dynbuflen < ci.cwnd) {
    	int rv = nghttp3_conn_resume_stream(qc->http_conn, sess->hmux_stream_id);
    	if (rv != 0) {
		  // TODO Handle error
		}
	}
*/
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
		.acked_stream_data = ngh3_acked_stream_data
	};

	nghttp3_settings settings;
	nghttp3_settings_default(&settings);
	settings.qpack_max_dtable_capacity = 4096;
	settings.qpack_blocked_streams = 100;

	const nghttp3_mem *mem = nghttp3_mem_default();

	int rv;
	if (qc->serv_conn) {
		rv = nghttp3_conn_server_new(&qc->http_conn, &callbacks, &settings, mem, hmux_sess->hmux_udta);
		if (rv != 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_server_new error %s\n", nghttp3_strerror(rv) ));
			return GF_SERVICE_ERROR;
		}
	} else {
		rv = nghttp3_conn_client_new(&qc->http_conn, &callbacks, &settings, mem, hmux_sess->hmux_udta);
		if (rv != 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] nghttp3_conn_client_new error %s\n", nghttp3_strerror(rv) ));
			return GF_SERVICE_ERROR;
		}
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

static GF_Err h3_session_send_ex(GF_DownloadSession *sess, NGTCP2Priv *qc)
{
	if (!qc) qc = (NGTCP2Priv *)sess->hmux_sess->hmux_udta;

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
		GF_Err e;
		if (sess)
			e = gf_sk_send_ex(sess->sock, buffer, nwrite, &written);
		else {
			e = quic_send_packet(qc->serv_conn->server, buffer, nwrite, (const u8 *) ps.path.remote.addr, (u32) ps.path.remote.addrlen);
		}

		if (e==GF_IP_NETWORK_EMPTY) {
			break;
		}
		if (e) {
			break;
		}
	}
	return GF_OK;
}
static GF_Err h3_session_send(GF_DownloadSession *sess)
{
	return h3_session_send_ex(sess, NULL);
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

static int ngq_stream_open(ngtcp2_conn *conn, int64_t stream_id, void *user_data)
{
	NGTCP2Priv *qc = user_data;
	if (!ngtcp2_is_bidi_stream(stream_id)) {
		return 0;
	}
	return 0;
}


static GF_Err h3_initialize(GF_DownloadSession *sess, char *server, u32 server_port,
	const ngtcp2_pkt_hd *srv_hd, const ngtcp2_cid *srv_ocid, u32 token_type, ngtcp2_path *srv_path, GF_QUICConnection *qsc)
{
	NGTCP2Priv *ng_quic;
	GF_Err e;

	if (!sess->server_mode && sess->dm && !sess->dm->ssl_ctx) {
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
	ng_quic->serv_conn = qsc;
	ng_quic->hmux = sess->hmux_sess;
	sess->hmux_sess->hmux_udta = ng_quic;
	ng_quic->conn_ref.get_conn = ngq_get_conn;
	ng_quic->conn_ref.user_data = ng_quic;
	SSL_set_app_data(sess->ssl, &ng_quic->conn_ref);

	if (qsc) qsc->hmux = sess->hmux_sess;

	if (sess->server_mode) {
		u8 *buf;
		buf = gf_malloc(sizeof(u8) * srv_path->local.addrlen);
		memcpy(buf, srv_path->local.addr, srv_path->local.addrlen);
		ng_quic->path.local.addr = (ngtcp2_sockaddr *) buf;
		ng_quic->path.local.addrlen = srv_path->local.addrlen;

		buf = gf_malloc(sizeof(u8) * srv_path->remote.addrlen);
		memcpy(buf, srv_path->remote.addr, srv_path->remote.addrlen);
		ng_quic->path.remote.addr = (ngtcp2_sockaddr *) buf;
		ng_quic->path.remote.addrlen = srv_path->remote.addrlen;
	} else {
		u8 *src, *dst;
		u32 src_len, dst_len;
		e = gf_sk_bind_ex(sess->sock, "127.0.0.1", 1234, server, server_port, 0, &dst, &dst_len, &src, &src_len);
		if (e) goto err;
		//UDP connect to make sure we only get datagrams for ourselves
		e = gf_sk_connect(sess->sock, (char *) server, server_port, NULL);
		if (e) goto err;

		ng_quic->path.local.addr = (ngtcp2_sockaddr*)src;
		ng_quic->path.local.addrlen = src_len;
		ng_quic->path.remote.addr = (ngtcp2_sockaddr*)dst;
		ng_quic->path.remote.addrlen = dst_len;
	}


	//setup callbacks
	ngtcp2_callbacks callbacks = {
		.handshake_completed = ngq_handshake_completed,
		.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb,
		.encrypt = ngtcp2_crypto_encrypt_cb,
		.decrypt = ngtcp2_crypto_decrypt_cb,
		.hp_mask = ngtcp2_crypto_hp_mask_cb,
		.recv_stream_data = ngq_recv_stream_data,
		.acked_stream_data_offset = ngq_acked_stream_data_offset,
		.extend_max_local_streams_bidi = ngq_extend_max_local_streams_bidi,
		.rand = ngq_rand_cb,
		.extend_max_stream_data = ngq_extend_max_stream_data,
		.update_key = ngtcp2_crypto_update_key_cb,
		.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb,
		.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
		.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb,
		.version_negotiation = ngtcp2_crypto_version_negotiation_cb,
		.get_new_connection_id = ngq_get_new_connection_id,
		.stream_close = ngq_stream_close,
		.stream_reset = ngq_stream_reset,
		.stream_stop_sending = ngq_stream_stop_sending,
	};

	if (sess->server_mode) {
	    callbacks.recv_client_initial = ngtcp2_crypto_recv_client_initial_cb;
		callbacks.recv_tx_key = ngq_recv_rx_key;
		callbacks.stream_open = ngq_stream_open;
//		callbacks.get_new_connection_id = ngq_get_new_connection_id_client;
//    .remove_connection_id = remove_connection_id,
//    .update_key = ::update_key,
//		.path_validation
	} else {
		callbacks.recv_retry = ngtcp2_crypto_recv_retry_cb;
		callbacks.client_initial = ngtcp2_crypto_client_initial_cb;
		callbacks.recv_rx_key = ngq_recv_rx_key;
		//todo ?
//    .recv_version_negotiation = ::recv_version_negotiation,
//    .path_validation = path_validation,
//    .select_preferred_addr = ::select_preferred_address,
//    .handshake_confirmed = ::handshake_confirmed,
//    .recv_new_token = ::recv_new_token,
//    .tls_early_data_rejected = ::early_data_rejected,

	}


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

	//server config
	if (sess->server_mode) {
		settings.token = srv_hd->token;
		settings.tokenlen = srv_hd->tokenlen;
		settings.token_type = token_type;
		settings.max_window = 6000000;
		settings.max_stream_window = 6000000;
	}

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

	if (sess->server_mode) {
		params.stateless_reset_token_present = 1;
		if (srv_ocid) {
			params.original_dcid = *srv_ocid;
			params.retry_scid = srv_hd->dcid;
			params.retry_scid_present = 1;
		} else {
			params.original_dcid = srv_hd->dcid;
		}
		params.original_dcid_present = 1;

		rv = ngtcp2_crypto_generate_stateless_reset_token(params.stateless_reset_token, qsc->server->secret, GF_SHA256_DIGEST_SIZE, &scid);
		if (rv != 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_crypto_generate_stateless_reset_token error %s\n", ngtcp2_strerror(rv) ));
			e = GF_IP_CONNECTION_FAILURE;
			goto err;
		}
		rv = ngtcp2_conn_server_new(&ng_quic->conn, &srv_hd->scid, &scid, &ng_quic->path, NGTCP2_PROTO_VER_V1,
							   &callbacks, &settings, &params, NULL, ng_quic);
		if (rv != 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_server_new error %s\n", ngtcp2_strerror(rv) ));
			e = GF_IP_CONNECTION_FAILURE;
			goto err;
		}
	} else {

		rv = ngtcp2_conn_client_new(&ng_quic->conn, &dcid, &scid, &ng_quic->path, NGTCP2_PROTO_VER_V1,
							   &callbacks, &settings, &params, NULL, ng_quic);
		if (rv != 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_client_new error %s\n", ngtcp2_strerror(rv) ));
			e = GF_IP_CONNECTION_FAILURE;
			goto err;
		}
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
	//don't flush packets now in server mode
	if (qsc) return GF_OK;

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
		GF_Err e = h3_initialize(sess, server, server_port, NULL, NULL, NULL, NULL, NULL);
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



void gf_dm_sess_set_callback(GF_DownloadSession *sess, gf_dm_user_io user_io, void *usr_cbk)
{
	if (!sess) return;
	sess->user_proc = user_io;
	sess->usr_cbk = usr_cbk;
}

GF_Err gf_dm_quic_server_new(GF_DownloadManager *dm, void *ssl_ctx, GF_QuicServer **oq, const char *ip, u32 port, const char *netcap_id,
	Bool (*accept_conn)(void *udta, GF_DownloadSession *sess, const char *address, u32 port),
	void *udta)
{
	GF_QuicServer *tmp;
	if (!dm || !ssl_ctx || !accept_conn || !port) return GF_BAD_PARAM;

	GF_SAFEALLOC(tmp, GF_QuicServer);
	if (!tmp) return GF_OUT_OF_MEM;
	tmp->sock = gf_sk_new_ex(GF_SOCK_TYPE_UDP, netcap_id);
	GF_Err e = gf_sk_bind_ex(tmp->sock, NULL, port, ip, 0, GF_SOCK_REUSE_PORT, NULL, NULL, &tmp->local_add, &tmp->local_add_len);
	if (e) {
		gf_sk_del(tmp->sock);
		gf_free(tmp);
		return e;
	}
	gf_sk_set_block_mode(tmp->sock, GF_TRUE);
	gf_sk_server_mode(tmp->sock, GF_TRUE);
	tmp->connections = gf_list_new();

	u8 buf[50];
	RAND_bytes(buf, 50);
	gf_sha256_csum(buf, 50, tmp->secret);
	tmp->stateless_reset_count = NGTCP2_STATELESS_RESET_BURST;

	tmp->ssl_ctx = ssl_ctx;
	tmp->dm = dm;
	tmp->accept_conn = accept_conn;
	tmp->udta = udta;

	*oq = tmp;
	return GF_OK;
}
void gf_dm_quic_server_del(GF_QuicServer *qs)
{
	gf_list_del(qs->connections);
	gf_sk_del(qs->sock);
	gf_free(qs);
}
GF_Socket *gf_dm_quic_get_socket(GF_QuicServer *qs)
{
	return qs ? qs->sock : NULL;
}

static int quic_verify_token(GF_QuicServer *qs, const ngtcp2_pkt_hd *hd, const u8 *sa, u32 salen)
{
	u64 ts = ngtcp2_timestamp();
	if (ngtcp2_crypto_verify_regular_token(hd->token, hd->tokenlen,
                                         qs->secret,  GF_SHA256_DIGEST_SIZE,
                                         (const ngtcp2_sockaddr *)sa, (ngtcp2_socklen) salen,
                                         3600 * NGTCP2_SECONDS, ts) != 0) {
		return -1;
	}
	return 0;
}
static int quic_verify_retry_token(GF_QuicServer *qs, ngtcp2_cid *ocid, const ngtcp2_pkt_hd *hd, const u8 *sa, u32 salen)
{
	u64 ts = ngtcp2_timestamp();
	int rv = ngtcp2_crypto_verify_retry_token2(
		ocid, hd->token, hd->tokenlen,
		qs->secret,  GF_SHA256_DIGEST_SIZE,
		hd->version, (const ngtcp2_sockaddr *)sa, (ngtcp2_socklen) salen, &hd->dcid,
		10 * NGTCP2_SECONDS, ts);

	switch (rv) {
	case 0:
		break;
	case NGTCP2_CRYPTO_ERR_VERIFY_TOKEN:
		return -1;
	default:
		return 1;
	}
	return 0;
}

static GF_Err quic_send_packet(GF_QuicServer *qs, const u8 *buf, u32 len, const u8 *to_a, u32 to_alen)
{
	u32 written;
	GF_Err e = gf_sk_send_to(qs->sock, buf, len, to_a, to_alen, &written);
	if (e) return e;
	if (written!=len)
		return GF_IP_NETWORK_EMPTY;
	return GF_OK;
}
static int quic_send_retry(GF_QuicServer *qs, const ngtcp2_pkt_hd *chd, const u8 *sa, u32 salen, u32 max_pktlen)
{
	ngtcp2_cid scid;
	scid.datalen = NGTCP2_SV_SCIDLEN;
	if (RAND_bytes(scid.data, scid.datalen) != 1) {
		return -1;
	}
	u8 token[NGTCP2_CRYPTO_MAX_RETRY_TOKENLEN2];
	u64 ts = ngtcp2_timestamp();

	int tokenlen = ngtcp2_crypto_generate_retry_token2(token, qs->secret, GF_SHA256_DIGEST_SIZE, chd->version,
		(const ngtcp2_sockaddr *)sa, (ngtcp2_socklen) salen, &scid, &chd->dcid, ts);

	if (tokenlen < 0) {
		return -1;
	}

	u32 blen = MIN(NGTCP2_MAX_UDP_PAYLOAD_SIZE, max_pktlen);
	u8 buf[NGTCP2_MAX_UDP_PAYLOAD_SIZE];

	int nwrite = ngtcp2_crypto_write_retry(buf, blen, chd->version, &chd->scid, &scid, &chd->dcid, token, tokenlen);
	if (nwrite < 0) {
		return -1;
	}
	GF_Err e = quic_send_packet(qs, buf, blen, sa, salen);
	if (e) return -1;
	return 0;
}

static int quic_send_stateless_reset(GF_QuicServer *qs, u32 pktlen, const u8 *dcid, u32 dcid_len, const u8 *sa, u32 salen)
{
	if (qs->stateless_reset_count == 0) return 0;
	qs->stateless_reset_count--;

	ngtcp2_cid cid;
	ngtcp2_cid_init(&cid, dcid, dcid_len);
	u8 token[NGTCP2_STATELESS_RESET_TOKENLEN];

	if (ngtcp2_crypto_generate_stateless_reset_token(token, qs->secret, GF_SHA256_DIGEST_SIZE, &cid) != 0) {
		return -1;
	}

	u32 max_rand_byteslen = NGTCP2_MAX_CIDLEN + 22 - NGTCP2_STATELESS_RESET_TOKENLEN;
	u8 rand_bytes[NGTCP2_MAX_CIDLEN + 22 - NGTCP2_STATELESS_RESET_TOKENLEN];
	u32 rand_byteslen;

	if (pktlen <= 43) {
		rand_byteslen = pktlen - NGTCP2_STATELESS_RESET_TOKENLEN - 1;
	} else {
		rand_byteslen = max_rand_byteslen;
	}

	if (RAND_bytes(rand_bytes, rand_byteslen) != 1) {
		return -1;
	}

	u8 buf[NGTCP2_MAX_UDP_PAYLOAD_SIZE];
	int nwrite = ngtcp2_pkt_write_stateless_reset(buf, NGTCP2_MAX_UDP_PAYLOAD_SIZE, token, rand_bytes, rand_byteslen);
	if (nwrite < 0) {
		return -1;
	}
	GF_Err e = quic_send_packet(qs, buf, nwrite, sa, salen);
	if (e) return -1;
	return 0;
}

enum
{
	QNET_OK = 0,
	QNET_ERR_CLOSE_WAIT,
	QNET_ERR_RETRY,
	QNET_ERR_DROP_CONN
};

static int quic_send_stateless_connection_close(GF_QuicServer *qs, const ngtcp2_pkt_hd *chd, const u8 *sa, u32 salen)
{
	u8 buf[NGTCP2_MAX_UDP_PAYLOAD_SIZE];
	int nwrite = ngtcp2_crypto_write_connection_close(buf, NGTCP2_MAX_UDP_PAYLOAD_SIZE, chd->version, &chd->scid, &chd->dcid, NGTCP2_INVALID_TOKEN, NULL, 0);
	if (nwrite < 0) return -1;
	GF_Err e = quic_send_packet(qs, buf, nwrite, sa, salen);
	if (e) return -1;
	return 0;
}

void quic_start_draining_period(NGTCP2Priv *qc)
{
	qc->serv_conn->drain_period_end = gf_sys_clock_high_res();
	qc->serv_conn->drain_period_end += ngtcp2_conn_get_pto(qc->conn)/1000; //(*3);
//call close_waitcb
}
int quic_start_closing_period(NGTCP2Priv *qc)
{
	if (!qc->conn || ngtcp2_conn_in_closing_period(qc->conn) || ngtcp2_conn_in_draining_period(qc->conn)) {
		return 0;
	}
	qc->serv_conn->close_period_end = gf_sys_clock_high_res();
	qc->serv_conn->close_period_end += ngtcp2_conn_get_pto(qc->conn)/1000; //(*3);
//call close_waitcb
	ngtcp2_path_storage ps;
	ngtcp2_path_storage_zero(&ps);
	ngtcp2_pkt_info pi;
	int n = ngtcp2_conn_write_connection_close(qc->conn, &ps.path, &pi, qc->serv_conn->closebuf, NGTCP2_MAX_UDP_PAYLOAD_SIZE, &qc->last_error, ngtcp2_timestamp());
	if (n<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_write_connection_close error %s\n", ngtcp2_strerror(n) ));
		return -1;
	}
	if (n == 0) return 0;
	qc->serv_conn->closebuf_len = n;
	return 0;
}

static int quic_send_conn_close(NGTCP2Priv *qc)
{
	assert(qc->serv_conn && qc->serv_conn->closebuf_len);
	assert(qc->conn);
	assert(!ngtcp2_conn_in_draining_period(qc->conn));
	const ngtcp2_path *path = ngtcp2_conn_get_path(qc->conn);
	GF_Err e = quic_send_packet(qc->serv_conn->server, qc->serv_conn->closebuf, qc->serv_conn->closebuf_len, (const u8 *)&path->remote.addr, (u32) path->remote.addrlen);
	if (e) return -1;
	return 0;
}

static int quic_handle_error(NGTCP2Priv *qc)
{
	if (qc->last_error.type == NGTCP2_CCERR_TYPE_IDLE_CLOSE) {
		return -1;
	}
	if (quic_start_closing_period(qc) != 0) {
		return -1;
	}

	if (ngtcp2_conn_in_draining_period(qc->conn)) {
		return QNET_ERR_CLOSE_WAIT;
	}
	int rv = quic_send_conn_close(qc);
	if (rv != QNET_OK) {
		return rv;
	}
	return QNET_ERR_CLOSE_WAIT;
}

static int gf_quic_on_data(GF_QUICConnection *c, const u8 *data, u32 nb_bytes)
{
	NGTCP2Priv *qc = (NGTCP2Priv *) c->hmux->hmux_udta;
	ngtcp2_path path;
	ngtcp2_pkt_info pi = {0};

	path = qc->path;
	int rv = ngtcp2_conn_read_pkt(qc->conn, &path, &pi, data, (size_t)nb_bytes, ngtcp2_timestamp() );
	if (rv != 0) {
		switch (rv) {
		case NGTCP2_ERR_DRAINING:
			quic_start_draining_period(qc);
			return QNET_ERR_CLOSE_WAIT;
		case NGTCP2_ERR_RETRY:
			return QNET_ERR_RETRY;
		case NGTCP2_ERR_DROP_CONN:
			return QNET_ERR_DROP_CONN;
		case NGTCP2_ERR_CRYPTO:
			if (!qc->last_error.error_code) {
				if (rv == NGTCP2_ERR_CRYPTO) {
					ngtcp2_ccerr_set_tls_alert(&qc->last_error, ngtcp2_conn_get_tls_alert(qc->conn), NULL, 0);
				} else {
					ngtcp2_ccerr_set_liberr(&qc->last_error, rv, NULL, 0);
				}
			}
			return GF_OK;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] ngtcp2_conn_read_pkt: %s\n", ngtcp2_strerror(rv) ));
			return GF_IP_NETWORK_FAILURE;
		}
	}
	return GF_OK;
}

static int quic_on_write(GF_QUICConnection *c)
{
	NGTCP2Priv *qc = (NGTCP2Priv *) c->hmux->hmux_udta;
	if (ngtcp2_conn_in_closing_period(qc->conn) || ngtcp2_conn_in_draining_period(qc->conn)) {
		return 0;
	}

/*	  if (tx_.send_blocked) {
		if (auto rv = send_blocked_packet(); rv != 0) {
		  return rv;
		}

		if (tx_.send_blocked) {
		  return 0;
		}
	  }
	ev_io_stop(loop_, &wev_);
*/

	GF_Err e = h3_session_send_ex(NULL, qc);
	if (e) return -1;
	return 0;
}

static GF_Err gf_quic_create_connection(GF_QuicServer *qs, GF_QUICConnection **oc, ngtcp2_version_cid *vc, const u8 *data, u32 data_len, const u8 *s_add, u32 s_add_len)
{
	GF_QUICConnection *c;
	if (!s_add) return GF_IP_CONNECTION_FAILURE;
	if (s_add_len>MAX_ADDR_LEN) return GF_BAD_PARAM;
	if (vc->dcidlen>MAX_CONNID_LEN) return GF_BAD_PARAM;

    ngtcp2_pkt_hd hd;
    int rv = ngtcp2_accept(&hd, data, data_len);
    if (rv != 0) {
		if (!(data[0] & 0x80) && (data_len >= NGTCP2_SV_SCIDLEN + 22) ) {
			quic_send_stateless_reset(qs, data_len, vc->dcid, vc->dcidlen, s_add, s_add_len);
		}
		return GF_IP_NETWORK_EMPTY;
	}

    ngtcp2_cid ocid;
    ngtcp2_cid *pocid = NULL;
    ngtcp2_token_type token_type = NGTCP2_TOKEN_TYPE_UNKNOWN;
    assert(hd.type == NGTCP2_PKT_INITIAL);

    if (qs->validate_address || hd.tokenlen) {
		if (hd.tokenlen == 0) {
			quic_send_retry(qs, &hd, s_add, s_add_len, data_len * 3);
			return GF_IP_NETWORK_EMPTY;
		}
		if ((hd.token[0] != NGTCP2_CRYPTO_TOKEN_MAGIC_RETRY2) && (hd.dcid.datalen < NGTCP2_MIN_INITIAL_DCIDLEN)) {
			quic_send_stateless_connection_close(qs, &hd, s_add, s_add_len);
			return GF_IP_NETWORK_EMPTY;
		}
		switch (hd.token[0]) {
		case NGTCP2_CRYPTO_TOKEN_MAGIC_RETRY2:
			switch (quic_verify_retry_token(qs, &ocid, &hd, s_add, s_add_len)) {
			case 0:
				pocid = &ocid;
				token_type = NGTCP2_TOKEN_TYPE_RETRY;
				break;
			case -1:
				quic_send_stateless_connection_close(qs, &hd, s_add, s_add_len);
				return GF_IP_NETWORK_EMPTY;
			case 1:
				hd.token = NULL;
				hd.tokenlen = 0;
				break;
			}
			break;
		case NGTCP2_CRYPTO_TOKEN_MAGIC_REGULAR:
			if (quic_verify_token(qs, &hd, s_add, s_add_len) != 0) {
				if (qs->validate_address) {
					quic_send_retry(qs, &hd, s_add, s_add_len, data_len * 3);
					return GF_IP_NETWORK_EMPTY;
				}
				hd.token = NULL;
				hd.tokenlen = 0;
			} else {
				token_type = NGTCP2_TOKEN_TYPE_NEW_TOKEN;
			}
			break;
		default:
			if (qs->validate_address) {
				quic_send_retry(qs, &hd, s_add, s_add_len, data_len * 3);
				return GF_IP_NETWORK_EMPTY;
			}
			hd.token = NULL;
			hd.tokenlen = 0;
			break;
		}
	}
	//we have a new connection
	char peer_address[GF_MAX_IP_NAME_LEN];
	u32 peer_port;
	gf_sk_get_remote_address_port(qs->sock, peer_address, &peer_port);

	GF_Err e;
	GF_DownloadSession *sess = gf_dm_sess_new_internal(qs->dm, NULL, GF_NETIO_SESSION_NO_BLOCK, NULL, NULL, NULL, GF_TRUE, &e);
	if (e) {
		quic_send_stateless_connection_close(qs, &hd, s_add, s_add_len);
		return GF_IP_NETWORK_EMPTY;
	}
	if (!qs->accept_conn(qs->udta, sess, peer_address, peer_port)) {
		gf_dm_sess_del(sess);
		quic_send_stateless_connection_close(qs, &hd, s_add, s_add_len);
		return GF_IP_NETWORK_EMPTY;
	}
	//setup SSL
	sess->ssl = SSL_new(qs->ssl_ctx);
	if (!sess->ssl) {
		quic_send_stateless_connection_close(qs, &hd, s_add, s_add_len);
		sess->status = GF_NETIO_DISCONNECTED;
		return GF_IP_CONNECTION_FAILURE;
	}
	SSL_set_accept_state(sess->ssl);

	GF_SAFEALLOC(c, GF_QUICConnection)
	if (!c) return GF_OUT_OF_MEM;
	memcpy(c->dcid, vc->dcid, vc->dcidlen);
	c->dcid_len = vc->dcidlen;
	memcpy(c->addr, s_add, s_add_len);
	c->addr_len = s_add_len;
	c->server = qs;
	gf_list_add(qs->connections, c);

	ngtcp2_path path;
	path.local.addr = (ngtcp2_sockaddr *) qs->local_add;
	path.local.addrlen = qs->local_add_len;
	path.remote.addr = (ngtcp2_sockaddr *) s_add;
	path.remote.addrlen = s_add_len;


	e = h3_initialize(sess, NULL, 0, &hd, pocid, token_type, &path, c);
	NGTCP2Priv *qc = sess->hmux_sess->hmux_udta;
	if (e) {
		gf_list_del_item(qs->connections, c);
		qc->serv_conn = NULL;
		gf_free(qc);
		sess->status = GF_NETIO_DISCONNECTED;
		quic_send_stateless_connection_close(qs, &hd, s_add, s_add_len);
		return GF_IP_CONNECTION_FAILURE;
	}
	int res = gf_quic_on_data(c, data, data_len);
    if (res) {
		if (res == QNET_ERR_RETRY)
			quic_send_retry(qs, &hd, s_add, s_add_len, data_len * 3);

		return GF_IP_NETWORK_EMPTY;
	}
	if (quic_on_write(qc->serv_conn))
		return GF_IP_NETWORK_EMPTY;
	//todo: gather CIDs
	return GF_OK;
}

GF_Err gf_dm_quic_process(GF_QuicServer *qs)
{
	GF_Err e;
	u8 buf[5000];
	u32 nb_read;

restart:

	nb_read=0;
	e = gf_sk_receive(qs->sock, buf, 5000, &nb_read);
	if (e) return e;
	if (nb_read<22) return GF_IP_NETWORK_EMPTY;

	u32 src_add_len = 0;
	const u8 *src_add =  gf_sk_get_address(qs->sock, &src_add_len);
	if (!src_add) return GF_IP_NETWORK_EMPTY;

	ngtcp2_version_cid vc;
	int rv = ngtcp2_pkt_decode_version_cid(&vc, buf, 5000, NGTCP2_SV_SCIDLEN);
	switch (rv) {
	case 0:
		break;
	case NGTCP2_ERR_VERSION_NEGOTIATION:
		//send_version_negotiation(vc.version, {vc.scid, vc.scidlen}, {vc.dcid, vc.dcidlen}, ep, local_addr, sa, salen);
		return GF_IP_NETWORK_EMPTY;
	default:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[QUIC] Could not decode version and CID: %s\n", ngtcp2_strerror(rv) ));
		return GF_IP_NETWORK_EMPTY;
	}

	GF_QUICConnection *qc = NULL;
	u32 i, count = gf_list_count(qs->connections);
	for (i=0; i<count; i++) {
		qc = gf_list_get(qs->connections, i);
		if ((qc->dcid_len == vc.dcidlen) && !memcmp(qc->dcid, vc.dcid, qc->dcid_len)) break;
		qc = NULL;
	}
	if (!qc) {
		e = gf_quic_create_connection(qs, &qc, &vc, buf, nb_read, src_add, src_add_len);
		if (e) return e;

		goto restart;
	}

	NGTCP2Priv *q = qc->hmux->hmux_udta;
	if (ngtcp2_conn_in_closing_period(q->conn)) {
		if (quic_send_conn_close(q) != 0) {
	//      remove(h);
			return GF_IP_NETWORK_EMPTY;
		}
		return GF_IP_NETWORK_EMPTY;
	}
	if (ngtcp2_conn_in_draining_period(q->conn)) {
		return GF_IP_NETWORK_EMPTY;
	}
	int res = gf_quic_on_data(qc, buf, nb_read);
	if (res) {
		if (res != QNET_ERR_CLOSE_WAIT) {
			//remove(h);
			return GF_IP_NETWORK_EMPTY;
		}
		return GF_IP_NETWORK_EMPTY;
	}
	if (quic_on_write(qc))
		return GF_IP_NETWORK_EMPTY;

	goto restart;

	return GF_OK;
}

GF_Err gf_dm_quic_verify(GF_QuicServer *qs)
{
	if (!qs) return GF_OK;

	return GF_OK;
}

#endif // !defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_NGTCP2)
