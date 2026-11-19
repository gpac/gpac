#include "tests.h"

#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
#include "../downloader.h"
#include <picoquic.h>
#include <picoquic_utils.h>
#include <picoquic_newreno.h>
#include <picoquic_cubic.h>
#include <picoquic_bbr.h>
#include <picoquic_set_textlog.h>
#include <picoquic_qlog.h>
#if !defined(WIN32)
#include <fcntl.h>
#endif
#if !defined(WIN32) && !defined(GPAC_DISABLE_THREADS)
#include <pthread.h>
#endif

static int test_pico_prepare(picoquic_quic_t *quic, uint64_t now, uint8_t *buffer, size_t capacity,
	size_t *length, struct sockaddr_storage *to, struct sockaddr_storage *from, int *if_index,
	picoquic_connection_id_t *log_cid, picoquic_cnx_t **last_cnx, size_t *send_msg_size);
static GF_Err test_pico_send(GF_Socket *sock, const u8 *buffer, u32 length, u32 *written);
static GF_Err test_pico_send_to(GF_Socket *sock, const u8 *buffer, u32 length,
	const u8 *addr, u32 addr_len, u32 *written);
static int test_pico_add_to_stream(picoquic_cnx_t *cnx, uint64_t stream_id,
	const uint8_t *data, size_t length, int fin, void *stream_ctx);
static int test_pico_mark_active(picoquic_cnx_t *cnx, uint64_t stream_id, int is_active, void *stream_ctx);
static uint8_t *test_pico_provide_buffer(void *context, size_t length, int is_fin, int is_still_active);
static u32 test_pico_option(const char *section, const char *name);
static const char *test_pico_option_key(const char *section, const char *name);
static Bool test_pico_option_bool(const char *section, const char *name);
static void test_pico_set_congestion_algorithm(picoquic_quic_t *quic, const picoquic_congestion_algorithm_t *algorithm);
static int test_pico_set_textlog_stream(picoquic_quic_t *quic, FILE *stream);
static int test_pico_set_qlog(picoquic_quic_t *quic, const char *directory);
static void test_pico_set_log_level(picoquic_quic_t *quic, int level);
static GF_Err test_gf_dm_sess_read(GF_DownloadSession *sess, char *data, u32 size, u32 *read);
static GF_DownloadSession *test_gf_dm_sess_new_internal(GF_DownloadManager *dm,
	const char *url, u32 flags, gf_dm_user_io user_io, void *user_data, GF_Socket *sock,
	Bool server, GF_Err *error);
static const u8 *test_gf_sk_get_address(GF_Socket *sock, u32 *size);
static GF_Err test_gf_sk_connect_ex(GF_Socket *sock, const char *peer, u16 port,
	const char *interface_name, Bool connected);
static SSLConnectStatus test_gf_ssl_try_connect(GF_DownloadSession *sess, const char *proxy);
static GF_DownloadSession *test_hmux_get_session(void *data, s64 stream_id, Bool reassign);
static void *test_pico_delete_session(void *data);

#define picoquic_prepare_next_packet_ex test_pico_prepare
#define gf_sk_send_ex test_pico_send
#define gf_sk_send_to test_pico_send_to
#define picoquic_add_to_stream_with_ctx test_pico_add_to_stream
#define picoquic_mark_active_stream test_pico_mark_active
#define picoquic_provide_stream_data_buffer test_pico_provide_buffer
#define gf_opts_get_int test_pico_option
#define gf_opts_get_key test_pico_option_key
#define gf_opts_get_bool test_pico_option_bool
#define picoquic_set_default_congestion_algorithm test_pico_set_congestion_algorithm
#define picoquic_set_textlog_stream test_pico_set_textlog_stream
#define picoquic_set_qlog test_pico_set_qlog
#define picoquic_set_log_level test_pico_set_log_level
#define gf_dm_sess_read test_gf_dm_sess_read
#define gf_dm_sess_new_internal test_gf_dm_sess_new_internal
#define gf_sk_get_address test_gf_sk_get_address
#define gf_sk_connect_ex test_gf_sk_connect_ex
#define gf_ssl_try_connect test_gf_ssl_try_connect
#define hmux_get_session test_hmux_get_session
#define http3_connect test_http3_connect
#define gf_dm_sess_set_callback test_gf_dm_sess_set_callback
#define gf_dm_quic_server_new test_gf_dm_quic_server_new
#define gf_dm_quic_server_del test_gf_dm_quic_server_del
#define gf_dm_quic_get_socket test_gf_dm_quic_get_socket
#define gf_dm_quic_process test_gf_dm_quic_process
#define gf_dm_quic_verify test_gf_dm_quic_verify
#define gf_dm_quic_next_wake test_gf_dm_quic_next_wake
#define h3_check_sess test_h3_check_sess
#include "../downloader_picoquic.c"
#undef http3_connect
#undef gf_dm_sess_set_callback
#undef gf_dm_quic_server_new
#undef gf_dm_quic_server_del
#undef gf_dm_quic_get_socket
#undef gf_dm_quic_process
#undef gf_dm_quic_verify
#undef gf_dm_quic_next_wake
#undef h3_check_sess
#undef picoquic_prepare_next_packet_ex
#undef gf_sk_send_ex
#undef gf_sk_send_to
#undef picoquic_add_to_stream_with_ctx
#undef picoquic_mark_active_stream
#undef picoquic_provide_stream_data_buffer
#undef gf_opts_get_int
#undef gf_opts_get_key
#undef gf_opts_get_bool
#undef picoquic_set_default_congestion_algorithm
#undef picoquic_set_textlog_stream
#undef picoquic_set_qlog
#undef picoquic_set_log_level
#undef gf_dm_sess_read
#undef gf_dm_sess_new_internal
#undef gf_sk_get_address
#undef gf_sk_connect_ex
#undef gf_ssl_try_connect
#undef hmux_get_session

static const char *test_pico_algorithm;
static const picoquic_congestion_algorithm_t *test_pico_selected_algorithm;
static u32 test_pico_algorithm_calls;
static Bool test_pico_trace;
static Bool test_pico_trace_fail;
static u32 test_pico_trace_calls;
static FILE *test_pico_trace_stream;
static const char *test_pico_qlog;
static const char *test_pico_qlog_directory;
static Bool test_pico_qlog_fail;
static u32 test_pico_qlog_calls;
static u32 test_pico_log_level_calls;
static int test_pico_log_level;

static const char *test_pico_option_key(const char *section, const char *name)
{
	if (!strcmp(section, "temp") && !strcmp(name, "h3-algo")) return test_pico_algorithm;
	if (!strcmp(section, "core") && !strcmp(name, "h3-qlog")) return test_pico_qlog;
	return NULL;
}

static int test_pico_set_qlog(picoquic_quic_t *quic, const char *directory)
{
	test_pico_qlog_calls++;
	test_pico_qlog_directory = directory;
	if (test_pico_qlog_fail) return -1;
	return picoquic_set_qlog(quic, directory);
}

static void test_pico_set_log_level(picoquic_quic_t *quic, int level)
{
	test_pico_log_level_calls++;
	test_pico_log_level = level;
	picoquic_set_log_level(quic, level);
}

static Bool test_pico_option_bool(const char *section, const char *name)
{
	return !strcmp(section, "core") && !strcmp(name, "h3-trace") && test_pico_trace;
}

static int test_pico_set_textlog_stream(picoquic_quic_t *quic, FILE *stream)
{
	test_pico_trace_calls++;
	test_pico_trace_stream = stream;
	if (test_pico_trace_fail) return -1;
	return picoquic_set_textlog_stream(quic, stream);
}

static void test_pico_set_congestion_algorithm(picoquic_quic_t *quic, const picoquic_congestion_algorithm_t *algorithm)
{
	test_pico_algorithm_calls++;
	test_pico_selected_algorithm = algorithm;
	picoquic_set_default_congestion_algorithm(quic, algorithm);
}

static u32 test_pico_option(const char *section, const char *name)
{
	(void) section;
	if (!strcmp(name, "hx-iwnd")) return 123456;
	if (!strcmp(name, "hx-st-iwnd")) return 23456;
	if (!strcmp(name, "hx-max-st")) return 7;
	return 0;
}

static GF_Err test_gf_dm_sess_read(GF_DownloadSession *sess, char *data, u32 size, u32 *read)
{
	(void) sess; (void) data; (void) size;
	*read = 0;
	return GF_IP_NETWORK_EMPTY;
}

static GF_DownloadSession *test_gf_dm_sess_new_internal(GF_DownloadManager *dm,
	const char *url, u32 flags, gf_dm_user_io user_io, void *user_data, GF_Socket *sock,
	Bool server, GF_Err *error)
{
	(void) dm; (void) url; (void) flags; (void) user_io; (void) user_data;
	(void) sock; (void) server;
	*error = GF_NOT_SUPPORTED;
	return NULL;
}

static const u8 *test_gf_sk_get_address(GF_Socket *sock, u32 *size)
{
	(void) sock;
	*size = 0;
	return NULL;
}

static GF_Err test_gf_sk_connect_ex(GF_Socket *sock, const char *peer, u16 port,
	const char *interface_name, Bool connected)
{
	(void) sock; (void) peer; (void) port; (void) interface_name; (void) connected;
	return GF_IP_CONNECTION_FAILURE;
}

static SSLConnectStatus test_gf_ssl_try_connect(GF_DownloadSession *sess, const char *proxy)
{
	(void) sess; (void) proxy;
	return SSL_CONNECT_WAIT;
}

static GF_DownloadSession *test_hmux_get_session(void *data, s64 stream_id, Bool reassign)
{
	(void) data; (void) stream_id; (void) reassign;
	return NULL;
}

static void *test_pico_delete_session(void *data)
{
	gf_dm_sess_del(data);
	return NULL;
}

static struct {
	u32 prepared, sends, addressed_sends;
	GF_Err send_result;
	int prepare_result;
	u32 written;
	u8 last_packet;
} pump_test;

static struct {
	u8 frame[512];
	size_t length;
	int fin, result;
	u64 stream_id;
	void *stream_ctx;
} header_test;

static int test_pico_add_to_stream(picoquic_cnx_t *cnx, uint64_t stream_id,
	const uint8_t *data, size_t length, int fin, void *stream_ctx)
{
	(void) cnx;
	if (header_test.result) return header_test.result;
	if (length > sizeof(header_test.frame)) return -1;
	if (length) memcpy(header_test.frame, data, length);
	header_test.length = length;
	header_test.fin = fin;
	header_test.stream_id = stream_id;
	header_test.stream_ctx = stream_ctx;
	return 0;
}

/* Stand-in for the packet buffer picoquic hands to prepare_to_send. */
static struct {
	u8 buffer[2048];
	size_t space, length;
	int fin, still_active, provide_calls, fail;
	u32 marks;
	int mark_result;
	u64 marked_stream;
	void *marked_ctx;
} pull_test;

static uint8_t *test_pico_provide_buffer(void *context, size_t length, int is_fin, int is_still_active)
{
	if (context != &pull_test) return NULL;
	pull_test.provide_calls++;
	if (pull_test.fail || length > pull_test.space) return NULL;
	pull_test.length = length;
	pull_test.fin = is_fin;
	pull_test.still_active = is_still_active;
	return pull_test.buffer;
}

static int test_pico_mark_active(picoquic_cnx_t *cnx, uint64_t stream_id, int is_active, void *stream_ctx)
{
	(void) cnx;
	if (!is_active) return 0;
	pull_test.marks++;
	pull_test.marked_stream = stream_id;
	pull_test.marked_ctx = stream_ctx;
	return pull_test.mark_result;
}

/* Run one prepare_to_send poll against the mock buffer and decode the DATA frame it produced. */
static GF_Err test_pico_poll(PicoSessionStream *stream, size_t space, const u8 **payload, u64 *payload_len)
{
	u64 frame_len = 0;
	size_t prefix;
	memset(pull_test.buffer, 0, sizeof(pull_test.buffer));
	pull_test.space = space;
	pull_test.length = 0;
	pull_test.fin = pull_test.still_active = 0;
	*payload = NULL;
	*payload_len = 0;
	if (pico_stream_prepare(stream, &pull_test, space)) return GF_IP_NETWORK_FAILURE;
	if (!pull_test.length) return GF_OK;
	if (pull_test.buffer[0] != h3zero_frame_data) return GF_NON_COMPLIANT_BITSTREAM;
	prefix = h3zero_varint_decode(pull_test.buffer + 1, pull_test.length - 1, &frame_len);
	if (!prefix || 1 + prefix + frame_len != pull_test.length) return GF_NON_COMPLIANT_BITSTREAM;
	*payload = pull_test.buffer + 1 + prefix;
	*payload_len = frame_len;
	return GF_OK;
}

static Bool test_pico_pull_init(PicoConnection *conn, PicoSessionStream *stream, GF_DownloadSession *sess, Bool server)
{
	char *fake_cnx = "cnx";
	memset(conn, 0, sizeof(*conn));
	memset(stream, 0, sizeof(*stream));
	memset(sess, 0, sizeof(*sess));
	memset(&pull_test, 0, sizeof(pull_test));
	conn->cnx = (picoquic_cnx_t *) fake_cnx;
	conn->ready = GF_TRUE;
	stream->connection = conn;
	stream->h3.sess = sess;
	stream->h3.id = 4;
	stream->assigned = GF_TRUE;
	stream->response_started = server;
	sess->server_mode = server;
	sess->hmux_priv = stream;
	return GF_TRUE;
}

static void test_pico_pull_clear(GF_DownloadSession *sess)
{
	if (sess->local_buf) gf_free(sess->local_buf);
	sess->local_buf = NULL;
	sess->local_buf_len = sess->local_buf_alloc = 0;
}

static int test_pico_header_field(const uint8_t *name, size_t name_length,
	const uint8_t *value, size_t value_length, int is_trailer, void *ctx)
{
	static const char *names[] = { ":method", ":path", "x-test", "set-cookie", "set-cookie" };
	static const char *values[] = { "GET", "/resource", "Some Value", "a=1", "b=2" };
	u32 *index = ctx;
	if (*index >= GF_ARRAY_LENGTH(names) || is_trailer ||
		strlen(names[*index]) != name_length || memcmp(names[*index], name, name_length) ||
		strlen(values[*index]) != value_length || memcmp(values[*index], value, value_length))
		return -1;
	(*index)++;
	return 0;
}

static Bool test_pico_receive_init(PicoH3Stream *stream, GF_DownloadSession *sess, Bool server)
{
	memset(stream, 0, sizeof(*stream));
	memset(sess, 0, sizeof(*sess));
	sess->server_mode = server;
	sess->headers = gf_list_new();
	stream->staged_headers = gf_list_new();
	if (!sess->headers || !stream->staged_headers) {
		if (sess->headers) gf_list_del(sess->headers);
		if (stream->staged_headers) gf_list_del(stream->staged_headers);
		return GF_FALSE;
	}
	stream->sess = sess;
	stream->parser.header_field_callback = pico_h3_receive_field;
	stream->parser.header_field_callback_ctx = stream;
	stream->parser.header_section_callback = pico_h3_receive_section;
	stream->parser.header_section_callback_ctx = stream;
	return GF_TRUE;
}

static void test_pico_receive_clear(PicoH3Stream *stream)
{
	GF_DownloadSession *sess = stream->sess;
	pico_h3_clear_headers(stream);
	gf_list_del(stream->staged_headers);
	h3zero_delete_data_stream_state(&stream->parser);
	gf_dm_sess_clear_headers(sess);
	gf_list_del(sess->headers);
	if (sess->hmux_buf.data) gf_free(sess->hmux_buf.data);
}

static int test_pico_prepare(picoquic_quic_t *quic, uint64_t now, uint8_t *buffer, size_t capacity,
	size_t *length, struct sockaddr_storage *to, struct sockaddr_storage *from, int *if_index,
	picoquic_connection_id_t *log_cid, picoquic_cnx_t **last_cnx, size_t *send_msg_size)
{
	(void) quic; (void) now; (void) capacity; (void) from; (void) if_index;
	(void) log_cid; (void) last_cnx; (void) send_msg_size;
	pump_test.prepared++;
	*length = pump_test.prepared <= 2 ? 2 : 0;
	if (*length) {
		buffer[0] = (u8) pump_test.prepared;
		buffer[1] = (u8) (pump_test.prepared + 10);
	}
	to->ss_family = AF_INET;
	return pump_test.prepare_result;
}

static GF_Err test_pico_send(GF_Socket *sock, const u8 *buffer, u32 length, u32 *written)
{
	(void) sock; (void) length;
	pump_test.sends++;
	pump_test.last_packet = buffer[0];
	*written = pump_test.written;
	return pump_test.send_result;
}

static GF_Err test_pico_send_to(GF_Socket *sock, const u8 *buffer, u32 length,
	const u8 *addr, u32 addr_len, u32 *written)
{
	(void) addr; (void) addr_len;
	pump_test.addressed_sends++;
	return test_pico_send(sock, buffer, length, written);
}
#endif

unittest(pico_udp_connect_nonblocking)
{
#if defined(GPAC_HAS_PICOQUIC) && defined(GPAC_HAS_IPV6) && !defined(GPAC_DISABLE_NETWORK) && !defined(WIN32)
	GF_Socket *sock = gf_sk_new(GF_SOCK_TYPE_UDP);
	int flags;
	assert_true(sock != NULL);
	if (!sock) return;
	assert_equal(gf_sk_set_block_mode(sock, GF_TRUE), GF_OK, "%d");
	assert_equal(gf_sk_connect(sock, "127.0.0.1", 12345, NULL), GF_OK, "%d");
	flags = fcntl(gf_sk_get_handle(sock), F_GETFL);
	assert_true(flags >= 0);
	assert_true(flags & O_NONBLOCK);
	gf_sk_del(sock);
#endif
}

unittest(pico_receive_fragmented_response)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoH3Stream stream;
	GF_DownloadSession sess;
	PicoHeader interim[] = { { ":status", "103" }, { "link", "</style.css>" } };
	PicoHeader final[] = { { ":status", "200" }, { "X-Test", "value" } };
	const u8 data[] = { 0, 5, 'h', 'e', 'l', 'l', 'o' };
	u8 *frame = NULL;
	size_t length = 0;
	Bool ok = test_pico_receive_init(&stream, &sess, GF_FALSE);

	assert_true(ok);
	if (!ok) return;
	assert_equal(pico_h3_encode_headers(interim, GF_ARRAY_LENGTH(interim), &frame, &length), GF_OK, "%d");
	if (!frame) goto exit;
	assert_equal(pico_h3_receive_data(&stream, frame, length, GF_FALSE), GF_OK, "%d");
	assert_false(sess.hmux_headers_seen);
	assert_equal(gf_list_count(sess.headers), 0, "%u");
	gf_free(frame);
	frame = NULL;
	assert_equal(pico_h3_encode_headers(final, GF_ARRAY_LENGTH(final), &frame, &length), GF_OK, "%d");
	if (!frame) goto exit;
	assert_equal(pico_h3_receive_data(&stream, frame, 1, GF_FALSE), GF_OK, "%d");
	assert_false(sess.hmux_headers_seen);
	assert_equal(pico_h3_receive_data(&stream, frame + 1, length - 1, GF_FALSE), GF_OK, "%d");
	assert_true(sess.hmux_headers_seen);
	assert_equal(gf_list_count(sess.headers), 2, "%u");
	assert_equal(pico_h3_receive_data(&stream, data, 3, GF_FALSE), GF_OK, "%d");
	assert_equal(pico_h3_receive_data(&stream, data + 3, sizeof(data) - 3, GF_TRUE), GF_OK, "%d");
	assert_equal(sess.hmux_buf.size, 5, "%u");
	if (sess.hmux_buf.data && sess.hmux_buf.size == 5)
		assert_equal_mem(sess.hmux_buf.data, "hello", 5);
	assert_true(sess.hmux_is_eos);
exit:
	if (frame) gf_free(frame);
	test_pico_receive_clear(&stream);
#endif
}

unittest(pico_receive_request_trailers)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoH3Stream stream;
	GF_DownloadSession sess;
	PicoHeader request[] = { { ":method", "POST" }, { ":scheme", "https" }, { ":path", "/" } };
	PicoHeader trailer[] = { { "x-trailer", "ok" } };
	u8 *frame = NULL;
	size_t length = 0;
	Bool ok = test_pico_receive_init(&stream, &sess, GF_TRUE);

	assert_true(ok);
	if (!ok) return;
	assert_equal(pico_h3_encode_headers(request, GF_ARRAY_LENGTH(request), &frame, &length), GF_OK, "%d");
	if (!frame) goto exit;
	assert_equal(pico_h3_receive_data(&stream, frame, length, GF_FALSE), GF_OK, "%d");
	assert_true(stream.is_put);
	gf_free(frame);
	frame = NULL;
	assert_equal(pico_h3_encode_headers(trailer, GF_ARRAY_LENGTH(trailer), &frame, &length), GF_OK, "%d");
	if (!frame) goto exit;
	assert_equal(pico_h3_receive_data(&stream, frame, length, GF_TRUE), GF_OK, "%d");
	assert_true(sess.hmux_is_eos);
	assert_true(stream.is_put);
exit:
	if (frame) gf_free(frame);
	test_pico_receive_clear(&stream);
#endif
}

unittest(pico_receive_upload_response)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoH3Stream stream;
	GF_DownloadSession sess;
	PicoHeader response[] = { { ":status", "405" } };
	u8 *frame = NULL;
	size_t length = 0;
	Bool ok = test_pico_receive_init(&stream, &sess, GF_FALSE);

	assert_true(ok);
	if (!ok) return;
	sess.put_state = 2;
	sess.status = GF_NETIO_WAIT_FOR_REPLY;
	assert_equal(pico_h3_encode_headers(response, GF_ARRAY_LENGTH(response), &frame, &length), GF_OK, "%d");
	if (frame) {
		assert_equal(pico_h3_receive_data(&stream, frame, length, GF_TRUE), GF_OK, "%d");
		assert_true(stream.received_fin);
		assert_true(sess.hmux_headers_seen);
		assert_equal(sess.status, GF_NETIO_WAIT_FOR_REPLY, "%d");
		assert_equal(sess.put_state, 2, "%d");
		gf_free(frame);
	}
	test_pico_receive_clear(&stream);
#endif
}

unittest(pico_receive_invalid_headers)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoH3Stream stream;
	GF_DownloadSession sess;
	PicoHeader invalid[] = { { ":status", "200" }, { "x-test", "value" }, { ":status", "201" } };
	u8 *frame = NULL;
	size_t length = 0;
	Bool ok = test_pico_receive_init(&stream, &sess, GF_FALSE);

	assert_true(ok);
	if (!ok) return;
	assert_equal(pico_h3_encode_headers(invalid, GF_ARRAY_LENGTH(invalid), &frame, &length), GF_OK, "%d");
	if (frame) {
		assert_equal(pico_h3_receive_data(&stream, frame, length, GF_FALSE), GF_REMOTE_SERVICE_ERROR, "%d");
		assert_false(sess.hmux_headers_seen);
		assert_equal(gf_list_count(sess.headers), 0, "%u");
		gf_free(frame);
	}
	test_pico_receive_clear(&stream);
#endif
}

unittest(pico_receive_truncated_data)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoH3Stream stream;
	GF_DownloadSession sess;
	PicoHeader response[] = { { ":status", "200" } };
	const u8 incomplete[] = { 0, 5, 'h', 'i' };
	u8 *frame = NULL;
	size_t length = 0;
	Bool ok = test_pico_receive_init(&stream, &sess, GF_FALSE);

	assert_true(ok);
	if (!ok) return;
	assert_equal(pico_h3_encode_headers(response, GF_ARRAY_LENGTH(response), &frame, &length), GF_OK, "%d");
	if (frame) {
		assert_equal(pico_h3_receive_data(&stream, frame, length, GF_FALSE), GF_OK, "%d");
		assert_equal(pico_h3_receive_data(&stream, incomplete, sizeof(incomplete), GF_TRUE), GF_REMOTE_SERVICE_ERROR, "%d");
		assert_false(sess.hmux_is_eos);
		gf_free(frame);
	}
	test_pico_receive_clear(&stream);
#endif
}

unittest(pico_transport_limits)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	picoquic_quic_t *quic = picoquic_create(1, NULL, NULL, NULL, "h3", NULL, NULL,
		NULL, NULL, NULL, picoquic_current_time(), NULL, NULL, NULL, 0);
	const picoquic_tp_t *params;
	assert_not_null(quic);
	if (!quic) return;
	assert_equal(pico_transport_configure(quic, GF_FALSE), GF_OK, "%d");
	params = picoquic_get_default_tp(quic);
	assert_true(params->initial_max_data == 123456);
	assert_true(params->initial_max_stream_data_bidi_local == 23456);
	assert_true(params->initial_max_stream_data_bidi_remote == 0);
	assert_true(params->initial_max_stream_data_uni == 23456);
	assert_equal(pico_transport_configure(quic, GF_TRUE), GF_OK, "%d");
	params = picoquic_get_default_tp(quic);
	assert_true(params->initial_max_data == 123456);
	assert_true(params->initial_max_stream_data_bidi_local == 0);
	assert_true(params->initial_max_stream_data_bidi_remote == 23456);
	assert_true(params->initial_max_stream_data_uni == 23456);
	assert_true(params->initial_max_streams_bidi == 7);
	picoquic_free(quic);
#endif
}

unittest(pico_congestion_control_option)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	picoquic_quic_t *quic = picoquic_create(1, NULL, NULL, NULL, "h3", NULL, NULL,
		NULL, NULL, NULL, picoquic_current_time(), NULL, NULL, NULL, 0);
	const char *names[] = { "cubic", "bbr", "reno" };
	const picoquic_congestion_algorithm_t *algorithms[] = {
		picoquic_cubic_algorithm, picoquic_bbr_algorithm, picoquic_newreno_algorithm
	};
	u32 i, server;
	assert_not_null(quic);
	if (!quic) return;
	for (server = 0; server < 2; server++) {
		test_pico_algorithm = NULL;
		test_pico_algorithm_calls = 0;
		assert_equal(pico_transport_configure(quic, server), GF_OK, "%d");
		assert_equal(test_pico_algorithm_calls, 0, "%u");
		for (i = 0; i < GF_ARRAY_LENGTH(names); i++) {
			test_pico_algorithm = names[i];
			test_pico_algorithm_calls = 0;
			test_pico_selected_algorithm = NULL;
			assert_equal(pico_transport_configure(quic, server), GF_OK, "%d");
			assert_equal(test_pico_algorithm_calls, 1, "%u");
			assert_true(test_pico_selected_algorithm == algorithms[i]);
		}
		test_pico_algorithm = "invalid";
		test_pico_algorithm_calls = 0;
		assert_equal(pico_transport_configure(quic, server), GF_BAD_PARAM, "%d");
		assert_equal(test_pico_algorithm_calls, 0, "%u");
	}
	test_pico_algorithm = NULL;
	picoquic_free(quic);
#endif
}

unittest(pico_trace_option)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	u32 server;
	for (server = 0; server < 2; server++) {
		picoquic_quic_t *quic = picoquic_create(1, NULL, NULL, NULL, "h3", NULL, NULL,
			NULL, NULL, NULL, picoquic_current_time(), NULL, NULL, NULL, 0);
		assert_not_null(quic);
		if (!quic) return;
		test_pico_trace = GF_FALSE;
		test_pico_trace_calls = 0;
		assert_equal(pico_transport_configure(quic, server), GF_OK, "%d");
		assert_equal(test_pico_trace_calls, 0, "%u");
		test_pico_trace = GF_TRUE;
		test_pico_trace_fail = GF_TRUE;
		assert_equal(pico_transport_configure(quic, server), GF_IO_ERR, "%d");
		assert_equal(test_pico_trace_calls, 1, "%u");
		test_pico_trace_fail = GF_FALSE;
		test_pico_trace_calls = 0;
		test_pico_trace_stream = NULL;
		assert_equal(pico_transport_configure(quic, server), GF_OK, "%d");
		assert_equal(test_pico_trace_calls, 1, "%u");
		assert_true(test_pico_trace_stream == stderr);
		picoquic_free(quic);
	}
	test_pico_trace = GF_FALSE;
#endif
}

unittest(pico_qlog_option)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	u32 server;
	for (server = 0; server < 2; server++) {
		picoquic_quic_t *quic = picoquic_create(1, NULL, NULL, NULL, "h3", NULL, NULL,
			NULL, NULL, NULL, picoquic_current_time(), NULL, NULL, NULL, 0);
		assert_not_null(quic);
		if (!quic) return;
		test_pico_qlog = NULL;
		test_pico_qlog_calls = 0;
		test_pico_log_level_calls = 0;
		assert_equal(pico_transport_configure(quic, server), GF_OK, "%d");
		assert_equal(test_pico_qlog_calls, 0, "%u");
		assert_equal(test_pico_log_level_calls, 0, "%u");
		test_pico_qlog = "";
		assert_equal(pico_transport_configure(quic, server), GF_BAD_PARAM, "%d");
		assert_equal(test_pico_qlog_calls, 0, "%u");
		test_pico_qlog = ".";
		test_pico_qlog_fail = GF_TRUE;
		assert_equal(pico_transport_configure(quic, server), GF_IO_ERR, "%d");
		assert_equal(test_pico_qlog_calls, 1, "%u");
		test_pico_qlog_fail = GF_FALSE;
		test_pico_qlog_calls = 0;
		test_pico_qlog_directory = NULL;
		test_pico_log_level_calls = 0;
		assert_equal(pico_transport_configure(quic, server), GF_OK, "%d");
		assert_equal(test_pico_qlog_calls, 1, "%u");
		assert_true(test_pico_qlog_directory && !strcmp(test_pico_qlog_directory, "."));
		assert_equal(test_pico_log_level_calls, 1, "%u");
		assert_equal(test_pico_log_level, 1, "%d");
		picoquic_free(quic);
	}
	test_pico_qlog = NULL;
#endif
}

unittest(pico_trace_stream_lifetime)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK) && !defined(WIN32)
	picoquic_quic_t *quic;
	picoquic_cnx_t *cnx;
	struct sockaddr_in addr;
	FILE *stream = tmpfile();
	FILE *other = tmpfile();
	char output[4096];
	size_t length;
	long marker_offset;
	int fd;

	assert_not_null(stream);
	assert_not_null(other);
	if (!stream || !other) {
		if (stream) fclose(stream);
		if (other) fclose(other);
		return;
	}
	fd = fileno(stream);
	quic = picoquic_create(1, NULL, NULL, NULL, "h3", NULL, NULL,
		NULL, NULL, NULL, picoquic_current_time(), NULL, NULL, NULL, 0);
	assert_not_null(quic);
	if (!quic) {
		fclose(stream);
		fclose(other);
		return;
	}
	assert_equal(picoquic_set_textlog_stream(quic, stream), 0, "%d");
	assert_equal(picoquic_set_textlog_stream(quic, other), -1, "%d");
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	cnx = picoquic_create_cnx(quic, picoquic_null_connection_id, picoquic_null_connection_id,
		(struct sockaddr *) &addr, picoquic_current_time(), 0, "localhost", "h3", 1);
	assert_not_null(cnx);
	if (cnx) assert_equal(picoquic_start_client_cnx(cnx), 0, "%d");
	marker_offset = ftell(stream);
	if (cnx) picoquic_log_app_message(cnx, "GPAC trace marker");
	picoquic_free(quic);
	assert_true(fcntl(fd, F_GETFD) != -1);
	if (fcntl(fd, F_GETFD) == -1) {
		fclose(other);
		return;
	}
	assert_true(marker_offset >= 0);
	if (marker_offset >= 0) fseek(stream, marker_offset, SEEK_SET);
	length = fread(output, 1, sizeof(output) - 1, stream);
	output[length] = 0;
	assert_true(strstr(output, "GPAC trace marker") != NULL);
	fclose(stream);
	fclose(other);
#endif
}

unittest(pico_shared_mutex_lifetime)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	GF_HMUX_Session *hmux;
	GF_DownloadSession session = {0};
	GF_Mutex *mx = gf_mx_new("pico-test-shared");

	assert_not_null(mx);
	if (!mx) return;
	GF_SAFEALLOC(hmux, GF_HMUX_Session);
	assert_not_null(hmux);
	if (!hmux) {
		gf_mx_del(mx);
		return;
	}
	hmux->sessions = gf_list_new();
	assert_not_null(hmux->sessions);
	if (!hmux->sessions) {
		gf_free(hmux);
		gf_mx_del(mx);
		return;
	}
	hmux->mx = mx;
	hmux->shared_mx = GF_TRUE;
	session.mx = mx;
	session.hmux_sess = hmux;
	assert_equal(gf_list_add(hmux->sessions, &session), GF_OK, "%d");
	gf_mx_ref(mx);
	gf_mx_p(mx);
	hmux_detach_session(hmux, &session);
	assert_true(session.mx == NULL);
	assert_true(session.hmux_sess == NULL);
	gf_mx_p(mx);
	gf_mx_v(mx);
	gf_mx_del(mx);
#endif
}

unittest(pico_server_destroy_with_live_sessions)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK) && !defined(GPAC_DISABLE_THREADS) && !defined(WIN32)
	GF_QuicServer *server;
	PicoConnection *conn;
	GF_DownloadSession *session;
	GF_DownloadSession *sub_session;
	pthread_t thread;
	GF_Mutex *mx;
	GF_Err e = GF_OK;
	int thread_err;

	GF_SAFEALLOC(server, GF_QuicServer);
	assert_not_null(server);
	if (!server) return;
	server->engine.mx = gf_mx_new("pico-test-server");
	server->engine.connections = gf_list_new();
	server->engine.pump.socket = gf_sk_new(GF_SOCK_TYPE_UDP);
	server->engine.pump.quic = picoquic_create(1, NULL, NULL, NULL, "h3", NULL, NULL,
		NULL, NULL, NULL, picoquic_current_time(), NULL, NULL, NULL, 0);
	assert_true(server->engine.mx && server->engine.connections &&
		server->engine.pump.socket && server->engine.pump.quic);
	if (!server->engine.mx || !server->engine.connections ||
		!server->engine.pump.socket || !server->engine.pump.quic) {
		if (server->engine.mx) gf_mx_del(server->engine.mx);
		if (server->engine.connections) gf_list_del(server->engine.connections);
		if (server->engine.pump.socket) gf_sk_del(server->engine.pump.socket);
		if (server->engine.pump.quic) picoquic_free(server->engine.pump.quic);
		gf_free(server);
		return;
	}
	GF_SAFEALLOC(conn, PicoConnection);
	assert_not_null(conn);
	if (!conn) {
		test_gf_dm_quic_server_del(server);
		return;
	}
	conn->server = server;
	conn->engine = &server->engine;
	conn->streams = gf_list_new();
	conn->control = h3zero_callback_create_context(NULL);
	assert_true(conn->streams && conn->control);
	if (!conn->streams || !conn->control) {
		if (conn->streams) gf_list_del(conn->streams);
		if (conn->control) h3zero_callback_delete_context(NULL, conn->control);
		gf_free(conn);
		test_gf_dm_quic_server_del(server);
		return;
	}
	e = gf_list_add(server->engine.connections, conn);
	assert_equal(e, GF_OK, "%d");
	if (e) {
		gf_list_del(conn->streams);
		h3zero_callback_delete_context(NULL, conn->control);
		gf_free(conn);
		test_gf_dm_quic_server_del(server);
		return;
	}
	GF_SAFEALLOC(session, GF_DownloadSession);
	assert_not_null(session);
	if (!session) {
		test_gf_dm_quic_server_del(server);
		return;
	}
	session->headers = gf_list_new();
	session->server_mode = GF_TRUE;
	session->status = GF_NETIO_CONNECTED;
	assert_not_null(session->headers);
	if (!session->headers) {
		gf_free(session);
		test_gf_dm_quic_server_del(server);
		return;
	}
	e = pico_attach(conn, session, server->engine.mx, GF_TRUE);
	assert_equal(e, GF_OK, "%d");
	if (e) {
		gf_dm_sess_del(session);
		test_gf_dm_quic_server_del(server);
		return;
	}
	GF_SAFEALLOC(sub_session, GF_DownloadSession);
	assert_not_null(sub_session);
	if (!sub_session) {
		gf_dm_sess_del(session);
		test_gf_dm_quic_server_del(server);
		return;
	}
	sub_session->headers = gf_list_new();
	if (!sub_session->headers) {
		gf_free(sub_session);
		gf_dm_sess_del(session);
		test_gf_dm_quic_server_del(server);
		return;
	}
	sub_session->server_mode = GF_TRUE;
	sub_session->status = GF_NETIO_CONNECTED;
	sub_session->hmux_sess = session->hmux_sess;
	sub_session->mx = session->mx;
	gf_mx_ref(sub_session->mx);
	e = gf_list_add(sub_session->hmux_sess->sessions, sub_session);
	if (!e) e = pico_setup_session(sub_session, GF_FALSE);
	assert_equal(e, GF_OK, "%d");
	if (e) {
		gf_dm_sess_del(sub_session);
		gf_dm_sess_del(session);
		test_gf_dm_quic_server_del(server);
		return;
	}
	mx = server->engine.mx;
	gf_mx_p(mx);
	thread_err = pthread_create(&thread, NULL, test_pico_delete_session, session);
	assert_equal(thread_err, 0, "%d");
	if (thread_err) {
		gf_mx_v(mx);
		gf_dm_sess_del(session);
		test_gf_dm_quic_server_del(server);
		return;
	}
	test_gf_dm_quic_server_del(server);
	gf_mx_v(mx);
	thread_err = pthread_join(thread, NULL);
	assert_equal(thread_err, 0, "%d");
	gf_mx_p(mx);
	gf_mx_v(mx);
	gf_dm_sess_del(sub_session);
#endif
}

unittest(pico_nested_detach_mutex_lifetime)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	GF_HMUX_Session *hmux;
	GF_DownloadSession session = {0};
	GF_Mutex *mx = gf_mx_new("pico-test-nested");

	assert_not_null(mx);
	if (!mx) return;
	GF_SAFEALLOC(hmux, GF_HMUX_Session);
	assert_not_null(hmux);
	if (!hmux) {
		gf_mx_del(mx);
		return;
	}
	hmux->sessions = gf_list_new();
	assert_not_null(hmux->sessions);
	if (!hmux->sessions) {
		gf_free(hmux);
		gf_mx_del(mx);
		return;
	}
	hmux->mx = mx;
	session.mx = mx;
	session.hmux_sess = hmux;
	assert_equal(gf_list_add(hmux->sessions, &session), GF_OK, "%d");
	gf_mx_p(mx);
	gf_mx_p(mx);
	hmux_detach_session(hmux, &session);
	assert_true(session.mx == mx);
	assert_equal(gf_mx_get_num_locks(mx), 2, "%d");
	gf_mx_v(session.mx);
	assert_equal(gf_mx_get_num_locks(mx), 1, "%d");
	gf_mx_v(session.mx);
	assert_equal(gf_mx_get_num_locks(mx), -1, "%d");
	gf_mx_del(mx);
#endif
}

unittest(pico_headers_roundtrip)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoHeader fields[] = {
		{ ":method", "GET" }, { ":path", "/resource" }, { "X-Test", "Some Value" },
		{ "Set-Cookie", "a=1" }, { "set-cookie", "b=2" }
	};
	PicoStreamAck ack = {0};
	char connection;
	u64 frame_size = 0;
	size_t prefix;
	u32 index = 0;
	u8 *decoded;

	memset(&header_test, 0, sizeof(header_test));
	assert_equal(pico_h3_send_headers((picoquic_cnx_t *) &connection, 8, &ack,
		fields, GF_ARRAY_LENGTH(fields), GF_TRUE, &ack), GF_OK, "%d");
	assert_equal(header_test.stream_id, (u64) 8, LLU);
	assert_true(header_test.stream_ctx == &ack);
	assert_equal(header_test.fin, 1, "%d");
	if (header_test.length < 2) return;
	assert_equal(header_test.frame[0], h3zero_frame_header, "%u");
	prefix = h3zero_varint_decode(header_test.frame + 1, header_test.length - 1, &frame_size);
	assert_true(prefix > 0);
	if (!prefix) return;
	assert_equal(frame_size, (u64) (header_test.length - 1 - prefix), LLU);
	assert_equal(ack.queued, (u64) header_test.length, LLU);
	decoded = h3zero_parse_qpack_header_fields(header_test.frame + 1 + prefix,
		header_test.frame + header_test.length, test_pico_header_field, &index, 0);
	assert_true(decoded == header_test.frame + header_test.length);
	assert_equal(index, (u32) GF_ARRAY_LENGTH(fields), "%u");
#endif
}

unittest(pico_headers_queue_failure)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoHeader field = { "x-test", "value" };
	PicoStreamAck ack = {0};
	char connection;

	memset(&header_test, 0, sizeof(header_test));
	header_test.result = -1;
	assert_equal(pico_h3_send_headers((picoquic_cnx_t *) &connection, 0, &ack, &field, 1, GF_FALSE, &ack),
		GF_IP_NETWORK_FAILURE, "%d");
	assert_equal(ack.queued, (u64) 0, LLU);
	field.name = "bad header";
	assert_equal(pico_h3_send_headers((picoquic_cnx_t *) &connection, 0, &ack, &field, 1, GF_FALSE, &ack),
		GF_BAD_PARAM, "%d");
	field.name = "good";
	field.value = "bad\r\nvalue";
	assert_equal(pico_h3_send_headers((picoquic_cnx_t *) &connection, 0, &ack, &field, 1, GF_FALSE, &ack),
		GF_BAD_PARAM, "%d");
	assert_equal(ack.queued, (u64) 0, LLU);
#endif
}

unittest(pico_packet_retry)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoPacketPump pump = {0};
	char quic, socket;

	memset(&pump_test, 0, sizeof(pump_test));
	pump.quic = (picoquic_quic_t *) &quic;
	pump.socket = (GF_Socket *) &socket;
	pump_test.send_result = GF_IP_NETWORK_EMPTY;
	assert_equal(pico_pump_write(&pump), GF_IP_NETWORK_EMPTY, "%d");
	assert_equal(pump_test.prepared, 1, "%u");
	assert_equal(pump_test.last_packet, 1, "%u");
	assert_equal(pump.packet_len, (size_t) 2, "%zu");

	pump_test.send_result = GF_OK;
	pump_test.written = 2;
	assert_equal(pico_pump_write(&pump), GF_OK, "%d");
	assert_equal(pump_test.prepared, 3, "%u");
	assert_equal(pump_test.sends, 3, "%u");
	assert_equal(pump_test.addressed_sends, 3, "%u");
	assert_equal(pump_test.last_packet, 2, "%u");
	assert_equal(pump.packet_len, (size_t) 0, "%zu");
#endif
}

unittest(pico_packet_send_errors)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoPacketPump pump = {0};
	char quic, socket;

	memset(&pump_test, 0, sizeof(pump_test));
	pump.quic = (picoquic_quic_t *) &quic;
	pump.socket = (GF_Socket *) &socket;
	pump.connected = GF_TRUE;
	pump_test.send_result = GF_IP_CONNECTION_CLOSED;
	assert_equal(pico_pump_write(&pump), GF_IP_CONNECTION_CLOSED, "%d");
	assert_equal(pump_test.prepared, 1, "%u");
	assert_equal(pump.packet_len, (size_t) 2, "%zu");
	assert_equal(pump_test.addressed_sends, 0, "%u");

	pump_test.send_result = GF_IP_NETWORK_EMPTY;
	pump_test.written = 1;
	assert_equal(pico_pump_write(&pump), GF_IP_NETWORK_FAILURE, "%d");
	assert_equal(pump_test.prepared, 1, "%u");
	assert_equal(pump.packet_len, (size_t) 2, "%zu");
#endif
}

unittest(pico_packet_prepare_error)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoPacketPump pump = {0};
	char quic, socket;

	memset(&pump_test, 0, sizeof(pump_test));
	pump.quic = (picoquic_quic_t *) &quic;
	pump.socket = (GF_Socket *) &socket;
	pump_test.prepare_result = -1;
	assert_equal(pico_pump_write(&pump), GF_IP_NETWORK_FAILURE, "%d");
	assert_equal(pump.packet_len, (size_t) 0, "%zu");
	assert_equal(pump_test.sends, 0, "%u");
#endif
}

unittest(pico_pull_chunked_body)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoConnection conn;
	PicoEngine engine = {0};
	GF_HMUX_Session hmux = {0};
	PicoSessionStream stream;
	GF_DownloadSession sess;
	u8 body[100];
	const u8 *payload;
	u64 payload_len;
	u32 i;

	for (i = 0; i < sizeof(body); i++) body[i] = (u8) i;
	test_pico_pull_init(&conn, &stream, &sess, GF_FALSE);
	engine.in_pico = GF_TRUE;
	conn.engine = &engine;
	hmux.hmux_udta = &conn;
	sess.hmux_sess = &hmux;

	/* Staging copies the caller buffer and arms the stream exactly once. */
	sess.hmux_send_data = body;
	sess.hmux_send_data_len = sizeof(body);
	assert_equal(pico_send_pending(&sess), GF_OK, "%d");
	assert_true(sess.hmux_send_data == NULL);
	assert_equal(sess.local_buf_len, 100u, "%u");
	assert_equal(pull_test.marks, 1u, "%u");
	assert_equal(pull_test.marked_stream, (u64) 4, LLU);
	assert_true(pull_test.marked_ctx == &stream);
	memset(body, 0xFF, sizeof(body));

	/* First poll: 40 bytes of space carry a 2-byte DATA prefix and 38 body bytes. */
	assert_equal(test_pico_poll(&stream, 40, &payload, &payload_len), GF_OK, "%d");
	assert_equal(pull_test.length, (size_t) 40, "%zu");
	assert_equal(payload_len, (u64) 38, LLU);
	assert_equal(payload[0], 0, "%u");
	assert_equal(payload[37], 37, "%u");
	assert_equal(pull_test.fin, 0, "%d");
	assert_equal(pull_test.still_active, 1, "%d");
	assert_equal(stream.sent, 38u, "%u");
	assert_equal(sess.hmux_data_paused, 0u, "%u");

	/* Second poll drains the rest, goes idle and resets the staging buffer. */
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_OK, "%d");
	assert_equal(payload_len, (u64) 62, LLU);
	assert_equal(payload[0], 38, "%u");
	assert_equal(payload[61], 99, "%u");
	assert_equal(pull_test.fin, 0, "%d");
	assert_equal(pull_test.still_active, 0, "%d");
	assert_equal(sess.hmux_data_paused, 1u, "%u");
	assert_equal(sess.local_buf_len, 0u, "%u");
	assert_equal(stream.sent, 0u, "%u");
	assert_equal(stream.h3.ack.queued, (u64) 104, LLU);

	/* Idle poll provides nothing. */
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_OK, "%d");
	assert_equal(pull_test.length, (size_t) 0, "%zu");
	assert_equal(pull_test.provide_calls, 2, "%d");

	/* EOS without data: resume re-arms, the poll sends a bare FIN. */
	sess.hmux_is_eos = 1;
	assert_equal(pico_resume(&sess), GF_OK, "%d");
	assert_equal(pull_test.marks, 2u, "%u");
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_OK, "%d");
	assert_equal(pull_test.length, (size_t) 0, "%zu");
	assert_equal(pull_test.fin, 1, "%d");
	assert_equal(pull_test.provide_calls, 3, "%d");
	assert_true(stream.fin_sent);
	assert_equal(sess.hmux_data_done, 1u, "%u");
	assert_equal(sess.hmux_is_eos, 0u, "%u");
	assert_equal(stream.h3.ack.queued, (u64) 104, LLU);

	/* Nothing re-arms a finished stream. */
	assert_equal(pico_resume(&sess), GF_OK, "%d");
	assert_equal(pull_test.marks, 2u, "%u");
	test_pico_pull_clear(&sess);
#endif
}

unittest(pico_pull_space_limits)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoConnection conn;
	PicoSessionStream stream;
	GF_DownloadSession sess;
	u8 body[100];
	const u8 *payload;
	u64 payload_len;

	memset(body, 0x42, sizeof(body));
	test_pico_pull_init(&conn, &stream, &sess, GF_FALSE);
	conn.streams = gf_list_new();
	assert_not_null(conn.streams);
	if (!conn.streams) return;
	gf_list_add(conn.streams, &stream);
	assert_equal(pico_stage_data(&sess, &stream, body, sizeof(body)), GF_OK, "%d");

	/* Clamping 100 bytes into 66 shrinks the length varint: 2-byte prefix + 63 bytes. */
	assert_equal(test_pico_poll(&stream, 66, &payload, &payload_len), GF_OK, "%d");
	assert_equal(pull_test.length, (size_t) 65, "%zu");
	assert_equal(payload_len, (u64) 63, LLU);

	/* Three bytes of credit still carry one body byte. */
	assert_equal(test_pico_poll(&stream, 3, &payload, &payload_len), GF_OK, "%d");
	assert_equal(pull_test.length, (size_t) 3, "%zu");
	assert_equal(payload_len, (u64) 1, LLU);
	assert_equal(stream.sent, 64u, "%u");

	/* Two bytes cannot: the stream goes idle and waits for a re-arm. */
	assert_equal(test_pico_poll(&stream, 2, &payload, &payload_len), GF_OK, "%d");
	assert_equal(pull_test.length, (size_t) 0, "%zu");
	assert_equal(pull_test.provide_calls, 2, "%d");
	assert_true(stream.send_blocked);
	assert_equal(sess.hmux_data_paused, 0u, "%u");
	assert_equal(pull_test.marks, 0u, "%u");
	pico_connection_rearm(&conn);
	assert_equal(pull_test.marks, 1u, "%u");
	assert_true(!stream.send_blocked);

	/* Staging while partially consumed compacts instead of growing. */
	sess.local_buf_alloc = 100;
	assert_equal(pico_stage_data(&sess, &stream, body, 50), GF_OK, "%d");
	assert_equal(stream.sent, 0u, "%u");
	assert_equal(sess.local_buf_len, 86u, "%u");
	assert_equal(sess.local_buf_alloc, 100u, "%u");
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_OK, "%d");
	assert_equal(payload_len, (u64) 86, LLU);
	assert_equal(sess.local_buf_len, 0u, "%u");

	/* A failed buffer hand-off is a stream error. */
	assert_equal(pico_stage_data(&sess, &stream, body, 5), GF_OK, "%d");
	pull_test.fail = 1;
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_IP_NETWORK_FAILURE, "%d");
	assert_equal(sess.status, (u32) GF_NETIO_STATE_ERROR, "%u");
	assert_equal(sess.last_error, GF_IP_NETWORK_FAILURE, "%d");
	gf_list_del(conn.streams);
	test_pico_pull_clear(&sess);
#endif
}

unittest(pico_pull_eos_with_data_and_flush)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoConnection conn;
	PicoEngine engine = {0};
	GF_HMUX_Session hmux = {0};
	PicoSessionStream stream;
	GF_DownloadSession sess;
	const u8 body[] = "0123456789";
	const u8 *payload;
	u64 payload_len;

	test_pico_pull_init(&conn, &stream, &sess, GF_FALSE);
	engine.in_pico = GF_TRUE;
	conn.engine = &engine;
	hmux.hmux_udta = &conn;
	sess.hmux_sess = &hmux;
	assert_equal(pico_ack_commit_framing(&stream.h3.ack, 20), GF_OK, "%d");

	sess.hmux_send_data = (u8 *) body;
	sess.hmux_send_data_len = 10;
	sess.hmux_is_eos = 1;
	sess.put_state = 1;
	sess.status = GF_NETIO_DATA_EXCHANGE;
	assert_equal(pico_send_pending(&sess), GF_OK, "%d");
	assert_equal(pico_async_flush(&sess, GF_FALSE), GF_IP_NETWORK_EMPTY, "%d");
	assert_equal(pico_async_flush(&sess, GF_TRUE), GF_IP_NETWORK_EMPTY, "%d");

	/* Data and FIN travel in the same frame; the upload then waits for its reply. */
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_OK, "%d");
	assert_equal(payload_len, (u64) 10, LLU);
	assert_equal_mem(payload, body, 10);
	assert_equal(pull_test.fin, 1, "%d");
	assert_equal(pull_test.still_active, 0, "%d");
	assert_true(stream.fin_sent);
	assert_equal(sess.hmux_data_done, 1u, "%u");
	assert_equal(sess.put_state, 2u, "%u");
	assert_equal(sess.status, (u32) GF_NETIO_WAIT_FOR_REPLY, "%u");
	assert_equal(stream.h3.ack.queued, (u64) 32, LLU);
	sess.put_state = 0;

	/* Regular flush is done; close waits for the acknowledgement. */
	assert_equal(pico_async_flush(&sess, GF_FALSE), GF_OK, "%d");
	assert_equal(pico_async_flush(&sess, GF_TRUE), GF_IP_NETWORK_EMPTY, "%d");
	pico_ack_callback(conn.cnx, 4, 20, &conn, &stream);
	assert_equal(pico_async_flush(&sess, GF_TRUE), GF_IP_NETWORK_EMPTY, "%d");
	pico_ack_callback(conn.cnx, 4, 32, &conn, &stream);
	assert_equal(stream.h3.ack.acknowledged, (u64) 32, LLU);
	assert_equal(pico_async_flush(&sess, GF_TRUE), GF_OK, "%d");

	/* Regressing or overshooting acknowledgements fail the connection. */
	pico_ack_callback(conn.cnx, 4, 31, &conn, &stream);
	assert_true(conn.closed);
	assert_equal(sess.status, (u32) GF_NETIO_STATE_ERROR, "%u");
	conn.closed = GF_FALSE;
	sess.status = GF_NETIO_CONNECTED;
	pico_ack_callback(conn.cnx, 4, 33, &conn, &stream);
	assert_true(conn.closed);
	test_pico_pull_clear(&sess);
#endif
}

unittest(pico_pull_server_reply_order)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoConnection conn;
	PicoEngine engine = {0};
	GF_HMUX_Session hmux = {0};
	PicoSessionStream stream;
	GF_DownloadSession sess;
	const u8 body[] = "chunk";
	const u8 *payload;
	u64 payload_len;

	test_pico_pull_init(&conn, &stream, &sess, GF_TRUE);
	stream.response_started = GF_FALSE;
	engine.in_pico = GF_TRUE;
	conn.engine = &engine;
	hmux.hmux_udta = &conn;
	sess.hmux_sess = &hmux;
	sess.headers = gf_list_new();
	assert_not_null(sess.headers);
	if (!sess.headers) return;
	memset(&header_test, 0, sizeof(header_test));

	/* Body staged before the status line is held back. */
	sess.hmux_send_data = (u8 *) body;
	sess.hmux_send_data_len = 5;
	assert_equal(pico_send_pending(&sess), GF_OK, "%d");
	assert_equal(pull_test.marks, 0u, "%u");
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_OK, "%d");
	assert_equal(pull_test.provide_calls, 0, "%d");

	/* Headers are queued first, then the stream is armed for the staged body. */
	assert_equal(pico_send_reply_locked(&sess, 200, NULL, 0, GF_FALSE), GF_OK, "%d");
	assert_equal(header_test.frame[0], h3zero_frame_header, "%u");
	assert_equal(header_test.fin, 0, "%d");
	assert_true(stream.response_started);
	assert_equal(pull_test.marks, 1u, "%u");
	assert_equal(stream.h3.ack.queued, (u64) header_test.length, LLU);
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_OK, "%d");
	assert_equal(payload_len, (u64) 5, LLU);
	assert_equal_mem(payload, body, 5);
	assert_equal(pull_test.fin, 0, "%d");
	assert_true(!stream.fin_sent);

	/* Inline reply bodies finish the stream. */
	stream.response_started = GF_FALSE;
	assert_equal(pico_send_reply_locked(&sess, 404, "oops", 4, GF_FALSE), GF_OK, "%d");
	assert_equal(header_test.fin, 0, "%d");
	assert_equal(pull_test.marks, 2u, "%u");
	assert_equal(test_pico_poll(&stream, 1200, &payload, &payload_len), GF_OK, "%d");
	assert_equal(payload_len, (u64) 4, LLU);
	assert_equal_mem(payload, "oops", 4);
	assert_equal(pull_test.fin, 1, "%d");
	assert_true(stream.fin_sent);
	assert_equal(sess.hmux_data_done, 1u, "%u");

	/* Headers-only replies never arm the stream. */
	stream.fin_sent = GF_FALSE;
	assert_equal(pico_send_reply_locked(&sess, 304, NULL, 0, GF_FALSE), GF_OK, "%d");
	assert_equal(header_test.fin, 1, "%d");
	assert_true(stream.fin_sent);
	assert_equal(pull_test.marks, 2u, "%u");
	gf_list_del(sess.headers);
	test_pico_pull_clear(&sess);
#endif
}

unittest(pico_pull_arm_failure)
{
#if defined(GPAC_HAS_PICOQUIC) && !defined(GPAC_DISABLE_NETWORK)
	PicoConnection conn;
	PicoEngine engine = {0};
	GF_HMUX_Session hmux = {0};
	PicoSessionStream stream;
	GF_DownloadSession sess;
	const u8 body[] = "data";

	test_pico_pull_init(&conn, &stream, &sess, GF_FALSE);
	engine.in_pico = GF_TRUE;
	conn.engine = &engine;
	hmux.hmux_udta = &conn;
	sess.hmux_sess = &hmux;
	pull_test.mark_result = -1;
	sess.hmux_send_data = (u8 *) body;
	sess.hmux_send_data_len = 4;
	assert_equal(pico_send_pending(&sess), GF_IP_CONNECTION_CLOSED, "%d");
	assert_true(sess.hmux_send_data == NULL);
	assert_equal(sess.local_buf_len, 4u, "%u");
	test_pico_pull_clear(&sess);
#endif
}
