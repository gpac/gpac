/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / RTP/RTSP input filter
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

#include "in_rtp.h"
#include <gpac/internal/ietf_dev.h>
//for ismacrypt scheme
#include <gpac/isomedia.h>
#include <gpac/avparse.h>

#ifndef GPAC_DISABLE_STREAMING

static GF_Err rtpin_setup_sdp(GF_RTPIn *rtp, GF_SDPInfo *sdp, GF_RTPInStream *for_stream)
{
	GF_Err e;
	GF_SDPMedia *media;
	Double Start, End;
	u32 i;
	char *sess_ctrl;
	GF_X_Attribute *att;
	GF_RTSPRange *range;

	Start = 0.0;
	End = -1.0;

	sess_ctrl = NULL;
	range = NULL;

	i=0;
	while ((att = (GF_X_Attribute*)gf_list_enum(sdp->Attributes, &i))) {
		if (!att->Name || !att->Value) continue;
		//session-level control string. Keep it in the current session if any
		if (!strcmp(att->Name, "control")) sess_ctrl = att->Value;
		//NPT range only for now
		else if (!strcmp(att->Name, "range") && !range) range = gf_rtsp_range_parse(att->Value);
		/*we have the H264-SVC streams*/
		else if (!strcmp(att->Name, "group") && !strncmp(att->Value, "DDP", 3))
			rtp->is_scalable = GF_TRUE;
	}
	if (range) {
		Start = range->start;
		End = range->end;
		gf_rtsp_range_del(range);
	}

	if (!sess_ctrl && rtp->session) {
		if (rtp->forceagg) {
			sess_ctrl = "*";
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTPIn] Session-level control missing in RTSP SDP - if playback failure, retry specifying `--forceagg`\n"));
		}
	}

	//setup all streams
	i=0;
	while ((media = (GF_SDPMedia*)gf_list_enum(sdp->media_desc, &i))) {
		GF_RTPInStream *stream = rtpin_stream_new(rtp, media, sdp, for_stream);
		//do not generate error if the channel is not created, just assume
		//1 - this is not an MPEG-4 configured channel -> not needed
		//2 - this is a 2nd describe and the channel was already created
		if (!stream) continue;

		e = rtpin_add_stream(rtp, stream, sess_ctrl);
		if (e) {
			rtpin_stream_del(stream);
			return e;
		}

		if (!(stream->flags & RTP_HAS_RANGE)) {
			stream->range_start = Start;
			stream->range_end = End;
			if (End > 0) stream->flags |= RTP_HAS_RANGE;
		}

		/*force interleaving whenever needed*/
		if (stream->rtsp) {
			if ((rtp->transport==RTP_TRANSPORT_TCP_ONLY) && ! (stream->rtsp->flags & RTSP_FORCE_INTER) ) {
				gf_rtsp_set_buffer_size(stream->rtsp->session, stream->rtpin->block_size);
				stream->rtsp->flags |= RTSP_FORCE_INTER;
			}
		}

	}
	return GF_OK;
}

//we now ignore the IOD (default scene anyway) and let the user deal with the media streams
//code is kept for reference
#if 0
/*load iod from data:application/mpeg4-iod;base64*/
static GF_Err rtpin_sdp_load_iod(GF_RTPIn *rtp, char *iod_str)
{
	char buf[2000];
	u32 size;

	if (rtp->iod_desc) return GF_SERVICE_ERROR;
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

	gf_odf_desc_read(buf, size, &rtp->iod_desc);
	return GF_OK;
}
#endif

void rtpin_declare_pid(GF_RTPInStream *stream, Bool force_iod, u32 ch_idx, u32 *ocr_es_id)
{
	GP_RTPSLMap *sl_map;
	const GF_RTPStaticMap *static_map;

	if (!stream->depacketizer) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("RTP Stream channel %u has no depacketizer - not supported\n", ch_idx));
		return;
	}

	gf_assert(!stream->opid);
	stream->opid = gf_filter_pid_new(stream->rtpin->filter);

	gf_filter_pid_set_property(stream->opid, GF_PROP_PID_ID, &PROP_UINT(ch_idx) );
	if (stream->ES_ID && (force_iod || stream->rtpin->iod_desc))
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_ESID, &PROP_UINT(stream->ES_ID) );

	gf_filter_pid_set_property(stream->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT( gf_rtp_get_clockrate(stream->rtp_ch) ) );


	if (stream->rtpin->session && (stream->flags & RTP_HAS_RANGE)) {
		GF_Fraction64 dur;
		dur.den = 1000;
		dur.num = (s64) (1000 * (stream->range_end - stream->range_start));
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_DURATION, &PROP_FRAC64(dur) );
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_SEEK ) );
	} else {
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_NONE ) );
	}
	if (stream->mid)
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_SCALABLE, &PROP_BOOL( GF_TRUE) );

	//TOCHECK: do we need to map ODID ?
	//od->objectDescriptorID = stream->OD_ID ? stream->OD_ID : stream->ES_ID;

	// for each channel depending on this channel, get esd, set esd->dependsOnESID and add to od
	if (stream->rtpin->is_scalable && stream->prev_stream) {
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT( stream->prev_stream) );
	}

	if (stream->rtpin->iod_desc) {
		u32 i, count;
		GF_ObjectDescriptor *iod = (GF_ObjectDescriptor *)stream->rtpin->iod_desc;
		count = gf_list_count(iod->ESDescriptors);
		for (i=0; i<count; i++) {
			GF_ESD *esd = gf_list_get(iod->ESDescriptors, i);
			if (esd->ESID == stream->ES_ID) {
				gf_filter_pid_set_property(stream->opid, GF_PROP_PID_IN_IOD, &PROP_BOOL( GF_TRUE ) );
			}
		}
	} else if (force_iod) {
		switch (stream->depacketizer->sl_map.StreamType) {
		case GF_STREAM_OD:
		case GF_STREAM_SCENE:
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_IN_IOD, &PROP_BOOL( GF_TRUE ) );
			break;
		}
	}
	if (!stream->ES_ID)
		stream->ES_ID = ch_idx;

	if (ocr_es_id) {
		if (! *ocr_es_id) *ocr_es_id = stream->ES_ID;
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT( *ocr_es_id) );
	}

	sl_map = &stream->depacketizer->sl_map;
	static_map = stream->depacketizer->static_map;

	if (sl_map->StreamType) {
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(sl_map->StreamType) );
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_CODECID, &PROP_UINT(sl_map->CodecID) );

		if (sl_map->config)
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(sl_map->config, sl_map->configSize) );

		if (sl_map->rvc_predef) {
			gf_filter_pid_set_property_str(stream->opid, "rvc:predef", &PROP_UINT( sl_map->rvc_predef) );
		} else if (sl_map->rvc_config) {
			gf_filter_pid_set_property_str(stream->opid, "rvc:config", &PROP_DATA_NO_COPY( sl_map->rvc_config, sl_map->rvc_config_size) );
			sl_map->rvc_config = NULL;
			sl_map->rvc_config_size = 0;
		}
	} else if (static_map) {
		if (static_map->stream_type)
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(static_map->stream_type) );
		if (static_map->codec_id)
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_CODECID, &PROP_UINT(static_map->codec_id) );
		if (static_map->mime)
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_MIME, &PROP_STRING((char*)static_map->mime) );
	}
	if (stream->depacketizer->w)
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_WIDTH, &PROP_UINT(stream->depacketizer->w) );
	if (stream->depacketizer->h)
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(stream->depacketizer->h) );


	/*ISMACryp config*/
	if (stream->depacketizer->flags & GF_RTP_HAS_ISMACRYP) {
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_PROTECTION_SCHEME_TYPE, &PROP_4CC(GF_ISOM_ISMACRYP_SCHEME) );

		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_PROTECTION_SCHEME_VERSION, &PROP_UINT(1) );
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_PROTECTION_KMS_URI, &PROP_STRING(stream->depacketizer->key) );
	}

	if (sl_map->StreamType==GF_STREAM_VISUAL) {
		if (stream->depacketizer->payt != GF_RTP_PAYT_MPEG4) {
			gf_filter_pid_recompute_dts(stream->opid, GF_TRUE);
		} else if (!stream->depacketizer->sl_map.DTSDeltaLength && !stream->depacketizer->sl_map.CTSDeltaLength) {
			gf_filter_pid_recompute_dts(stream->opid, GF_TRUE);
		}
	}


	switch (sl_map->CodecID) {
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		if (sl_map->config) {
#ifndef GPAC_DISABLE_AV_PARSERS
			GF_M4ADecSpecInfo acfg;
			gf_m4a_get_config(sl_map->config, sl_map->configSize, &acfg);
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(acfg.base_sr) );
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(acfg.nb_chan) );
#endif

		} else {
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(gf_rtp_get_clockrate(stream->rtp_ch) ) );
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(2) );
		}
		break;
	//declare as unframed the following for decoder config extraction
	case GF_CODECID_MPEG1:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_SPATIAL:
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
		break;
	}
}


static void rtpin_declare_media(GF_RTPIn *rtp, Bool force_iod)
{
	GF_RTPInStream *stream;
	u32 i, ocr_es_id;
	ocr_es_id = 0;
	/*add everything*/
	i=0;
	while ((stream = (GF_RTPInStream *)gf_list_enum(rtp->streams, &i))) {
		if (stream->control && !strnicmp(stream->control, "data:", 5)) continue;

		if (rtp->stream_type && (rtp->stream_type!=stream->depacketizer->sl_map.StreamType)) continue;

		rtpin_declare_pid(stream, force_iod, i, &ocr_es_id);
	}
	rtp->stream_type = 0;
}

void rtpin_load_sdp(GF_RTPIn *rtp, char *sdp_text, u32 sdp_len, GF_RTPInStream *stream)
{
	GF_Err e;
	u32 i;
	GF_SDPInfo *sdp;
	Bool is_isma_1;
#if 0
	char *iod_str = NULL;
#endif
	GF_X_Attribute *att;
	Bool force_in_iod = GF_FALSE;

	is_isma_1 = GF_FALSE;
	sdp = gf_sdp_info_new();
	e = gf_sdp_info_parse(sdp, sdp_text, sdp_len);

	if (e == GF_OK) e = rtpin_setup_sdp(rtp, sdp, stream);

	if (!gf_list_count(rtp->streams))
		e = GF_NOT_SUPPORTED;

	if (e != GF_OK) {
		if (!stream) {
			gf_filter_setup_failure(rtp->filter, e);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTPIn] code not tested file %s line %d !!\n", __FILE__, __LINE__));
			gf_filter_setup_failure(rtp->filter, e);
			stream->status = RTP_Unavailable;
		}
		gf_sdp_info_del(sdp);
		return;
	}

	/*root SDP, attach service*/
	if (!stream) {
		/*look for IOD*/
		i=0;
		while ((att = (GF_X_Attribute*)gf_list_enum(sdp->Attributes, &i))) {
#if 0
			if (!iod_str && !strcmp(att->Name, "mpeg4-iod") ) iod_str = att->Value;
#endif
			if (!is_isma_1 && !strcmp(att->Name, "isma-compliance") ) {
				if (!stricmp(att->Value, "1,1.0,1")) is_isma_1 = GF_TRUE;
			}
		}

#if 0
		/*force iod reconstruction with ISMA to use proper clock dependencies*/
		if (is_isma_1) iod_str = NULL;

		if (iod_str) {
			e = rtpin_sdp_load_iod(rtp, iod_str);
		} else
#endif
		{
			GF_RTPInStream *a_stream;
			i=0;
			while (!force_in_iod && (a_stream = (GF_RTPInStream *)gf_list_enum(rtp->streams, &i))) {
				if (!a_stream->depacketizer) continue;
				if (a_stream->depacketizer->payt!=GF_RTP_PAYT_MPEG4) continue;
				switch (a_stream->depacketizer->sl_map.StreamType) {
				case GF_STREAM_SCENE:
				case GF_STREAM_OD:
					force_in_iod = GF_TRUE;
					break;
				default:
					break;
				}
			}
		}

		/* service failed*/
		if (e) gf_filter_setup_failure(rtp->filter, e);
		else rtpin_declare_media(rtp, force_in_iod);
	}
	/*channel SDP */
	else {
		/*connect*/
		rtpin_stream_setup(stream, NULL);
	}
	gf_sdp_info_del(sdp);
}

#endif /*GPAC_DISABLE_STREAMING*/
