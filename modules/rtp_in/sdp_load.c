/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / RTP input module
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

#include "rtp_in.h"
#include <gpac/internal/ietf_dev.h>

#ifndef GPAC_DISABLE_STREAMING

GF_Err RP_SetupSDP(RTPClient *rtp, GF_SDPInfo *sdp, RTPStream *stream)
{
	GF_Err e;
	GF_SDPMedia *media;
	Double Start, End;
	u32 i;
	char *sess_ctrl, *session_id, *url;
	GF_X_Attribute *att;
	GF_RTSPRange *range;
	RTPStream *ch;
	RTSPSession *migrate_sess = NULL;

	Start = 0.0;
	End = -1.0;

	sess_ctrl = NULL;
	range = NULL;
	session_id = url = NULL;

	i=0;
	while ((att = (GF_X_Attribute*)gf_list_enum(sdp->Attributes, &i))) {
		//session-level control string. Keep it in the current session if any
		if (!strcmp(att->Name, "control") && att->Value) sess_ctrl = att->Value;
		//NPT range only for now
		else if (!strcmp(att->Name, "range") && !range) range = gf_rtsp_range_parse(att->Value);
		/*session migration*/
		else if (!strcmp(att->Name, "x-session-name")) url = att->Value;
		else if (!strcmp(att->Name, "x-session-id")) session_id = att->Value;
		/*we have the H264-SVC streams*/
		else if (!strcmp(att->Name, "group") && !strncmp(att->Value, "DDP", 3)) rtp->is_scalable = 1;
	}
	if (range) {
		Start = range->start;
		End = range->end;
		gf_rtsp_range_del(range);
	}

	if (url) {
		migrate_sess = RP_NewSession(rtp, url);
		if (migrate_sess && session_id) {
			migrate_sess->session_id = gf_strdup(session_id);
		}
		sess_ctrl = url;
	}

	//setup all streams
	i=0;
	while ((media = (GF_SDPMedia*)gf_list_enum(sdp->media_desc, &i))) {
		ch = RP_NewStream(rtp, media, sdp, stream);
		//do not generate error if the channel is not created, just assume
		//1 - this is not an MPEG-4 configured channel -> not needed
		//2 - this is a 2nd describe and the channel was already created
		if (!ch) continue;

		e = RP_AddStream(rtp, ch, sess_ctrl);
		if (e) {
			RP_DeleteStream(ch);
			return e;
		}

		if (!(ch->flags & RTP_HAS_RANGE)) {
			ch->range_start = Start;
			ch->range_end = End;
			if (End > 0) ch->flags |= RTP_HAS_RANGE;
		}

		/*force interleaving whenever needed*/	
		if (ch->rtsp) {
			switch (ch->depacketizer->sl_map.StreamType) {
			case GF_STREAM_VISUAL:
			case GF_STREAM_AUDIO:
				if ((rtp->transport_mode==1) && ! (ch->rtsp->flags & RTSP_FORCE_INTER) ) {
					gf_rtsp_set_buffer_size(ch->rtsp->session, RTSP_TCP_BUFFER_SIZE);
					ch->rtsp->flags |= RTSP_FORCE_INTER;
				}
				break;
			default:
				if (rtp->transport_mode && ! (ch->rtsp->flags & RTSP_FORCE_INTER) ) {
					gf_rtsp_set_buffer_size(ch->rtsp->session, RTSP_TCP_BUFFER_SIZE);
					ch->rtsp->flags |= RTSP_FORCE_INTER;
				}
				break;
			}
		}
	
	}
	return GF_OK;
}

/*load iod from data:application/mpeg4-iod;base64*/
GF_Err RP_SDPLoadIOD(RTPClient *rtp, char *iod_str)
{
	char buf[2000];
	u32 size;

	if (rtp->session_desc) return GF_SERVICE_ERROR;
	/*the only IOD format we support*/
	iod_str += 1;
	if (!strnicmp(iod_str, "data:application/mpeg4-iod;base64", strlen("data:application/mpeg4-iod;base64"))) {
		char *buf64;
		u32 size64;

		buf64 = strstr(iod_str, ",");
		if (!buf64) return GF_URL_ERROR;
		buf64 += 1;
		size64 = (u32) strlen(buf64) - 1;

		size = gf_base64_decode(buf64, size64, buf, 2000);
		if (!size) return GF_SERVICE_ERROR;
	} else if (!strnicmp(iod_str, "data:application/mpeg4-iod;base16", strlen("data:application/mpeg4-iod;base16"))) {
		char *buf16;
		u32 size16;

		buf16 = strstr(iod_str, ",");
		if (!buf16) return GF_URL_ERROR;
		buf16 += 1;
		size16 = (u32) strlen(buf16) - 1;

		size = gf_base16_decode(buf16, size16, buf, 2000);
		if (!size) return GF_SERVICE_ERROR;
	} else {
		return GF_NOT_SUPPORTED;
	}

	gf_odf_desc_read(buf, size, &rtp->session_desc);
	return GF_OK;
}


static u32 get_stream_type_from_hint(u32 ht)
{
	switch (ht) {
	case GF_MEDIA_OBJECT_VIDEO: return GF_STREAM_VISUAL;
	case GF_MEDIA_OBJECT_AUDIO: return GF_STREAM_AUDIO;
	case GF_MEDIA_OBJECT_TEXT: return GF_STREAM_TEXT;
	default: return 0;
	}
}

static GF_ESD *RP_GetChannelESD(RTPStream *ch, u32 ch_idx)
{
	GF_ESD *esd;

	if (!ch->ES_ID) ch->ES_ID = ch_idx + 1;

	esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = gf_rtp_get_clockrate(ch->rtp_ch);
	esd->slConfig->useRandomAccessPointFlag = 1;
	esd->slConfig->useTimestampsFlag = 1;
	esd->slConfig->no_dts_signaling = ch->depacketizer->sl_map.DTSDeltaLength ? 0 : 1;
	esd->ESID = ch->ES_ID;
	esd->OCRESID = 0;
	if (ch->mid)
		esd->has_ref_base = 1;

	esd->decoderConfig->streamType = ch->depacketizer->sl_map.StreamType;
	esd->decoderConfig->objectTypeIndication = ch->depacketizer->sl_map.ObjectTypeIndication;
	if (ch->depacketizer->sl_map.config) {
		esd->decoderConfig->decoderSpecificInfo->data = (char*)gf_malloc(sizeof(char) * ch->depacketizer->sl_map.configSize);
		memcpy(esd->decoderConfig->decoderSpecificInfo->data, ch->depacketizer->sl_map.config, sizeof(char) * ch->depacketizer->sl_map.configSize);
		esd->decoderConfig->decoderSpecificInfo->dataLength = ch->depacketizer->sl_map.configSize;
	}
	if (ch->depacketizer->sl_map.rvc_predef) {
		esd->decoderConfig->predefined_rvc_config = ch->depacketizer->sl_map.rvc_predef;
	} else if (ch->depacketizer->sl_map.rvc_config) {
		esd->decoderConfig->rvc_config = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
		esd->decoderConfig->rvc_config->data = ch->depacketizer->sl_map.rvc_config;
		esd->decoderConfig->rvc_config->dataLength = ch->depacketizer->sl_map.rvc_config_size;
		ch->depacketizer->sl_map.rvc_config = NULL;
		ch->depacketizer->sl_map.rvc_config_size = 0;
	}

	return esd;
}

static GF_ObjectDescriptor *RP_GetChannelOD(RTPStream *ch, u32 ch_idx)
{
	GF_ESD *esd;
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);

	esd = RP_GetChannelESD(ch, ch_idx);

	od->objectDescriptorID = ch->OD_ID ? ch->OD_ID : ch->ES_ID;

	gf_list_add(od->ESDescriptors, esd);

	// for each channel depending on this channel, get esd, set esd->dependsOnESID and add to od
	if (ch->owner->is_scalable)
	{
		u32 i, count;

		count = gf_list_count(ch->owner->channels);
		for (i = 0; i < count; i++)
		{
			RTPStream *the_channel;
			GF_ESD *the_esd;

			the_channel = (RTPStream *)gf_list_get(ch->owner->channels, i);
			if (the_channel->base_stream == ch->mid) 
			{
				the_esd = RP_GetChannelESD(the_channel, i);
				the_esd->dependsOnESID = the_channel->prev_stream;
				gf_list_add(od->ESDescriptors, the_esd);
			}
		}
	}
	
	return od;
}

GF_Descriptor *RP_EmulateIOD(RTPClient *rtp, const char *sub_url)
{
	GF_ObjectDescriptor *the_od;
	RTPStream *a_str, *ch;
	u32 i;

	if (rtp->media_type==GF_MEDIA_OBJECT_INTERACT) return NULL;
	if (rtp->media_type==GF_MEDIA_OBJECT_UPDATES) return NULL;

	/*single object generation*/
	a_str = NULL;
	if (sub_url || ((rtp->media_type != GF_MEDIA_OBJECT_SCENE) && (rtp->media_type != GF_MEDIA_OBJECT_UNDEF)) ) {
		i=0;
		while ((ch = (RTPStream *)gf_list_enum(rtp->channels, &i))) {
			if (ch->depacketizer->sl_map.StreamType != get_stream_type_from_hint(rtp->media_type)) continue;

			if (!sub_url || (ch->control && strstr(sub_url, ch->control)) ) {
				the_od = RP_GetChannelOD(ch, i-1);
				if (!the_od) continue;
				return (GF_Descriptor *) the_od;
			}
			if (!a_str) a_str = ch;
		}
		if (a_str) {
			the_od = RP_GetChannelOD(a_str, gf_list_find(rtp->channels, a_str) );
			return (GF_Descriptor *) the_od;
		}
		return NULL;
	}
	return NULL;
}


void RP_SetupObjects(RTPClient *rtp)
{
	GF_ObjectDescriptor *od;
	RTPStream *ch;
	u32 i;

	/*add everything*/
	i=0;
	while ((ch = (RTPStream *)gf_list_enum(rtp->channels, &i))) {
		if (ch->control && !strnicmp(ch->control, "data:", 5)) continue;
		if (ch->prev_stream) continue;

		if (!rtp->media_type) {
			od = RP_GetChannelOD(ch, i);
			if (!od) continue;
			gf_term_add_media(rtp->service, (GF_Descriptor*)od, 1);
		} else if (rtp->media_type==ch->depacketizer->sl_map.StreamType) {
			od = RP_GetChannelOD(ch, i);
			if (!od) continue;
			gf_term_add_media(rtp->service, (GF_Descriptor*)od, 1);
			rtp->media_type = 0;
			break;
		}
	}
	gf_term_add_media(rtp->service, NULL, 0);
}

void RP_LoadSDP(RTPClient *rtp, char *sdp_text, u32 sdp_len, RTPStream *stream)
{
	GF_Err e;
	u32 i;
	GF_SDPInfo *sdp;
	Bool is_isma_1, has_iod;
	char *iod_str;
	GF_X_Attribute *att;

	is_isma_1 = 0;
	iod_str = NULL;
	sdp = gf_sdp_info_new();
	e = gf_sdp_info_parse(sdp, sdp_text, sdp_len);

	if (e == GF_OK) e = RP_SetupSDP(rtp, sdp, stream);

	/*root SDP, attach service*/
	if (! stream) {
		/*look for IOD*/
		if (e==GF_OK) {
			i=0;
			while ((att = (GF_X_Attribute*)gf_list_enum(sdp->Attributes, &i))) {
				if (!iod_str && !strcmp(att->Name, "mpeg4-iod") ) iod_str = att->Value;
				if (!is_isma_1 && !strcmp(att->Name, "isma-compliance") ) {
					if (!stricmp(att->Value, "1,1.0,1")) is_isma_1 = 1;
				}
			}

			/*force iod reconstruction with ISMA to use proper clock dependencies*/
			if (is_isma_1) iod_str = NULL;

			/*some folks have weird notions of MPEG-4 systems, they use hardcoded IOD 
			with AAC ESD even when streaming AMR...*/
			if (iod_str) {
				RTPStream *ch;
				i=0;
				while ((ch = (RTPStream *)gf_list_enum(rtp->channels, &i))) {
					if ((ch->depacketizer->payt==GF_RTP_PAYT_AMR) || (ch->depacketizer->payt==GF_RTP_PAYT_AMR_WB) ) {
						iod_str = NULL;
						break;
					}
				}
			} 
			if (!iod_str) {
				RTPStream *ch;
				Bool needs_iod = 0;
				i=0;
				while ((ch = (RTPStream *)gf_list_enum(rtp->channels, &i))) {
					if ((ch->depacketizer->payt==GF_RTP_PAYT_MPEG4) && (ch->depacketizer->sl_map.StreamType==GF_STREAM_SCENE) 
//						|| ((ch->depacketizer->payt==GF_RTP_PAYT_3GPP_DIMS) && (ch->depacketizer->sl_map.StreamType==GF_STREAM_SCENE))
						) {
						needs_iod = 1;
						break;
					}
				}
				if (needs_iod) {
					rtp->session_desc = (GF_Descriptor *)RP_GetChannelOD(ch, 0);
				}
			}
			
			if (iod_str) e = RP_SDPLoadIOD(rtp, iod_str);
		}
		/*attach service*/
		has_iod = rtp->session_desc ? 1 : 0;
		gf_term_on_connect(rtp->service, NULL, e);
		if (!e && !has_iod && !rtp->media_type) RP_SetupObjects(rtp);
		rtp->media_type = 0;
	}
	/*channel SDP */
	else {
		if (e) {
			gf_term_on_connect(rtp->service, stream->channel, e);
			stream->status = RTP_Unavailable;
		} else {
			/*connect*/
			RP_SetupChannel(stream, NULL);
		}
	}
	/*store SDP for later session migration*/
	if (sdp) {
		char *buf=NULL;
		gf_sdp_info_write(sdp, &buf);
		if (buf) {
			rtp->session_state_data = gf_malloc(sizeof(char) * (strlen("data:application/sdp,") + strlen(buf) + 1) );
			strcpy(rtp->session_state_data, "data:application/sdp,");
			strcat(rtp->session_state_data, buf);
			gf_free(buf);
		}
		gf_sdp_info_del(sdp);
	}
}

void MigrateSDP_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	RTPClient *rtp = (RTPClient *)cbk;

	switch (param->msg_type) {
	case GF_NETIO_GET_METHOD:
		param->name = "POST";
		return;
	case GF_NETIO_GET_HEADER:
		if (!strcmp(param->name, "POST")) {
			param->name = "Content-Type";
			param->value = "application/sdp";
		}
		return;

	case GF_NETIO_GET_CONTENT:
		param->data = rtp->session_state_data + strlen("data:application/sdp,");
		param->size = (u32) strlen(param->data);
		return;
	}
}


void RP_SaveSessionState(RTPClient *rtp)
{
	GF_Err e;
	char *sdp_buf;
	const char *opt;
	GF_X_Attribute*att;
	u32 i, j;
	GF_SDPInfo *sdp;
	RTSPSession *sess = NULL;

	if (!rtp->session_state_data) return;

	sdp_buf = rtp->session_state_data + strlen("data:application/sdp,");
	sdp = gf_sdp_info_new();
	e = gf_sdp_info_parse(sdp, sdp_buf, (u32) strlen(sdp_buf) );

	for (i=0; i<gf_list_count(rtp->channels); i++) {
		GF_SDPMedia *media = NULL;
		RTPStream *ch = gf_list_get(rtp->channels, i);
		if (!ch->control) continue;

		for (j=0; j<gf_list_count(sdp->media_desc); j++) {
			u32 k;
			GF_SDPMedia *med = (GF_SDPMedia*)gf_list_get(sdp->media_desc, j);

			for (k=0; k<gf_list_count(med->Attributes); k++) {
				att = (GF_X_Attribute*)gf_list_get(med->Attributes, k);
				if (!stricmp(att->Name, "control") && (strstr(att->Value, ch->control)!=NULL) ) {
					media = med;
					break;
				}
			}
			if (media)
				break;
		}
		if (!media) continue;

		if (ch->rtp_ch->net_info.IsUnicast) {
			char szPorts[4096];
			u16 porta, portb;
			media->PortNumber = ch->rtp_ch->net_info.client_port_first;

			/*remove x-server-port extension*/
			for (j=0; j<gf_list_count(media->Attributes); j++) {
				att = (GF_X_Attribute*)gf_list_get(media->Attributes, j);
				if (!stricmp(att->Name, "x-stream-state") ) {
					gf_free(att->Name);
					gf_free(att->Value);
					gf_free(att);
					gf_list_rem(media->Attributes, j);
				}
			}
			ch->current_start += gf_rtp_get_current_time(ch->rtp_ch);

			GF_SAFEALLOC(att, GF_X_Attribute);
			att->Name = gf_strdup("x-stream-state");
			porta = ch->rtp_ch->net_info.port_first ? ch->rtp_ch->net_info.port_first : ch->rtp_ch->net_info.client_port_first;
			portb = ch->rtp_ch->net_info.port_last ? ch->rtp_ch->net_info.port_last : ch->rtp_ch->net_info.client_port_last;

			sprintf(szPorts, "server-port=%d-%d;ssrc=%X;npt=%g;seq=%d;rtptime=%d", 
				porta, 
				portb,
				ch->rtp_ch->SenderSSRC,
				ch->current_start,
				ch->rtp_ch->rtp_first_SN,
				ch->rtp_ch->rtp_time
			);
			att->Value = gf_strdup(szPorts);
			gf_list_add(media->Attributes, att);

			if (ch->rtsp)
				sess = ch->rtsp;
		} else {
			media->PortNumber = ch->rtp_ch->net_info.port_first;
		}

	}
	/*remove x-server-port/x-session-id extension*/
	for (j=0; j<gf_list_count(sdp->Attributes); j++) {
		att = (GF_X_Attribute*)gf_list_get(sdp->Attributes, j);
		if (!stricmp(att->Name, "x-session-id") || !stricmp(att->Name, "x-session-name")
		) {
			gf_free(att->Name);
			gf_free(att->Value);
			gf_free(att);
			gf_list_rem(sdp->Attributes, j);
		}
	}
	if (sess) {
		char szURL[4096];

		if (sess->session_id) {
			GF_SAFEALLOC(att, GF_X_Attribute);
			att->Name = gf_strdup("x-session-id");
			att->Value = gf_strdup(sess->session_id);
			gf_list_add(sdp->Attributes, att);
		}

		GF_SAFEALLOC(att, GF_X_Attribute);
		att->Name = gf_strdup("x-session-name");
		sprintf(szURL, "rtsp://%s:%d/%s", sess->session->Server, sess->session->Port, sess->session->Service);
		att->Value = gf_strdup(szURL);
		gf_list_add(sdp->Attributes, att);
	}

	gf_free(rtp->session_state_data);
	sdp_buf = NULL;
	gf_sdp_info_write(sdp, &sdp_buf);
	if (sdp_buf) {
		rtp->session_state_data = gf_malloc(sizeof(char) * (strlen("data:application/sdp,") + strlen(sdp_buf) + 1) );
		strcpy(rtp->session_state_data, "data:application/sdp,");
		strcat(rtp->session_state_data, sdp_buf);
		gf_free(sdp_buf);
	}


	gf_sdp_info_del(sdp);


	opt = (char *) gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(rtp->service), "Streaming", "SessionMigrationServer");
	if (opt) {
		if (rtp->dnload) gf_term_download_del(rtp->dnload);
		rtp->dnload = NULL;

		if (strnicmp(opt, "http://", 7)) {
			rtp->dnload = gf_term_download_new(rtp->service, opt, GF_NETIO_SESSION_NOT_THREADED, MigrateSDP_NetIO, rtp);
			while (1) {
				char buffer[100];
				u32 read;
				e = gf_dm_sess_fetch_data(rtp->dnload, buffer, 100, &read);
				if (e && (e!=GF_IP_NETWORK_EMPTY)) break;
			}
			gf_term_download_del(rtp->dnload);
			rtp->dnload = NULL;
		} else {
			FILE *f = gf_f64_open(opt, "wt");
			if (f) {
				sdp_buf = rtp->session_state_data + strlen("data:application/sdp,");
				gf_fwrite(sdp_buf, 1, strlen(sdp_buf), f);
				fclose(f);
			} else {
				e = GF_IO_ERR;
			}
		}
		if (e<0) {
			gf_term_on_message(sess->owner->service, e, "Error saving session state");
		}
	}
}

#endif /*GPAC_DISABLE_STREAMING*/
