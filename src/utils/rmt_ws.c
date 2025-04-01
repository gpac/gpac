#include <gpac/tools.h>
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/bitstream.h>
#include <gpac/network.h>
#include <gpac/download.h>
#include <gpac/base_coding.h>


// Global settings
static RMT_Settings g_Settings;
static Bool g_SettingsInitialized = GF_FALSE;

static const char websocket_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";


struct RMT_WS {

    // The main server thread
    GF_Thread* thread;

    // Should we stop the thread?
    Bool should_stop;

};



GF_DownloadSession *gf_dm_sess_new_server(GF_DownloadManager *dm, GF_Socket *server, void *ssl_ctx, gf_dm_user_io user_io, void *usr_cbk, Bool async, GF_Err *e);
void  gf_dm_sess_set_header(GF_DownloadSession *sess, const char *name, const char *value);
GF_Err gf_dm_sess_send_reply(GF_DownloadSession *sess, u32 reply_code, const char *response_body, u32 body_len, Bool no_body);
void gf_dm_sess_clear_headers(GF_DownloadSession *sess);
void  gf_dm_sess_set_header(GF_DownloadSession *sess, const char *name, const char *value);
GF_Err dm_sess_write(GF_DownloadSession *session, const u8 *buffer, u32 size);
GF_Err gf_dm_read_data(GF_DownloadSession *sess, char *data, u32 data_size, u32 *out_read);
void httpout_format_date(u64 time, char szDate[200], Bool for_listing);

#ifdef GPAC_HAS_SSL

void *gf_ssl_new(void *ssl_server_ctx, GF_Socket *client_sock, GF_Err *e);
void *gf_ssl_server_context_new(const char *cert, const char *key);
void gf_ssl_server_context_del(void *ssl_server_ctx);
Bool gf_ssl_init_lib();

#endif

enum {
    RMT_WEBSOCKET_CONTINUATION = 0,
    RMT_WEBSOCKET_TEXT = 1,
    RMT_WEBSOCKET_BINARY = 2,
    RMT_WEBSOCKET_CLOSE = 8,
    RMT_WEBSOCKET_PING = 9,
    RMT_WEBSOCKET_PONG = 10,
};

static char RMT_WEBSOCKET_PING_MSG[2] = { 0x89, 0x00 };

struct __rmt_serverctx {

    GF_Socket* server_sock;

    GF_SockGroup* sg;

    void *ssl_ctx;

    GF_DownloadManager* dm_sess;

    GF_List* active_clients;

};

struct __rmt_clientctx {

    GF_Socket* client_sock;
    RMT_ServerCtx* ctx;
    GF_DownloadSession* http_sess;

    char peer_address[GF_MAX_IP_NAME_LEN + 16];
    char buffer[1024];

    Bool is_ws;
    u64 last_active_time;
    u64 last_ping_time;
    Bool should_close;

    rmt_client_on_data_cbk on_data_cbk;
    void* on_data_cbk_task;

    rmt_client_on_del_cbk on_del_cbk;
    void* on_del_cbk_task;

};

GF_EXPORT
const char* rmt_get_peer_address(RMT_ClientCtx* client) {
    if (client && client->peer_address)
        return client->peer_address;
    return NULL;
}

GF_EXPORT
void* rmt_client_get_on_data_task(RMT_ClientCtx* client) {
    if (client)
        return client->on_data_cbk_task;
    return NULL;
}

GF_EXPORT
void rmt_set_on_new_client_cbk(void *task, rmt_on_new_client_cbk cbk) {
    RMT_Settings *rmtcfg = rmt_get_settings();
    if (rmtcfg) {
        rmtcfg->on_new_client_cbk = cbk;
        rmtcfg->on_new_client_cbk_task = task;
    }
}

GF_EXPORT
void* rmt_get_on_new_client_task() {
    return g_Settings.on_new_client_cbk_task;
}

GF_EXPORT
void rmt_client_set_on_del_cbk(RMT_ClientCtx* client, void* task, rmt_client_on_del_cbk cbk) {
    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("rmt_client_set_on_del_cbk client %p task %p cbk %p\n", client, task, cbk));
    if (client) {
        client->on_del_cbk = cbk;
        client->on_del_cbk_task = task;
    }
}

GF_EXPORT
void* rmt_client_get_on_del_task(RMT_ClientCtx* client) {
    if (client)
        return client->on_del_cbk_task;
    return NULL;
}

void rmt_clientctx_del(RMT_ClientCtx* client) {

    if (!client) return;

    GF_LOG(GF_LOG_INFO, GF_LOG_RMTWS, ("[RMT] closing client %s\n", client->peer_address));
    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("rmt_clientctx_del client %p del_cbk %p\n", client, client ? client->on_del_cbk : NULL));

    if (client->on_del_cbk) {
        client->on_del_cbk(client->on_del_cbk_task);
    }

    if (client->client_sock) {
        if (client->ctx && client->ctx->sg)
            gf_sk_group_unregister(client->ctx->sg, client->client_sock);

        if (!client->http_sess)
            gf_sk_del(client->client_sock); //socket deleted by gf_dm_sess_del() if http_sess has been created
        client->client_sock = NULL;
    }

    if (client->http_sess) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("deleting http_sess %p\n", client->http_sess));
        gf_dm_sess_del(client->http_sess);
        client->http_sess = NULL;
    }

    client->ctx = NULL;
    client->on_data_cbk = NULL;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("%s:%d rmt_clientctx_del client %p task %p\n", __FILE__, __LINE__, client, client->on_data_cbk_task));

    if (client->on_data_cbk_task) gf_free(client->on_data_cbk_task);
    client->on_data_cbk_task = NULL;

    if (client->on_del_cbk_task) gf_free(client->on_del_cbk_task);
    client->on_del_cbk_task = NULL;

    gf_free(client);
}

void rmt_serverctx_reset(RMT_ServerCtx* ctx) {
	if (ctx->active_clients) {
		while (gf_list_count(ctx->active_clients)) {
			RMT_ClientCtx* client = gf_list_pop_back(ctx->active_clients);
            rmt_clientctx_del(client);
            client = NULL;
		}
		gf_list_del(ctx->active_clients);
		ctx->active_clients = NULL;
	}

    if (ctx->server_sock) {
        if (ctx->sg)
            gf_sk_group_unregister(ctx->sg, ctx->server_sock);

        gf_sk_del(ctx->server_sock);
        ctx->server_sock = NULL;
    }

    if (ctx->sg) { gf_sk_group_del(ctx->sg); ctx->sg = NULL;  }

    if (ctx->dm_sess) { gf_dm_del(ctx->dm_sess); ctx->dm_sess = NULL; }

    if (ctx->ssl_ctx) {
#ifdef GPAC_HAS_SSL
		gf_ssl_server_context_del(ctx->ssl_ctx);
#else
        gf_free(ctx->ssl_ctx);
#endif
        ctx->ssl_ctx = NULL;
    }


}

void rmt_close_client(RMT_ClientCtx* client) {
    if (!client) return;
    RMT_ServerCtx* ctx = client->ctx;
    if (ctx && ctx->active_clients) {
        gf_list_del_item(ctx->active_clients, client);
    }
    rmt_clientctx_del(client);
}

GF_Err rmt_send_reply(GF_DownloadSession* http_sess, int responseCode, char* response_body, char* content_type) {

        u32 body_size = 0;
        char szFmt[100];
        char szDate[200];

	    httpout_format_date(gf_net_get_utc(), szDate, GF_FALSE);
	    gf_dm_sess_set_header(http_sess, "Date", szDate);
        gf_dm_sess_set_header(http_sess, "Server", gf_gpac_version());

        if (response_body) {
            body_size = (u32) strlen(response_body);
            gf_dm_sess_set_header(http_sess, "Content-Type", content_type ? content_type : "text/html");
            sprintf(szFmt, "%d", body_size);
            gf_dm_sess_set_header(http_sess, "Content-Length", szFmt);

        }

        return gf_dm_sess_send_reply(http_sess, responseCode, response_body, response_body ? (u32) strlen(response_body) : 0, (response_body==NULL));

}

static void rmt_on_http_session_data(void *usr_cbk, GF_NETIO_Parameter *parameter) {

    RMT_ClientCtx* client_ctx = (RMT_ClientCtx*) usr_cbk;
    if (!client_ctx) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT] sess io on null session\n"));
        return;
    }
    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("rmt_on_http_session_data on peer %s param %p msg_type %d \nerror %s \ndata %p \nsize %d \nname %p \nvalue %p \nreply %d\n",
                    client_ctx->peer_address,
                    parameter,
                    parameter->msg_type,
                    gf_error_to_string(parameter->error),
                    parameter->data,
                    parameter->size,
                    parameter->name,
                    parameter->value,
                    parameter->reply
            ));

    client_ctx->last_active_time = gf_sys_clock_high_res();

    const char* durl = gf_dm_sess_get_resource_name(parameter->sess);
    const char* ua = gf_dm_sess_get_header(parameter->sess, "User-Agent");
    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("requested url: %s with ua %s\n", durl, ua));
    GF_Err e;

    if (parameter->msg_type != GF_NETIO_PARSE_REPLY) {
        if (parameter->size) {
            strncat(client_ctx->buffer, parameter->data, parameter->size);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("session data is now: %s\n", client_ctx->buffer));
        }
    }

    if (parameter->msg_type == GF_NETIO_PARSE_REPLY) {

        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("final buffer before response: %s\n", client_ctx->buffer));

        GF_DownloadSession* http_sess = parameter->sess ;

        if (!client_ctx->is_ws) {
            const char* ws_header_version = gf_dm_sess_get_header(http_sess, "Sec-WebSocket-Version");
            const char* ws_header_key = gf_dm_sess_get_header(http_sess, "Sec-WebSocket-Key");

            if (!ws_header_version || strcmp(ws_header_version, "13") || !ws_header_key) {

                gf_dm_sess_clear_headers(http_sess);
                gf_dm_sess_set_header(http_sess, "Connection", "close");

                rmt_send_reply(http_sess, 404, "only ws connections are accepted", NULL);
                client_ctx->should_close = GF_TRUE;
                return;

            }
            else {

                char* resp_key = gf_strdup(ws_header_key);
                gf_dynstrcat(&resp_key, websocket_guid, NULL);

                u32 resp_key_len = (u32) strlen(resp_key);
                if (resp_key_len < 1)
                    return;


                u8 hash[GF_SHA1_DIGEST_SIZE];
                gf_sha1_csum( resp_key, resp_key_len, hash );

                u32 end_b64 = gf_base64_encode(hash, GF_SHA1_DIGEST_SIZE, resp_key, resp_key_len);
                resp_key[resp_key_len-1] = 0;
                if (end_b64 < resp_key_len) {
                    resp_key[end_b64] = 0;
                } else {
                    return;
                }

	            gf_dm_sess_clear_headers(http_sess);

	            gf_dm_sess_set_header(http_sess, "Connection", "Upgrade");
                gf_dm_sess_set_header(http_sess, "Upgrade", "websocket");
                gf_dm_sess_set_header(http_sess, "Sec-WebSocket-Accept", resp_key);

                e = rmt_send_reply(http_sess, 101, NULL, NULL);

                if (e==GF_OK) {
                    client_ctx->is_ws = GF_TRUE;

                    if (g_Settings.on_new_client_cbk && g_Settings.on_new_client_cbk_task) {
                        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("%s:%d calling on new client cb %p with client %p\n", __FILE__,__LINE__, g_Settings.on_new_client_cbk, client_ctx));
                        g_Settings.on_new_client_cbk(g_Settings.on_new_client_cbk_task, client_ctx);
                    }
                }
                gf_free(resp_key);
            }
        } else {
            GF_LOG(GF_LOG_WARNING, GF_LOG_RMTWS, ("this should be a ws message right?\n"));
        }



        memset(&client_ctx->buffer, 0, 1024);
    }


}


GF_Err rmt_create_server(RMT_ServerCtx* ctx) {

    GF_Err e = GF_OK;

    rmt_serverctx_reset(ctx);

    if (g_Settings.cert && g_Settings.pkey) {
#ifdef GPAC_HAS_SSL
		if (!gf_file_exists(g_Settings.cert)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT] Certificate file %s not found\n", g_Settings.cert));
			return GF_IO_ERR;
		}
		if (!gf_file_exists(g_Settings.pkey)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT] Private key file %s not found\n", g_Settings.pkey));
			return GF_IO_ERR;
		}
		if (gf_ssl_init_lib()) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT] Failed to initialize OpenSSL library\n"));
			return GF_IO_ERR;
		}

        Bool prev_noh2 = gf_opts_get_bool("core", "no-h2");
        if (!prev_noh2)
            gf_opts_set_key("core", "no-h2", "1");

        ctx->ssl_ctx = gf_ssl_server_context_new(g_Settings.cert, g_Settings.pkey);

        if (!prev_noh2)
            gf_opts_set_key("core", "no-h2", "0");

		if (!ctx->ssl_ctx) return GF_IO_ERR;
#else
		GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT] TLS key/certificate set but GPAC compiled without TLS support\n"));
		return GF_NOT_SUPPORTED;

#endif
    }

    ctx->sg = gf_sk_group_new();

    ctx->server_sock = gf_sk_new(GF_SOCK_TYPE_TCP);
    e = gf_sk_bind( ctx->server_sock,
                    g_Settings.limit_connections_to_localhost ? "127.0.0.1" : "0.0.0.0",
                    g_Settings.port,
                    NULL, 0, GF_SOCK_REUSE_PORT );

    if (!e) e = gf_sk_listen(ctx->server_sock, 0);
    if (e) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT] failed to start server on port %d: %s\n", g_Settings.port, gf_error_to_string(e) ));
        return e;
    }

    gf_sk_group_register(ctx->sg, ctx->server_sock);

    gf_sk_server_mode(ctx->server_sock, GF_TRUE);
    GF_LOG(GF_LOG_INFO, GF_LOG_RMTWS, ("[RMT] Server running on port %d\n", g_Settings.port));

    Bool prev_noh2 = gf_opts_get_bool("core", "no-h2");
    if (!prev_noh2)
        gf_opts_set_key("core", "no-h2", "1");
    ctx->dm_sess = gf_dm_new(NULL);
    if (!prev_noh2)
        gf_opts_set_key("core", "no-h2", "0");
    ctx->active_clients = gf_list_new();

    return e;

}

GF_EXPORT
void rmt_client_set_on_data_cbk(RMT_ClientCtx* client, void* task, rmt_client_on_data_cbk cbk) {
    if (client) {
        client->on_data_cbk = cbk;
        client->on_data_cbk_task = task;
    }
}

GF_Err rmt_server_handle_new_client(RMT_ServerCtx* ctx) {
    GF_Err e = GF_OK;

    void *ssl_c = NULL;

    RMT_ClientCtx* new_client;
    GF_SAFEALLOC(new_client, RMT_ClientCtx);

    new_client->ctx = ctx;

    e = gf_sk_accept(ctx->server_sock, &new_client->client_sock);
    gf_sk_group_register(ctx->sg, new_client->client_sock);

    u32 port;
    gf_sk_get_remote_address_port(new_client->client_sock, new_client->peer_address, &port);
    sprintf(new_client->peer_address + strlen(new_client->peer_address), ":%d", port); //TDOO: size check
    GF_LOG(GF_LOG_INFO, GF_LOG_RMTWS, ("[RMT] connected to remote peer %s\n",  new_client->peer_address));


#ifdef GPAC_HAS_SSL
	if (ctx->ssl_ctx) {
		ssl_c = gf_ssl_new(ctx->ssl_ctx, new_client->client_sock, &e);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT] Failed to create TLS session from %s: %s\n", new_client->peer_address, gf_error_to_string(e) ));
			rmt_clientctx_del(new_client);
			return e;
		}
	}
#endif

    new_client->http_sess = gf_dm_sess_new_server(ctx->dm_sess, new_client->client_sock, ssl_c, rmt_on_http_session_data, new_client, GF_TRUE, &e);

    new_client->is_ws = GF_FALSE;
    new_client->last_active_time = gf_sys_clock_high_res();
    new_client->last_ping_time = gf_sys_clock_high_res();

    gf_list_add(ctx->active_clients, new_client);
    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("adding active client socket %p %s\n",  new_client, new_client->peer_address));

    return e;
}

GF_Err rmt_client_send_payload(RMT_ClientCtx* client, const u8* payload, u64 size, Bool is_binary) {

    GF_Err e = GF_OK;

    GF_BitStream* respbs = gf_bs_new(NULL, size+10, GF_BITSTREAM_WRITE_DYN);
    gf_bs_write_int(respbs, 1, 1); //FIN=1
    gf_bs_write_int(respbs, 0, 3); //RSV=0
    gf_bs_write_int(respbs, is_binary ? RMT_WEBSOCKET_BINARY : RMT_WEBSOCKET_TEXT, 4); //opcode=text
    gf_bs_write_int(respbs, 0, 1); //masked=0

    if (size < 126)
        gf_bs_write_int(respbs, (s32)size, 7);
    else if (size < 65536) {
        gf_bs_write_int(respbs, 126, 7);
        gf_bs_write_long_int(respbs, size, 16);
    } else {
        gf_bs_write_long_int(respbs, 127, 7);
        gf_bs_write_long_int(respbs, size, 64);
    }

    gf_bs_write_data(respbs, payload, size);

    u8* respbuf=NULL;
    u32 respsize=0;
    gf_bs_get_content(respbs, &respbuf, &respsize);
    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("ready to send respbuf of size %d: %.*s\n", respsize, respsize, respbuf));
    e = dm_sess_write(client->http_sess, respbuf, respsize);

    gf_bs_del(respbs);
    gf_free(respbuf);

    return e;

}

GF_Err rmt_client_handle_ws_payload(RMT_ClientCtx* client, u8* payload, u64 size, Bool is_binary) {

    if (client->on_data_cbk && client->on_data_cbk_task) {
        client->on_data_cbk(client->on_data_cbk_task, payload, size, is_binary);
    }

    return GF_OK;

    // if (size)
    //     payload[0] = 'A';

    // return rmt_client_send_payload(client, payload, size, is_binary);

}

GF_Err rmt_client_handle_ws_frame(RMT_ClientCtx* client, GF_BitStream* bs) {

    GF_Err e = GF_OK;

    if (gf_bs_available(bs) < 2) return GF_IO_ERR;

    u32 FIN = gf_bs_read_int(bs, 1); //TODO: handle fragmented?
    /*u32 RSV =*/ gf_bs_read_int(bs, 3);
    u32 opcode = gf_bs_read_int(bs, 4);
    u32 masked = gf_bs_read_int(bs, 1);
    u64 payload_size = gf_bs_read_int(bs, 7);
    if (payload_size == 126) {
        if (gf_bs_available(bs) < 2) return GF_IO_ERR;
        payload_size = gf_bs_read_int(bs, 16);
    }
    else if (payload_size == 127) {
        if (gf_bs_available(bs) < 8) return GF_IO_ERR;
        payload_size = gf_bs_read_long_int(bs, 64);
    }
    if (gf_bs_available(bs) < 4) return GF_IO_ERR;
    char masking_key[4] = {0};
    if (masked) {
        gf_bs_read_data(bs, (u8*)&masking_key, 4);
    }

    u8* extra_payload = NULL;
    u32 extra_read = 0;

    if (payload_size + gf_bs_get_position(bs) > gf_bs_get_size(bs)) {
        u64 extra_size = payload_size + gf_bs_get_position(bs) - gf_bs_get_size(bs) ;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("buffer too small for payload_size %llu bs_pos %u bs_size %u => extra_size %llu\n", payload_size, gf_bs_get_position(bs), gf_bs_get_size(bs), extra_size));
        extra_payload = gf_malloc( sizeof(u8) * extra_size );

        e = GF_OK;
        while (!e && extra_read < extra_size) {
            u32 new_read = 0;
            e = gf_dm_read_data(client->http_sess, extra_payload + extra_read, extra_size-extra_read, &new_read);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("extra gf_dm_read_data => %d e=%s\n",extra_read, gf_error_to_string(e)));
            extra_read += new_read;
        }
        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("extra gf_dm_read_data => %d e=%s\n",extra_read, gf_error_to_string(e)));
    }

    u8* unmasked_payload = gf_malloc( payload_size * sizeof(u8) + 1); // add 1 to add null to get c string
    int i=0;
    for (i=0; i<payload_size && gf_bs_available(bs); i++) {
        unmasked_payload[i] = (u8) ( gf_bs_read_u8(bs) ^ masking_key[i%4] );
    }

    if (extra_payload) {
        for (u32 j=0; j<extra_read; j++) {
            unmasked_payload[i] = (u8) ( extra_payload[j] ^ masking_key[i%4] );
            i++;
        }
    }

    unmasked_payload[i] = 0;

    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("parsed ws message: FIN %d \n opcode %d \n masked %d \n payload_size %d \n masking_key %.*s \n unmasked_payload \n",
        FIN,
        opcode,
        masked,
        payload_size,
        4, masking_key
    ));

    switch(opcode) {

        case RMT_WEBSOCKET_CLOSE:
            client->should_close = GF_TRUE;
            break;

        case RMT_WEBSOCKET_TEXT:
        case RMT_WEBSOCKET_BINARY:

            e = rmt_client_handle_ws_payload(client, unmasked_payload, payload_size, (opcode==RMT_WEBSOCKET_BINARY));

            break;


        case RMT_WEBSOCKET_PING:

            GF_BitStream* respbs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE_DYN);
            gf_bs_write_int(respbs, 1, 1); //FIN=1
            gf_bs_write_int(respbs, 0, 3); //RSV=0
            gf_bs_write_int(respbs, RMT_WEBSOCKET_PONG, 4); //opcode=pong
            gf_bs_write_int(respbs, 0, 1); //masked=0
            gf_bs_write_int(respbs, (s32)payload_size, 7);
            gf_bs_write_data(respbs, unmasked_payload, payload_size);

            u8* respbuf=NULL;
            u32 respsize=0;
            gf_bs_get_content(respbs, &respbuf, &respsize);
            GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("ready to send PONG respbuf of size %d: %.*s\n", respsize, respsize, respbuf));
            e = dm_sess_write(client->http_sess, respbuf, respsize);

            gf_bs_del(respbs);
            gf_free(respbuf);

            break;


        case RMT_WEBSOCKET_CONTINUATION:    // not supported yet
        case RMT_WEBSOCKET_PONG:            // last active timer already reset
        default:
            break;
    }


    gf_free(unmasked_payload);
    if (extra_payload)
        gf_free(extra_payload);

    return e;
}

GF_Err rmt_client_handle_event(RMT_ClientCtx* client) {
    GF_Err e = GF_OK;

    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("select ready for socket %p %s\n",  client, client->peer_address));

    if (!client->is_ws) {
        e = gf_dm_sess_process(client->http_sess);
        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("[RMT] gf_dm_sess_process: %s\n",  gf_error_to_string(e)));
    }
    else {
        u8 buffer[1024]; //TODO: what size here?? do something like extra_payload??
        u32 read = 0;
        e = gf_dm_read_data(client->http_sess, buffer, 1024, &read);
        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("gf_dm_read_data => %d sHTTP %.*s\n", read, read, buffer));

        client->last_active_time = gf_sys_clock_high_res();

        GF_BitStream* bs = gf_bs_new(buffer, read, GF_BITSTREAM_READ);

        while (e==GF_OK && gf_bs_available(bs)) {

            e = rmt_client_handle_ws_frame(client, bs);

            GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("rmt_client_handle_ws_frame() returned %s bs has avail %d \n", gf_errno_str(e), gf_bs_available(bs)));

        }


        gf_bs_del(bs);


    }


    return e;
}

GF_Err rmt_client_send_ping(RMT_ClientCtx* client) {

    GF_Err e = dm_sess_write(client->http_sess, RMT_WEBSOCKET_PING_MSG, 2);
    return e;

}

GF_EXPORT
GF_Err rmt_client_send_to_ws(RMT_ClientCtx* client, const char* msg, u64 size, Bool is_binary) {

    return rmt_client_send_payload(client, (const u8*) msg, size, is_binary);

}

Bool rmt_client_should_close(RMT_ClientCtx* client) {

    // check valid object
    if (!client || !client->client_sock) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_RMTWS, ("[RMT] weird dead session in rmt\n"));
        return GF_TRUE;
    }

    if (client->should_close) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("Disconnected socket %p %s because request done\n",  client, client->peer_address));
        return GF_TRUE;
    }

    // check disconnection
    GF_Err e = gf_sk_probe(client->client_sock);
    if (e==GF_IP_CONNECTION_CLOSED) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("Disconnected socket %p %s\n",  client, client->peer_address));
        return GF_TRUE;
    }

    // check timeout
    if (g_Settings.timeout_secs) {
        u32 diff_sec = (u32) (gf_sys_clock_high_res() - client->last_active_time)/1000000;
        if ( diff_sec > g_Settings.timeout_secs ) {
            GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("Disconnected socket %p %s for timeout \n",  client, client->peer_address));
            return GF_TRUE;
        }
    }

    return GF_FALSE;

}

GF_Err rmt_server_wait_for_event(RMT_ServerCtx* ctx) {

    GF_Err e = gf_sk_group_select(ctx->sg, 10, GF_SK_SELECT_BOTH);

    if (e==GF_OK) {

        // event on server socket = new client
        if (gf_sk_group_sock_is_set(ctx->sg, ctx->server_sock, GF_SK_SELECT_READ)) {

            e = rmt_server_handle_new_client(ctx);
            if (e)
                return e;

        }

        // event on one the active client
        else {

            // check active sessions
		    u32 count = gf_list_count(ctx->active_clients);
		    for (u32 i=0; i<count; i++) {
			    RMT_ClientCtx* client = gf_list_get(ctx->active_clients, i);

                if (rmt_client_should_close(client)) {

                    gf_list_rem(ctx->active_clients, i);
                    i--;
                    count--;
                    rmt_clientctx_del(client);
                    client = NULL;
                    continue;
                }

                if (gf_sk_group_sock_is_set(client->ctx->sg, client->client_sock, GF_SK_SELECT_WRITE)) {

                    // check if we should send ping
                    if (g_Settings.ping_secs) {

                        u32 diff_sec = (u32) (gf_sys_clock_high_res() - client->last_ping_time)/1000000;

                        if ( diff_sec > g_Settings.ping_secs ) {

                            GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("Sending PING on client %p %s\n",  client, client->peer_address));
                            e = rmt_client_send_ping(client);
                            if (!e) {
                                client->last_ping_time = gf_sys_clock_high_res();
                            }
                            continue;
                        }
                    }

                }

                // handle received event
                if (gf_sk_group_sock_is_set(client->ctx->sg, client->client_sock, GF_SK_SELECT_READ)) {

                    e = rmt_client_handle_event(client);

                }
            }

        }

    }

    gf_sleep(g_Settings.msSleepBetweenServerUpdates);
    return e;

}

static u32 rmt_ws_thread_main(void* par) {

    RMT_WS* rmt = (RMT_WS*)par;

    RMT_ServerCtx* ctx;
    GF_SAFEALLOC(ctx, RMT_ServerCtx);

    GF_Err e;

    while (!rmt->should_stop) {

        // create socket if not exist
        if (!ctx->server_sock) {

            e = rmt_create_server(ctx);
            if (e) {
                if (ctx) {
                    rmt_serverctx_reset(ctx);
                    gf_free(ctx);
                    ctx = NULL;
                }
                return e;
            }

            continue;
        }

        e = rmt_server_wait_for_event(ctx);

    }

    GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("[RMT] thread main request exit! should cleanup\n"));
    rmt_serverctx_reset(ctx);
    gf_free(ctx);
    ctx = NULL;
    return GF_OK;

}



RMT_Settings* rmt_get_settings()
{
    // Default-initialize on first call
    if( g_SettingsInitialized == GF_FALSE ) {

        g_Settings.port = 6363;
        g_Settings.timeout_secs = 20;
        g_Settings.ping_secs = 7;

        g_Settings.limit_connections_to_localhost = GF_FALSE;
        g_Settings.msSleepBetweenServerUpdates = 50;

        g_Settings.on_new_client_cbk = NULL;
        g_Settings.on_new_client_cbk_task = NULL;

        g_Settings.cert = NULL;
        g_Settings.pkey = NULL;

        g_SettingsInitialized = GF_TRUE;
    }

    return &g_Settings;
}


void rmt_ws_del(RMT_WS* rmt) {

    if (rmt && rmt->thread) {


        rmt->should_stop = GF_TRUE;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("rmt_ws_del() calling stop...\n"));

        gf_th_stop(rmt->thread);

        GF_LOG(GF_LOG_DEBUG, GF_LOG_RMTWS, ("rmt_ws_del() stop returned thread status is now %d\n", gf_th_status(rmt->thread)));

        gf_th_del(rmt->thread);

        rmt->thread = NULL;

    }

    gf_free(rmt);
    rmt = NULL;

}

RMT_WS* rmt_ws_new() {
    RMT_WS* rmt = NULL;

    GF_SAFEALLOC(rmt, RMT_WS);
    if (!rmt) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT_WS] unable init rmt thread\n"));
        return NULL;
    }


    // Default-initialise if user has not set values
    rmt_get_settings();

    rmt->thread = gf_th_new("rmt_ws_main_th");
    rmt->should_stop = GF_FALSE;

    GF_Err e = gf_th_run(rmt->thread, rmt_ws_thread_main, rmt);
    if (e != GF_OK) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_RMTWS, ("[RMT_WS] unable to start websocket thread: %s\n", gf_error_to_string(e)));
        rmt_ws_del(rmt);
        rmt = NULL;
    }

    return rmt;


}
