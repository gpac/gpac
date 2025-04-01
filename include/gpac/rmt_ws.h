#ifndef RMT_WS_INCLUDED_H
#define RMT_WS_INCLUDED_H


typedef void (*rmt_on_new_client_cbk)(void *task, void* new_client);


// Handle to the main instance
typedef struct RMT_WS RMT_WS;

RMT_WS* rmt_ws_new();
void rmt_ws_del(RMT_WS* rmt_ws);

// Struture to fill in to modify default settings
typedef struct RMT_Settings
{
    // Which port to listen for incoming connections on
    u16 port;

    // inactivity timeout before closing connections
    u32 timeout_secs;

    // time between websocket ping requests (0 to disable)
    u32 ping_secs;


    // Only allow connections on localhost?
    Bool limit_connections_to_localhost;

    // How long to sleep between server updates
    u32 msSleepBetweenServerUpdates;

    // function to call when a new websocket connection is accepted
    rmt_on_new_client_cbk on_new_client_cbk;
    // context for on_new_client_cbk
    void* on_new_client_cbk_task;

    // server certificate and private key to use for ssl websocket (null to disable wss)
    const char* cert;
    const char* pkey;

} RMT_Settings;


RMT_Settings* rmt_get_settings();


typedef struct __rmt_serverctx RMT_ServerCtx;
typedef struct __rmt_clientctx RMT_ClientCtx;

typedef enum {
    RMT_CALLBACK_NONE=0x12,
    RMT_CALLBACK_JS,
    RMT_CALLBACK_NODE,
} RMT_Callback_type;


void rmt_set_on_new_client_cbk(void *task, rmt_on_new_client_cbk cbk);
void* rmt_get_on_new_client_task();

const char* rmt_get_peer_address(RMT_ClientCtx* client);
GF_Err rmt_client_send_to_ws(RMT_ClientCtx* client, const char* msg, u64 size, Bool is_binary);

typedef void (*rmt_client_on_data_cbk)(void* task, const u8* payload, u64 size, Bool is_binary);
void rmt_client_set_on_data_cbk(RMT_ClientCtx* client, void* task, rmt_client_on_data_cbk cbk);
void* rmt_client_get_on_data_task(RMT_ClientCtx* client);

typedef void (*rmt_client_on_del_cbk)(void* task);
void rmt_client_set_on_del_cbk(RMT_ClientCtx* client, void* task, rmt_client_on_del_cbk cbk);
void* rmt_client_get_on_del_task(RMT_ClientCtx* client);



//! @endcond

#endif
