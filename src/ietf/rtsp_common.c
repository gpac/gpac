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


GF_Err gf_rtsp_read_reply(GF_RTSPSession *sess)
{
	GF_Err e;
	u32 res, body_size = 0;
	u32 BodyStart = 0;

	//fetch more data on the socket if needed
	while (1) {
		//Locate header / body
		if (!BodyStart) gf_rtsp_get_body_info(sess, &BodyStart, &body_size);

		if (BodyStart) {
			//enough data
			res = sess->CurrentSize - sess->CurrentPos;
			if (!body_size || (res >= body_size + BodyStart)) {
				//done
				break;
			}
		}
		//this is the tricky part: if we do NOT have a body start -> we refill
		e = gf_rtsp_refill_buffer(sess);
		if (e) return e;
	}
	return GF_OK;
}

void gf_rtsp_get_body_info(GF_RTSPSession *sess, u32 *body_start, u32 *body_size)
{
	u32 i;
	s32 start;
	char *buffer;
	char *cl_str, val[30];

	*body_start = *body_size = 0;

	buffer = sess->tcp_buffer + sess->CurrentPos;
	start = gf_token_find(buffer, 0, sess->CurrentSize - sess->CurrentPos, "\r\n\r\n");
	if (start<=0) {
		return;
	}

	//if found add the 2 "\r\n" and parse it
	*body_start = start + 4;

	//get the content length
	cl_str = strstr(buffer, "Content-Length: ");
	if (!cl_str) cl_str = strstr(buffer, "Content-length: ");

	if (cl_str) {
		cl_str += 16;
		i = 0;
		while (cl_str[i] != '\r') {
			val[i] = cl_str[i];
			i += 1;
		}
		val[i] = 0;
		*body_size = atoi(val);
	} else {
		*body_size = 0;
	}
}


GF_Err gf_rtsp_refill_buffer(GF_RTSPSession *sess)
{
	GF_Err e;
	u32 res;
	char *ptr;

	if (!sess) return GF_BAD_PARAM;
	if (!sess->connection) return GF_IP_NETWORK_EMPTY;

	res = sess->CurrentSize - sess->CurrentPos;
	if (!res) return gf_rtsp_fill_buffer(sess);

	ptr = (char *)gf_malloc(sizeof(char) * res);
	memcpy(ptr, sess->tcp_buffer + sess->CurrentPos, res);
	memcpy(sess->tcp_buffer, ptr, res);
	gf_free(ptr);

	sess->CurrentPos = 0;
	sess->CurrentSize = res;

	//now read from current pos
	e = gf_sk_receive(sess->connection, sess->tcp_buffer + sess->CurrentSize, sess->SockBufferSize - sess->CurrentSize, &res);

	if (!e) {
		sess->CurrentSize += res;
	}
	return e;
}


GF_Err gf_rtsp_fill_buffer(GF_RTSPSession *sess)
{
	GF_Err e = GF_OK;

	if (!sess->connection) return GF_IP_NETWORK_EMPTY;

	if (sess->CurrentSize == sess->CurrentPos) {
		e = gf_sk_receive(sess->connection, sess->tcp_buffer, sess->SockBufferSize, &sess->CurrentSize);
		sess->CurrentPos = 0;
		sess->tcp_buffer[sess->CurrentSize] = 0;
		if (e) sess->CurrentSize = 0;
	} else if (!sess->CurrentSize) e = GF_IP_NETWORK_EMPTY;
	return e;
}


GF_RTSPTransport *gf_rtsp_transport_parse(u8 *buffer)
{
	Bool IsFirst;
	char buf[100], param_name[100], param_val[100];
	s32 pos, nPos;
	u32 v1, v2;
	GF_RTSPTransport *tmp;
	if (!buffer) return NULL;
	//only support for RTP/AVP for now
	if (strnicmp(buffer, "RTP/AVP", 7) && strnicmp(buffer, "RTP/SAVP", 8)) return NULL;

	GF_SAFEALLOC(tmp, GF_RTSPTransport);
	if (!tmp) return NULL;

	IsFirst = GF_TRUE;
	pos = 0;
	while (1) {
		pos = gf_token_get(buffer, pos, " ;", buf, 100);
		if (pos <= 0) break;
		if (strstr(buf, "=")) {
			nPos = gf_token_get(buf, 0, "=", param_name, 100);
			/*nPos = */gf_token_get(buf, nPos, "=", param_val, 100);
		} else {
			strcpy(param_name, buf);
		}

		//very first param is the profile
		if (IsFirst) {
			tmp->Profile = gf_strdup(param_name);
			IsFirst = GF_FALSE;
			continue;
		}

		if (!stricmp(param_name, "destination")) {
			if (tmp->destination) gf_free(tmp->destination);
			tmp->destination = gf_strdup(param_val);
		}
		else if (!stricmp(param_name, "source")) {
			if (tmp->source) gf_free(tmp->source);
			tmp->source = gf_strdup(param_val);
		}
		else if (!stricmp(param_name, "unicast")) tmp->IsUnicast = GF_TRUE;
		else if (!stricmp(param_name, "RECORD")) tmp->IsRecord = GF_TRUE;
		else if (!stricmp(param_name, "append")) tmp->Append = GF_TRUE;
		else if (!stricmp(param_name, "interleaved")) {
			u32 rID, rcID;
			tmp->IsInterleaved = GF_TRUE;
			if (sscanf(param_val, "%u-%u", &rID, &rcID) == 1) {
				sscanf(param_val, "%u", &rID);
				tmp->rtcpID = tmp->rtpID = (u8) rID;
			} else {
				tmp->rtpID = (u8) rID;
				tmp->rtcpID = (u8) rcID;
			}
		}
		else if (!stricmp(param_name, "layers")) sscanf(param_val, "%u", &tmp->MulticastLayers);
		else if (!stricmp(param_name, "ttl")) sscanf(param_val, "%c	", &tmp->TTL);
		else if (!stricmp(param_name, "port")) {
			sscanf(param_val, "%u-%u", &v1, &v2);
			tmp->port_first = (u16) v1;
			tmp->port_last = (u16) v2;
		}
		/*do not use %hud here, broken on Win32 (sscanf returns 1)*/
		else if (!stricmp(param_name, "server_port")) {
			sscanf(param_val, "%d-%d", &v1, &v2);
			tmp->port_first = (u16) v1;
			tmp->port_last = (u16) v2;
		}
		/*do not use %hud here, broken on Win32 (sscanf returns 1)*/
		else if (!stricmp(param_name, "client_port")) {
			sscanf(param_val, "%d-%d", &v1, &v2);
			tmp->client_port_first = (u16) v1;
			tmp->client_port_last = (u16) v2;
		}
		else if (!stricmp(param_name, "ssrc")) sscanf(param_val, "%X", &tmp->SSRC);
	}
	return tmp;
}



GF_Err gf_rtsp_parse_header(u8 *buffer, u32 BufferSize, u32 BodyStart, GF_RTSPCommand *com, GF_RTSPResponse *rsp)
{
	char LineBuffer[1024];
	char HeaderBuf[100], ValBuf[1024], temp[400];
	s32 Pos, LinePos;
	u32 HeaderLine;

	//then parse the full header
	LinePos = 0;
	strcpy(HeaderBuf, "");
	while (1) {
		LinePos = gf_token_get_line(buffer, LinePos, BufferSize, LineBuffer, 1024);
		if (LinePos <= 0) return GF_REMOTE_SERVICE_ERROR;

		//extract field header and value. Warning: some params (transport, ..) may be on several lines
		Pos = gf_token_get(LineBuffer, 0, ":\r\n", temp, 400);

		//end of header
		if (Pos <= 0) {
			HeaderLine = 2;
		}
		//this is a header
		else if (LineBuffer[0] != ' ') {
			HeaderLine = 1;
		} else {
			Pos = gf_token_get(LineBuffer, 0, ", \r\n", temp, 400);
			//end of header - process any pending one
			if (Pos <= 0) {
				HeaderLine = 2;
			} else {
				//n-line value - append
				strcat(ValBuf, "\r\n");
				strcat(ValBuf, temp);
				continue;
			}
		}
		//process current value
		if (HeaderLine && strlen(HeaderBuf)) {
			if (rsp) {
				gf_rtsp_set_response_value(rsp, HeaderBuf, ValBuf);
			}
			else {
				gf_rtsp_set_command_value(com, HeaderBuf, ValBuf);
			}
		}
		//done with the header
		if ( (HeaderLine == 2) || ((u32) LinePos >= BodyStart) ) return GF_OK;

		//process current line
		strcpy(HeaderBuf, temp);

		//skip ':'
		Pos += 1;
		//a server should normally reply with a space, but check it
		if (LineBuffer[Pos] == ' ') Pos += 1;
		/*!! empty value !! - DSS may send these for CSeq if something goes wrong*/
		if (!strcmp(LineBuffer+Pos, "\r\n")) {
			HeaderBuf[0] = 0;
			continue;
		}
		Pos = gf_token_get(LineBuffer, Pos, "\r\n", ValBuf, 400);
		if (Pos <= 0) break;

	}
	//if we get here we haven't reached the BodyStart
	return GF_REMOTE_SERVICE_ERROR;
}


GF_EXPORT
const char *gf_rtsp_nc_to_string(u32 ErrCode)
{
	switch (ErrCode) {
	case NC_RTSP_Continue:
		return "Continue";
	case NC_RTSP_OK:
		return "OK";
	case NC_RTSP_Created:
		return "Created";
	case NC_RTSP_Low_on_Storage_Space:
		return "Low on Storage Space";
	case NC_RTSP_Multiple_Choice:
		return "Multiple Choice";
	case NC_RTSP_Moved_Permanently:
		return "Moved Permanently";
	case NC_RTSP_Moved_Temporarily:
		return "Moved Temporarily";
	case NC_RTSP_See_Other:
		return "See Other";
	case NC_RTSP_Use_Proxy:
		return "Use Proxy";
	case NC_RTSP_Bad_Request:
		return "Bad Request";
	case NC_RTSP_Unauthorized:
		return "Unauthorized";
	case NC_RTSP_Payment_Required:
		return "Payment Required";
	case NC_RTSP_Forbidden:
		return "Forbidden";
	case NC_RTSP_Not_Found:
		return "Not Found";
	case NC_RTSP_Method_Not_Allowed:
		return "Method Not Allowed";
	case NC_RTSP_Not_Acceptable:
		return "Not Acceptable";
	case NC_RTSP_Proxy_Authentication_Required:
		return "Proxy Authentication Required";
	case NC_RTSP_Request_Timeout:
		return "Request Timeout";
	case NC_RTSP_Gone:
		return "Gone";
	case NC_RTSP_Length_Required:
		return "Length Required";
	case NC_RTSP_Precondition_Failed:
		return "Precondition Failed";
	case NC_RTSP_Request_Entity_Too_Large:
		return "Request Entity Too Large";
	case NC_RTSP_Request_URI_Too_Long:
		return "Request URI Too Long";
	case NC_RTSP_Unsupported_Media_Type:
		return "Unsupported Media Type";
	case NC_RTSP_Invalid_parameter:
		return "Invalid parameter";
	case NC_RTSP_Illegal_Conference_Identifier:
		return "Illegal Conference Identifier";
	case NC_RTSP_Not_Enough_Bandwidth:
		return "Not Enough Bandwidth";
	case NC_RTSP_Session_Not_Found:
		return "Session Not Found";
	case NC_RTSP_Method_Not_Valid_In_This_State:
		return "Method Not Valid In This State";
	case NC_RTSP_Header_Field_Not_Valid:
		return "Header Field Not Valid";
	case NC_RTSP_Invalid_Range:
		return "Invalid Range";
	case NC_RTSP_Parameter_Is_ReadOnly:
		return "Parameter Is Read-Only";
	case NC_RTSP_Aggregate_Operation_Not_Allowed:
		return "Aggregate Operation Not Allowed";
	case NC_RTSP_Only_Aggregate_Operation_Allowed:
		return "Only Aggregate Operation Allowed";
	case NC_RTSP_Unsupported_Transport:
		return "Unsupported Transport";
	case NC_RTSP_Destination_Unreachable:
		return "Destination Unreachable";
	case NC_RTSP_Internal_Server_Error:
		return "Internal Server Error";
	case NC_RTSP_Bad_Gateway:
		return "Bad Gateway";
	case NC_RTSP_Service_Unavailable:
		return "Service Unavailable";
	case NC_RTSP_Gateway_Timeout:
		return "Gateway Timeout";
	case NC_RTSP_RTSP_Version_Not_Supported:
		return "RTSP Version Not Supported";
	case NC_RTSP_Option_not_support:
		return "Option not support";

	case NC_RTSP_Not_Implemented:
	default:
		return "Not Implemented";
	}
}

#endif /*GPAC_DISABLE_STREAMING*/
