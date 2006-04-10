/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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


GF_Err RP_SetupSDP(RTPClient *rtp, GF_SDPInfo *sdp, RTPStream *stream)
{
	GF_Err e;
	GF_SDPMedia *media;
	Double Start, End;
	u32 i;
	char *sess_ctrl;
	GF_X_Attribute *att;
	GF_RTSPRange *range;
	RTPStream *ch;

	Start = 0.0;
	End = -1.0;

	sess_ctrl = NULL;
	range = NULL;

	i=0;
	while ((att = gf_list_enum(sdp->Attributes, &i))) {
		//session-level control string. Keep it in the current session if any
		if (!strcmp(att->Name, "control") && att->Value) sess_ctrl = att->Value;
		//NPT range only for now
		else if (!strcmp(att->Name, "range") && !range) range = gf_rtsp_range_parse(att->Value);
	}
	if (range) {
		Start = range->start;
		End = range->end;
		gf_rtsp_range_del(range);
	}
	
	//setup all streams
	i=0;
	while ((media = gf_list_enum(sdp->media_desc, &i))) {
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

		if (!(ch->flags & CH_HasRange)) {
			ch->range_start = Start;
			ch->range_end = End;
			if (End > 0) ch->flags |= CH_HasRange;
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
		size64 = strlen(buf64) - 1;

		size = gf_base64_decode(buf64, size64, buf, 2000);
		if (!size) return GF_SERVICE_ERROR;
	} else if (!strnicmp(iod_str, "data:application/mpeg4-iod;base16", strlen("data:application/mpeg4-iod;base16"))) {
		char *buf16;
		u32 size16;

		buf16 = strstr(iod_str, ",");
		if (!buf16) return GF_URL_ERROR;
		buf16 += 1;
		size16 = strlen(buf16) - 1;

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

static GF_ObjectDescriptor *RP_GetChannelOD(RTPStream *ch, u16 OCR_ES_ID, u32 ch_idx)
{
	GF_ESD *esd;
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);

	if (!ch->ES_ID) ch->ES_ID = ch_idx + 1;
	od->objectDescriptorID = ch->ES_ID;
	esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = ch->clock_rate;
	esd->slConfig->useRandomAccessPointFlag = 1;
	esd->slConfig->useTimestampsFlag = 1;
	esd->ESID = ch->ES_ID;
	esd->OCRESID = OCR_ES_ID;
	esd->decoderConfig->streamType = ch->sl_map.StreamType;
	esd->decoderConfig->objectTypeIndication = ch->sl_map.ObjectTypeIndication;
	if (ch->sl_map.config) {
		esd->decoderConfig->decoderSpecificInfo->data = malloc(sizeof(char) * ch->sl_map.configSize);
		memcpy(esd->decoderConfig->decoderSpecificInfo->data, ch->sl_map.config, sizeof(char) * ch->sl_map.configSize);
		esd->decoderConfig->decoderSpecificInfo->dataLength = ch->sl_map.configSize;
	}
	gf_list_add(od->ESDescriptors, esd);
	return od;
}

GF_Descriptor *RP_EmulateIOD(RTPClient *rtp, u32 expect_type, const char *sub_url)
{
	GF_ObjectDescriptor *the_od;
	RTPStream *a_str, *ch;
	u32 i;

	if (expect_type==GF_MEDIA_OBJECT_INTERACT) return NULL;
	if (expect_type==GF_MEDIA_OBJECT_BIFS) return NULL;

	/*single object generation*/
	a_str = NULL;
	if (sub_url || ((expect_type!=GF_MEDIA_OBJECT_SCENE) && (expect_type!=GF_MEDIA_OBJECT_UNDEF)) ) {
		i=0;
		while ((ch = gf_list_enum(rtp->channels, &i))) {
			if (ch->sl_map.StreamType != get_stream_type_from_hint(expect_type)) continue;

			if (!sub_url || strstr(sub_url, ch->control)) {
				the_od = RP_GetChannelOD(ch, 0, i);
				if (!the_od) continue;
				return (GF_Descriptor *) the_od;
			}
			if (!a_str) a_str = ch;
		}
		if (a_str) {
			the_od = RP_GetChannelOD(a_str, 0, gf_list_find(rtp->channels, a_str) );
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
	while ((ch = gf_list_enum(rtp->channels, &i))) {
		if (!rtp->forced_type) {
			od = RP_GetChannelOD(ch, 0, i);
			if (!od) continue;
			gf_term_add_media(rtp->service, (GF_Descriptor*)od, 1);
		} else if (rtp->forced_type==ch->sl_map.StreamType) {
			od = RP_GetChannelOD(ch, 0, i);
			if (!od) continue;
			gf_term_add_media(rtp->service, (GF_Descriptor*)od, 1);
			rtp->forced_type = 0;
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
	Bool is_isma_1;
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
			while ((att = gf_list_enum(sdp->Attributes, &i))) {
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
				while ((ch = gf_list_enum(rtp->channels, &i))) {
					if ((ch->rtptype==GP_RTP_PAYT_AMR) || (ch->rtptype==GP_RTP_PAYT_AMR_WB) ) {
						iod_str = NULL;
						break;
					}
				}
			}
			
			if (iod_str) e = RP_SDPLoadIOD(rtp, iod_str);
		}
		/*attach service*/
		gf_term_on_connect(rtp->service, NULL, e);
		if (!e) RP_SetupObjects(rtp);
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

	if (sdp) gf_sdp_info_del(sdp);
}

