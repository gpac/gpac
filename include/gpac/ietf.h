/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / IETF RTP/RTSP/SDP sub-project
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


#ifndef	_GF_IETF_H_
#define _GF_IETF_H_


#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/ietf.h>
\brief Tools for real-time streaming over IP using RTP/RTCP/RTSP/SDP .
*/
	
/*!
\addtogroup ietf_grp RTP Streaming
\ingroup media_grp
\brief  Tools for real-time streaming over IP using RTP/RTCP/RTSP/SDP.


This section documents the tools used for real-time streaming over IP using RTP/RTCP/RTSP/SDP.

@{
 */

#include <gpac/list.h>

#ifndef GPAC_DISABLE_STREAMING

#include <gpac/bitstream.h>
#include <gpac/sync_layer.h>
#include <gpac/network.h>


/*! RTSP version supported by GPAC*/
#define GF_RTSP_VERSION		"RTSP/1.0"


/*! RTSP NOTIF CODES */
enum
{
	NC_RTSP_Continue		=		100,
	NC_RTSP_OK				=		200,
	NC_RTSP_Created			=		201,
	NC_RTSP_Low_on_Storage_Space	=	250,

	NC_RTSP_Multiple_Choice	=	300,
	NC_RTSP_Moved_Permanently	=	301,
	NC_RTSP_Moved_Temporarily	=	302,
	NC_RTSP_See_Other	=	303,
	NC_RTSP_Use_Proxy	=	305,

	NC_RTSP_Bad_Request	=	400,
	NC_RTSP_Unauthorized	=	401,
	NC_RTSP_Payment_Required	=	402,
	NC_RTSP_Forbidden	=	403,
	NC_RTSP_Not_Found	=	404,
	NC_RTSP_Method_Not_Allowed	=	405,
	NC_RTSP_Not_Acceptable	=	406,
	NC_RTSP_Proxy_Authentication_Required	=	407,
	NC_RTSP_Request_Timeout	=	408,
	NC_RTSP_Gone	=	410,
	NC_RTSP_Length_Required	=	411,
	NC_RTSP_Precondition_Failed	=	412,
	NC_RTSP_Request_Entity_Too_Large	=	413,
	NC_RTSP_Request_URI_Too_Long	=	414,
	NC_RTSP_Unsupported_Media_Type	=	415,

	NC_RTSP_Invalid_parameter	=	451,
	NC_RTSP_Illegal_Conference_Identifier	=	452,
	NC_RTSP_Not_Enough_Bandwidth	=	453,
	NC_RTSP_Session_Not_Found	=	454,
	NC_RTSP_Method_Not_Valid_In_This_State	=	455,
	NC_RTSP_Header_Field_Not_Valid	=	456,
	NC_RTSP_Invalid_Range	=	457,
	NC_RTSP_Parameter_Is_ReadOnly	=	458,
	NC_RTSP_Aggregate_Operation_Not_Allowed	=	459,
	NC_RTSP_Only_Aggregate_Operation_Allowed	=	460,
	NC_RTSP_Unsupported_Transport	=	461,
	NC_RTSP_Destination_Unreachable	=	462,

	NC_RTSP_Internal_Server_Error	=	500,
	NC_RTSP_Not_Implemented	=	501,
	NC_RTSP_Bad_Gateway	=	502,
	NC_RTSP_Service_Unavailable	=	503,
	NC_RTSP_Gateway_Timeout	=	504,
	NC_RTSP_RTSP_Version_Not_Supported	=	505,

	NC_RTSP_Option_not_support	=	551,
};

/*! Gives string description of error code
\param ErrCode the RTSP error code
\return the description of the RTSP error code
*/
const char *gf_rtsp_nc_to_string(u32 ErrCode);

/*
		Common structures between commands and responses
*/

/*! RTSP Range information
 	RTSP Session level only, although this is almost the same
	format as an SDP range, this is not used in the SDP lib as "a=range" is not part of SDP
	but part of RTSP
*/
typedef struct {
	/* start and end range. If end is -1, the range is open (from start to unknown) */
	Double start, end;
	/* use SMPTE range (Start and End specify the number of frames) (currently not supported) */
	u32 UseSMPTE;
	/* framerate for SMPTE range */
	Double FPS;
} GF_RTSPRange;

/*! parses a Range line and returns range header structure. This can be used for RTSP extension of SDP
\note Only support for npt for now
\param range_buf the range string
\return a newly allocated RTSP range
*/
GF_RTSPRange *gf_rtsp_range_parse(char *range_buf);
/*! creates a new RTSP range
\return a newly allocated RTSP range
*/
GF_RTSPRange *gf_rtsp_range_new();
/*! destroys a RTSP range
\param range the target RTSP range
*/
void gf_rtsp_range_del(GF_RTSPRange *range);

/*
			Transport structure
		contains all network info for RTSP sessions (ports, uni/multi-cast, ...)
*/

/*! RTSP AVP Transport Profile */
#define GF_RTSP_PROFILE_RTP_AVP			"RTP/AVP"
/*! RTSP AVP + TCP Transport Profile */
#define GF_RTSP_PROFILE_RTP_AVP_TCP		"RTP/AVP/TCP"
/*! RTSP UDP Transport Profile */
#define GF_RTSP_PROFILE_UDP				"udp"

/*! RTSP transport structure*/
typedef struct
{
	/* set to 1 if unicast */
	Bool IsUnicast;
	/* for multicast */
	char *destination;
	/* for redirections internal to servers */
	char *source;
	/*IsRecord is usually 0 (PLAY) . If set, Append specify that the stream should
	be concatenated to existing resources */
	Bool IsRecord, Append;
	/* in case transport is on TCP/RTSP, If only 1 ID is specified, it is stored in rtpID (this
	is not RTP interleaving) */
	Bool IsInterleaved;
	u32 rtpID, rtcpID;
	/* Multicast specific */
	u32 MulticastLayers;
	u8 TTL;
	/*RTP specific*/

	/*port for multicast*/
	/*server port in unicast - RTP implies low is even , and last is low+1*/
	u16 port_first, port_last;
	/*client port in unicast - RTP implies low is even , and last is low+1*/
	u16 client_port_first, client_port_last;
	u32 SSRC;

	/*Transport protocol. In this version we only support RTP/AVP, the following flag tells
	us if this is RTP/AVP/TCP or RTP/AVP (default)*/
	char *Profile;

	Bool is_sender;
} GF_RTSPTransport;

/*! clones a RTSP transport
\param transp the target RTSP transport
\return an allocated copy RTSP transport
*/
GF_RTSPTransport *gf_rtsp_transport_clone(GF_RTSPTransport *transp);
/*! destroys a RTSP transport
\param transp the target RTSP transport
*/
void gf_rtsp_transport_del(GF_RTSPTransport *transp);


/*
				RTSP Command
		the RTSP Response is sent by a client / received by a server
	text Allocation is done by the lib when parsing a command, and
	is automatically freed when calling reset / delete. Therefore you must
	set/allocate the fields yourself when writing a command (client)

*/

/*ALL RTSP METHODS - all other methods will be ignored*/
/*! RTSP DESCRIBE method*/
#define GF_RTSP_DESCRIBE		"DESCRIBE"
/*! RTSP SETUP method*/
#define GF_RTSP_SETUP			"SETUP"
/*! RTSP PLAY method*/
#define GF_RTSP_PLAY			"PLAY"
/*! RTSP PAUSE method*/
#define GF_RTSP_PAUSE			"PAUSE"
/*! RTSP RECORD method*/
#define GF_RTSP_RECORD			"RECORD"
/*! RTSP TEARDOWN method*/
#define GF_RTSP_TEARDOWN		"TEARDOWN"
/*! RTSP GET_PARAMETER method*/
#define GF_RTSP_GET_PARAMETER	"GET_PARAMETER"
/*! RTSP SET_PARAMETER method*/
#define GF_RTSP_SET_PARAMETER	"SET_PARAMETER"
/*! RTSP OPTIONS method*/
#define GF_RTSP_OPTIONS			"OPTIONS"
/*! RTSP ANNOUCE method*/
#define GF_RTSP_ANNOUNCE		"ANNOUNCE"
/*! RTSP REDIRECT method*/
#define GF_RTSP_REDIRECT		"REDIRECT"

/*! RTSP command structure*/
typedef struct
{
	char *Accept;
	char *Accept_Encoding;
	char *Accept_Language;
	char *Authorization;
	u32 Bandwidth;
	u32 Blocksize;
	char *Cache_Control;
	char *Conference;
	char *Connection;
	u32 Content_Length;
	u32 CSeq;
	char *From;
	char *Proxy_Authorization;
	char *Proxy_Require;
	GF_RTSPRange *Range;
	char *Referer;
	Double Scale;
	char *Session;
	Double Speed;
	/*nota : RTSP allows several configurations for a single channel (multicast and
	unicast , ...). Usually only 1*/
	GF_List *Transports;
	char *User_Agent;

	/*type of the command, one of the described above*/
	char *method;

	/*Header extensions*/
	GF_List *Xtensions;

	/*body of the command, size is Content-Length (auto computed when sent). It is not
	terminated by a NULL char*/
	char *body;

	/*
			Specify ControlString if your request targets
		a specific media stream in the service. If null, the service name only will be used
		for control (for ex, both A and V streams in a single file)
		If the request is GF_RTSP_OPTIONS, you must provide a control string containing the options
		you want to query
	*/
	char *ControlString;

	/*user data: this is never touched by the lib, its intend is to help stacking
	RTSP commands in your app*/
	void *user_data;
	/*user flags: this is never touched by the lib, its intend is to help stacking
	RTSP commands in your app*/
	u32 user_flags;


	/*
		Server side Extensions
	*/

	/*full URL of the command. Not used at client side, as the URL is ALWAYS relative
	to the server / service of the RTSP session
	On the server side however redirections are up to the server, so we cannot decide for it	*/
	char *service_name;
	/*RTSP status code of the command as parsed. One of the above RTSP StatusCode*/
	u32 StatusCode;

	Bool is_resend;
} GF_RTSPCommand;

/*! creates an RTSP command
\return the newly allocated RTSP command
*/
GF_RTSPCommand *gf_rtsp_command_new();
/*! destroys an RTSP command
\param com the target RTSP command
*/
void gf_rtsp_command_del(GF_RTSPCommand *com);
/*! resets an RTSP command
\param com the target RTSP command
*/
void gf_rtsp_command_reset(GF_RTSPCommand *com);



/*
				RTSP Response
		the RTSP Response is received by a client / sent by a server
	text Allocation is done by the lib when parsing a response, and
	is automatically freed when calling reset / delete. Therefore you must
	allocate the fields yourself when writing a response (server)

*/

/*! RTP-Info for RTP channels.

	There may be several RTP-Infos in one response
	based on the server implementation (DSS/QTSS begaves this way)
*/
typedef struct
{
	/*control string of the channel*/
	char *url;
	/*seq num for asociated rtp_time*/
	u32 seq;
	/*rtp TimeStamp corresponding to the Range start specified in the PLAY request*/
	u32 rtp_time;
	/*ssrc of sender if known, 0 otherwise*/
	u32 ssrc;
} GF_RTPInfo;



/*! RTSP Response */
typedef struct
{
	/* response code*/
	u32 ResponseCode;
	/* comment from the server */
	char *ResponseInfo;

	/*	Header Fields	*/
	char *Accept;
	char *Accept_Encoding;
	char *Accept_Language;
	char *Allow;
	char *Authorization;
	u32 Bandwidth;
	u32 Blocksize;
	char *Cache_Control;
	char *Conference;
	char *Connection;
	char *Content_Base;
	char *Content_Encoding;
	char *Content_Language;
	u32 Content_Length;
	char *Content_Location;
	char *Content_Type;
	u32 CSeq;
	char *Date;
	char *Expires;
	char *From;
	char *Host;
	char *If_Match;
	char *If_Modified_Since;
	char *Last_Modified;
	char *Location;
	char *Proxy_Authenticate;
	char *Proxy_Require;
	char *Public;
	GF_RTSPRange *Range;
	char *Referer;
	char *Require;
	char *Retry_After;
	GF_List *RTP_Infos;
	Double Scale;
	char *Server;
	char *Session;
	u32 SessionTimeOut;
	Double Speed;
	u32 StreamID; //only when sess->satip is true
	char *Timestamp;
	/*nota : RTSP allows several configurations for a single channel (multicast and
	unicast , ...). Usually only 1*/
	GF_List *Transports;
	char *Unsupported;
	char *User_Agent;
	char *Vary;
	char *Via;
	char *WWW_Authenticate;

	/*Header extensions*/
	GF_List *Xtensions;

	/*body of the response, size is Content-Length (auto computed when sent). It is not
	terminated by a NULL char when response is parsed but must be null-terminated when
	response is being sent*/
	char *body;
} GF_RTSPResponse;

/*! creates an RTSP response
\return the newly allocated RTSP response
*/
GF_RTSPResponse *gf_rtsp_response_new();
/*! deletes an RTSP response
\param rsp the target RTSP response
*/
void gf_rtsp_response_del(GF_RTSPResponse *rsp);
/*! resets an RTSP response
\param rsp the target RTSP response
*/
void gf_rtsp_response_reset(GF_RTSPResponse *rsp);


/*! RTSP session*/
typedef struct _tag_rtsp_session GF_RTSPSession;

/*! creates a new RTSP session
\param sURL the target RTSP session URL
\param DefaultPort the target RTSP session port
\return a newly allocated RTSP session
*/
GF_RTSPSession *gf_rtsp_session_new(char *sURL, u16 DefaultPort);
/*! destroys an RTSP session
\param sess the target RTSP session
*/
void gf_rtsp_session_del(GF_RTSPSession *sess);

/*! sets TCP buffer size of an RTSP session
\param sess the target RTSP session
\param BufferSize desired buffer size in bytes
\return error if any
*/
GF_Err gf_rtsp_set_buffer_size(GF_RTSPSession *sess, u32 BufferSize);


/*! resets state machine, invalidate SessionID
\note RFC2326 requires that the session is reseted when all RTP streams
are closed. As this lib doesn't maintain the number of valid streams
you MUST call reset when all your streams are shutdown (either requested through
TEARDOWN or signaled through RTCP BYE packets for RTP, or any other signaling means
for other protocols)
\param sess the target RTSP session
\param ResetConnection if set, this will destroy the associated TCP socket. This is useful in case of timeouts, because
some servers do not restart with the right CSeq.
\return number of retries for reset happening before first server reply, 0 otherwise
*/
u32 gf_rtsp_session_reset(GF_RTSPSession *sess, Bool ResetConnection);

/*! checks if an RTSP session matches an RTSP URL
\param sess the target RTSP session
\param url the URL to test
\return GF_TRUE if the session matches the URL, GF_FALSE otherwise
*/
Bool gf_rtsp_is_my_session(GF_RTSPSession *sess, char *url);

/*! gets server name of an RTSP session
\param sess the target RTSP session
\return the server name
*/
const char *gf_rtsp_get_server_name(GF_RTSPSession *sess);

/*! gets user name of an RTSP session
\param sess the target RTSP session
\return the user name or NULL if none
*/
const char *gf_rtsp_get_user(GF_RTSPSession *sess);

/*! gets password of an RTSP session
\param sess the target RTSP session
\return the password or NULL if none
*/
const char *gf_rtsp_get_password(GF_RTSPSession *sess);


/*! gets server port of an RTSP session
\param sess the target RTSP session
\return the server port
*/
u16 gf_rtsp_get_session_port(GF_RTSPSession *sess);

/*! checks if RTSP connection is over TLS
\param sess the target RTSP session
\return GF_TRUE if connection is secured
*/
Bool gf_rtsp_use_tls(GF_RTSPSession *sess);

/*! fetches an RTSP response from the server.  the GF_RTSPResponse will be reseted before fetch
\param sess the target RTSP session
\param rsp the response object to fill with the response. This will be reseted before TCP fetch
\return error if any
*/
GF_Err gf_rtsp_get_response(GF_RTSPSession *sess, GF_RTSPResponse *rsp);


/*! RTSP States. The only non blocking mode is GF_RTSP_STATE_WAIT_FOR_CONTROL*/
enum
{
	/*! Initialized (connection might be off, but all structures are in place)
	This is the default state between # requests (aka, DESCRIBE and SETUP
	or SETUP and PLAY ...)*/
	GF_RTSP_STATE_INIT	=	0,
	/*! Waiting*/
	GF_RTSP_STATE_WAITING,
	/*! PLAY, PAUSE, RECORD. Aggregation is allowed for the same type, you can send several command
	in a row. However the session will return GF_SERVICE_ERROR if you do not have
	a valid SessionID in the command
	You cannot issue a SETUP / DESCRIBE while in this state*/
	GF_RTSP_STATE_WAIT_FOR_CONTROL,

	/*! FATAL ERROR: session is invalidated by server. Call reset and restart from SETUP if needed*/
	GF_RTSP_STATE_INVALIDATED
};

/*! gets the RTSP session state
\param sess the target RTSP session
\return the session state
*/
u32 gf_rtsp_get_session_state(GF_RTSPSession *sess);
/*! forces a reset of the state to GF_RTSP_STATE_INIT
\param sess the target RTSP session
*/
void gf_rtsp_reset_aggregation(GF_RTSPSession *sess);

/*! sends an RTSP request to the server.
\param sess the target RTSP session
\param com the RTSP command to send
\return error if any
*/
GF_Err gf_rtsp_send_command(GF_RTSPSession *sess, GF_RTSPCommand *com);

/*! checks connection status - should be called before processing any RTSP for non-blocking IO
\param sess the target RTSP session
\return GF_IP_NETWORK_EMPTY if connection is still pending or data cannot be flushed, GF_OK if connected and no more data to send or error if any
*/
GF_Err gf_rtsp_check_connection(GF_RTSPSession *sess);


/*! callback function for interleaved RTSP/TCP transport
\param sess the target RTSP session
\param cbk_ptr opaque data
\param buffer RTP or RTCP packet
\param bufferSize packet size in bytes
\param IsRTCP set to GF_TRUE if the packet is an RTCP packet, GF_FALSE otherwise
\return error if any
*/
typedef GF_Err (*gf_rtsp_interleave_callback)(GF_RTSPSession *sess, void *cbk_ptr, u8 *buffer, u32 bufferSize, Bool IsRTCP);

/*! assigns the callback function for interleaved RTSP/TCP transport
\param sess the target RTSP session
\param SignalData the callback function on each interleaved packet
\return error if any
*/
GF_Err gf_rtsp_set_interleave_callback(GF_RTSPSession *sess, gf_rtsp_interleave_callback SignalData);

/*! reads RTSP session (response fetch and interleaved RTSP/TCP transport)
\param sess the target RTSP session
\return error if any
*/
GF_Err gf_rtsp_session_read(GF_RTSPSession *sess);

/*! registers a new interleaved RTP channel over an RTSP connection
\param sess the target RTSP session
\param the_ch opaque data passed to \ref gf_rtsp_interleave_callback
\param LowInterID ID of the RTP interleave channel
\param HighInterID ID of the RCTP interleave channel
\return error if any
*/
GF_Err gf_rtsp_register_interleave(GF_RTSPSession *sess, void *the_ch, u8 LowInterID, u8 HighInterID);
/*! unregisters a new interleaved RTP channel over an RTSP connection
\param sess the target RTSP session
\param LowInterID ID of the RTP interleave channel
\return the numbers of registered interleaved channels remaining
*/
u32 gf_rtsp_unregister_interleave(GF_RTSPSession *sess, u8 LowInterID);


/*! creates a new RTSP session from an existing socket in listen state. If no pending connection
	is detected, return NULL
\param rtsp_listener the listening server socket
\param allow_http_tunnel indicate if HTTP tunnel should be enabled
\param ssl_ctx OpenSSL context
\return the newly allocated RTSP session if any, NULL otherwise
*/
GF_RTSPSession *gf_rtsp_session_new_server(GF_Socket *rtsp_listener, Bool allow_http_tunnel, void *ssl_ctx);

/*! special error code for \ref gf_rtsp_get_command*/
#define GF_RTSP_TUNNEL_POST	-1000
/*! fetches an RTSP request
\param sess the target RTSP session
\param com the RTSP command to fill with the command. This will be reseted before fetch
\return error if any or GF_RTSP_TUNNEL_POST if this is a POST on a HTPP tunnel
*/
GF_Err gf_rtsp_get_command(GF_RTSPSession *sess, GF_RTSPCommand *com);

/*! generates a session ID for the given session
\param sess the target RTSP session
\return an allocated string containing a session ID
*/
char *gf_rtsp_generate_session_id(GF_RTSPSession *sess);

/*! sends an RTSP response
\param sess the target RTSP session
\param rsp the response to send
\return error if any
*/
GF_Err gf_rtsp_send_response(GF_RTSPSession *sess, GF_RTSPResponse *rsp);

/*! gets the IP address of the local host running the session
\param sess the target RTSP session
\param buffer buffer to store the local host name
\return error if any
*/
GF_Err gf_rtsp_get_session_ip(GF_RTSPSession *sess, char buffer[GF_MAX_IP_NAME_LEN]);

/*! gets the IP address of the connected peer
\param sess the target RTSP session
\param buffer buffer to store the connected peer name
\return error if any
*/
GF_Err gf_rtsp_get_remote_address(GF_RTSPSession *sess, char *buffer);

/*! writes a packet on an interleaved channel over RTSP
\param sess the target RTSP session
\param idx ID (RTP or RTCP) of the interleaved channel
\param pck packet data (RTP or RTCP packet) to write
\param pck_size packet size in bytes
\return error if any
*/
GF_Err gf_rtsp_session_write_interleaved(GF_RTSPSession *sess, u32 idx, u8 *pck, u32 pck_size);

/*! gets sessioncookie for HTTP tunnel
\param sess the target RTSP session
\return cookie or NULL if none
*/
const char *gf_rtsp_get_session_cookie(GF_RTSPSession *sess);

/*! move TCP connection of a POST HTTP tunnel link to main session
\param sess the target RTSP session
\param post_sess the target RTSP POST http tunnel session - the session is not destroyed, only its connection is detached
\return error if any
*/
GF_Err gf_rtsp_merge_tunnel(GF_RTSPSession *sess, GF_RTSPSession *post_sess);

/*! sets associated netcap rules
\param sess the target RTSP session
\param netcap_id ID of netcap configuration to use, may be null (see gpac -h netcap)

*/
void gf_rtsp_session_set_netcap_id(GF_RTSPSession *sess, const char *netcap_id);

/*
		RTP LIB EXPORTS
*/

/*! RTP header */
typedef struct tagRTP_HEADER {
	/*version, must be 2*/
	u8 Version;
	/*padding bits in the payload*/
	u8 Padding;
	/*header extension is defined*/
	u8 Extension;
	/*number of CSRC (<=15)*/
	u8 CSRCCount;
	/*Marker Bit*/
	u8 Marker;
	/*payload type on 7 bits*/
	u8 PayloadType;
	/*packet seq number*/
	u16 SequenceNumber;
	/*packet time stamp*/
	u32 TimeStamp;
	/*sync source identifier*/
	u32 SSRC;
	/*in our basic client, CSRC should always be NULL*/
	u32 CSRC[16];

	/*internal to out lib*/
	u64 recomputed_ntp_ts;
} GF_RTPHeader;


/*! RTPMap information*/
typedef struct
{
	/*dynamic payload type of this map*/
	u32 PayloadType;
	/*registered payload name of this map*/
	char *payload_name;
	/*RTP clock rate (TS resolution) of this map*/
	u32 ClockRate;
	/*optional parameters for audio, specifying number of channels. Unused for other media types.*/
	u32 AudioChannels;
} GF_RTPMap;


/*! RTP channel*/
typedef struct __tag_rtp_channel GF_RTPChannel;

/*! creates a new RTP channel
\return a newly allocated RTP channel
*/
GF_RTPChannel *gf_rtp_new();

/*! creates a new RTP channel
\param netcap_id ID of netcap configuration to use, may be null (see gpac -h netcap)
\return a newly allocated RTP channel
*/
GF_RTPChannel *gf_rtp_new_ex(const char *netcap_id);

/*! destroys an RTP channel
\param ch the target RTP channel
*/
void gf_rtp_del(GF_RTPChannel *ch);

/*! setup transport for an RTP channel
A server channelis configured through the transport structure, with the same info as a
client channel, the client_port_* info designing the REMOTE client and port_* designing
the server channel
\param ch the target RTP channel
\param trans_info the transport info for this channel
\param remote_address the remote / destination address of the channel
\return error if any
*/
GF_Err gf_rtp_setup_transport(GF_RTPChannel *ch, GF_RTSPTransport *trans_info, const char *remote_address);

/*! setup of rtp/rtcp transport ports
This only applies in unicast, non interleaved cases.
For multicast port setup MUST be done through the above gf_rtp_setup_transport function
This will take care of port reuse
\param ch the target RTP channel
\param first_port RTP port number of the RTP channel
\return error if any
*/
GF_Err gf_rtp_set_ports(GF_RTPChannel *ch, u16 first_port);

/*! init of RTP payload information. Only ONE payload per sync source is supported in this
version of the library (a sender cannot switch payload types on a single media)
\param ch the target RTP channel
\param PayloadType identifier of RTP payload
\param ClockRate clock rate in (1/Hz) of RTP channel
\return error if any
*/
GF_Err gf_rtp_setup_payload(GF_RTPChannel *ch, u32 PayloadType, u32 ClockRate);

/*! enables sending of NAT keep-alive packets for NAT traversal
\param ch the target RTP channel
\param nat_timeout specifies the inactivity period in ms after which NAT keepalive packets are sent. If 0, disables NAT keep-alive packets
*/
void gf_rtp_enable_nat_keepalive(GF_RTPChannel *ch, u32 nat_timeout);


/*! initializes the RTP channel.
\param ch the target RTP channel
\param UDPBufferSize UDP stack buffer size if configurable by OS/ISP - ignored otherwise
\note On WinCE devices, this is not configurable on an app bases but for the whole OS
you must update the device registry with:
\code
	[HKEY_LOCAL_MACHINE\Comm\Afd]
	DgramBuffer=dword:N
\endcode

	where N is the number of UDP datagrams a socket should be able to buffer. For multimedia
app you should set N as large as possible. The device MUST be reseted for the param to take effect

\param IsSource if true, the channel is a sender (media data, sender report, Receiver report processing)
if source, you must specify the Path MTU size. The RTP lib won't send any packet bigger than this size
your application shall perform payload size splitting if needed
\param PathMTU desired path MTU (max packet size) in bytes
\param ReorederingSize max number of packets to queue for reordering. 0 means no reordering
\param MaxReorderDelay max time to wait in ms before releasing first packet in reoderer when only one packet is present.
If 0 and reordering size is specified, defaults to 200 ms (usually enough).

\param local_interface_ip local interface address to use for multicast. If NULL, default address is used
\return error if any
*/
GF_Err gf_rtp_initialize(GF_RTPChannel *ch, u32 UDPBufferSize, Bool IsSource, u32 PathMTU, u32 ReorederingSize, u32 MaxReorderDelay, char *local_interface_ip);

/*! stops the RTP channel. This destrpys RTP and RTCP sockets as well as packet reorderer
\param ch the target RTP channel
\return error if any
*/
GF_Err gf_rtp_stop(GF_RTPChannel *ch);

/*! sets source-specific IPs - this MUST be called before calling \ref gf_rtp_initialize
\param ch the target RTP channel
\param src_ip_inc IP of sources to receive from
\param nb_src_ip_inc number of sources to receive from
\param src_ip_exc IP of sources to exclude
\param nb_src_ip_exc number of sources to exclude
\return error if any
*/
GF_Err gf_rtp_set_ssm(GF_RTPChannel *ch, const char **src_ip_inc, u32 nb_src_ip_inc, const char **src_ip_exc, u32 nb_src_ip_exc);

/*! inits the RTP info after a PLAY or PAUSE, rtp_time is the rtp TimeStamp of the RTP packet
with seq_num sequence number. This info is needed to compute the CurrentTime of the RTP channel
ssrc may not be known if sender hasn't indicated it (use 0 then)
\param ch the target RTP channel
\param seq_num the seq num of the next packet to be received
\param rtp_time the time in RTP timescale of the next packet to be received
\param ssrc the SSRC identifier of the next packet to be received
\return error if any
*/
GF_Err gf_rtp_set_info_rtp(GF_RTPChannel *ch, u32 seq_num, u32 rtp_time, u32 ssrc);

/*! retrieves current RTP time in sec. If rtp_time was unknown (not on demand media) the time is absolute.
Otherwise this is the time in ms elapsed since the last PLAY range start value
Not supported yet if played without RTSP (aka RTCP time not supported)
\param ch the target RTP channel
\return NTP clock in seconds
*/
Double gf_rtp_get_current_time(GF_RTPChannel *ch);

/*! resets both sockets and packet reorderer
\param ch the target RTP channel
*/
void gf_rtp_reset_buffers(GF_RTPChannel *ch);

/*! resets sender SSRC
\param ch the target RTP channel
*/
void gf_rtp_reset_ssrc(GF_RTPChannel *ch);

/*! reads any RTP data on UDP only (not valid for TCP). Performs re-ordering if configured for it
\param ch the target RTP channel
\param buffer the buffer where to store the RTP packet
\param buffer_size the size of the buffer
\return amount of data read in bytes
*/
u32 gf_rtp_read_rtp(GF_RTPChannel *ch, u8 *buffer, u32 buffer_size);

/*! flushes any pending data in packet reorderer, but does not flush packet reorderer if reorderer timeout is not exceeded
\param ch the target RTP channel
\param buffer the buffer where to store the data
\param buffer_size the size of the buffer
\return amount of data read in bytes
*/
u32 gf_rtp_flush_rtp(GF_RTPChannel *ch, u8 *buffer, u32 buffer_size);

/*! reads any RTCP data on UDP only (not valid for TCP). Performs re-ordering if configured for it
\param ch the target RTP channel
\param buffer the buffer where to store the RTCP packet
\param buffer_size the size of the buffer
\return amount of data read in bytes
*/
u32 gf_rtp_read_rtcp(GF_RTPChannel *ch, u8 *buffer, u32 buffer_size);

/*! flushes any pending data in packet reorderer, and flushes packet reorderer if reorderer timeout is not exceeded. Typically called several times until returning 0.
\param ch the target RTP channel
\param buffer the buffer where to store the data
\param buffer_size the size of the buffer
\return amount of data read in bytes
*/
u32 gf_rtp_read_flush(GF_RTPChannel *ch, u8 *buffer, u32 buffer_size);

/*! decodes an RTP packet and gets the beginning of the RTP payload
\param ch the target RTP channel
\param pck the RTP packet buffer
\param pck_size the size of the RTP packet
\param rtp_hdr filled with decoded RTP header information
\param PayloadStart set to the offset in bytes of the start of the payload in the RTP packet
\return error if any
*/
GF_Err gf_rtp_decode_rtp(GF_RTPChannel *ch, u8 *pck, u32 pck_size, GF_RTPHeader *rtp_hdr, u32 *PayloadStart);

/*! decodes an RTCP packet and update timing info, send ReceiverReport too
\param ch the target RTP channel
\param pck the RTP packet buffer
\param pck_size the size of the RTP packet
\param has_sr set to GF_TRUE if the RTCP packet contained an SenderReport
\return error if any
*/
GF_Err gf_rtp_decode_rtcp(GF_RTPChannel *ch, u8 *pck, u32 pck_size, Bool *has_sr);

/*! computes and send Receiver report.
If the channel is a TCP channel, you must specify
the callback function.
\note Many RTP implementation do NOT process RTCP info received on TCP...
the lib will decide whether the report shall be sent or not, therefore you should call
this function at regular times
\param ch the target RTP channel
\return error if any
*/
GF_Err gf_rtp_send_rtcp_report(GF_RTPChannel *ch);

/*! forces loss rate for next Receiver report
\param ch the target RTP channel
\param loss_rate loss rate in per-thousand
*/
void gf_rtp_set_loss_rate(GF_RTPChannel *ch, u32 loss_rate);

/*! sends a BYE info (leaving the session)
\param ch the target RTP channel
\return error if any
*/
GF_Err gf_rtp_send_bye(GF_RTPChannel *ch);


/*! sends an RTP packet. In fast_send mode,
\param ch the target RTP channel
\param rtp_hdr the RTP header of the packet
\param pck the RTP payload buffer
\param pck_size the RTP payload size
\param fast_send if set, the payload buffer contains 12 bytes available BEFORE its indicated start where the RTP header is written in place
\return error if any
*/
GF_Err gf_rtp_send_packet(GF_RTPChannel *ch, GF_RTPHeader *rtp_hdr, u8 *pck, u32 pck_size, Bool fast_send);


/*! callback used for writing rtp over TCP
\param cbk1 opaque user data
\param cbk2 opaque user data
\param is_rtcp indicates the data is an RTCP packet
\param pck the RTP/RTCP buffer
\param pck_size the RTP/RTCP size
\return error if any
*/
typedef GF_Err (*gf_rtp_tcp_callback)(void *cbk1, void *cbk2, Bool is_rtcp, u8 *pck, u32 pck_size);

/*! sets RTP interleaved callback on the RTP channel
\param ch the target RTP channel
\param tcp_callback the callback function
\param cbk1 user data for the callback function
\param cbk2 user data for the callback function
\return error if any
*/
GF_Err gf_rtp_set_interleave_callbacks(GF_RTPChannel *ch, gf_rtp_tcp_callback tcp_callback, void *cbk1, void *cbk2);

/*! checks if an RTP channel is unicast
\param ch the target RTP channel
\return GF_TRUE if unicast, GF_FALSE otherwise
*/
u32 gf_rtp_is_unicast(GF_RTPChannel *ch);
/*! checks if an RTP channel is interleaved
\param ch the target RTP channel
\return GF_TRUE if interleaved, GF_FALSE otherwise
*/
u32 gf_rtp_is_interleaved(GF_RTPChannel *ch);
/*! gets clockrate/timescale of an RTP channel
\param ch the target RTP channel
\return the RTP clock rate
*/
u32 gf_rtp_get_clockrate(GF_RTPChannel *ch);
/*! gets the low interleave ID of an RTP channel
\param ch the target RTP channel
\return the low (RTP) interleave ID of the channel, or 0 if not interleaved
*/
u8 gf_rtp_get_low_interleave_id(GF_RTPChannel *ch);
/*! gets the high interleave ID of an RTP channel
\param ch the target RTP channel
\return the high (RTCP) interleave ID of the channel, or 0 if not interleaved
*/
u8 gf_rtp_get_hight_interleave_id(GF_RTPChannel *ch);
/*! gets the transport associated with an RTP channel
\param ch the target RTP channel
\return the RTSP transport information
*/
const GF_RTSPTransport *gf_rtp_get_transport(GF_RTPChannel *ch);

/*! gets loss rate of the RTP channel
\param ch the target RTP channel
\return the loss rate of the channel, between 0 and 100 %
*/
Float gf_rtp_get_loss(GF_RTPChannel *ch);
/*! gets number of TCP bytes send for an interleaved channel
\param ch the target RTP channel
\return the number of bytes sent
*/
u32 gf_rtp_get_tcp_bytes_sent(GF_RTPChannel *ch);
/*! gets ports of an non-interleaved RTP channel
\param ch the target RTP channel
\param rtp_port the RTP port number
\param rtcp_port the RCTP port number
*/
void gf_rtp_get_ports(GF_RTPChannel *ch, u16 *rtp_port, u16 *rtcp_port);



/****************************************************************************

					SDP LIBRARY EXPORTS

		  SDP is mainly a text protocol with
	well defined containers. The following structures are used to write / read
	SDP information, and the library also provides consistency checking

  When reading SDP, all text items/structures are allocated by the lib, and you
  must call gf_sdp_info_reset(GF_SDPInfo *sdp) or gf_sdp_info_del(GF_SDPInfo *sdp) to release the memory

  When writing the SDP from a GF_SDPInfo, the output buffer is allocated by the library,
  and you must release it yourself

  Some quick constructors are available for GF_SDPConnection and GF_SDPMedia in order to set up
  some specific parameters to their default value
****************************************************************************/

/*! Extension Attribute

	All attributes x-ZZZZ are considered as extensions attributes. If no "x-" is found
	the attributes in the RTSP response is SKIPPED. The "x-" radical is removed in the structure
	when parsing commands / responses
*/
typedef struct
{
	char *Name;
	char *Value;
} GF_X_Attribute;


/*! SDP bandwidth info*/
typedef struct
{
	/*"CT", "AS" are defined. Private extensions must be "X-*" ( * "are recommended to be short")*/
	char *name;
	/*in kBitsPerSec*/
	u32 value;
} GF_SDPBandwidth;

/*! SDP maximum time offset
We do not support more than this offset / zone adjustment
if more are needed, RFC recommends to use several entries rather than a big offset*/
#define GF_SDP_MAX_TIMEOFFSET	10

/*! SDP Timing information*/
typedef struct
{
	/*NPT time in sec*/
	u32 StartTime;
	/*if 0, session is unbound. NPT time in sec*/
	u32 StopTime;
	/*if 0 session is not repeated. Expressed in sec.
	Session is signaled repeated every repeatInterval*/
	u32 RepeatInterval;
	/*active duration of the session in sec*/
	u32 ActiveDuration;

	/*time offsets to use with repeat. Specify a non-regular repeat time from the Start time*/
	u32 OffsetFromStart[GF_SDP_MAX_TIMEOFFSET];
	/*Number of offsets*/
	u32 NbRepeatOffsets;

	/*EX of repeat:
	a session happens 3 times a week, on mon 1PM, thu 3PM and fri 10AM
	1- StartTime should be NPT for the session on the very first monday, StopTime
	the end of this session
	2- the repeatInterval should be 1 week, ActiveDuration the length of the session
	3- 3 offsets: 0 (for monday) (3*24+2)*3600 for thu and (4*24-3) for fri
	*/


	/*timezone adjustments, to cope with #timezones, daylight saving countries and co ...
	Ex: adjTime = [2882844526 2898848070] adjOffset=[-1h 0]
	[0]: at 2882844526 the time base by which the session's repeat times are calculated
	is shifted back by 1 hour
	[1]: at time 2898848070 the session's original time base is restored
	*/

	/*Adjustment time at which the corresponding time offset is to be applied to the
	session time line (time used to compute the "repeat session").
	All Expressed in NPT*/
	u32 AdjustmentTime[GF_SDP_MAX_TIMEOFFSET];
	/* Offset with the session time line, ALWAYS ABSOLUTE OFFSET TO the specified StartTime*/
	s32 AdjustmentOffset[GF_SDP_MAX_TIMEOFFSET];
	/*Number of offsets.*/
	u32 NbZoneOffsets;
} GF_SDPTiming;

/*! SDP Connection information*/
typedef struct
{
	/*only "IN" currently defined*/
	char *net_type;
	/*"IP4","IP6"*/
	char *add_type;
	/*hex IPv6 address or doted IPv4 address*/
	char *host;
	/*TTL - MUST BE PRESENT if IP is multicast - -1 otherwise*/
	s32 TTL;
	/*multiple address counts - ONLY in media descriptions if needed. This
	is used for content scaling, when # quality of the same media are multicasted on
	# IP addresses*/
	u32 add_count;
} GF_SDPConnection;

/*! SDP FormatTypePayload

	description of dynamic payload types. This is opaque at the SDP level.
	Each attributes is assumed to be formatted as <param_name=param_val; ...>
	If not the case the attribute will have an empty value string and only the
	parameter name.
*/
typedef struct
{
	/*payload type of the format described*/
	u32 PayloadType;
	/*list of GF_X_Attribute elements. The Value field may be NULL*/
	GF_List *Attributes;
} GF_SDP_FMTP;

/*! SDP Media information*/
typedef struct
{
	/*m=
	0: application - 1:video - 2: audio - 3: text - 4:data - 5: control*/
	u32 Type;
	/*Port Number - For transports based on UDP, the value should be in the range 1024
	to 65535 inclusive. For RTP compliance it should be an even number*/
	u32 PortNumber;
	/*number of ports described. If >= 2, the next media(s) in the SDP will be configured
	to use the next tuple (for RTP). If 0 or 1, ignored
	\note This is used for scalable media: PortNumber indicates the port of the base
	media and NumPorts the ports||total number of the upper layers*/
	u32 NumPorts;
	/*currently only "RTP/AVP" and "udp" defined*/
	char *Profile;

	/*list of GF_SDPConnection's. A media can have several connection in case of scalable content*/
	GF_List *Connections;

	/*RTPMaps contains a list SDPRTPMaps*/
	GF_List *RTPMaps;

	/*FMTP contains a list of FMTP structures*/
	GF_List *FMTP;

	/*for RTP this is PayloadType, but can be opaque (string) depending on the app.
	Formatted as XX WW QQ FF
	When reading the SDP, the payloads defined in RTPMap are removed from this list
	When writing the SDP for RTP, you should only specify static payload types here,
	as dynamic ones are stored in RTPMaps and automatically written*/
	char *fmt_list;

	/*all attributes not defined in RFC 2327 for the media*/
	GF_List *Attributes;

	/*Other SDP attributes for media desc*/

	/*k=
	method is 'clear' (key follows), 'base64' (key in base64), 'uri' (key is the URI)
	or 'prompt' (key not included)*/
	char *k_method, *k_key;

	GF_List *Bandwidths;

	/*0 if not present*/
	u32 PacketTime;
	/*0: none - 1: recv, 2: send, 3 both*/
	u32 SendReceive;
	char *orientation, *sdplang, *lang;
	/*for video only, 0.0 if not present*/
	Double FrameRate;
	/*between 0 and 10, -1 if not present*/
	s32 Quality;
} GF_SDPMedia;

/*! SDP information*/
typedef struct
{
	/*v=*/
	u32 Version;
	/*o=*/
	char *o_username, *o_session_id, *o_version, *o_address;
	/*"IN" for Net, "IP4" or "IP6" for address are currently valid*/
	char *o_net_type, *o_add_type;

	/*s=*/
	char *s_session_name;
	/*i=*/
	char *i_description;
	/*u=*/
	char *u_uri;
	/*e=*/
	char *e_email;
	/*p=*/
	char *p_phone;
	/*c= either 1 or 0 GF_SDPConnection */
	GF_SDPConnection *c_connection;
	/*b=*/
	GF_List *b_bandwidth;
	/*All time info (t, r, z)*/
	GF_List *Timing;
	/*k=
	method is 'clear' (key follows), 'base64' (key in base64), 'uri' (key is the URI)
	or 'prompt' (key not included)*/
	char *k_method, *k_key;
	/*all possible attributes (a=), session level*/
	char *a_cat, *a_keywds, *a_tool;
	/*0: none, 1: recv, 2: send, 3 both*/
	u32 a_SendReceive;
	/*should be `broadcast', `meeting', `moderated', `test' or `H332'*/
	char *a_type;
	char *a_charset;
	char *a_sdplang, *a_lang;

	/*all attributes not defined in RFC 2327 for the presentation*/
	GF_List *Attributes;

	/*list of media in the SDP*/
	GF_List *media_desc;
} GF_SDPInfo;


/*
*/
/*! creates a new SDP info
\return the newly allocated SDP
*/
GF_SDPInfo *gf_sdp_info_new();
/*! destrucs an SDP info
  Memory Consideration: the destructors free all non-NULL string. You should therefore
  be careful while (de-)assigning the strings. The function gf_sdp_info_parse() performs a complete
  reset of the GF_SDPInfo

\param sdp the target SDP to destroy
*/
void gf_sdp_info_del(GF_SDPInfo *sdp);
/*! resets all structures and destroys substructures too
\param sdp the target SDP to destroy
*/
void gf_sdp_info_reset(GF_SDPInfo *sdp);
/*! parses a memory SDP buffer
\param sdp the target SDP to destroy
\param sdp_text the encoded SDP buffer
\param text_size sizeo if the encoded SDP buffer
\return error if any
*/
GF_Err gf_sdp_info_parse(GF_SDPInfo *sdp, char *sdp_text, u32 text_size);


/*! creates a new SDP media
\return the newly allocated SDP media
*/
GF_SDPMedia *gf_sdp_media_new();
/*! destroys an SDP media
\param media the target SDP media
*/
void gf_sdp_media_del(GF_SDPMedia *media);

/*! creates a new SDP connection
\return the newly allocated SDP connection
*/
GF_SDPConnection *gf_sdp_conn_new();
/*! destroys an SDP connection
\param conn the target SDP connection
*/
void gf_sdp_conn_del(GF_SDPConnection *conn);

/*! creates a new SDP payload
\return the newly allocated SDP payload
*/
GF_SDP_FMTP *gf_sdp_fmtp_new();
/*! destroys an SDP payload
\param fmtp the target SDP payload
*/
void gf_sdp_fmtp_del(GF_SDP_FMTP *fmtp);



/*
	RTP packetizer
*/

/*! Mapping between RTP and GPAC / MPEG-4 Systems SyncLayer */
typedef struct
{
	/*1 - required options*/

	/*mode, or "" if no mode ("generic" should be used instead)*/
	char mode[30];

	/*config of the stream if carried in SDP*/
	u8 *config;
	u32 configSize;
	u8 config_updated;
	/* Stream Type*/
	u8 StreamType;
	/* stream profile and level indication - for AVC/H264, 0xPPCCLL, with PP:profile, CC:compatibility, LL:level*/
	u32 PL_ID;

	/*rvc config of the stream if carried in SDP*/
	u16 rvc_predef;
	u8 *rvc_config;
	u32 rvc_config_size;

	/*2 - optional options*/

	/*size of AUs if constant*/
	u32 ConstantSize;
	/*duration of AUs if constant, in RTP timescale*/
	u32 ConstantDuration;

	/* CodecID */
	u32 CodecID;
	/*audio max displacement when interleaving (eg, de-interleaving window buffer max length) in RTP timescale*/
	u32 maxDisplacement;
	/*de-interleaveBufferSize if not recomputable from maxDisplacement*/
	u32 deinterleaveBufferSize;

	/*The number of bits on which the AU-size field is encoded in the AU-header*/
	u32 SizeLength;
	/*The number of bits on which the AU-Index is encoded in the first AU-header*/
	u32 IndexLength;
	/*The number of bits on which the AU-Index-delta field is encoded in any non-first AU-header*/
	u32 IndexDeltaLength;

	/*The number of bits on which the DTS-delta field is encoded in the AU-header*/
	u32 DTSDeltaLength;
	/*The number of bits on which the CTS-delta field is encoded in the AU-header*/
	u32 CTSDeltaLength;
	/*random access point flag present*/
	Bool RandomAccessIndication;

	/*The number of bits on which the Stream-state field is encoded in the AU-header (systems only)*/
	u32 StreamStateIndication;
	/*The number of bits that is used to encode the auxiliary-data-size field
	(no normative usage of this section)*/
	u32 AuxiliaryDataSizeLength;

	/*ISMACryp stuff*/
	u8 IV_length, IV_delta_length;
	u8 KI_length;

	/*internal stuff*/
	/*len of first AU header in an RTP payload*/
	u32 auh_first_min_len;
	/*len of non-first AU header in an RTP payload*/
	u32 auh_min_len;
} GP_RTPSLMap;


/*! packetizer config flags - some flags are dynamically re-assigned when detecting multiSL / B-Frames / ...*/
enum
{
	/*forces MPEG-4 generic transport if MPEG-4 systems mapping is available*/
	GP_RTP_PCK_FORCE_MPEG4 =	(1),
	/*Enables AUs concatenation in an RTP packet (if payload supports it) - this forces GP_RTP_PCK_SIGNAL_SIZE for MPEG-4*/
	GP_RTP_PCK_USE_MULTI	=	(1<<1),
	/*if set, audio interleaving is used if payload supports it (forces GP_RTP_PCK_USE_MULTI flag)
		THIS IS CURRENTLY NOT IMPLEMENTED*/
	GP_RTP_PCK_USE_INTERLEAVING =	(1<<2),
	/*uses static RTP payloadID if any defined*/
	GP_RTP_PCK_USE_STATIC_ID =	(1<<3),

	/*MPEG-4 generic transport option*/
	/*if flag set, RAP flag is signaled in RTP payload*/
	GP_RTP_PCK_SIGNAL_RAP	=	(1<<4),
	/*if flag set, AU indexes are signaled in RTP payload - only usable for AU interleaving (eg audio)*/
	GP_RTP_PCK_SIGNAL_AU_IDX	=	(1<<5),
	/*if flag set, AU size is signaled in RTP payload*/
	GP_RTP_PCK_SIGNAL_SIZE	=	(1<<6),
	/*if flag set, CTS is signaled in RTP payload - DTS is automatically set if needed*/
	GP_RTP_PCK_SIGNAL_TS	=	(1<<7),

	/*setup payload for carouseling of systems streams*/
	GP_RTP_PCK_SYSTEMS_CAROUSEL = (1<<8),

	/*use LATM payload for AAC-LC*/
	GP_RTP_PCK_USE_LATM_AAC	=	(1<<9),

	/*ISMACryp options*/
	/*signals that input data is selectively encrypted (eg not all input frames are encrypted)
	- this is usually automatically set by hinter*/
	GP_RTP_PCK_SELECTIVE_ENCRYPTION =	(1<<10),
	/*signals that each sample will have its own key indicator - ignored in non-multi modes
	if not set and key indicator changes, a new RTP packet will be forced*/
	GP_RTP_PCK_KEY_IDX_PER_AU =	(1<<11),

	/*is zip compression used in DIMS unit ?*/
	GP_RTP_DIMS_COMPRESSED =	(1<<12),
};



/*
		Generic packetization tools - used by track hinters and future live tools
*/

/*! Supported payload types*/
enum
{
	/*assigned payload types*/
	/*cf RFC 3551*/
	GF_RTP_PAYT_PCMU = 0,
	/*cf RFC 3551*/
	GF_RTP_PAYT_GSM,
	/*cf RFC 3551*/
	GF_RTP_PAYT_G723,
	/*cf RFC 3551*/
	GF_RTP_PAYT_DVI4_8K,
	/*cf RFC 3551*/
	GF_RTP_PAYT_DVI4_16K,
	/*cf RFC 3551*/
	GF_RTP_PAYT_LPC,
	/*cf RFC 3551*/
	GF_RTP_PAYT_PCMA,
	/*cf RFC 3551*/
	GF_RTP_PAYT_G722,
	/*cf RFC 3551*/
	GF_RTP_PAYT_L16_STEREO,
	/*cf RFC 3551*/
	GF_RTP_PAYT_L16_MONO,
	/*cf RFC 3551*/
	GF_RTP_PAYT_QCELP_BASIC,
	/*cf RFC 3389*/
	GF_RTP_PAYT_CN,
	/*use generic MPEG-1/2 audio transport - RFC 2250*/
	GF_RTP_PAYT_MPEG12_AUDIO,
	/*cf RFC 3551*/
	GF_RTP_PAYT_G728,
	GF_RTP_PAYT_DVI4_11K,
	GF_RTP_PAYT_DVI4_22K,
	/*cf RFC 3551*/
	GF_RTP_PAYT_G729,
	/*cf RFC 2029*/
	GF_RTP_PAYT_CelB = 25,
	/*cf RFC 2435*/
	GF_RTP_PAYT_JPEG = 26,
	/*cf RFC 3551*/
	GF_RTP_PAYT_nv = 28,
	/*cf RFC 4587*/
	GF_RTP_PAYT_H261 = 31,
	/* generic MPEG-1/2 video transport - RFC 2250*/
	GF_RTP_PAYT_MPEG12_VIDEO = 32,
	/*MPEG-2 TS - RFC 2250*/
	GF_RTP_PAYT_MP2T = 33,
	/*use H263 transport - RFC 2429*/
	GF_RTP_PAYT_H263 = 34,

	GF_RTP_PAYT_LAST_STATIC_DEFINED = 35,

	/*not defined*/
	GF_RTP_PAYT_UNKNOWN = 128,

	/*internal types for all dynamic payloads*/

	/*use generic MPEG-4 transport - RFC 3016 and RFC 3640*/
	GF_RTP_PAYT_MPEG4,
	/*use AMR transport - RFC 3267*/
	GF_RTP_PAYT_AMR,
	/*use AMR-WB transport - RFC 3267*/
	GF_RTP_PAYT_AMR_WB,
	/*use QCELP transport - RFC 2658*/
	GF_RTP_PAYT_QCELP,
	/*use EVRC/SMV transport - RFC 3558*/
	GF_RTP_PAYT_EVRC_SMV,
	/*use 3GPP Text transport - no RFC yet, only draft*/
	GF_RTP_PAYT_3GPP_TEXT,
	/*use H264 transport - no RFC yet, only draft*/
	GF_RTP_PAYT_H264_AVC,
	/*use LATM for AAC-LC*/
	GF_RTP_PAYT_LATM,
	/*use AC3 audio format*/
	GF_RTP_PAYT_AC3,
	/*use EAC3 audio format*/
	GF_RTP_PAYT_EAC3,
	/*use H264-SVC transport*/
	GF_RTP_PAYT_H264_SVC,
	/*use HEVC/H265 transport (RFC 7798)*/
	GF_RTP_PAYT_HEVC,
	GF_RTP_PAYT_LHVC,
#if GPAC_ENABLE_3GPP_DIMS_RTP
	/*use 3GPP DIMS format*/
	GF_RTP_PAYT_3GPP_DIMS,
#endif
	/*use VVC transport (no RFC yet)*/
	GF_RTP_PAYT_VVC,
	/*use opus audio format*/
	GF_RTP_PAYT_OPUS,
};



/*
	RTP packetizer
*/


/*! RTP builder (packetizer)*/
typedef struct __tag_rtp_packetizer GP_RTPPacketizer;

/*! creates a new builder
\param rtp_payt rtp payload format, one of the above GF_RTP_PAYT_*
\param slc user-given SL config to use. If none specified, default RFC config is used
\param flags packetizer flags, one of the above GP_RTP_PCK_*
\param cbk_obj callback object passed back in functions
\param OnNewPacket callback function starting new RTP packet
		header: rtp header for new packet - note that RTP header flags are not used until PacketDone is called
\param OnPacketDone callback function closing current RTP packet
		header: final rtp header for packet
\param OnDataReference optional, to call each time data from input buffer is added to current RTP packet.
		If not set, data must be added through OnData
		payload_size: size of reference data
		offset_from_orig: start offset in input buffer
\param OnData to call each time data is added to current RTP packet (either extra data from payload or
		data from input when not using referencing)
		is_head: signal the data added MUST be inserted at the beginning of the payload. Otherwise data is concatenated as received
\return a new RTP packetizer object
*/
GP_RTPPacketizer *gf_rtp_builder_new(u32 rtp_payt,
                                     GF_SLConfig *slc,
                                     u32 flags,
                                     void *cbk_obj,
                                     void (*OnNewPacket)(void *cbk, GF_RTPHeader *header),
                                     void (*OnPacketDone)(void *cbk, GF_RTPHeader *header),
                                     void (*OnDataReference)(void *cbk, u32 payload_size, u32 offset_from_orig),
                                     void (*OnData)(void *cbk, u8 *data, u32 data_size, Bool is_head)
                                    );

/*! destroys an RTP packetizer
\param builder the target RTP packetizer
*/
void gf_rtp_builder_del(GP_RTPPacketizer *builder);

/*! inits the RTP packetizer
\param builder the target RTP packetizer
\param PayloadType the payload type to use
\param MaxPayloadSize maximum payload size of RTP packets (eg MTU minus IP/UDP/RTP headers)
\param max_ptime maximum packet duration IN RTP TIMESCALE
\param StreamType MPEG-4 system stream type - MUST always be provided for payloads format specifying
		audio or video streams
\param codecid ID of the media codec
\param PL_ID profile and level identifier for the stream

			*** all other params are for RFC 3640 ***

\param avgSize average size of an AU. This is not always known (real-time encoding).
In this case you should specify a rough compute indicating how many packets could be
stored per RTP packet. for ex AAC stereo at 44100 k / 64kbps , one AU ~= 380 bytes
so 3 AUs for 1500 MTU is ok - BE CAREFUL: MultiSL adds some SL info on top of the 12
byte RTP header so you should specify a smaller size
The packetizer will ALWAYS make sure there's no pb storing the packets so specifying
more will result in a slight overhead in the SL mapping but the gain to singleSL
will still be worth it.
	-Nota: at init, the packetizer can decide to switch to SingleSL if the average size
specified is too close to the PathMTU

\param maxSize max size of an AU. If unknown (real-time) set to 0
\param avgTS average CTS progression (1000/FPS for video)
\param maxDTS maximum DTS offset in case of bidirectional coding.
\param IV_length size (in bytes) of IV when ISMACrypted
\param KI_length size (in bytes) of key indicator when ISMACrypted
\param pref_mode MPEG-4 generic only, specifies the payload mode - can be NULL (mode generic)
*/
void gf_rtp_builder_init(GP_RTPPacketizer *builder, u8 PayloadType, u32 MaxPayloadSize, u32 max_ptime,
                         u32 StreamType, u32 codecid, u32 PL_ID,
                         u32 avgSize, u32 maxSize,
                         u32 avgTS, u32 maxDTS,
                         u32 IV_length, u32 KI_length,
                         char *pref_mode);

/*! sets frame crypto info (ISMA E&A) for frame starting in next RTP packet
\param builder the target RTP packetizer
\param IV initialization vector
\param key_indicator key indicator
\param is_encrypted encrypted flag
*/
void gf_rtp_builder_set_cryp_info(GP_RTPPacketizer *builder, u64 IV, char *key_indicator, Bool is_encrypted);
/*! packetizes input buffer
\param builder the target RTP packetizer
\param data input buffer
\param data_size input buffer size
\param IsAUEnd set to one if this buffer is the last of the AU
\param FullAUSize complete access unit size if known, 0 otherwise
\param duration sample duration in rtp timescale (mostly needed for 3GPP text streams)
\param descIndex sample description index (mostly needed for 3GPP text streams)
\return error if any
*/
GF_Err gf_rtp_builder_process(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize, u32 duration, u8 descIndex);

/*! formats the "fmtp: " attribute for the MPEG-4 generic packetizer. sdpline shall be at least 2000 char
\param builder the target RTP packetizer
\param payload_name name of the payload to use (profile of RFC 3640)
\param sdp_line SDP line buffer to fill
\param dsi decoder config of stream if any, or NULL
\param dsi_size size of the decoder config
\return error if any
*/
GF_Err gf_rtp_builder_format_sdp(GP_RTPPacketizer *builder, char *payload_name, char *sdp_line, char *dsi, u32 dsi_size);
/*! formats SDP payload name and media name
\param builder the target RTP packetizer
\param payload_name the buffer to fill with the payload name
\param media_name the buffer to fill with the payload name
\return GF_TRUE if success, GF_FALSE otherwise
*/
Bool gf_rtp_builder_get_payload_name(GP_RTPPacketizer *builder, char payload_name[20], char media_name[20]);


/*! rtp payload flags*/
enum
{
	/*AU end was detected (eg next packet is AU start)*/
	GF_RTP_NEW_AU = (1),
	/*AMR config*/
	GF_RTP_AMR_ALIGN = (1<<1),
	/*for RFC3016, signals bitstream inspection for RAP discovery*/
	GF_RTP_M4V_CHECK_RAP = (1<<2),

	/*AWFULL hack at rtp level to cope with ffmpeg h264 crashes when jumping in stream without IDR*/
	GF_RTP_AVC_WAIT_RAP = (1<<4),
	/*ISMACryp stuff*/
	GF_RTP_HAS_ISMACRYP = (1<<5),
	GF_RTP_ISMA_SEL_ENC = (1<<6),
	GF_RTP_ISMA_HAS_KEY_IDX = (1<<7),

	GF_RTP_AVC_USE_ANNEX_B = (1<<8)
};

/*! Static RTP map definition, for static payload types*/
typedef struct rtp_static_payt {
	u32 fmt;
	u32 clock_rate;
	u32 stream_type;
	u32 codec_id;
	const char *mime;
} GF_RTPStaticMap;


/*! RTP depacketizer callback
\param udta opaque user data
\param payload depacketized payload data
\param size payload size
\param hdr MPEG-4 Sync Layer structure for the payload (with AU start/end flag, timestamps, etc)
\param e error if any
*/
typedef void (*gf_rtp_packet_cbk)(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e);



/*! RTP parser (depacketizer)*/
typedef struct __tag_rtp_depacketizer GF_RTPDepacketizer;

/*! creates a new depacketizer
\param media the SDP media structure describing the RTP stream - can be NULL for static payload types
\param hdr_payt the static RTP payload type when no SDP is used
\param sl_packet_cbk callback function of the depacketizer to retrieves payload
\param udta opaque data for the callback function
\return a newly allocated RTP depacketizer, or NULL if not supported*/
GF_RTPDepacketizer *gf_rtp_depacketizer_new(GF_SDPMedia *media, u32 hdr_payt, gf_rtp_packet_cbk sl_packet_cbk, void *udta);
/*! destroys an RTP depacketizer
\param rtpd the target RTP depacketizer
*/
void gf_rtp_depacketizer_del(GF_RTPDepacketizer *rtpd);
/*! resets an RTP depacketizer, assuming next packet should be an AU start
\param rtpd the target RTP depacketizer
\param full_reset if set, completely reset the SL header
*/
void gf_rtp_depacketizer_reset(GF_RTPDepacketizer *rtpd, Bool full_reset);
/*! process an RTP depacketizer
\param rtpd the target RTP depacketizer
\param hdr the RTP packet header
\param payload the RTP packet payload
\param size the RTP packet payload size
*/
void gf_rtp_depacketizer_process(GF_RTPDepacketizer *rtpd, GF_RTPHeader *hdr, u8 *payload, u32 size);

#endif /*GPAC_DISABLE_STREAMING*/

/*! @} */

#ifdef __cplusplus
}
#endif

#endif		/*_GF_IETF_H_*/

