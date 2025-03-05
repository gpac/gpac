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


#ifdef GPAC_HAS_NGTCP2
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_quictls.h>
#endif

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
}


/* NPN TLS extension client callback. We check that server advertised
   the HTTP/2 protocol the nghttp2 library supports. If not, exit
   the program. */
static int ssl_select_next_proto_cb(SSL *ssl , unsigned char **out,
                                unsigned char *outlen, const unsigned char *in,
                                unsigned int inlen, void *arg)
{
#ifdef GPAC_HAS_HTTP2
	if (nghttp2_select_next_protocol(out, outlen, in, inlen) <= 0) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTP/2] Server did not advertise " NGHTTP2_PROTO_VERSION_ID));
	}
#endif
	return SSL_TLSEXT_ERR_OK;
}


static Bool _ssl_is_initialized = GF_FALSE;

/*!
 * initialize the SSL library once for all download managers
\return GF_FALSE if everyhing is OK, GF_TRUE otherwise
 */
Bool gf_ssl_init_lib()
{
	if (_ssl_is_initialized)
		return GF_FALSE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPS] Initializing SSL library...\n"));
	init_prng();
	if (RAND_status() != 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPS] Error while initializing Random Number generator, failed to init SSL !\n"));
		return GF_TRUE;
	}

	/* per https://www.openssl.org/docs/man1.1.0/ssl/OPENSSL_init_ssl.html
	** As of version 1.1.0 OpenSSL will automatically allocate all resources that it needs so no explicit initialisation is required.
	** Similarly it will also automatically deinitialise as required.
	*/
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_library_init();
	SSL_load_error_strings();
	SSLeay_add_all_algorithms();
	SSLeay_add_ssl_algorithms();
#endif

	_ssl_is_initialized = GF_TRUE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[HTTPS] Initialization of SSL library complete.\n"));
	return GF_FALSE;
}

#ifndef GPAC_DISABLE_LOG
static const char *tls_rt_get_name(int type)
{
	switch(type) {
#ifdef SSL3_RT_HEADER
	case SSL3_RT_HEADER: return "TLS header";
#endif
	case SSL3_RT_CHANGE_CIPHER_SPEC: return "TLS change cipher";
	case SSL3_RT_ALERT: return "TLS alert";
	case SSL3_RT_HANDSHAKE: return "TLS handshake";
	case SSL3_RT_APPLICATION_DATA: return "TLS app data";
	default: return "TLS Unknown";
	}
}

static const char *ssl_msg_get_name(int version, int msg)
{
#ifdef SSL_CTRL_SET_MSG_CALLBACK

#ifdef SSL2_VERSION_MAJOR
	if (version == SSL2_VERSION_MAJOR) {
		switch(msg) {
		case SSL2_MT_ERROR: return "Error";
		case SSL2_MT_CLIENT_HELLO: return "Client hello";
		case SSL2_MT_CLIENT_MASTER_KEY: return "Client key";
		case SSL2_MT_CLIENT_FINISHED: return "Client finished";
		case SSL2_MT_SERVER_HELLO: return "Server hello";
		case SSL2_MT_SERVER_VERIFY: return "Server verify";
		case SSL2_MT_SERVER_FINISHED: return "Server finished";
		case SSL2_MT_REQUEST_CERTIFICATE: return "Request CERT";
		case SSL2_MT_CLIENT_CERTIFICATE: return "Client CERT";
		}
	} else
#endif
	if (version == SSL3_VERSION_MAJOR) {
		switch(msg) {
		case SSL3_MT_HELLO_REQUEST: return "Hello request";
		case SSL3_MT_CLIENT_HELLO: return "Client hello";
		case SSL3_MT_SERVER_HELLO: return "Server hello";
	#ifdef SSL3_MT_NEWSESSION_TICKET
		case SSL3_MT_NEWSESSION_TICKET: return "Newsession Ticket";
	#endif
		case SSL3_MT_CERTIFICATE: return "Certificate";
		case SSL3_MT_SERVER_KEY_EXCHANGE: return "Server key exchange";
		case SSL3_MT_CLIENT_KEY_EXCHANGE: return "Client key exchange";
		case SSL3_MT_CERTIFICATE_REQUEST: return "Request CERT";
		case SSL3_MT_SERVER_DONE: return "Server finished";
		case SSL3_MT_CERTIFICATE_VERIFY: return "CERT verify";
		case SSL3_MT_FINISHED: return "Finished";
#ifdef SSL3_MT_CERTIFICATE_STATUS
		case SSL3_MT_CERTIFICATE_STATUS: return "Certificate Status";
#endif
#ifdef SSL3_MT_ENCRYPTED_EXTENSIONS
		case SSL3_MT_ENCRYPTED_EXTENSIONS: return "Encrypted Extensions";
#endif
#ifdef SSL3_MT_SUPPLEMENTAL_DATA
		case SSL3_MT_SUPPLEMENTAL_DATA: return "Supplemental data";
#endif
#ifdef SSL3_MT_END_OF_EARLY_DATA
		case SSL3_MT_END_OF_EARLY_DATA: return "End of early data";
#endif
#ifdef SSL3_MT_KEY_UPDATE
		case SSL3_MT_KEY_UPDATE: return "Key update";
#endif
#ifdef SSL3_MT_NEXT_PROTO
		case SSL3_MT_NEXT_PROTO: return "Next protocol";
#endif
#ifdef SSL3_MT_MESSAGE_HASH
		case SSL3_MT_MESSAGE_HASH: return "Message hash";
#endif
		}
	}
#endif
	return "Unknown";
}

static void ssl_on_log(int write_p, int version, int content_type, const void *buf, size_t len, SSL *ssl, void *udat)
{
	const char *msg_name, *tls_rt_name;
	int msg_type;
	char szUnknown[32];
	const char *szVersion = NULL;

	if (!version) return;
#ifdef SSL3_RT_INNER_CONTENT_TYPE
	if (content_type == SSL3_RT_INNER_CONTENT_TYPE) return;
#endif

	switch (version) {
#ifdef SSL2_VERSION
	case SSL2_VERSION: szVersion = "SSLv2"; break;
#endif
#ifdef SSL3_VERSION
	case SSL3_VERSION: szVersion = "SSLv3"; break;
#endif
	case TLS1_VERSION: szVersion = "TLSv1.0"; break;
#ifdef TLS1_1_VERSION
	case TLS1_1_VERSION: szVersion = "TLSv1.1"; break;
#endif
#ifdef TLS1_2_VERSION
	case TLS1_2_VERSION: szVersion = "TLSv1.2"; break;
#endif
#ifdef TLS1_3_VERSION
	case TLS1_3_VERSION: szVersion = "TLSv1.3"; break;
#endif
	case 0: break;
	default:
		snprintf(szUnknown, 31, "(%x)", version);
		szVersion = szUnknown;
		break;
	}
	version >>= 8;
	if ((version == SSL3_VERSION_MAJOR) && content_type)
		tls_rt_name = tls_rt_get_name(content_type);
	else
		tls_rt_name = "";

	if (content_type == SSL3_RT_CHANGE_CIPHER_SPEC) {
		msg_type = *(char *)buf;
		msg_name = "Change cipher spec";
	} else if (content_type == SSL3_RT_ALERT) {
		msg_type = (((char *)buf)[0] << 8) + ((char *)buf)[1];
		msg_name = SSL_alert_desc_string_long(msg_type);
	} else {
		msg_type = *(char *)buf;
		msg_name = ssl_msg_get_name(version, msg_type);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[%s] %s %s (%d)\n", szVersion, tls_rt_name, msg_name, msg_type));
}
#endif //GPAC_DISABLE_LOG

static char ALPN_PROTOS[20];

void *gf_dm_ssl_init(GF_DownloadManager *dm, Bool no_quic)
{
#if OPENSSL_VERSION_NUMBER > 0x00909000
	const
#endif
	SSL_METHOD *meth;

	if (!dm) return NULL;

	gf_mx_p(dm->cache_mx);
	/* The SSL has already been initialized. */
	if (dm->ssl_ctx) {
		gf_mx_v(dm->cache_mx);
		return dm->ssl_ctx;
	}
	/* Init the PRNG.  If that fails, bail out.  */
	if (gf_ssl_init_lib()) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[HTTPS] Failed to properly initialize SSL library\n"));
		goto error;
	}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	meth = SSLv23_client_method();
#else
	meth = TLS_client_method();
#endif

	dm->ssl_ctx = SSL_CTX_new(meth);
	if (!dm->ssl_ctx) goto error;
	SSL_CTX_set_options(dm->ssl_ctx, SSL_OP_ALL);
	SSL_CTX_set_default_verify_paths(dm->ssl_ctx);
	SSL_CTX_load_verify_locations (dm->ssl_ctx, NULL, NULL);
	/* SSL_VERIFY_NONE instructs OpenSSL not to abort SSL_connect if the
	 certificate is invalid.  We verify the certificate separately in
	 ssl_check_certificate, which provides much better diagnostics
	 than examining the error stack after a failed SSL_connect.  */
	SSL_CTX_set_verify(dm->ssl_ctx, SSL_VERIFY_NONE, NULL);

	if (!gf_opts_get_bool("core", "broken-cert")) {

		const char* ca_bundle = gf_opts_get_key("core", "ca-bundle");

		if (ca_bundle && gf_file_exists(ca_bundle)) {
			SSL_CTX_load_verify_locations(dm->ssl_ctx, ca_bundle, NULL);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[SSL] Using CA bundle at %s\n", ca_bundle));
		}
		else {
			const char* ossl_bundle = X509_get_default_cert_file_env();
			ossl_bundle = getenv(ossl_bundle);
			if (!ossl_bundle)
				ossl_bundle = X509_get_default_cert_file();

			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[SSL] OpenSSL CA bundle location: %s\n", ossl_bundle));

			if (!ossl_bundle || !gf_file_exists(ossl_bundle)) {

				const char* ca_bundle_default = gf_opts_get_key("core", "ca-bundle-default");

				if (ca_bundle_default) {
					SSL_CTX_load_verify_locations(dm->ssl_ctx, ca_bundle_default, NULL);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[SSL] Using default CA bundle at %s\n", ca_bundle_default));
				}
			}
		}
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_NETWORK, GF_LOG_DEBUG) ) {
		SSL_CTX_set_msg_callback(dm->ssl_ctx, ssl_on_log);
	}
#endif


	ALPN_PROTOS[0] = 0;
#ifdef GPAC_HAS_HTTP2
	if (!dm->disable_http2) {
		strcat(ALPN_PROTOS, "\x02h2");
	}
#endif

	//configure H3
	if (dm->h3_mode && !no_quic) {
#ifdef GPAC_HAS_NGTCP2
		if (ngtcp2_crypto_quictls_configure_client_context(dm->ssl_ctx) != 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unable to initialize SSL context for QUIC, disabling HTTP3\n"));
			dm->h3_mode = H3_MODE_NO;
		} else {
			strcat(ALPN_PROTOS, "\x02h3");
		}
#endif
	}

	if (ALPN_PROTOS[0]) {
		SSL_CTX_set_next_proto_select_cb(dm->ssl_ctx, ssl_select_next_proto_cb, NULL);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
		SSL_CTX_set_alpn_protos(dm->ssl_ctx, ALPN_PROTOS, (u32) strlen(ALPN_PROTOS));
#endif
	}

	/* Since fd_write unconditionally assumes partial writes (and handles them correctly),
	allow them in OpenSSL.  */
	SSL_CTX_set_mode(dm->ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
	gf_mx_v(dm->cache_mx);
	return dm->ssl_ctx;
error:
	if (dm->ssl_ctx) SSL_CTX_free(dm->ssl_ctx);
	dm->ssl_ctx = NULL;
	gf_mx_v(dm->cache_mx);
	return NULL;
}

#ifdef GPAC_HAS_HTTP2
void h2_initialize_session(GF_DownloadSession *sess);
#endif

#if defined(GPAC_HAS_HTTP2) || defined(GPAC_HAS_NGTCP2)

static unsigned char next_proto_list[256];
static size_t next_proto_list_len;

#ifndef OPENSSL_NO_NEXTPROTONEG
static int next_proto_cb(SSL *ssl, const unsigned char **data, unsigned int *len, void *arg)
{
	if (data && len) {
		*data = next_proto_list;
		*len = (unsigned int)next_proto_list_len;
	}
	return SSL_TLSEXT_ERR_OK;
}
#endif //OPENSSL_NO_NEXTPROTONEG

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg)
{

#ifdef GPAC_HAS_NGTCP2
	ngtcp2_crypto_conn_ref *cref = (ngtcp2_crypto_conn_ref *) SSL_get_app_data(ssl);
	if (cref) {
		const uint8_t *alpn;
		size_t alpnlen;
		// This should be the negotiated version, but we have not set the
		// negotiated version when this callback is called.
		uint32_t version = ngtcp2_conn_get_client_chosen_version( cref->get_conn(cref) );

		switch (version) {
		case NGTCP2_PROTO_VER_V1:
		case NGTCP2_PROTO_VER_V2:
			alpn = "\x2h3";
			alpnlen = 3;
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Unexpected quic protocol version %X\n", version ));
			return SSL_TLSEXT_ERR_ALERT_FATAL;
		}
		const unsigned char *p, *end;
		for (p = in, end = in + inlen; p + alpnlen <= end; p += *p + 1) {
			if (!strncmp(alpn, p, alpnlen)) {
				*out = p + 1;
				*outlen = *p;
				return SSL_TLSEXT_ERR_OK;
			}
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Client did not present ALPN %s\n", &alpn[1] ));
		return SSL_TLSEXT_ERR_ALERT_FATAL;
	}
#endif


#ifdef GPAC_HAS_NGHTTP2
	int rv = nghttp2_select_next_protocol((unsigned char **)out, outlen, in, inlen);
	if (rv == 1)
		return SSL_TLSEXT_ERR_OK
#endif
	return SSL_TLSEXT_ERR_NOACK;
}

#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

#endif //GPAC_HAS_HTTP2


#if !defined(LIBRESSL_VERSION_NUMBER) && defined(GPAC_HAS_NGTCP2)
int ngq_gen_ticket_cb(SSL *ssl, void *arg)
{
	ngtcp2_crypto_conn_ref *cref = (ngtcp2_crypto_conn_ref *) SSL_get_app_data(ssl);
	if (!cref) return 0;
	int ver = htonl(ngtcp2_conn_get_negotiated_version(cref->get_conn(cref) ) );
	if (!SSL_SESSION_set1_ticket_appdata(SSL_get0_session(ssl), &ver, sizeof(ver))) {
		return 0;
	}
	return 1;
}

SSL_TICKET_RETURN ngq_decrypt_ticket_cb(SSL *ssl, SSL_SESSION *session, const unsigned char *keyname, size_t keynamelen, SSL_TICKET_STATUS status, void *arg)
{
	switch (status) {
	case SSL_TICKET_EMPTY:
	case SSL_TICKET_NO_DECRYPT:
		return SSL_TICKET_RETURN_IGNORE_RENEW;
	}

	uint8_t *pver;
	uint32_t ver;
	size_t verlen;

	if (!SSL_SESSION_get0_ticket_appdata(session, (void **) &pver, &verlen) || (verlen != sizeof(ver))) {
		switch (status) {
		case SSL_TICKET_SUCCESS:
			return SSL_TICKET_RETURN_IGNORE;
		case SSL_TICKET_SUCCESS_RENEW:
		default:
			return SSL_TICKET_RETURN_IGNORE_RENEW;
		}
	}
	memcpy(&ver, pver, sizeof(ver));
	ngtcp2_crypto_conn_ref *cref = (ngtcp2_crypto_conn_ref *) SSL_get_app_data(ssl);
	if (!cref) return SSL_TICKET_RETURN_IGNORE_RENEW;

	if (ngtcp2_conn_get_client_chosen_version(cref->get_conn(cref) ) != ntohl(ver)) {
		switch (status) {
		case SSL_TICKET_SUCCESS:
			return SSL_TICKET_RETURN_IGNORE;
		case SSL_TICKET_SUCCESS_RENEW:
		default:
			return SSL_TICKET_RETURN_IGNORE_RENEW;
		}
	}
	switch (status) {
	case SSL_TICKET_SUCCESS:
		return SSL_TICKET_RETURN_USE;
	case SSL_TICKET_SUCCESS_RENEW:
	default:
	return SSL_TICKET_RETURN_USE_RENEW;
	}
}
#endif // !definedLIBRESSL_VERSION_NUMBER) && defined(GPAC_HAS_NGTCP2)


void *gf_ssl_server_context_new(const char *cert, const char *key, Bool for_quic)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

#ifdef GPAC_HAS_NGTCP2
	if (for_quic) {
		method = TLS_server_method();
	}
#else
	if (for_quic) return NULL;
#endif

    ctx = SSL_CTX_new(method);
    if (!ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unable to create SSL context\n"));
		ERR_print_errors_fp(stderr);
		return NULL;
    }

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_NETWORK, GF_LOG_DEBUG) ) {
		SSL_CTX_set_msg_callback(ctx, ssl_on_log);
	}
#endif

	//configure H2
#ifdef GPAC_HAS_HTTP2
	if (!for_quic && !gf_opts_get_bool("core", "no-h2")) {
		next_proto_list[0] = NGHTTP2_PROTO_VERSION_ID_LEN;
		memcpy(&next_proto_list[1], NGHTTP2_PROTO_VERSION_ID, NGHTTP2_PROTO_VERSION_ID_LEN);
		next_proto_list_len = 1 + NGHTTP2_PROTO_VERSION_ID_LEN;
	}
#endif


#ifdef GPAC_HAS_NGTCP2
	if (for_quic) {
		ngtcp2_crypto_quictls_configure_server_context(ctx);

		next_proto_list[next_proto_list_len] = 2;
		memcpy(&next_proto_list[next_proto_list_len+1], "h3", 2);
		next_proto_list_len = 1 + 2;

		SSL_CTX_set_max_early_data(ctx, UINT32_MAX);

		u64 ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                            SSL_OP_SINGLE_ECDH_USE |
                            SSL_OP_CIPHER_SERVER_PREFERENCE
#ifndef LIBRESSL_VERSION_NUMBER
                            | SSL_OP_NO_ANTI_REPLAY
#endif // !defined(LIBRESSL_VERSION_NUMBER)
		;
		SSL_CTX_set_options(ctx, ssl_opts);

		SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);

#ifndef LIBRESSL_VERSION_NUMBER
		SSL_CTX_set_session_ticket_cb(ctx, ngq_gen_ticket_cb, ngq_decrypt_ticket_cb, NULL);
#endif // !defined(LIBRESSL_VERSION_NUMBER)

		SSL_CTX_set_ciphersuites(ctx, "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256");
		SSL_CTX_set1_groups_list(ctx, "X25519:P-256:P-384:P-521");
	}
#endif

/*	if (!for_quic)
		SSL_CTX_set_quic_method(ctx, NULL);
*/
	if (next_proto_list_len) {
		SSL_CTX_set_next_protos_advertised_cb(ctx, next_proto_cb, NULL);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
		SSL_CTX_set_alpn_select_cb(ctx, alpn_select_proto_cb, NULL);
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L
	}


	SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_set_ecdh_auto(ctx, 1);

//	if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0) {
	if (SSL_CTX_use_certificate_chain_file(ctx, cert) != 1) {
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		return NULL;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0 ) {
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		return NULL;
	}
	if (SSL_CTX_check_private_key(ctx) != 1) {
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		return NULL;
	}

    return ctx;
}

void gf_ssl_server_context_del(void *ssl_ctx)
{
	SSL_CTX_free(ssl_ctx);
}
void *gf_ssl_new(void *ssl_ctx, GF_Socket *client_sock, GF_Err *e)
{
	SSL *ssl;
	ssl = SSL_new(ssl_ctx);
	if (!ssl) {
		*e = GF_IO_ERR;
		return NULL;
	}
	SSL_set_fd(ssl, gf_sk_get_handle(client_sock) );
	if (SSL_accept(ssl) <= 0) {
		if (gf_log_tool_level_on(GF_LOG_NETWORK, GF_LOG_DEBUG)) {
			//this one crashes /exits on windows, only use if debug level
			ERR_print_errors_fp(stderr);
		} else {
			ERR_print_errors_fp(stderr);
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[SSL] Accept failure\n"));
		}
		SSL_shutdown(ssl);
        SSL_free(ssl);
		*e = GF_AUTHENTICATION_FAILURE;
		return NULL;
	}
	*e = GF_OK;
	return ssl;
}
void gf_ssl_del(void *ssl)
{
	if (ssl) SSL_free(ssl);
}

Bool gf_ssl_check_cert(SSL *ssl, const char *server_name)
{
	long vresult = SSL_get_verify_result(ssl);
	Bool success = (vresult == X509_V_OK);

	if (!success) {
		int level = GF_LOG_ERROR;
		if (gf_opts_get_bool("core", "broken-cert")) {
			success = GF_TRUE;
			level = GF_LOG_WARNING;
		}
		GF_LOG(level, GF_LOG_HTTP, ("[SSL] Certificate verification failed for domain %s: %s. %s\n", server_name, ERR_error_string(vresult, NULL), (level == GF_LOG_ERROR ? "Use -broken-cert to bypass." : "")));
	}

	return success;
}

GF_Err gf_ssl_write(GF_DownloadSession *sess, const u8 *buffer, u32 size, u32 *written)
{
	u32 idx=0;
	s32 nb_tls_blocks = size/16000;
	*written = 0;
	while (nb_tls_blocks>=0) {
		u32 len, to_write = 16000;
		if (nb_tls_blocks==0)
			to_write = size - idx*16000;

		len = SSL_write(sess->ssl, buffer + idx*16000, to_write);
		nb_tls_blocks--;
		idx++;

		if (len != to_write) {
			if (sess->flags & GF_NETIO_SESSION_NO_BLOCK) {
				int err = SSL_get_error(sess->ssl, len);
				if ((err==SSL_ERROR_WANT_READ) || (err==SSL_ERROR_WANT_WRITE)) {
					return GF_IP_NETWORK_EMPTY;
				}
				if (err==SSL_ERROR_SSL) {
					char msg[1024];
					SSL_load_error_strings();
					ERR_error_string_n(ERR_get_error(), msg, 1023);
					msg[1023]=0;
					GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Cannot send, error %s\n", msg));
				}
			}
			return GF_IP_NETWORK_FAILURE;
		}
		*written += to_write;
	}
	return GF_OK;
}

GF_Err gf_ssl_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read)
{
	s32 size;
	GF_Err e;

	//receive on null buffer (select only, check if data available)
	e = gf_sk_receive(sess->sock, NULL, 0, NULL);
	//empty and no pending bytes in SSL, network empty
	if ((e==GF_IP_NETWORK_EMPTY) &&
#if 1
		!SSL_pending(sess->ssl)
#else
		//no support for SSL_has_pending in old libSSL and same result can be achieved with SSL_pending
		!SSL_has_pending(sess->ssl)
#endif
	) {
		return GF_NOT_READY;
	}
	size = SSL_read(sess->ssl, data, data_size);
	if (size < 0) {
		int err = SSL_get_error(sess->ssl, size);
		if (err==SSL_ERROR_SSL) {
			char msg[1024];
			SSL_load_error_strings();
			ERR_error_string_n(ERR_get_error(), msg, 1023);
			msg[1023]=0;
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Cannot read, error %s\n", msg));
			e = GF_IO_ERR;
		} else {
			e = gf_sk_probe(sess->sock);
		}
	} else if (!size)
		e = GF_IP_NETWORK_EMPTY;
	else {
		e = GF_OK;
		*out_read = size;
	}
	return e;
}

void gf_dm_sess_server_setup_ssl(GF_DownloadSession *sess)
{
	gf_assert(sess->ssl);
	if (sess->flags & GF_NETIO_SESSION_NO_BLOCK)
		SSL_set_mode(sess->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER|SSL_MODE_ENABLE_PARTIAL_WRITE);

#ifdef GPAC_HAS_HTTP2
	const unsigned char *alpn = NULL;
	unsigned int alpnlen = 0;
	SSL_get0_next_proto_negotiated(sess->ssl, &alpn, &alpnlen);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
	if (alpn == NULL) {
		SSL_get0_alpn_selected(sess->ssl, &alpn, &alpnlen);
	}
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L

	if (alpn && (alpnlen == 2) && !memcmp("h2", alpn, 2)) {
		h2_initialize_session(sess);
	}
#endif //GPAC_HAS_HTTP2

}

SSLConnectStatus gf_ssl_try_connect(GF_DownloadSession *sess, const char *proxy)
{
	u64 now = gf_sys_clock_high_res();
	if (sess->dm && !sess->dm->ssl_ctx)
		gf_dm_ssl_init(sess->dm, (sess->flags & GF_NETIO_SESSION_USE_QUIC) ? GF_FALSE : GF_TRUE);

	/*socket is connected, configure SSL layer*/
	if (!sess->dm || !sess->dm->ssl_ctx) return SSL_CONNECT_OK;

	int ret;
	Bool success;

	if (!sess->ssl) {
		sess->ssl = SSL_new(sess->dm->ssl_ctx);
		SSL_set_fd(sess->ssl, gf_sk_get_handle(sess->sock));
		SSL_ctrl(sess->ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, (void*) proxy);
		SSL_set_connect_state(sess->ssl);

		if (sess->flags & GF_NETIO_SESSION_NO_BLOCK)
			SSL_set_mode(sess->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER|SSL_MODE_ENABLE_PARTIAL_WRITE);

#ifdef GPAC_HAS_HTTP2
		if (sess->h2_upgrade_state==3) {
			SSL_set_alpn_protos(sess->ssl, NULL, 0);
			sess->h2_upgrade_state = 0;
		}
		//h1 disabled, don't use alpn
		else if (sess->h2_upgrade_state==4) {
			SSL_set_alpn_protos(sess->ssl, NULL, 0);
		}
#endif

#ifdef GPAC_HAS_NGTCP2
		//if not using quic, reset quic methods in ssl
		if (!(sess->flags & GF_NETIO_SESSION_USE_QUIC))
			SSL_set_quic_method(sess->ssl, NULL);
#endif
	}

	if (sess->flags & GF_NETIO_SESSION_USE_QUIC)
		return SSL_CONNECT_RETRY;

	sess->connect_pending = 0;
	ret = SSL_connect(sess->ssl);
	if (ret<=0) {
		ret = SSL_get_error(sess->ssl, ret);
		if (ret==SSL_ERROR_SSL) {

#if (OPENSSL_VERSION_NUMBER >= 0x10002000L) && defined(GPAC_HAS_HTTP2)
			//error and we tried with alpn for h2, retry without (kill/reconnect)
			if (!sess->h2_upgrade_state && !sess->dm->disable_http2) {
				SSL_free(sess->ssl);
				sess->ssl = NULL;
				dm_sess_sk_del(sess);
				sess->status = GF_NETIO_SETUP;
				sess->h2_upgrade_state = 3;
				return SSL_CONNECT_RETRY;
			}
#endif

			char msg[1024];
			SSL_load_error_strings();
			ERR_error_string_n(ERR_get_error(), msg, 1023);
			msg[1023]=0;
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Cannot connect, error %s\n", msg));
			if (!sess->dm->disable_http2) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("\tYou may want to retry with HTTP/2 support disabled (-no-h2)\n"));
			}
			SET_LAST_ERR(GF_SERVICE_ERROR)
		} else if ((ret==SSL_ERROR_WANT_READ) || (ret==SSL_ERROR_WANT_WRITE)) {
			sess->status = GF_NETIO_SETUP;
			sess->connect_pending = 2;
			return SSL_CONNECT_WAIT;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP, ("[SSL] Cannot connect, error %d\n", ret));
			SET_LAST_ERR(GF_REMOTE_SERVICE_ERROR)
		}
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[SSL] connected\n"));


#ifdef GPAC_HAS_HTTP2
		if (!sess->dm->disable_http2) {
			const u8 *alpn = NULL;
			u32 alpnlen = 0;
			SSL_get0_next_proto_negotiated(sess->ssl, &alpn, &alpnlen);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
			if (alpn == NULL) {
				SSL_get0_alpn_selected(sess->ssl, &alpn, &alpnlen);
			}
#endif
			if (alpn == NULL || alpnlen != 2 || memcmp("h2", alpn, 2) != 0) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_HTTP, ("[SSL] HTTP/2 is not negotiated\n"));
				//disable h2 for the session
				sess->h2_upgrade_state = 4;
			} else {
				h2_initialize_session(sess);
			}
		}
#endif
	}

	success = gf_ssl_check_cert(sess->ssl, sess->server_name);
	if (!success) {
		gf_dm_disconnect(sess, HTTP_RESET_CONN);
		sess->status = GF_NETIO_STATE_ERROR;
		SET_LAST_ERR(GF_AUTHENTICATION_FAILURE)
		gf_dm_sess_notify_state(sess, sess->status, sess->last_error);
	}

	sess->ssl_setup_time = (u32) (gf_sys_clock_high_res() - now);
	return SSL_CONNECT_OK;
}

#endif // GPAC_HAS_SSL

/**
 * \brief Saves the digest for authentication of password and username
\param dm The download manager
\param creds The credentials to fill
\return GF_OK if info has been filled, GF_BAD_PARAM if creds == NULL or dm == NULL, GF_AUTHENTICATION_FAILURE if user did not filled the info.
 */
static GF_Err gf_user_credentials_save_digest( GF_DownloadManager * dm, GF_UserCredentials * creds, const char * password, Bool store_info) {
	int size;
	char *pass_buf = NULL;
	char range_buf[1024];
	if (!dm || !creds || !password)
		return GF_BAD_PARAM;
	gf_dynstrcat(&pass_buf, creds->username, NULL);
	gf_dynstrcat(&pass_buf, password, ":");
	if (!pass_buf) return GF_OUT_OF_MEM;
	size = gf_base64_encode(pass_buf, (u32) strlen(pass_buf), range_buf, 1024);
	gf_free(pass_buf);
	range_buf[size] = 0;
	strcpy(creds->digest, range_buf);
	creds->valid = GF_TRUE;

#ifndef GPAC_DISABLE_CRYPTO
	if (store_info) {
		u32 plen = (u32) strlen(password);
		char *key = NULL;
		char szPATH[GF_MAX_PATH];
		const char *cred = gf_opts_get_key("core", "cred");
		if (!cred) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] No credential key defined, will not store password - use -cred=PATH_TO_KEY to specify it\n"));
			return GF_OK;
		}

		FILE *f = gf_fopen(cred, "r");
		bin128 k;
		if (!f) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] Failed to open credential key %s, will not store password\n", cred));
			return GF_OK;
		}
		u32 ksize = (u32) gf_fread(k, 16, f);
		gf_fclose(f);
		if (ksize!=16) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] Invalid credential key %s: size %d but expecting 16 bytes, will not store password\n", cred, ksize));
			return GF_OK;
		}

		GF_Crypt *gfc = gf_crypt_open(GF_AES_128, GF_CTR);
		gf_crypt_init(gfc, k, NULL);
		strcpy(szPATH, password);
		gf_crypt_encrypt(gfc, szPATH, plen);
		gf_crypt_close(gfc);
		szPATH[plen] = 0;
		gf_dynstrcat(&key, creds->username, NULL);
		gf_dynstrcat(&key, szPATH, ":");
		gf_opts_set_key("credentials", creds->site, key);
		gf_free(key);
		gf_opts_save();
	}
#endif
	return GF_OK;
}
/**
 * Find a User's credentials for a given site
 */
GF_UserCredentials* gf_user_credentials_find_for_site(GF_DownloadManager *dm, const char *server_name, const char *user_name)
{
	GF_UserCredentials * cred;
	u32 count, i;
	if (!dm || !dm->credentials || !server_name || !strlen(server_name))
		return NULL;
	count = gf_list_count( dm->credentials);
	for (i = 0 ; i < count; i++) {
		cred = (GF_UserCredentials*)gf_list_get(dm->credentials, i );
		if (!cred || strcmp(cred->site, server_name))
			continue;

		if (!user_name || !strcmp(user_name, cred->username))
			return cred;
	}

#ifndef GPAC_DISABLE_CRYPTO
	char *key = (char*) gf_opts_get_key("credentials", server_name);
	if (key) {
		bin128 k;
		const char *credk = gf_opts_get_key("core", "cred");
		if (credk) {
			FILE *f = gf_fopen(credk, "r");
			if (f) {
				gf_fread(k, 16, f);
				gf_fclose(f);
			} else {
				credk = NULL;
			}
		}
		char *sep = credk ? strrchr(key, ':') : NULL;
		if (sep) {
			char szP[1024];
			GF_SAFEALLOC(cred, GF_UserCredentials);
			if (!cred) return NULL;
			cred->dm = dm;
			sep[0] = 0;
			strcpy(cred->username, key);
			strcpy(szP, sep+1);
			sep[0] = ':';

			GF_Crypt *gfc = gf_crypt_open(GF_AES_128, GF_CTR);
			gf_crypt_init(gfc, k, NULL);
			gf_crypt_decrypt(gfc, szP, (u32) strlen(szP));
			gf_crypt_close(gfc);
			gf_user_credentials_save_digest(dm, cred, szP, GF_FALSE);
			return cred;
		}
	}
#endif
	return NULL;
}


static void on_user_pass(void *udta, const char *user, const char *pass, Bool store_info)
{
	GF_UserCredentials *creds = (GF_UserCredentials *)udta;
	if (!creds) return;
	u32 len = user ? (u32) strlen(user) : 0;
	if (len && (user != creds->username)) {
		if (len> 49) len = 49;
		strncpy(creds->username, user, 49);
		creds->username[len]=0;
	}
	if (user && pass) {
		GF_Err e = gf_user_credentials_save_digest((GF_DownloadManager *)creds->dm, creds, pass, store_info);
		if (e != GF_OK) {
			creds->valid = GF_FALSE;
		}
	} else {
		creds->valid = GF_FALSE;
	}
	creds->req_state = GF_CREDS_STATE_DONE;
}

/**
 * \brief Asks the user for credentials for given site
\param dm The download manager
\param creds The credentials to fill
\return GF_OK if info has been filled, GF_BAD_PARAM if creds == NULL or dm == NULL, GF_AUTHENTICATION_FAILURE if user did not filled the info.
 */
static GF_Err gf_user_credentials_ask_password( GF_DownloadManager * dm, GF_UserCredentials * creds, Bool secure)
{
	char szPASS[50];
	if (!dm || !creds)
		return GF_BAD_PARAM;
	memset(szPASS, 0, 50);
	if (!dm->get_user_password) return GF_AUTHENTICATION_FAILURE;
	creds->req_state = GF_CREDS_STATE_PENDING;
	if (!dm->get_user_password(dm->usr_cbk, secure, creds->site, creds->username, szPASS, on_user_pass, creds)) {
		creds->req_state = GF_CREDS_STATE_NONE;
		return GF_AUTHENTICATION_FAILURE;
	}
	return GF_OK;
}

GF_UserCredentials * gf_user_credentials_register(GF_DownloadManager * dm, Bool secure, const char * server_name, const char * username, const char * password, Bool valid)
{
	GF_UserCredentials * creds;
	if (!dm)
		return NULL;
	gf_assert( server_name );
	creds = gf_user_credentials_find_for_site(dm, server_name, username);
	/* If none found, we create one */
	if (!creds) {
		GF_SAFEALLOC(creds, GF_UserCredentials);
		if (!creds) return NULL;
		creds->dm = dm;
		gf_list_insert(dm->credentials, creds, 0);
	}
	creds->valid = valid;
	if (username) {
		strncpy(creds->username, username, 49);
		creds->username[49] = 0;
	} else {
		creds->username[0] = 0;
	}
	strncpy(creds->site, server_name, 1023);
	creds->site[1023] = 0;
	if (username && password && valid)
		gf_user_credentials_save_digest(dm, creds, password, GF_FALSE);
	else {
		if (GF_OK != gf_user_credentials_ask_password(dm, creds, secure)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_HTTP,
			       ("[Downloader] Failed to get password information.\n"));
			gf_list_del_item( dm->credentials, creds);
			gf_free( creds );
			creds = NULL;
		}
	}
	return creds;
}


#endif //GPAC_DISABLE_NETWORK
