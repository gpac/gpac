/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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

#include <gpac/internal/ietf_dev.h>
#include <gpac/base_coding.h>


#ifndef GPAC_DISABLE_STREAMING

static GF_Err gf_rtsp_write_sock(GF_RTSPSession *sess, u8 *data, u32 len);
static GF_Err gf_rstp_flush_buffer(GF_RTSPSession *sess);
static GF_Err gf_rtsp_http_tunnel_setup(GF_RTSPSession *sess);

/*default packet size to use when storing incomplete packets*/
#define RTSP_PCK_SIZE			1000

GF_Err RTSP_UnpackURL(char *sURL, char *Server, u16 *Port, char *Service, Bool *useTCP)
{
	char schema[10], *test, text[1024], *retest, *sep;
	u32 i, len;
	Bool is_ipv6;
	if (!sURL) return GF_BAD_PARAM;

	strcpy(Server, "");
	strcpy(Service, "");
	*Port = 0;
	*useTCP = GF_FALSE;

	if (!strchr(sURL, ':')) return GF_BAD_PARAM;

	sep = strchr(sURL, '?');
	if (sep) sep[0] = 0;
	//extract the schema
	i = 0;
	while (i<=strlen(sURL)) {
		if (sURL[i] == ':') goto found;
		schema[i] = sURL[i];
		i += 1;
	}
	if (sep) sep[0] = '?';
	return GF_BAD_PARAM;

found:
	schema[i] = 0;
	if (stricmp(schema, "rtsp") && stricmp(schema, "rtspu") && stricmp(schema, "rtsph") && stricmp(schema, "satip")) return GF_URL_ERROR;
	//check for user/pass - not allowed
	/*
		test = strstr(sURL, "@");
		if (test) return GF_NOT_SUPPORTED;
	*/
	test = strstr(sURL, "://");
	if (!test) {
		if (sep) sep[0] = '?';
		return GF_URL_ERROR;
	}
	test += 3;
	//check for service
	retest = strstr(test, "/");
	if (!retest) {
		if (sep) sep[0] = '?';
		return GF_URL_ERROR;
	}
	if (!stricmp(schema, "rtsp") || !stricmp(schema, "satip") || !stricmp(schema, "rtsph"))
		*useTCP = GF_TRUE;

	//check for port
	retest = strrchr(test, ':');
	/*IPV6 address*/
	if (retest && strchr(retest, ']')) retest = NULL;

	if (retest && strstr(retest, "/")) {
		retest += 1;
		i=0;
		while (i<strlen(retest)) {
			if (retest[i] == '/') break;
			text[i] = retest[i];
			i += 1;
		}
		text[i] = 0;
		*Port = atoi(text);
	}

	//get the server name
	is_ipv6 = GF_FALSE;
	len = (u32) strlen(test);
	i=0;
	while (i<len) {
		if (test[i]=='[') is_ipv6 = GF_TRUE;
		else if (test[i]==']') is_ipv6 = GF_FALSE;
		if ( (test[i] == '/') || (!is_ipv6 && (test[i] == ':')) ) break;
		text[i] = test[i];
		i += 1;
	}
	text[i] = 0;
	strcpy(Server, text);
	if (sep) sep[0] = '?';

	while (test[i] != '/') i += 1;
	strcpy(Service, test+i+1);

	return GF_OK;
}


//create a new GF_RTSPSession from URL - DO NOT USE WITH SDP
GF_EXPORT
GF_RTSPSession *gf_rtsp_session_new(char *sURL, u16 DefaultPort)
{
	GF_RTSPSession *sess;
	char server[1024], service[1024];
	GF_Err e;
	u16 Port;
	Bool UseTCP;

	if (!sURL) return NULL;

	e = RTSP_UnpackURL(sURL, server, &Port, service, &UseTCP);
	if (e) return NULL;

	GF_SAFEALLOC(sess, GF_RTSPSession);
	if (!sess) return NULL;
	sess->ConnectionType = UseTCP ? GF_SOCK_TYPE_TCP : GF_SOCK_TYPE_UDP;
	if (Port) sess->Port = Port;
	else if (DefaultPort) sess->Port = DefaultPort;
	else sess->Port = 554;

	//HTTP tunnel
	if ((sess->Port == 80) || (sess->Port == 8080) || !strnicmp(sURL, "rtsph://", 8)){
		sess->ConnectionType = GF_SOCK_TYPE_TCP;
		sess->tunnel_mode = RTSP_HTTP_CLIENT;
	}

	gf_rtsp_set_buffer_size(sess, RTSP_PCK_SIZE);

	sess->Server = gf_strdup(server);
	sess->Service = gf_strdup(service);
	sess->TCPChannels = gf_list_new();
	gf_rtsp_session_reset(sess, GF_FALSE);
	return sess;
}


GF_EXPORT
void gf_rtsp_reset_aggregation(GF_RTSPSession *sess)
{
	if (!sess) return;

	if (sess->RTSP_State == GF_RTSP_STATE_WAIT_FOR_CONTROL) {
		strcpy(sess->RTSPLastRequest, "RESET");
		//skip all we haven't received
		sess->CSeq += sess->NbPending;
		sess->NbPending = 0;
	}
	sess->RTSP_State = GF_RTSP_STATE_INIT;
}

void RemoveTCPChannels(GF_RTSPSession *sess)
{
	while (gf_list_count(sess->TCPChannels)) {
		GF_TCPChan *ch = (GF_TCPChan*)gf_list_get(sess->TCPChannels, 0);
		gf_free(ch);
		gf_list_rem(sess->TCPChannels, 0);
	}
}


GF_EXPORT
void gf_rtsp_session_reset(GF_RTSPSession *sess, Bool ResetConnection)
{
	sess->last_session_id = NULL;
	sess->NeedConnection = 1;

	if (ResetConnection) {
		if (sess->connection) gf_sk_del(sess->connection);
		sess->connection = NULL;
		if (sess->http) {
			gf_sk_del(sess->http);
			sess->http = NULL;
			if (sess->tunnel_mode<RTSP_HTTP_DISABLE)
				sess->tunnel_mode = 0;
			sess->tunnel_state = 0;
		}
	}

	sess->RTSP_State = GF_RTSP_STATE_INIT;
//	sess->CSeq = sess->NbPending = 0;
	sess->InterID = (u8) -1;
	sess->pck_start = sess->payloadSize = 0;
	sess->CurrentPos = sess->CurrentSize = 0;
	strcpy(sess->RTSPLastRequest, "");
	RemoveTCPChannels(sess);
}


GF_EXPORT
void gf_rtsp_session_del(GF_RTSPSession *sess)
{
	if (!sess) return;

	gf_rtsp_session_reset(sess, GF_FALSE);

	if (sess->connection) gf_sk_del(sess->connection);
	if (sess->http) gf_sk_del(sess->http);
	if (sess->Server) gf_free(sess->Server);
	if (sess->Service) gf_free(sess->Service);
	gf_list_del(sess->TCPChannels);
	if (sess->rtsp_pck_buf) gf_free(sess->rtsp_pck_buf);
	gf_free(sess->tcp_buffer);
	if (sess->HTTP_Cookie) gf_free(sess->HTTP_Cookie);
	if (sess->async_buf) gf_free(sess->async_buf);

	gf_free(sess);
}

GF_EXPORT
u32 gf_rtsp_get_session_state(GF_RTSPSession *sess)
{
	u32 state;
	if (!sess) return GF_RTSP_STATE_INVALIDATED;

	state = sess->RTSP_State;
	return state;
}

#if 0 //unused
char *gf_rtsp_get_last_request(GF_RTSPSession *sess)
{
	if (!sess) return NULL;
	return sess->RTSPLastRequest;
}
#endif


//check whether the url contains server and service name
//no thread protection as this is const throughout the session
GF_EXPORT
Bool gf_rtsp_is_my_session(GF_RTSPSession *sess, char *url)
{
	if (!sess) return GF_FALSE;
	if (!strstr(url, sess->Server)) return GF_FALSE;
	//same url or sub-url
	if (strstr(url, sess->Service)) return GF_TRUE;
	return GF_FALSE;
}

#if 0 //unused
const char *gf_rtsp_get_last_session_id(GF_RTSPSession *sess)
{
	if (!sess) return NULL;
	return sess->last_session_id;
}
#endif

GF_EXPORT
char *gf_rtsp_get_server_name(GF_RTSPSession *sess)
{
	if (!sess) return NULL;
	return sess->Server;
}

GF_EXPORT
u16 gf_rtsp_get_session_port(GF_RTSPSession *sess)
{
	return (sess ? sess->Port : 0);
}

GF_Err gf_rtsp_check_connection(GF_RTSPSession *sess)
{
	GF_Err e;
	if (!sess) return GF_BAD_PARAM;
	//active, return
	if (!sess->NeedConnection) {
		if (sess->tunnel_state) return gf_rtsp_http_tunnel_setup(sess);
		return gf_rstp_flush_buffer(sess);
	}

	//socket is destroyed, recreate
	if (!sess->connection) {
		sess->connection = gf_sk_new(sess->ConnectionType);
		if (!sess->connection) return GF_OUT_OF_MEM;

		//work in non-blocking mode
		gf_sk_set_block_mode(sess->connection, GF_TRUE);
		sess->timeout_in = gf_opts_get_int("core", "tcp-timeout");
		if (!sess->timeout_in) sess->timeout_in = 5000;
		sess->timeout_in += gf_sys_clock();
	}
	//the session is down, reconnect
	e = gf_sk_connect(sess->connection, sess->Server, sess->Port, NULL);
	if (e) {
		if (e==GF_IP_NETWORK_EMPTY) {
			if (sess->timeout_in < gf_sys_clock())
				e = GF_IP_CONNECTION_FAILURE;
		}
		return e;
	}

	if (sess->SockBufferSize) gf_sk_set_buffer_size(sess->connection, GF_FALSE, sess->SockBufferSize);

	sess->NeedConnection = 0;
	if (!sess->http && (sess->tunnel_mode==RTSP_HTTP_CLIENT)) {
		e = gf_rtsp_http_tunnel_setup(sess);
		if (e) return e;
	}
	return gf_rstp_flush_buffer(sess);
}


GF_Err gf_rtsp_send_data(GF_RTSPSession *sess, u8 *buffer, u32 Size)
{
	GF_Err e;
	u32 Size64;

	e = gf_rtsp_check_connection(sess);
	if (e) return e;

	//RTSP requests on HTTP are base 64 encoded
	if (sess->tunnel_mode==RTSP_HTTP_CLIENT) {
		char *buf64 = gf_malloc(sizeof(char)*Size*2);
		if (!buf64) return GF_OUT_OF_MEM;
		Size64 = gf_base64_encode(buffer, Size, buf64, Size*2);
		//send on http connection
		e = gf_rtsp_write_sock(sess, buf64, Size64);
		gf_free(buf64);
		return e;
	} else {
		return gf_rtsp_write_sock(sess, buffer, Size);
	}
}



static GF_TCPChan *GetTCPChannel(GF_RTSPSession *sess, u8 rtpID, u8 rtcpID, Bool RemoveIt)
{
	GF_TCPChan *ptr;
	u32 i, count = gf_list_count(sess->TCPChannels);
	for (i=0; i<count; i++) {
		ptr = (GF_TCPChan *)gf_list_get(sess->TCPChannels, i);
		if (ptr->rtpID == rtpID) goto exit;
		if (ptr->rtcpID == rtcpID) goto exit;
	}
	return NULL;
exit:
	if (RemoveIt) gf_list_rem(sess->TCPChannels, i);
	return ptr;
}


GF_Err gf_rtsp_do_deinterleave(GF_RTSPSession *sess)
{
	GF_TCPChan *ch;
	Bool IsRTCP;
	u8 InterID;
	u16 paySize;
	u32 res, Size;
	char *buffer;

	if (!sess) return GF_SERVICE_ERROR;

	Size = sess->CurrentSize - sess->CurrentPos;
	buffer = sess->tcp_buffer + sess->CurrentPos;

	//we do not work with just a header -> force a refill
	if (!Size)
		return gf_rtsp_fill_buffer(sess);
	if (Size <= 4)
		return gf_rtsp_refill_buffer(sess);

	//break if we get RTSP response on the wire
	if (!strncmp(buffer, "RTSP", 4))
		return GF_EOS;

	//new packet
	if (!sess->pck_start && (buffer[0] == '$')) {
		InterID = buffer[1];
		paySize = ((buffer[2] << 8) & 0xFF00) | (buffer[3] & 0xFF);
		/*this may be NULL (data fetched after a teardown) - resync and return*/
		ch = GetTCPChannel(sess, InterID, InterID, GF_FALSE);

		/*then check whether this is a full packet or a split*/
		if (paySize <= Size-4) {
			if (ch) {
				IsRTCP = (ch->rtcpID == InterID) ? GF_TRUE : GF_FALSE;
				sess->RTSP_SignalData(sess, ch->ch_ptr, buffer+4, paySize, IsRTCP);
			}
			sess->CurrentPos += paySize+4;
			assert(sess->CurrentPos <= sess->CurrentSize);
		} else {
			/*missed end of pck ?*/
			if (sess->payloadSize) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP over RTSP] Missed end of packet (%d bytes) in stream %d\n", sess->payloadSize - sess->pck_start, sess->InterID));
				ch = GetTCPChannel(sess, sess->InterID, sess->InterID, GF_FALSE);
				if (ch) {
					IsRTCP = (ch->rtcpID == sess->InterID) ? GF_TRUE : GF_FALSE;
					sess->RTSP_SignalData(sess, ch->ch_ptr, sess->rtsp_pck_buf, sess->payloadSize, IsRTCP);
				}
			}
			sess->InterID = InterID;
			sess->payloadSize = paySize;
			sess->pck_start = Size-4;
			if (sess->rtsp_pck_size < paySize) {
				sess->rtsp_pck_buf = (char *)gf_realloc(sess->rtsp_pck_buf, sizeof(char)*paySize);
				sess->rtsp_pck_size = paySize;
			}
			memcpy(sess->rtsp_pck_buf, buffer+4, Size-4);
			sess->CurrentPos += Size;
			assert(sess->CurrentPos <= sess->CurrentSize);
		}
	}
	/*end of packet*/
	else if (sess->payloadSize - sess->pck_start <= Size) {
//		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP over RTSP] Missed beginning of packet (%d bytes) in stream %d\n", Size, sess->InterID));

		res = sess->payloadSize - sess->pck_start;
		memcpy(sess->rtsp_pck_buf + sess->pck_start, buffer, res);
		//flush - same as above, don't complain if channel not found
		ch = GetTCPChannel(sess, sess->InterID, sess->InterID, GF_FALSE);
		if (ch) {
			IsRTCP = (ch->rtcpID == sess->InterID) ? GF_TRUE : GF_FALSE;
			sess->RTSP_SignalData(sess, ch->ch_ptr, sess->rtsp_pck_buf, sess->payloadSize, IsRTCP);
		}
		sess->payloadSize = 0;
		sess->pck_start = 0;
		sess->InterID = (u8) -1;
		sess->CurrentPos += res;
		assert(sess->CurrentPos <= sess->CurrentSize);
	}
	/*middle of packet*/
	else {
//		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP over RTSP] Missed beginning of RTP packet in stream %d\n", sess->InterID));
		memcpy(sess->rtsp_pck_buf + sess->pck_start, buffer, Size);
		sess->pck_start += Size;
		sess->CurrentPos += Size;
		assert(sess->CurrentPos <= sess->CurrentSize);
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_rtsp_session_read(GF_RTSPSession *sess)
{
	GF_Err e;
	if (!sess) return GF_BAD_PARAM;

	e = gf_rtsp_fill_buffer(sess);
	if (!e) {
		//read as much as possible (will break if no data or if "RTSP" is found)
		while (1) {
			e = gf_rtsp_do_deinterleave(sess);
			if (e) return e;
		}
	}
	return e;
}


GF_EXPORT
u32 gf_rtsp_unregister_interleave(GF_RTSPSession *sess, u8 LowInterID)
{
	u32 res;
	GF_TCPChan *ptr;
	if (!sess) return 0;

	ptr = GetTCPChannel(sess, LowInterID, LowInterID, GF_TRUE);
	if (ptr) gf_free(ptr);
	res = gf_list_count(sess->TCPChannels);
	if (!res) sess->interleaved = GF_FALSE;
	return res;
}

GF_EXPORT
GF_Err gf_rtsp_register_interleave(GF_RTSPSession *sess, void *the_ch, u8 LowInterID, u8 HighInterID)
{
	GF_TCPChan *ptr;

	if (!sess) return GF_BAD_PARAM;

	//do NOT register twice
	ptr = GetTCPChannel(sess, LowInterID, HighInterID, GF_FALSE);
	if (!ptr) {
		ptr = (GF_TCPChan *)gf_malloc(sizeof(GF_TCPChan));
		ptr->ch_ptr = the_ch;
		ptr->rtpID = LowInterID;
		ptr->rtcpID = HighInterID;
		gf_list_add(sess->TCPChannels, ptr);
	}
	sess->interleaved=GF_TRUE;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_rtsp_set_interleave_callback(GF_RTSPSession *sess, gf_rtsp_interleave_callback SignalData)
{
	if (!sess) return GF_BAD_PARAM;

	//only if existing
	if (SignalData) sess->RTSP_SignalData = SignalData;

	if (!sess->rtsp_pck_buf || (sess->rtsp_pck_size != RTSP_PCK_SIZE) ) {
		if (!sess->rtsp_pck_buf)
			sess->pck_start = 0;
		sess->rtsp_pck_size = RTSP_PCK_SIZE;
		sess->rtsp_pck_buf = (char *)gf_realloc(sess->rtsp_pck_buf, sizeof(char)*sess->rtsp_pck_size);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_rtsp_set_buffer_size(GF_RTSPSession *sess, u32 BufferSize)
{
	if (!sess) return GF_BAD_PARAM;
	if (sess->SockBufferSize >= BufferSize) return GF_OK;
	sess->SockBufferSize = BufferSize;
	sess->tcp_buffer = gf_realloc(sess->tcp_buffer, (BufferSize+1));
	return GF_OK;
}


static Bool HTTP_RandInit = GF_TRUE;

#define HTTP10_RSP_OK	"HTTP/1.0 200 OK"
#define HTTP11_RSP_OK	"HTTP/1.1 200 OK"

static GF_Err gf_rtsp_http_tunnel_setup(GF_RTSPSession *sess)
{
	GF_Err e;
	u32 size;
	s32 pos;
	const char *ua;
	char buffer[GF_RTSP_DEFAULT_BUFFER];

	if (!sess) return GF_BAD_PARAM;

	if (!sess->tunnel_state) {
		//generate http cookie
		if (HTTP_RandInit) {
			gf_rand_init(GF_FALSE);
			HTTP_RandInit = GF_FALSE;
		}
		if (!sess->HTTP_Cookie) {
			char szBuf[30];
			u32 r = gf_rand();
			snprintf(szBuf, 29, "GPAC_%x_"LLX, r, gf_sys_clock_high_res() );
			szBuf[29]=0;
			sess->HTTP_Cookie = gf_strdup(szBuf);
			if (!sess->HTTP_Cookie) return GF_OUT_OF_MEM;
		}

		ua = gf_opts_get_key("core", "user-agent");
		if (!ua) ua = "GPAC " GPAC_VERSION;

		//  1. send "GET /sample.mov HTTP/1.0\r\n ..."
		memset(buffer, 0, GF_RTSP_DEFAULT_BUFFER);
		pos = 0;
		pos += sprintf(buffer + pos, "GET /%s HTTP/1.0\r\n", sess->Service);
		pos += sprintf(buffer + pos, "User-Agent: %s\r\n", ua);
		pos += sprintf(buffer + pos, "x-sessioncookie: %s\r\n", sess->HTTP_Cookie);
		pos += sprintf(buffer + pos, "Accept: application/x-rtsp-tunnelled\r\n" );
		pos += sprintf(buffer + pos, "Pragma: no-cache\r\n" );
		/*pos += */sprintf(buffer + pos, "Cache-Control: no-cache\r\n\r\n" );

		sess->tunnel_state = 1;

		//	send it - we assume this will fit the socket buffer
		e = gf_sk_send(sess->connection, buffer, (u32) strlen(buffer));
		if (e) return e;
	}

	if (sess->tunnel_state == 1) {
		//	2. wait for "HTTP/1.0 200 OK"
		e = gf_sk_receive(sess->connection, buffer, GF_RTSP_DEFAULT_BUFFER, &size);
		if (e) {
			if ((e==GF_IP_NETWORK_EMPTY) && (sess->timeout_in < gf_sys_clock()))
				e = GF_IP_CONNECTION_FAILURE;
			return e;
		}
		sess->tunnel_state = 2;

		//get HTTP/1.0 200 OK
		if (strncmp(buffer, HTTP10_RSP_OK, strlen(HTTP10_RSP_OK)) && strncmp(buffer, HTTP11_RSP_OK, strlen(HTTP11_RSP_OK))) {
			sess->tunnel_state = 0;
			return GF_REMOTE_SERVICE_ERROR;
		}

		//	3. send "POST /sample.mov HTTP/1.0\r\n ..."
		sess->http = gf_sk_new(GF_SOCK_TYPE_TCP);
		if (!sess->http ) {
			sess->tunnel_state = 0;
			return GF_IP_NETWORK_FAILURE;
		}
		gf_sk_set_block_mode(sess->http, GF_TRUE);
	}

	e = gf_sk_connect(sess->http, sess->Server, sess->Port, NULL);
	if (e) {
		if ((e==GF_IP_NETWORK_EMPTY) && (sess->timeout_in < gf_sys_clock()))
			e = GF_IP_CONNECTION_FAILURE;
		return e;
	}
	ua = gf_opts_get_key("core", "user-agent");
	if (!ua) ua = "GPAC " GPAC_VERSION;
	memset(buffer, 0, GF_RTSP_DEFAULT_BUFFER);
	pos = 0;
	pos += sprintf(buffer + pos, "POST /%s HTTP/1.0\r\n", sess->Service);
	pos += sprintf(buffer + pos, "User-Agent: %s\r\n", ua);
	pos += sprintf(buffer + pos, "x-sessioncookie: %s\r\n", sess->HTTP_Cookie);
	pos += sprintf(buffer + pos, "Accept: application/x-rtsp-tunnelled\r\n");
	pos += sprintf(buffer + pos, "Pragma: no-cache\r\n");
	pos += sprintf(buffer + pos, "Cache-Control: no-cache\r\n");
	pos += sprintf(buffer + pos, "Content-Length: 32767\r\n");
	/*pos += */sprintf(buffer + pos, "Expires: Sun. 9 Jan 1972 00:00:00 GMT\r\n\r\n");

	sess->tunnel_state = 0;
	//	send it, no need to wait for answer
	return gf_sk_send(sess->http, buffer, (u32) strlen(buffer));
}


/*server-side RTSP sockets*/

static u32 SessionID_RandInit = 0;


GF_EXPORT
GF_RTSPSession *gf_rtsp_session_new_server(GF_Socket *rtsp_listener, Bool allow_http_tunnel)
{
	GF_RTSPSession *sess;
	GF_Socket *new_conn;
	GF_Err e;
	u32 fam;
	u16 port;

	if (!rtsp_listener) return NULL;


	e = gf_sk_accept(rtsp_listener, &new_conn);
	if (!new_conn || e) return NULL;

	e = gf_sk_get_local_info(new_conn, &port, &fam);
	if (e) {
		gf_sk_del(new_conn);
		return NULL;
	}
	e = gf_sk_set_block_mode(new_conn, GF_TRUE);
	if (e) {
		gf_sk_del(new_conn);
		return NULL;
	}
	e = gf_sk_server_mode(new_conn, GF_TRUE);
	if (e) {
		gf_sk_del(new_conn);
		return NULL;
	}

	//OK create a new session
	GF_SAFEALLOC(sess, GF_RTSPSession);
	if (!sess) return NULL;
	
	sess->connection = new_conn;
	sess->Port = port;
	sess->ConnectionType = fam;

	const char *name = gf_opts_get_key("core", "user-agent");
	if (name) {
		sess->Server = gf_strdup(name);
	} else {
		sess->Server = gf_strdup("GPAC-");
		gf_dynstrcat(&sess->Server, gf_gpac_version(), NULL);
	}
	gf_rtsp_set_buffer_size(sess, 4096);
	sess->TCPChannels = gf_list_new();
	if (!allow_http_tunnel)
		sess->tunnel_mode = RTSP_HTTP_DISABLE;
	return sess;
}


#if 0 //unused
GF_Err gf_rtsp_load_service_name(GF_RTSPSession *sess, char *URL)
{
	char server[1024], service[1024];
	GF_Err e;
	u16 Port;
	Bool UseTCP;
	u32 type;

	if (!sess || !URL) return GF_BAD_PARAM;
	e = RTSP_UnpackURL(URL, server, &Port, service, &UseTCP);
	if (e) return e;

	type = UseTCP ? GF_SOCK_TYPE_TCP : GF_SOCK_TYPE_UDP;
	//check the network type matches, otherwise deny client
	if (sess->ConnectionType != type) return GF_URL_ERROR;
	if (sess->Port != Port) return GF_URL_ERROR;

	//ok
	sess->Server = gf_strdup(server);
	sess->Service = gf_strdup(service);
	return GF_OK;
}
#endif

GF_EXPORT
char *gf_rtsp_generate_session_id(GF_RTSPSession *sess)
{
	u32 one;
	u64 res;
	char buffer[30];

	if (!sess) return NULL;

	if (!SessionID_RandInit) {
		SessionID_RandInit = 1;
		gf_rand_init(GF_FALSE);
	}
	one = gf_rand();
	res = one;
	res <<= 32;
	res+= (PTR_TO_U_CAST sess) + sess->CurrentPos + sess->CurrentSize;
	sprintf(buffer, LLU, res);
	return gf_strdup(buffer);
}


GF_EXPORT
GF_Err gf_rtsp_get_session_ip(GF_RTSPSession *sess, char buffer[GF_MAX_IP_NAME_LEN])
{
	if (!sess || !sess->connection) return GF_BAD_PARAM;
	gf_sk_get_local_ip(sess->connection, buffer);
	return GF_OK;
}


#if 0 //unused
u8 gf_rtsp_get_next_interleave_id(GF_RTSPSession *sess)
{
	u32 i;
	u8 id;
	GF_TCPChan *ch;
	id = 0;
	i=0;
	while ((ch = (GF_TCPChan *)gf_list_enum(sess->TCPChannels, &i))) {
		if (ch->rtpID >= id) id = ch->rtpID + 1;
		if (ch->rtcpID >= id) id = ch->rtcpID + 1;
	}
	return id;
}
#endif

GF_EXPORT
GF_Err gf_rtsp_get_remote_address(GF_RTSPSession *sess, char *buf)
{
	if (!sess || !sess->connection) return GF_BAD_PARAM;
	return gf_sk_get_remote_address(sess->connection, buf);
}

static GF_Err gf_rtsp_write_sock(GF_RTSPSession *sess, u8 *data, u32 len)
{
	u32 remain, written=0;
	if (!sess->async_buf_size) {
		GF_Err e = gf_sk_send_ex((sess->http && (sess->tunnel_mode==RTSP_HTTP_CLIENT)) ? sess->http : sess->connection, data, len, &written);
		if (e && (e!= GF_IP_NETWORK_EMPTY))
			return e;
		if (written==len) return GF_OK;
	}

	remain = len - written;
	if (sess->async_buf_size + remain > sess->async_buf_alloc) {
		sess->async_buf_alloc = sess->async_buf_size+remain;
		sess->async_buf = gf_realloc(sess->async_buf, sess->async_buf_alloc);
		if (!sess->async_buf) return GF_OUT_OF_MEM;
	}
	memcpy(sess->async_buf + sess->async_buf_size, data+written, remain);
	sess->async_buf_size += remain;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_rtsp_session_write_interleaved(GF_RTSPSession *sess, u32 idx, u8 *pck, u32 pck_size)
{
	GF_Err e;
	char streamID[4];
	if (!sess || !sess->connection) return GF_BAD_PARAM;

	streamID[0] = '$';
	streamID[1] = (u8) idx;
	streamID[2] = (pck_size>>8) & 0xFF;
	streamID[3] = pck_size & 0xFF;

	e = gf_rtsp_write_sock(sess, streamID, 4);
	e |= gf_rtsp_write_sock(sess, pck, pck_size);
	return e;
}


static GF_Err gf_rstp_flush_buffer(GF_RTSPSession *sess)
{
	while (sess->async_buf_size) {
		u32 written = 0;
		GF_Err e = gf_sk_send_ex( (sess->tunnel_mode==RTSP_HTTP_CLIENT) ? sess->http : sess->connection, sess->async_buf, sess->async_buf_size, &written);
		if (e) {
			if (e!= GF_IP_NETWORK_EMPTY)
				sess->async_buf_size = 0;
			return e;
		}
		if (written==sess->async_buf_size) {
			sess->async_buf_size = 0;
			return GF_OK;
		}
		memmove(sess->async_buf, sess->async_buf + written, sess->async_buf_size - written);
		sess->async_buf_size -= written;
	}
	return GF_OK;
}

const char *gf_rtsp_get_session_cookie(GF_RTSPSession *sess)
{
	return sess ? sess->HTTP_Cookie : NULL;
}

GF_Err gf_rtsp_merge_tunnel(GF_RTSPSession *sess, GF_RTSPSession *post_sess)
{
	if (!sess || !post_sess) return GF_BAD_PARAM;
	if (!sess->HTTP_Cookie || !post_sess->HTTP_Cookie) return GF_BAD_PARAM;
	if (strcmp(sess->HTTP_Cookie, post_sess->HTTP_Cookie)) return GF_BAD_PARAM;
	if (!post_sess->connection) return GF_BAD_PARAM;

	//move any pending data
	if (post_sess->CurrentPos<post_sess->CurrentSize) {
		u8 *buf = post_sess->tcp_buffer + post_sess->CurrentPos;
		u32 remain = post_sess->CurrentSize - post_sess->CurrentPos;
		if (sess->CurrentSize + remain > sess->SockBufferSize) {
			sess->SockBufferSize = sess->CurrentSize + remain;
			sess->tcp_buffer = gf_realloc(sess->tcp_buffer, sess->SockBufferSize);
			if (!sess->tcp_buffer) return GF_OUT_OF_MEM;
		}
		memcpy(sess->tcp_buffer+sess->CurrentPos, buf, remain);
		sess->CurrentSize += remain;
	}
	sess->tunnel_mode = RTSP_HTTP_SERVER;
	sess->http = post_sess->connection;
	post_sess->connection = NULL;
	return GF_OK;
}


#endif /*GPAC_DISABLE_STREAMING*/
