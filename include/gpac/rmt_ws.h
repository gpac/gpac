#ifndef RMT_WS_INCLUDED_H
#define RMT_WS_INCLUDED_H

//! type for callbacks called when a new client connects
//! \param task user data sent back to the callback
//! \param new_client a structure representing the client (of type RMT_ClientCtx)
typedef void (*rmt_on_new_client_cbk)(void *task, void* new_client);


//! Handle to the main instance
typedef struct RMT_WS RMT_WS;

//! creates the main instance
RMT_WS* rmt_ws_new();

//! starts the server thread
void rmt_ws_run(RMT_WS*);

//! deletes the main instance
void rmt_ws_del(RMT_WS* rmt_ws);

//! Struture to fill in to modify default settings
typedef struct RMT_Settings {

    //! Which port to listen for incoming connections on
    u16 port;

    //! inactivity timeout before closing connections
    u32 timeout_secs;

    //! time between websocket ping requests (0 to disable)
    u32 ping_secs;


    //! Only allow connections on localhost?
    Bool limit_connections_to_localhost;

    //! How long to sleep between server updates
    u32 msSleepBetweenServerUpdates;

    //! function to call when a new websocket connection is accepted
    rmt_on_new_client_cbk on_new_client_cbk;
    //! context for on_new_client_cbk
    void* on_new_client_cbk_task;

    //! server certificate and private key to use for ssl websocket (null to disable wss)
    const char* cert;
    const char* pkey;

} RMT_Settings;

//! gets the current rmtws settings (creates the structure if necessary)
RMT_Settings* gf_rmt_get_settings(RMT_WS*);

//! structure representing the http server
typedef struct __rmt_serverctx RMT_ServerCtx;

//! structure representing a websocket client
typedef struct __rmt_clientctx RMT_ClientCtx;

typedef enum {
    RMT_CALLBACK_NONE=0x12,
    RMT_CALLBACK_JS,
    RMT_CALLBACK_NODE,
} RMT_Callback_type;

//! sets the callback called when new clients connect to the sever
//! \param rmt the structure representing the server handler
//! \param task user data stored and passed back to the callback
//! \param cbk the callback of type \ref rmt_on_new_client_cbk
void gf_rmt_set_on_new_client_cbk(RMT_WS* rmt, void *task, rmt_on_new_client_cbk cbk);

//! gets the userdata associated with the new client callback if defined
void* gf_rmt_get_on_new_client_task(RMT_WS*);

//! gets a string representing the client in the format ip:port
//! \param client the client object
//! \return a "ip:port" string of the given client
const char* gf_rmt_get_peer_address(RMT_ClientCtx* client);

//! sends data to a client on the websocket
//! \param client the client object
//! \param msg a buffer containing the data to send
//! \param size the size of the data to send
//! \param is_binary false if we're sending a utf8 string, true otherwise
GF_Err gf_rmt_client_send_to_ws(RMT_ClientCtx* client, const char* msg, u64 size, Bool is_binary);

//! type for callbacks called when a client receives data on the websocket
//! \param task user data passed back to the callback
//! \param payload a buffer containing the data received
//! \param size the size of the data received
//! \param is_binary false the data is a utf8 string, true otherwise
typedef void (*rmt_client_on_data_cbk)(void* task, const u8* payload, u64 size, Bool is_binary);

//! sets the callback called when a client receives data
//! \param client the client for which we are setting the callback
//! \param task user data stored and passed back to the callback
//! \param cbk the callback of type \ref rmt_client_on_data_cbk
void gf_rmt_client_set_on_data_cbk(RMT_ClientCtx* client, void* task, rmt_client_on_data_cbk cbk);

//! gets the userdata associated with the client on data callback if defined
void* gf_rmt_client_get_on_data_task(RMT_ClientCtx* client);

//! type for callbacks called when a client is deleted (e.g. on disconnects)
//! \param task user data passed back to the callback
typedef void (*rmt_client_on_del_cbk)(void* task);

//! sets the callback called when a client is deleted
//! \param client the client for which we are setting the callback
//! \param task user data stored and passed back to the callback
//! \param cbk the callback of type \ref rmt_client_on_del_cbk
void gf_rmt_client_set_on_del_cbk(RMT_ClientCtx* client, void* task, rmt_client_on_del_cbk cbk);

//! gets the userdata associated with the client on deleted callback if defined
void* gf_rmt_client_get_on_del_task(RMT_ClientCtx* client);

//! gets the ws server handler associated with a client
RMT_WS* gf_rmt_client_get_rmt(RMT_ClientCtx* client);


#endif
