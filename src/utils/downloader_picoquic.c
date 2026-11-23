/*
 * GPAC Multimedia Framework - HTTP/3 picoquic backend
 *
 * This file is part of GPAC and is distributed under the GNU Lesser
 * General Public License, version 2 or later.
 */

#include "downloader.h"

#if !defined(GPAC_DISABLE_NETWORK) && defined(GPAC_HAS_PICOQUIC)

#include <picoquic.h>
#include <picoquic_utils.h>
#include <picoquic_newreno.h>
#include <picoquic_cubic.h>
#include <picoquic_bbr.h>
#include <picoquic_set_textlog.h>
#include <picoquic_qlog.h>
#include <h3zero.h>
#include <h3zero_common.h>
#include <picotls/openssl.h>
#include <stdlib.h>

GF_Err gf_sk_send_to(GF_Socket *sock, const u8 *buffer, u32 length, const u8 *addr, u32 addr_len, u32 *written);

typedef struct
{
	picoquic_quic_t *quic;
	GF_Socket *socket;
	Bool connected;
	u8 packet[PICOQUIC_MAX_PACKET_SIZE];
	size_t packet_len;
	struct sockaddr_storage packet_to;
} PicoPacketPump;

/* A prepared packet is already committed to picoquic and must survive a
 * blocked UDP send. Only prepare another packet after this one is sent. */
static GF_Err pico_pump_write(PicoPacketPump *pump)
{
	Bool sent = GF_FALSE;

	if (!pump || !pump->quic || !pump->socket) return GF_BAD_PARAM;
	for (;;) {
		if (pump->packet_len) {
			GF_Err e;
			u32 written = 0;
			if (pump->connected) {
				e = gf_sk_send_ex(pump->socket, pump->packet, (u32) pump->packet_len, &written);
			} else {
				int addr_len = picoquic_addr_length((const struct sockaddr *) &pump->packet_to);
				if (addr_len <= 0) return GF_IP_NETWORK_FAILURE;
				e = gf_sk_send_to(pump->socket, pump->packet, (u32) pump->packet_len,
					(const u8 *) &pump->packet_to, (u32) addr_len, &written);
			}
			if (written && written != pump->packet_len) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Partial UDP datagram sent (%u of %u bytes)\n",
					written, (u32) pump->packet_len));
				return GF_IP_NETWORK_FAILURE;
			}
			if (written == pump->packet_len) {
				pump->packet_len = 0;
				sent = GF_TRUE;
				if (e) return e;
				continue;
			}
			return e ? e : GF_IP_NETWORK_EMPTY;
		}
		{
			struct sockaddr_storage from;
			int if_index = 0;
			int ret = picoquic_prepare_next_packet_ex(pump->quic, picoquic_current_time(),
				pump->packet, sizeof(pump->packet), &pump->packet_len,
				&pump->packet_to, &from, &if_index, NULL, NULL, NULL);
			if (ret) {
				pump->packet_len = 0;
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Failed to prepare UDP datagram: %d\n", ret));
				return GF_IP_NETWORK_FAILURE;
			}
		}
		if (!pump->packet_len) return sent ? GF_OK : GF_IP_NETWORK_EMPTY;
	}
}

/* Stream bytes handed to picoquic (headers, DATA framing and body) and the
 * contiguous prefix the peer acknowledged; equal once everything is delivered. */
typedef struct
{
	u64 queued, acknowledged;
} PicoStreamAck;

static GF_Err pico_ack_commit_framing(PicoStreamAck *ack, u64 size)
{
	if (size > GF_UINT64_MAX - ack->queued) return GF_BAD_PARAM;
	ack->queued += size;
	return GF_OK;
}

static GF_Err pico_ack_update(PicoStreamAck *ack, u64 offset)
{
	if (offset < ack->acknowledged || offset > ack->queued) return GF_BAD_PARAM;
	ack->acknowledged = offset;
	return GF_OK;
}

static Bool pico_h3_token_char(u8 c)
{
	return ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
		c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
		c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' ||
		c == '^' || c == '_' || c == '`' || c == '|' || c == '~') ? GF_TRUE : GF_FALSE;
}

typedef struct
{
	const char *name;
	const char *value;
} PicoHeader;

/* Encode a static-QPACK field section in an HTTP/3 HEADERS frame. */
static GF_Err pico_h3_encode_headers(const PicoHeader *fields, u32 count, u8 **out, size_t *out_len)
{
	size_t capacity = 11, max_name = 0, frame_len, prefix_len;
	u8 *frame = NULL, *cursor, *end, *name = NULL, *prefix_end;
	u32 i;
	GF_Err e = GF_BAD_PARAM;

	if (!out || !out_len || (!fields && count)) return GF_BAD_PARAM;
	*out = NULL;
	*out_len = 0;
	for (i = 0; i < count; i++) {
		size_t n, v;
		if (!fields[i].name || !fields[i].value) return GF_BAD_PARAM;
		n = strlen(fields[i].name);
		v = strlen(fields[i].value);
		if (!n || n > SIZE_MAX - 22 || v > SIZE_MAX - n - 22 ||
			capacity > SIZE_MAX - n - v - 22)
			return GF_BAD_PARAM;
		capacity += n + v + 22;
		if (n > max_name) max_name = n;
	}
	if (capacity > UINT32_MAX) return GF_BAD_PARAM;
	frame = gf_malloc(capacity);
	if (!frame) return GF_OUT_OF_MEM;
	if (max_name) {
		name = gf_malloc(max_name);
		if (!name) {
			e = GF_OUT_OF_MEM;
			goto exit;
		}
	}
	/* Reserve the largest QUIC varint for frame length. */
	cursor = frame + 9;
	end = frame + capacity;
	*cursor++ = 0;
	*cursor++ = 0;
	for (i = 0; i < count; i++) {
		size_t j, n = strlen(fields[i].name), v = strlen(fields[i].value);
		for (j = 0; j < n; j++) {
			unsigned char c = (unsigned char) fields[i].name[j];
			if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
			if (!pico_h3_token_char(c) && !(c == ':' && !j && n > 1))
				goto exit;
			name[j] = c;
		}
		for (j = 0; j < v; j++) {
			unsigned char c = (unsigned char) fields[i].value[j];
			if ((c < 0x20 && c != '\t') || c == 0x7f) goto exit;
		}
		cursor = h3zero_qpack_literal_plus_name_encode(cursor, end, name, n,
			(const u8 *) fields[i].value, v);
		if (!cursor) goto exit;
	}
	frame_len = (size_t) (cursor - (frame + 9));
	frame[0] = h3zero_frame_header;
	prefix_end = picoquic_frames_varint_encode(frame + 1, frame + 9, frame_len);
	if (!prefix_end) goto exit;
	prefix_len = (size_t) (prefix_end - (frame + 1));
	memmove(frame + 1 + prefix_len, frame + 9, frame_len);
	*out_len = 1 + prefix_len + frame_len;
	*out = frame;
	frame = NULL;
	e = GF_OK;

exit:
	if (name) gf_free(name);
	if (frame) gf_free(frame);
	return e;
}

static GF_Err pico_h3_send_headers(picoquic_cnx_t *cnx, u64 stream_id, PicoStreamAck *ack,
	const PicoHeader *fields, u32 count, Bool fin, void *stream_ctx)
{
	u8 *frame;
	size_t length;
	GF_Err e;
	if (!cnx || !ack) return GF_BAD_PARAM;
	e = pico_h3_encode_headers(fields, count, &frame, &length);
	if (e) return e;
	if (length > GF_UINT64_MAX - ack->queued) {
		gf_free(frame);
		return GF_BAD_PARAM;
	}
	if (picoquic_add_to_stream_with_ctx(cnx, stream_id, frame, length, fin, stream_ctx)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/3] Failed to queue HEADERS for stream "LLU"\n", stream_id));
		e = GF_IP_NETWORK_FAILURE;
	} else {
		e = pico_ack_commit_framing(ack, length);
	}
	gf_free(frame);
	return e;
}

/* Write one HTTP/3 DATA frame straight into the packet buffer picoquic offers
 * through picoquic_callback_prepare_to_send. Returns GF_IP_NETWORK_EMPTY when
 * nothing is pending and GF_BUFFER_TOO_SMALL when the offered space cannot
 * carry a single body byte; neither consumes the offer. */
static GF_Err pico_h3_fill_data(void *context, size_t space, PicoStreamAck *ack,
	const u8 *data, size_t available, Bool eos, size_t *consumed, Bool *fin)
{
	size_t payload, prefix_len, total;
	u8 *buffer;

	if (!context || !ack || !consumed || !fin || (!data && available)) return GF_BAD_PARAM;
	*consumed = 0;
	*fin = GF_FALSE;
	if (!available) {
		if (!eos) return GF_IP_NETWORK_EMPTY;
		if (!picoquic_provide_stream_data_buffer(context, 0, 1, 0)) return GF_IP_NETWORK_FAILURE;
		*fin = GF_TRUE;
		return GF_OK;
	}
	/* A DATA frame needs the type byte, a length varint and at least one body byte. */
	if (space < 3) return GF_BUFFER_TOO_SMALL;
	payload = (available < space - 2) ? available : space - 2;
	for (;;) {
		prefix_len = 1 + picoquic_frames_varint_encode_length(payload);
		if (prefix_len + payload <= space) break;
		payload = space - prefix_len;
	}
	total = prefix_len + payload;
	if (total > GF_UINT64_MAX - ack->queued) return GF_BAD_PARAM;
	*fin = (eos && payload == available) ? GF_TRUE : GF_FALSE;
	buffer = picoquic_provide_stream_data_buffer(context, total, *fin, (payload < available) ? 1 : 0);
	if (!buffer) return GF_IP_NETWORK_FAILURE;
	buffer[0] = h3zero_frame_data;
	if (!picoquic_frames_varint_encode(buffer + 1, buffer + prefix_len, payload)) return GF_BAD_PARAM;
	memcpy(buffer + prefix_len, data, payload);
	ack->queued += total;
	*consumed = payload;
	return GF_OK;
}

typedef struct
{
	PicoStreamAck ack;
	h3zero_data_stream_state_t parser;
	GF_DownloadSession *sess;
	GF_List *staged_headers;
	u64 id;
	u32 pseudo_fields;
	u32 status_code;
	Bool regular_seen, is_put, is_connect, received_fin;
	GF_Err error;
} PicoH3Stream;

enum {
	PICO_H3_METHOD = 1,
	PICO_H3_SCHEME = 2,
	PICO_H3_AUTHORITY = 4,
	PICO_H3_PATH = 8,
	PICO_H3_STATUS = 16
};

static void pico_h3_clear_headers(PicoH3Stream *stream)
{
	while (gf_list_count(stream->staged_headers)) {
		GF_HTTPHeader *header = gf_list_pop_back(stream->staged_headers);
		gf_free(header->name);
		gf_free(header->value);
		gf_free(header);
	}
	stream->pseudo_fields = stream->status_code = 0;
	stream->regular_seen = GF_FALSE;
}

static int pico_h3_receive_field(const u8 *name, size_t nlen, const u8 *value, size_t vlen,
	int trailer, void *ctx)
{
	PicoH3Stream *stream = ctx;
	GF_HTTPHeader *header;
	u32 pseudo = 0;
	size_t i;
	GF_Err e;

	if (!stream->sess || !nlen || nlen >= UINT32_MAX || vlen >= UINT32_MAX) return -1;
	for (i = 0; i < nlen; i++) {
		u8 c = name[i];
		if (!pico_h3_token_char(c) && !(c == ':' && !i && nlen > 1))
			return -1;
	}
	for (i = 0; i < vlen; i++) {
		if ((value[i] < 0x20 && value[i] != '\t') || value[i] == 0x7f) return -1;
	}
	if (name[0] == ':') {
		if (trailer || stream->regular_seen) return -1;
		if (nlen == 7 && !memcmp(name, ":method", 7)) pseudo = PICO_H3_METHOD;
		else if (nlen == 7 && !memcmp(name, ":scheme", 7)) pseudo = PICO_H3_SCHEME;
		else if (nlen == 10 && !memcmp(name, ":authority", 10)) pseudo = PICO_H3_AUTHORITY;
		else if (nlen == 5 && !memcmp(name, ":path", 5)) pseudo = PICO_H3_PATH;
		else if (nlen == 7 && !memcmp(name, ":status", 7)) pseudo = PICO_H3_STATUS;
		else return -1;
		if ((stream->pseudo_fields & pseudo) ||
			(stream->sess->server_mode ? (pseudo == PICO_H3_STATUS) : (pseudo != PICO_H3_STATUS)))
			return -1;
		if (!vlen) return -1;
		stream->pseudo_fields |= pseudo;
		if (pseudo == PICO_H3_METHOD) {
			for (i = 0; i < vlen; i++) {
				u8 c = value[i];
				if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
				if (!pico_h3_token_char(c)) return -1;
			}
		}
		if (pseudo == PICO_H3_STATUS) {
			if (vlen != 3 || value[0] < '1' || value[0] > '5' ||
				value[1] < '0' || value[1] > '9' || value[2] < '0' || value[2] > '9')
				return -1;
			stream->status_code = (value[0] - '0') * 100 + (value[1] - '0') * 10 + value[2] - '0';
		}
		if (pseudo == PICO_H3_METHOD &&
			((vlen == 3 && !memcmp(value, "PUT", 3)) || (vlen == 4 && !memcmp(value, "POST", 4))))
			stream->is_put = GF_TRUE;
		if (pseudo == PICO_H3_METHOD && vlen == 7 && !memcmp(value, "CONNECT", 7))
			stream->is_connect = GF_TRUE;
	} else {
		stream->regular_seen = GF_TRUE;
		if ((nlen == 10 && !memcmp(name, "connection", 10)) ||
			(nlen == 10 && !memcmp(name, "keep-alive", 10)) ||
			(nlen == 16 && !memcmp(name, "proxy-connection", 16)) ||
			(nlen == 17 && !memcmp(name, "transfer-encoding", 17)) ||
			(nlen == 7 && !memcmp(name, "upgrade", 7)))
			return -1;
		if (nlen == 2 && !memcmp(name, "te", 2) &&
			(vlen != 8 || strnicmp((const char *) value, "trailers", 8)))
			return -1;
	}
	if (trailer) return 0;
	GF_SAFEALLOC(header, GF_HTTPHeader);
	if (!header) {
		stream->error = GF_OUT_OF_MEM;
		return -1;
	}
	header->name = gf_malloc(nlen + 1);
	header->value = gf_malloc(vlen + 1);
	if (!header->name || !header->value) {
		if (header->name) gf_free(header->name);
		if (header->value) gf_free(header->value);
		gf_free(header);
		stream->error = GF_OUT_OF_MEM;
		return -1;
	}
	memcpy(header->name, name, nlen);
	header->name[nlen] = 0;
	memcpy(header->value, value, vlen);
	header->value[vlen] = 0;
	e = gf_list_add(stream->staged_headers, header);
	if (e) {
		gf_free(header->name);
		gf_free(header->value);
		gf_free(header);
		stream->error = e;
		return -1;
	}
	return 0;
}

static int pico_h3_receive_section(h3zero_header_section_type_t type, void *ctx)
{
	PicoH3Stream *stream = ctx;
	GF_DownloadSession *sess = stream->sess;
	GF_HTTPHeader *header;

	if (!sess) return -1;
	if (type == h3zero_header_section_trailer) {
		pico_h3_clear_headers(stream);
		return 0;
	}
	if (sess->server_mode) {
		u32 required = PICO_H3_METHOD | (stream->is_connect ? PICO_H3_AUTHORITY : (PICO_H3_SCHEME | PICO_H3_PATH));
		if (type != h3zero_header_section_initial || (stream->pseudo_fields & required) != required ||
			(stream->is_connect && (stream->pseudo_fields & (PICO_H3_SCHEME | PICO_H3_PATH))))
			return -1;
	} else if (stream->pseudo_fields != PICO_H3_STATUS ||
		((type == h3zero_header_section_informational) != (stream->status_code < 200))) {
		return -1;
	}
	if (type == h3zero_header_section_informational) {
		pico_h3_clear_headers(stream);
		return 0;
	}
	gf_dm_sess_clear_headers(sess);
	while ((header = gf_list_get(stream->staged_headers, 0))) {
		GF_Err e = gf_list_add(sess->headers, header);
		if (e) {
			stream->error = e;
			gf_dm_sess_clear_headers(sess);
			return -1;
		}
		gf_list_rem(stream->staged_headers, 0);
	}
	sess->hmux_headers_seen = 1;
	stream->pseudo_fields = stream->status_code = 0;
	stream->regular_seen = GF_FALSE;
	return 0;
}

static GF_Err pico_h3_receive_data(PicoH3Stream *stream, const u8 *data, size_t length, Bool fin)
{
	u8 *cursor = (u8 *) data;
	u8 *end = cursor ? cursor + length : NULL;
	GF_DownloadSession *sess = stream->sess;
	u64 error = 0;

	if (!sess || (!data && length)) return GF_BAD_PARAM;
	while (cursor && cursor < end) {
		u8 *before = cursor;
		size_t available = 0;
		cursor = h3zero_parse_data_stream(cursor, end, &stream->parser, &available, &error);
		if (!cursor || stream->parser.is_web_transport || available > (size_t) (end - cursor)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/3] Invalid stream "LLU" frame (error "LLU")\n", stream->id, error));
			return stream->error ? stream->error : GF_REMOTE_SERVICE_ERROR;
		}
		if (available) {
			u32 size;
			if (available > UINT32_MAX - sess->hmux_buf.size) return GF_OUT_OF_MEM;
			size = sess->hmux_buf.size + (u32) available;
			if (size > sess->hmux_buf.alloc) {
				u8 *new_data = gf_realloc(sess->hmux_buf.data, size);
				if (!new_data) return GF_OUT_OF_MEM;
				sess->hmux_buf.data = new_data;
				sess->hmux_buf.alloc = size;
			}
			memcpy(sess->hmux_buf.data + sess->hmux_buf.size, cursor, available);
			sess->hmux_buf.size = size;
			cursor += available;
		}
		if (cursor == before) return GF_REMOTE_SERVICE_ERROR;
	}
	if (fin) {
		if (!stream->parser.header_found || stream->parser.frame_prefix_parsed ||
			stream->parser.frame_header_read)
			return GF_REMOTE_SERVICE_ERROR;
		stream->received_fin = GF_TRUE;
		if ((!sess->server_mode || stream->is_put) && !sess->hmux_data_done)
			sess->hmux_is_eos = 1;
	}
	return GF_OK;
}

static void pico_tls_free_verifier(ptls_verify_certificate_t *verifier)
{
	ptls_openssl_dispose_verify_certificate((ptls_openssl_verify_certificate_t *) verifier);
	free(verifier);
}

static GF_Err pico_tls_client_configure(picoquic_quic_t *quic, SSL_CTX *ssl_ctx, Bool broken_cert)
{
	ptls_openssl_verify_certificate_t *verifier;
	X509_STORE *store;

	if (!quic || !ssl_ctx) return GF_BAD_PARAM;
	if (broken_cert) {
		picoquic_set_null_verifier(quic);
		return GF_OK;
	}
	store = SSL_CTX_get_cert_store(ssl_ctx);
	if (!store) return GF_AUTHENTICATION_FAILURE;

	/* picoquic calls this verifier's disposer, which uses the C allocator. */
	verifier = malloc(sizeof(*verifier));
	if (!verifier) return GF_OUT_OF_MEM;
#ifdef PTLS_OPENSSL_VERIFY_CERTIFICATE_ENABLE_OVERRIDE
	if (ptls_openssl_init_verify_certificate(verifier, store, NULL)) {
#else
	if (ptls_openssl_init_verify_certificate(verifier, store)) {
#endif
		free(verifier);
		return GF_AUTHENTICATION_FAILURE;
	}
	picoquic_set_verify_certificate_callback(quic, &verifier->super, pico_tls_free_verifier);
	return GF_OK;
}

static GF_Err pico_transport_configure(picoquic_quic_t *quic, Bool server)
{
	picoquic_tp_t params;
	const picoquic_congestion_algorithm_t *algorithm = NULL;
	const char *name, *qlog_dir;
	Bool trace;
	u32 stream_window;
	if (!quic) return GF_BAD_PARAM;
	name = gf_opts_get_key("temp", "h3-algo");
	if (name) {
		if (!strcmp(name, "cubic")) algorithm = picoquic_cubic_algorithm;
		else if (!strcmp(name, "bbr")) algorithm = picoquic_bbr_algorithm;
		else if (!strcmp(name, "reno")) algorithm = picoquic_newreno_algorithm;
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Unsupported congestion control algorithm %s\n", name));
			return GF_BAD_PARAM;
		}
	}
	params = *picoquic_get_default_tp(quic);
	stream_window = gf_opts_get_int("core", "hx-st-iwnd");
	params.initial_max_data = gf_opts_get_int("core", "hx-iwnd");
	params.initial_max_stream_data_bidi_local = server ? 0 : stream_window;
	params.initial_max_stream_data_bidi_remote = server ? stream_window : 0;
	params.initial_max_stream_data_uni = stream_window;
	if (server)
		params.initial_max_streams_bidi = gf_opts_get_int("core", "hx-max-st");
	if (picoquic_set_default_tp(quic, &params)) return GF_BAD_PARAM;
	if (algorithm) picoquic_set_default_congestion_algorithm(quic, algorithm);
	trace = gf_opts_get_bool("core", "h3-trace");
	qlog_dir = gf_opts_get_key("core", "h3-qlog");
	if (qlog_dir && (!qlog_dir[0] || !gf_dir_exists(qlog_dir))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Qlog directory does not exist: %s\n", qlog_dir));
		return GF_BAD_PARAM;
	}
	if (trace) {
		if (picoquic_set_textlog_stream(quic, stderr)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Failed to enable Picoquic tracing\n"));
			return GF_IO_ERR;
		}
	}
	if (qlog_dir && picoquic_set_qlog(quic, qlog_dir)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Failed to enable Picoquic qlog in %s\n", qlog_dir));
		return GF_IO_ERR;
	}
	if (trace || qlog_dir) {
		picoquic_set_log_level(quic, 1);
	}
	return GF_OK;
}

static GF_Err pico_tls_server_configure(picoquic_quic_t *quic, SSL_CTX *ssl_ctx)
{
	X509 *cert;
	EVP_PKEY *key;
	STACK_OF(X509) *chain = NULL;
	ptls_iovec_t *certs = NULL;
	unsigned char *key_der = NULL, *cursor;
	size_t count = 0, i;
	int length, key_length = 0;
	GF_Err e = GF_AUTHENTICATION_FAILURE;

	if (!quic || !ssl_ctx) return GF_BAD_PARAM;
	cert = SSL_CTX_get0_certificate(ssl_ctx);
	key = SSL_CTX_get0_privatekey(ssl_ctx);
	if (!cert || !key || SSL_CTX_check_private_key(ssl_ctx) != 1)
		return GF_AUTHENTICATION_FAILURE;

	SSL_CTX_get0_chain_certs(ssl_ctx, &chain);
	if (!chain)
		SSL_CTX_get_extra_chain_certs(ssl_ctx, &chain);
	count = 1 + (chain ? (size_t) sk_X509_num(chain) : 0);

	/* picoquic takes ownership and frees both the array and its elements. */
	certs = calloc(count, sizeof(*certs));
	if (!certs) return GF_OUT_OF_MEM;
	for (i = 0; i < count; i++) {
		X509 *item = i ? sk_X509_value(chain, (int) i - 1) : cert;
		length = i2d_X509(item, NULL);
		if (length <= 0) goto exit;
		certs[i].base = malloc((size_t) length);
		if (!certs[i].base) {
			e = GF_OUT_OF_MEM;
			goto exit;
		}
		certs[i].len = (size_t) length;
		cursor = certs[i].base;
		if (i2d_X509(item, &cursor) != length) goto exit;
	}
	key_length = i2d_PrivateKey(key, NULL);
	if (key_length <= 0) goto exit;
	key_der = gf_malloc((size_t) key_length);
	if (!key_der) {
		e = GF_OUT_OF_MEM;
		goto exit;
	}
	cursor = key_der;
	if (i2d_PrivateKey(key, &cursor) != key_length) goto exit;
	if (picoquic_set_tls_key(quic, key_der, (size_t) key_length)) goto exit;
	picoquic_set_tls_certificate_chain(quic, certs, count);
	certs = NULL;
	picoquic_enforce_client_only(quic, 0);
	e = GF_OK;

exit:
	if (key_der) {
		OPENSSL_cleanse(key_der, (size_t) key_length);
		gf_free(key_der);
	}
	if (certs) {
		for (i = 0; i < count; i++) free(certs[i].base);
		free(certs);
	}
	return e;
}

#ifndef GPAC_PICOQUIC_TEST_HELPERS_ONLY

const u8 *gf_sk_get_address(GF_Socket *sock, u32 *addr_size);
GF_Err gf_sk_bind_ex(GF_Socket *sock, const char *ifce_ip_or_name, u16 port,
	const char *peer_name, u16 peer_port, u32 options, u8 **dst_sock_addr,
	u32 *dst_sock_addr_len, u8 **src_sock_addr, u32 *src_sock_addr_len);
GF_Err gf_sk_connect_ex(GF_Socket *sock, const char *peer_name, u16 port,
	const char *ifce_ip_or_name, Bool use_udp_connect);

#define PICO_SEND_LIMIT 200000
#define PICO_STAGE_MIN_ALLOC 100000

typedef struct _pico_connection PicoConnection;

typedef struct
{
	PicoPacketPump pump;
	GF_Mutex *mx;
	GF_List *connections;
	Bool in_pico;
} PicoEngine;

struct __gf_quic_server
{
	PicoEngine engine;
	GF_DownloadManager *dm;
	Bool (*accept_conn)(void *udta, GF_DownloadSession *sess, const char *address, u32 port);
	void *udta;
	u8 *local_addr;
	u32 local_addr_len;
	u8 packet[65536];
};

struct _pico_connection
{
	PicoEngine *engine;
	GF_QuicServer *server;
	GF_HMUX_Session *hmux;
	picoquic_cnx_t *cnx;
	h3zero_callback_ctx_t *control;
	GF_List *streams;
	struct sockaddr_storage peer, local;
	Bool ready, closed, destroy_pending;
};

typedef struct
{
	PicoH3Stream h3;
	PicoConnection *connection;
	/* Bytes of sess->local_buf already pulled by picoquic. */
	u32 sent;
	Bool assigned, released, fin_sent, response_started, send_blocked;
} PicoSessionStream;

static void pico_stream_free(PicoSessionStream *stream)
{
	if (!stream) return;
	pico_h3_clear_headers(&stream->h3);
	gf_list_del(stream->h3.staged_headers);
	h3zero_delete_data_stream_state(&stream->h3.parser);
	gf_free(stream);
}

static GF_Err pico_stream_new(PicoConnection *conn, GF_DownloadSession *sess)
{
	PicoSessionStream *stream;
	GF_Err e;
	GF_SAFEALLOC(stream, PicoSessionStream);
	if (!stream) return GF_OUT_OF_MEM;
	stream->h3.staged_headers = gf_list_new();
	if (!stream->h3.staged_headers) {
		gf_free(stream);
		return GF_OUT_OF_MEM;
	}
	stream->connection = conn;
	stream->h3.sess = sess;
	stream->h3.parser.header_field_callback = pico_h3_receive_field;
	stream->h3.parser.header_field_callback_ctx = &stream->h3;
	stream->h3.parser.header_section_callback = pico_h3_receive_section;
	stream->h3.parser.header_section_callback_ctx = &stream->h3;
	e = gf_list_add(conn->streams, stream);
	if (e) {
		pico_stream_free(stream);
		return e;
	}
	sess->hmux_priv = stream;
	return GF_OK;
}

static void pico_stream_detach(PicoSessionStream *stream)
{
	PicoConnection *conn;
	if (!stream) return;
	conn = stream->connection;
	if (stream->h3.sess) {
		if (stream->h3.sess->hmux_priv == stream)
			stream->h3.sess->hmux_priv = NULL;
		/* Staged bytes belong to this stream and cannot follow the session elsewhere. */
		stream->h3.sess->local_buf_len = 0;
	}
	stream->h3.sess = NULL;
	stream->sent = 0;
	if (!stream->assigned || stream->released) {
		gf_list_del_item(conn->streams, stream);
		pico_stream_free(stream);
	}
}

/* Callers reuse their buffer once gf_dm_sess_send() returns, so keep a copy in
 * sess->local_buf until picoquic pulls it from pico_stream_prepare. */
static GF_Err pico_stage_data(GF_DownloadSession *sess, PicoSessionStream *stream, const u8 *data, u32 size)
{
	u32 needed;
	if (!size) return GF_OK;
	if (!data) return GF_BAD_PARAM;
	if (stream->sent == sess->local_buf_len) {
		sess->local_buf_len = 0;
		stream->sent = 0;
	}
	if (size > UINT32_MAX - sess->local_buf_len) return GF_OUT_OF_MEM;
	needed = sess->local_buf_len + size;
	if (needed > sess->local_buf_alloc && stream->sent) {
		sess->local_buf_len -= stream->sent;
		memmove(sess->local_buf, sess->local_buf + stream->sent, sess->local_buf_len);
		stream->sent = 0;
		needed = sess->local_buf_len + size;
	}
	if (needed > sess->local_buf_alloc) {
		u32 alloc = needed < PICO_STAGE_MIN_ALLOC ? PICO_STAGE_MIN_ALLOC : needed;
		u8 *buf = gf_realloc(sess->local_buf, alloc);
		if (!buf) return GF_OUT_OF_MEM;
		sess->local_buf = buf;
		sess->local_buf_alloc = alloc;
	}
	memcpy(sess->local_buf + sess->local_buf_len, data, size);
	sess->local_buf_len += size;
	return GF_OK;
}

static u32 pico_stream_staged(GF_DownloadSession *sess, PicoSessionStream *stream)
{
	return (sess->local_buf_len > stream->sent) ? sess->local_buf_len - stream->sent : 0;
}

static Bool pico_stream_eos_pending(GF_DownloadSession *sess, PicoSessionStream *stream)
{
	if (!sess->hmux_is_eos || stream->fin_sent) return GF_FALSE;
	if (sess->server_mode && !stream->response_started) return GF_FALSE;
	return GF_TRUE;
}

/* Ask picoquic to poll the stream when it has room; a no-op when nothing is pending. */
static GF_Err pico_stream_arm(PicoConnection *conn, PicoSessionStream *stream, GF_DownloadSession *sess)
{
	if (!stream || !stream->assigned || stream->fin_sent || !conn->cnx) return GF_OK;
	if (sess->server_mode && !stream->response_started) return GF_OK;
	if (!pico_stream_staged(sess, stream) && !pico_stream_eos_pending(sess, stream)) return GF_OK;
	if (picoquic_mark_active_stream(conn->cnx, stream->h3.id, 1, stream)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTP/3] Cannot activate stream "LLU" for sending\n", stream->h3.id));
		return GF_IP_CONNECTION_CLOSED;
	}
	stream->send_blocked = GF_FALSE;
	sess->hmux_data_paused = 0;
	return GF_OK;
}

/* picoquic_callback_prepare_to_send for a request/response stream. */
static int pico_stream_prepare(PicoSessionStream *stream, void *context, size_t space)
{
	GF_DownloadSession *sess = stream->h3.sess;
	size_t consumed = 0;
	Bool fin = GF_FALSE;
	GF_Err e;

	/* Returning without providing data leaves the stream inactive. */
	if (!sess || !stream->assigned || stream->fin_sent) return 0;
	if (sess->server_mode && !stream->response_started) return 0;
	e = pico_h3_fill_data(context, space, &stream->h3.ack, sess->local_buf + stream->sent,
		pico_stream_staged(sess, stream), pico_stream_eos_pending(sess, stream), &consumed, &fin);
	switch (e) {
	case GF_OK:
		break;
	case GF_IP_NETWORK_EMPTY:
		sess->hmux_data_paused = 1;
		return 0;
	case GF_BUFFER_TOO_SMALL:
		/* Flow control left too little credit; pico_stream_arm retries on the next poll. */
		stream->send_blocked = GF_TRUE;
		return 0;
	default:
		sess->last_error = e;
		sess->status = GF_NETIO_STATE_ERROR;
		return -1;
	}
	stream->sent += (u32) consumed;
	if (stream->sent == sess->local_buf_len) {
		sess->local_buf_len = 0;
		stream->sent = 0;
	}
	if (fin) {
		stream->fin_sent = GF_TRUE;
		sess->hmux_data_done = 1;
		sess->hmux_is_eos = 0;
		/* EOS was requested while data was still staged: hmux_send_payload left the
		 * upload state to us, as it does for HTTP/2. */
		if (sess->put_state == 1) {
			sess->put_state = 2;
			sess->status = GF_NETIO_WAIT_FOR_REPLY;
		}
	} else if (!pico_stream_staged(sess, stream)) {
		sess->hmux_data_paused = 1;
	}
	return 0;
}

static GF_Err pico_setup_session(GF_DownloadSession *sess, Bool destroy)
{
	PicoConnection *conn;
	GF_Err e = GF_OK;
	gf_mx_p(sess->mx);
	if (destroy) {
		pico_stream_detach(sess->hmux_priv);
	} else if (!sess->hmux_priv) {
		if (!sess->hmux_sess || !sess->hmux_sess->hmux_udta)
			e = GF_BAD_PARAM;
		else {
			conn = sess->hmux_sess->hmux_udta;
			e = pico_stream_new(conn, sess);
		}
	}
	gf_mx_v(sess->mx);
	return e;
}

static void pico_ack_callback(picoquic_cnx_t *cnx, uint64_t id, uint64_t offset,
	void *ctx, void *stream_ctx)
{
	PicoConnection *conn = ctx;
	PicoSessionStream *stream = stream_ctx;
	(void)cnx;
	if ((id & 2) || !stream || stream->connection != conn) return;
	if (pico_ack_update(&stream->h3.ack, offset)) {
		conn->closed = GF_TRUE;
		if (stream->h3.sess) {
			stream->h3.sess->last_error = GF_IP_NETWORK_FAILURE;
			stream->h3.sess->status = GF_NETIO_STATE_ERROR;
		}
	}
}

static GF_Err pico_engine_write(PicoEngine *engine)
{
	GF_Err e;
	if (engine->in_pico) return GF_OK;
	engine->in_pico = GF_TRUE;
	e = pico_pump_write(&engine->pump);
	engine->in_pico = GF_FALSE;
	return e;
}

static GF_Err pico_write(GF_DownloadSession *sess);

static GF_Err pico_submit_request(GF_DownloadSession *sess, char *method, const char *url,
	const char *params, Bool has_body)
{
	PicoConnection *conn = sess->hmux_sess->hmux_udta;
	PicoSessionStream *stream = sess->hmux_priv;
	PicoHeader *fields;
	char *authority = NULL, *path = NULL;
	char port[20];
	u32 i;
	size_t count = gf_list_count(sess->headers);
	GF_Err e;

	if (!conn || conn->closed || !conn->ready || !stream || !method || !url)
		return GF_IP_CONNECTION_CLOSED;
	if (stream->assigned) {
		if (pico_stream_staged(sess, stream) || pico_stream_eos_pending(sess, stream))
			return GF_IP_NETWORK_EMPTY;
		pico_stream_detach(stream);
		e = pico_stream_new(conn, sess);
		if (e) return e;
		stream = sess->hmux_priv;
	}
	if (count > (UINT32_MAX - 4) || count + 4 > SIZE_MAX / sizeof(*fields))
		return GF_OUT_OF_MEM;
	fields = gf_malloc((count + 4) * sizeof(*fields));
	if (!fields) return GF_OUT_OF_MEM;
	gf_dynstrcat(&authority, sess->server_name, NULL);
	snprintf(port, sizeof(port), ":%u", sess->port);
	gf_dynstrcat(&authority, port, NULL);
	if (params) {
		gf_dynstrcat(&path, url, NULL);
		gf_dynstrcat(&path, params, strchr(url, '?') ? "&" : "?");
	}
	if (!authority || (params && !path)) {
		e = GF_OUT_OF_MEM;
		goto exit;
	}
	fields[0].name = ":method"; fields[0].value = method;
	fields[1].name = ":scheme"; fields[1].value = "https";
	fields[2].name = ":authority"; fields[2].value = authority;
	fields[3].name = ":path"; fields[3].value = path ? path : url;
	for (i = 0; i < count; i++) {
		GF_HTTPHeader *header = gf_list_get(sess->headers, i);
		fields[i + 4].name = header->name;
		fields[i + 4].value = header->value;
	}
	stream->h3.id = picoquic_get_next_local_stream_id(conn->cnx, 0);
	e = pico_h3_send_headers(conn->cnx, stream->h3.id, &stream->h3.ack,
		fields, (u32) count + 4, !has_body, stream);
	if (!e) {
		stream->assigned = GF_TRUE;
		stream->fin_sent = !has_body;
		sess->hmux_stream_id = stream->h3.id;
		sess->hmux_data_done = 0;
		sess->hmux_headers_seen = sess->hmux_ready_to_send = 0;
		e = pico_write(sess);
		if (e == GF_IP_NETWORK_EMPTY) e = GF_OK;
	}
exit:
	if (authority) gf_free(authority);
	if (path) gf_free(path);
	gf_free(fields);
	return e;
}

static GF_Err pico_send_pending(GF_DownloadSession *sess)
{
	PicoConnection *conn = sess->hmux_sess->hmux_udta;
	PicoSessionStream *stream = sess->hmux_priv;
	GF_Err e;
	if (!sess->hmux_send_data || !sess->hmux_send_data_len) return GF_OK;
	if (!conn || conn->closed || !stream || !stream->assigned || stream->fin_sent)
		return GF_IP_CONNECTION_CLOSED;
	e = pico_stage_data(sess, stream, sess->hmux_send_data, sess->hmux_send_data_len);
	if (e) return e;
	sess->hmux_send_data = NULL;
	sess->hmux_send_data_len = 0;
	e = pico_stream_arm(conn, stream, sess);
	if (e) return e;
	e = pico_engine_write(conn->engine);
	return e == GF_IP_NETWORK_EMPTY ? GF_OK : e;
}

static GF_Err pico_write_locked(GF_DownloadSession *sess)
{
	PicoConnection *conn = sess->hmux_sess->hmux_udta;
	PicoSessionStream *stream = sess->hmux_priv;
	GF_Err e;
	if (!conn || conn->closed) return GF_IP_CONNECTION_CLOSED;
	if (stream && stream->assigned && sess->hmux_send_data) {
		e = pico_send_pending(sess);
		if (e && e != GF_IP_NETWORK_EMPTY) return e;
		if (sess->hmux_send_data) return GF_IP_NETWORK_EMPTY;
	}
	e = pico_stream_arm(conn, stream, sess);
	if (e) return e;
	return pico_engine_write(conn->engine);
}

static GF_Err pico_write(GF_DownloadSession *sess)
{
	GF_Err e;
	gf_mx_p(sess->mx);
	e = pico_write_locked(sess);
	gf_mx_v(sess->mx);
	return e;
}

static GF_Err pico_send_reply_locked(GF_DownloadSession *sess, u32 code, const char *body,
	u32 body_len, Bool no_body)
{
	PicoConnection *conn = sess->hmux_sess->hmux_udta;
	PicoSessionStream *stream = sess->hmux_priv;
	PicoHeader *fields;
	char status[16];
	u32 i;
	size_t count = gf_list_count(sess->headers);
	GF_Err e;
	if (!conn || conn->closed || !stream || !stream->assigned) return GF_IP_CONNECTION_CLOSED;
	if (body) no_body = GF_FALSE;
	else if (!no_body && code != 200 && code != 206) no_body = GF_TRUE;
	if (count == UINT32_MAX || count + 1 > SIZE_MAX / sizeof(*fields)) return GF_OUT_OF_MEM;
	fields = gf_malloc((count + 1) * sizeof(*fields));
	if (!fields) return GF_OUT_OF_MEM;
	snprintf(status, sizeof(status), "%u", code);
	fields[0].name = ":status"; fields[0].value = status;
	for (i = 0; i < count; i++) {
		GF_HTTPHeader *header = gf_list_get(sess->headers, i);
		fields[i + 1].name = header->name;
		fields[i + 1].value = header->value;
	}
	e = pico_h3_send_headers(conn->cnx, stream->h3.id, &stream->h3.ack,
		fields, (u32) count + 1, no_body, stream);
	gf_free(fields);
	if (e) return e;
	stream->response_started = GF_TRUE;
	stream->fin_sent = no_body;
	sess->hmux_data_done = no_body;
	sess->hmux_is_eos = 0;
	if (body) {
		e = pico_stage_data(sess, stream, (const u8 *) body, body_len);
		if (e) return e;
		sess->hmux_is_eos = 1;
	}
	e = pico_stream_arm(conn, stream, sess);
	if (e) return e;
	e = pico_engine_write(conn->engine);
	return e == GF_IP_NETWORK_EMPTY ? GF_OK : e;
}

static GF_Err pico_send_reply(GF_DownloadSession *sess, u32 code, const char *body,
	u32 body_len, Bool no_body)
{
	GF_Err e;
	gf_mx_p(sess->mx);
	e = pico_send_reply_locked(sess, code, body, body_len, no_body);
	gf_mx_v(sess->mx);
	return e;
}

static PicoConnection *pico_connection_new(PicoEngine *engine, GF_QuicServer *server,
	picoquic_cnx_t *cnx)
{
	PicoConnection *conn;
	GF_SAFEALLOC(conn, PicoConnection);
	if (!conn) return NULL;
	conn->engine = engine;
	conn->server = server;
	conn->cnx = cnx;
	conn->streams = gf_list_new();
	conn->control = h3zero_callback_create_context(NULL);
	if (!conn->streams || !conn->control || gf_list_add(engine->connections, conn)) {
		if (conn->streams) gf_list_del(conn->streams);
		if (conn->control) h3zero_callback_delete_context(NULL, conn->control);
		gf_free(conn);
		return NULL;
	}
	picoquic_set_stream_ack_callback(cnx, pico_ack_callback, conn);
	return conn;
}

static void pico_connection_free(PicoConnection *conn)
{
	PicoSessionStream *stream;
	if (conn->cnx) {
		picoquic_set_stream_ack_callback(conn->cnx, NULL, NULL);
		picoquic_set_callback(conn->cnx, NULL, NULL);
		if (conn->server) picoquic_close(conn->cnx, 0);
	}
	while ((stream = gf_list_pop_back(conn->streams))) {
		if (stream->h3.sess && stream->h3.sess->hmux_priv == stream)
			stream->h3.sess->hmux_priv = NULL;
		pico_stream_free(stream);
	}
	gf_list_del(conn->streams);
	h3zero_callback_delete_context(NULL, conn->control);
	gf_list_del_item(conn->engine->connections, conn);
	gf_free(conn);
}

static void pico_engine_collect(PicoEngine *engine)
{
	u32 i = 0;
	if (engine->in_pico) return;
	while (i < gf_list_count(engine->connections)) {
		PicoConnection *conn = gf_list_get(engine->connections, i);
		if (conn->destroy_pending) pico_connection_free(conn);
		else i++;
	}
}

static void pico_stream_reset(GF_DownloadSession *sess, Bool abort)
{
	PicoConnection *conn;
	PicoSessionStream *stream;
	gf_mx_p(sess->mx);
	conn = sess->hmux_sess->hmux_udta;
	stream = sess->hmux_priv;
	if (abort && conn && !conn->closed && stream && stream->assigned) {
		if (picoquic_reset_stream(conn->cnx, stream->h3.id, H3ZERO_REQUEST_CANCELLED) ||
			picoquic_stop_sending(conn->cnx, stream->h3.id, H3ZERO_REQUEST_CANCELLED))
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[HTTP/3] Could not reset stream "LLU"\n", stream->h3.id));
	}
	gf_mx_v(sess->mx);
}

static GF_Err pico_resume(GF_DownloadSession *sess)
{
	PicoConnection *conn = sess->hmux_sess->hmux_udta;
	if (!conn || conn->closed) return GF_IP_CONNECTION_CLOSED;
	sess->hmux_data_paused = 0;
	return pico_stream_arm(conn, sess->hmux_priv, sess);
}

static void pico_close(GF_HMUX_Session *hmux)
{
	PicoConnection *conn = hmux->hmux_udta;
	if (conn && conn->cnx && !conn->closed) {
		GF_Err e;
		picoquic_close(conn->cnx, 0);
		e = pico_engine_write(conn->engine);
		if (e && e != GF_IP_NETWORK_EMPTY)
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[QUIC] Failed to send connection close: %s\n", gf_error_to_string(e)));
	}
}

static void pico_destroy(GF_HMUX_Session *hmux)
{
	PicoConnection *conn = hmux->hmux_udta;
	PicoEngine *engine;
	Bool server;
	u32 i;
	if (!conn) return;
	engine = conn->engine;
	server = conn->server ? GF_TRUE : GF_FALSE;
	for (i = 0; i < gf_list_count(conn->streams); i++) {
		PicoSessionStream *stream = gf_list_get(conn->streams, i);
		if (stream->h3.sess && stream->h3.sess->hmux_priv == stream)
			stream->h3.sess->hmux_priv = NULL;
		stream->h3.sess = NULL;
	}
	conn->hmux = NULL;
	conn->destroy_pending = GF_TRUE;
	if (engine->in_pico) return;
	pico_engine_collect(engine);
	if (!server) {
		picoquic_free(engine->pump.quic);
		gf_list_del(engine->connections);
		gf_free(engine);
	}
}

static void pico_close_session(GF_DownloadSession *sess)
{
	gf_mx_p(sess->mx);
	pico_close(sess->hmux_sess);
	sess->status = GF_NETIO_DISCONNECTED;
	gf_mx_v(sess->mx);
}

static int pico_callback(picoquic_cnx_t *cnx, uint64_t id, uint8_t *bytes, size_t length,
	picoquic_call_back_event_t event, void *ctx, void *stream_ctx)
{
	PicoConnection *conn = ctx;
	PicoSessionStream *stream = stream_ctx;
	GF_DownloadSession *sess;
	GF_Err e;
	if (!conn || conn->destroy_pending) return -1;
	switch (event) {
	case picoquic_callback_almost_ready:
	case picoquic_callback_ready:
		if (h3zero_protocol_init_safe(cnx, conn->control)) return -1;
		conn->ready = GF_TRUE;
		if (conn->hmux) conn->hmux->connected = GF_TRUE;
		return 0;
	case picoquic_callback_close:
	case picoquic_callback_application_close:
	case picoquic_callback_stateless_reset:
		{
			u64 local_error = picoquic_get_local_error(cnx);
			u64 remote_error = picoquic_get_remote_error(cnx);
			if ((local_error & ~(u64) 0xff) == 0x100) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] TLS handshake failed (alert "LLU")\n", local_error & 0xff));
			} else if (local_error || remote_error) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Connection closed with local error "LLU" and remote error "LLU"\n",
					local_error, remote_error));
			}
		}
		conn->closed = GF_TRUE;
		picoquic_set_stream_ack_callback(cnx, NULL, NULL);
		picoquic_set_callback(cnx, NULL, NULL);
		conn->cnx = NULL;
		return 0;
	case picoquic_callback_stream_data:
	case picoquic_callback_stream_fin:
		if (id & 2)
			return h3zero_callback(cnx, id, bytes, length, event, conn->control, stream_ctx);
		if (!stream && conn->server && conn->hmux && conn->hmux->net_sess) {
			sess = hmux_get_session(conn->hmux, (s64)id, GF_TRUE);
			if (!sess && conn->hmux->net_sess->hmux_stream_id == -2) {
				sess = conn->hmux->net_sess;
				sess->hmux_stream_id = (s64)id;
			}
			if (!sess && conn->hmux->net_sess->user_proc) {
				GF_NETIO_Parameter param;
				GF_DownloadSession *parent = conn->hmux->net_sess;
				memset(&param, 0, sizeof(param));
				param.msg_type = GF_NETIO_REQUEST_SESSION;
				param.sess = parent;
				param.stream_id = id;
				parent->in_callback = GF_TRUE;
				parent->user_proc(parent->usr_cbk, &param);
				parent->in_callback = GF_FALSE;
				if (conn->destroy_pending || !conn->hmux) return -1;
				if (!param.error) sess = hmux_get_session(conn->hmux, (s64)id, GF_FALSE);
			}
			if (!sess || !sess->hmux_priv) return -1;
			stream = sess->hmux_priv;
			if (stream->assigned) {
				pico_stream_detach(stream);
				if (pico_stream_new(conn, sess)) return -1;
				stream = sess->hmux_priv;
			}
			stream->assigned = GF_TRUE;
			stream->h3.id = id;
			sess->hmux_stream_id = (s64)id;
			if (picoquic_set_app_stream_ctx(cnx, id, stream)) return -1;
		}
		if (!stream || stream->h3.id != id) return -1;
		if (!stream->h3.sess) return 0;
		e = pico_h3_receive_data(&stream->h3, bytes, length, event == picoquic_callback_stream_fin);
		if (e) {
			stream->h3.sess->last_error = e;
			stream->h3.sess->status = GF_NETIO_STATE_ERROR;
			return -1;
		}
		return 0;
	case picoquic_callback_prepare_to_send:
		if (id & 2)
			return h3zero_callback(cnx, id, bytes, length, event, conn->control, stream_ctx);
		if (!stream || stream->h3.id != id || stream->connection != conn) return 0;
		return pico_stream_prepare(stream, bytes, length);
	case picoquic_callback_stream_released:
		if (id & 2)
			return h3zero_callback(cnx, id, bytes, length, event, conn->control, stream_ctx);
		if (stream) {
			stream->released = GF_TRUE;
			if (stream->h3.sess && stream->h3.sess->hmux_stream_id == (s64)id)
				stream->h3.sess->hmux_stream_id = -1;
			else if (!stream->h3.sess)
				pico_stream_detach(stream);
		}
		return 0;
	case picoquic_callback_stream_reset:
	case picoquic_callback_stop_sending:
		if (id & 2)
			return h3zero_callback(cnx, id, bytes, length, event, conn->control, stream_ctx);
		if (stream && stream->h3.sess) {
			stream->h3.sess->last_error = GF_IP_CONNECTION_CLOSED;
			stream->h3.sess->status = GF_NETIO_STATE_ERROR;
		}
		return 0;
	default:
		return 0;
	}
}

/* Streams that found no flow-control credit stay inactive until new credit may have arrived. */
static void pico_connection_rearm(PicoConnection *conn)
{
	u32 i;
	if (conn->closed || conn->destroy_pending) return;
	for (i = 0; i < gf_list_count(conn->streams); i++) {
		PicoSessionStream *stream = gf_list_get(conn->streams, i);
		if (stream->send_blocked && stream->h3.sess)
			pico_stream_arm(conn, stream, stream->h3.sess);
	}
}

static GF_Err pico_data_received(GF_DownloadSession *sess, const u8 *data, u32 size)
{
	PicoConnection *conn = sess->hmux_sess->hmux_udta;
	PicoEngine *engine = conn->engine;
	GF_Err e = GF_OK;
	if (conn->closed) return GF_IP_CONNECTION_CLOSED;
	if (engine->in_pico) return GF_IP_NETWORK_EMPTY;
	if (size) {
		u8 *packet = gf_malloc(size);
		picoquic_cnx_t *first_cnx = NULL;
		int ret;
		if (!packet) return GF_OUT_OF_MEM;
		memcpy(packet, data, size);
		engine->in_pico = GF_TRUE;
		ret = picoquic_incoming_packet_ex(engine->pump.quic, packet, size,
			(struct sockaddr *)&conn->peer, (struct sockaddr *)&conn->local,
			0, 0, &first_cnx, picoquic_current_time());
		engine->in_pico = GF_FALSE;
		gf_free(packet);
		if (ret) e = GF_IP_NETWORK_FAILURE;
		else pico_connection_rearm(conn);
	}
	if (!e) e = pico_engine_write(engine);
	pico_engine_collect(engine);
	return e == GF_IP_NETWORK_EMPTY ? GF_OK : e;
}

static GF_Err pico_async_flush(GF_DownloadSession *sess, Bool for_close)
{
	PicoConnection *conn;
	PicoSessionStream *stream;
	GF_Err e;
	gf_mx_p(sess->mx);
	conn = sess->hmux_sess->hmux_udta;
	stream = sess->hmux_priv;
	if (!conn || conn->closed) {
		gf_mx_v(sess->mx);
		return GF_IP_CONNECTION_CLOSED;
	}
	if (!sess->server_mode && !conn->engine->in_pico) {
		char buf[2048];
		u32 read = 0;
		e = gf_dm_sess_read(sess, buf, sizeof(buf), &read);
		if (e && e != GF_IP_NETWORK_EMPTY) {
			gf_mx_v(sess->mx);
			return e;
		}
	}
	e = pico_write(sess);
	if (e == GF_IP_NETWORK_EMPTY) e = GF_OK;
	if (!e && stream && stream->assigned) {
		u32 staged = pico_stream_staged(sess, stream);
		if (staged >= PICO_SEND_LIMIT || pico_stream_eos_pending(sess, stream))
			e = GF_IP_NETWORK_EMPTY;
		else if (for_close && (staged || stream->h3.ack.acknowledged < stream->h3.ack.queued ||
			(sess->put_state && !stream->h3.received_fin)))
			e = GF_IP_NETWORK_EMPTY;
	}
	gf_mx_v(sess->mx);
	return e;
}

static GF_Err pico_attach(PicoConnection *conn, GF_DownloadSession *sess, GF_Mutex *mx,
	Bool shared_mx)
{
	GF_HMUX_Session *hmux;
	GF_Err e;
	GF_SAFEALLOC(hmux, GF_HMUX_Session);
	if (!hmux) return GF_OUT_OF_MEM;
	hmux->sessions = gf_list_new();
	if (!hmux->sessions) { gf_free(hmux); return GF_OUT_OF_MEM; }
	if (shared_mx) gf_mx_ref(mx);
	hmux->net_sess = sess;
	hmux->mx = mx;
	hmux->shared_mx = shared_mx;
	hmux->hmux_udta = conn;
	hmux->setup_session = pico_setup_session;
	hmux->close_session = pico_close_session;
	hmux->submit_request = pico_submit_request;
	hmux->send_reply = pico_send_reply;
	hmux->write = pico_write;
	hmux->stream_reset = pico_stream_reset;
	hmux->resume = pico_resume;
	hmux->close = pico_close;
	hmux->destroy = pico_destroy;
	hmux->data_received = pico_data_received;
	hmux->send_pending_data = pico_send_pending;
	hmux->async_flush = pico_async_flush;
	e = gf_list_add(hmux->sessions, sess);
	if (e) {
		gf_list_del(hmux->sessions);
		gf_free(hmux);
		if (shared_mx) gf_mx_del(mx);
		return e;
	}
	conn->hmux = hmux;
	sess->hmux_sess = hmux;
	sess->mx = mx;
	e = pico_setup_session(sess, GF_FALSE);
	if (e) {
		sess->hmux_sess = NULL;
		conn->hmux = NULL;
		gf_list_del(hmux->sessions);
		gf_free(hmux);
	}
	return e;
}

static int pico_server_callback(picoquic_cnx_t *cnx, uint64_t id, uint8_t *bytes, size_t length,
	picoquic_call_back_event_t event, void *ctx, void *stream_ctx)
{
	GF_QuicServer *server = ctx;
	PicoConnection *conn;
	GF_DownloadSession *sess;
	struct sockaddr *peer = NULL;
	char address[GF_MAX_IP_NAME_LEN];
	GF_Err e = GF_OK;

	conn = pico_connection_new(&server->engine, server, cnx);
	if (!conn) return -1;
	picoquic_get_peer_addr(cnx, &peer);
	if (!peer || !picoquic_addr_text(peer, address, sizeof(address))) {
		pico_connection_free(conn);
		return -1;
	}
	sess = gf_dm_sess_new_internal(server->dm, NULL, GF_NETIO_SESSION_NO_BLOCK,
		NULL, NULL, NULL, GF_TRUE, &e);
	if (!sess || e) {
		pico_connection_free(conn);
		return -1;
	}
	sess->flags |= GF_NETIO_SESSION_USE_QUIC;
	e = pico_attach(conn, sess, server->engine.mx, GF_TRUE);
	if (e) {
		gf_dm_sess_del(sess);
		pico_connection_free(conn);
		return -1;
	}
	sess->hmux_stream_id = -2;
	picoquic_set_callback(cnx, pico_callback, conn);
	if (!server->accept_conn(server->udta, sess, address, picoquic_get_addr_port(peer))) {
		gf_dm_sess_del(sess);
		return -1;
	}
	return pico_callback(cnx, id, bytes, length, event, conn, stream_ctx);
}

GF_Err http3_connect(GF_DownloadSession *sess, char *server, u32 server_port)
{
	PicoEngine *engine;
	PicoConnection *conn;
	GF_Err e;
	u32 read = 0;
	if (sess->hmux_sess) {
		conn = sess->hmux_sess->hmux_udta;
		if (!conn || conn->closed) return GF_IP_CONNECTION_FAILURE;
		e = gf_dm_sess_read(sess, sess->http_buf, sess->http_buf_size, &read);
		if (e && e != GF_IP_NETWORK_EMPTY) return e;
		if (conn->closed) return GF_IP_CONNECTION_FAILURE;
		return conn->ready ? GF_OK : GF_IP_NETWORK_EMPTY;
	}
	if (!sess->dm || !server || !server_port) return GF_BAD_PARAM;
	if (!sess->ssl) gf_ssl_try_connect(sess, NULL);
	if (!sess->ssl || !sess->dm->ssl_ctx) return GF_IO_ERR;
	GF_SAFEALLOC(engine, PicoEngine);
	if (!engine) return GF_OUT_OF_MEM;
	engine->connections = gf_list_new();
	engine->pump.socket = sess->sock;
	engine->pump.connected = GF_TRUE;
	engine->pump.quic = picoquic_create(1, NULL, NULL, NULL, "h3", NULL, NULL,
		NULL, NULL, NULL, picoquic_current_time(), NULL, NULL, NULL, 0);
	if (!engine->connections || !engine->pump.quic) {
		e = GF_OUT_OF_MEM;
		goto fail_engine;
	}
	e = pico_transport_configure(engine->pump.quic, GF_FALSE);
	if (e) goto fail_engine;
	e = pico_tls_client_configure(engine->pump.quic, sess->dm->ssl_ctx,
		sess->dm->allow_broken_certificate);
	if (e) goto fail_engine;
	{
		u8 *dst = NULL, *src = NULL;
		u32 dst_len = 0, src_len = 0, port = 1234;
		while (port < 65535) {
			e = gf_sk_bind_ex(sess->sock, "127.0.0.1", port, server, server_port, 0,
				&dst, &dst_len, &src, &src_len);
			if (e != GF_IP_CONNECTION_FAILURE) break;
			port++;
		}
		if (!e) e = gf_sk_connect_ex(sess->sock, server, server_port, NULL, GF_TRUE);
		if (!e && (!dst || !src || dst_len > sizeof(conn->peer) || src_len > sizeof(conn->local)))
			e = GF_IP_CONNECTION_FAILURE;
		if (e) {
			if (dst) gf_free(dst);
			if (src) gf_free(src);
			goto fail_engine;
		}
		{
			picoquic_cnx_t *cnx = picoquic_create_cnx(engine->pump.quic,
				picoquic_null_connection_id, picoquic_null_connection_id,
				(struct sockaddr *)dst, picoquic_current_time(), 0, sess->server_name, "h3", 1);
			if (!cnx) {
				gf_free(dst); gf_free(src);
				e = GF_IP_CONNECTION_FAILURE;
				goto fail_engine;
			}
			conn = pico_connection_new(engine, NULL, cnx);
			if (!conn) {
				gf_free(dst); gf_free(src);
				e = GF_OUT_OF_MEM;
				goto fail_engine;
			}
			memcpy(&conn->peer, dst, dst_len);
			memcpy(&conn->local, src, src_len);
			gf_free(dst); gf_free(src);
			picoquic_set_callback(cnx, pico_callback, conn);
			picoquic_set_local_addr(cnx, (struct sockaddr *)&conn->local);
		}
	}
	if (!sess->mx) sess->mx = gf_mx_new("http3-picoquic");
	if (!sess->mx) { e = GF_OUT_OF_MEM; goto fail_connection; }
	engine->mx = sess->mx;
	e = pico_attach(conn, sess, sess->mx, GF_FALSE);
	if (e) goto fail_connection;
	if (picoquic_start_client_cnx(conn->cnx)) {
		e = GF_IP_CONNECTION_FAILURE;
		goto fail_attached;
	}
	e = pico_engine_write(engine);
	if (e && e != GF_IP_NETWORK_EMPTY) goto fail_attached;
	return GF_IP_NETWORK_EMPTY;

fail_attached:
	gf_list_del(sess->hmux_sess->sessions);
	gf_free(sess->hmux_sess);
	sess->hmux_sess = NULL;
	conn->hmux = NULL;
fail_connection:
	pico_connection_free(conn);
fail_engine:
	GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Client initialization failed: %s\n", gf_error_to_string(e)));
	if (engine->pump.quic) picoquic_free(engine->pump.quic);
	if (engine->connections) gf_list_del(engine->connections);
	gf_free(engine);
	return e;
}

void gf_dm_sess_set_callback(GF_DownloadSession *sess, gf_dm_user_io user_io, void *usr_cbk)
{
	if (!sess) return;
	sess->user_proc = user_io;
	sess->usr_cbk = usr_cbk;
}

GF_Err gf_dm_quic_server_new(GF_DownloadManager *dm, void *ssl_ctx, GF_QuicServer **out,
	const char *ip, u32 port, const char *netcap_id,
	Bool (*accept_conn)(void *udta, GF_DownloadSession *sess, const char *address, u32 port),
	void *udta)
{
	GF_QuicServer *server;
	GF_Err e;
	if (!dm || !ssl_ctx || !out || !port || !accept_conn) return GF_BAD_PARAM;
	*out = NULL;
	GF_SAFEALLOC(server, GF_QuicServer);
	if (!server) return GF_OUT_OF_MEM;
	server->dm = dm;
	server->accept_conn = accept_conn;
	server->udta = udta;
	server->engine.mx = gf_mx_new("picoquic-server");
	server->engine.connections = gf_list_new();
	server->engine.pump.socket = gf_sk_new_ex(GF_SOCK_TYPE_UDP, netcap_id);
	server->engine.pump.quic = picoquic_create(1024, NULL, NULL, NULL, "h3",
		pico_server_callback, server, NULL, NULL, NULL, picoquic_current_time(), NULL, NULL, NULL, 0);
	if (!server->engine.mx || !server->engine.connections ||
		!server->engine.pump.socket || !server->engine.pump.quic) {
		e = GF_OUT_OF_MEM;
		goto fail;
	}
	e = pico_transport_configure(server->engine.pump.quic, GF_TRUE);
	if (e) goto fail;
	e = pico_tls_server_configure(server->engine.pump.quic, ssl_ctx);
	if (e) goto fail;
	gf_sk_set_block_mode(server->engine.pump.socket, GF_TRUE);
	e = gf_sk_bind_ex(server->engine.pump.socket, NULL, port, ip, 0, GF_SOCK_REUSE_PORT,
		NULL, NULL, &server->local_addr, &server->local_addr_len);
	if (e) goto fail;
	gf_sk_set_block_mode(server->engine.pump.socket, GF_TRUE);
	gf_sk_server_mode(server->engine.pump.socket, GF_TRUE);
	*out = server;
	return GF_OK;
fail:
	if (server->engine.pump.quic) picoquic_free(server->engine.pump.quic);
	if (server->engine.pump.socket) gf_sk_del(server->engine.pump.socket);
	if (server->engine.connections) gf_list_del(server->engine.connections);
	if (server->engine.mx) gf_mx_del(server->engine.mx);
	if (server->local_addr) gf_free(server->local_addr);
	gf_free(server);
	return e;
}

void gf_dm_quic_server_del(GF_QuicServer *server)
{
	PicoEngine *engine;
	u32 i;
	if (!server) return;
	engine = &server->engine;
	gf_mx_p(engine->mx);
	engine->in_pico = GF_TRUE;
	for (i = 0; i < gf_list_count(engine->connections); i++) {
		PicoConnection *conn = gf_list_get(engine->connections, i);
		while (conn->hmux && gf_list_count(conn->hmux->sessions)) {
			GF_DownloadSession *sess = gf_list_last(conn->hmux->sessions);
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[QUIC] Server destroyed with active HTTP sessions\n"));
			gf_mx_p(sess->mx);
			sess->hmux_sess->setup_session(sess, GF_TRUE);
			hmux_detach_session_ex(sess->hmux_sess, sess, GF_TRUE);
			gf_mx_v(sess->mx);
		}
		if (conn->hmux) conn->hmux->hmux_udta = NULL;
	}
	engine->in_pico = GF_FALSE;
	while (gf_list_count(engine->connections)) {
		PicoConnection *conn = gf_list_get(engine->connections, 0);
		pico_connection_free(conn);
	}
	picoquic_free(server->engine.pump.quic);
	gf_list_del(server->engine.connections);
	gf_sk_del(server->engine.pump.socket);
	if (server->local_addr) gf_free(server->local_addr);
	gf_mx_v(engine->mx);
	gf_mx_del(engine->mx);
	gf_free(server);
}

GF_Socket *gf_dm_quic_get_socket(GF_QuicServer *server)
{
	return server ? server->engine.pump.socket : NULL;
}

GF_Err gf_dm_quic_process(GF_QuicServer *server)
{
	PicoEngine *engine;
	GF_Err e;
	u32 size;
	if (!server) return GF_BAD_PARAM;
	engine = &server->engine;
	gf_mx_p(engine->mx);
	if (engine->in_pico) { gf_mx_v(engine->mx); return GF_IP_NETWORK_EMPTY; }
	for (;;) {
		u32 addr_len;
		const u8 *addr;
		picoquic_cnx_t *first_cnx = NULL;
		int ret;
		size = 0;
		e = gf_sk_receive(engine->pump.socket, server->packet, sizeof(server->packet), &size);
		if (e || !size) break;
		addr = gf_sk_get_address(engine->pump.socket, &addr_len);
		if (!addr || !addr_len) { e = GF_IP_NETWORK_FAILURE; break; }
		engine->in_pico = GF_TRUE;
		ret = picoquic_incoming_packet_ex(engine->pump.quic, server->packet, size,
			(struct sockaddr *)addr, (struct sockaddr *)server->local_addr, 0, 0, &first_cnx,
			picoquic_current_time());
		engine->in_pico = GF_FALSE;
		pico_engine_collect(engine);
		if (ret) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_HTTP, ("[QUIC] Failed to process datagram: %d\n", ret));
		}
	}
	{
		GF_Err write_error;
		u32 i;
		for (i = 0; i < gf_list_count(engine->connections); i++)
			pico_connection_rearm(gf_list_get(engine->connections, i));
		write_error = pico_engine_write(engine);
		pico_engine_collect(engine);
		if (write_error && write_error != GF_IP_NETWORK_EMPTY) e = write_error;
	}
	gf_mx_v(engine->mx);
	return e;
}

GF_Err gf_dm_quic_verify(GF_QuicServer *server)
{
	GF_Err e;
	if (!server) return GF_IP_NETWORK_EMPTY;
	gf_mx_p(server->engine.mx);
	e = pico_engine_write(&server->engine);
	pico_engine_collect(&server->engine);
	gf_mx_v(server->engine.mx);
	return e;
}

s32 gf_dm_quic_next_wake(GF_QuicServer *server)
{
	int64_t delay;
	if (!server) return -1;
	gf_mx_p(server->engine.mx);
	if (!server->engine.pump.packet_len && !gf_list_count(server->engine.connections)) {
		gf_mx_v(server->engine.mx);
		return -1;
	}
	delay = server->engine.pump.packet_len ? 1 :
		picoquic_get_next_wake_delay(server->engine.pump.quic, picoquic_current_time(), 50000);
	gf_mx_v(server->engine.mx);
	return delay > 0 ? (s32) delay : 1;
}

GF_Err h3_check_sess(GF_DownloadSession *sess)
{
	PicoConnection *conn = sess->hmux_sess->hmux_udta;
	return (!conn || conn->closed) ? GF_IP_CONNECTION_CLOSED : GF_IP_NETWORK_EMPTY;
}

#endif /* !GPAC_PICOQUIC_TEST_HELPERS_ONLY */

#endif
