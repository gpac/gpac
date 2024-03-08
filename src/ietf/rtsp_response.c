/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2024
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
GF_RTSPResponse *gf_rtsp_response_new()
{
	GF_RTSPResponse *tmp;
	GF_SAFEALLOC(tmp, GF_RTSPResponse);
	if (!tmp) return NULL;
	tmp->Transports = gf_list_new();
	tmp->RTP_Infos = gf_list_new();
	tmp->Xtensions = gf_list_new();
	return tmp;
}


#define RSP_FREE_CLEAN(hdr)		if (rsp->hdr) gf_free(rsp->hdr);	\
								rsp->hdr = NULL;

GF_EXPORT
void gf_rtsp_response_reset(GF_RTSPResponse *rsp)
{
	if (!rsp) return;

	//free all headers
	RSP_FREE_CLEAN(Accept);
	RSP_FREE_CLEAN(Accept_Encoding);
	RSP_FREE_CLEAN(Accept_Language);
	RSP_FREE_CLEAN(Allow);
	RSP_FREE_CLEAN(Authorization);
	RSP_FREE_CLEAN(Cache_Control);
	RSP_FREE_CLEAN(Conference);
	RSP_FREE_CLEAN(Connection);
	RSP_FREE_CLEAN(Content_Base);
	RSP_FREE_CLEAN(Content_Encoding);
	RSP_FREE_CLEAN(Content_Language);
	RSP_FREE_CLEAN(Content_Location);
	RSP_FREE_CLEAN(Content_Type);
	RSP_FREE_CLEAN(Date);
	RSP_FREE_CLEAN(Expires);
	RSP_FREE_CLEAN(From);
	RSP_FREE_CLEAN(Host);
	RSP_FREE_CLEAN(If_Match);
	RSP_FREE_CLEAN(If_Modified_Since);
	RSP_FREE_CLEAN(Last_Modified);
	RSP_FREE_CLEAN(Location);
	RSP_FREE_CLEAN(Proxy_Authenticate);
	RSP_FREE_CLEAN(Proxy_Require);
	RSP_FREE_CLEAN(Public);
	RSP_FREE_CLEAN(Referer);
	RSP_FREE_CLEAN(Require);
	RSP_FREE_CLEAN(Retry_After);
	RSP_FREE_CLEAN(Server);
	RSP_FREE_CLEAN(Session);
	RSP_FREE_CLEAN(Timestamp);
	RSP_FREE_CLEAN(Unsupported);
	RSP_FREE_CLEAN(User_Agent);
	RSP_FREE_CLEAN(Vary);
	RSP_FREE_CLEAN(Via);
	RSP_FREE_CLEAN(WWW_Authenticate);

	//this is for us
	RSP_FREE_CLEAN(ResponseInfo);
	RSP_FREE_CLEAN(body);

	rsp->Bandwidth = rsp->Blocksize = rsp->ResponseCode = rsp->Content_Length = rsp->CSeq = 0;
	rsp->Scale = rsp->Speed = 0.0;
	if (rsp->Range) gf_free(rsp->Range);
	rsp->Range = NULL;

	rsp->SessionTimeOut = 0;

	while (gf_list_count(rsp->Transports)) {
		GF_RTSPTransport *trans = (GF_RTSPTransport*) gf_list_get(rsp->Transports, 0);
		gf_list_rem(rsp->Transports, 0);
		gf_rtsp_transport_del(trans);
	}

	while (gf_list_count(rsp->RTP_Infos)) {
		GF_RTPInfo *inf = (GF_RTPInfo*) gf_list_get(rsp->RTP_Infos, 0);
		gf_list_rem(rsp->RTP_Infos, 0);
		if (inf->url) gf_free(inf->url);
		gf_free(inf);
	}
	while (gf_list_count(rsp->Xtensions)) {
		GF_X_Attribute *att = (GF_X_Attribute*)gf_list_get(rsp->Xtensions, 0);
		gf_list_rem(rsp->Xtensions, 0);
		gf_free(att->Name);
		gf_free(att->Value);
		gf_free(att);
	}
}

GF_EXPORT
void gf_rtsp_response_del(GF_RTSPResponse *rsp)
{
	if (!rsp) return;

	gf_rtsp_response_reset(rsp);
	gf_list_del(rsp->RTP_Infos);
	gf_list_del(rsp->Xtensions);
	gf_list_del(rsp->Transports);
	gf_free(rsp);
}



GF_EXPORT
GF_RTSPRange *gf_rtsp_range_parse(char *range_buf)
{
	GF_RTSPRange *rg;

	if (!strstr(range_buf, "npt")) return NULL;

	GF_SAFEALLOC(rg, GF_RTSPRange);
	if (!rg) return NULL;
	if (sscanf(range_buf, "npt=%lf-%lf", &rg->start, &rg->end) != 2) {
		rg->end = -1.0;
		sscanf(range_buf, "npt=%lf-", &rg->start);
	}
	return rg;
}

GF_EXPORT
void gf_rtsp_transport_del(GF_RTSPTransport *transp)
{
	if (!transp) return;
	if (transp->destination) gf_free(transp->destination);
	if (transp->Profile) gf_free(transp->Profile);
	if (transp->source) gf_free(transp->source);
	gf_free(transp);
}

GF_EXPORT
GF_RTSPTransport *gf_rtsp_transport_clone(GF_RTSPTransport *original)
{
	GF_RTSPTransport *tr;

	if (!original) return NULL;

	tr = (GF_RTSPTransport*) gf_malloc(sizeof(GF_RTSPTransport));
	memcpy(tr, original, sizeof(GF_RTSPTransport));
	tr->destination = tr->source = tr->Profile = NULL;
	if (original->destination) tr->destination = gf_strdup(original->destination);
	if (original->source) tr->source = gf_strdup(original->source);
	if (original->Profile) tr->Profile = gf_strdup(original->Profile);
	return tr;
}

GF_EXPORT
GF_RTSPRange *gf_rtsp_range_new()
{
	GF_RTSPRange *tmp;
	GF_SAFEALLOC(tmp, GF_RTSPRange);
	return tmp;
}

GF_EXPORT
void gf_rtsp_range_del(GF_RTSPRange *range)
{
	if (!range) return;
	gf_free(range);
}

void gf_rtsp_set_response_value(GF_RTSPResponse *rsp, char *Header, char *Value)
{
	char LineBuffer[400], buf[1000], param_name[100], param_val[1000];
	s32 LinePos, Pos, nPos;
	GF_RTPInfo *info;
	GF_RTSPTransport *trans;
	GF_X_Attribute *x_Att;

	if (!stricmp(Header, "Accept")) rsp->Accept = gf_strdup(Value);
	else if (!stricmp(Header, "Accept-Encoding")) rsp->Accept_Encoding = gf_strdup(Value);
	else if (!stricmp(Header, "Accept-Language")) rsp->Accept_Language = gf_strdup(Value);
	else if (!stricmp(Header, "Allow")) rsp->Allow = gf_strdup(Value);
	else if (!stricmp(Header, "Authorization")) rsp->Authorization = gf_strdup(Value);
	else if (!stricmp(Header, "Bandwidth")) sscanf(Value, "%u", &rsp->Bandwidth);
	else if (!stricmp(Header, "Blocksize")) sscanf(Value, "%u", &rsp->Blocksize);
	else if (!stricmp(Header, "Cache-Control")) rsp->Cache_Control = gf_strdup(Value);
	else if (!stricmp(Header, "com.ses.streamID")) sscanf(Value, "%u", &rsp->StreamID);
	else if (!stricmp(Header, "Conference")) rsp->Conference = gf_strdup(Value);
	else if (!stricmp(Header, "Connection")) rsp->Connection = gf_strdup(Value);
	else if (!stricmp(Header, "Content-Base")) rsp->Content_Base = gf_strdup(Value);
	else if (!stricmp(Header, "Content-Encoding")) rsp->Content_Encoding = gf_strdup(Value);
	else if (!stricmp(Header, "Content-Length")) sscanf(Value, "%u", &rsp->Content_Length);
	else if (!stricmp(Header, "Content-Language")) rsp->Content_Language = gf_strdup(Value);
	else if (!stricmp(Header, "Content-Location")) rsp->Content_Location = gf_strdup(Value);
	else if (!stricmp(Header, "Content-Type")) rsp->Content_Type = gf_strdup(Value);
	else if (!stricmp(Header, "CSeq")) sscanf(Value, "%u", &rsp->CSeq);
	else if (!stricmp(Header, "Date")) rsp->Date = gf_strdup(Value);
	else if (!stricmp(Header, "Expires")) rsp->Expires = gf_strdup(Value);
	else if (!stricmp(Header, "From")) rsp->From = gf_strdup(Value);
	else if (!stricmp(Header, "Host")) rsp->Host = gf_strdup(Value);
	else if (!stricmp(Header, "If-Match")) rsp->If_Match = gf_strdup(Value);
	else if (!stricmp(Header, "If-Modified-Since")) rsp->If_Modified_Since = gf_strdup(Value);
	else if (!stricmp(Header, "Last-Modified")) rsp->Last_Modified = gf_strdup(Value);
	else if (!stricmp(Header, "Location")) rsp->Location = gf_strdup(Value);
	else if (!stricmp(Header, "Proxy-Authenticate")) rsp->Proxy_Authenticate = gf_strdup(Value);
	else if (!stricmp(Header, "Proxy-Require")) rsp->Proxy_Require = gf_strdup(Value);
	else if (!stricmp(Header, "Public")) rsp->Public = gf_strdup(Value);
	else if (!stricmp(Header, "Referer")) rsp->Referer = gf_strdup(Value);
	else if (!stricmp(Header, "Require")) rsp->Require = gf_strdup(Value);
	else if (!stricmp(Header, "Retry-After")) rsp->Retry_After = gf_strdup(Value);
	else if (!stricmp(Header, "Scale")) sscanf(Value, "%lf", &rsp->Scale);
	else if (!stricmp(Header, "Server")) rsp->Server = gf_strdup(Value);
	else if (!stricmp(Header, "Speed")) sscanf(Value, "%lf", &rsp->Speed);
	else if (!stricmp(Header, "Timestamp")) rsp->Timestamp = gf_strdup(Value);
	else if (!stricmp(Header, "Unsupported")) rsp->Unsupported = gf_strdup(Value);
	else if (!stricmp(Header, "User-Agent")) rsp->User_Agent = gf_strdup(Value);
	else if (!stricmp(Header, "Vary")) rsp->Vary = gf_strdup(Value);
	else if (!stricmp(Header, "Via")) rsp->Via = gf_strdup(Value);
	else if (!stricmp(Header, "WWW-Authenticate")) rsp->WWW_Authenticate = gf_strdup(Value);
	else if (!stricmp(Header, "Transport")) {
		LinePos = 0;
		while (1) {
			LinePos = gf_token_get(Value, LinePos, "\r\n", LineBuffer, 400);
			if (LinePos <= 0) return;
			trans = gf_rtsp_transport_parse(Value);
			if (trans) gf_list_add(rsp->Transports, trans);
		}
	}
	//Session
	else if (!stricmp(Header, "Session")) {
		LinePos = gf_token_get(Value, 0, ";\r\n", LineBuffer, 400);
		rsp->Session = gf_strdup(LineBuffer);
		//get timeout if any
		if (Value[LinePos] == ';') {
			LinePos += 1;
			/*LinePos = */gf_token_get(Value, LinePos, ";\r\n", LineBuffer, 400);
			rsp->SessionTimeOut = 60; //default
			sscanf(LineBuffer, "timeout=%u", &rsp->SessionTimeOut);
		}
	}

	//Range
	else if (!stricmp(Header, "Range")) rsp->Range = gf_rtsp_range_parse(Value);
	//RTP-Info
	else if (!stricmp(Header, "RTP-Info")) {
		LinePos = 0;
		while (1) {
			LinePos = gf_token_get(Value, LinePos, ",\r\n", LineBuffer, 400);
			if (LinePos <= 0) return;

			GF_SAFEALLOC(info, GF_RTPInfo);
			if (!info) return;

			Pos = 0;
			while (1) {
				Pos = gf_token_get(LineBuffer, Pos, " ;", buf, 1000);
				if (Pos <= 0) break;
				if (strstr(buf, "=")) {
					nPos = gf_token_get(buf, 0, "=", param_name, 100);
					nPos += 1;
					/*nPos = */gf_token_get(buf, nPos, "", param_val, 1000);
				} else {
					strcpy(param_name, buf);
				}
				if (!stricmp(param_name, "url")) info->url = gf_strdup(param_val);
				else if (!stricmp(param_name, "seq")) sscanf(param_val, "%u", &info->seq);
				else if (!stricmp(param_name, "rtptime")) {
					sscanf(param_val, "%u", &info->rtp_time);
				}
				else if (!stricmp(param_name, "ssrc")) {
					sscanf(param_val, "%u", &info->ssrc);
				}
			}
			gf_list_add(rsp->RTP_Infos, info);
		}
	}
	//check for extended attributes
	else if (!strnicmp(Header, "x-", 2)) {
		x_Att = (GF_X_Attribute*)gf_malloc(sizeof(GF_X_Attribute));
		x_Att->Name = gf_strdup(Header+2);
		x_Att->Value = NULL;
		if (Value && strlen(Value)) x_Att->Value = gf_strdup(Value);
		gf_list_add(rsp->Xtensions, x_Att);
	}
	//unknown field - skip it
}



//parse all fields in the header
GF_Err RTSP_ParseResponseHeader(GF_RTSPSession *sess, GF_RTSPResponse *rsp, u32 BodyStart)
{
	char LineBuffer[1024];
	char ValBuf[400];
	char *buffer;
	s32 Pos, ret;
	u32 Size;

	Size = sess->CurrentSize - sess->CurrentPos;
	buffer = sess->tcp_buffer + sess->CurrentPos;

	//parse first line
	ret = gf_token_get_line(buffer, 0, Size, LineBuffer, 1024);
	if (ret < 0)
		return GF_REMOTE_SERVICE_ERROR;
	//RTSP/1.0
	Pos = gf_token_get(LineBuffer, 0, " \t\r\n", ValBuf, 400);
	if (Pos <= 0)
		return GF_REMOTE_SERVICE_ERROR;
	if (strcmp(ValBuf, GF_RTSP_VERSION))
		return GF_SERVICE_ERROR;
	//CODE
	Pos = gf_token_get(LineBuffer, Pos, " \t\r\n", ValBuf, 400);
	if (Pos <= 0)
		return GF_REMOTE_SERVICE_ERROR;
	rsp->ResponseCode = atoi(ValBuf);
	//string info
	Pos = gf_token_get(LineBuffer, Pos, "\t\r\n", ValBuf, 400);
	if (Pos > 0) rsp->ResponseInfo = gf_strdup(ValBuf);

	return gf_rtsp_parse_header(buffer + ret, Size - ret, BodyStart, NULL, rsp);
}



u32 IsRTSPMessage(char *buffer)
{
	if (!buffer) return 0;
	if (buffer[0]=='$') return 0;

	if (!strncmp(buffer, "RTSP", 4)) return 1;
	if (!strncmp(buffer, "HTTP", 4)) return 1;
	if (!strncmp(buffer, "GET_PARAMETER", strlen("GET_PARAMETER"))) return 1;
	if (!strncmp(buffer, "ANNOUNCE", strlen("ANNOUNCE"))) return 1;
	if (!strncmp(buffer, "SET_PARAMETER", strlen("SET_PARAMETER"))) return 1;
	if (!strncmp(buffer, "REDIRECT", strlen("REDIRECT"))) return 1;
	if (!strncmp(buffer, "OPTIONS", strlen("OPTIONS"))) return 1;
	return 0;
}


GF_EXPORT
GF_Err gf_rtsp_get_response(GF_RTSPSession *sess, GF_RTSPResponse *rsp)
{
	GF_Err e;
	Bool force_reset = GF_FALSE;
	u32 BodyStart, size;

	if (!sess || !rsp) return GF_BAD_PARAM;
	gf_rtsp_response_reset(rsp);

	e = gf_rtsp_check_connection(sess);
	if (e)
		goto exit;

	//push data in our queue
	e = gf_rtsp_fill_buffer(sess);
	if (e)
		goto exit;

	//this is interleaved data
	if (!IsRTSPMessage(sess->tcp_buffer+sess->CurrentPos) ) {
		gf_rtsp_session_read(sess);
		e = GF_IP_NETWORK_EMPTY;
		goto exit;
	}
	e = gf_rtsp_read_reply(sess);
	if (e)
		goto exit;

	//get the reply
	gf_rtsp_get_body_info(sess, &BodyStart, &size, GF_FALSE);

	if (!strncmp(sess->tcp_buffer+sess->CurrentPos, "HTTP", 4)) {
		sess->CurrentPos += BodyStart + rsp->Content_Length;
		e = GF_IP_NETWORK_EMPTY;
		goto exit;
	}
	e = RTSP_ParseResponseHeader(sess, rsp, BodyStart);

	//copy the body if any
	if (!e && rsp->Content_Length) {
		u32 rsp_size = sess->CurrentSize - sess->CurrentPos;
		if (rsp_size < rsp->Content_Length) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] Invalid content length %u - Response was: \n%s\n", rsp->Content_Length, sess->tcp_buffer+sess->CurrentPos));
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto exit;
		}
		rsp->body = (char *)gf_malloc(sizeof(char) * (rsp->Content_Length));
		memcpy(rsp->body, sess->tcp_buffer+sess->CurrentPos + BodyStart, rsp->Content_Length);
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSP] Got Response:\n%s\n", sess->tcp_buffer+sess->CurrentPos));

	//reset TCP buffer
	sess->CurrentPos += BodyStart + rsp->Content_Length;


	if (e) goto exit;

	//update RTSP aggreagation info
	if (sess->NbPending) sess->NbPending -= 1;

	if (sess->RTSP_State == GF_RTSP_STATE_WAITING) sess->RTSP_State = GF_RTSP_STATE_INIT;
	//control, and everything is received
	else if (sess->RTSP_State == GF_RTSP_STATE_WAIT_FOR_CONTROL) {
		if (!sess->NbPending) sess->RTSP_State = GF_RTSP_STATE_INIT;
	}
	//this is a late reply to an aggregated control - signal nothing
	if (!strcmp(sess->RTSPLastRequest, "RESET") && sess->CSeq > rsp->CSeq) {
		e = GF_IP_NETWORK_EMPTY;
		goto exit;
	}

	//reset last request
	if (sess->RTSP_State == GF_RTSP_STATE_INIT) strcpy(sess->RTSPLastRequest, "");

	//check the CSeq is in the right range. The server should ALWAYS reply in sequence
	//to an aggreagated sequence of requests
	//if we have reseted the connection (due to an APP error) return empty
	if (rsp->CSeq && sess->CSeq > rsp->CSeq + sess->NbPending) {
		return gf_rtsp_get_response(sess, rsp);
	}

	if (sess->CSeq != rsp->CSeq + sess->NbPending) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] Invalid sequence number - got %d but expected %d\n", sess->CSeq, rsp->CSeq + sess->NbPending));
		e = GF_REMOTE_SERVICE_ERROR;
		goto exit;
	}

	/*check session ID*/
	if (rsp->Session && sess->last_session_id && strcmp(sess->last_session_id, rsp->Session)) {
		e = GF_REMOTE_SERVICE_ERROR;
		goto exit;
	}

	//destroy sessionID if needed - real doesn't close the connection when destroying
	//session
	if (!strcmp(sess->RTSPLastRequest, GF_RTSP_TEARDOWN)) {
		sess->last_session_id = NULL;
	}

exit:
	if (rsp->Connection && !stricmp(rsp->Connection, "Close"))
		force_reset = GF_TRUE;
	else if (e && (e != GF_IP_NETWORK_EMPTY))
		force_reset = GF_TRUE;

	if (force_reset) {
		gf_rtsp_session_reset(sess, GF_FALSE);
		//destroy the socket
		if (sess->connection) gf_sk_del(sess->connection);
		sess->connection = NULL;

		//destroy the http tunnel if any
		if (sess->http) {
			gf_sk_del(sess->http);
			sess->http = NULL;
		}
	}
	return e;
}

GF_Err RTSP_WriteResponse(GF_RTSPSession *sess, GF_RTSPResponse *rsp,
                          unsigned char **out_buffer, u32 *out_size)
{
	u32 i, cur_pos, size, count;
	char *buffer;

	*out_buffer = NULL;

	size = RTSP_WRITE_STEPALLOC;
	buffer = (char *) gf_malloc(size);
	cur_pos = 0;

	//RTSP line
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, GF_RTSP_VERSION);
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, " ");
	RTSP_WRITE_INT(buffer, size, cur_pos, rsp->ResponseCode, 0);
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, " ");
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, gf_rtsp_nc_to_string(rsp->ResponseCode));
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");

	//all headers
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Accept", rsp->Accept);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Accept-Encoding", rsp->Accept_Encoding);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Accept-Language", rsp->Accept_Language);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Allow", rsp->Allow);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Authorization", rsp->Authorization);
	if (rsp->Bandwidth) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Bandwidth: ");
		RTSP_WRITE_INT(buffer, size, cur_pos, rsp->Bandwidth, 0);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	if (rsp->Blocksize) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Blocksize: ");
		RTSP_WRITE_INT(buffer, size, cur_pos, rsp->Blocksize, 0);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Cache-Control", rsp->Cache_Control);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Conference", rsp->Conference);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Connection", rsp->Connection);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Content-Base", rsp->Content_Base);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Content-Encoding", rsp->Content_Encoding);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Content-Language", rsp->Content_Language);
	//if we have a body write the content length
	if (rsp->body) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Content-Length: ");
		RTSP_WRITE_INT(buffer, size, cur_pos, (u32) strlen(rsp->body), 0);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Content-Location", rsp->Content_Location);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Content-Type", rsp->Content_Type);
	//write the CSeq - use the RESPONSE CSeq
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "CSeq: ");
	RTSP_WRITE_INT(buffer, size, cur_pos, rsp->CSeq, 0);
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");

	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Date", rsp->Date);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Expires", rsp->Expires);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "From", rsp->From);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Host", rsp->Host);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "If-Match", rsp->If_Match);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "If-Modified-Since", rsp->If_Modified_Since);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Last-Modified", rsp->Last_Modified);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Location", rsp->Location);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Proxy-Authenticate", rsp->Proxy_Authenticate);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Proxy-Require", rsp->Proxy_Require);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Public", rsp->Public);

	//Range, only NPT
	if (rsp->Range && !rsp->Range->UseSMPTE) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Range: npt=");
		RTSP_WRITE_FLOAT_WITHOUT_CHECK(buffer, size, cur_pos, rsp->Range->start);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "-");
		if (rsp->Range->end > rsp->Range->start) {
			RTSP_WRITE_FLOAT_WITHOUT_CHECK(buffer, size, cur_pos, rsp->Range->end);
		}
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}

	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Referer", rsp->Referer);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Require", rsp->Require);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Retry-After", rsp->Retry_After);

	//RTP Infos
	count = gf_list_count(rsp->RTP_Infos);
	if (count) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "RTP-Info: ");

		for (i=0; i<count; i++) {
			GF_RTPInfo *info;
			//line separator for headers
			if (i) RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n ,");
			info = (GF_RTPInfo*)gf_list_get(rsp->RTP_Infos, i);

			if (info->url) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "url=");
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, info->url);
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";");
			}
			RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "seq=");
			RTSP_WRITE_INT(buffer, size, cur_pos, info->seq, 0);
			RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";rtptime=");
			RTSP_WRITE_INT(buffer, size, cur_pos, info->rtp_time, 0);
		}
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}

	if (rsp->Scale != 0.0) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Scale: ");
		RTSP_WRITE_FLOAT_WITHOUT_CHECK(buffer, size, cur_pos, rsp->Scale);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Server", rsp->Server);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Session", rsp->Session);
	if (rsp->Speed != 0.0) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Scale: ");
		RTSP_WRITE_FLOAT_WITHOUT_CHECK(buffer, size, cur_pos, rsp->Speed);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Timestamp", rsp->Timestamp);

	//transport info
	count = gf_list_count(rsp->Transports);
	if (count) {
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "Transport: ");
		for (i=0; i<count; i++) {
			GF_RTSPTransport *trans;
			//line separator for headers
			if (i) RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n ,");
			trans = (GF_RTSPTransport*)gf_list_get(rsp->Transports, i);

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
			if (trans->port_first) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, (const char *) (trans->IsUnicast ? ";server_port=" : ";port="));
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->port_first, 0);
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "-");
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->port_last, 0);
			}
			if (trans->IsUnicast && trans->client_port_first) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";client_port=");
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->client_port_first, 0);
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "-");
				RTSP_WRITE_INT(buffer, size, cur_pos, trans->client_port_last, 0);
			}
			if (trans->SSRC) {
				RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, ";ssrc=");
				RTSP_WRITE_HEX(buffer, size, cur_pos, trans->SSRC, 0);
			}
		}
		//done with transport
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	}

	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Unsupported", rsp->Unsupported);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "User-Agent", rsp->User_Agent);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Vary", rsp->Vary);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "Via", rsp->Via);
	RTSP_WRITE_HEADER(buffer, size, cur_pos, "WWW-Authenticate", rsp->WWW_Authenticate);

	//eXtensions
	count = gf_list_count(rsp->Xtensions);
	for (i=0; i<count; i++) {
		GF_X_Attribute *att = (GF_X_Attribute*)gf_list_get(rsp->Xtensions, i);
		RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "x-");
		RTSP_WRITE_HEADER(buffer, size, cur_pos, att->Name, att->Value);
	}
	//end of header
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, "\r\n");
	//then body
	RTSP_WRITE_ALLOC_STR(buffer, size, cur_pos, rsp->body);

	*out_buffer = (unsigned char *) buffer;
	*out_size = (u32) strlen(buffer);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_rtsp_send_response(GF_RTSPSession *sess, GF_RTSPResponse *rsp)
{
	u32 size;
	char *buffer;
	GF_Err e;

	if (!sess || !rsp || !rsp->CSeq) return GF_BAD_PARAM;

	//check we're not sending something greater than the current CSeq
	if (rsp->CSeq > sess->CSeq) return GF_BAD_PARAM;

	e = RTSP_WriteResponse(sess, rsp, (unsigned char **) &buffer, &size);
	if (!e) {
		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTSP] Sending response %s", buffer));
		//send buffer
		e = gf_rtsp_send_data(sess, buffer, size);
	}
	if (buffer) gf_free(buffer);
	return e;
}

#endif /*GPAC_DISABLE_STREAMING*/
