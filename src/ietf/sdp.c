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



#include <gpac/ietf.h>

#ifndef GPAC_DISABLE_STREAMING

#include <gpac/token.h>


#define SDP_WRITE_STEPALLOC		2048


GF_EXPORT
GF_SDP_FMTP *gf_sdp_fmtp_new()
{
	GF_SDP_FMTP *tmp = (GF_SDP_FMTP*)gf_malloc(sizeof(GF_SDP_FMTP));
	tmp->PayloadType = 0;
	tmp->Attributes = gf_list_new();
	return tmp;
}

GF_EXPORT
void gf_sdp_fmtp_del(GF_SDP_FMTP *fmtp)
{
	if (!fmtp) return;
	while (gf_list_count(fmtp->Attributes)) {
		GF_X_Attribute *att = (GF_X_Attribute*)gf_list_get(fmtp->Attributes, 0);
		gf_list_rem(fmtp->Attributes, 0);
		if (att->Name) gf_free(att->Name);
		if (att->Value) gf_free(att->Value);
		gf_free(att);
	}
	gf_list_del(fmtp->Attributes);
	gf_free(fmtp);
}

GF_SDP_FMTP *SDP_GetFMTPForPayload(GF_SDPMedia *media, u32 PayloadType)
{
	GF_SDP_FMTP *tmp;
	u32 i;
	if (!media) return NULL;
	i=0;
	while ((tmp = (GF_SDP_FMTP*)gf_list_enum(media->FMTP, &i))) {
		if (tmp->PayloadType == PayloadType) return tmp;
	}
	return NULL;
}

void SDP_ParseAttribute(GF_SDPInfo *sdp, char *buffer, GF_SDPMedia *media)
{
	s32 pos;
	u32 PayT;
	char comp[3000];
	GF_X_Attribute *att;

	pos = gf_token_get(buffer, 0, " :\t\r\n", comp, 3000);

	if (!strcmp(comp, "cat")) {
		if (media) return;
		/*pos = */gf_token_get(buffer, pos, ":\t\r\n", comp, 3000);
		if (sdp->a_cat) return;
		sdp->a_cat = gf_strdup(comp);
		return;
	}
	if (!strcmp(comp, "keywds")) {
		if (media) return;
		/*pos = */gf_token_get(buffer, pos, ":\t\r\n", comp, 3000);
		if (sdp->a_keywds) return;
		sdp->a_keywds = gf_strdup(comp);
		return;
	}
	if (!strcmp(comp, "tool")) {
		if (media) return;
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		if (sdp->a_tool) return;
		sdp->a_tool = gf_strdup(comp);
		return;
	}

	if (!strcmp(comp, "ptime")) {
		if (!media) return;
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		media->PacketTime = atoi(comp);
		return;
	}
	if (!strcmp(comp, "recvonly")) {
		if (!media) {
			sdp->a_SendReceive = 1;
		} else {
			media->SendReceive = 1;
		}
		return;
	}
	if (!strcmp(comp, "sendonly")) {
		if (!media) {
			sdp->a_SendReceive = 2;
		} else {
			media->SendReceive = 2;
		}
		return;
	}
	if (!strcmp(comp, "sendrecv")) {
		if (!media) {
			sdp->a_SendReceive = 3;
		} else {
			media->SendReceive = 3;
		}
		return;
	}
	if (!strcmp(comp, "orient")) {
		if (!media || media->Type) return;
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		if (media->orientation) return;
		media->orientation = gf_strdup(comp);
		return;
	}
	if (!strcmp(comp, "type")) {
		if (media) return;
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		if (sdp->a_type) return;
		sdp->a_type = gf_strdup(comp);
		return;
	}
	if (!strcmp(comp, "charset")) {
		if (media) return;
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		if (sdp->a_charset)
		sdp->a_charset = gf_strdup(comp);
		return;
	}
	if (!strcmp(comp, "sdplang")) {
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		if (media) {
			if (media->sdplang) return;
			media->sdplang = gf_strdup(comp);
		} else {
			if (sdp->a_sdplang) return;
			sdp->a_sdplang = gf_strdup(comp);
		}
		return;
	}
	if (!strcmp(comp, "lang")) {
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		if (media) {
			if (media->lang) return;
			media->lang = gf_strdup(comp);
		} else {
			if (sdp->a_lang) return;
			sdp->a_lang = gf_strdup(comp);
		}
		return;
	}
	if (!strcmp(comp, "framerate")) {
		//only for video
		if (!media || (media->Type != 1)) return;
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		media->FrameRate = atof(comp);
		return;
	}
	if (!strcmp(comp, "quality")) {
		if (!media) return;
		/*pos = */gf_token_get(buffer, pos, ":\r\n", comp, 3000);
		media->Quality = atoi(comp);
		return;
	}
	if (!strcmp(comp, "rtpmap")) {
		GF_RTPMap *map;
		if (!media) return;
		map = (GF_RTPMap*)gf_malloc(sizeof(GF_RTPMap));
		pos = gf_token_get(buffer, pos, ": \r\n", comp, 3000);
		map->PayloadType = atoi(comp);
		pos = gf_token_get(buffer, pos, " /\r\n", comp, 3000);
		map->payload_name = gf_strdup(comp);
		pos = gf_token_get(buffer, pos, " /\r\n", comp, 3000);
		map->ClockRate = atoi(comp);
		pos = gf_token_get(buffer, pos, " /\r\n", comp, 3000);
		map->AudioChannels = (pos > 0) ? atoi(comp) : 0;
		gf_list_add(media->RTPMaps, map);
		return;
	}
	//FMTP
	if (!strcmp(comp, "fmtp")) {
		GF_SDP_FMTP *fmtp;
		if (!media) return;
		pos = gf_token_get(buffer, pos, ": \r\n", comp, 3000);
		PayT = atoi(comp);
		fmtp = SDP_GetFMTPForPayload(media, PayT);
		if (!fmtp) {
			fmtp = gf_sdp_fmtp_new();
			fmtp->PayloadType = PayT;
			gf_list_add(media->FMTP, fmtp);
		}
		while (1) {
			pos = gf_token_get(buffer, pos, "; =\r\n", comp, 3000);
			if (pos <= 0) break;
			att = (GF_X_Attribute*)gf_malloc(sizeof(GF_X_Attribute));
			att->Name = gf_strdup(comp);
			att->Value = NULL;
			pos ++;
			pos = gf_token_get(buffer, pos, ";\r\n", comp, 3000);
			if (pos > 0) att->Value = gf_strdup(comp);
			gf_list_add(fmtp->Attributes, att);
		}
		return;
	}
	//the rest cannot be discarded that way as it may be application-specific
	//so keep it.
	//a= <attribute> || <attribute>:<value>
	//we add <attribute> <value> in case ...
	pos = gf_token_get(buffer, 0, " :\r\n", comp, 3000);
	att = (GF_X_Attribute*)gf_malloc(sizeof(GF_X_Attribute));
	att->Name = gf_strdup(comp);
	att->Value = NULL;

	if (pos+1 < (s32)strlen(buffer)) {
		pos += 1;
		if (buffer[pos] == ' ') pos += 1;
	}

	pos = gf_token_get(buffer, pos, "\r\n", comp, 3000);
	if (pos > 0) att->Value = gf_strdup(comp);

	if (media) {
		gf_list_add(media->Attributes, att);
	} else {
		gf_list_add(sdp->Attributes, att);
	}
}



#define SDPM_DESTROY(p) if (media->p) gf_free(media->p)
GF_EXPORT
void gf_sdp_media_del(GF_SDPMedia *media)
{
	if (!media) return;

	while (gf_list_count(media->FMTP)) {
		GF_SDP_FMTP *fmtp = (GF_SDP_FMTP*)gf_list_get(media->FMTP, 0);
		gf_list_rem(media->FMTP, 0);
		gf_sdp_fmtp_del(fmtp);
	}
	gf_list_del(media->FMTP);

	while (gf_list_count(media->Attributes)) {
		GF_X_Attribute *att = (GF_X_Attribute*)gf_list_get(media->Attributes, 0);
		gf_list_rem(media->Attributes, 0);
		if (att->Name) gf_free(att->Name);
		if (att->Value) gf_free(att->Value);
		gf_free(att);
	}
	gf_list_del(media->Attributes);

	while (gf_list_count(media->RTPMaps)) {
		GF_RTPMap *map = (GF_RTPMap*)gf_list_get(media->RTPMaps, 0);
		gf_free(map->payload_name);
		gf_free(map);
		gf_list_rem(media->RTPMaps, 0);
	}
	gf_list_del(media->RTPMaps);

	while (gf_list_count(media->Connections)) {
		GF_SDPConnection *conn = (GF_SDPConnection*)gf_list_get(media->Connections, 0);
		gf_list_rem(media->Connections, 0);
		gf_sdp_conn_del(conn);
	}
	gf_list_del(media->Connections);

	while (gf_list_count(media->Bandwidths)) {
		GF_SDPBandwidth *bw = (GF_SDPBandwidth*)gf_list_get(media->Bandwidths, 0);
		gf_list_rem(media->Bandwidths, 0);
		if (bw->name) gf_free(bw->name);
		gf_free(bw);
	}
	gf_list_del(media->Bandwidths);

	SDPM_DESTROY(orientation);
	SDPM_DESTROY(sdplang);
	SDPM_DESTROY(lang);
	SDPM_DESTROY(Profile);
	SDPM_DESTROY(fmt_list);
	SDPM_DESTROY(k_method);
	SDPM_DESTROY(k_key);
	gf_free(media);
}


GF_EXPORT
GF_SDPConnection *gf_sdp_conn_new()
{
	GF_SDPConnection *conn;
	GF_SAFEALLOC(conn, GF_SDPConnection);
	if (!conn) return NULL;
	conn->TTL = -1;
	return conn;
}

GF_EXPORT
void gf_sdp_conn_del(GF_SDPConnection *conn)
{
	if (conn->add_type) gf_free(conn->add_type);
	if (conn->host) gf_free(conn->host);
	if (conn->net_type) gf_free(conn->net_type);
	gf_free(conn);
}

GF_EXPORT
GF_SDPMedia *gf_sdp_media_new()
{
	GF_SDPMedia *tmp;
	GF_SAFEALLOC(tmp, GF_SDPMedia);
	if (!tmp) return NULL;
	tmp->FMTP = gf_list_new();
	tmp->RTPMaps = gf_list_new();
	tmp->Attributes = gf_list_new();
	tmp->Connections = gf_list_new();
	tmp->Bandwidths = gf_list_new();
	tmp->Quality = -1;
	return tmp;
}

GF_EXPORT
GF_SDPInfo *gf_sdp_info_new()
{
	GF_SDPInfo *sdp;
	GF_SAFEALLOC(sdp, GF_SDPInfo);
	if (!sdp) return NULL;
	sdp->b_bandwidth = gf_list_new();
	sdp->media_desc = gf_list_new();
	sdp->Attributes = gf_list_new();
	sdp->Timing = gf_list_new();
	return sdp;
}

#define SDP_DESTROY(p) if (sdp->p) { gf_free(sdp->p); sdp->p = NULL; }


GF_EXPORT
void gf_sdp_info_reset(GF_SDPInfo *sdp)
{
	if (!sdp) return;

	while (gf_list_count(sdp->media_desc)) {
		GF_SDPMedia *media = (GF_SDPMedia*)gf_list_get(sdp->media_desc, 0);
		gf_list_rem(sdp->media_desc, 0);
		gf_sdp_media_del(media);
	}
	while (gf_list_count(sdp->Attributes)) {
		GF_X_Attribute *att = (GF_X_Attribute*)gf_list_get(sdp->Attributes, 0);
		gf_list_rem(sdp->Attributes, 0);
		if (att->Name) gf_free(att->Name);
		if (att->Value) gf_free(att->Value);
		gf_free(att);
	}
	while (gf_list_count(sdp->b_bandwidth)) {
		GF_SDPBandwidth *bw = (GF_SDPBandwidth*)gf_list_get(sdp->b_bandwidth, 0);
		gf_list_rem(sdp->b_bandwidth, 0);
		if (bw->name) gf_free(bw->name);
		gf_free(bw);
	}
	while (gf_list_count(sdp->Timing)) {
		GF_SDPTiming *timing = (GF_SDPTiming*)gf_list_get(sdp->Timing, 0);
		gf_list_rem(sdp->Timing, 0);
		gf_free(timing);
	}

	//then delete all info ...
	SDP_DESTROY(o_username);
	SDP_DESTROY(o_session_id);
	SDP_DESTROY(o_version);
	SDP_DESTROY(o_address);
	SDP_DESTROY(o_net_type);
	SDP_DESTROY(o_add_type);
	SDP_DESTROY(s_session_name);
	SDP_DESTROY(i_description);
	SDP_DESTROY(u_uri);
	SDP_DESTROY(e_email);
	SDP_DESTROY(p_phone);
	SDP_DESTROY(k_method);
	SDP_DESTROY(k_key);
	SDP_DESTROY(a_cat);
	SDP_DESTROY(a_keywds);
	SDP_DESTROY(a_tool);
	SDP_DESTROY(a_type);
	SDP_DESTROY(a_charset);
	SDP_DESTROY(a_sdplang);
	SDP_DESTROY(a_lang);

	if (sdp->c_connection) {
		gf_sdp_conn_del(sdp->c_connection);
		sdp->c_connection = NULL;
	}
	sdp->a_SendReceive = 0;
}

GF_EXPORT
void gf_sdp_info_del(GF_SDPInfo *sdp)
{
	if (!sdp) return;
	gf_sdp_info_reset(sdp);
	gf_list_del(sdp->media_desc);
	gf_list_del(sdp->Attributes);
	gf_list_del(sdp->b_bandwidth);
	gf_list_del(sdp->Timing);
	gf_free(sdp);
}


Bool SDP_IsDynamicPayload(GF_SDPMedia *media, char *payt)
{
	u32 i;
	GF_RTPMap *map;
	char buf[10];
	i=0;
	while ((map = (GF_RTPMap*)gf_list_enum(media->RTPMaps, &i))) {
		sprintf(buf, "%d", map->PayloadType);
		if (!strcmp(payt, buf)) return GF_TRUE;
	}
	return GF_FALSE;
}

//translate h || m || d in sec. Fractions are not allowed with this writing
static s32 SDP_MakeSeconds(char *buf)
{
	s32 sign;
	char num[30], *test;
	sign = 1;
	if (buf[0] == '-') {
		sign = -1;
		buf += 1;
	}
	memset(num, 0, 30);
	test = strstr(buf, "d");
	if (test) {
		if (strlen(buf)-strlen(test) < sizeof(num))
			memcpy(num, buf, MIN(sizeof(num)-1, strlen(buf)-strlen(test)));
		else
			gf_assert(0);
		return (atoi(num)*sign*86400);
	}
	test = strstr(buf, "h");
	if (test) {
		if (strlen(buf)-strlen(test) < sizeof(num))
			memcpy(num, buf, MIN(sizeof(num)-1, strlen(buf)-strlen(test)));
		else
			gf_assert(0);
		return (atoi(num)*sign*3600);
	}
	test = strstr(buf, "m");
	if (test) {
		if (strlen(buf)-strlen(test) < sizeof(num))
			memcpy(num, buf, MIN(sizeof(num)-1, strlen(buf)-strlen(test)));
		else
			gf_assert(0);
		return (atoi(num)*sign*60);
	}
	return (atoi(buf) * sign);
}


GF_EXPORT
GF_Err gf_sdp_info_parse(GF_SDPInfo *sdp, char *sdp_text, u32 text_size)
{
	GF_SDPBandwidth *bw;
	GF_SDPConnection *conn;
	GF_SDPMedia *media;
	GF_SDPTiming *timing;
	u32 i;
	s32 pos, LinePos;
	char LineBuf[3000], comp[3000];

	media = NULL;
	timing = NULL;

	if (!sdp) return GF_BAD_PARAM;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		SDP_MakeSeconds("30m");
	}
#endif

	//Clean SDP info
	gf_sdp_info_reset(sdp);

	LinePos = 0;
	while (1) {
		LinePos = gf_token_get_line(sdp_text, LinePos, text_size, LineBuf, 3000);
		if (LinePos <= 0) break;
		if (!strcmp(LineBuf, "\r\n") || !strcmp(LineBuf, "\n") || !strcmp(LineBuf, "\r")) continue;

		pos=0;
		switch (LineBuf[0]) {
		case 'v':
			/*pos = */gf_token_get(LineBuf, 2, "\t\r\n", comp, 3000);
			sdp->Version = atoi(comp);
			break;
		case 'o':
			//only use first one
			if (sdp->o_username) break;
			pos = gf_token_get(LineBuf, 2, " \t\r\n", comp, 3000);
			sdp->o_username = gf_strdup(comp);
			pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
			sdp->o_session_id = gf_strdup(comp);
			pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
			sdp->o_version = gf_strdup(comp);

			pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
			sdp->o_net_type = gf_strdup(comp);

			pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
			sdp->o_add_type = gf_strdup(comp);

			/*pos = */gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
			sdp->o_address = gf_strdup(comp);
			break;
		case 's':
			if (sdp->s_session_name) break;
			/*pos = */gf_token_get(LineBuf, 2, "\t\r\n", comp, 3000);
			sdp->s_session_name = gf_strdup(comp);
			break;
		case 'i':
			if (sdp->i_description) break;
			/*pos = */gf_token_get(LineBuf, 2, "\t\r\n", comp, 3000);
			sdp->i_description = gf_strdup(comp);
			break;
		case 'u':
			if (sdp->u_uri) break;
			/*pos = */gf_token_get(LineBuf, 2, "\t\r\n", comp, 3000);
			sdp->u_uri = gf_strdup(comp);
			break;
		case 'e':
			if (sdp->e_email) break;
			/*pos = */gf_token_get(LineBuf, 2, "\t\r\n", comp, 3000);
			sdp->e_email = gf_strdup(comp);
			break;
		case 'p':
			if (sdp->p_phone) break;
			/*pos = */gf_token_get(LineBuf, 2, "\t\r\n", comp, 3000);
			sdp->p_phone = gf_strdup(comp);
			break;
		case 'c':
			//if at session level, only 1 is allowed for all SDP
			if (sdp->c_connection) break;

			conn = gf_sdp_conn_new();

			pos = gf_token_get(LineBuf, 2, " \t\r\n", comp, 3000);
			conn->net_type = gf_strdup(comp);

			pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
			conn->add_type = gf_strdup(comp);

			pos = gf_token_get(LineBuf, pos, " /\r\n", comp, 3000);
			conn->host = gf_strdup(comp);
			if (gf_sk_is_multicast_address(conn->host)) {
				//a valid SDP will have TTL if address is multicast
				pos = gf_token_get(LineBuf, pos, "/\r\n", comp, 3000);
				if (pos > 0) {
					conn->TTL = atoi(comp);
					//multiple address indication is only valid for media
					pos = gf_token_get(LineBuf, pos, "/\r\n", comp, 3000);
				}
				if (pos > 0) {
					if (!media) {
						gf_sdp_conn_del(conn);
						break;
					}
					conn->add_count = atoi(comp);
				}
			}
			if (!media)
				sdp->c_connection = conn;
			else
				gf_list_add(media->Connections, conn);

			break;
		case 'b':
			pos = gf_token_get(LineBuf, 2, ":\r\n", comp, 3000);
			if (strcmp(comp, "CT") && strcmp(comp, "AS") && (comp[0] != 'X')) break;

			GF_SAFEALLOC(bw, GF_SDPBandwidth);
			if (!bw) return GF_OUT_OF_MEM;
			bw->name = gf_strdup(comp);
			/*pos = */gf_token_get(LineBuf, pos, ":\r\n", comp, 3000);
			bw->value = atoi(comp);
			if (media) {
				gf_list_add(media->Bandwidths, bw);
			} else {
				gf_list_add(sdp->b_bandwidth, bw);
			}
			break;

		case 't':
			if (media) break;
			//create a new time structure for each entry
			GF_SAFEALLOC(timing, GF_SDPTiming);
			if (!timing) return GF_OUT_OF_MEM;
			pos = gf_token_get(LineBuf, 2, " \t\r\n", comp, 3000);
			timing->StartTime = atoi(comp);
			/*pos = */gf_token_get(LineBuf, pos, "\r\n", comp, 3000);
			timing->StopTime = atoi(comp);
			gf_list_add(sdp->Timing, timing);
			break;
		case 'r':
			if (media) break;
			pos = gf_token_get(LineBuf, 2, " \t\r\n", comp, 3000);
			if (!timing) return GF_NON_COMPLIANT_BITSTREAM;
			timing->RepeatInterval = SDP_MakeSeconds(comp);
			pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
			timing->ActiveDuration = SDP_MakeSeconds(comp);
			while (pos>=0) {
				if (timing->NbRepeatOffsets == GF_SDP_MAX_TIMEOFFSET) break;
				pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
				if (pos <= 0) break;
				timing->OffsetFromStart[timing->NbRepeatOffsets] = SDP_MakeSeconds(comp);
				timing->NbRepeatOffsets += 1;
			}
			break;
		case 'z':
			if (media) break;
			pos = 2;
			if (!timing) return GF_NON_COMPLIANT_BITSTREAM;
			while (1) {
				pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
				if (pos <= 0) break;
				if (timing->NbZoneOffsets >= GF_SDP_MAX_TIMEOFFSET) break;
				timing->AdjustmentTime[timing->NbZoneOffsets] = atoi(comp);
				pos = gf_token_get(LineBuf, pos, " \t\r\n", comp, 3000);
				timing->AdjustmentOffset[timing->NbZoneOffsets] = SDP_MakeSeconds(comp);
				timing->NbZoneOffsets += 1;
			}
			break;
		case 'k':
			if (sdp->k_method) break;
			pos = gf_token_get(LineBuf, 2, ":\t\r\n", comp, 3000);
			if (media) {
				media->k_method = gf_strdup(comp);
			} else {
				sdp->k_method = gf_strdup(comp);
			}
			pos = gf_token_get(LineBuf, pos, ":\r\n", comp, 3000);
			if (pos > 0) {
				if (media) {
					media->k_key = gf_strdup(comp);
				} else {
					sdp->k_key = gf_strdup(comp);
				}
			}
			break;
		case 'a':
			SDP_ParseAttribute(sdp, LineBuf+2, media);
			break;
		case 'm':
			pos = gf_token_get(LineBuf, 2, " \t\r\n", comp, 3000);
			if (strcmp(comp, "audio")
			        && strcmp(comp, "data")
			        && strcmp(comp, "control")
			        && strcmp(comp, "video")
			        && strcmp(comp, "text")
			        && strcmp(comp, "application")) {
				return GF_SERVICE_ERROR;
			}
			media = gf_sdp_media_new();
			//media type
			if (!strcmp(comp, "video")) media->Type = 1;
			else if (!strcmp(comp, "audio")) media->Type = 2;
			else if (!strcmp(comp, "text")) media->Type = 3;
			else if (!strcmp(comp, "data")) media->Type = 4;
			else if (!strcmp(comp, "control")) media->Type = 5;
			else media->Type = 0;
			//port numbers
			gf_token_get(LineBuf, pos, " ", comp, 3000);
			if (!strstr(comp, "/")) {
				pos = gf_token_get(LineBuf, pos, " \r\n", comp, 3000);
				media->PortNumber = atoi(comp);
				media->NumPorts = 0;
			} else {
				pos = gf_token_get(LineBuf, pos, " /\r\n", comp, 3000);
				media->PortNumber = atoi(comp);
				pos = gf_token_get(LineBuf, pos, " \r\n", comp, 3000);
				media->NumPorts = atoi(comp);
			}
			//transport Profile
			pos = gf_token_get(LineBuf, pos, " \r\n", comp, 3000);
			media->Profile = gf_strdup(comp);
			/*pos = */gf_token_get(LineBuf, pos, " \r\n", comp, 3000);
			media->fmt_list = gf_strdup(comp);

			gf_list_add(sdp->media_desc, media);
			break;
		}

		if (pos<0)
			return GF_NON_COMPLIANT_BITSTREAM;
	}
	//finally rewrite the fmt_list for all media, and remove dynamic payloads
	//from the list
	i=0;
	while ((media = (GF_SDPMedia*)gf_list_enum(sdp->media_desc, &i))) {
		pos = 0;
		LinePos = 1;
		strcpy(LineBuf, "");
		while (1) {
			if (!media->fmt_list) break;
			pos = gf_token_get(media->fmt_list, pos, " ", comp, 3000);
			if (pos <= 0) break;
			if (!SDP_IsDynamicPayload(media, comp)) {
				if (!LinePos) {
					strcat(LineBuf, " ");
				} else {
					LinePos = 0;
				}
				strcat(LineBuf, comp);
			}
			gf_free(media->fmt_list);
			media->fmt_list = NULL;
			if (strlen(LineBuf)) {
				media->fmt_list = gf_strdup(LineBuf);
			}
		}
	}
	return GF_OK;
}


#if 0 //unused

static GF_Err SDP_CheckConnection(GF_SDPConnection *conn)
{
	if (!conn) return GF_BAD_PARAM;
	if (!conn->host || !conn->add_type || !conn->net_type) return GF_REMOTE_SERVICE_ERROR;
	if (gf_sk_is_multicast_address(conn->host)) {
		if (conn->TTL < 0 || conn->TTL > 255) return GF_REMOTE_SERVICE_ERROR;
	} else {
		conn->TTL = -1;
		conn->add_count = 0;
	}
	return GF_OK;
}

//return GF_BAD_PARAM if invalid structure, GF_REMOTE_SERVICE_ERROR if bad formatting
//or GF_OK
GF_Err gf_sdp_info_check(GF_SDPInfo *sdp)
{
	GF_Err e;
	u32 i, j, count;
	GF_SDPMedia *media;
	GF_SDPConnection *conn;
	GF_RTPMap *map;
	Bool HasGlobalConnection, HasSeveralPorts;

	if (!sdp || !sdp->media_desc || !sdp->Attributes) return GF_BAD_PARAM;
	//we force at least one media per SDP
	if (!gf_list_count(sdp->media_desc)) return GF_REMOTE_SERVICE_ERROR;

	//normative fields
	//o=
	if (!sdp->o_add_type || !sdp->o_address || !sdp->o_username || !sdp->o_session_id || !sdp->o_version)
		return GF_REMOTE_SERVICE_ERROR;
	//s=
	//commented for intermedia demos
//	if (!sdp->s_session_name) return GF_REMOTE_SERVICE_ERROR;
	//t=
//	if () return GF_REMOTE_SERVICE_ERROR;
	//c=
	if (sdp->c_connection) {
		e = SDP_CheckConnection(sdp->c_connection);
		if (e) return e;
		//multiple addresses are only for media desc
		if (sdp->c_connection->add_count >= 2) return GF_REMOTE_SERVICE_ERROR;
		HasGlobalConnection = GF_TRUE;
	} else {
		HasGlobalConnection = GF_FALSE;
	}

	//then check all media
	i=0;
	while ((media = (GF_SDPMedia*)gf_list_enum(sdp->media_desc, &i))) {
		HasSeveralPorts = GF_FALSE;

		//m= : force non-null port, profile and fmt_list
		if (/*!media->PortNumber || */ !media->Profile) return GF_REMOTE_SERVICE_ERROR;
		if (media->NumPorts) HasSeveralPorts = GF_TRUE;

		//no connections specified - THIS IS AN ERROR IN SDP BUT NOT IN ALL RTSP SESSIONS...
//		if (!HasGlobalConnection && !gf_list_count(media->Connections)) return GF_REMOTE_SERVICE_ERROR;
		//too many connections specified
		if (HasGlobalConnection && gf_list_count(media->Connections)) return GF_REMOTE_SERVICE_ERROR;

		//check all connections, and make sure we don't have multiple addresses
		//and multiple ports at the same time
		count = gf_list_count(media->Connections);
		if (count>1 && HasSeveralPorts) return GF_REMOTE_SERVICE_ERROR;

		for (j=0; j<count; j++) {
			conn = (GF_SDPConnection*)gf_list_get(media->Connections, j);
			e = SDP_CheckConnection(conn);
			if (e) return e;
			if ((conn->add_count >= 2) && HasSeveralPorts) return GF_REMOTE_SERVICE_ERROR;
		}
		//RTPMaps. 0 is tolerated, but if some are specified check them
		j=0;
		while ((map = (GF_RTPMap*)gf_list_enum(media->RTPMaps, &j))) {
			//RFC2327 is not clear here, but we assume the PayloadType should be a DYN one
			//however this depends on the profile (RTP/AVP or others) so don't check it
			//ClockRate SHALL NOT be NULL
			if (!map->payload_name || !map->ClockRate) return GF_REMOTE_SERVICE_ERROR;
		}
	}
	//Encryption: nothing tells whether the scope of the global key is eclusive or not.
	//we accept a global key + keys per media entry, assuming that the media key primes
	//on the global key

	return GF_OK;
}


#define SDP_WRITE_ALLOC_STR_WITHOUT_CHECK(str, space)		\
	if (strlen(str)+pos + (space ? 1 : 0) >= buf_size) {	\
		buf_size += SDP_WRITE_STEPALLOC;	\
		buf = (char*)gf_realloc(buf, sizeof(char)*buf_size);		\
	}	\
	strcpy(buf+pos, str);		\
	pos += (u32) strlen(str);		\
	if (space) {			\
		strcat(buf+pos, " ");	\
		pos += 1;		\
	}

#define SDP_WRITE_ALLOC_STR(str, space)		\
	if (str) { \
		SDP_WRITE_ALLOC_STR_WITHOUT_CHECK(str, space); \
	}		\

#define SDP_WRITE_ALLOC_INT(d, spa, sig)		\
	if (sig < 0) { \
		sprintf(temp, "%d", d);		\
	} else { \
		sprintf(temp, "%u", d);		\
	}	\
	SDP_WRITE_ALLOC_STR_WITHOUT_CHECK(temp, spa);

#define SDP_WRITE_ALLOC_FLOAT(d, spa)		\
	sprintf(temp, "%.2f", d);		\
	SDP_WRITE_ALLOC_STR_WITHOUT_CHECK(temp, spa);

#define TEST_SDP_WRITE_SINGLE(type, str, sep)		\
	if (str) {		\
		SDP_WRITE_ALLOC_STR(type, 0);		\
		if (sep) SDP_WRITE_ALLOC_STR(":", 0);		\
		SDP_WRITE_ALLOC_STR(str, 0);		\
		SDP_WRITE_ALLOC_STR("\r\n", 0);		\
	}


#define SDP_WRITE_CONN(conn)		\
	if (conn) {			\
		SDP_WRITE_ALLOC_STR("c=", 0);	\
		SDP_WRITE_ALLOC_STR(conn->net_type, 1);		\
		SDP_WRITE_ALLOC_STR(conn->add_type, 1);		\
		SDP_WRITE_ALLOC_STR(conn->host, 0);			\
		if (gf_sk_is_multicast_address(conn->host)) {	\
			SDP_WRITE_ALLOC_STR("/", 0);			\
			SDP_WRITE_ALLOC_INT(conn->TTL, 0, 0);		\
			if (conn->add_count >= 2) {		\
				SDP_WRITE_ALLOC_STR("/", 0);		\
				SDP_WRITE_ALLOC_INT(conn->add_count, 0, 0);	\
			}		\
		}	\
		SDP_WRITE_ALLOC_STR("\r\n", 0);		\
	}

GF_Err gf_sdp_info_write(GF_SDPInfo *sdp, char **out_str_buf)
{
	char *buf;
	GF_SDP_FMTP *fmtp;
	char temp[50];
	GF_SDPMedia *media;
	GF_SDPBandwidth *bw;
	u32 buf_size, pos, i, j, k;
	GF_RTPMap *map;
	GF_SDPConnection *conn;
	GF_Err e;
	GF_SDPTiming *timing;
	GF_X_Attribute *att;

	e = gf_sdp_info_check(sdp);
	if (e) return e;

	buf = (char *)gf_malloc(SDP_WRITE_STEPALLOC);
	buf_size = SDP_WRITE_STEPALLOC;
	pos = 0;

	//v
	SDP_WRITE_ALLOC_STR("v=", 0);
	SDP_WRITE_ALLOC_INT(sdp->Version, 0, 0);
	SDP_WRITE_ALLOC_STR("\r\n", 0);
	//o
	SDP_WRITE_ALLOC_STR("o=", 0);
	SDP_WRITE_ALLOC_STR(sdp->o_username, 1);
	SDP_WRITE_ALLOC_STR(sdp->o_session_id, 1);
	SDP_WRITE_ALLOC_STR(sdp->o_version, 1);
	SDP_WRITE_ALLOC_STR(sdp->o_net_type, 1);
	SDP_WRITE_ALLOC_STR(sdp->o_add_type, 1);
	SDP_WRITE_ALLOC_STR(sdp->o_address, 0);
	SDP_WRITE_ALLOC_STR("\r\n", 0);
	//s
	TEST_SDP_WRITE_SINGLE("s=", sdp->s_session_name, 0);
	//i
	TEST_SDP_WRITE_SINGLE("i=", sdp->i_description, 0);
	//u
	TEST_SDP_WRITE_SINGLE("u=", sdp->u_uri, 0);
	//e
	TEST_SDP_WRITE_SINGLE("e=", sdp->e_email, 0);
	//p
	TEST_SDP_WRITE_SINGLE("p=", sdp->p_phone, 0);
	//c
	SDP_WRITE_CONN(sdp->c_connection);
	//b
	i=0;
	while ((bw = (GF_SDPBandwidth*)gf_list_enum(sdp->b_bandwidth, &i))) {
		SDP_WRITE_ALLOC_STR("b=", 0);
		SDP_WRITE_ALLOC_STR(bw->name, 0);
		SDP_WRITE_ALLOC_STR(":", 0);
		SDP_WRITE_ALLOC_INT(bw->value, 0, 0);
		SDP_WRITE_ALLOC_STR("\r\n", 0);
	}
	//t+r+z
	i=0;
	while ((timing = (GF_SDPTiming*)gf_list_enum(sdp->Timing, &i))) {
		if (timing->NbRepeatOffsets > GF_SDP_MAX_TIMEOFFSET) timing->NbRepeatOffsets = GF_SDP_MAX_TIMEOFFSET;
		if (timing->NbZoneOffsets > GF_SDP_MAX_TIMEOFFSET) timing->NbZoneOffsets = GF_SDP_MAX_TIMEOFFSET;
		//t
		SDP_WRITE_ALLOC_STR("t=", 0);
		SDP_WRITE_ALLOC_INT(timing->StartTime, 1, 0);
		SDP_WRITE_ALLOC_INT(timing->StopTime, 0, 0);
		SDP_WRITE_ALLOC_STR("\r\n", 0);
		if (timing->NbRepeatOffsets) {
			SDP_WRITE_ALLOC_STR("r=", 0);
			SDP_WRITE_ALLOC_INT(timing->RepeatInterval, 1, 0);
			SDP_WRITE_ALLOC_INT(timing->ActiveDuration, 0, 0);
			for (j=0; j<timing->NbRepeatOffsets; j++) {
				SDP_WRITE_ALLOC_STR(" ", 0);
				SDP_WRITE_ALLOC_INT(timing->OffsetFromStart[j], 0, 0);
			}
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
		if (timing->NbZoneOffsets) {
			SDP_WRITE_ALLOC_STR("z=", 0);
			for (j=0; j<timing->NbZoneOffsets; j++) {
				SDP_WRITE_ALLOC_INT(timing->AdjustmentTime[j], 1, 0);
				if (j+1 == timing->NbRepeatOffsets) {
					SDP_WRITE_ALLOC_INT(timing->AdjustmentOffset[j], 0, 1);
				} else {
					SDP_WRITE_ALLOC_INT(timing->AdjustmentOffset[j], 1, 1);
				}
			}
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
	}
	//k
	if (sdp->k_method) {
		SDP_WRITE_ALLOC_STR("k=", 0);
		SDP_WRITE_ALLOC_STR(sdp->k_method, 0);
		if (sdp->k_key) {
			SDP_WRITE_ALLOC_STR(":", 0);
			SDP_WRITE_ALLOC_STR(sdp->k_key, 0);
		}
		SDP_WRITE_ALLOC_STR("\r\n", 0);
	}
	//a=cat
	TEST_SDP_WRITE_SINGLE("a=cat", sdp->a_cat, 1);
	//a=keywds
	TEST_SDP_WRITE_SINGLE("a=keywds", sdp->a_keywds, 1);
	//a=tool
	TEST_SDP_WRITE_SINGLE("a=tool", sdp->a_tool, 1);
	//a=SendRecv
	switch (sdp->a_SendReceive) {
	case 1:
		TEST_SDP_WRITE_SINGLE("a=", "recvonly", 0);
		break;
	case 2:
		TEST_SDP_WRITE_SINGLE("a=", "sendonly", 0);
		break;
	case 3:
		TEST_SDP_WRITE_SINGLE("a=", "sendrecv", 0);
		break;
	default:
		break;
	}
	//a=type
	TEST_SDP_WRITE_SINGLE("a=type", sdp->a_type, 1);
	//a=charset
	TEST_SDP_WRITE_SINGLE("a=charset", sdp->a_charset, 1);
	//a=sdplang
	TEST_SDP_WRITE_SINGLE("a=sdplang", sdp->a_sdplang, 1);
	//a=lang
	TEST_SDP_WRITE_SINGLE("a=lang", sdp->a_lang, 1);

	//the rest
	i=0;
	while ((att = (GF_X_Attribute*)gf_list_enum(sdp->Attributes, &i))) {
		SDP_WRITE_ALLOC_STR("a=", 0);
		SDP_WRITE_ALLOC_STR(att->Name, 0);
		if (att->Value) {
			SDP_WRITE_ALLOC_STR(":", 0);
			SDP_WRITE_ALLOC_STR(att->Value, 0);
		}
		SDP_WRITE_ALLOC_STR("\r\n", 0);
	}

	//now write media specific
	i=0;
	while ((media = (GF_SDPMedia*)gf_list_enum(sdp->media_desc, &i))) {
		//m=
		SDP_WRITE_ALLOC_STR("m=", 0);
		switch (media->Type) {
		case 1:
			SDP_WRITE_ALLOC_STR("video", 1);
			break;
		case 2:
			SDP_WRITE_ALLOC_STR("audio", 1);
			break;
		case 3:
			SDP_WRITE_ALLOC_STR("data", 1);
			break;
		case 4:
			SDP_WRITE_ALLOC_STR("control", 1);
			break;
		default:
			SDP_WRITE_ALLOC_STR("application", 1);
			break;
		}
		SDP_WRITE_ALLOC_INT(media->PortNumber, 0, 0);
		if (media->NumPorts >= 2) {
			SDP_WRITE_ALLOC_STR("/", 0);
			SDP_WRITE_ALLOC_INT(media->NumPorts, 1, 0);
		} else {
			SDP_WRITE_ALLOC_STR(" ", 0);
		}
		SDP_WRITE_ALLOC_STR(media->Profile, 1);
		SDP_WRITE_ALLOC_STR(media->fmt_list, 0);

		j=0;
		while ((map = (GF_RTPMap*)gf_list_enum(media->RTPMaps, &j))) {
			SDP_WRITE_ALLOC_STR(" ", 0);
			SDP_WRITE_ALLOC_INT(map->PayloadType, 0, 0);
		}
		SDP_WRITE_ALLOC_STR("\r\n", 0);

		//c=
		j=0;
		while ((conn = (GF_SDPConnection*)gf_list_enum(media->Connections, &j))) {
			SDP_WRITE_CONN(conn);
		}

		//k=
		if (media->k_method) {
			SDP_WRITE_ALLOC_STR("k=", 0);
			SDP_WRITE_ALLOC_STR(media->k_method, 0);
			if (media->k_key) {
				SDP_WRITE_ALLOC_STR(":", 0);
				SDP_WRITE_ALLOC_STR(media->k_key, 0);
			}
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
		//b
		j=0;
		while ((bw = (GF_SDPBandwidth*)gf_list_enum(media->Bandwidths, &j))) {
			SDP_WRITE_ALLOC_STR("b=", 0);
			SDP_WRITE_ALLOC_STR(bw->name, 0);
			SDP_WRITE_ALLOC_STR(":", 0);
			SDP_WRITE_ALLOC_INT(bw->value, 0, 0);
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}

		//a=rtpmap
		j=0;
		while ((map = (GF_RTPMap*)gf_list_enum(media->RTPMaps, &j))) {

			SDP_WRITE_ALLOC_STR("a=rtpmap", 0);
			SDP_WRITE_ALLOC_STR(":", 0);
			SDP_WRITE_ALLOC_INT(map->PayloadType, 1, 0);
			SDP_WRITE_ALLOC_STR(map->payload_name, 0);
			SDP_WRITE_ALLOC_STR("/", 0);
			SDP_WRITE_ALLOC_INT(map->ClockRate, 0, 0);
			if (map->AudioChannels > 1) {
				SDP_WRITE_ALLOC_STR("/", 0);
				SDP_WRITE_ALLOC_INT(map->AudioChannels, 0, 0);
			}
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
		//a=fmtp
		j=0;
		while ((fmtp = (GF_SDP_FMTP*)gf_list_enum(media->FMTP, &j))) {
			SDP_WRITE_ALLOC_STR("a=fmtp:", 0);
			SDP_WRITE_ALLOC_INT(fmtp->PayloadType, 1 , 0);
			k=0;
			while ((att = (GF_X_Attribute*)gf_list_enum(fmtp->Attributes, &k)) ) {
				if (k>1) SDP_WRITE_ALLOC_STR(";", 0);
				SDP_WRITE_ALLOC_STR(att->Name, 0);
				if (att->Value) {
					SDP_WRITE_ALLOC_STR("=", 0);
					SDP_WRITE_ALLOC_STR(att->Value, 0);
				}
			}
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
		//a=ptime
		if (media->PacketTime) {
			SDP_WRITE_ALLOC_STR("a=ptime:", 0);
			SDP_WRITE_ALLOC_INT(media->PacketTime, 0, 0);
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
		//a=FrameRate
		if (media->Type == 1 && media->FrameRate) {
			SDP_WRITE_ALLOC_STR("a=framerate:", 0);
			SDP_WRITE_ALLOC_FLOAT(media->FrameRate, 0);
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
		//a=SendRecv
		switch (media->SendReceive) {
		case 1:
			TEST_SDP_WRITE_SINGLE("a=", "recvonly", 0);
			break;
		case 2:
			TEST_SDP_WRITE_SINGLE("a=", "sendonly", 0);
			break;
		case 3:
			TEST_SDP_WRITE_SINGLE("a=", "sendrecv", 0);
			break;
		default:
			break;
		}
		//a=orient
		TEST_SDP_WRITE_SINGLE("a=orient", media->orientation, 1);
		//a=sdplang
		TEST_SDP_WRITE_SINGLE("a=sdplang", media->sdplang, 1);
		//a=lang
		TEST_SDP_WRITE_SINGLE("a=lang", media->lang, 1);
		//a=quality
		if (media->Quality >= 0) {
			SDP_WRITE_ALLOC_STR("a=quality:", 0);
			SDP_WRITE_ALLOC_INT(media->Quality, 0, 0);
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
		//the rest
		j=0;
		while ((att = (GF_X_Attribute*)gf_list_enum(media->Attributes, &j))) {
			SDP_WRITE_ALLOC_STR("a=", 0);
			SDP_WRITE_ALLOC_STR(att->Name, 0);
			if (att->Value) {
				SDP_WRITE_ALLOC_STR(":", 0);
				SDP_WRITE_ALLOC_STR(att->Value, 0);
			}
			SDP_WRITE_ALLOC_STR("\r\n", 0);
		}
	}

	//finally gf_realloc
	//finall NULL char
	pos += 1;
	buf = (char *)gf_realloc(buf, pos);
	*out_str_buf = buf;
	return GF_OK;
}
#endif


#endif /*GPAC_DISABLE_STREAMING*/
