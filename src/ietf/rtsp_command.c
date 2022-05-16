/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef GPAC_DISABLE_STREAMING

#include <gpac/token.h>

GF_EXPORT
GF_RTSPCommand *gf_rtsp_command_new()
{
	GF_RTSPCommand *tmp;
	GF_SAFEALLOC(tmp, GF_RTSPCommand);
	if (!tmp) return NULL;
	tmp->Xtensions = gf_list_new();
	tmp->Transports = gf_list_new();
	return tmp;
}


#define COM_FREE_CLEAN(hdr)		if (com->hdr) gf_free(com->hdr);	\
								com->hdr = NULL;

GF_EXPORT
void gf_rtsp_command_reset(GF_RTSPCommand *com)
{
	if (!com) return;

	//free all headers
	COM_FREE_CLEAN(Accept);
	COM_FREE_CLEAN(Accept_Encoding);
	COM_FREE_CLEAN(Accept_Language);
	COM_FREE_CLEAN(Authorization);
	COM_FREE_CLEAN(Cache_Control);
	COM_FREE_CLEAN(Conference);
	COM_FREE_CLEAN(Connection);
	COM_FREE_CLEAN(From);
	COM_FREE_CLEAN(Proxy_Authorization);
	COM_FREE_CLEAN(Proxy_Require);
	COM_FREE_CLEAN(Referer);
	COM_FREE_CLEAN(Session);
	COM_FREE_CLEAN(User_Agent);
	COM_FREE_CLEAN(body);
	COM_FREE_CLEAN(service_name);
	COM_FREE_CLEAN(ControlString);
	COM_FREE_CLEAN(method);

	//this is for server only, set to OK by default
	com->StatusCode = NC_RTSP_OK;


	com->user_data = NULL;

	com->Bandwidth = com->Blocksize = com->Content_Length = com->CSeq = 0;
	com->Scale = com->Speed = 0.0;
	if (com->Range) gf_free(com->Range);
	com->Range = NULL;

	while (gf_list_count(com->Transports)) {
		GF_RTSPTransport *trans = (GF_RTSPTransport *) gf_list_get(com->Transports, 0);
		gf_list_rem(com->Transports, 0);
		gf_rtsp_transport_del(trans);
	}
	while (gf_list_count(com->Xtensions)) {
		GF_X_Attribute *att = (GF_X_Attribute*)gf_list_get(com->Xtensions, 0);
		gf_list_rem(com->Xtensions, 0);
		gf_free(att->Name);
		gf_free(att->Value);
		gf_free(att);
	}
}

GF_EXPORT
void gf_rtsp_command_del(GF_RTSPCommand *com)
{
	if (!com) return;
	gf_rtsp_command_reset(com);
	gf_list_del(com->Xtensions);
	gf_list_del(com->Transports);
	gf_free(com);
}


GF_Err RTSP_WriteCommand(GF_RTSPSession *sess, GF_RTSPCommand *com, unsigned char *req_buffer,
                         unsigned char **out_buffer, u32 *out_size)
{
	u32 i, cur_pos, size, count;
	char *buffer;

	*out_buffer = NULL;
	
	size = RTSP_WRITE_STEPALLOC;
	buffer = (char *) gf_malloc(size);
	cur_pos = 0;

	//request
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, req_buffer);

	//then all headers
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Accept", com->Accept);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Accept-Encoding", com->Accept_Encoding);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Accept-Language", com->Accept_Language);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Authorization", com->Authorization);
	if (com->Bandwidth) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Bandwidth: ");
		RTSP_WRITE_INT(buffer, size, cur_pos, com->Bandwidth, 0);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	if (com->Blocksize) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Blocksize: ");
		RTSP_WRITE_INT(buffer, size, cur_pos, com->Blocksize, 0);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Cache-Control", com->Cache_Control);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Conference", com->Conference);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Connection", com->Connection);
	//if we have a body write the content length
	if (com->body) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Content-Length: ");
		RTSP_WRITE_INT(buffer, size, cur_pos, (u32) strlen(com->body), 0);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	//write the CSeq - use the SESSION CSeq
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "CSeq: ");
	RTSP_WRITE_INT(buffer, size, cur_pos, sess->CSeq, 0);
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");

	RTSP_WRITE_HEADER(buffer, size, cur_pos, "From", com->From);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Proxy-Authorization", com->Proxy_Authorization);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Proxy-Require", com->Proxy_Require);

	//Range, only NPT
	if (com->Range && !com->Range->UseSMPTE) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Range: npt=");
		RTSP_WRITE_FLOAT_WITHOUT_CHECK(buffer, size, cur_pos, com->Range->start);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "-");
		if (com->Range->end > com->Range->start) {
			RTSP_WRITE_FLOAT_WITHOUT_CHECK(buffer, size, cur_pos, com->Range->end);
		}
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}

	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Referer", com->Referer);
	if (com->Scale != 0.0) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Scale: ");
		RTSP_WRITE_FLOAT_WITHOUT_CHECK(buffer, size, cur_pos, com->Scale);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Session", com->Session);
	if (com->Speed != 0.0) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Speed: ");
		RTSP_WRITE_FLOAT_WITHOUT_CHECK(buffer, size, cur_pos, com->Speed);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}

	//transport info
	count = gf_list_count(com->Transports);
	if (count) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Transport: ");
		for (i=0; i<count; i++) {
			GF_RTSPTransport *trans;
			//line separator for headers
			if (i) RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n ,");
			trans = (GF_RTSPTransport *) gf_list_get(com->Transports, i);

			//then write the structure
			RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, trans->Profile);
			RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, (trans->IsUnicast ? ";unicast" : ";multicast"));
			if (trans->destination) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";destination=");
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, trans->destination);
			}
			if (trans->source) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";source=");
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, trans->source);
			}
			if (trans->IsRecord) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";mode=RECORD");
				if (trans->Append) RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";append");
			}
			if (trans->IsInterleaved) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";interleaved=");
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->rtpID, 0);
				if (trans->rtcpID != trans->rtpID) {
					RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "-");
					RTSP_WRITE_INT(buffer, size, cur_pos, trans->rtcpID, 0);
				}
			}
			if (trans->port_first) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, (trans->IsUnicast ? ";server_port=" : ";port="));
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->port_first, 0);
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "-");
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->port_last, 0);
			}
			if (/*trans->IsUnicast && */trans->client_port_first) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";client_port=");
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->client_port_first, 0);
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "-");
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->client_port_last, 0);
			}
			//multicast specific
			if (!trans->IsUnicast) {
				if (trans->MulticastLayers) {
					RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";layers=");
					RTSP_WRITE_INT(buffer, size, cur_pos, trans->MulticastLayers, 0);
				}
				if (trans->TTL) {
					RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";ttl=");
					RTSP_WRITE_INT(buffer, size, cur_pos, trans->TTL, 0);
				}
			}
			if (trans->SSRC) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";ssrc=");
				RTSP_WRITE_HEX(buffer, size, cur_pos, trans->SSRC, 0);
			}
		}
		//done with transport
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "User-Agent", com->User_Agent);

	//eXtensions
	count = gf_list_count(com->Xtensions);
	for (i=0; i<count; i++) {
		GF_X_Attribute *att = (GF_X_Attribute *) gf_list_get(com->Xtensions, i);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "x-");
		RTSP_WRITE_HEADER(buffer, size, cur_pos, att->Name, att->Value);
	}

	//the end of header
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	//then body
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, com->body);
	//the end of message ? to check, should not be needed...
//	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");

	*out_buffer = (unsigned char *)buffer;
	*out_size = (u32) strlen(buffer);
	return GF_OK;
}


//format a DESCRIBE, SETUP, PLAY or PAUSE on a session
//YOUR COMMAND MUST BE FORMATTED ACCORDINGLY
//sCtrl contains a control string if needed, formatting the REQUEST as server_url/service_name/sCtrl
GF_EXPORT
GF_Err gf_rtsp_send_command(GF_RTSPSession *sess, GF_RTSPCommand *com)
{
	GF_Err e;
	char *sCtrl;
	const char *rad;
	u32 size;
	char buffer[1024], *result, *body;

	if (!com || !com->method) return GF_BAD_PARAM;

	sCtrl = com->ControlString;

	//NB: OPTIONS is not sent this way
	if (strcmp(com->method, GF_RTSP_DESCRIBE)
	        && strcmp(com->method, GF_RTSP_ANNOUNCE)
	        && strcmp(com->method, GF_RTSP_GET_PARAMETER)
	        && strcmp(com->method, GF_RTSP_SET_PARAMETER)
	        && strcmp(com->method, GF_RTSP_SETUP)
	        && strcmp(com->method, GF_RTSP_PLAY)
	        && strcmp(com->method, GF_RTSP_PAUSE)
	        && strcmp(com->method, GF_RTSP_RECORD)
	        && strcmp(com->method, GF_RTSP_REDIRECT)
	        && strcmp(com->method, GF_RTSP_TEARDOWN)
	        && strcmp(com->method, GF_RTSP_OPTIONS)

	   ) return GF_BAD_PARAM;

	//check the state machine
	if (strcmp(com->method, GF_RTSP_PLAY)
	        && strcmp(com->method, GF_RTSP_PAUSE)
	        && strcmp(com->method, GF_RTSP_RECORD)
	        && sess->RTSP_State != GF_RTSP_STATE_INIT)
		return GF_SERVICE_ERROR;

	//aggregation is ONLY for the same request - unclear in RFC2326 ...
	//it is often mentioned "queued requests" at the server, like 3 PLAYS
	//and a PAUSE ....

	/*
	else if (sess->RTSP_State == GF_RTSP_STATE_WAIT_FOR_CONTROL
		&& strcmp(com->method, sess->RTSPLastRequest))
		&& strcmp(com->method, GF_RTSP_OPTIONS))

		return GF_BAD_PARAM;
	*/

	//OPTIONS must have a parameter string
	if (!strcmp(com->method, GF_RTSP_OPTIONS) && !sCtrl) return GF_BAD_PARAM;


	//update sequence number
	sess->CSeq += 1;
	sess->NbPending += 1;

	if (!strcmp(com->method, GF_RTSP_OPTIONS)) {
		sprintf(buffer, "OPTIONS %s %s\r\n", sCtrl, GF_RTSP_VERSION);
	} else {
		rad = (sess->ConnectionType == GF_SOCK_TYPE_TCP) ? "rtsp" : "rtspu";
		if (sCtrl) {
			//if both server and service names are included in the control, just
			//use the control
			if (strstr(sCtrl, sess->Server) && strstr(sCtrl, sess->Service)) {
				sprintf(buffer, "%s %s %s\r\n", com->method, sCtrl, GF_RTSP_VERSION);
			}
			//if service is specified in ctrl, do not rewrite it
			else if (strstr(sCtrl, sess->Service)) {
				sprintf(buffer, "%s %s://%s:%d/%s %s\r\n", com->method, rad, sess->Server, sess->Port, sCtrl, GF_RTSP_VERSION);
			}
			else if (!strnicmp(sCtrl, "rtsp", 4)) {
				sprintf(buffer, "%s %s %s\r\n", com->method, sCtrl, GF_RTSP_VERSION);
			}
			//otherwise rewrite full URL
			else {
				sprintf(buffer, "%s %s://%s/%s/%s %s\r\n", com->method, rad, sess->Server, sess->Service, sCtrl, GF_RTSP_VERSION);
//				sprintf(buffer, "%s %s://%s:%d/%s/%s %s\r\n", com->method, rad, sess->Server, sess->Port, sess->Service, sCtrl, GF_RTSP_VERSION);
			}
		} else {
			sprintf(buffer, "%s %s://%s:%d/%s %s\r\n", com->method, rad, sess->Server, sess->Port, sess->Service, GF_RTSP_VERSION);
		}
	}

	//Body on ANNOUNCE, GET_PARAMETER, SET_PARAMETER ONLY
	body = NULL;
	if (strcmp(com->method, GF_RTSP_ANNOUNCE)
	        && strcmp(com->method, GF_RTSP_GET_PARAMETER)
	        && strcmp(com->method, GF_RTSP_SET_PARAMETER)
	   ) {
		//this is an error, but don't say anything
		if (com->body) {
			body = com->body;
			com->body = NULL;
		}
	}

	result = NULL;
	e = RTSP_WriteCommand(sess, com, (unsigned char *)buffer, (unsigned char **) &result, &size);
	//restore body if needed
	if (body) com->body = body;
	if (e) goto exit;


	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSP] Sending Command:\n%s\n", result));

	//send buffer
	e = gf_rtsp_send_data(sess, result, size);
	if (e) goto exit;


	//update our state
	if (!strcmp(com->method, GF_RTSP_RECORD)) sess->RTSP_State = GF_RTSP_STATE_WAIT_FOR_CONTROL;
	else if (!strcmp(com->method, GF_RTSP_PLAY)) sess->RTSP_State = GF_RTSP_STATE_WAIT_FOR_CONTROL;
	else if (!strcmp(com->method, GF_RTSP_PAUSE)) sess->RTSP_State = GF_RTSP_STATE_WAIT_FOR_CONTROL;
	else sess->RTSP_State = GF_RTSP_STATE_WAITING;
	//teardown invalidates the session most of the time, so we force the user to wait for the reply
	//as the reply may indicate a connection-closed
	strcpy(sess->RTSPLastRequest, com->method);

exit:
	if (result) gf_free(result);
	return e;
}


void gf_rtsp_set_command_value(GF_RTSPCommand *com, char *Header, char *Value)
{
	char LineBuffer[400];
	s32 LinePos;
	GF_RTSPTransport *trans;
	GF_X_Attribute *x_Att;

	if (!stricmp(Header, "Accept")) com->Accept = gf_strdup(Value);
	else if (!stricmp(Header, "Accept-Encoding")) com->Accept_Encoding = gf_strdup(Value);
	else if (!stricmp(Header, "Accept-Language")) com->Accept_Language = gf_strdup(Value);
	else if (!stricmp(Header, "Authorization")) com->Authorization = gf_strdup(Value);
	else if (!stricmp(Header, "Bandwidth")) sscanf(Value, "%u", &com->Bandwidth);
	else if (!stricmp(Header, "Blocksize")) sscanf(Value, "%u", &com->Blocksize);
	else if (!stricmp(Header, "Cache-Control")) com->Cache_Control = gf_strdup(Value);
	else if (!stricmp(Header, "Conference")) com->Conference = gf_strdup(Value);
	else if (!stricmp(Header, "Connection")) com->Connection = gf_strdup(Value);
	else if (!stricmp(Header, "Content-Length")) sscanf(Value, "%u", &com->Content_Length);
	else if (!stricmp(Header, "CSeq")) sscanf(Value, "%u", &com->CSeq);
	else if (!stricmp(Header, "From")) com->From = gf_strdup(Value);
	else if (!stricmp(Header, "Proxy_Authorization")) com->Proxy_Authorization = gf_strdup(Value);
	else if (!stricmp(Header, "Proxy_Require")) com->Proxy_Require = gf_strdup(Value);
	else if (!stricmp(Header, "Range")) com->Range = gf_rtsp_range_parse(Value);
	else if (!stricmp(Header, "Referer")) com->Referer = gf_strdup(Value);
	else if (!stricmp(Header, "Scale")) sscanf(Value, "%lf", &com->Scale);
	else if (!stricmp(Header, "Session"))
		com->Session = gf_strdup(Value);
	else if (!stricmp(Header, "Speed")) sscanf(Value, "%lf", &com->Speed);
	else if (!stricmp(Header, "User_Agent")) com->User_Agent = gf_strdup(Value);
	//Transports
	else if (!stricmp(Header, "Transport")) {
		LinePos = 0;
		while (1) {
			LinePos = gf_token_get(Value, LinePos, "\r\n", LineBuffer, 400);
			if (LinePos <= 0) return;
			trans = gf_rtsp_transport_parse(Value);
			if (trans) gf_list_add(com->Transports, trans);
		}
	}
	//eXtensions attributes
	else if (!strnicmp(Header, "x-", 2)) {
		x_Att = (GF_X_Attribute*)gf_malloc(sizeof(GF_X_Attribute));
		x_Att->Name = gf_strdup(Header+2);
		x_Att->Value = NULL;
		if (Value && strlen(Value)) x_Att->Value = gf_strdup(Value);
		gf_list_add(com->Xtensions, x_Att);
	}
	//the rest is ignored
}

GF_Err RTSP_ParseCommandHeader(GF_RTSPSession *sess, GF_RTSPCommand *com, u32 BodyStart)
{
	char LineBuffer[1024];
	char ValBuf[1024];
	char *buffer;
	s32 Pos, ret;
	u32 Size;

	Size = sess->CurrentSize - sess->CurrentPos;
	buffer = sess->tcp_buffer + sess->CurrentPos;

	//by default the command is wrong ;)
	com->StatusCode = NC_RTSP_Bad_Request;

	//parse first line
	ret = gf_token_get_line(buffer, 0, Size, LineBuffer, 1024);
	if (ret < 0) return GF_REMOTE_SERVICE_ERROR;

	//method
	Pos = gf_token_get(LineBuffer, 0, " \t\r\n", ValBuf, 1024);
	if (Pos <= 0) return GF_OK;
	com->method = gf_strdup((const char *) ValBuf);

	//URL
	Pos = gf_token_get(LineBuffer, Pos, " \t\r\n", ValBuf, 1024);
	if (Pos <= 0) return GF_OK;
	com->service_name = gf_strdup(ValBuf);

	//RTSP version
	Pos = gf_token_get(LineBuffer, Pos, " \t\r\n", ValBuf, 1024);
	if (Pos <= 0) return GF_OK;
	if (strcmp(ValBuf, GF_RTSP_VERSION)) {
		com->StatusCode = NC_RTSP_RTSP_Version_Not_Supported;
		return GF_OK;
	}

	com->StatusCode = NC_RTSP_OK;

	return gf_rtsp_parse_header(buffer + ret, Size - ret, BodyStart, com, NULL);
}

char *RTSP_DEFINED_METHODS[] =
{
	GF_RTSP_DESCRIBE,
	GF_RTSP_SETUP,
	GF_RTSP_PLAY,
	GF_RTSP_PAUSE,
	GF_RTSP_RECORD,
	GF_RTSP_TEARDOWN,
	GF_RTSP_GET_PARAMETER,
	GF_RTSP_SET_PARAMETER,
	GF_RTSP_OPTIONS,
	GF_RTSP_ANNOUNCE,
	GF_RTSP_REDIRECT,
	NULL
};

GF_EXPORT
GF_Err gf_rtsp_get_command(GF_RTSPSession *sess, GF_RTSPCommand *com)
{
	GF_Err e;
	u32 BodyStart, size;
	if (!sess || !com) return GF_BAD_PARAM;

	//reset the command
	gf_rtsp_command_reset(com);
	//if no connection, we have sent a "Connection: Close"
	if (!sess->connection) return GF_IP_CONNECTION_CLOSED;

	//lock
	//fill TCP buffer
	e = gf_rtsp_fill_buffer(sess);
	if (e) goto exit;
	if (sess->TCPChannels)
	//this is upcoming, interleaved data
	if (sess->interleaved) {
		u32 i=0;
		Bool sync = GF_FALSE;
		while (RTSP_DEFINED_METHODS[i]) {
			if (!strncmp(sess->tcp_buffer+sess->CurrentPos, RTSP_DEFINED_METHODS[i], strlen(RTSP_DEFINED_METHODS[i]) ) ) {
				sync = GF_TRUE;
				break;
			}
		}
		if (!sync) {
			e = GF_IP_NETWORK_EMPTY;
			goto exit;
		}
	}
	e = gf_rtsp_read_reply(sess);
	if (e) goto exit;

	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSP] Got Command:\n%s\n", sess->tcp_buffer+sess->CurrentPos));

	gf_rtsp_get_body_info(sess, &BodyStart, &size);
	e = RTSP_ParseCommandHeader(sess, com, BodyStart);
	//before returning an error we MUST reset the TCP buffer

	//copy the body if any
	if (!e && com->Content_Length) {
		u32 rsp_size = sess->CurrentSize - sess->CurrentPos;
		if (rsp_size < com->Content_Length) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] Invalid content length %u - Response was: \n%s\n", com->Content_Length, sess->tcp_buffer+sess->CurrentPos));
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto exit;
		}
		com->body = (char *) gf_malloc(sizeof(char) * (com->Content_Length));
		memcpy(com->body, sess->tcp_buffer+sess->CurrentPos + BodyStart, com->Content_Length);
	}
	//reset TCP buffer
	sess->CurrentPos += BodyStart + com->Content_Length;

	if (!com->CSeq)
		com->StatusCode = NC_RTSP_Bad_Request;

	if (e || (com->StatusCode != NC_RTSP_OK)) goto exit;

	//NB: there is no "session state" in our lib when acting at the server side, as it depends
	//on the server implementation. We cannot block responses / announcement to be sent
	//dynamically, nor reset the session ourselves as we don't know the details of the session
	//(eg TEARDOWN may keep resources up or not, ...)

	//we also have the same pb for CSeq, as nothing forbids a server to buffer commands (and it
	//happens during aggregation of PLAY/PAUSE with overlapping ranges)

	//however store the last CSeq in case for client checking
	if (!sess->CSeq) {
		sess->CSeq = com->CSeq;
	}
	//check we're in the right range
	else {
		if (sess->CSeq >= com->CSeq)
			com->StatusCode = NC_RTSP_Header_Field_Not_Valid;
		else
			sess->CSeq = com->CSeq;
	}

	//
	//if a connection closed is signal, check this is the good session
	// and reset it (the client is no longer connected)
	if (sess->last_session_id && com->Session && !strcmp(com->Session, sess->last_session_id)
	        && com->Connection && !stricmp(com->Connection, "Close")) {

		gf_rtsp_session_reset(sess, GF_FALSE);
		//destroy the socket
		if (sess->connection) gf_sk_del(sess->connection);
		sess->connection = NULL;

		//destroy the http tunnel if any
		if (sess->HasTunnel && sess->http) {
			gf_sk_del(sess->http);
			sess->http = NULL;
		}
	}

exit:
	return e;
}

#endif /*GPAC_DISABLE_STREAMING*/
